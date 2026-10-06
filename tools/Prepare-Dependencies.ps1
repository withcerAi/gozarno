param([string]$Destination = (Join-Path $PSScriptRoot '../build-dependencies'))
$ErrorActionPreference = 'Stop'
$targetRoot = [IO.Path]::GetFullPath($Destination)
New-Item -ItemType Directory -Force -Path $targetRoot | Out-Null
function Receive-VerifiedFile([string]$Url, [string]$Name, [string]$Hash, [switch]$Signed) {
    $path = Join-Path $targetRoot $Name
    if (!(Test-Path -LiteralPath $path)) { Invoke-WebRequest -Uri $Url -OutFile $path }
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $Hash.ToLowerInvariant()) {
        throw "Checksum mismatch: $Name"
    }
    if ($Signed -and (Get-AuthenticodeSignature -LiteralPath $path).Status -ne 'Valid') { throw "Invalid signature: $Name" }
    return $path
}
$release = 'https://github.com/wiresock/proxifyre/releases/download/v2.6.1'
$checksum = Invoke-RestMethod -Uri "$release/ProxiFyre-v2.6.1-x64.zip.sha256"
$proxyHash = ([regex]::Match([string]$checksum, '[a-fA-F0-9]{64}')).Value
if (!$proxyHash) { throw 'Missing ProxiFyre checksum' }
$payload = Receive-VerifiedFile "$release/ProxiFyre-v2.6.1-x64.zip" 'proxifyre.zip' $proxyHash
Expand-Archive -LiteralPath $payload -DestinationPath (Join-Path $targetRoot 'backend') -Force
Receive-VerifiedFile 'https://github.com/wiresock/ndisapi/releases/download/v3.6.2/Windows.Packet.Filter.3.6.2.1.x64.msi' `
    'packet-filter.msi' '9c388c0b7f189f7fa98720bae2caecf7d64f30910838b80b438ecf8956b8502c' -Signed | Out-Null
Receive-VerifiedFile 'https://download.visualstudio.microsoft.com/download/pr/0b44c2d1-8944-4834-a01a-c9a225f8088a/CC0FF0EB1DC3F5188AE6300FAEF32BF5BEEBA4BDD6E8E445A9184072096B713B/VC_redist.x64.exe' `
    'vc-runtime.exe' 'cc0ff0eb1dc3f5188ae6300faef32bf5beeba4bdd6e8e445a9184072096b713b' -Signed | Out-Null
$licenseRoot = Join-Path $targetRoot 'licenses'
New-Item -ItemType Directory -Force -Path $licenseRoot | Out-Null
Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/wiresock/proxifyre/v2.6.1/LICENSE' -OutFile (Join-Path $licenseRoot 'ProxiFyre-AGPL-3.0.txt')
Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/wiresock/ndisapi/v3.6.2/LICENSE' -OutFile (Join-Path $licenseRoot 'NDISAPI-MIT.txt')
Invoke-WebRequest -Uri 'https://www.wintun.net/builds/wintun-0.14.1.zip' -OutFile (Join-Path $targetRoot 'wintun.zip')
Expand-Archive -LiteralPath (Join-Path $targetRoot 'wintun.zip') -DestinationPath (Join-Path $targetRoot 'wintun') -Force
$driver = Join-Path $targetRoot 'wintun/wintun/bin/amd64/wintun.dll'
if ((Get-AuthenticodeSignature -LiteralPath $driver).Status -ne 'Valid') { throw 'Invalid Wintun signature' }
Get-ChildItem -LiteralPath $targetRoot -File | Get-FileHash -Algorithm SHA256 |
    Select-Object @{n='File';e={Split-Path $_.Path -Leaf}},Hash |
    ConvertTo-Json | Set-Content (Join-Path $targetRoot 'manifest.json')
Write-Output 'Dependencies staged and verified; no drivers or services were installed.'
