
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef          int   int32_t;

#include "audio_data.h"

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
static uint16_t* _fb;

void winit() {
    _sys->width = 320;
    _sys->height = 240;
    _sys->bpp = 16;
    _sys->scale = 4;
    _sys->signal_count = 4;
    _fb = (uint16_t*)(512 + _sys->signal_count);

    const char* t = "Wagnostic - Audio Player";
    for (int i = 0; i < 127 && t[i]; i++) ((char*)_sys->message)[i] = t[i];
    _sig[1] = 3;

    _sys->audio_size = music_size;
    _sys->audio_sample_rate = 44100;
    _sys->audio_bpp = 2;
    _sys->audio_channels = 2;
}

void fill_audio() {
    uint8_t* mem = (uint8_t*)0;
    uint32_t audio_ptr = 512 + _sys->signal_count + (320 * 240 * 2);
    uint8_t* audio_buf = mem + audio_ptr;
    
    uint32_t to_copy = music_size - _sys->audio_write_ptr;
    if (to_copy > 32768) to_copy = 32768;

    for (uint32_t i = 0; i < to_copy; i++) {
        audio_buf[(_sys->audio_write_ptr + i) % _sys->audio_size] = music_raw[_sys->audio_write_ptr + i];
    }
    _sys->audio_write_ptr = (_sys->audio_write_ptr + to_copy) % _sys->audio_size;
}

__attribute__((visibility("default")))
void wupdate() {
    for (int i = 0; i < 320 * 240; i++) {
        _fb[i] = (_sys->audio_write_ptr >> 8) & 0xFFFF;
    }
    fill_audio();
    _sig[0] = 1;
}
