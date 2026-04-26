
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

#pragma pack(push, 1)
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
    
    // ---- Mouse ----
    int32_t  mouse_x;
    int32_t  mouse_y;
    uint32_t mouse_buttons;
    int32_t  mouse_wheel;

    // ---- Metadata & Future ----
    char     title[128];   // Offset 312
    uint32_t bpp;          // Offset 440 (Bits per pixel: 1, 2, 4)
    uint8_t  reserved[68]; // Padding to reach 512 bytes
} SystemConfig;
#pragma pack(pop)

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

// ---- GLSL 1.20 / GLES 100 shaders ----
#ifdef PORTMASTER
static const char* VERT_SRC =
    "#version 100\n"
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "varying vec2 v_uv;\n"
    "void main() {\n"
    "    v_uv = a_uv;\n"
    "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
    "}\n";

static const char* FRAG_SRC =
    "#version 100\n"
    "precision mediump float;\n"
    "uniform sampler2D u_fb;\n"
    "varying vec2 v_uv;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(u_fb, v_uv);\n"
    "}\n";
#else
static const char* VERT_SRC =
    "#version 120\n"
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "varying vec2 v_uv;\n"
    "void main() {\n"
    "    v_uv = a_uv;\n"
    "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
    "}\n";

static const char* FRAG_SRC =
    "#version 120\n"
    "uniform sampler2D u_fb;\n"
    "varying vec2 v_uv;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(u_fb, v_uv);\n"
    "}\n";
#endif

static GLuint compile_shader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024]; glGetShaderInfoLog(s, sizeof(log), NULL, log);
        fprintf(stderr, "Shader compile error:\n%s\n", log);
    }
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
    GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024]; glGetProgramInfoLog(p, sizeof(log), NULL, log);
        fprintf(stderr, "Program link error:\n%s\n", log);
    }
    return p;
}


static IM3Runtime runtime;
static uint32_t sys_wasm_offset = 0;
static SDL_Window* window = NULL;
static SDL_GLContext gl_ctx = NULL;
static GLuint fb_tex = 0;
static int scale = 4;
static uint32_t W = 0, H = 0;

m3ApiRawFunction(host_init) {
    m3ApiGetArg(uint32_t, w);
    m3ApiGetArg(uint32_t, h);
    m3ApiGetArg(uint32_t, vram);
    m3ApiGetArg(uint32_t, ram);

    printf(">>> Host Init Called: %ux%u (VRAM: %u, RAM: %u)\n", w, h, vram, ram);
    W = w; H = h;

    uint8_t* mem = m3_GetMemory(runtime, NULL, 0);
    SystemConfig* sys = (SystemConfig*)(mem + sys_wasm_offset);
    sys->width = w;
    sys->height = h;
    sys->vram = vram;
    sys->ram = ram;

    if (window) {
        SDL_SetWindowSize(window, W * scale, H * scale);
        glViewport(0, 0, W * scale, H * scale);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fb_tex);
        // Use RGBA8 as internal format for maximum flexibility
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    }

    m3ApiSuccess();
}

m3ApiRawFunction(host_get_ticks) {
    m3ApiReturnType(uint32_t);
    m3ApiReturn(SDL_GetTicks());
}

