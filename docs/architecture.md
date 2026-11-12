# Architecture

## Goal

A kernel-mode audio driver that presents four full-duplex "cables". Audio
rendered into a cable's render endpoint appears on the paired capture endpoint,
clocked together so the two streams never drift.

## Device model

One root-enumerated driver instance (`Root\AudioMixerCable`) with eight audio
subdevices, created by the adapter's `CreateMiniports`:

| Subdevice | Endpoint friendly name | Direction |
|---|---|---|
| render 1..4 | `Speakers (Virtual Cable 01..04)` | host → driver |
| capture 1..4 | `Microphone (Virtual Cable 01..04)` | driver → host |

Names are assigned by index in the miniport/topology description, not by the
INF, so all four cables come from one signed package.

Deliberately matching the names used by the current USB/IP implementation means
the mixer's `cable_label()`, strip headers, APPS list and saved scenes keep
working unchanged.

## Loopback

Each cable owns one non-paged ring allocated at adapter start.

- Render miniport: WaveRT gives the host a memory-mapped cyclic buffer. On every
  service callback the driver copies the frames between the last position and
  the new position into the cable ring.
- Capture miniport: copies frames from the ring into its WaveRT buffer; underruns
  are filled with silence.
- One 1 ms kernel timer advances both stream positions. Because render and
  capture share the timer, no rate conversion or drift correction is needed.
- The read pointer trails the write pointer by `LatencyMs` (default: two engine
  periods + 5 ms, i.e. about 10 ms at 48 kHz). The value lives under the device's
  registry key (`LatencyMs`, DWORD, clamp 2..100 ms).

Formats are stereo PCM 16/24-bit and 32-bit float at 44.1/48/88.2/96 kHz. The
driver is format-converting-free: it accepts those formats and copies frames
verbatim.

## Mixing and multi-client

Windows' audio engine mixes applications in shared mode, so the driver sees one
stream per endpoint instance. Multiple applications can therefore play into the
same cable, and multiple applications can record from it, exactly like a
hardware endpoint. Exclusive mode bypasses the engine and is supported.

## Power and lifecycle

- `IAdapterPowerManagement` from SysVAD is kept: the ring survives sleep, and
  streams are torn down/re-created around D0 transitions.
- No user-mode service is required.

## Differences from SysVAD

- All APO, keyword-spotter, Bluetooth, USB-HS, A2DP and HDMI/SPDIF subdevices are
  removed.
- The sample's "loopback" endpoint (which synthesises a tone for AEC testing) is
  replaced by the real render→capture ring.
- Endpoint names and the cable count come from build-time constants.

## Mixer integration (Audio-Mixer-PRO)

When the driver endpoints are present the mixer must:

- skip the USB/IP cable server and show "driver detected" in the CABLES panel;
- feed B1/B2 by playing to `Speakers (Virtual Cable 01/02)` through cpal instead
  of injecting PCM into the USB device's capture path;
- otherwise behave identically (strips, APPS, ducking, dynamics, EQ).
