#include "audio.h"

#include <3ds.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

namespace {

const int kFramesPerBuffer = 4096;
const int kBufferCount = 4;

drmp3 g_mp3;
bool g_mp3Valid = false;
bool g_ndspReady = false;

bool g_playing = false;
bool g_paused = true;
bool g_decoderEnded = false;

int g_channels = 2;
int g_sampleRate = 44100;
u64 g_totalFrames = 0;

u64 g_positionFrames = 0;
u64 g_startPosition = 0;
u64 g_startMs = 0;

ndspWaveBuf g_waveBuf[kBufferCount];
s16* g_buffers[kBufferCount] = {nullptr, nullptr, nullptr, nullptr};

std::function<void()> g_finishedCallback;

u64 current_frames() {
    if (!g_mp3Valid) return 0;
    if (g_paused || !g_playing) return g_positionFrames;

    u64 now = osGetTime();
    u64 elapsed = now > g_startMs ? now - g_startMs : 0;
    u64 pos = g_startPosition + (elapsed * (u64)g_sampleRate) / 1000u;
    if (g_totalFrames > 0 && pos > g_totalFrames) pos = g_totalFrames;
    return pos;
}

u32 fill_buffer(int index) {
    if (!g_mp3Valid || g_decoderEnded) return 0;

    u64 read = drmp3_read_pcm_frames_s16(&g_mp3, kFramesPerBuffer, g_buffers[index]);
    if (read == 0) {
        g_decoderEnded = true;
        return 0;
    }
    if (read < (u64)kFramesPerBuffer) g_decoderEnded = true;

    size_t bytes = (size_t)read * (size_t)g_channels * sizeof(s16);
    DSP_FlushDataCache(g_buffers[index], bytes);

    g_waveBuf[index].data_pcm16 = g_buffers[index];
    g_waveBuf[index].nsamples = (u32)read;
    g_waveBuf[index].looping = false;
    ndspChnWaveBufAdd(0, &g_waveBuf[index]);
    return (u32)read;
}

void fill_all() {
    for (int i = 0; i < kBufferCount; ++i) {
        if (fill_buffer(i) == 0) break;
    }
}

void clear_buffers() {
    ndspChnWaveBufClear(0);
    for (int i = 0; i < kBufferCount; ++i) {
        g_waveBuf[i].status = NDSP_WBUF_FREE;
        g_waveBuf[i].nsamples = 0;
        g_waveBuf[i].data_pcm16 = g_buffers[i];
        g_waveBuf[i].looping = false;
    }
}

void seek_internal(u64 target, bool resume) {
    if (!g_mp3Valid) return;

    if (g_totalFrames > 0 && target >= g_totalFrames) target = g_totalFrames - 1;

    ndspChnSetPaused(0, true);
    clear_buffers();

    drmp3_seek_to_pcm_frame(&g_mp3, target);
    g_decoderEnded = false;
    g_positionFrames = target;
    g_startPosition = target;
    g_startMs = osGetTime();

    fill_all();

    if (resume) {
        g_playing = true;
        g_paused = false;
        ndspChnSetPaused(0, false);
    } else {
        g_playing = false;
        g_paused = true;
        ndspChnSetPaused(0, true);
    }
}

}  // namespace

bool audio_init() {
    if (g_ndspReady) return true;

    Result rc = ndspInit();
    if (R_FAILED(rc)) return false;

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnReset(0);

    for (int i = 0; i < kBufferCount; ++i) {
        g_buffers[i] = (s16*)linearAlloc((size_t)kFramesPerBuffer * 2 * sizeof(s16));
        if (!g_buffers[i]) {
            for (int j = 0; j < i; ++j) {
                linearFree(g_buffers[j]);
                g_buffers[j] = nullptr;
            }
            ndspExit();
            return false;
        }
        std::memset(&g_waveBuf[i], 0, sizeof(ndspWaveBuf));
        g_waveBuf[i].data_pcm16 = g_buffers[i];
        g_waveBuf[i].status = NDSP_WBUF_FREE;
    }

    g_ndspReady = true;
    return true;
}

void audio_exit() {
    if (g_mp3Valid) {
        drmp3_uninit(&g_mp3);
        g_mp3Valid = false;
    }

    if (g_ndspReady) {
        ndspChnReset(0);
        ndspExit();
        g_ndspReady = false;
    }

    for (int i = 0; i < kBufferCount; ++i) {
        if (g_buffers[i]) {
            linearFree(g_buffers[i]);
            g_buffers[i] = nullptr;
        }
    }
}

