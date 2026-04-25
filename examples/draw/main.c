
#define OLIVEC_IMPLEMENTATION
#include "olive.c"
#include "image_data.h"

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef          int   int32_t;

// Imports do Host
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

static SystemConfig* _sys = (SystemConfig*)0;
static uint16_t* _fb = (uint16_t*)296;
static Olivec_Canvas _oc;
static Olivec_Canvas _img_sprite;

typedef struct {
    float x, y, vx, vy;
    int w, h;
} Sprite;

#define SPRITE_COUNT 200
static Sprite _sprites[SPRITE_COUNT];

int main() {
    // Inicializa o motor via Host
    init(320, 240, 320 * 240 * 2, 1024 * 512);

    _oc = olivec_canvas(_fb, 320, 240, 320);
    _img_sprite = olivec_canvas((uint16_t*)image_raw, image_width, image_height, image_width);

    for (int i = 0; i < SPRITE_COUNT; i++) {
        _sprites[i].w = 24;
        _sprites[i].h = 24;
        _sprites[i].x = (float)(i % 20) * 15;
        _sprites[i].y = (float)(i / 20) * 15;
        _sprites[i].vx = (float)((i % 5) + 1) * 0.5f;
        _sprites[i].vy = (float)((i / 5) % 5 + 1) * 0.5f;
    }

    while (1) {
        olivec_fill(_oc, 0x1818);

        for (int i = 0; i < SPRITE_COUNT; i++) {
            _sprites[i].x += _sprites[i].vx;
            _sprites[i].y += _sprites[i].vy;

            if (_sprites[i].x <= 0 || _sprites[i].x + _sprites[i].w >= 320) _sprites[i].vx *= -1;
            if (_sprites[i].y <= 0 || _sprites[i].y + _sprites[i].h >= 240) _sprites[i].vy *= -1;

            olivec_sprite_copy(_oc, (int)_sprites[i].x, (int)_sprites[i].y, _sprites[i].w, _sprites[i].h, _img_sprite);
        }

        _sys->redraw = 1;
        draw();
    }

    return 0;
}
