#include <algorithm>
#include <iostream>
#include <coreinit/ipcbufpool.h>
#include <experimental/scope>
#include <whb/proc.h>
#include <ranges>
#include "uac.hpp"
#include "logging.hpp"
#include <simple_wav.h>

static constinit std::array<uint8_t, 0x65fe> backingBuffer = {};
#define DEFER(expr) auto _ = std::experimental::scope_exit([&]{ expr; });


static bool run(std::vector<int16_t>& samples) {
    auto result = UACInit();
    cz::os_reportln("UACInit -> {}", cz::enum_identifier(result));
    if (result != UAC_SUCCESS) {
        return false;
    }
    uint32_t msgCount = 0;
    auto poolHandle = IPCBufPoolCreate(backingBuffer.data(), backingBuffer.size(), 0x2180, &msgCount, 0);
    cz::os_reportln("IPCBufPoolCreate -> {:p}", static_cast<void *>(poolHandle));
    if (!poolHandle) {
        return false;
    }
    const auto ipcMem = static_cast<uint8_t *>(IPCBufPoolAllocate(poolHandle, 0x2180));
    cz::os_reportln("IPCBufPoolAllocate -> {:p}", static_cast<void *>(ipcMem));
    UACIpcWorkMemory workMem{
        .buffer = ipcMem,
        .bufferSizeBytes = 0x2000,
        .isoDescBuffer = reinterpret_cast<UACISODesc *>(ipcMem + 0x2000),
        .isoDescBufferSizeBytes = 0x180
    };

    result = UACOpen(UAC_CHANNEL_0, &workMem);
    DEFER(
    result = UACClose(UAC_CHANNEL_0);
        cz::os_reportln("UACClose -> {}", cz::enum_identifier(result));
    );

    UACISODesc *desc;
    OSEvent gotAudioEvent;
    char eventName[] = "UACAudioEvt";
    OSInitEventEx(&gotAudioEvent, 0, OS_EVENT_MODE_MANUAL, eventName);

    const auto endTime = OSGetSystemTime() + OSSecondsToTicks(10);
    while (OSGetSystemTime() < endTime) {
        result = UACGetAudio(UAC_CHANNEL_0, &gotAudioEvent, &desc);
        if (result != UAC_SUCCESS) {
            cz::os_reportln("UACGetAudio -> {}", cz::enum_identifier(result));
            continue;
        }
        OSWaitEvent(&gotAudioEvent);
        for (auto [buf, bufInfo] : std::views::zip(desc->sampleBufs, desc->bufInfo)) {
            samples.append_range(std::span(static_cast<int16_t*>(buf), bufInfo.sizeBytes >> 1));
        }
        OSResetEvent(&gotAudioEvent);
        UACFreeISODesc(UAC_CHANNEL_0, desc);
        OSSleepTicks(OSMillisecondsToTicks(1));
    }

    return true;
}

int main() {
    WHBProcInit();
    std::vector<int16_t> sampleBuffer;
    if (run(sampleBuffer)) {
        cz::os_reportln("Success! Got {} samples", sampleBuffer.size());
        simple_wav_format_info info = {
            .channel_count = 1,
            .sample_rate = 16000,
            .bits_per_sample = 16,
            .audio_format = SIMPLE_WAV_AUDIO_FMT_PCM
        };
        simple_wav_error error{};
        auto wav = simple_wav_wr_open(&info, "output.wav", &error);
        if (!wav) {
            cz::os_reportln("Failed to open wav: {}", simple_wav_strerrorname(error));
        }
        else {
            const auto bytesWritten = simple_wav_wr_write_sample_bytes(wav, sampleBuffer.data(), sampleBuffer.size(), &error);

            if (error)
                cz::os_reportln("Error writing WAV file: {}", simple_wav_strerrorname(error));
            else
                cz::os_reportln("Wrote {} bytes", bytesWritten);
            simple_wav_wr_close(wav);
        }
    } else {
        cz::os_reportln("Failure!");
    }
    while (WHBProcIsRunning()) {
    }

    WHBProcShutdown();


    return 0;
}
