/*
 * Wagnostic 2.0 Reference Emulator — wasm3 + SDL2 host
 *
 * Implements the Wagnostic 2.0 ABI:
 * - Exports: wupdate() -> int32_t (WUPDATE_OK, WUPDATE_EXIT, WUPDATE_ERROR)
 * - Imports: env.wextension(const char *name, uint32_t version) -> void*
 *
 * Standard Extensions:
 * - std:surface  (v1)
 * - std:clock    (v1)
 * - std:keyboard (v1)
 * - std:mouse    (v1)
 * - std:gamepad  (v1)
 * - std:audio    (v1)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <SDL2/SDL.h>

#include "wasm3.h"
#include "m3_env.h"
#include "m3_api_libc.h"

#include "wagnostic.h"
#include "surface.h"
#include "clock.h"
#include "keyboard.h"
#include "mouse.h"
#include "gamepad.h"
#include "audio.h"
#include "dispatch.h"

/* ================================================================
 * Globals & State
 * ================================================================ */

static IM3Module  g_module  = NULL;
static IM3Runtime g_runtime = NULL;

static uint8_t *g_mem     = NULL;
static uint32_t g_mem_len = 0;

static uint32_t g_surface_ptr  = 0;
static uint32_t g_clock_ptr    = 0;
static uint32_t g_keyboard_ptr = 0;
static uint32_t g_mouse_ptr    = 0;
static uint32_t g_gamepad_ptr  = 0;
static uint32_t g_audio_ptr    = 0;
static uint32_t g_dispatch_ptr = 0;

static uint32_t g_default_fb_ptr    = 0;
static uint32_t g_default_dirty_ptr = 0;
static uint32_t g_default_audio_ptr = 0;

static uint32_t g_arena_offset = 0;

static SDL_Window   *g_window   = NULL;
static SDL_Renderer *g_renderer = NULL;
static SDL_Texture  *g_texture  = NULL;

static SDL_AudioDeviceID g_audio_dev   = 0;
static SDL_mutex        *g_audio_mutex = NULL;

static uint32_t g_prev_w = 0;
static uint32_t g_prev_h = 0;
static uint32_t g_prev_fmt = 0;

static int g_is_tar = 0;
static char g_rom_path[1024] = {0};

/* ================================================================
 * TAR Helpers
 * ================================================================ */

static uint8_t* tar_extract_file(const char* tar_path, const char* target_filename, size_t* out_sz) {
    FILE* f = fopen(tar_path, "rb");
    if (!f) return NULL;
    uint8_t header[512];
    uint8_t* best_data = NULL;
    size_t best_sz = 0;
    while (fread(header, 1, 512, f) == 512) {
        if (header[0] == '\0') break;
        char name[101];
        memcpy(name, header, 100);
        name[100] = '\0';
        size_t size = 0;
        for (int i = 0; i < 11; i++) {
            if (header[124+i] >= '0' && header[124+i] <= '7')
                size = size * 8 + (header[124+i] - '0');
        }
        if (strcmp(name, target_filename) == 0) {
            if (best_data) free(best_data);
            best_data = (uint8_t*)malloc(size);
            best_sz = size;
            fread(best_data, 1, size, f);
            long remainder = (512 - (size % 512)) % 512;
            fseek(f, remainder, SEEK_CUR);
        } else {
            long skip = size + ((512 - (size % 512)) % 512);
            fseek(f, skip, SEEK_CUR);
        }
    }
    fclose(f);
    if (out_sz) *out_sz = best_sz;
    return best_data;
}

/* ================================================================
 * Memory & Arena Helpers
 * ================================================================ */

static void refresh_memory(void) {
    g_mem = m3_GetMemory(g_runtime, &g_mem_len, 0);
}

static uint32_t host_alloc(uint32_t size, uint32_t align) {
    refresh_memory();
    if (g_arena_offset == 0) {
        /* Allocate from safe zone in WASM memory */
        g_arena_offset = (g_mem_len > 1048576) ? 0x20000 : 0x8000;
    }
    if (align > 1) {
        g_arena_offset = (g_arena_offset + align - 1) & ~(align - 1);
    }
    uint32_t ptr = g_arena_offset;
    g_arena_offset += size;
    if (g_arena_offset > g_mem_len && g_runtime) {
        uint32_t pages = (g_arena_offset + 65535) / 65536;
        ResizeMemory(g_runtime, pages);
        refresh_memory();
    }
    return ptr;
}

