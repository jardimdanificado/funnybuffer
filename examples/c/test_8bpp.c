
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef          int   int32_t;

#pragma pack(push, 1)
typedef struct {
    char     message[128];
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t scale;
    uint32_t audio_size;
    uint32_t audio_write_ptr;
    uint32_t audio_read_ptr;
    uint32_t audio_sample_rate, audio_bpp, audio_channels;
    uint32_t signal_count;
    uint32_t gamepad_buttons;
    int32_t  joystick_lx, joystick_ly, joystick_rx, joystick_ry;
    uint8_t  keys[256];
    int32_t  mouse_x, mouse_y;
    uint32_t mouse_buttons;
    int32_t  mouse_wheel;
    uint8_t  reserved[48];
} SystemConfig;
#pragma pack(pop)

#define _sys ((volatile SystemConfig*)0)
#define _sig ((volatile uint8_t*)512)
static uint8_t* _fb;

#define RGB332(r, g, b) (uint8_t)((((r) & 0xE0)) | (((g) & 0xE0) >> 3) | (((b) & 0xC0) >> 6))

void winit() {
    _sys->width = 320;
    _sys->height = 240;
    _sys->bpp = 8;
    _sys->scale = 4;
    _sys->signal_count = 4;
    _fb = (uint8_t*)(512 + _sys->signal_count);

    const char* t = "Wagnostic - 8bpp Test (RGB332)";
    for (int i = 0; i < 127 && t[i]; i++) ((char*)_sys->message)[i] = t[i];
    _sig[1] = 3; // UPDATE_TITLE
}

void wupdate() {
    for (int y = 0; y < 240; y++) {
        for (int x = 0; x < 320; x++) {
            uint8_t r = (x * 255) / 320;
            uint8_t g = (y * 255) / 240;
            uint8_t b = 128;
            _fb[y * 320 + x] = RGB332(r, g, b);
        }
    }

    _sig[0] = 1;
}
