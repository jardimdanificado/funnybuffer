
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
    uint32_t ram_ptr;
    uint32_t vram_ptr;
    uint32_t pal_ptr;
    uint32_t fb_dirty;
    uint32_t pal_dirty;
    
    // ---- Inputs ----
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
    "uniform sampler2D u_pal;\n"
    "varying vec2 v_uv;\n"
    "void main() {\n"
    "    float idx = texture2D(u_fb, v_uv).r;\n"
    "    float palX = (idx * 255.0 + 0.5) / 256.0;\n"
    "    gl_FragColor = texture2D(u_pal, vec2(palX, 0.5));\n"
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
    "uniform sampler2D u_pal;\n"
    "varying vec2 v_uv;\n"
    "void main() {\n"
    "    float idx = texture2D(u_fb, v_uv).r;\n"
    "    float palX = (idx * 255.0 + 0.5) / 256.0;\n"
    "    gl_FragColor = texture2D(u_pal, vec2(palX, 0.5));\n"
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

int main(int argc, char** argv) {
    if (argc < 2) { printf("usage: %s game.wasm [scale]\n", argv[0]); return 1; }
    int scale = (argc >= 3) ? atoi(argv[2]) : 4;
    if (scale < 1) scale = 1;

    // ---- Bootstrap Wasm3 ----
    IM3Environment env     = m3_NewEnvironment();
    IM3Runtime     runtime = m3_NewRuntime(env, 4 * 1024 * 1024, NULL);

    FILE* f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
    fseek(f, 0, SEEK_END); size_t wsize = ftell(f); rewind(f);
    uint8_t* wasm = malloc(wsize);
    fread(wasm, 1, wsize, f); fclose(f);

    IM3Module module;
    m3_ParseModule(env, &module, wasm, wsize);
    m3_LoadModule(runtime, module);

    IM3Function fn_init, fn_update, fn_system;
    m3_FindFunction(&fn_init,   runtime, "papagaio_init");
    m3_FindFunction(&fn_update, runtime, "papagaio_update");
    m3_FindFunction(&fn_system, runtime, "papagaio_system");

    // papagaio_init computes memory layout internally (game owns __heap_base access)
    m3_CallV(fn_init);

    // papagaio_system() returns wasm offset of SystemConfig struct
    uint32_t sys_wasm_offset = 0;
    m3_CallV(fn_system);
    m3_GetResultsV(fn_system, &sys_wasm_offset);

    fprintf(stderr, "sys_wasm_offset=0x%x\n", sys_wasm_offset);

    uint8_t* mem = m3_GetMemory(runtime, NULL, 0);
    SystemConfig* sys = (SystemConfig*)(mem + sys_wasm_offset);

    fprintf(stderr, "width=%u height=%u vram_ptr=0x%x ram_ptr=0x%x pal_ptr=0x%x\n",
            sys->width, sys->height, sys->vram_ptr, sys->ram_ptr, sys->pal_ptr);

    // Host does the memory grow via Wasm3 internal API
    uint32_t total_needed   = sys->ram_ptr + sys->ram;
    uint32_t required_pages = (total_needed + 65535) / 65536;
    uint32_t current_pages  = m3_GetMemorySize(runtime) / 65536;
    if (required_pages > current_pages)
        ResizeMemory(runtime, required_pages);

    // Refresh pointers after grow
    mem = m3_GetMemory(runtime, NULL, 0);
    sys = (SystemConfig*)(mem + sys_wasm_offset);
    uint8_t*  fb  = mem + sys->vram_ptr;
    uint32_t* pal = (uint32_t*)(mem + sys->pal_ptr);
    uint32_t  W   = sys->width, H = sys->height;

    // ---- SDL + OpenGL Context Initialization ----
#ifdef PORTMASTER
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
    if (SDL_NumJoysticks() > 0) {
        if (SDL_IsGameController(0)) SDL_GameControllerOpen(0);
        else SDL_JoystickOpen(0);
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES); // Mobile EGL
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_ShowCursor(0);
    SDL_Window* window = SDL_CreateWindow(
        "papagame",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        640, 480, // R36S native bounds
        SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN
    );
#else
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
    if (SDL_NumJoysticks() > 0) {
        if (SDL_IsGameController(0)) SDL_GameControllerOpen(0);
        else SDL_JoystickOpen(0);
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow(
        "papagame",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        W * scale, H * scale,
        SDL_WINDOW_OPENGL
    );
#endif
    SDL_GLContext ctx = SDL_GL_CreateContext(window);
    if (!ctx) { fprintf(stderr, "GL context error: %s\n", SDL_GetError()); return 1; }
    SDL_GL_SetSwapInterval(1);
    fprintf(stderr, "GL: %s\n", glGetString(GL_VERSION));

    // ---- Build shader ----
    GLuint prog = build_program();
    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "u_fb"),  0);
    glUniform1i(glGetUniformLocation(prog, "u_pal"), 1);

    // ---- Fullscreen quad VBO ----
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

    // ---- Framebuffer texture (GL_LUMINANCE 8-bit, native res) ----
    GLuint fb_tex;
    glGenTextures(1, &fb_tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fb_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, W, H, 0,
                 GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);

