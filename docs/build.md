# Building and testing

## Prerequisites (host)

1. Visual Studio 2022 with **Desktop development with C++** (installed).
2. **Windows Driver Kit** for the installed SDK (10.0.26100). The Kits folder
   currently has no `km` libraries, so the WDK is not installed yet.
   Install the WDK from Microsoft ("Download the WDK") and the *Windows Driver*
   workload in the VS Installer (C++, ATL, Spectre-mitigated libs).
3. Git.

## Prerequisites (test machine)

Kernel drivers must be developed against a VM, never the daily machine.

Disk layout: the SDK and WDK install under `C:\Program Files (x86)\Windows
Kits\10` (no drive choice, roughly 3 GB). Put the VM on the data drive - the
script defaults to `D:\VMs`, and D: has ~290 GB free.

1. Enable Hyper-V (elevated PowerShell, then reboot):
   ```
   Enable-WindowsOptionalFeature -Online -FeatureName Microsoft-Hyper-V-All -All
   ```
2. Create the VM on D: with a Windows 10 22H2 or 11 ISO:
   ```
   .\tools\hyperv-testvm.ps1 -Iso "D:\ISOs\Win10_22H2.iso"
   .\tools\hyperv-testvm.ps1 -Iso "D:\ISOs\Win11.iso" -Windows11   # adds a vTPM
   ```
   Windows 10 22H2 is simpler (no TPM requirement) and matches the support
   target. Take a checkpoint named `clean-install` after Windows is installed.
3. Inside the VM (admin):
   ```
   bcdedit /set testsigning on
   shutdown /r /t 0
   ```
4. Import the certificate produced by `tools\make-test-cert.ps1` into
   `LocalMachine\Root` and `LocalMachine\TrustedPublisher`, then install the
   driver with `tools\install.ps1`.

### Windows Home editions

`Microsoft-Hyper-V-All` does not exist on Windows 10/11 **Home**, so neither
the feature nor Hyper-V Manager can be installed. Use VirtualBox 7 instead,
which shares the CPU virtualization with the VBS hypervisor through WHPX:

```
.\tools\virtualbox-testvm.ps1 -Iso "D:\ISOs\Win10_22H2.iso"
.\tools\virtualbox-testvm.ps1 -Iso "D:\ISOs\Win11.iso" -Windows11
```

The VM is created under `D:\VMs` with a SATA disk, EFI firmware, no virtual
sound card (the driver's endpoints are the loopback) and a snapshot point after
Windows is installed.

If Windows 11 setup still says "This PC can't run Windows 11" (VirtualBox does
not always expose a usable TPM 2.0 / Secure Boot to the guest), press
**Shift+F10** at that screen and run:

```
reg add HKLM\SYSTEM\Setup\LabConfig /v BypassTPMCheck /t REG_DWORD /d 1 /f
reg add HKLM\SYSTEM\Setup\LabConfig /v BypassSecureBootCheck /t REG_DWORD /d 1 /f
reg add HKLM\SYSTEM\Setup\LabConfig /v BypassRAMCheck /t REG_DWORD /d 1 /f
reg add HKLM\SYSTEM\Setup\LabConfig /v BypassCPUCheck /t REG_DWORD /d 1 /f
```

Close the prompt, click **Back**, then **Next** again. The VM runs unsupported
but is fine for driver testing (feature updates may refuse to install).

Alternatively install Windows 10 22H2, which has no TPM/Secure Boot check. Inside the guest, keep Memory Integrity (Core isolation)
off, otherwise test-signed drivers are refused.

No physical sound card is required in the VM: the cable's render and capture
endpoints are the loopback the test uses.

Windows setup will ask for a product key: choose **I don't have a product
key** and pick any edition (Home is fine for driver testing). The VM runs
unactivated, which is expected for a throwaway test machine. On Windows 11,
setup also demands a Microsoft account - press Shift+F10 and use a local
account if you prefer offline.

## Build the driver

Open `driver/CableAudio.sln` in Visual Studio with the WDK installed, select
`x64` + `Debug`/`Release`, build. The package lands in
`driver/x64/<config>/CableAudio/`.

If the SysVAD-derived projects complain about v142 toolset/ATL, retarget them to
v143 in the VS project properties.

## Install / uninstall

```
powershell -ExecutionPolicy Bypass -File tools/make-test-cert.ps1   # once per VM
powershell -ExecutionPolicy Bypass -File tools/install.ps1  -Package driver\x64\Debug\CableAudio
powershell -ExecutionPolicy Bypass -File tools/uninstall.ps1
```

`install.ps1` uses `pnputil /add-driver ... /install` on the built package and
`devcon`/`pnputil` to create the root device instance.

## Loopback test

`tests/wasapi_loopback_test` is a user-mode tool (no WDK needed):

```
cmake -S tests/wasapi_loopback_test -B tests/wasapi_loopback_test/out
cmake --build tests/wasapi_loopback_test/out --config Release
tests\wasapi_loopback_test\out\Release\wasapi_loopback_test.exe --list
tests\wasapi_loopback_test\out\Release\wasapi_loopback_test.exe ^
    --render "Virtual Cable 01" --capture "Virtual Cable 01" --seconds 10
```

It renders a 1 kHz click train, records the paired capture endpoint and reports
level, approximate round-trip latency, glitches and clock drift. Run it before
the driver exists against any existing loopback (VB-Cable, Voicemeeter, or the
USB/IP cable) to validate the harness itself.