/* ================================================================
 * Audio Callback
 * ================================================================ */

static void host_audio_callback(void *userdata, Uint8 *stream, int len) {
    (void)userdata;
    if (!g_mem || g_audio_ptr == 0) {
        memset(stream, 0, len);
        return;
    }
    waudio_t *a = (waudio_t*)(g_mem + g_audio_ptr);
    if (!a || a->buffer == 0 || a->capacity == 0) {
        memset(stream, 0, len);
        return;
    }

    uint32_t channels = a->channels ? a->channels : 2;
    uint32_t sample_size = (a->format == WAUDIO_S16) ? sizeof(int16_t) : sizeof(float);
    uint32_t frame_size = channels * sample_size;
    if (frame_size == 0) { memset(stream, 0, len); return; }

    int wanted_frames = len / (int)frame_size;
    uint32_t read_idx = a->read;
    uint32_t write_idx = a->write;
    uint32_t available = (write_idx >= read_idx) ? (write_idx - read_idx) : 0;
    if (available > a->capacity) available = a->capacity;

    int frames_to_copy = (wanted_frames < (int)available) ? wanted_frames : (int)available;
    uint8_t *ring_buf = g_mem + a->buffer;

    for (int i = 0; i < frames_to_copy; i++) {
        uint32_t src_frame_idx = (read_idx + i) % a->capacity;
        memcpy(stream + i * frame_size, ring_buf + src_frame_idx * frame_size, frame_size);
    }
    if (frames_to_copy < wanted_frames) {
        memset(stream + frames_to_copy * frame_size, 0, (wanted_frames - frames_to_copy) * frame_size);
    }
    a->read = read_idx + frames_to_copy;
}

/* ================================================================
 * Extension Dispatcher
 * ================================================================ */

