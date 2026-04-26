
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef          int   int32_t;

extern void init(int w, int h, int vram, int ram);

#pragma pack(push, 1)
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t ram;
    uint32_t vram;
    uint32_t redraw;
    uint32_t gamepad_buttons;
    int32_t  joystick_lx, joystick_ly, joystick_rx, joystick_ry;
    uint8_t  keys[256];
    int32_t  mouse_x, mouse_y;
    uint32_t mouse_buttons;
    int32_t  mouse_wheel;
    char     title[128];
    uint32_t bpp;
    uint8_t  reserved[68];
} SystemConfig;
#pragma pack(pop)

#define _sys ((volatile SystemConfig*)0)
#define _fb  ((uint8_t*)512)

#define RGB332(r, g, b) (uint8_t)((((r) & 0xE0)) | (((g) & 0xE0) >> 3) | (((b) & 0xC0) >> 6))

void str_cpy(char* dst, const char* src) {
    while((*dst++ = *src++));
}

int main() {
    if (_sys->width == 0) {
        _sys->bpp = 1; // Set BPP BEFORE init so host knows the format immediately
        init(320, 240, 320 * 240 * 1, 0);
        str_cpy((char*)_sys->title, "wagnostic - 8bpp RGB332 Mode");
    }

    // Fill background
    for (int i = 0; i < 320 * 240; i++) _fb[i] = RGB332(40, 40, 40);

    // Draw a square at mouse position
    int x = _sys->mouse_x, y = _sys->mouse_y;
    uint8_t color = RGB332(255, 255, 255);
    if (_sys->mouse_buttons) color = RGB332(255, 0, 0);

    for(int iy = -10; iy < 10; iy++) {
        for(int ix = -10; ix < 10; ix++) {
            int px = x + ix, py = y + iy;
            if(px >= 0 && px < 320 && py >= 0 && py < 240) {
                _fb[py * 320 + px] = color;
            }
        }
    }

    _sys->redraw = 1;
    return 0;
}
