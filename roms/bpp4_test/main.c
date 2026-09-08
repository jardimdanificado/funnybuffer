// bpp4_test - 4bpp 16-color simulation
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
            int c = (x / 20) % 16;
            uint8_t r = (c & 1) ? 255 : ((c & 8) ? 128 : 0);
            uint8_t g = (c & 2) ? 255 : ((c & 8) ? 128 : 0);
            uint8_t b = (c & 4) ? 255 : ((c & 8) ? 128 : 0);
            fb[y * 320 + x] = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
        }
    }

    return WUPDATE_OK;
}