m3ApiRawFunction(host_wextension) {
    m3ApiReturnType(uint32_t);
    m3ApiGetArg(uint32_t, name_ptr);
    m3ApiGetArg(uint32_t, version);

    refresh_memory();
    if (!g_mem || name_ptr >= g_mem_len) {
        m3ApiReturn(0);
    }

    const char* name = (const char*)(g_mem + name_ptr);

    if (strcmp(name, WSURFACE_EXTENSION) == 0 && version == WSURFACE_VERSION) {
        if (g_surface_ptr == 0) {
            g_surface_ptr = host_alloc(sizeof(wsurface_t), 4);
            g_default_fb_ptr = host_alloc(640 * 480 * 4, 4);
            g_default_dirty_ptr = host_alloc(32 * sizeof(wrect_t), 4);

            wsurface_t *s = (wsurface_t*)(g_mem + g_surface_ptr);
            s->version = 1;
            s->size = sizeof(wsurface_t);
            s->width = 320;
            s->height = 240;
            s->format = WSURFACE_RGBA8888;
            s->stride = 320;
            s->pixels = g_default_fb_ptr;
            s->dirty_count = 0;
            s->dirty_offset = g_default_dirty_ptr;
        }
        m3ApiReturn(g_surface_ptr);
    }

    if (strcmp(name, WCLOCK_EXTENSION) == 0 && version == WCLOCK_VERSION) {
        if (g_clock_ptr == 0) {
            g_clock_ptr = host_alloc(sizeof(wclock_t), 8);
            wclock_t *c = (wclock_t*)(g_mem + g_clock_ptr);
            c->version = 1;
            c->size = sizeof(wclock_t);
            c->ticks = (uint64_t)SDL_GetTicks();
            c->frequency = 1000;
            c->delta = 0.0166667f;
        }
        m3ApiReturn(g_clock_ptr);
    }

    if (strcmp(name, WKEYBOARD_EXTENSION) == 0 && version == WKEYBOARD_VERSION) {
        if (g_keyboard_ptr == 0) {
            g_keyboard_ptr = host_alloc(sizeof(wkeyboard_t), 4);
            wkeyboard_t *k = (wkeyboard_t*)(g_mem + g_keyboard_ptr);
            k->version = 1;
            k->size = sizeof(wkeyboard_t);
            memset(k->keys, 0, 256);
        }
        m3ApiReturn(g_keyboard_ptr);
    }

    if (strcmp(name, WMOUSE_EXTENSION) == 0 && version == WMOUSE_VERSION) {
        if (g_mouse_ptr == 0) {
            g_mouse_ptr = host_alloc(sizeof(wmouse_t), 4);
            wmouse_t *m = (wmouse_t*)(g_mem + g_mouse_ptr);
            m->version = 1;
            m->size = sizeof(wmouse_t);
            m->x = 0;
            m->y = 0;
            m->buttons = 0;
            m->wheel_x = 0;
            m->wheel_y = 0;
        }
        m3ApiReturn(g_mouse_ptr);
    }

    if (strcmp(name, WGAMEPAD_EXTENSION) == 0 && version == WGAMEPAD_VERSION) {
        if (g_gamepad_ptr == 0) {
            g_gamepad_ptr = host_alloc(sizeof(wgamepad_t), 4);
            wgamepad_t *gp = (wgamepad_t*)(g_mem + g_gamepad_ptr);
            gp->version = 1;
            gp->size = sizeof(wgamepad_t);
            gp->buttons = 0;
            memset(gp->axes, 0, sizeof(gp->axes));
        }
        m3ApiReturn(g_gamepad_ptr);
    }

    if (strcmp(name, WAUDIO_EXTENSION) == 0 && version == WAUDIO_VERSION) {
        if (g_audio_ptr == 0) {
            g_audio_ptr = host_alloc(sizeof(waudio_t), 4);
            g_default_audio_ptr = host_alloc(4096 * 2 * sizeof(float), 4);
            waudio_t *a = (waudio_t*)(g_mem + g_audio_ptr);
            a->version = 1;
            a->size = sizeof(waudio_t);
            a->sample_rate = 44100;
            a->channels = 2;
            a->format = WAUDIO_F32;
            a->buffer = g_default_audio_ptr;
            a->capacity = 4096;
            a->write = 0;
            a->read = 0;
        }
        m3ApiReturn(g_audio_ptr);
    }

    if ((strcmp(name, WDISPATCH_EXTENSION) == 0 && version == WDISPATCH_VERSION) ||
        (strcmp(name, WASH_DISPATCH_EXTENSION) == 0 && version == WASH_DISPATCH_VERSION)) {
        if (g_dispatch_ptr == 0) {
            g_dispatch_ptr = host_alloc(sizeof(wdispatch_t), 4);
            wdispatch_t *d = (wdispatch_t*)(g_mem + g_dispatch_ptr);
            d->version = 1;
            d->size = sizeof(wdispatch_t);
            d->worker_id = 0;
            d->worker_count = 1;
            d->global_offset = 0;
            d->global_length = 320 * 240;
            d->total_elements = 320 * 240;
            d->tile_x = 0;
            d->tile_y = 0;
            d->tile_w = 320;
            d->tile_h = 240;
            d->full_w = 320;
            d->full_h = 240;
            d->stride = 320;
            d->data_ptr = g_default_fb_ptr;
        }
        m3ApiReturn(g_dispatch_ptr);
    }

    m3ApiReturn(0);
}

/* ================================================================
 * Aspect-ratio-correct letterbox & Mouse coords
 * ================================================================ */

static void calc_letterbox(int win_w, int win_h, uint32_t W, uint32_t H, SDL_Rect *dst) {
    float aspect_rom = (float)W / (float)H;
    float aspect_win = (float)win_w / (float)win_h;
    if (aspect_win > aspect_rom) {
        dst->h = win_h;
        dst->w = (int)(win_h * aspect_rom);
        dst->x = (win_w - dst->w) / 2;
        dst->y = 0;
    } else {
        dst->w = win_w;
        dst->h = (int)(win_w / aspect_rom);
        dst->x = 0;
        dst->y = (win_h - dst->h) / 2;
    }
}

