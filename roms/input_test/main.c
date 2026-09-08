// input_test — Tests all input methods

#include "wagnostic.h"
#include "surface.h"
#include "clock.h"
#include "keyboard.h"
#include "mouse.h"
#include "gamepad.h"

static wsurface_t  *surface;
static wclock_t    *clock_ext;
static wkeyboard_t *keyboard;
static wmouse_t    *mouse;
static wgamepad_t  *gamepad;

static uint32_t ticks = 0;
static int initialized = 0;

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static void set_pixel(int x, int y, uint16_t c) {
    if (!surface || !surface->pixels) return;
    if (x >= 0 && x < (int)surface->width && y >= 0 && y < (int)surface->height) {
        uint16_t *fb = (uint16_t*)surface->pixels;
        uint32_t stride = surface->stride ? surface->stride : surface->width;
        fb[y * stride + x] = c;
    }
}

static void fill_rect(int rx, int ry, int rw, int rh, uint16_t c) {
    for (int y = ry; y < ry + rh; y++)
        for (int x = rx; x < rx + rw; x++)
            set_pixel(x, y, c);
}

static void draw_hline(int x1, int x2, int y, uint16_t c) {
    for (int x = x1; x < x2; x++) set_pixel(x, y, c);
}

static void draw_vline(int x, int y1, int y2, uint16_t c) {
    for (int y = y1; y < y2; y++) set_pixel(x, y, c);
}

static const uint8_t font5x7[10][7] = {
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
    {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F},
    {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
};

static void draw_digit(int x, int y, int d, uint16_t c) {
    if (d < 0 || d > 9) return;
    for (int row = 0; row < 7; row++)
        for (int col = 0; col < 5; col++)
            if (font5x7[d][row] & (0x10 >> col))
                set_pixel(x + col, y + row, c);
}

static void draw_number(int x, int y, int n, uint16_t c) {
    if (n == 0) { draw_digit(x, y, 0, c); return; }
    char buf[12]; int len = 0;
    int tmp = n;
    if (tmp < 0) { set_pixel(x, y, c); tmp = -tmp; x += 7; }
    while (tmp > 0 && len < 12) { buf[len++] = tmp % 10; tmp /= 10; }
    for (int i = len - 1; i >= 0; i--) {
        draw_digit(x, y, buf[i], c);
        x += 6;
    }
}

static void draw_keyboard_section(void) {
    int cols = 16, rows = 16;
    int cell_w = 12, cell_h = 7;
    int ox = 8, oy = 5;

    fill_rect(ox - 2, oy - 2, cols * cell_w + 4, rows * cell_h + 4, rgb565(20, 20, 30));

    for (int i = 0; i < 256; i++) {
        int cx = i % cols, cy = i / cols;
        int px = ox + cx * cell_w, py = oy + cy * cell_h;
        int is_pressed = keyboard && keyboard->keys[i];
        uint16_t col = is_pressed ? rgb565(0, 220, 80) : rgb565(60, 60, 70);
        fill_rect(px, py, cell_w - 1, cell_h - 1, col);
    }

    draw_number(ox, oy + rows * cell_h + 4, (int)ticks / 60, rgb565(200, 200, 200));
}

static void draw_mouse_section(void) {
    int ox = 10, oy = 140;
    int w = 140, h = 95;

    fill_rect(ox, oy, w, h, rgb565(20, 20, 30));
    draw_hline(ox, ox + w, oy, rgb565(80, 80, 80));
    draw_vline(ox, oy, oy + h, rgb565(80, 80, 80));

    int mx = mouse ? mouse->x : 0;
    int my = mouse ? mouse->y : 0;
    uint32_t mbtns = mouse ? mouse->buttons : 0;
    int mwheel = mouse ? mouse->wheel_y : 0;

    int cx = ox + 5 + (mx * (w - 10)) / 320;
    int cy = oy + 5 + (my * (h - 20)) / 240;
    draw_hline(cx - 8, cx + 8, cy, rgb565(255, 255, 255));
    draw_vline(cx, cy - 8, cy + 8, rgb565(255, 255, 255));
    fill_rect(cx - 1, cy - 1, 3, 3, rgb565(255, 0, 0));

    uint16_t lc = (mbtns & WMOUSE_BTN_LEFT) ? rgb565(255, 50, 50) : rgb565(80, 80, 80);
    uint16_t rc = (mbtns & WMOUSE_BTN_RIGHT) ? rgb565(50, 50, 255) : rgb565(80, 80, 80);
    fill_rect(ox + 10, oy + h - 18, 25, 12, lc);
    fill_rect(ox + 40, oy + h - 18, 25, 12, rc);

    draw_number(ox + 80, oy + h - 18, mwheel, rgb565(255, 255, 0));
}

static void draw_gamepad_section(void) {
    int ox = 170, oy = 140;
    int w = 145, h = 95;

    fill_rect(ox, oy, w, h, rgb565(20, 20, 30));
    draw_hline(ox, ox + w, oy, rgb565(80, 80, 80));

    uint32_t gp = gamepad ? gamepad->buttons : 0;

    int bx = ox + 10, by = oy + 10;
    uint16_t dc = rgb565(100, 100, 100);
    fill_rect(bx + 10, by, 10, 10, (gp & WGAMEPAD_BTN_DPAD_UP) ? rgb565(0,255,0) : dc);
    fill_rect(bx + 10, by + 22, 10, 10, (gp & WGAMEPAD_BTN_DPAD_DOWN) ? rgb565(0,255,0) : dc);
    fill_rect(bx, by + 11, 10, 10, (gp & WGAMEPAD_BTN_DPAD_LEFT) ? rgb565(0,255,0) : dc);
    fill_rect(bx + 20, by + 11, 10, 10, (gp & WGAMEPAD_BTN_DPAD_RIGHT) ? rgb565(0,255,0) : dc);
    fill_rect(bx + 10, by + 11, 10, 10, rgb565(50,50,50));

    fill_rect(bx + 45, by + 5, 15, 15, (gp & WGAMEPAD_BTN_A) ? rgb565(255,50,50) : dc);
    fill_rect(bx + 65, by + 5, 15, 15, (gp & WGAMEPAD_BTN_B) ? rgb565(50,50,255) : dc);
    fill_rect(bx + 45, by + 25, 15, 10, (gp & WGAMEPAD_BTN_SELECT) ? rgb565(200,200,0) : dc);
    fill_rect(bx + 65, by + 25, 15, 10, (gp & WGAMEPAD_BTN_START) ? rgb565(200,200,0) : dc);

    draw_number(bx, by + 50, (int)gp, rgb565(180, 180, 180));
}

int32_t wupdate(void) {
    ticks++;
    if (!initialized) {
        surface   = (wsurface_t*)wextension("std:surface", 1);
        clock_ext = (wclock_t*)wextension("std:clock", 1);
        keyboard  = (wkeyboard_t*)wextension("std:keyboard", 1);
        mouse     = (wmouse_t*)wextension("std:mouse", 1);
        gamepad   = (wgamepad_t*)wextension("std:gamepad", 1);

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
    for (int i = 0; i < 320 * 240; i++) fb[i] = rgb565(15, 15, 20);

    draw_keyboard_section();
    draw_mouse_section();
    draw_gamepad_section();

    if (keyboard && keyboard->keys[41]) return WUPDATE_EXIT; // Escape

    return WUPDATE_OK;
}
