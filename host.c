
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
    uint32_t width;
    uint32_t height;
    uint32_t ram;
    uint32_t vram;
    uint32_t redraw;
    
    // ---- Inputs ----
    uint32_t gamepad_buttons;
    int32_t  joystick_lx;
    int32_t  joystick_ly;
    int32_t  joystick_rx;
    int32_t  joystick_ry;
    uint8_t  keys[256];
} SystemConfig;

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

// Global state for the engine
static IM3Runtime    g_runtime;
static SystemConfig* g_sys;
static uint16_t*     g_fb;
static uint32_t      g_sys_offset = 0;
static uint32_t      g_vram_ptr = 0;

static SDL_Window*   g_window;
static SDL_GLContext g_gl_ctx;
static GLuint        g_fb_tex;
static int           g_scale = 4;
static int           g_running = 1;

// ---- GLSL Shaders ----
#ifdef PORTMASTER
static const char* VERT_SRC = "#version 100\nattribute vec2 a_pos;attribute vec2 a_uv;varying vec2 v_uv;void main(){v_uv=a_uv;gl_Position=vec4(a_pos,0.0,1.0);}\n";
static const char* FRAG_SRC = "#version 100\nprecision mediump float;uniform sampler2D u_fb;varying vec2 v_uv;void main(){gl_FragColor=texture2D(u_fb,v_uv);}\n";
#else
static const char* VERT_SRC = "#version 120\nattribute vec2 a_pos;attribute vec2 a_uv;varying vec2 v_uv;void main(){v_uv=a_uv;gl_Position=vec4(a_pos,0.0,1.0);}\n";
static const char* FRAG_SRC = "#version 120\nuniform sampler2D u_fb;varying vec2 v_uv;void main(){gl_FragColor=texture2D(u_fb,v_uv);}\n";
#endif

static GLuint compile_shader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    return s;
}

static GLuint build_program(void) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER,   VERT_SRC);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, FRAG_SRC);
    GLuint p  = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glBindAttribLocation(p, 0, "a_pos");
    glBindAttribLocation(p, 1, "a_uv");
    glLinkProgram(p);
    return p;
}

// ---- WASM Imports ----

m3ApiRawFunction(host_init) {
    m3ApiGetArg(uint32_t, w)
    m3ApiGetArg(uint32_t, h)
    m3ApiGetArg(uint32_t, vram_size)
    m3ApiGetArg(uint32_t, ram_size)

    uint8_t* mem = m3_GetMemory(g_runtime, NULL, 0);
    g_sys = (SystemConfig*)(mem + g_sys_offset);
    
    g_sys->width = w;
    g_sys->height = h;
    g_sys->vram = vram_size;
    g_sys->ram = ram_size;

    g_vram_ptr = g_sys_offset + sizeof(SystemConfig);
    uint32_t total_needed = g_vram_ptr + vram_size + ram_size;
    uint32_t required_pages = (total_needed + 65535) / 65536;
    uint32_t current_pages = m3_GetMemorySize(g_runtime) / 65536;

    if (required_pages > current_pages) {
        ResizeMemory(g_runtime, required_pages);
    }

    // Refresh pointers after possible resize
    mem = m3_GetMemory(g_runtime, NULL, 0);
    g_sys = (SystemConfig*)(mem + g_sys_offset);
    g_fb = (uint16_t*)(mem + g_vram_ptr);

    // Initialize SDL Window
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
    if (SDL_NumJoysticks() > 0) {
        if (SDL_IsGameController(0)) SDL_GameControllerOpen(0);
        else SDL_JoystickOpen(0);
    }

#ifdef PORTMASTER
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_ShowCursor(0);
    g_window = SDL_CreateWindow("funnybuffer", 0, 0, 640, 480, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    g_window = SDL_CreateWindow("funnybuffer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w * g_scale, h * g_scale, SDL_WINDOW_OPENGL);
#endif
    g_gl_ctx = SDL_GL_CreateContext(g_window);
    SDL_GL_SetSwapInterval(1);

    GLuint prog = build_program();
    glUseProgram(prog);
    
    float quad[] = { -1.f,-1.f,0.f,1.f, 1.f,-1.f,1.f,1.f, -1.f,1.f,0.f,0.f, 1.f,1.f,1.f,0.f };
    GLuint vbo; glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void*)8);

    glGenTextures(1, &g_fb_tex);
    glBindTexture(GL_TEXTURE_2D, g_fb_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);

    m3ApiSuccess();
}

m3ApiRawFunction(host_draw) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) { g_running = 0; exit(0); }
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
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    SDL_GL_SwapWindow(g_window);

    m3ApiSuccess();
}

m3ApiRawFunction(host_get_ticks) {
    m3ApiReturnType(uint32_t);
    m3ApiReturn(SDL_GetTicks());
}

int main(int argc, char** argv) {
    if (argc < 2) { printf("usage: %s game.wasm [scale]\n", argv[0]); return 1; }
    g_scale = (argc >= 3) ? atoi(argv[2]) : 4;

    IM3Environment env = m3_NewEnvironment();
    g_runtime = m3_NewRuntime(env, 8 * 1024 * 1024, NULL);

    FILE* f = fopen(argv[1], "rb");
    fseek(f, 0, SEEK_END); size_t wsize = ftell(f); rewind(f);
    uint8_t* wasm = malloc(wsize); fread(wasm, 1, wsize, f); fclose(f);

    IM3Module module;
    m3_ParseModule(env, &module, wasm, wsize);
    m3_LoadModule(g_runtime, module);

    m3_LinkRawFunction(module, "env", "init",      "v(iiii)", host_init);
    m3_LinkRawFunction(module, "env", "draw",      "v()",     host_draw);
    m3_LinkRawFunction(module, "env", "get_ticks", "i()",     host_get_ticks);

    IM3Function fn_main;
    if (m3_FindFunction(&fn_main, g_runtime, "main") != m3Err_none) {
        fprintf(stderr, "main function not found\n");
        return 1;
    }

    m3_CallV(fn_main);
    return 0;
}