#ifdef PORTMASTER
    uint32_t init_pal[256];
    for (int i=0; i<256; i++) init_pal[i] = __builtin_bswap32(pal[i]);
#endif

    GLuint pal_tex;
    glGenTextures(1, &pal_tex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, pal_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#ifdef PORTMASTER
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 1, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, init_pal);
#else
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 1, 0,
                 GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, pal);
#endif

#ifndef PORTMASTER
    // ---- Double PBOs for async fb upload ----
    size_t fb_size = W * H;
    GLuint pbo[2];
    glGenBuffers(2, pbo);
    for (int i = 0; i < 2; i++) {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo[i]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, fb_size, NULL, GL_STREAM_DRAW);
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    int cur_pbo = 0;
#endif

    // ---- Main loop ----
    int running = 1;
    SDL_Event e;
    glClearColor(0.2f, 0.0f, 0.0f, 1.0f); // Dark Red fallback if shader fails

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;

            // ---- Inputs: Teclado (Para PC ou GPTOKEYB mappings) ----
            if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
                int down = (e.type == SDL_KEYDOWN);
                uint32_t bit = 0;
                switch (e.key.keysym.sym) {
                    case SDLK_UP:     bit = BTN_UP; break;
                    case SDLK_DOWN:   bit = BTN_DOWN; break;
                    case SDLK_LEFT:   bit = BTN_LEFT; break;
                    case SDLK_RIGHT:  bit = BTN_RIGHT; break;
                    case SDLK_z:      bit = BTN_A; break;
                    case SDLK_x:      bit = BTN_B; break;
                    case SDLK_s:      bit = BTN_X; break;
                    case SDLK_a:      bit = BTN_Y; break;
                    case SDLK_q:      bit = BTN_L1; break;
                    case SDLK_w:      bit = BTN_R1; break;
                    case SDLK_e:      bit = BTN_L2; break;
                    case SDLK_r:      bit = BTN_R2; break;
                    case SDLK_RETURN: bit = BTN_START; break;
                    case SDLK_SPACE:  
                    case SDLK_ESCAPE: bit = BTN_SELECT; break;
                }
                if (bit) {
                    if (down) sys->gamepad_buttons |= bit;
                    else      sys->gamepad_buttons &= ~bit;
                }
            }

            // ---- Inputs: SDL GameController Oficial ----
            if (e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONUP) {
                int down = (e.type == SDL_CONTROLLERBUTTONDOWN);
                uint32_t bit = 0;
                switch (e.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:    bit = BTN_UP; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  bit = BTN_DOWN; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  bit = BTN_LEFT; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: bit = BTN_RIGHT; break;
                    case SDL_CONTROLLER_BUTTON_A:          bit = BTN_A; break;
                    case SDL_CONTROLLER_BUTTON_B:          bit = BTN_B; break;
                    case SDL_CONTROLLER_BUTTON_X:          bit = BTN_X; break;
                    case SDL_CONTROLLER_BUTTON_Y:          bit = BTN_Y; break;
                    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: bit = BTN_L1; break;
                    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:bit = BTN_R1; break;
                    case SDL_CONTROLLER_BUTTON_START:      bit = BTN_START; break;
                    case SDL_CONTROLLER_BUTTON_BACK:       bit = BTN_SELECT; break;
                    case SDL_CONTROLLER_BUTTON_LEFTSTICK:  bit = BTN_L3; break;
                    case SDL_CONTROLLER_BUTTON_RIGHTSTICK: bit = BTN_R3; break;
                }
                if (bit) {
                    if (down) sys->gamepad_buttons |= bit;
                    else      sys->gamepad_buttons &= ~bit;
                }
            }
            if (e.type == SDL_CONTROLLERAXISMOTION) {
                if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX)       sys->joystick_lx = e.caxis.value;
                else if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)  sys->joystick_ly = e.caxis.value;
                else if (e.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTX) sys->joystick_rx = e.caxis.value;
                else if (e.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTY) sys->joystick_ry = e.caxis.value;
                else if (e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT) {
                    if (e.caxis.value > 15000) sys->gamepad_buttons |= BTN_L2; else sys->gamepad_buttons &= ~BTN_L2;
                }
                else if (e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
                    if (e.caxis.value > 15000) sys->gamepad_buttons |= BTN_R2; else sys->gamepad_buttons &= ~BTN_R2;
                }
            }

            // ---- Inputs: Fallback Joystick Generico (R36S nativo) ----
            if (e.type == SDL_JOYBUTTONDOWN || e.type == SDL_JOYBUTTONUP) {
                int down = (e.type == SDL_JOYBUTTONDOWN);
                uint32_t bit = 0;
                switch (e.jbutton.button) {
                    case 0: bit = BTN_B; break; // R36S mapping
                    case 1: bit = BTN_A; break;
                    case 2: bit = BTN_X; break;
                    case 3: bit = BTN_Y; break;
                    case 4: bit = BTN_L1; break;
                    case 5: bit = BTN_R1; break;
                    case 6: bit = BTN_L2; break;
                    case 7: bit = BTN_R2; break;
                    case 8: bit = BTN_SELECT; break;
                    case 9: bit = BTN_START; break;
                    case 10: bit = BTN_L3; break;
                    case 11: bit = BTN_R3; break;
                }
                if (bit) {
                    if (down) sys->gamepad_buttons |= bit;
                    else      sys->gamepad_buttons &= ~bit;
                }
            }
            if (e.type == SDL_JOYAXISMOTION) {
                if (e.jaxis.axis == 0) sys->joystick_lx = e.jaxis.value;
                if (e.jaxis.axis == 1) sys->joystick_ly = e.jaxis.value;
                if (e.jaxis.axis == 2) sys->joystick_rx = e.jaxis.value;
                if (e.jaxis.axis == 3) sys->joystick_ry = e.jaxis.value;
            }
        }

        // Ativa Hard-Quit em caso de segurar Start+Select internamente (se o PortMaster falhar em matar ou o PC quiser sair)
        if ((sys->gamepad_buttons & BTN_START) && (sys->gamepad_buttons & BTN_SELECT)) running = 0;

        m3_CallV(fn_update);

        // Opt 3: skip upload if fb unchanged this frame
        if (sys->fb_dirty) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, fb_tex);
#ifdef PORTMASTER
            // Directly upload from WASM memory without PBOs/glMapBuffer which fail on generic wrappers
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, W, H,
                            GL_LUMINANCE, GL_UNSIGNED_BYTE, fb);
#else
            // Opt 1+5: double PBO async DMA
            cur_pbo = 1 - cur_pbo;
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo[cur_pbo]);
            void* ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
            if (ptr) { memcpy(ptr, fb, fb_size); glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER); }
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, W, H,
                            GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
#endif
            sys->fb_dirty = 0;
        }

        // Opt 2: palette animation — only 1KB upload when palette changes
        if (sys->pal_dirty) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, pal_tex);
#ifdef PORTMASTER
            uint32_t swapped_pal[256];
            for (int i=0; i<256; i++) swapped_pal[i] = __builtin_bswap32(pal[i]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1,
                            GL_RGBA, GL_UNSIGNED_BYTE, swapped_pal);
#else
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1,
                            GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, pal);
#endif
            sys->pal_dirty = 0;
        }

        int dw, dh;
        SDL_GL_GetDrawableSize(window, &dw, &dh);
        glViewport(0, 0, dw, dh);

        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
