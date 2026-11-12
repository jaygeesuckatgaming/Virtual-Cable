// WASAPI loopback test for the Virtual Cable driver.
//
// Renders a 1 kHz click train into one render endpoint and records the paired
// capture endpoint. Reports the captured level, approximate round-trip latency
// (QPC, includes the engine's capture buffering), clock drift between the two
// streams and a count of glitches. Works with any loopback device (the USB/IP
// cable, VB-Cable, Voicemeeter) so the harness can be validated before the
// driver exists.
//
// Usage:
//   wasapi_loopback_test --list
//   wasapi_loopback_test --render "Virtual Cable 01" --capture "Virtual Cable 01"
//                        [--seconds 10] [--freq 1000]

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <objbase.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr double kClickPeriodSeconds = 0.5;
constexpr double kClickLengthSeconds = 0.002;
constexpr float kClickAmplitude = 0.5f;

// PKEY_Device_FriendlyName, spelled out so propsys.lib is not required.
const PROPERTYKEY kFriendlyName = {
    {0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}},
    14};

struct ComInit {
    ComInit() { CoInitializeEx(nullptr, COINIT_MULTITHREADED); }
    ~ComInit() { CoUninitialize(); }
};

std::string Narrow(const wchar_t* text) {
    if (!text) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(size > 0 ? size - 1 : 0), '\0');
    if (size > 1) WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), size, nullptr, nullptr);
    return out;
}

std::wstring Widen(const std::string& text) {
    int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring out(static_cast<size_t>(size > 0 ? size - 1 : 0), L'\0');
    if (size > 1) MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), size);
    return out;
}

bool NameMatches(IMMDevice* device, const std::string& needle) {
    IPropertyStore* store = nullptr;
    if (FAILED(device->OpenPropertyStore(STGM_READ, &store))) return false;
    PROPVARIANT value;
    PropVariantInit(&value);
    bool match = false;
    if (SUCCEEDED(store->GetValue(kFriendlyName, &value)) && value.vt == VT_LPWSTR) {
        std::string name = Narrow(value.pwszVal);
        std::string lowerName = name;
        std::string lowerNeedle = needle;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        std::transform(lowerNeedle.begin(), lowerNeedle.end(), lowerNeedle.begin(), ::tolower);
        match = lowerName.find(lowerNeedle) != std::string::npos;
    }
    PropVariantClear(&value);
    store->Release();
    return match;
}

IMMDevice* FindEndpoint(IMMDeviceEnumerator* enumerator, EDataFlow flow,
                        const std::string& needle) {
    IMMDeviceCollection* collection = nullptr;
    if (FAILED(enumerator->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &collection))) {
        return nullptr;
    }
    UINT count = 0;
    collection->GetCount(&count);
    IMMDevice* found = nullptr;
    for (UINT i = 0; i < count && !found; ++i) {
        IMMDevice* device = nullptr;
        if (FAILED(collection->Item(i, &device))) continue;
        if (NameMatches(device, needle)) {
            found = device;
        } else {
            device->Release();
        }
    }
    collection->Release();
    return found;
}

void ListEndpoints(IMMDeviceEnumerator* enumerator) {
    for (int pass = 0; pass < 2; ++pass) {
        EDataFlow flow = pass == 0 ? eRender : eCapture;
        wprintf(L"%s\n", flow == eRender ? L"Render endpoints:" : L"Capture endpoints:");
        IMMDeviceCollection* collection = nullptr;
        if (FAILED(enumerator->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &collection))) {
            continue;
        }
        UINT count = 0;
        collection->GetCount(&count);
        for (UINT i = 0; i < count; ++i) {
            IMMDevice* device = nullptr;
            if (FAILED(collection->Item(i, &device))) continue;
            IPropertyStore* store = nullptr;
            if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &store))) {
                PROPVARIANT value;
                PropVariantInit(&value);
                if (SUCCEEDED(store->GetValue(kFriendlyName, &value)) &&
                    value.vt == VT_LPWSTR) {
                    wprintf(L"  %s\n", value.pwszVal);
                }
                PropVariantClear(&value);
                store->Release();
            }
            device->Release();
        }
        collection->Release();
        wprintf(L"\n");
    }
}

struct AudioStream {
    IAudioClient* client = nullptr;
    WAVEFORMATEX* format = nullptr;
    double rate = 0.0;
    UINT32 bufferFrames = 0;
};

