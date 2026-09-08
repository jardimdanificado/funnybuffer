// exotic2_test - Tests WSURFACE_BGRA8888 32-bit surface
#include "wagnostic.h"
#include "surface.h"

static wsurface_t *surface;
static int initialized = 0;
static uint32_t ticks = 0;

int32_t wupdate(void) {
    ticks++;
    if (!initialized) {
        surface = (wsurface_t*)wextension("std:surface", 1);
        if (surface) {
            surface->width = 320;
            surface->height = 240;
            surface->stride = 320;
            surface->format = WSURFACE_BGRA8888;
        }
        initialized = 1;
    }

    if (!surface || !surface->pixels) return WUPDATE_ERROR;

    uint32_t *fb = (uint32_t*)surface->pixels;

    for (int y = 0; y < 240; y++) {
        for (int x = 0; x < 320; x++) {
            uint8_t r = (uint8_t)(x * 255 / 320);
            uint8_t g = (uint8_t)(y * 255 / 240);
            uint8_t b = (uint8_t)((ticks * 3) & 0xFF);
            // BGRA: B in low byte, R in 3rd byte
            fb[y * 320 + x] = 0xFF000000 | (r << 16) | (g << 8) | b;
        }
    }

    return WUPDATE_OK;
}
