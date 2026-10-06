$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$output = Join-Path $root 'dist/test-signed'
$app = Join-Path $root 'dist/GozarnoVPN/GozarnoVPN.exe'
$setup = Join-Path $root 'dist/GozarnoVPN-1.2.0-Setup-x64.exe'
New-Item -ItemType Directory -Force -Path $output | Out-Null
if (Test-Path (Join-Path $output 'certificate.json')) { throw 'Test certificate already created; refusing to replace it.' }
$cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=Gozarno Development' -FriendlyName 'Gozarno test signing only' -CertStoreLocation 'Cert:\CurrentUser\My' -KeyAlgorithm RSA -KeyLength 3072 -HashAlgorithm SHA256 -KeyExportPolicy NonExportable -NotAfter (Get-Date).AddYears(1)
Export-Certificate -Cert $cert -FilePath (Join-Path $output 'Gozarno-Development-Test.cer') | Out-Null
$cert | Select-Object Subject,Thumbprint,NotBefore,NotAfter | ConvertTo-Json | Set-Content (Join-Path $output 'certificate.json') -Encoding utf8
Copy-Item -LiteralPath $app -Destination (Join-Path $output 'original-app.exe')
Copy-Item -LiteralPath $setup -Destination (Join-Path $output 'original-setup.exe')
function Sign-TestFile([string]$path) {
  Set-AuthenticodeSignature -FilePath $path -Certificate $cert -HashAlgorithm SHA256 | Out-Null
  $signature = Get-AuthenticodeSignature -FilePath $path
  if ($signature.SignerCertificate.Thumbprint -ne $cert.Thumbprint -or $signature.Status -eq 'HashMismatch' -or $signature.Status -eq 'NotSigned') { throw "Signature failed: $path ($($signature.Status))" }
  $signature | Select-Object Path,Status,StatusMessage
}
try {
  Sign-TestFile $app
  Copy-Item -LiteralPath $app -Destination (Join-Path $output 'GozarnoVPN.exe')
  & (Join-Path $root '.tools/msys64/mingw64/bin/makensis.exe') (Join-Path $root 'installer/GozarnoVPN.nsi') *> (Join-Path $output 'installer-build.log')
  if ($LASTEXITCODE) { throw 'Test installer build failed' }
  $signedSetup = Join-Path $output 'GozarnoVPN-1.2.0-TestSigned-Setup-x64.exe'
  Copy-Item -LiteralPath $setup -Destination $signedSetup
  Sign-TestFile $signedSetup
  Get-FileHash -LiteralPath $signedSetup -Algorithm SHA256 | Select-Object Path,Hash | ConvertTo-Json | Set-Content (Join-Path $output 'setup-sha256.json')
} finally {
  Copy-Item -LiteralPath (Join-Path $output 'original-app.exe') -Destination $app
  Copy-Item -LiteralPath (Join-Path $output 'original-setup.exe') -Destination $setup
}
# Deliberately do not install this certificate into Trusted Root or Trusted Publishers.
# The private key stays non-exportable in the current Windows user's certificate store.
