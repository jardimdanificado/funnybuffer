// bpp2_test - 2bpp 4-shade grayscale simulation
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
    static const uint16_t palette[4] = { 0x0000, 0x52AA, 0xAD55, 0xFFFF };

    for (int y = 0; y < 240; y++) {
        for (int x = 0; x < 320; x++) {
            int shade = ((x / 80) + (ticks / 30)) % 4;
            fb[y * 320 + x] = palette[shade];
        }
    }

    return WUPDATE_OK;
}
