typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef          int   int32_t;

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
    uint8_t  keys[256];
} SystemConfig;

static SystemConfig _sys;
// Standard C64 style palette (16 colors)
static uint32_t _pal[16] = {
    0x000000ff, 0xffffffff, 0x880000ff, 0xaaffeeff,
    0xcc44ccff, 0x00cc55ff, 0x0000aaff, 0xeeee77ff,
    0xdd8855ff, 0x664400ff, 0xff7777ff, 0x333333ff,
    0x777777ff, 0xaaff66ff, 0x0088ffff, 0xbbbbbbff
};
static uint8_t* _fb;

void draw_rect(int x, int y, int w, int h, uint8_t color) {
    for (int iy = y; iy < y + h; iy++) {
        for (int ix = x; ix < x + w; ix++) {
            if (ix >= 0 && ix < _sys.width && iy >= 0 && iy < _sys.height) {
                _fb[iy * _sys.width + ix] = color;
            }
        }
    }
}

uint32_t papagaio_system(void) {
    return (uint32_t)&_sys;
}

void papagaio_init(void) {
    _sys.width  = 320;
    _sys.height = 240;
    _sys.vram   = 320 * 240;
    _sys.ram    = 1024 * 512;

    uint32_t base = ((uint32_t)&__heap_base + 65535) & ~65535;
    _sys.vram_ptr = base;
    _sys.ram_ptr  = base + _sys.vram;
    _sys.pal_ptr  = (uint32_t)_pal;

    _sys.pal_dirty = 1;
    _fb = (uint8_t*)_sys.vram_ptr;
}

void papagaio_update(void) {
    // Fill screen with background
    for (int i = 0; i < _sys.width * _sys.height; i++) _fb[i] = 11; // dark grey
    
    // Grid settings
    int cols = 16;
    int rows = 16;
    int cell_w = 16;
    int cell_h = 10;
    int margin_x = (_sys.width - (cols * cell_w)) / 2;
    int margin_y = (_sys.height - (rows * cell_h)) / 2;

    for (int i = 0; i < 256; i++) {
        int cx = i % cols;
        int cy = i / cols;
        
        int px = margin_x + cx * cell_w;
        int py = margin_y + cy * cell_h;
        
        uint8_t col = 12; // Unpressed color (lighter grey)
        
        // Highlight square if key is held down!
        if (_sys.keys[i] == 1) {
            col = 5; // Bright Green
        }
        
        // Draw physical cell with a 1px border visually
        draw_rect(px, py, cell_w - 1, cell_h - 1, col);
    }

    _sys.fb_dirty = 1;
}