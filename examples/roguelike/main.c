
#include "data/sprites_data.h"

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

static SystemConfig* _sys = (SystemConfig*)0;
static uint16_t* _fb = (uint16_t*)296;

#define MAP_WIDTH 40
#define MAP_HEIGHT 30

static uint8_t _map[MAP_WIDTH * MAP_HEIGHT];
static int _player_x = 2, _player_y = 2;

// Desenha um sprite 8x8 de uma imagem 256x256
void draw_sprite(int screen_x, int screen_y, int id) {
    // A imagem tem 256 pixels de largura. Cada sprite tem 8x8.
    // Sprites por linha = 256 / 8 = 32.
    int sprites_per_row = 32;
    int sprite_row = id / sprites_per_row;
    int sprite_col = id % sprites_per_row;
    
    int src_x_start = sprite_col * 8;
    int src_y_start = sprite_row * 8;

    for (int j = 0; j < 8; j++) {
        int sy = screen_y * 8 + j;
        if (sy < 0 || sy >= 240) continue;
        
        int src_y = src_y_start + j;
        const uint16_t* src_row = &image_raw[src_y * 256];
        uint16_t* dst_row = &_fb[sy * 320];

        for (int i = 0; i < 8; i++) {
            int sx = screen_x * 8 + i;
            if (sx < 0 || sx >= 320) continue;

            uint16_t color = src_row[src_x_start + i];
            if (color != 0x0000) { // Transparência (Preto)
                dst_row[sx] = color;
            }
        }
    }
}

__attribute__((visibility("default")))
int main() {
    init(320, 240, 320 * 240 * 2, 65536 * 4);

    for (int i = 0; i < MAP_WIDTH * MAP_HEIGHT; i++) {
        _map[i] = (i % 7 == 0 || i % 11 == 0) ? 1 : 0;
    }

    uint32_t last_move = 0;

    while (1) {
        // Clear
        for (int i = 0; i < 320 * 240; i++) _fb[i] = 0x1082;

        // Draw Map
        for (int y = 0; y < MAP_HEIGHT; y++) {
            for (int x = 0; x < MAP_WIDTH; x++) {
                if (_map[y * MAP_WIDTH + x]) draw_sprite(x, y, 1);
            }
        }

        // Handle Input with cooldown
        uint32_t now = get_ticks();
        if (now - last_move > 150) {
            if (_sys->keys[79] || (_sys->gamepad_buttons & BTN_RIGHT)) { _player_x++; last_move = now; }
            if (_sys->keys[80] || (_sys->gamepad_buttons & BTN_LEFT))  { _player_x--; last_move = now; }
            if (_sys->keys[81] || (_sys->gamepad_buttons & BTN_DOWN))  { _player_y++; last_move = now; }
            if (_sys->keys[82] || (_sys->gamepad_buttons & BTN_UP))    { _player_y--; last_move = now; }
        }

        // Draw Player
        draw_sprite(_player_x, _player_y, 0);

        _sys->redraw = 1;
        draw();
    }
    return 0;
}
