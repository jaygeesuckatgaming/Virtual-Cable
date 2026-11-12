# wasapi_loopback_test

User-mode validation tool for the virtual cable. Renders a 1 kHz click train
into one render endpoint and records a capture endpoint, reporting:

- captured tone level (dBFS) and a rough SNR,
- approximate round-trip latency (QPC; includes the engine's capture buffering),
- clock drift between the two streams (ppm),
- glitches: render buffer-full events, capture starvation, non-finite samples.

No WDK required - it only needs the Windows SDK.

```
cmake -G "Visual Studio 17 2022" -A x64 -S . -B out
cmake --build out --config Release

out\Release\wasapi_loopback_test.exe --list
out\Release\wasapi_loopback_test.exe --render "Virtual Cable 01" --capture "Virtual Cable 01" --seconds 10
```

Limitations: latency is a QPC estimate because the click's exact play time is
not exposed by WASAPI; treat it as an upper bound that includes the capture
endpoint's buffering. Drift and level are measured directly and are the metrics
that matter for the driver's loopback.
