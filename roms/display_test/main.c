// display_test — Tests surface modes and formats

#include "wagnostic.h"
#include "surface.h"
#include "keyboard.h"

static wsurface_t  *surface;
static wkeyboard_t *keyboard;

static int current_fmt = WSURFACE_RGB565;
static int frame_phase = 0;
static int resize_state = 0;
static int initialized = 0;

static void set_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (!surface || !surface->pixels) return;
    int w = (int)surface->width;
    int h = (int)surface->height;
    int stride = (int)(surface->stride ? surface->stride : surface->width);
    if (x < 0 || x >= w || y < 0 || y >= h) return;

    int idx = y * stride + x;
    if (surface->format == WSURFACE_RGB565) {
        uint16_t* fb = (uint16_t*)surface->pixels;
        fb[idx] = (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    } else if (surface->format == WSURFACE_RGB888) {
        uint8_t* fb = (uint8_t*)surface->pixels;
        fb[idx * 3 + 0] = r;
        fb[idx * 3 + 1] = g;
        fb[idx * 3 + 2] = b;
    } else if (surface->format == WSURFACE_BGRA8888) {
        uint32_t* fb = (uint32_t*)surface->pixels;
        fb[idx] = (0xFF000000) | (r << 16) | (g << 8) | b;
    } else { // WSURFACE_RGBA8888
        uint32_t* fb = (uint32_t*)surface->pixels;
        fb[idx] = (0xFF000000) | (b << 16) | (g << 8) | r;
    }
}

static void fill_rect(int rx, int ry, int rw, int rh, uint8_t r, uint8_t g, uint8_t b) {
    for (int y = ry; y < ry + rh; y++)
        for (int x = rx; x < rx + rw; x++)
            set_pixel(x, y, r, g, b);
}

static void clear_screen(uint8_t r, uint8_t g, uint8_t b) {
    if (!surface) return;
    fill_rect(0, 0, (int)surface->width, (int)surface->height, r, g, b);
}

static void draw_color_bars(void) {
    if (!surface) return;
    int w = (int)surface->width, h = (int)surface->height;
    int bar_w = w / 8;
    uint8_t colors[8][3] = {
        {255,255,255}, {255,255,0}, {0,255,255}, {0,255,0},
        {255,0,255}, {255,0,0}, {0,0,255}, {0,0,0}
    };
    for (int i = 0; i < 8; i++) {
        fill_rect(i * bar_w, 0, bar_w, h - 30,
                  colors[i][0], colors[i][1], colors[i][2]);
    }
}

static void draw_grid(void) {
    if (!surface) return;
    int w = (int)surface->width, h = (int)surface->height;
    for (int x = 0; x < w; x += 32) {
        for (int y = 0; y < h; y++)
            set_pixel(x, y, 128, 128, 128);
    }
    for (int y = 0; y < h; y += 32) {
        for (int x = 0; x < w; x++)
            set_pixel(x, y, 128, 128, 128);
    }
}

static void draw_status(void) {
    if (!surface) return;
    int w = (int)surface->width, h = (int)surface->height;
    fill_rect(0, h - 30, w, 30, 0, 0, 0);
    fill_rect(5, h - 25, 20, 20, 0, 200, 255);
    fill_rect(35, h - 25, 10, 10, 255, 255, 255);
    fill_rect(50, h - 25, 10, 10, 200, 200, 200);
}

int32_t wupdate(void) {
    if (!initialized) {
        surface  = (wsurface_t*)wextension("std:surface", 1);
        keyboard = (wkeyboard_t*)wextension("std:keyboard", 1);

        if (surface) {
            surface->width = 320;
            surface->height = 240;
            surface->stride = 320;
            surface->format = WSURFACE_RGB565;
        }

        initialized = 1;
    }

    if (!surface) return WUPDATE_ERROR;

    frame_phase++;

    static int r_was_down = 0;
    static int key1_was_down = 0, key2_was_down = 0, key3_was_down = 0;

    int r_down = keyboard ? keyboard->keys[21] : 0;
    if (r_down && !r_was_down) {
        resize_state = (resize_state + 1) % 3;
        if (resize_state == 0) {
            surface->width = 320; surface->height = 240; surface->stride = 320;
        } else if (resize_state == 1) {
            surface->width = 640; surface->height = 480; surface->stride = 640;
        } else {
            surface->width = 160; surface->height = 120; surface->stride = 160;
        }
    }
    r_was_down = r_down;

    int k1 = keyboard ? keyboard->keys[30] : 0;
    if (k1 && !key1_was_down) surface->format = WSURFACE_RGB565;
    key1_was_down = k1;

    int k2 = keyboard ? keyboard->keys[31] : 0;
    if (k2 && !key2_was_down) surface->format = WSURFACE_RGB888;
    key2_was_down = k2;

    int k3 = keyboard ? keyboard->keys[32] : 0;
    if (k3 && !key3_was_down) surface->format = WSURFACE_RGBA8888;
    key3_was_down = k3;

    clear_screen(32, 32, 32);
    draw_color_bars();
    draw_grid();
    draw_status();

    int ax = (frame_phase * 3) % (int)surface->width;
    fill_rect(ax, 10, 20, 20, 255, 200, 0);

    return WUPDATE_OK;
}
