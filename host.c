
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <SDL2/SDL.h>
#ifdef PORTMASTER
#include <SDL2/SDL_opengles2.h>
#else
#define GL_GLEXT_PROTOTYPES 1
#include <SDL2/SDL_opengl.h>
#endif

#include "wasm3.h"
#include "m3_env.h"

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
#define BTN_START  (1 << 10)
#define BTN_SELECT (1 << 11)

static IM3Runtime    g_runtime;
static SystemConfig* g_sys = NULL;
static uint16_t*     g_fb = NULL;
static uint32_t      g_sys_offset = 0;
static uint32_t      g_vram_ptr = 0;
static SDL_Window*   g_window = NULL;
static SDL_GLContext g_gl_ctx;
static GLuint        g_fb_tex;
static int           g_scale = 4;

m3ApiRawFunction(host_init) {
    m3ApiGetArg(uint32_t, w)
    m3ApiGetArg(uint32_t, h)
    m3ApiGetArg(uint32_t, vram_size)
    m3ApiGetArg(uint32_t, ram_size)

    uint8_t* mem = m3_GetMemory(g_runtime, NULL, 0);
    g_sys = (SystemConfig*)(mem + g_sys_offset);
    g_sys->width = w; g_sys->height = h;
    g_vram_ptr = g_sys_offset + sizeof(SystemConfig);
    
    uint32_t total_needed = g_vram_ptr + vram_size + ram_size;
    uint32_t required_pages = (total_needed + 65535) / 65536;
    if (required_pages > m3_GetMemorySize(g_runtime) / 65536) ResizeMemory(g_runtime, required_pages);

    mem = m3_GetMemory(g_runtime, NULL, 0);
    g_sys = (SystemConfig*)(mem + g_sys_offset);
    g_fb = (uint16_t*)(mem + g_vram_ptr);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
        m3ApiTrap("SDL_Init failed");
    }

#ifdef PORTMASTER
    g_window = SDL_CreateWindow("funnybuffer", 0, 0, 640, 480, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
#else
    g_window = SDL_CreateWindow("funnybuffer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w * g_scale, h * g_scale, SDL_WINDOW_OPENGL);
#endif

    if (!g_window) m3ApiTrap("Failed to create SDL Window");
    g_gl_ctx = SDL_GL_CreateContext(g_window);
    SDL_GL_SetSwapInterval(1);

    glGenTextures(1, &g_fb_tex); glBindTexture(GL_TEXTURE_2D, g_fb_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    m3ApiSuccess();
}

m3ApiRawFunction(host_draw) {
    if (!g_sys || !g_window) m3ApiSuccess(); 

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) exit(0);
        if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
            int down = (e.type == SDL_KEYDOWN);
            if (e.key.keysym.scancode < 256) g_sys->keys[e.key.keysym.scancode] = down;
            uint32_t bit = 0;
            switch (e.key.keysym.sym) {
                case SDLK_UP: bit = BTN_UP; break; case SDLK_DOWN: bit = BTN_DOWN; break;
                case SDLK_LEFT: bit = BTN_LEFT; break; case SDLK_RIGHT: bit = BTN_RIGHT; break;
                case SDLK_z: bit = BTN_A; break; case SDLK_x: bit = BTN_B; break;
                case SDLK_RETURN: bit = BTN_START; break; case SDLK_ESCAPE: bit = BTN_SELECT; break;
            }
            if (bit) { if (down) g_sys->gamepad_buttons |= bit; else g_sys->gamepad_buttons &= ~bit; }
        }
    }

    if (g_sys->redraw) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, g_sys->width, g_sys->height, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, g_fb);
        g_sys->redraw = 0;
    }

    glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, g_fb_tex);
    glBegin(GL_QUADS);
        glTexCoord2f(0,1); glVertex2f(-1,-1); glTexCoord2f(1,1); glVertex2f(1,-1);
        glTexCoord2f(1,0); glVertex2f(1,1);   glTexCoord2f(0,0); glVertex2f(-1,1);
    glEnd();

    SDL_GL_SwapWindow(g_window);
    m3ApiSuccess();
}

m3ApiRawFunction(host_get_ticks) { m3ApiReturnType(uint32_t); m3ApiReturn(SDL_GetTicks()); }

int main(int argc, char** argv) {
    if (argc < 2) { printf("usage: %s game.wasm [scale]\n", argv[0]); return 1; }
    g_scale = (argc >= 3) ? atoi(argv[2]) : 4;

    IM3Environment env = m3_NewEnvironment();
    g_runtime = m3_NewRuntime(env, 8 * 1024 * 1024, NULL);

    FILE* f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "Error: Cannot open %s\n", argv[1]); return 1; }
    fseek(f, 0, SEEK_END); size_t wsize = ftell(f); rewind(f);
    uint8_t* wasm = malloc(wsize); fread(wasm, 1, wsize, f); fclose(f);

    IM3Module module;
    if (m3_ParseModule(env, &module, wasm, wsize)) { fprintf(stderr, "Error: WASM Parse failed\n"); return 1; }
    m3_LoadModule(g_runtime, module);

    m3_LinkRawFunction(module, "env", "init", "v(iiii)", host_init);
    m3_LinkRawFunction(module, "env", "draw", "v()", host_draw);
    m3_LinkRawFunction(module, "env", "get_ticks", "i()", host_get_ticks);

    IM3Function fn_main;
    M3Result res = m3_FindFunction(&fn_main, g_runtime, "main");
    if (res) { fprintf(stderr, "Error: main function not found: %s\n", res); return 1; }

    printf("Starting WASM main loop...\n");
    res = m3_CallV(fn_main);
    
    if (res) {
        M3ErrorInfo info; m3_GetErrorInfo(g_runtime, &info);
        fprintf(stderr, "WASM Error: %s (%s)\n", res, info.message);
    }

    return 0;
}