static void convert_mouse_coords(int wx, int wy, int *rx, int *ry,
                                 int win_w, int win_h, uint32_t W, uint32_t H) {
    SDL_Rect dst;
    calc_letterbox(win_w, win_h, W, H, &dst);
    if (dst.w <= 0 || dst.h <= 0) return;
    float scale_x = (float)W / (float)dst.w;
    float scale_y = (float)H / (float)dst.h;
    *rx = (int)((wx - dst.x) * scale_x);
    *ry = (int)((wy - dst.y) * scale_y);
    if (*rx < 0) *rx = 0;
    if (*rx >= (int)W) *rx = (int)W - 1;
    if (*ry < 0) *ry = 0;
    if (*ry >= (int)H) *ry = (int)H - 1;
}

/* ================================================================
 * Surface Presentation
 * ================================================================ */

static void render_surface(wsurface_t *s) {
    if (!s || s->pixels == 0) return;
    refresh_memory();
    if (!g_mem) return;

    uint32_t W = s->width ? s->width : 320;
    uint32_t H = s->height ? s->height : 240;
    uint32_t stride = s->stride ? s->stride : W;
    uint8_t *vram = g_mem + s->pixels;

    Uint32 sdl_fmt = SDL_PIXELFORMAT_ABGR8888;
    int bytes_per_pixel = 4;
    if (s->format == WSURFACE_BGRA8888) {
        sdl_fmt = SDL_PIXELFORMAT_ARGB8888;
        bytes_per_pixel = 4;
    } else if (s->format == WSURFACE_RGB565) {
        sdl_fmt = SDL_PIXELFORMAT_RGB565;
        bytes_per_pixel = 2;
    } else if (s->format == WSURFACE_RGB888) {
        sdl_fmt = SDL_PIXELFORMAT_RGB24;
        bytes_per_pixel = 3;
    } else {
        sdl_fmt = SDL_PIXELFORMAT_ABGR8888;
        bytes_per_pixel = 4;
    }

    if (!g_texture || g_prev_w != W || g_prev_h != H || g_prev_fmt != s->format) {
        if (g_texture) SDL_DestroyTexture(g_texture);
        g_texture = SDL_CreateTexture(g_renderer, sdl_fmt, SDL_TEXTUREACCESS_STREAMING, (int)W, (int)H);
        SDL_SetTextureScaleMode(g_texture, SDL_ScaleModeNearest);
        SDL_SetTextureBlendMode(g_texture, SDL_BLENDMODE_BLEND);
        g_prev_w = W; g_prev_h = H; g_prev_fmt = s->format;
    }

    if (s->dirty_count > 0 && s->dirty_offset != 0 && s->dirty_offset + s->dirty_count * sizeof(wrect_t) <= g_mem_len) {
        wrect_t *rects = (wrect_t*)(g_mem + s->dirty_offset);
        uint32_t count = s->dirty_count;
        if (count > 64) count = 64;
        for (uint32_t i = 0; i < count; i++) {
            int rx = rects[i].x;
            int ry = rects[i].y;
            int rw = rects[i].width;
            int rh = rects[i].height;
            if (rx < 0) { rw += rx; rx = 0; }
            if (ry < 0) { rh += ry; ry = 0; }
            if (rx + rw > (int)W) rw = (int)W - rx;
            if (ry + rh > (int)H) rh = (int)H - ry;
            if (rw <= 0 || rh <= 0) continue;

            SDL_Rect r = { rx, ry, rw, rh };
            void *pixels; int pitch;
            if (SDL_LockTexture(g_texture, &r, &pixels, &pitch) == 0) {
                for (int y = ry; y < ry + rh; y++) {
                    memcpy((uint8_t*)pixels + (y - ry) * pitch,
                           vram + (y * stride + rx) * bytes_per_pixel,
                           rw * bytes_per_pixel);
                }
                SDL_UnlockTexture(g_texture);
            }
        }
        s->dirty_count = 0;
    } else {
        SDL_Rect r = { 0, 0, (int)W, (int)H };
        void *pixels; int pitch;
        if (SDL_LockTexture(g_texture, &r, &pixels, &pitch) == 0) {
            for (int y = 0; y < (int)H; y++) {
                memcpy((uint8_t*)pixels + y * pitch,
                       vram + y * stride * bytes_per_pixel,
                       W * bytes_per_pixel);
            }
            SDL_UnlockTexture(g_texture);
        }
    }

    int win_w, win_h;
    SDL_GetWindowSize(g_window, &win_w, &win_h);
    SDL_Rect dst;
    calc_letterbox(win_w, win_h, W, H, &dst);

    SDL_RenderClear(g_renderer);
    SDL_RenderCopy(g_renderer, g_texture, NULL, &dst);
    SDL_RenderPresent(g_renderer);
}

