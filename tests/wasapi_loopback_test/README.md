# wasapi_loopback_test

User-mode validation tool for the virtual cable. Renders a 1 kHz click train
into one render endpoint and records a capture endpoint, reporting:

- captured tone level (dBFS) and a rough SNR,
- approximate round-trip latency (QPC; includes the engine's capture buffering),
- drift between the two streams, read from each endpoint's own IAudioClock and
  expressed in ppm (the `clock` and `drift` lines),
- click-spacing drift as a second, independent figure (the `interval` line);
  it also reflects this tool's sample bookkeeping, so the clock line is the
  one to trust,
- glitches: render underruns, capture starvation, non-finite and stray samples.

PASS requires at least two matched clicks, a tone above -60 dBFS, no non-finite
samples, both device clocks readable, and |drift| < 200 ppm.

For the isolation check, render into one cable and capture from a different one
with `--expect-silence`: PASS then means the capture stayed silent, i.e. the
cables do not leak into each other. Without that flag the tool reports PASS only
when it hears the tone, so a correctly isolated pair shows `PASS false` - that is
the expected outcome, not a failure.

No WDK required - it only needs the Windows SDK. The executable links the static
C runtime, so it runs on a clean test machine without the VC++ redistributable.

```
cmake -G "Visual Studio 17 2022" -A x64 -S . -B out
cmake --build out --config Release

out\Release\wasapi_loopback_test.exe --list
out\Release\wasapi_loopback_test.exe --render "Virtual Cable 01" --capture "Virtual Cable 01" --seconds 10
out\Release\wasapi_loopback_test.exe --render "Virtual Cable 01" --capture "Virtual Cable 02" --expect-silence --seconds 5
```

Limitations: latency is a QPC estimate because the click's exact play time is
not exposed by WASAPI; treat it as an upper bound that includes the capture
endpoint's buffering (the tool holds 10 ms blocks on both streams). Level and
drift are measured directly and are the metrics that matter for the driver's
loopback.
