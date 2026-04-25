typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef          int   int32_t;

#include "data/sprites_data.h"

extern unsigned char __heap_base;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t ram;         
    uint32_t vram;        
    uint32_t ram_ptr;     
    uint32_t vram_ptr;    
    uint32_t pal_ptr;     
    uint32_t fb_dirty;    
    uint32_t pal_dirty;   

    uint32_t gamepad_buttons; 
    int32_t  joystick_lx;     
    int32_t  joystick_ly;
    int32_t  joystick_rx;     
    int32_t  joystick_ry;
} SystemConfig;

#define BTN_UP     (1 << 0)
#define BTN_DOWN   (1 << 1)
#define BTN_LEFT   (1 << 2)
#define BTN_RIGHT  (1 << 3)
#define BTN_A      (1 << 4)
#define BTN_B      (1 << 5)
#define BTN_START  (1 << 10)
#define BTN_SELECT (1 << 11)

static SystemConfig _sys;
static uint8_t* _fb;

// Skip the first 4 bytes (width, height) to get direct pixel data from the raw spritesheet
static const uint8_t* sheet_ptr = &image_raw[4];

// Draws a tile (0 to 63) from the 8x8 spritesheet onto the screen grid coordinate (tx, ty)
void draw_tile(int tile_idx, int tx, int ty) {
    if (tile_idx < 0 || tile_idx > 63) return;
    
    int spritesheet_x = (tile_idx % 8) * 32;
    int spritesheet_y = (tile_idx / 8) * 32;
    
    int screen_px = tx * 32;
    int screen_py = ty * 32;
    
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            uint8_t pixel = sheet_ptr[(spritesheet_y + y) * 256 + (spritesheet_x + x)];
            
            // Color index 0 is our background/transparent color
            if (pixel != 0) { 
                int scr_idx = (screen_py + y) * _sys.width + (screen_px + x);
                // Bounds check
                if (scr_idx >= 0 && scr_idx < _sys.width * _sys.height) {
                    _fb[scr_idx] = pixel;
                }
            }
        }
    }
}

uint32_t papagaio_system(void) {
    return (uint32_t)&_sys;
}

void papagaio_init(void) {
    _sys.width    = 320;
    _sys.height   = 240;
    _sys.ram      = 65536 * 4; // Allocate enough ram
    _sys.vram     = 320 * 240;
    _sys.fb_dirty = 0;
    _sys.pal_dirty = 1;

    uint32_t base   = ((uint32_t)&__heap_base + 65535) & ~65535;
    _sys.vram_ptr   = base;
    _sys.ram_ptr    = base + _sys.vram;
    _sys.pal_ptr    = (uint32_t)image_palette;

    _fb = (uint8_t*)_sys.vram_ptr;
}

// Simple map offset to test inputs
int px = 0;
int py = 0;

void papagaio_update(void) {
    // Clear screen
    for (int i=0; i<_sys.width*_sys.height; i++) _fb[i] = 0;

    if (_sys.gamepad_buttons & BTN_LEFT)  px--;
    if (_sys.gamepad_buttons & BTN_RIGHT) px++;
    if (_sys.gamepad_buttons & BTN_UP)    py--;
    if (_sys.gamepad_buttons & BTN_DOWN)  py++;

    // Test map: Draw the 64 sprites sequentially on the screen
    int idx = 0;
    for (int ty = 0; ty < 7; ty++) {
        for (int tx = 0; tx < 10; tx++) {
            // Draw a tile, applying movement offset
            draw_tile(idx, tx + (px/10), ty + (py/10));
            idx++;
            if (idx >= 64) idx = 0;
        }
    }

    _sys.fb_dirty = 1;
}