int main(int argc, char** argv) {
    if (argc < 2) { printf("usage: %s game.wasm [scale]\n", argv[0]); return 1; }
    scale = (argc >= 3) ? atoi(argv[2]) : 4;
    if (scale < 1) scale = 1;

    IM3Environment env     = m3_NewEnvironment();
    runtime = m3_NewRuntime(env, 8 * 1024 * 1024, NULL); 
    
    FILE* f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
    fseek(f, 0, SEEK_END); size_t wsize = ftell(f); rewind(f);
    uint8_t* wasm = malloc(wsize);
    fread(wasm, 1, wsize, f); fclose(f);

    IM3Module module;
    M3Result result = m3_ParseModule(env, &module, wasm, wsize);
    if (result) { fprintf(stderr, "m3_ParseModule error: %s\n", result); return 1; }
    
    result = m3_LoadModule(runtime, module);
    if (result) { fprintf(stderr, "m3_LoadModule error: %s\n", result); return 1; }

    // ONLY zero out the system struct area (first 1KB) to avoid wiping data segments at 1MB+
    uint32_t current_mem_size = 0;
    uint8_t* initial_mem = m3_GetMemory(runtime, &current_mem_size, 0);
    if (initial_mem && current_mem_size >= 1024) {
        memset(initial_mem, 0, 1024);
    }

    m3_LinkRawFunction(module, "env", "get_ticks", "i()", host_get_ticks);
    m3_LinkRawFunction(module, "env", "init", "v(iiii)", host_init);

    IM3Function fn_frame;
    result = m3_FindFunction(&fn_frame, runtime, "game_frame");
    if (result) {
        result = m3_FindFunction(&fn_frame, runtime, "main");
    }
    if (result) { fprintf(stderr, "m3_FindFunction (game_frame/main) error: %s\n", result); return 1; }

    printf("Initializing SDL and Window...\n");
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }

    // Create window immediately with a placeholder size
    window = SDL_CreateWindow("Wagnostic", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) { fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError()); return 1; }
    
    gl_ctx = SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(1);
    glViewport(0, 0, 640, 480);

    GLuint prog = build_program();
    if (!prog) { fprintf(stderr, "Failed to build shader program\n"); return 1; }
    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "u_fb"), 0);

    float quad[] = {
        -1.f, -1.f,  0.f, 1.f,
         1.f, -1.f,  1.f, 1.f,
        -1.f,  1.f,  0.f, 0.f,
         1.f,  1.f,  1.f, 0.f,
    };
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void*)8);

    glGenTextures(1, &fb_tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fb_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int running = 1;
    SDL_Event event;
    uint32_t last_time = SDL_GetTicks();
    char last_title[128] = {0};

    while (running) {
        uint8_t* mem = m3_GetMemory(runtime, NULL, 0);
        if (!mem) { SDL_Delay(10); continue; }
        SystemConfig* sys = (SystemConfig*)(mem + sys_wasm_offset);

        // Update window title if changed in WASM memory (offset 312)
        if (sys->title[0] != '\0' && strcmp(sys->title, last_title) != 0) {
            strncpy(last_title, sys->title, 127);
            SDL_SetWindowTitle(window, last_title);
        }

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
                glViewport(0, 0, event.window.data1, event.window.data2);
            }
            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
                int down = (event.type == SDL_KEYDOWN);
                if (event.key.keysym.scancode < 256) sys->keys[event.key.keysym.scancode] = down ? 1 : 0;
                uint32_t bit = 0;
                switch (event.key.keysym.sym) {
                    case SDLK_UP:     bit = BTN_UP; break;
                    case SDLK_DOWN:   bit = BTN_DOWN; break;
                    case SDLK_LEFT:   bit = BTN_LEFT; break;
                    case SDLK_RIGHT:  bit = BTN_RIGHT; break;
                    case SDLK_z:      bit = BTN_A; break;
                    case SDLK_x:      bit = BTN_B; break;
                    case SDLK_RETURN: bit = BTN_START; break;
                    case SDLK_ESCAPE: bit = BTN_SELECT; break;
                }
                if (bit) { if (down) sys->gamepad_buttons |= bit; else sys->gamepad_buttons &= ~bit; }
            }
            if (event.type == SDL_MOUSEMOTION) {
                int ww, wh;
                SDL_GetWindowSize(window, &ww, &wh);
                if (ww > 0 && wh > 0) {
                    sys->mouse_x = (event.motion.x * W) / ww;
                    sys->mouse_y = (event.motion.y * H) / wh;
                }
            }
            if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
                int down = (event.type == SDL_MOUSEBUTTONDOWN);
                uint32_t bit = (1 << (event.button.button - 1));
                if (down) sys->mouse_buttons |= bit; else sys->mouse_buttons &= ~bit;
            }
            if (event.type == SDL_MOUSEWHEEL) {
                sys->mouse_wheel += event.wheel.y;
            }
        }

        if (fn_frame) {
            result = m3_CallV(fn_frame);
            if (result) {
                fprintf(stderr, "m3_CallV error (TRAP): %s\n", result);
                break;
            }
        }

        mem = m3_GetMemory(runtime, NULL, 0);
        if (mem) {
            sys = (SystemConfig*)(mem + sys_wasm_offset);
            uint8_t* fb = mem + 512;

            if (fb_tex && W > 0 && H > 0) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, fb_tex);

                if (sys->bpp == 4) {
                    // 32bpp: Direct upload
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, fb);
                } else if (sys->bpp == 1) {
                    // 8bpp (RGB332): Convert to 32bpp
                    static uint32_t* temp_fb32 = NULL;
                    temp_fb32 = realloc(temp_fb32, W * H * 4);
                    for(int i=0; i<W*H; i++) {
                        uint8_t c = fb[i];
                        uint8_t r = ((c >> 5) & 0x07) * 255 / 7;
                        uint8_t g = ((c >> 2) & 0x07) * 255 / 7;
                        uint8_t b = (c & 0x03) * 255 / 3;
                        temp_fb32[i] = (255 << 24) | (b << 16) | (g << 8) | r;
                    }
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, temp_fb32);
                } else {
                    // Default 16bpp (RGB565): Convert to 32bpp
                    static uint32_t* temp_fb32 = NULL;
                    temp_fb32 = realloc(temp_fb32, W * H * 4);
                    uint16_t* fb16 = (uint16_t*)fb;
                    for(int i=0; i<W*H; i++) {
                        uint16_t c = fb16[i];
                        uint8_t r = ((c >> 11) & 0x1F) * 255 / 31;
                        uint8_t g = ((c >> 5) & 0x3F) * 255 / 63;
                        uint8_t b = (c & 0x1F) * 255 / 31;
                        temp_fb32[i] = (255 << 24) | (b << 16) | (g << 8) | r;
                    }
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, temp_fb32);
                }
            }
            
            glClear(GL_COLOR_BUFFER_BIT);
            if (W > 0) glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            SDL_GL_SwapWindow(window);
            sys->redraw = 0;
        }

        uint32_t now = SDL_GetTicks();
        uint32_t elapsed = now - last_time;
        if (elapsed < 16) SDL_Delay(16 - elapsed);
        last_time = SDL_GetTicks();
    }
    printf("Main loop exited\n");
    return 0;
}
