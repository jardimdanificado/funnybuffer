
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef          int   int32_t;

#include "image_data.h"

#pragma pack(push, 1)
typedef struct {
    char     message[128];      // 0
    uint32_t width, height, bpp, scale;
    uint32_t audio_size, audio_write_ptr, audio_read_ptr;
    uint32_t audio_sample_rate, audio_bpp, audio_channels;
    uint32_t signal_count;      // 168
    uint32_t gamepad_buttons;   // 172
    int32_t  joystick_lx, joystick_ly, joystick_rx, joystick_ry; // 176
    uint8_t  keys[256];         // 192
    int32_t  mouse_x, mouse_y;  // 448
    uint32_t mouse_buttons;     // 456
    int32_t  mouse_wheel;       // 460
    uint8_t  reserved[48];      // 464
} SystemConfig;
#pragma pack(pop)

#define _sys ((volatile SystemConfig*)0)
#define _sig ((volatile uint8_t*)512)
static uint16_t* _fb;

#define BTN_UP     (1 << 0)
#define BTN_DOWN   (1 << 1)
#define BTN_LEFT   (1 << 2)
#define BTN_RIGHT  (1 << 3)
#define BTN_A      (1 << 4)
#define BTN_L1     (1 << 8)
#define BTN_R1     (1 << 9)
#define BTN_L2     (1 << 12)
#define BTN_R2     (1 << 13)

#define pixels_ptr ((const uint16_t*)image_raw)

void draw_image_scaled(int dest_x, int dest_y, int dest_w, int dest_h) {
    if (dest_w <= 0 || dest_h <= 0) return;
    for (int j = 0; j < dest_h; j++) {
        int screen_y = dest_y + j;
        if (screen_y < 0 || screen_y >= 240) continue;
        int v = (j * image_height) / dest_h;
        const uint16_t* src_row = &pixels_ptr[v * image_width];
        uint16_t* dst_row = &_fb[screen_y * 320];
        for (int i = 0; i < dest_w; i++) {
            int screen_x = dest_x + i;
            if (screen_x < 0 || screen_x >= 320) continue;
            int u = (i * image_width) / dest_w;
            dst_row[screen_x] = src_row[u];
        }
    }
}

static int pos_x = 100;
static int pos_y = 70;
static int scale_w = 120;
static int scale_h = 100;

void winit() {
    _sys->width = 320; _sys->height = 240; _sys->bpp = 16; _sys->scale = 4;
    _sys->signal_count = 4;
    _fb = (uint16_t*)(512 + _sys->signal_count);
    const char* t = "Wagnostic - Images Example";
    for (int i = 0; i < 127 && t[i]; i++) ((char*)_sys->message)[i] = t[i];
    _sig[1] = 3;
}

__attribute__((visibility("default")))
void wupdate() {
    _fb = (uint16_t*)(512 + _sys->signal_count);
    for (int i=0; i<320*240; i++) _fb[i] = 0;
    if (_sys->gamepad_buttons & BTN_LEFT)  pos_x -= 2;
    if (_sys->gamepad_buttons & BTN_RIGHT) pos_x += 2;
    if (_sys->gamepad_buttons & BTN_UP)    pos_y -= 2;
    if (_sys->gamepad_buttons & BTN_DOWN)  pos_y += 2;
    if (_sys->gamepad_buttons & BTN_L1) scale_w -= 2;
    if (_sys->gamepad_buttons & BTN_R1) scale_w += 2;
    if (_sys->gamepad_buttons & BTN_L2) scale_h -= 2;
    if (_sys->gamepad_buttons & BTN_R2) scale_h += 2;
    if (_sys->gamepad_buttons & BTN_A) { scale_w = image_width; scale_h = image_height; }
    draw_image_scaled(pos_x, pos_y, scale_w, scale_h);
    _sig[0] = 1;
}
