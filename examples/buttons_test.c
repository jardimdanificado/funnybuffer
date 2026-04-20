
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef          int   int32_t;

extern unsigned char __heap_base;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t ram;         // bytes
    uint32_t vram;        // bytes
    uint32_t ram_ptr;     // wasm linear memory offset for RAM
    uint32_t vram_ptr;    // wasm linear memory offset for VRAM (fb)
    uint32_t pal_ptr;     // wasm linear memory offset for palette
    uint32_t fb_dirty;    // set to 1 when fb was written this frame
    uint32_t pal_dirty;   // set to 1 when palette changed

    // ---- Gamepad / Inputs Mapped ----
    uint32_t gamepad_buttons; // Bitmask dos 16 botoes
    int32_t  joystick_lx;     // Analogico esquerdo (-32768 a 32767)
    int32_t  joystick_ly;
    int32_t  joystick_rx;     // Analogico direito (-32768 a 32767)
    int32_t  joystick_ry;
} SystemConfig;

// Mapeamento dos bits na mascara gamepad_buttons
#define BTN_UP     (1 << 0)
#define BTN_DOWN   (1 << 1)
#define BTN_LEFT   (1 << 2)
#define BTN_RIGHT  (1 << 3)
#define BTN_A      (1 << 4)
#define BTN_B      (1 << 5)
#define BTN_X      (1 << 6)
#define BTN_Y      (1 << 7)
#define BTN_L1     (1 << 8)
#define BTN_R1     (1 << 9)
#define BTN_START  (1 << 10)
#define BTN_SELECT (1 << 11)
#define BTN_L2     (1 << 12)
#define BTN_R2     (1 << 13)
#define BTN_L3     (1 << 14)
#define BTN_R3     (1 << 15)

#define MAX_DIRTY 32

