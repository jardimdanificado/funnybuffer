#include "wagnostic.h"
#include "surface.h"
#include "keyboard.h"
#include "mouse.h"

static wsurface_t  *surface;
static wkeyboard_t *keyboard;
static wmouse_t    *mouse;

#define W_RGB565(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

static void draw_rect(int x, int y, int w, int h, uint16_t color) {
    if (!surface || !surface->pixels) return;
    uint16_t* _fb = (uint16_t*)surface->pixels;
    uint32_t stride = surface->stride ? surface->stride : surface->width;
    for (int iy = y; iy < y + h; iy++) {
        if (iy < 0 || iy >= (int)surface->height) continue;
        for (int ix = x; ix < x + w; ix++) {
            if (ix >= 0 && ix < (int)surface->width)
                _fb[iy * stride + ix] = color;
        }
    }
}

static int initialized = 0;

int32_t wupdate(void) {
    if (!initialized) {
        surface  = (wsurface_t*)wextension("std:surface", 1);
        keyboard = (wkeyboard_t*)wextension("std:keyboard", 1);
        mouse    = (wmouse_t*)wextension("std:mouse", 1);

        if (surface) {
            surface->width = 320;
            surface->height = 240;
            surface->stride = 320;
            surface->format = WSURFACE_RGB565;
        }

        initialized = 1;
    }

    if (!surface || !surface->pixels) return WUPDATE_ERROR;

    uint16_t* _fb = (uint16_t*)surface->pixels;
    for (int i = 0; i < 320 * 240; i++) _fb[i] = W_RGB565(51, 51, 51);

    int cols = 16, rows = 16, cell_w = 16, cell_h = 10;
    int margin_x = (320 - (cols * cell_w)) / 2;
    int margin_y = (240 - (rows * cell_h)) / 2;

    for (int i = 0; i < 256; i++) {
        int cx = i % cols, cy = i / cols;
        int px = margin_x + cx * cell_w, py = margin_y + cy * cell_h;
        uint16_t col = W_RGB565(119, 119, 119);
        if (keyboard && keyboard->keys[i]) col = W_RGB565(0, 204, 85);
        draw_rect(px, py, cell_w - 1, cell_h - 1, col);
    }

    int mx = mouse ? mouse->x : 0;
    int my = mouse ? mouse->y : 0;
    uint32_t mbtns = mouse ? mouse->buttons : 0;

    draw_rect(mx - 2, my - 2, 5, 5, W_RGB565(255, 255, 255));
    if (mbtns & WMOUSE_BTN_LEFT) draw_rect(mx - 4, my - 4, 9, 9, W_RGB565(255, 0, 0));

    return WUPDATE_OK;
}
