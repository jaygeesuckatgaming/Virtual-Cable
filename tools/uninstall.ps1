# Remove the CableAudio driver/package from this machine.
# Elevated PowerShell required.
$ErrorActionPreference = "Stop"

$principal = New-Object Security.Principal.WindowsPrincipal(
    [Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw "Run this script from an elevated PowerShell."
}

Write-Host "Removing CableAudio device instance(s) ..."
$instances = Get-PnpDevice -ErrorAction SilentlyContinue |
    Where-Object { $_.FriendlyName -match "CableAudio|Virtual Cable" -and $_.InstanceId -like "ROOT\*" }
foreach ($device in $instances) {
    Write-Host "  $($device.InstanceId)"
    pnputil /remove-device $device.InstanceId
}

Write-Host "Deleting published driver packages ..."
$output = pnputil /enum-drivers
$current = @{}
foreach ($line in $output) {
    if ($line -match "Published Name:\s+(\S+)") { $current = @{ Published = $Matches[1] } }
    elseif ($line -match "Original Name:\s+(\S+)") {
        $current.Original = $Matches[1]
        if ($current.Original -match "CableAudio" -and $current.Published) {
            Write-Host "  $($current.Published) ($($current.Original))"
            pnputil /delete-driver $current.Published /uninstall /force
        }
        $current = @{}
    }
}

Write-Host "Done. A reboot clears any remaining audio endpoints."