static SystemConfig _sys;
static uint32_t     _pal[256] = {
    0x00000000, 0x800000ff, 0x008000ff, 0x808000ff, 0x000080ff, 0x800080ff, 0x008080ff, 0xc0c0c0ff,
    0xc0dcc0ff, 0xa6caf0ff, 0x2a3faaff, 0x2a3fffff, 0x2a5f00ff, 0x2a5f55ff, 0x2a5faaff, 0x2a5fffff,
    0x2a7f00ff, 0x2a7f55ff, 0x2a7faaff, 0x2a7fffff, 0x2a9f00ff, 0x2a9f55ff, 0x2a9faaff, 0x2a9fffff,
    0x2abf00ff, 0x2abf55ff, 0x2abfaaff, 0x2abfffff, 0x2adf00ff, 0x2adf55ff, 0x2adfaaff, 0x2adfffff,
    0x2aff00ff, 0x2aff55ff, 0x2affaaff, 0x2affffff, 0x550000ff, 0x550055ff, 0x5500aaff, 0x5500ffff,
    0x551f00ff, 0x551f55ff, 0x551faaff, 0x551fffff, 0x553f00ff, 0x553f55ff, 0x553faaff, 0x553fffff,
    0x555f00ff, 0x555f55ff, 0x555faaff, 0x555fffff, 0x557f00ff, 0x557f55ff, 0x557faaff, 0x557fffff,
    0x559f00ff, 0x559f55ff, 0x559faaff, 0x559fffff, 0x55bf00ff, 0x55bf55ff, 0x55bfaaff, 0x55bfffff,
    0x55df00ff, 0x55df55ff, 0x55dfaaff, 0x55dfffff, 0x55ff00ff, 0x55ff55ff, 0x55ffaaff, 0x55ffffff,
    0x7f0000ff, 0x7f0055ff, 0x7f00aaff, 0x7f00ffff, 0x7f1f00ff, 0x7f1f55ff, 0x7f1faaff, 0x7f1fffff,
    0x7f3f00ff, 0x7f3f55ff, 0x7f3faaff, 0x7f3fffff, 0x7f5f00ff, 0x7f5f55ff, 0x7f5faaff, 0x7f5fffff,
    0x7f7f00ff, 0x7f7f55ff, 0x7f7faaff, 0x7f7fffff, 0x7f9f00ff, 0x7f9f55ff, 0x7f9faaff, 0x7f9fffff,
    0x7fbf00ff, 0x7fbf55ff, 0x7fbfaaff, 0x7fbfffff, 0x7fdf00ff, 0x7fdf55ff, 0x7fdfaaff, 0x7fdfffff,
    0x7fff00ff, 0x7fff55ff, 0x7fffaaff, 0x7fffffff, 0xaa0000ff, 0xaa0055ff, 0xaa00aaff, 0xaa00ffff,
    0xaa1f00ff, 0xaa1f55ff, 0xaa1faaff, 0xaa1fffff, 0xaa3f00ff, 0xaa3f55ff, 0xaa3faaff, 0xaa3fffff,
    0xaa5f00ff, 0xaa5f55ff, 0xaa5faaff, 0xaa5fffff, 0xaa7f00ff, 0xaa7f55ff, 0xaa7faaff, 0xaa7fffff,
    0xaa9f00ff, 0xaa9f55ff, 0xaa9faaff, 0xaa9fffff, 0xaabf00ff, 0xaabf55ff, 0xaabfaaff, 0xaabfffff,
    0xaadf00ff, 0xaadf55ff, 0xaadfaaff, 0xaadfffff, 0xaaff00ff, 0xaaff55ff, 0xaaffaaff, 0xaaffffff,
    0xd40000ff, 0xd40055ff, 0xd400aaff, 0xd400ffff, 0xd41f00ff, 0xd41f55ff, 0xd41faaff, 0xd41fffff,
    0xd43f00ff, 0xd43f55ff, 0xd43faaff, 0xd43fffff, 0xd45f00ff, 0xd45f55ff, 0xd45faaff, 0xd45fffff,
    0xd47f00ff, 0xd47f55ff, 0xd47faaff, 0xd47fffff, 0xd49f00ff, 0xd49f55ff, 0xd49faaff, 0xd49fffff,
    0xd4bf00ff, 0xd4bf55ff, 0xd4bfaaff, 0xd4bfffff, 0xd4df00ff, 0xd4df55ff, 0xd4dfaaff, 0xd4dfffff,
    0xd4ff00ff, 0xd4ff55ff, 0xd4ffaaff, 0xd4ffffff, 0xff0055ff, 0xff00aaff, 0xff1f00ff, 0xff1f55ff,
    0xff1faaff, 0xff1fffff, 0xff3f00ff, 0xff3f55ff, 0xff3faaff, 0xff3fffff, 0xff5f00ff, 0xff5f55ff,
    0xff5faaff, 0xff5fffff, 0xff7f00ff, 0xff7f55ff, 0xff7faaff, 0xff7fffff, 0xff9f00ff, 0xff9f55ff,
    0xff9faaff, 0xff9fffff, 0xffbf00ff, 0xffbf55ff, 0xffbfaaff, 0xffbfffff, 0xffdf00ff, 0xffdf55ff,
    0xffdfaaff, 0xffdfffff, 0xffff55ff, 0xffffaaff, 0xccccffff, 0xffccffff, 0x33ffffff, 0x66ffffff,
    0x99ffffff, 0xccffffff, 0x007f00ff, 0x007f55ff, 0x007faaff, 0x007fffff, 0x009f00ff, 0x009f55ff,
    0x009faaff, 0x009fffff, 0x00bf00ff, 0x00bf55ff, 0x00bfaaff, 0x00bfffff, 0x00df00ff, 0x00df55ff,
    0x00dfaaff, 0x00dfffff, 0x00ff55ff, 0x00ffaaff, 0x2a0000ff, 0x2a0055ff, 0x2a00aaff, 0x2a00ffff,
    0x2a1f00ff, 0x2a1f55ff, 0x2a1faaff, 0x2a1fffff, 0x2a3f00ff, 0x2a3f55ff, 0xfffbf0ff, 0xa0a0a4ff,
    0x808080ff, 0xff0000ff, 0x00ff00ff, 0xffff00ff, 0x0000ffff, 0xff00ffff, 0x00ffffff, 0xffffffff
};
static uint8_t* _fb;

