# Install a built CableAudio driver package on a test machine.
# Elevated PowerShell required.
param(
    [Parameter(Mandatory = $true)]
    [string]$Package,
    [string]$HardwareId = "Root\AudioMixerCable"
)

$ErrorActionPreference = "Stop"

$principal = New-Object Security.Principal.WindowsPrincipal(
    [Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw "Run this script from an elevated PowerShell."
}

$inf = Get-ChildItem -Path $Package -Filter *.inf -Recurse |
    Where-Object { $_.Name -match "CableAudio" } |
    Select-Object -First 1
if (-not $inf) {
    throw "No CableAudio .inf found under $Package"
}

$cat = Get-ChildItem -Path $inf.Directory -Filter *.cat | Select-Object -First 1
if (-not $cat) {
    throw "No catalog (.cat) beside $($inf.FullName). The package must be signed."
}

Write-Host "Installing $($inf.Name) ..."
pnputil /add-driver $inf.FullName /install
if ($LASTEXITCODE -ne 0) {
    throw "pnputil failed with $LASTEXITCODE"
}

# Root-enumerated devices need an explicit device instance. devcon ships with
# the WDK; pnputil cannot create one on older builds.
$devcon = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\Tools\x64\devcon.exe"
if (Test-Path $devcon) {
    & $devcon install $inf.FullName $HardwareId
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "devcon returned $LASTEXITCODE. Check Device Manager for CableAudio."
    }
} else {
    Write-Warning "devcon.exe not found; create the device from Device Manager (Add legacy hardware)."
}

Write-Host "Done. Look for 'Speakers (Virtual Cable 01..04)' and 'Microphone (Virtual Cable 01..04)' in the Sound panel."