/* ================================================================
 * main
 * ================================================================ */

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <rom.wasm>\n", argv[0]);
        return 1;
    }

    strncpy(g_rom_path, argv[1], sizeof(g_rom_path)-1);

    /* ---- Load ROM (TAR or Raw WASM) ---- */
    uint8_t *wasm_data = NULL;
    size_t sz = 0;

    wasm_data = tar_extract_file(g_rom_path, "main.wasm", &sz);
    if (wasm_data) {
        g_is_tar = 1;
    } else {
        FILE *f = fopen(argv[1], "rb");
        if (!f) { perror("Failed to open ROM"); return 1; }
        fseek(f, 0, SEEK_END);
        sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        wasm_data = (uint8_t *)malloc(sz);
        if (!wasm_data) { fprintf(stderr, "Out of memory\n"); fclose(f); return 1; }
        fread(wasm_data, 1, sz, f);
        fclose(f);
    }

    /* ---- Initialize wasm3 ---- */
    IM3Environment env = m3_NewEnvironment();
    if (!env) { fprintf(stderr, "m3_NewEnvironment failed\n"); free(wasm_data); return 1; }

    g_runtime = m3_NewRuntime(env, 64 * 1024 * 1024, NULL);
    if (!g_runtime) { fprintf(stderr, "m3_NewRuntime failed\n"); m3_FreeEnvironment(env); free(wasm_data); return 1; }

    M3Result result = m3_ParseModule(env, &g_module, wasm_data, sz);
    if (result) { fprintf(stderr, "Parse error: %s\n", result); m3_FreeRuntime(g_runtime); m3_FreeEnvironment(env); free(wasm_data); return 1; }

    result = m3_LoadModule(g_runtime, g_module);
    if (result) { fprintf(stderr, "Load error: %s\n", result); m3_FreeRuntime(g_runtime); m3_FreeEnvironment(env); free(wasm_data); return 1; }

    m3_LinkRawFunction(g_module, "env", "wextension", "i(ii)", &host_wextension);

    /* ---- Find wupdate ---- */
    IM3Function f_wupdate = NULL;
    result = m3_FindFunction(&f_wupdate, g_runtime, "wupdate");
    if (result || !f_wupdate) {
        fprintf(stderr, "ROM does not export wupdate(): %s\n", result ? result : "not found");
        m3_FreeRuntime(g_runtime); m3_FreeEnvironment(env); free(wasm_data);
        return 1;
    }

    /* ---- Initialize SDL ---- */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        m3_FreeRuntime(g_runtime); m3_FreeEnvironment(env); free(wasm_data);
        return 1;
    }

    uint32_t init_w = 320, init_h = 240;

    g_window = SDL_CreateWindow(
        "Wagnostic 2.0",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        (int)(init_w * 2), (int)(init_h * 2),
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!g_window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit(); m3_FreeRuntime(g_runtime); m3_FreeEnvironment(env); free(wasm_data);
        return 1;
    }

    g_renderer = SDL_CreateRenderer(g_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    if (!g_renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_window); SDL_Quit();
        m3_FreeRuntime(g_runtime); m3_FreeEnvironment(env); free(wasm_data);
        return 1;
    }

    uint8_t keys_state[256];
    memset(keys_state, 0, sizeof(keys_state));

    uint32_t mouse_buttons = 0;
    int mouse_x = 0, mouse_y = 0;
    int mouse_wheel_x = 0, mouse_wheel_y = 0;
    uint32_t gamepad_buttons = 0;
    int16_t gamepad_axes[8] = {0};

    uint64_t last_time = SDL_GetPerformanceCounter();
    double perf_freq = (double)SDL_GetPerformanceFrequency();

    int running = 1;
    while (running) {
        uint64_t current_time = SDL_GetPerformanceCounter();
        float dt = (float)((double)(current_time - last_time) / perf_freq);
        last_time = current_time;

        /* ---- Poll SDL events ---- */
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_QUIT:
                running = 0;
                break;

            case SDL_KEYDOWN:
            case SDL_KEYUP:
                if (ev.key.keysym.scancode < 256) {
                    keys_state[ev.key.keysym.scancode] =
                        (ev.type == SDL_KEYDOWN) ? 1 : 0;
                }
                break;

            case SDL_MOUSEMOTION: {
                int win_w, win_h;
                SDL_GetWindowSize(g_window, &win_w, &win_h);
                uint32_t cur_w = g_prev_w ? g_prev_w : 320;
                uint32_t cur_h = g_prev_h ? g_prev_h : 240;
                convert_mouse_coords(ev.motion.x, ev.motion.y,
                                     &mouse_x, &mouse_y,
                                     win_w, win_h, cur_w, cur_h);
                break;
            }

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP: {
                int pressed = (ev.type == SDL_MOUSEBUTTONDOWN);
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    if (pressed) mouse_buttons |= WMOUSE_BTN_LEFT; else mouse_buttons &= ~WMOUSE_BTN_LEFT;
                } else if (ev.button.button == SDL_BUTTON_RIGHT) {
                    if (pressed) mouse_buttons |= WMOUSE_BTN_RIGHT; else mouse_buttons &= ~WMOUSE_BTN_RIGHT;
                } else if (ev.button.button == SDL_BUTTON_MIDDLE) {
                    if (pressed) mouse_buttons |= WMOUSE_BTN_MIDDLE; else mouse_buttons &= ~WMOUSE_BTN_MIDDLE;
                }
                break;
            }

            case SDL_MOUSEWHEEL:
                mouse_wheel_x += ev.wheel.x;
                mouse_wheel_y += ev.wheel.y;
                break;

            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP: {
                int pressed = (ev.type == SDL_CONTROLLERBUTTONDOWN);
                SDL_GameControllerButton btn = ev.cbutton.button;
                uint32_t mask = 0;
                switch (btn) {
                    case SDL_CONTROLLER_BUTTON_A:      mask = WGAMEPAD_BTN_A; break;
                    case SDL_CONTROLLER_BUTTON_B:      mask = WGAMEPAD_BTN_B; break;
                    case SDL_CONTROLLER_BUTTON_X:      mask = WGAMEPAD_BTN_X; break;
                    case SDL_CONTROLLER_BUTTON_Y:      mask = WGAMEPAD_BTN_Y; break;
                    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  mask = WGAMEPAD_BTN_LEFTSHOULDER; break;
                    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: mask = WGAMEPAD_BTN_RIGHTSHOULDER; break;
                    case SDL_CONTROLLER_BUTTON_BACK:   mask = WGAMEPAD_BTN_SELECT; break;
                    case SDL_CONTROLLER_BUTTON_START:  mask = WGAMEPAD_BTN_START; break;
                    case SDL_CONTROLLER_BUTTON_LEFTSTICK:  mask = WGAMEPAD_BTN_LEFTSTICK; break;
                    case SDL_CONTROLLER_BUTTON_RIGHTSTICK: mask = WGAMEPAD_BTN_RIGHTSTICK; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:    mask = WGAMEPAD_BTN_DPAD_UP; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  mask = WGAMEPAD_BTN_DPAD_DOWN; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  mask = WGAMEPAD_BTN_DPAD_LEFT; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: mask = WGAMEPAD_BTN_DPAD_RIGHT; break;
                    default: break;
                }
                if (mask) {
                    if (pressed) gamepad_buttons |= mask;
                    else         gamepad_buttons &= ~mask;
                }
                break;
            }

            case SDL_CONTROLLERAXISMOTION:
                if (ev.caxis.axis < 8) {
                    gamepad_axes[ev.caxis.axis] = ev.caxis.value;
                }
                break;
            }
        }

        /* ---- Update extensions input / clock before wupdate() ---- */
        refresh_memory();
        if (g_mem) {
            if (g_clock_ptr != 0 && g_clock_ptr + sizeof(wclock_t) <= g_mem_len) {
                wclock_t *c = (wclock_t*)(g_mem + g_clock_ptr);
                c->ticks = (uint64_t)SDL_GetTicks();
                c->frequency = 1000;
                c->delta = dt;
            }
            if (g_keyboard_ptr != 0 && g_keyboard_ptr + sizeof(wkeyboard_t) <= g_mem_len) {
                wkeyboard_t *k = (wkeyboard_t*)(g_mem + g_keyboard_ptr);
                memcpy(k->keys, keys_state, 256);
            }
            if (g_mouse_ptr != 0 && g_mouse_ptr + sizeof(wmouse_t) <= g_mem_len) {
                wmouse_t *m = (wmouse_t*)(g_mem + g_mouse_ptr);
                m->x = mouse_x;
                m->y = mouse_y;
                m->buttons = mouse_buttons;
                m->wheel_x = mouse_wheel_x;
                m->wheel_y = mouse_wheel_y;
            }
            if (g_gamepad_ptr != 0 && g_gamepad_ptr + sizeof(wgamepad_t) <= g_mem_len) {
                wgamepad_t *gp = (wgamepad_t*)(g_mem + g_gamepad_ptr);
                gp->buttons = gamepad_buttons;
                memcpy(gp->axes, gamepad_axes, sizeof(gamepad_axes));
            }
        }

        /* ---- Step: Call wupdate() ---- */
        int32_t status = WUPDATE_OK;
        result = m3_CallV(f_wupdate);
        if (result) {
            fprintf(stderr, "wupdate() runtime error: %s\n", result);
            break;
        }
        m3_GetResultsV(f_wupdate, &status);

        if (status == WUPDATE_EXIT) {
            break;
        }
        if (status < 0) {
            fprintf(stderr, "wupdate() returned error code %d\n", status);
            break;
        }

        /* ---- Audio Check & Initialize device if std:audio active ---- */
        refresh_memory();
        if (g_mem && g_audio_ptr != 0 && g_audio_ptr + sizeof(waudio_t) <= g_mem_len) {
            waudio_t *a = (waudio_t*)(g_mem + g_audio_ptr);
            if (!g_audio_dev && a->sample_rate > 0 && a->channels > 0) {
                SDL_AudioSpec wanted, have;
                SDL_zero(wanted);
                wanted.freq     = a->sample_rate ? a->sample_rate : 44100;
                wanted.format   = (a->format == WAUDIO_S16) ? AUDIO_S16SYS : AUDIO_F32SYS;
                wanted.channels = a->channels ? a->channels : 2;
                wanted.samples  = 512;
                wanted.callback = host_audio_callback;
                g_audio_dev = SDL_OpenAudioDevice(NULL, 0, &wanted, &have, 0);
                if (g_audio_dev) SDL_PauseAudioDevice(g_audio_dev, 0);
            }
        }

        /* ---- Render Surface if std:surface active ---- */
        if (g_mem && g_surface_ptr != 0 && g_surface_ptr + sizeof(wsurface_t) <= g_mem_len) {
            wsurface_t *s = (wsurface_t*)(g_mem + g_surface_ptr);
            render_surface(s);
        }

        /* Reset relative deltas */
        mouse_wheel_x = 0;
        mouse_wheel_y = 0;
        if (g_mem && g_mouse_ptr != 0 && g_mouse_ptr + sizeof(wmouse_t) <= g_mem_len) {
            wmouse_t *m = (wmouse_t*)(g_mem + g_mouse_ptr);
            m->wheel_x = 0;
            m->wheel_y = 0;
        }

        /* FPS limiter (approx 60fps) */
        static uint32_t frame_start = 0;
        uint32_t now = SDL_GetTicks();
        uint32_t elapsed = now - frame_start;
        int32_t delay = (1000 / 60) - (int32_t)elapsed;
        if (delay > 0) SDL_Delay((uint32_t)delay);
        frame_start = SDL_GetTicks();
    }

    /* ================================================================
     * Cleanup
     * ================================================================ */

    if (g_audio_dev) SDL_CloseAudioDevice(g_audio_dev);
    if (g_texture) SDL_DestroyTexture(g_texture);
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window) SDL_DestroyWindow(g_window);
    SDL_Quit();

    m3_FreeRuntime(g_runtime);
    m3_FreeEnvironment(env);
    free(wasm_data);

    return 0;
}