static int t = 0;

// papagaio_ functions are auto-exported by papagaiocc
// Host calls this to get the SystemConfig address as a wasm offset
uint32_t papagaio_system(void) {
    return (uint32_t)&_sys;
}

void papagaio_init(void) {
    _sys.width    = 320;
    _sys.height   = 240;
    _sys.ram      = 65536;  // 64 KB
    _sys.vram     = 65536;  // 64 KB
    _sys.fb_dirty = 0;
    _sys.pal_dirty = 0;

    // Game computes its own layout — it has native access to __heap_base
    uint32_t base   = ((uint32_t)&__heap_base + 65535) & ~65535;
    _sys.vram_ptr   = base;
    _sys.ram_ptr    = base + _sys.vram;
    _sys.pal_ptr    = (uint32_t)_pal;

    // Set fb pointer inside the game (no injection needed from host)
    _fb = (uint8_t*)_sys.vram_ptr;
}

void papagaio_update(void) {
    t++;

    // Limpa a tela frame a frame (fundo preto = 255)
    for (int i=0; i<_sys.width*_sys.height; i++) _fb[i] = 255;

    // Define uma posicao central
    int cx = _sys.width / 2;
    int cy = _sys.height / 2;

    // Movimentacao digital pelo D-PAD principal
    if ((_sys.gamepad_buttons & BTN_LEFT)  != 0) cx -= 30;
    if ((_sys.gamepad_buttons & BTN_RIGHT) != 0) cx += 30;
    if ((_sys.gamepad_buttons & BTN_UP)    != 0) cy -= 30;
    if ((_sys.gamepad_buttons & BTN_DOWN)  != 0) cy += 30;

    // Movimentacao dinamica analógica somada no Joy 1 e 2
    cx += (_sys.joystick_lx / 1000);
    cy += (_sys.joystick_ly / 1000);
    cx += (_sys.joystick_rx / 2000); // 2o analógico ajuda horizontalmente
    cy += (_sys.joystick_ry / 2000); // CORRECAO: 2o analógico move verticalmente

    // Testa os botões de A, B, X, Y alterando a cor do objeto principal
    int color = 55;
    if ((_sys.gamepad_buttons & BTN_A) != 0) color = 10;
    if ((_sys.gamepad_buttons & BTN_B) != 0) color = 30;
    if ((_sys.gamepad_buttons & BTN_X) != 0) color = 50;
    if ((_sys.gamepad_buttons & BTN_Y) != 0) color = 70;

    // Testa R/L mudando a grossura / tamanho com os gatilhos e botoes shoulder
    int size = 15;
    if ((_sys.gamepad_buttons & BTN_L1) != 0) size += 10;
    if ((_sys.gamepad_buttons & BTN_R1) != 0) size += 10;
    if ((_sys.gamepad_buttons & BTN_L2) != 0) size += 15;
    if ((_sys.gamepad_buttons & BTN_R2) != 0) size += 15;
    if ((_sys.gamepad_buttons & BTN_L3) != 0) color = 80;
    if ((_sys.gamepad_buttons & BTN_R3) != 0) color = 90;

    // Ações para Start e Select - Reage com brilho e miniaturização!
    if ((_sys.gamepad_buttons & BTN_START) != 0) color = 130;
    if ((_sys.gamepad_buttons & BTN_SELECT) != 0) size = 5;

    for (int y = cy - size; y < cy + size; y++) {
        for (int x = cx - size; x < cx + size; x++) {
            if (x >= 0 && x < _sys.width && y >= 0 && y < _sys.height) {
                _fb[y * _sys.width + x] = color;
            }
        }
    }

    _sys.fb_dirty = 1;
}