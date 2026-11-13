# Build an ISO with the transfer files (driver package + test tool) so the VM
# can read them without VirtualBox shared folders.
#
#   powershell -ExecutionPolicy Bypass -File tools\make-transfer-iso.ps1
#
# Output: handoff\cable-test.iso. Attach it in the VM via
# Devices -> Optical Drives -> Choose a disk file...
param(
    [string]$Source = "handoff",
    [string]$Iso = "handoff\cable-test.iso",
    [string]$Volume = "CABLETEST"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$src = (Resolve-Path $Source).Path
$isoPath = Join-Path (Get-Location) $Iso

# Stage into a scratch tree so previously built ISOs (which live next to the
# package) never end up inside the new image.
$stage = Join-Path $env:TEMP ("cable-iso-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $stage | Out-Null
foreach ($item in Get-ChildItem $src -Force) {
    if ($item.Extension -eq ".iso") { continue }
    Copy-Item $item.FullName -Destination $stage -Recurse -Force
}
$src = $stage

Add-Type -TypeDefinition @"
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;

public static class CableIso
{
    public static void Copy(object streamObject, string path)
    {
        var stream = (IStream)streamObject;
        using (var file = File.Create(path))
        {
            var buffer = new byte[65536];
            IntPtr read = Marshal.AllocHGlobal(sizeof(int));
            try
            {
                while (true)
                {
                    stream.Read(buffer, buffer.Length, read);
                    int count = Marshal.ReadInt32(read);
                    if (count <= 0) break;
                    file.Write(buffer, 0, count);
                }
            }
            finally
            {
                Marshal.FreeHGlobal(read);
            }
        }
    }
}
"@

if (Test-Path $isoPath) { Remove-Item $isoPath -Force }

$fsi = New-Object -ComObject IMAPI2FS.MsftFileSystemImage
$fsi.FileSystemsToCreate = 3   # ISO9660 + Joliet
$fsi.VolumeName = $Volume
$fsi.Root.AddTree($src, $false)
$image = $fsi.CreateResultImage()
[CableIso]::Copy($image.ImageStream, $isoPath)

Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue

$info = Get-Item $isoPath
"ISO: $($info.FullName) ($([math]::Round($info.Length / 1MB, 2)) MB)"
