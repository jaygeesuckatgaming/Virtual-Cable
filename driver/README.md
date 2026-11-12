# CableAudio driver

The driver project is derived from Microsoft's SysVAD sample, vendored unmodified
in `vendor/sysvad` (MIT, see `vendor/Windows-driver-samples-LICENSE.txt`).

`CableAudio.sln` will be added once the WDK is installed; until then this
directory documents exactly what is derived and what is dropped.

## Keep from SysVAD

| File | Role |
|---|---|
| `adapter.cpp/.h` | adapter, `CreateMiniports`, power management |
| `common.cpp/.h` | `CAdapterCommon`, shared state |
| `hw.cpp/.h` | register/memory helpers |
| `basetopo.cpp/.h` | topology miniport base |
| `kshelper.cpp/.h` | KS helpers |
| `EndpointsCommon/*wavert*` | WaveRT stream base and position registers |
| `EndpointsCommon/*miniport*` | miniport base classes |
| `EndpointsCommon/*topo*` | topology base classes |

## Drop

- `APO/`, `KeywordDetectorAdapter/`, `ContosoKeywordDetector.h`
- `A2dpHpDevice.*`, `BthhfpDevice.*`, `UsbHsDevice.*`
- HDMI/SPDIF/phone subdevices from `TabletAudioSample`
- `ToneGenerator.*` (replaced by the real loopback ring)

## Add

- `CableAudioAdapter.cpp`: one root device, `CreateMiniports` creates four
  render and four capture subdevices named `Virtual Cable 01..04`.
- `CableRing.cpp/.h`: non-paged per-cable ring, write on render service, read
  on capture service, silence on underrun.
- `CableClock.cpp/.h`: one 1 ms timer advancing both stream positions.
- `CableAudio.inf`: root-enumerated `Root\AudioMixerCable`, `LatencyMs` registry
  value, endpoint names.

The build order is: M1 endpoints (names, enumeration) → M2 ring buffer and
clock → M3 robustness.
