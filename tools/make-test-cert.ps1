# Create a self-signed code-signing certificate for driver testing and trust it
# on this machine. Run in an elevated PowerShell. The PFX is written to
# tools\signing and is git-ignored: never commit it.
param(
    [string]$Name = "AudioMixerCable Test Signing",
    [string]$Password = "test"
)

$ErrorActionPreference = "Stop"

$principal = New-Object Security.Principal.WindowsPrincipal(
    [Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw "Run this script from an elevated PowerShell."
}

$dir = Join-Path $PSScriptRoot "signing"
New-Item -ItemType Directory -Force -Path $dir | Out-Null
$pfx = Join-Path $dir "test-signing.pfx"
$cer = Join-Path $dir "test-signing.cer"

$cert = New-SelfSignedCertificate `
    -Type CodeSigningCert `
    -Subject "CN=$Name" `
    -KeyUsage DigitalSignature `
    -KeyAlgorithm RSA `
    -KeyLength 2048 `
    -HashAlgorithm SHA256 `
    -CertStoreLocation "Cert:\CurrentUser\My" `
    -NotAfter (Get-Date).AddYears(3)

$secure = ConvertTo-SecureString -String $Password -Force -AsPlainText
Export-PfxCertificate -Cert $cert -FilePath $pfx -Password $secure | Out-Null
Export-Certificate -Cert $cert -FilePath $cer | Out-Null

Import-Certificate -FilePath $cer -CertStoreLocation "Cert:\LocalMachine\Root" | Out-Null
Import-Certificate -FilePath $cer -CertStoreLocation "Cert:\LocalMachine\TrustedPublisher" | Out-Null

Write-Host "Certificate created and trusted."
Write-Host "  PFX: $pfx"
Write-Host "  CER: $cer"
Write-Host ""
Write-Host "Enable test signing once, then reboot:"
Write-Host "  bcdedit /set testsigning on"
Write-Host ""
Write-Host "Sign the built package with:"
Write-Host "  signtool sign /f `"$pfx`" /p $Password /fd SHA256 /a <file.sys>"
Write-Host "  inf2cat /driver:<package dir> /os:10_X64"
Write-Host "  signtool sign /f `"$pfx`" /p $Password /fd SHA256 /a <driver.cat>"
