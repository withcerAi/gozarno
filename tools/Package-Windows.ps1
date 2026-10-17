param([string]$BuildDirectory = 'build-arovan')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$versionMatch=[regex]::Match((Get-Content (Join-Path $projectRoot 'CMakeLists.txt') -Raw),'VERSION\s+(\d+\.\d+\.\d+)')
if(!$versionMatch.Success) { throw 'Cannot read application version from CMakeLists.txt' }
$releaseVersion=$versionMatch.Groups[1].Value
$builtVersion=[Diagnostics.FileVersionInfo]::GetVersionInfo((Join-Path $projectRoot "$BuildDirectory/bin/openconnect-gui.exe")).FileVersion
if($builtVersion -ne $releaseVersion) { throw "Build version $builtVersion does not match release $releaseVersion. Rebuild before packaging." }
$runtimeRoot = Join-Path $projectRoot '.tools/msys64/mingw64/bin'
$stage = Join-Path $projectRoot 'dist/GozarnoVPN'
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item -LiteralPath (Join-Path $projectRoot "$BuildDirectory/bin/openconnect-gui.exe") -Destination (Join-Path $stage 'GozarnoVPN.exe')
Copy-Item -LiteralPath (Join-Path $projectRoot "$BuildDirectory/bin/vpnc-script.js") -Destination $stage
$engineExports = & (Join-Path $runtimeRoot 'objdump.exe') -p (Join-Path $projectRoot '.tools/openconnect-prefix/bin/libopenconnect-5.dll')
if (-not ($engineExports -match 'gozarno_openconnect_packet_io_v1')) { throw 'OpenConnect lacks the isolated SOCKS packet API. Apply tools/patches/openconnect-9.12-packet-io.patch and rebuild it.' }
Copy-Item -LiteralPath (Join-Path $projectRoot '.tools/openconnect-prefix/bin/libopenconnect-5.dll') -Destination $stage
Copy-Item -LiteralPath (Join-Path $projectRoot 'build-dependencies/wintun/wintun/bin/amd64/wintun.dll') -Destination $stage
$env:Path = "$runtimeRoot;$env:Path"
& (Join-Path $runtimeRoot 'windeployqt.exe') --release --no-translations --no-compiler-runtime (Join-Path $stage 'GozarnoVPN.exe')
if ($LASTEXITCODE) { throw 'Qt deployment failed' }
$queue = [Collections.Generic.Queue[string]]::new()
Get-ChildItem -LiteralPath $stage -Recurse -File | Where-Object Extension -in '.dll','.exe' | ForEach-Object { $queue.Enqueue($_.FullName) }
$seen = @{}
while ($queue.Count) {
  $binary = $queue.Dequeue()
  if ($seen.ContainsKey($binary)) { continue }
  $seen[$binary] = $true
  $imports = & (Join-Path $runtimeRoot 'objdump.exe') -p $binary 2>$null
  foreach ($line in $imports) {
    if ($line -notmatch 'DLL Name:\s*(\S+)') { continue }
    $name = $Matches[1]
    $destination = Join-Path $stage $name
    if (Test-Path -LiteralPath $destination) { continue }
    $source = Join-Path $runtimeRoot $name
    if (Test-Path -LiteralPath $source) {
      Copy-Item -LiteralPath $source -Destination $destination
      $queue.Enqueue($destination)
    } elseif (!(Test-Path -LiteralPath (Join-Path "$env:SystemRoot/System32" $name)) -and $name -notmatch '^(api-ms-|ext-ms-)') {
      throw "Missing dependency: $name ($binary)"
    }
  }
}
New-Item -ItemType Directory -Force -Path (Join-Path $stage 'backend'),(Join-Path $stage 'licenses') | Out-Null
Get-ChildItem (Join-Path $projectRoot 'build-dependencies/backend') -File | Where-Object { $_.Name -notlike 'ProxiFyreUI*' -and $_.Extension -in '.exe','.dll','.config' } | Copy-Item -Destination (Join-Path $stage 'backend')
Copy-Item (Join-Path $projectRoot 'build-dependencies/licenses/*') -Destination (Join-Path $stage 'licenses')
$noticeTarget=Join-Path $stage 'licenses/MSYS2'
if(Test-Path -LiteralPath $noticeTarget) {
  $resolvedNotice=(Resolve-Path -LiteralPath $noticeTarget).Path
  $expectedNotice=[IO.Path]::GetFullPath((Join-Path $projectRoot 'dist/GozarnoVPN/licenses/MSYS2'))
  if($resolvedNotice -ne $expectedNotice) { throw 'Unexpected notice staging path' }
  Remove-Item -LiteralPath $resolvedNotice -Recurse -Force
}
Copy-Item (Join-Path $projectRoot '.tools/msys64/mingw64/share/licenses') -Destination $noticeTarget -Recurse
Copy-Item (Join-Path $projectRoot 'LICENSE.txt') -Destination (Join-Path $stage 'licenses/Gozarno-GPL.txt')
Copy-Item (Join-Path $projectRoot 'docs/licenses/*') -Destination (Join-Path $stage 'licenses')
Copy-Item (Join-Path $projectRoot '.tools/openconnect/COPYING.LGPL') -Destination (Join-Path $stage 'licenses/OpenConnect-LGPL-2.1.txt')
Copy-Item (Join-Path $projectRoot 'build-dependencies/wintun/wintun/LICENSE.txt') -Destination (Join-Path $stage 'licenses/Wintun.txt')
Copy-Item (Join-Path $projectRoot 'docs/client-modernization.md') -Destination (Join-Path $stage 'README.md')
Copy-Item (Join-Path $projectRoot 'docs/socks-mode.md') -Destination (Join-Path $stage 'SOCKS-README.md')
Copy-Item (Join-Path $projectRoot 'docs/third-party.md') -Destination (Join-Path $stage 'licenses/third-party.md')
Copy-Item (Join-Path $projectRoot 'docs/source-build.md') -Destination (Join-Path $stage 'licenses/source-build.md')
$uninstallCommands=New-Object Collections.Generic.List[string]
Get-ChildItem -LiteralPath $stage -Recurse -File | ForEach-Object {
  $relative=$_.FullName.Substring($stage.Length+1)
  $uninstallCommands.Add('Delete "$INSTDIR\'+$relative+'"')
}
Get-ChildItem -LiteralPath $stage -Recurse -Directory | Sort-Object {$_.FullName.Length} -Descending | ForEach-Object {
  $relative=$_.FullName.Substring($stage.Length+1)
  $uninstallCommands.Add('RMDir "$INSTDIR\'+$relative+'"')
}
$uninstallCommands | Set-Content (Join-Path $projectRoot 'installer/uninstall-files.nsh') -Encoding utf8
& (Join-Path $runtimeRoot 'makensis.exe') "/DAPP_VERSION=$releaseVersion" (Join-Path $projectRoot 'installer/GozarnoVPN.nsi')
if ($LASTEXITCODE) { throw 'Installer compilation failed' }
Get-ChildItem -LiteralPath $stage -Recurse -File | Get-FileHash -Algorithm SHA256 | Select-Object Path,Hash | ConvertTo-Json | Set-Content (Join-Path $projectRoot 'dist/manifest.json')
Write-Output "Package created: $stage"
