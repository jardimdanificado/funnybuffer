// exotic1_test - Tests WSURFACE_RGB888 24-bit surface
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
            surface->format = WSURFACE_RGB888;
        }
        initialized = 1;
    }

    if (!surface || !surface->pixels) return WUPDATE_ERROR;

    uint8_t *fb = (uint8_t*)surface->pixels;

    for (int y = 0; y < 240; y++) {
        for (int x = 0; x < 320; x++) {
            size_t idx = (y * 320 + x) * 3;
            fb[idx + 0] = (uint8_t)(x * 255 / 320);
            fb[idx + 1] = (uint8_t)(y * 255 / 240);
            fb[idx + 2] = (uint8_t)((ticks * 4) & 0xFF);
        }
    }

    return WUPDATE_OK;
}