bool OpenShared(IMMDevice* device, bool render, AudioStream* stream) {
    if (FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                reinterpret_cast<void**>(&stream->client)))) {
        return false;
    }
    if (FAILED(stream->client->GetMixFormat(&stream->format))) return false;
    stream->rate = static_cast<double>(stream->format->nSamplesPerSec);

    DWORD flags = AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                  AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
    REFERENCE_TIME duration = 20 * 10000; // 20 ms
    HRESULT hr = stream->client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, duration, 0,
                                            stream->format, nullptr);
    if (FAILED(hr)) return false;
    if (FAILED(stream->client->GetBufferSize(&stream->bufferFrames))) return false;
    (void)render;
    return true;
}

// A click event recorded when it is written into the render buffer.
struct ClickEvent {
    unsigned long long frame = 0;
    double writtenQpc = 0.0;
    unsigned long long padding = 0;
};

double QpcSeconds() {
    static LARGE_INTEGER frequency = {};
    if (frequency.QuadPart == 0) QueryPerformanceFrequency(&frequency);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return static_cast<double>(now.QuadPart) / static_cast<double>(frequency.QuadPart);
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    std::string renderNeedle;
    std::string captureNeedle;
    double seconds = 10.0;
    double frequency = 1000.0;
    bool list = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = Narrow(argv[i]);
        auto next = [&](std::string* out) {
            if (i + 1 < argc) *out = Narrow(argv[++i]);
        };
        if (arg == "--list") {
            list = true;
        } else if (arg == "--render") {
            next(&renderNeedle);
        } else if (arg == "--capture") {
            next(&captureNeedle);
        } else if (arg == "--seconds") {
            std::string value;
            next(&value);
            if (!value.empty()) seconds = std::max(1.0, std::atof(value.c_str()));
        } else if (arg == "--freq") {
            std::string value;
            next(&value);
            if (!value.empty()) frequency = std::max(20.0, std::atof(value.c_str()));
        } else {
            wprintf(L"Unknown argument: %s\n", argv[i]);
            return 2;
        }
    }

    ComInit com;
    IMMDeviceEnumerator* enumerator = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator),
                                reinterpret_cast<void**>(&enumerator)))) {
        wprintf(L"Could not create the audio device enumerator.\n");
        return 1;
    }

    if (list) {
        ListEndpoints(enumerator);
        enumerator->Release();
        return 0;
    }

    if (renderNeedle.empty() || captureNeedle.empty()) {
        wprintf(L"Usage: --render <name> --capture <name> [--seconds N] [--freq HZ]\n");
        wprintf(L"       --list\n");
        enumerator->Release();
        return 2;
    }

    IMMDevice* renderDevice = FindEndpoint(enumerator, eRender, renderNeedle);
    IMMDevice* captureDevice = FindEndpoint(enumerator, eCapture, captureNeedle);
    enumerator->Release();
    if (!renderDevice || !captureDevice) {
        wprintf(L"Could not find the requested endpoints (render=%d capture=%d).\n",
                renderDevice != nullptr, captureDevice != nullptr);
        if (renderDevice) renderDevice->Release();
        if (captureDevice) captureDevice->Release();
        return 1;
    }

    AudioStream render;
    AudioStream capture;
    if (!OpenShared(renderDevice, true, &render)) {
        wprintf(L"Could not open the render endpoint.\n");
        return 1;
    }
    if (!OpenShared(captureDevice, false, &capture)) {
        wprintf(L"Could not open the capture endpoint.\n");
        return 1;
    }
    renderDevice->Release();
    captureDevice->Release();

    IAudioRenderClient* renderClient = nullptr;
    IAudioCaptureClient* captureClient = nullptr;
    if (FAILED(render.client->GetService(__uuidof(IAudioRenderClient),
                                         reinterpret_cast<void**>(&renderClient))) ||
        FAILED(capture.client->GetService(__uuidof(IAudioCaptureClient),
                                          reinterpret_cast<void**>(&captureClient)))) {
        wprintf(L"Could not get the stream services.\n");
        return 1;
    }

    const double renderRate = render.rate;
    const double captureRate = capture.rate;
    const unsigned long long periodFrames =
        static_cast<unsigned long long>(renderRate * kClickPeriodSeconds);
    const unsigned long long clickFrames =
        static_cast<unsigned long long>(renderRate * kClickLengthSeconds);
    const double sampleStep = 2.0 * 3.14159265358979323846 * frequency / renderRate;

    std::atomic<bool> stop{false};
    std::atomic<unsigned long long> framesWritten{0};
    std::atomic<int> renderUnderruns{0};

    std::vector<ClickEvent> pending;
    CRITICAL_SECTION pendingLock;
    InitializeCriticalSection(&pendingLock);

    HRESULT renderStart = render.client->Start();
    HRESULT captureStart = capture.client->Start();
    wprintf(L"start   : render hr=0x%08X, capture hr=0x%08X, bits %u/%u, channels %u/%u",
            static_cast<unsigned>(renderStart), static_cast<unsigned>(captureStart),
            render.format->wBitsPerSample, capture.format->wBitsPerSample,
            render.format->nChannels, capture.format->nChannels);
    wprintf(L"%c", 10);

    std::thread renderThread([&] {
        const UINT32 block = std::max<UINT32>(1, static_cast<UINT32>(renderRate * 0.010));
        std::vector<float> samples(block * 2);
        unsigned long long frame = 0;
        double phase = 0.0;
        while (!stop.load()) {
            UINT32 padding = 0;
            render.client->GetCurrentPadding(&padding);
            if (padding >= render.bufferFrames) {
                renderUnderruns.fetch_add(1);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            UINT32 available = render.bufferFrames - padding;
            UINT32 write = std::min(available, block);
            BYTE* buffer = nullptr;
            if (FAILED(renderClient->GetBuffer(write, &buffer))) break;
            for (UINT32 i = 0; i < write; ++i) {
                const unsigned long long position = frame + i;
                const bool inClick = (position % periodFrames) < clickFrames;
                float value = 0.0f;
                if (inClick) {
                    value = kClickAmplitude * static_cast<float>(std::sin(phase));
                    phase += sampleStep;
                } else {
                    phase = 0.0;
                }
                samples[i * 2] = value;
                samples[i * 2 + 1] = value;
                if (inClick && (position % periodFrames) == 0) {
                    ClickEvent event;
                    event.frame = position;
                    event.writtenQpc = QpcSeconds();
                    event.padding = padding;
                    EnterCriticalSection(&pendingLock);
                    if (pending.size() < 256) pending.push_back(event);
                    LeaveCriticalSection(&pendingLock);
                }
            }
            renderClient->ReleaseBuffer(write, 0);
            frame += write;
            framesWritten.store(frame);
            if (write == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } else {
                const double seconds = 0.9 * write / renderRate;
                std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
            }
        }
    });

    const double startTime = QpcSeconds();
    unsigned long long capturedSamples = 0;
    double lastOnsetQpc = 0.0;
    unsigned long long lastOnsetSample = 0;
    int clickCount = 0;
    int matchCount = 0;
    double latencySum = 0.0;
    double latencyMin = 1e9;
    double latencyMax = 0.0;
    double driftSum = 0.0;
    int driftCount = 0;
    int captureStarves = 0;
    int nonFinite = 0;
    int strayNoise = 0;
    double tonePowerSum = 0.0;
    int toneWindows = 0;
    double noisePowerSum = 0.0;
    int noiseWindows = 0;
    bool armed = true;
    int quietRuns = 0;
    double lastPacketQpc = startTime;
    bool starveOpen = false;

    while (QpcSeconds() - startTime < seconds) {
        UINT32 packet = 0;
        if (FAILED(captureClient->GetNextPacketSize(&packet))) break;
        if (packet == 0) {
            const double silentFor = QpcSeconds() - lastPacketQpc;
            if (silentFor > 0.1 && !starveOpen) {
                captureStarves++;
                starveOpen = true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }
        lastPacketQpc = QpcSeconds();
        starveOpen = false;
        BYTE* data = nullptr;
        UINT32 frames = 0;
        DWORD flags = 0;
        if (FAILED(captureClient->GetBuffer(&data, &frames, &flags, nullptr, nullptr))) break;
        const float* samples = reinterpret_cast<const float*>(data);
        const UINT32 channels = capture.format->nChannels;

        if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
            capturedSamples += frames;
        } else if (samples && capture.format->wBitsPerSample == 32) {
            for (UINT32 i = 0; i < frames; ++i) {
                const float value = samples[i * channels];
                if (!std::isfinite(value)) nonFinite++;
                const float magnitude = std::fabs(value);

                if (armed && magnitude > 0.15f) {
                    const double onsetQpc = QpcSeconds();
                    const unsigned long long onsetSample = capturedSamples + i;
                    if (clickCount > 0) {
                        const double interval = static_cast<double>(
                            onsetSample - lastOnsetSample) / captureRate;
                        const double ppm = (interval / kClickPeriodSeconds - 1.0) * 1e6;
                        if (std::fabs(ppm) < 100000.0) {
                            driftSum += ppm;
                            driftCount++;
                        }
                    }
                    lastOnsetSample = onsetSample;
                    lastOnsetQpc = onsetQpc;
                    clickCount++;

                    EnterCriticalSection(&pendingLock);
                    if (!pending.empty()) {
                        const ClickEvent event = pending.front();
                        pending.erase(pending.begin());
                        const double playQpc = event.writtenQpc +
                            static_cast<double>(event.padding) / renderRate;
                        const double latency = onsetQpc - playQpc;
                        if (latency > 0.0 && latency < 2.0) {
                            latencySum += latency;
                            latencyMin = std::min(latencyMin, latency);
                            latencyMax = std::max(latencyMax, latency);
                            matchCount++;
                        }
                    }
                    LeaveCriticalSection(&pendingLock);

                    // Tone power over the click window, noise over the gaps.
                    double power = 0.0;
                    for (UINT32 j = i; j < frames && j < i + 48; ++j) {
                        power += static_cast<double>(samples[j * channels]) *
                                 static_cast<double>(samples[j * channels]);
                    }
                    tonePowerSum += power / 48.0;
                    toneWindows++;
                    armed = false;
                    quietRuns = 0;
                } else if (!armed) {
                    if (magnitude < 0.02f) {
                        quietRuns++;
                        if (quietRuns > static_cast<int>(captureRate * 0.1)) armed = true;
                    } else {
                        quietRuns = 0;
                    }
                } else if (magnitude > 0.02f) {
                    noisePowerSum += static_cast<double>(value) * value;
                    noiseWindows++;
                } else if (magnitude > 0.05f) {
                    strayNoise++;
                }
            }
            capturedSamples += frames;
        } else {
            capturedSamples += frames;
        }
        captureClient->ReleaseBuffer(frames);
    }

    stop.store(true);
    renderThread.join();

    render.client->Stop();
    capture.client->Stop();
    renderClient->Release();
    captureClient->Release();
    render.client->Release();
    capture.client->Release();
    CoTaskMemFree(render.format);
    CoTaskMemFree(capture.format);
    DeleteCriticalSection(&pendingLock);

    const double tonePower = toneWindows > 0 ? tonePowerSum / toneWindows : 0.0;
    const double noisePower = noiseWindows > 0 ? noisePowerSum / noiseWindows : 1e-12;
    const double toneDb = 10.0 * std::log10(std::max(tonePower, 1e-12));
    const double noiseDb = 10.0 * std::log10(std::max(noisePower, 1e-12));
    const double snr = toneDb - noiseDb;
    const double latencyAvg = matchCount > 0 ? latencySum / matchCount : 0.0;
    const double driftPpm = driftCount > 0 ? driftSum / driftCount : 0.0;

    wprintf(L"render  : %.0f Hz, buffer %u frames\n", renderRate, render.bufferFrames);
    wprintf(L"capture : %.0f Hz, buffer %u frames\n", captureRate, capture.bufferFrames);
    wprintf(L"written : %llu frames\n", framesWritten.load());
    wprintf(L"clicks  : %d captured, %d matched\n", clickCount, matchCount);
    wprintf(L"level   : %.1f dBFS (tone), SNR %.1f dB\n", toneDb, snr);
    if (matchCount > 0) {
        wprintf(L"latency : avg %.2f ms, min %.2f, max %.2f (QPC approx)\n",
                latencyAvg * 1000.0, latencyMin * 1000.0, latencyMax * 1000.0);
    } else {
        wprintf(L"latency : n/a (no matched clicks)\n");
    }
    wprintf(L"drift   : %.1f ppm over %d intervals\n", driftPpm, driftCount);
    wprintf(L"glitch  : render buffer-full %d, capture starves %d, non-finite %d, stray %d\n",
            renderUnderruns.load(), captureStarves, nonFinite, strayNoise);

    const bool pass = clickCount >= 2 && matchCount >= 2 && toneDb > -60.0 &&
                      nonFinite == 0 && std::fabs(driftPpm) < 200.0;
    wprintf(L"PASS %s\n", pass ? L"true" : L"false");
    return pass ? 0 : 1;
}
