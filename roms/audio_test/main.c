// audio_test — Tests std:audio real-time tone generation and std:surface + std:clock

#include "wagnostic.h"
#include "surface.h"
#include "clock.h"
#include "audio.h"

static wsurface_t *surface;
static wclock_t   *clock_ext;
static waudio_t   *audio;

static int initialized = 0;
static float phase = 0.0f;
static uint32_t frame_count = 0;

static inline float sin_approx(float x) {
    // Wrap to [-PI, PI]
    while (x > 3.14159265f) x -= 6.28318530f;
    while (x < -3.14159265f) x += 6.28318530f;
    // Bhaskara I approximation
    float x2 = x * x;
    return (16.0f * x * (3.14159265f - ((x < 0) ? -x : x))) / (5.0f * 3.14159265f * 3.14159265f - 4.0f * x2);
}

int32_t wupdate(void) {
    if (!initialized) {
        surface   = (wsurface_t*)wextension("std:surface", 1);
        clock_ext = (wclock_t*)wextension("std:clock", 1);
        audio     = (waudio_t*)wextension("std:audio", 1);

        if (surface) {
            surface->width = 320;
            surface->height = 240;
            surface->stride = 320;
            surface->format = WSURFACE_RGB565;
        }

        initialized = 1;
    }

    frame_count++;

    // Generate Audio Samples
    if (audio && audio->buffer && audio->capacity > 0) {
        float *ring_buf = (float*)audio->buffer;
        uint32_t channels = audio->channels ? audio->channels : 2;
        uint32_t sample_rate = audio->sample_rate ? audio->sample_rate : 44100;
        float freq = 440.0f; // 440 Hz A4 tone

        uint32_t write_idx = audio->write;
        uint32_t read_idx = audio->read;
        uint32_t used = (write_idx >= read_idx) ? (write_idx - read_idx) : 0;
        uint32_t free_space = (audio->capacity > used) ? (audio->capacity - used - 1) : 0;

        uint32_t samples_to_write = (free_space > 1024) ? 1024 : free_space;
        float phase_inc = 6.28318530f * freq / (float)sample_rate;

        for (uint32_t i = 0; i < samples_to_write; i++) {
            float sample = sin_approx(phase) * 0.25f;
            phase += phase_inc;
            if (phase > 6.28318530f) phase -= 6.28318530f;

            uint32_t dst_frame = (write_idx + i) % audio->capacity;
            ring_buf[dst_frame * channels + 0] = sample;
            if (channels > 1) ring_buf[dst_frame * channels + 1] = sample;
        }
        audio->write = write_idx + samples_to_write;
    }

    // Render Visual Oscilloscope
    if (surface && surface->pixels) {
        uint16_t *fb = (uint16_t*)surface->pixels;
        uint32_t w = surface->width ? surface->width : 320;
        uint32_t h = surface->height ? surface->height : 240;
        uint32_t stride = surface->stride ? surface->stride : w;

        for (uint32_t i = 0; i < w * h; i++) fb[i] = 0x1082; // dark gray

        // Draw sine wave
        for (uint32_t x = 0; x < w; x++) {
            float wave = sin_approx((float)x * 0.1f + (float)frame_count * 0.2f);
            int y = (int)(120 + wave * 40.0f);
            if (y >= 0 && y < (int)h) {
                fb[y * stride + x] = 0x07E0; // green
                if (y + 1 < (int)h) fb[(y + 1) * stride + x] = 0x07E0;
            }
        }
    }

    return WUPDATE_OK;
}
