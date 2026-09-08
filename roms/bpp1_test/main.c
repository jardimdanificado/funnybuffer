// bpp1_test - 1bpp (Monochrome) simulated via RGB565 surface
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
            surface->format = WSURFACE_RGB565;
        }
        initialized = 1;
    }

    if (!surface || !surface->pixels) return WUPDATE_ERROR;

    uint16_t *fb = (uint16_t*)surface->pixels;

    for (int y = 0; y < 240; y++) {
        for (int x = 0; x < 320; x++) {
            int stripe = (x / 40) % 2;
            fb[y * 320 + x] = stripe ? 0xFFFF : 0x0000;
        }
    }

    int px = (ticks * 2) % 320;
    int py = 110;

    for (int sy = 0; sy < 20; sy++) {
        for (int sx = 0; sx < 20; sx++) {
            int cx = (px + sx) % 320;
            int cy = py + sy;
            fb[cy * 320 + cx] ^= 0xFFFF;
        }
    }

    return WUPDATE_OK;
}