bool audio_load(const Song& song) {
    if (!g_ndspReady) return false;

    if (g_mp3Valid) {
        drmp3_uninit(&g_mp3);
        g_mp3Valid = false;
    }

    std::memset(&g_mp3, 0, sizeof(g_mp3));
    if (!drmp3_init_file(&g_mp3, song.path.c_str(), nullptr)) {
        return false;
    }
    g_mp3Valid = true;

    g_channels = (int)g_mp3.channels;
    if (g_channels < 1) g_channels = 1;
    if (g_channels > 2) g_channels = 2;
    g_sampleRate = (int)g_mp3.sampleRate;
    if (g_sampleRate <= 0) g_sampleRate = song.sampleRate > 0 ? song.sampleRate : 44100;

    u64 total = drmp3_get_pcm_frame_count(&g_mp3);
    if (total == 0 && song.duration > 0.0)
        total = (u64)(song.duration * (double)g_sampleRate + 0.5);
    g_totalFrames = total;

    g_decoderEnded = false;
    g_playing = false;
    g_paused = true;
    g_positionFrames = 0;
    g_startPosition = 0;
    g_startMs = osGetTime();

    ndspChnReset(0);
    ndspChnSetPaused(0, true);
    ndspChnSetInterp(0, NDSP_INTERP_POLYPHASE);
    ndspChnSetFormat(0, g_channels == 1 ? NDSP_FORMAT_MONO_PCM16 : NDSP_FORMAT_STEREO_PCM16);
    ndspChnSetRate(0, (float)g_sampleRate);

    float mix[12];
    std::memset(mix, 0, sizeof(mix));
    mix[0] = 1.0f;
    mix[1] = 1.0f;
    ndspChnSetMix(0, mix);

    for (int i = 0; i < kBufferCount; ++i) {
        g_waveBuf[i].data_pcm16 = g_buffers[i];
        g_waveBuf[i].nsamples = 0;
        g_waveBuf[i].status = NDSP_WBUF_FREE;
        g_waveBuf[i].looping = false;
    }

    fill_all();
    ndspChnSetPaused(0, true);
    return true;
}

void audio_play() {
    if (!g_mp3Valid) return;

    if (g_totalFrames > 0 && g_positionFrames >= g_totalFrames) {
        seek_internal(0, true);
        return;
    }

    g_startPosition = g_positionFrames;
    g_startMs = osGetTime();
    g_playing = true;
    g_paused = false;
    ndspChnSetPaused(0, false);
}

void audio_pause() {
    if (!g_mp3Valid || g_paused) return;
    g_positionFrames = current_frames();
    g_playing = false;
    g_paused = true;
    ndspChnSetPaused(0, true);
}

void audio_toggle() {
    if (!g_mp3Valid) return;
    if (g_paused)
        audio_play();
    else
        audio_pause();
}

void audio_stop() {
    if (!g_mp3Valid) return;
    seek_internal(0, false);
}

void audio_close() {
    if (!g_mp3Valid) return;

    ndspChnSetPaused(0, true);
    clear_buffers();
    drmp3_uninit(&g_mp3);

    g_mp3Valid = false;
    g_playing = false;
    g_paused = true;
    g_decoderEnded = false;
    g_totalFrames = 0;
    g_positionFrames = 0;
    g_startPosition = 0;
}

void audio_seek(double seconds) {
    if (!g_mp3Valid) return;
    if (seconds < 0.0) seconds = 0.0;

    bool resume = g_playing && !g_paused;
    u64 target = (u64)(seconds * (double)g_sampleRate + 0.5);
    seek_internal(target, resume);
}

void audio_update() {
    if (!g_mp3Valid || !g_playing || g_paused) return;

    for (int i = 0; i < kBufferCount; ++i) {
        u8 status = g_waveBuf[i].status;
        if (status == NDSP_WBUF_DONE || status == NDSP_WBUF_FREE) {
            if (fill_buffer(i) == 0) break;
        }
    }

    if (!g_decoderEnded) return;

    bool allDone = true;
    for (int i = 0; i < kBufferCount; ++i) {
        if (g_waveBuf[i].status == NDSP_WBUF_QUEUED ||
            g_waveBuf[i].status == NDSP_WBUF_PLAYING) {
            allDone = false;
            break;
        }
    }

    if (allDone) {
        g_playing = false;
        g_paused = true;
        g_positionFrames = g_totalFrames;
        g_startPosition = g_totalFrames;
        ndspChnSetPaused(0, true);
        if (g_finishedCallback) g_finishedCallback();
    }
}

bool audio_loaded() { return g_mp3Valid; }
bool audio_is_playing() { return g_mp3Valid && g_playing && !g_paused; }
bool audio_is_paused() { return !g_playing || g_paused; }

double audio_position() {
    if (!g_mp3Valid || g_sampleRate <= 0) return 0.0;
    return (double)current_frames() / (double)g_sampleRate;
}

double audio_duration() {
    if (!g_mp3Valid || g_sampleRate <= 0) return 0.0;
    return (double)g_totalFrames / (double)g_sampleRate;
}

void audio_set_finished_callback(std::function<void()> callback) {
    g_finishedCallback = std::move(callback);
}
