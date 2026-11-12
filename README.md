# Virtual Cable

A real Windows virtual audio cable: a PortCls/WaveRT audio driver that exposes
paired render and capture endpoints, so any application can play into
`Speakers (Virtual Cable NN)` and another can record from
`Microphone (Virtual Cable NN)` with no USB/IP device server and no attach
churn.

Development base: Microsoft's [SysVAD](https://github.com/microsoft/Windows-driver-samples/tree/main/audio/sysvad)
sample (MIT), vendored under `driver/vendor/sysvad`.

## Status

| Milestone | State |
|---|---|
| M0 toolchain + VM | in progress (WDK and a Hyper-V test VM required) |
| M1 endpoints | not started |
| M2 real loopback | not started |
| M3 robustness | not started |
| M4 mixer integration | not started |
| M5 packaging | not started |

## Layout

```
driver/     derived CableAudio driver project (built with the WDK)
  vendor/   unmodified SysVAD sample + its MIT licence
tests/      user-mode WASAPI loopback test (tone in / capture out)
tools/      test certificate, install and uninstall scripts
docs/       architecture, build and signing notes
```

## Target

- Windows 10 22H2 and Windows 11, x64.
- Four cables, endpoints named exactly
  `Speakers (Virtual Cable 01..04)` and `Microphone (Virtual Cable 01..04)` so
  Audio Mixer PRO keeps its existing device mapping.
- Stereo, 44.1/48/88.2/96 kHz, 16/24-bit PCM and 32-bit float.
- One shared clock per cable: render and capture positions advance together.

See `docs/architecture.md` and `docs/build.md`.
