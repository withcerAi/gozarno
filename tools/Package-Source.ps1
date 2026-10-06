$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$destination=Join-Path $projectRoot 'dist/sources'
New-Item -ItemType Directory -Force -Path $destination | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
function Write-SourceArchive([string]$Root,[string]$Archive) {
    $files=& git -C $Root ls-files --cached --others --exclude-standard
    if($LASTEXITCODE) { throw "Cannot inventory source: $Root" }
    $archivePath=Join-Path $destination $Archive
    $stream=[IO.File]::Open($archivePath,[IO.FileMode]::Create)
    $zip=[IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach($name in $files) {
            $absolute=Join-Path $Root $name
            if([IO.File]::Exists($absolute)) { [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$absolute,$name,[IO.Compression.CompressionLevel]::Optimal) | Out-Null }
        }
    } finally { $zip.Dispose(); $stream.Dispose() }
}
Write-SourceArchive $projectRoot 'GozarnoVPN-1.2.1-source.zip'
Write-SourceArchive (Join-Path $projectRoot '.tools/openconnect') 'OpenConnect-9.12-patched-source.zip'
Write-SourceArchive (Join-Path $projectRoot '.tools/proxifyre-source') 'ProxiFyre-2.6.1-source.zip'
Write-SourceArchive (Join-Path $projectRoot 'build-arovan/external/src/qt-solutions-master') 'QtSolutions-source.zip'
Get-ChildItem -LiteralPath $destination -File | Get-FileHash -Algorithm SHA256 | Select-Object Path,Hash | ConvertTo-Json | Set-Content (Join-Path $destination 'manifest.json')
Write-Output 'Source archives created. Consult docs/third-party.md before public binary distribution.'
