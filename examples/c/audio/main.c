#include "wagnostic.h"
#include "audio_data.h"

static uint16_t* _fb;
static uint8_t* _audio_buf;

extern uint32_t get_ticks();

void winit() {
    w_setup("Wagnostic - Audio Player", 320, 240, 16, 4, 8);
    _fb = (uint16_t*)W_FB_PTR;
    
    W_SYS->audio_size = music_size;
    W_SYS->audio_sample_rate = 44100;
    W_SYS->audio_bpp = 2;
    W_SYS->audio_channels = 2;
    W_SYS->audio_write_ptr = 0;
    W_SYS->audio_read_ptr = 0;
    
    W_SIGNALS[4] = W_SIG_UPDATE_AUDIO; 
    _audio_buf = (uint8_t*)w_audio_ptr();
}

void fill_audio() {
    uint32_t to_write = 16384; 
    for (uint32_t i = 0; i < to_write; i++) {
        _audio_buf[W_SYS->audio_write_ptr] = music_data[W_SYS->audio_write_ptr];
        W_SYS->audio_write_ptr = (W_SYS->audio_write_ptr + 1) % music_size;
    }
}

__attribute__((visibility("default")))
void wupdate() {
    static uint32_t last_tick = 0;
    static uint16_t color = 0;
    
    uint32_t now = get_ticks();
    if (now - last_tick > 1000) {
        // Muda a cor da tela baseado na posição da música, mas só a cada 1s
        color = (uint16_t)((W_SYS->audio_write_ptr >> 8) & 0xFFFF);
        last_tick = now;
    }

    for (int i = 0; i < 320 * 240; i++) {
        _fb[i] = color;
    }

    fill_audio();
    w_redraw();
}
