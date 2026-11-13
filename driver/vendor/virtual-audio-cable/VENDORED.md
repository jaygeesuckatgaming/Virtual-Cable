# Vendored: DestinyObs/Virtual-Audio-Cable

Unmodified snapshot of the `loopback-test-signed` branch of

- https://github.com/DestinyObs/Virtual-Audio-Cable

at commit `66bebc0ede33f2dfcf8f0d361524e82dc0956901`.

That branch adds the speaker-to-microphone loopback (`Source/Inc/loopback.h`,
`Source/Main/loopback.cpp`, hooks in `minwavertstream.cpp`) and a test-signed
build/install pipeline on top of `VirtualDrivers/Virtual-Audio-Driver`, which in
turn derives from Microsoft's Simple Audio Sample.

The working driver in `driver/CableAudio` is a copy of this tree plus two local
build fixes (see `driver/README.md`).

## Licenses

- Original code in this fork: MIT (see `LICENSE`, Copyright (c) 2025 MikeTheTech
  and contributors).
- Microsoft sample-derived code: Microsoft Public License (MS-PL), see
  `THIRD_PARTY_NOTICES.md`.
- Upstream: https://github.com/VirtualDrivers/Virtual-Audio-Driver.

This directory is reference material only. The installers under `tools/` are
kept here for reference and are not used by our build.
