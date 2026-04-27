
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef          short int16_t;
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
static uint16_t* _fb;

void winit() {
    _sys->width = 320;
    _sys->height = 240;
    _sys->bpp = 16;
    _sys->scale = 4;
    _sys->signal_count = 4;
    _fb = (uint16_t*)(512 + _sys->signal_count);
    _sys->audio_size = 16384;
    _sys->audio_sample_rate = 44100;
    _sys->audio_bpp = 2;
    _sys->audio_channels = 1;
    const char* t = "Wagnostic - Audio Sine Test";
    for (int i = 0; i < 127 && t[i]; i++) ((char*)_sys->message)[i] = t[i];
    _sig[1] = 3;
}

#define SAMPLE_RATE 44100
#define PI 3.14159265f
static float phase = 0;

__attribute__((visibility("default")))
void wupdate() {
    uint8_t* mem = (uint8_t*)0;
    uint32_t audio_ptr = 512 + _sys->signal_count + (_sys->width * _sys->height * (_sys->bpp / 8));
    int16_t* audio_buf = (int16_t*)(mem + audio_ptr);

    uint32_t free_space;
    if (_sys->audio_write_ptr >= _sys->audio_read_ptr) {
        free_space = _sys->audio_size - (_sys->audio_write_ptr - _sys->audio_read_ptr);
    } else {
        free_space = _sys->audio_read_ptr - _sys->audio_write_ptr;
    }

    uint32_t to_write = free_space > 1024 ? 1024 : free_space;
    for (uint32_t i = 0; i < to_write / 2; i++) {
        float sample = 0.5f * __builtin_sinf(phase);
        audio_buf[(_sys->audio_write_ptr / 2 + i) % (_sys->audio_size / 2)] = (int16_t)(sample * 32767);
        phase += 2.0f * PI * 440.0f / SAMPLE_RATE;
        if (phase > 2.0f * PI) phase -= 2.0f * PI;
    }
    _sys->audio_write_ptr = (_sys->audio_write_ptr + to_write) % _sys->audio_size;

    // Simple visual feedback
    for(int i=0; i<320*240; i++) _fb[i] = (uint16_t)(phase * 1000);
    _sig[0] = 1;
}
