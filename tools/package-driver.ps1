# Build a signed, installable driver package from the compiled .sys and the
# sample INX. Run from the repository root (Windows PowerShell).
#
#   powershell -ExecutionPolicy Bypass -File tools\package-driver.ps1
#
# Output: handoff\package\ (INF, SYS, CAT, devcon.exe)
param(
    [string]$Sys = "driver\CableAudio\out\CableAudio.sys",
    [string]$Inx = "driver\CableAudio\Main\VirtualAudioDriver.inx",
    [string]$Out = "handoff\package",
    [string]$Pfx = "tools\signing\test-signing.pfx",
    [string]$PfxPassword = "test",
    [string]$SysName = "virtualaudiodriver.sys",
    [string]$InfName = "VirtualAudioDriver.inf",
    [string]$CatName = "VirtualAudioDriver.cat"
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

if (-not (Test-Path $Sys)) { throw "Missing $Sys (build the driver first)." }
if (-not (Test-Path $Inx)) { throw "Missing $Inx" }
if (-not (Test-Path $Pfx)) { throw "Missing $Pfx (run tools\make-test-cert.ps1)." }

$kits = "${env:ProgramFiles(x86)}\Windows Kits\10"
$version = (Get-ChildItem "$kits\Include" -Directory |
    Where-Object { Test-Path (Join-Path $_.FullName "km\ntddk.h") } |
    Sort-Object { [version]$_.Name } -Descending |
    Select-Object -First 1).Name
if (-not $version) { throw "No WDK found under $kits" }

$inf2cat = Join-Path $kits "bin\$version\x86\Inf2Cat.exe"
$signtool = Join-Path $kits "bin\$version\x64\signtool.exe"
$devcon = Join-Path $kits "Tools\$version\x64\devcon.exe"

Remove-Item $Out -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $Out | Out-Null

# The sample INF expects simpleaudiosample.sys; keep that name for now (M1a).
$sysName = $SysName
$infName = $InfName
$catName = $CatName

# INX -> INF. StampInf is avoided here: only two tokens are involved and the
# architecture token must become "amd64" (not "x64").
$text = Get-Content $Inx -Raw
$text = $text.Replace('$ARCH$', 'amd64').Replace('$KMDFVERSION$', '1.33')
$inf = Join-Path $Out $infName
Set-Content -Path $inf -Value $text -Encoding Unicode

Copy-Item $Sys (Join-Path $Out $sysName) -Force

Write-Host "Building catalog..."
& $inf2cat /driver:$Out /os:10_X64
if ($LASTEXITCODE -ne 0) { throw "Inf2Cat failed ($LASTEXITCODE)." }

Write-Host "Signing driver and catalog..."
& $signtool sign /f $Pfx /p $PfxPassword /fd SHA256 /a (Join-Path $Out $sysName)
if ($LASTEXITCODE -ne 0) { throw "signtool failed for the .sys" }
& $signtool sign /f $Pfx /p $PfxPassword /fd SHA256 /a (Join-Path $Out $catName)
if ($LASTEXITCODE -ne 0) { throw "signtool failed for the .cat" }

if (Test-Path $devcon) {
    Copy-Item $devcon (Join-Path $Out "devcon.exe") -Force
}

Get-ChildItem $Out | Select-Object Name, Length | Format-Table -AutoSize
Write-Host "Package ready: $Out"
