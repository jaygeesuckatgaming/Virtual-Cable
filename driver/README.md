# CableAudio driver

Working copy of the Windows virtual audio cable, based on
`DestinyObs/Virtual-Audio-Cable` (branch `loopback-test-signed`, commit
`66bebc0`), vendored under `vendor/virtual-audio-cable`.

That fork derives from `VirtualDrivers/Virtual-Audio-Driver` (MIT), which in
turn derives from Microsoft's Simple Audio Sample (MS-PL). See the licenses in
`vendor/virtual-audio-cable/`.

## What it does today

- One render endpoint (`Virtual Cable Output`) and one capture endpoint
  (`Virtual Cable Input`).
- `Main/loopback.cpp` + `Inc/loopback.h`: a 48 kHz stereo 32-bit PCM ring
  (500 ms, spin-locked, 16/24/32-bit conversion, 60 ms latency cap).
- `CMiniportWaveRTStream::LoopbackRender` feeds the ring from the render path;
  `WriteBytes` fills the capture buffer from it, silence on underrun.
- Only 48 kHz stereo integer PCM is looped; other formats give silence on the
  capture side.
- INF targets Windows 10 1903+ and names the endpoints "Virtual Cable
  Output"/"Virtual Cable Input".

## Local changes on top of the vendored tree

1. `Utilities/savedata.cpp`: `RtlStringCchPrintfW` replaced with
   `RtlStringCchCopyW`. The printf path pulls the user-mode UCRT into the
   kernel link (`__stdio_common_vswprintf`).
2. The driver is linked with the KMDF entry point `FxDriverEntry`
   (`driver/build.py --entry FxDriverEntry`). Using `DriverEntry` skips the
   framework bind and crashes the machine.

## Build and package

```
python driver/build.py --project driver/CableAudio --name CableAudio --entry FxDriverEntry
powershell -ExecutionPolicy Bypass -File tools/package-driver.ps1
```

`build.py` uses the installed WDK kit and MSVC directly (no Visual Studio
driver toolset). The package lands in `handoff/package` with signed
`VirtualAudioDriver.inf/sys/cat`.

## Next

- Per-cable rings: the vendored loopback uses one global ring, so every render
  feeds every capture. Replace it with one ring per cable pair and duplicate the
  miniport pairs to 4 render + 4 capture, named `Virtual Cable 01..04`.
- Tune the latency cap for the mixer (~25-30 ms) and decide whether to support
  32-bit float / more sample rates.
