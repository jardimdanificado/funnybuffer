
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef          int   int32_t;

#include "audio_data.h"



#pragma pack(push, 1)
typedef struct {
    char     title[128];
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
#define _fb ((volatile uint16_t*)(512 + 1))
#define _sig ((volatile uint8_t*)512)

void winit() {
    _sys->width = 320;
    _sys->height = 240;
    _sys->bpp = 16;
    _sys->scale = 4;
    _sys->signal_count = 1;
    _sys->audio_size = 64 * 1024 * 1024; // HUGE
    _sys->audio_sample_rate = 44100; // music_rate
    _sys->audio_bpp = 2; // music_bpp
    _sys->audio_channels = 2; // music_channels
    const char* t = "Wagnostic - Audio Player";
    for (int i = 0; i < 127 && t[i]; i++) ((char*)_sys->title)[i] = t[i];
}

#define AUDIO_RING_SIZE 32768
static uint32_t music_pos = 0;

__attribute__((visibility("default")))
void wupdate() {

    // Fill ring buffer with music data
    uint8_t* mem = (uint8_t*)0;
    uint32_t vram_size = _sys->width * _sys->height * (_sys->bpp / 8);
    uint8_t* audio_ring = mem + 512 + vram_size;

    uint32_t r = _sys->audio_read_ptr;
    uint32_t w = _sys->audio_write_ptr;
    uint32_t size = _sys->audio_size;

    // Calculate free space
    int free_space = (r > w) ? (r - w - 1) : (size - w + r - 1);
    
    if (free_space > 0 && music_pos < music_size) {
        int to_copy = (music_size - music_pos);
        if (to_copy > free_space) to_copy = free_space;

        for (int i = 0; i < to_copy; i++) {
            audio_ring[w] = music_data[music_pos++];
            w = (w + 1) % size;
        }
        _sys->audio_write_ptr = w;
    }

    // Simple visualization (optional)
    uint16_t* fb = (uint16_t*)(mem + 512);
    for (int i = 0; i < 320 * 240; i++) fb[i] = 0;
    
    // Draw progress bar
    int progress = (music_pos * 300) / music_size;
    for (int x = 10; x < 10 + progress; x++) {
        for (int y = 110; y < 130; y++) {
            fb[y * 320 + x] = 0x07E0; // Green
        }
    }

    _sig[0] = 1;
}
