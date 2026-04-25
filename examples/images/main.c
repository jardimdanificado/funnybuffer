
#include "image_data.h"

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef          int   int32_t;

extern void init(int w, int h, int vram, int ram);
extern void draw();
extern uint32_t get_ticks();

typedef struct {
    uint32_t width, height, ram, vram, redraw;
    uint32_t gamepad_buttons;
    int32_t  joystick_lx, joystick_ly, joystick_rx, joystick_ry;
    uint8_t  keys[256];
} SystemConfig;

#define BTN_UP     (1 << 0)
#define BTN_DOWN   (1 << 1)
#define BTN_LEFT   (1 << 2)
#define BTN_RIGHT  (1 << 3)
#define BTN_A      (1 << 4)
#define BTN_B      (1 << 5)
#define BTN_L1     (1 << 8)
#define BTN_R1     (1 << 9)
#define BTN_L2     (1 << 12)
#define BTN_R2     (1 << 13)

static SystemConfig* _sys = (SystemConfig*)0;
static uint16_t* _fb = (uint16_t*)296;

static const uint16_t* pixels_ptr = (const uint16_t*)image_raw;

void draw_image_scaled(int dest_x, int dest_y, int dest_w, int dest_h) {
    if (dest_w <= 0 || dest_h <= 0) return;
    for (int j = 0; j < dest_h; j++) {
        int screen_y = dest_y + j;
        if (screen_y < 0 || screen_y >= (int)_sys->height) continue;
        int v = (j * image_height) / dest_h;
        const uint16_t* src_row = &pixels_ptr[v * image_width];
        uint16_t* dst_row = &_fb[screen_y * _sys->width];
        for (int i = 0; i < dest_w; i++) {
            int screen_x = dest_x + i;
            if (screen_x < 0 || screen_x >= (int)_sys->width) continue;
            int u = (i * image_width) / dest_w;
            dst_row[screen_x] = src_row[u];
        }
    }
}

int main() {
    init(320, 240, 320 * 240 * 2, 65536);

    int pos_x = 100, pos_y = 70;
    int scale_w = 120, scale_h = 100;

    while (1) {
        // Limpa tela
        for (int i = 0; i < 320 * 240; i++) _fb[i] = 0x3186; // Fundo azulado

        // Movimentação via setas ou gamepad
        if (_sys->keys[79] || (_sys->gamepad_buttons & BTN_RIGHT)) pos_x += 2;
        if (_sys->keys[80] || (_sys->gamepad_buttons & BTN_LEFT))  pos_x -= 2;
        if (_sys->keys[81] || (_sys->gamepad_buttons & BTN_DOWN))  pos_y += 2;
        if (_sys->keys[82] || (_sys->gamepad_buttons & BTN_UP))    pos_y -= 2;

        // Escala via L1/R1
        if (_sys->gamepad_buttons & BTN_L1) { scale_w -= 2; scale_h -= 2; }
        if (_sys->gamepad_buttons & BTN_R1) { scale_w += 2; scale_h += 2; }

        draw_image_scaled(pos_x, pos_y, scale_w, scale_h);
        
        _sys->redraw = 1;
        draw();
    }
    return 0;
}
