// wash_shader — Demonstration of a Wash compute shader running on the Wagnostic 2.0 ABI
// Demonstrates: std:surface + std:clock + std:mouse + wash:dispatch (wdispatch_t)

#include "wagnostic.h"
#include "surface.h"
#include "clock.h"
#include "mouse.h"
#include "dispatch.h"

static wsurface_t  *surface;
static wclock_t    *clock_ext;
static wmouse_t    *mouse;
static wdispatch_t *dispatch;

static int initialized = 0;
static uint32_t ticks = 0;

static inline float sin_approx(float x) {
    while (x > 3.14159265f) x -= 6.28318530f;
    while (x < -3.14159265f) x += 6.28318530f;
    float x2 = x * x;
    return (16.0f * x * (3.14159265f - ((x < 0) ? -x : x))) / (5.0f * 3.14159265f * 3.14159265f - 4.0f * x2);
}

static inline float cos_approx(float x) {
    return sin_approx(x + 1.57079632f);
}

int32_t wupdate(void) {
    if (!initialized) {
        surface   = (wsurface_t*) wextension("std:surface", 1);
        clock_ext = (wclock_t*)   wextension("std:clock", 1);
        mouse     = (wmouse_t*)   wextension("std:mouse", 1);
        dispatch  = (wdispatch_t*)wextension("wash:dispatch", 1);

        if (surface) {
            surface->width  = 320;
            surface->height = 240;
            surface->stride = 320;
            surface->format = WSURFACE_RGBA8888;
        }

        initialized = 1;
    }

    ticks++;

    if (!surface || !surface->pixels) return WUPDATE_ERROR;

    uint32_t *pixels = (uint32_t*)surface->pixels;
    uint32_t full_w = surface->width;
    uint32_t full_h = surface->height;
    uint32_t stride = surface->stride ? surface->stride : full_w;

    // Use dispatch tile bounds if multi-threaded, or full surface if single-threaded
    uint32_t x0 = (dispatch && dispatch->tile_w > 0) ? dispatch->tile_x : 0;
    uint32_t y0 = (dispatch && dispatch->tile_h > 0) ? dispatch->tile_y : 0;
    uint32_t x1 = x0 + ((dispatch && dispatch->tile_w > 0) ? dispatch->tile_w : full_w);
    uint32_t y1 = y0 + ((dispatch && dispatch->tile_h > 0) ? dispatch->tile_h : full_h);

    if (x1 > full_w) x1 = full_w;
    if (y1 > full_h) y1 = full_h;

    float t = (float)ticks * 0.03f;
    float aspect = (float)full_w / (float)full_h;

    // Interactive Julia constant modulated by mouse or time
    float cx = (mouse && mouse->buttons) ? ((float)mouse->x / (float)full_w - 0.5f) * 1.5f : sin_approx(t * 0.4f) * 0.7885f;
    float cy = (mouse && mouse->buttons) ? ((float)mouse->y / (float)full_h - 0.5f) * 1.5f : cos_approx(t * 0.4f) * 0.7885f;

    for (uint32_t y = y0; y < y1; ++y) {
        for (uint32_t x = x0; x < x1; ++x) {
            float zx = (((float)x / (float)full_w) - 0.5f) * 2.8f * aspect;
            float zy = (((float)y / (float)full_h) - 0.5f) * 2.8f;

            int iter = 0;
            const int max_iter = 40;

            while (zx * zx + zy * zy < 4.0f && iter < max_iter) {
                float tmp = zx * zx - zy * zy + cx;
                zy = 2.0f * zx * zy + cy;
                zx = tmp;
                iter++;
            }

            uint32_t pixel;
            if (iter >= max_iter) {
                pixel = 0xFF050510; // Dark background
            } else {
                float norm = (float)iter / (float)max_iter;
                uint8_t r = (uint8_t)(sin_approx(norm * 6.28f + 0.0f) * 127.0f + 128.0f);
                uint8_t g = (uint8_t)(sin_approx(norm * 6.28f + 2.0f) * 127.0f + 128.0f);
                uint8_t b = (uint8_t)(sin_approx(norm * 6.28f + 4.0f) * 127.0f + 128.0f);
                pixel = (0xFF << 24) | (b << 16) | (g << 8) | r;
            }

            pixels[y * stride + x] = pixel;
        }
    }

    return WUPDATE_OK;
}
