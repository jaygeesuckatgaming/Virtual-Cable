/*++

Module Name:

    loopback.h

Abstract:

    Per-cable speaker-to-microphone loopback.

    Each cable pair (Virtual Cable 01..04) owns one ring buffer. Audio rendered
    to that cable's speaker endpoint is copied into its ring and played back out
    of the matching microphone endpoint, so the pair behaves as an audio cable.

    The ring holds 48 kHz, 2-channel, 32-bit signed PCM (left-justified, as in
    WAVEFORMATEXTENSIBLE). Each endpoint converts between its own stream format
    and the ring format. Only 48 kHz stereo integer PCM is looped; any other
    stream format produces silence on the microphone side.

    Ring storage and locks are static and zero-initialised, which is a valid
    initial state for KSPIN_LOCK. All functions are callable at
    IRQL <= DISPATCH_LEVEL.

--*/

#ifndef _VIRTUALAUDIODRIVER_LOOPBACK_H_
#define _VIRTUALAUDIODRIVER_LOOPBACK_H_

#define LOOPBACK_CABLES             4
#define LOOPBACK_SAMPLE_RATE        48000
#define LOOPBACK_CHANNELS           2

// Ring capacity: 500 ms.
#define LOOPBACK_RING_FRAMES        (LOOPBACK_SAMPLE_RATE / 2)

// Maximum latency kept between speaker and microphone: 30 ms.
// Older audio is dropped so the microphone never lags far behind.
#define LOOPBACK_MAX_LATENCY_FRAMES (LOOPBACK_SAMPLE_RATE * 30 / 1000)

//
// Returns TRUE if the stream format can be looped and sets *BytesPerSample
// to the container size of one sample (2, 3 or 4 bytes).
//
BOOLEAN
LoopbackIsFormatSupported
(
    _In_opt_ PWAVEFORMATEXTENSIBLE  WaveFormat,
    _Out_    ULONG*                 BytesPerSample
);

//
// Speaker side: copy rendered audio into the cable's ring buffer.
//
VOID
LoopbackWrite
(
    _In_ ULONG                              CableIndex,
    _In_reads_bytes_(ByteCount) const BYTE* Source,
    _In_ ULONG                              ByteCount,
    _In_ ULONG                              BytesPerSample
);

//
// Microphone side: fill the capture buffer from the cable's ring buffer.
// Any shortfall is filled with silence.
//
VOID
LoopbackRead
(
    _In_ ULONG                          CableIndex,
    _Out_writes_bytes_(ByteCount) BYTE* Destination,
    _In_ ULONG                          ByteCount,
    _In_ ULONG                          BytesPerSample
);

#endif // _VIRTUALAUDIODRIVER_LOOPBACK_H_
