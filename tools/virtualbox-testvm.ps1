# Create a VirtualBox test VM for the CableAudio driver on D:.
# Use this when Hyper-V is unavailable (Windows Home editions).
#
#   .\virtualbox-testvm.ps1 -Iso "D:\ISOs\Win10_22H2.iso"
#   .\virtualbox-testvm.ps1 -Iso "D:\ISOs\Win11.iso" -Windows11
#
# Requires VirtualBox 7.x (https://www.virtualbox.org/wiki/Downloads).
param(
    [Parameter(Mandatory = $true)]
    [string]$Iso,
    [string]$VmRoot = "D:\VMs",
    [string]$Name = "AudioCableTest",
    [int]$MemoryMB = 8192,
    [int]$DiskMB = 81920,
    [int]$Cpu = 4,
    [switch]$Windows11
)

$ErrorActionPreference = "Stop"

$vbox = Join-Path $env:ProgramFiles "Oracle\VirtualBox\VBoxManage.exe"
if (-not (Test-Path $vbox)) {
    throw "VirtualBox is not installed. Install it from https://www.virtualbox.org/wiki/Downloads and run this again."
}
if (-not (Test-Path $Iso)) {
    throw "ISO not found: $Iso"
}

$vmDir = Join-Path $VmRoot $Name
New-Item -ItemType Directory -Force -Path $vmDir | Out-Null
$disk = Join-Path $vmDir "$Name.vdi"

$existing = & $vbox list vms
if ($existing -match [regex]::Escape("`"$Name`"")) {
    throw "A VirtualBox VM named '$Name' already exists. Remove it first (VBoxManage unregistervm --delete $Name)."
}

$osType = if ($Windows11) { "Windows11_64" } else { "Windows10_64" }

& $vbox createvm --name $Name --ostype $osType --basefolder $VmRoot --register
& $vbox modifyvm $Name --memory $MemoryMB --cpus $Cpu --firmware efi --boot1 dvd --boot2 disk --boot3 none --boot4 none
& $vbox modifyvm $Name --audio none --usb on --usb-xhci on --graphicscontroller vboxsvga --vram 128
if ($Windows11) {
    & $vbox modifyvm $Name --tpm-type 2.0
}
& $vbox createmedium disk --filename $disk --size $DiskMB --variant Standard
& $vbox storagectl $Name --name SATA --add sata --controller IntelAhci --portcount 2
& $vbox storageattach $Name --storagectl SATA --port 0 --device 0 --type hdd --medium $disk
& $vbox storageattach $Name --storagectl SATA --port 1 --device 0 --type dvddrive --medium $Iso

& $vbox startvm $Name

Write-Host ""
Write-Host "VM created and started. Next:" -ForegroundColor Green
Write-Host "  1. Install Windows from the ISO, then take a snapshot named 'clean-install':"
Write-Host "       VBoxManage snapshot $Name take clean-install"
Write-Host "  2. In the VM (admin):"
Write-Host "       bcdedit /set testsigning on"
Write-Host "       shutdown /r /t 0"
Write-Host "  3. Make sure Memory Integrity (Core isolation) is OFF in the VM,"
Write-Host "     otherwise test-signed drivers are blocked."
Write-Host "  4. Copy tools\signing\test-signing.cer into the VM and trust it,"
Write-Host "     then install the built package with tools\install.ps1."
