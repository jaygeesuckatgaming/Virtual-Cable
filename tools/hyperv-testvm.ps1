# Create a Hyper-V test VM for the CableAudio driver on D:.
# Run from an elevated PowerShell.
#
#   .\hyperv-testvm.ps1 -Iso "D:\ISOs\Win10_22H2.iso"
#   .\hyperv-testvm.ps1 -Iso "D:\ISOs\Win11.iso" -Windows11
#
# The VM is stored under D:\VMs by default so the system drive is not used.
param(
    [Parameter(Mandatory = $true)]
    [string]$Iso,
    [string]$VmRoot = "D:\VMs",
    [string]$Name = "AudioCableTest",
    [int]$MemoryGB = 8,
    [int]$DiskGB = 80,
    [int]$Cpu = 4,
    [string]$SwitchName = "Default Switch",
    [switch]$Windows11
)

$ErrorActionPreference = "Stop"

$principal = New-Object Security.Principal.WindowsPrincipal(
    [Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw "Run this script from an elevated PowerShell."
}

# --- Hyper-V must be enabled first -----------------------------------------
$feature = Get-WindowsOptionalFeature -Online -FeatureName Microsoft-Hyper-V-All
if ($feature.State -ne "Enabled") {
    Write-Host "Hyper-V is not enabled. Enabling it now..."
    Enable-WindowsOptionalFeature -Online -FeatureName Microsoft-Hyper-V-All -All -NoRestart | Out-Null
    Write-Host ""
    Write-Host "Hyper-V was enabled. Reboot, then run this script again." -ForegroundColor Yellow
    exit 0
}
if (-not (Get-Service vmms -ErrorAction SilentlyContinue)) {
    throw "The Hyper-V Virtual Machine Management service (vmms) is missing. Reboot after enabling Hyper-V."
}

if (-not (Test-Path $Iso)) {
    throw "ISO not found: $Iso"
}

$vmDir = Join-Path $VmRoot $Name
if (Get-VM -Name $Name -ErrorAction SilentlyContinue) {
    throw "A VM named '$Name' already exists. Remove it (Remove-VM) or pass -Name with another name."
}
New-Item -ItemType Directory -Force -Path $vmDir | Out-Null

$vhd = Join-Path $vmDir "$Name.vhdx"
Write-Host "Creating $Name (Gen 2, ${MemoryGB} GB RAM, ${DiskGB} GB disk) in $vmDir"
New-VM -Name $Name `
    -Generation 2 `
    -MemoryStartupBytes ($MemoryGB * 1GB) `
    -NewVHDPath $vhd `
    -NewVHDSizeBytes ($DiskGB * 1GB) `
    -SwitchName $SwitchName `
    -Path $vmDir | Out-Null

Set-VMProcessor -VMName $Name -Count $Cpu
Set-VMFirmware -VMName $Name -EnableSecureBoot On
Set-VM -Name $Name -AutomaticCheckpointsEnabled $false

if ($Windows11) {
    # Windows 11 requires a virtual TPM.
    Set-VMKeyProtector -VMName $Name -NewLocalKeyProtector
    Enable-VMTPM -VMName $Name
}

Add-VMDvdDrive -VMName $Name -Path $Iso
$dvd = Get-VMDvdDrive -VMName $Name
Set-VMFirmware -VMName $Name -FirstBootDevice $dvd

Start-VM -Name $Name

Write-Host ""
Write-Host "VM created and started. Next:" -ForegroundColor Green
Write-Host "  1. Install Windows, then take a checkpoint named 'clean-install'."
Write-Host "  2. In the VM (admin):"
Write-Host "       bcdedit /set testsigning on"
Write-Host "       shutdown /r /t 0"
Write-Host "  3. Copy tools\signing\test-signing.cer into the VM, install it into"
Write-Host "     LocalMachine\Root and LocalMachine\TrustedPublisher."
Write-Host "  4. Copy the built driver package into the VM and run tools\install.ps1."
Write-Host ""
Write-Host "No physical sound card is needed: the cable endpoints are the loopback."
