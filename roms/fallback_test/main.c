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

    uint16_t* fb = (uint16_t*)surface->pixels;
    static uint32_t last_tick = 0;
    static uint16_t color = 0x1F;

    if (ticks - last_tick > 60) {
        color = (color == 0x1F) ? 0xF800 : (color == 0xF800 ? 0x07E0 : 0x1F);
        last_tick = ticks;
    }

    for (int i = 0; i < 320 * 240; i++)
        fb[i] = color;

    return WUPDATE_OK;
}
