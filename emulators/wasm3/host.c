/*
 * Wagnostic 2.0 Reference Native Emulator — wasm3 + SDL2 / Headless GIF host
 *
 * Implements the Wagnostic 2.0 ABI:
 * - Exports: wupdate() -> int32_t (WUPDATE_OK, WUPDATE_EXIT, WUPDATE_ERROR)
 * - Imports: env.wextension(const char *name, uint32_t version) -> void*
 *
 * Standard Extensions:
 * - std:framebuffer (v1)
 * - std:clock       (v1)
 * - std:keyboard    (v1)
 * - std:mouse       (v1)
 * - std:gif         (v1)
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
#include "framebuffer.h"
#include "clock.h"
#include "io.h"
#include "gif.h"
#include "gif_encoder.h"

/* ================================================================
 * Globals & State
 * ================================================================ */

static IM3Module  g_module  = NULL;
static IM3Runtime g_runtime = NULL;

static uint8_t *g_mem     = NULL;
static uint32_t g_mem_len = 0;

static uint32_t g_fb_ptr       = 0;
static uint32_t g_clock_ptr    = 0;
static uint32_t g_io_ptr       = 0;
static uint32_t g_gif_ptr      = 0;

static uint32_t g_default_fb_ptr = 0;

static uint32_t g_arena_offset = 0;

static SDL_Window   *g_window   = NULL;
static SDL_Renderer *g_renderer = NULL;
static SDL_Texture  *g_texture  = NULL;

static uint32_t g_prev_w = 0;
static uint32_t g_prev_h = 0;
static uint32_t g_prev_fmt = 0;

static const char *g_gif_path = NULL;
static GIFEncoder *g_gif_encoder = NULL;
static uint8_t    *g_gif_rgb_buf = NULL;
static size_t      g_gif_rgb_buf_sz = 0;
static uint64_t    g_max_frames = 0;
static uint32_t    g_frame_skip = 0;
static uint16_t    g_delay_cs = 2;
static uint32_t    g_target_fps = 60;
static uint32_t    g_captured_frames = 0;
static int         g_headless = 0;
static int         g_benchmark = 0;

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
 * Extension Dispatcher
 * ================================================================ */

m3ApiRawFunction(host_wextension) {
    m3ApiReturnType(uint32_t);
    m3ApiGetArg(uint32_t, name_ptr);
    m3ApiGetArg(uint32_t, version);

    refresh_memory();
    if (!g_mem || name_ptr >= g_mem_len) m3ApiReturn(0);

    const char *name = (const char*)(g_mem + name_ptr);

    /* 1. Framebuffer: framebuffer (and legacy alias surface) */
    if ((strcmp(name, WFRAMEBUFFER_EXTENSION) == 0 || strcmp(name, "surface") == 0 ||
         strcmp(name, "std:framebuffer") == 0 || strcmp(name, "std:surface") == 0) && version == WFRAMEBUFFER_VERSION) {
        if (g_fb_ptr == 0) {
            g_fb_ptr = host_alloc(sizeof(wframebuffer_t), 4);
            g_default_fb_ptr = host_alloc(640 * 480 * 4, 4);
            wframebuffer_t *fb = (wframebuffer_t*)(g_mem + g_fb_ptr);
            fb->version = 1;
            fb->size = sizeof(wframebuffer_t);
            fb->width = 320;
            fb->height = 240;
            fb->pixels = g_default_fb_ptr;
        }
        m3ApiReturn(g_fb_ptr);
    }

    /* 2. Clock: clock */
    if ((strcmp(name, WCLOCK_EXTENSION) == 0 || strcmp(name, "clock") == 0 ||
         strcmp(name, "std:clock") == 0) && version == WCLOCK_VERSION) {
        if (g_clock_ptr == 0) {
            g_clock_ptr = host_alloc(sizeof(wclock_t), 8);
            wclock_t *clk = (wclock_t*)(g_mem + g_clock_ptr);
            clk->version = 1;
            clk->size = sizeof(wclock_t);
            clk->ticks = 0;
            clk->frequency = 1000;
            clk->delta = 0.0166667f;
        }
        m3ApiReturn(g_clock_ptr);
    }

    /* 3. Unified I/O (Keyboard, Mouse, Gamepad): std:io */
    if ((strcmp(name, WIO_EXTENSION) == 0 || strcmp(name, "io") == 0 ||
         strcmp(name, "std:io") == 0 || strcmp(name, "std:keyboard") == 0 ||
         strcmp(name, "std:mouse") == 0 || strcmp(name, "std:gamepad") == 0 ||
         strcmp(name, "keyboard") == 0 || strcmp(name, "mouse") == 0 ||
         strcmp(name, "gamepad") == 0) && version == WIO_VERSION) {
        if (g_io_ptr == 0) {
            g_io_ptr = host_alloc(sizeof(wio_t), 4);
            wio_t *io = (wio_t*)(g_mem + g_io_ptr);
            io->version = 1;
            io->size = sizeof(wio_t);
            io->mouse_x = 0;
            io->mouse_y = 0;
            io->mouse_buttons = 0;
            io->mouse_wheel_x = 0;
            io->mouse_wheel_y = 0;
            io->gamepad_buttons = 0;
            memset(io->gamepad_axes, 0, sizeof(io->gamepad_axes));
            memset(io->keys, 0, sizeof(io->keys));
        }
        m3ApiReturn(g_io_ptr);
    }

    /* 4. GIF Recording Extension: std:gif */
    if ((strcmp(name, WGIF_EXTENSION) == 0 || strcmp(name, "gif") == 0 ||
         strcmp(name, "std:gif") == 0) && version == WGIF_VERSION) {
        if (g_gif_ptr == 0) {
            g_gif_ptr = host_alloc(sizeof(wgif_t), 4);
            wgif_t *g = (wgif_t*)(g_mem + g_gif_ptr);
            g->version = 1;
            g->size = sizeof(wgif_t);
            g->recording = (g_gif_path != NULL) ? 1 : 0;
            g->frame_count = 0;
            g->max_frames = (uint32_t)g_max_frames;
            g->delay_cs = g_delay_cs;
            g->save_trigger = 0;
        }
        m3ApiReturn(g_gif_ptr);
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
 * Surface Presentation & GIF Conversion
 * ================================================================ */

static int render_to_rgb24(wframebuffer_t *fb, uint8_t *out_rgb) {
    if (!fb || fb->pixels == 0 || !out_rgb) return 0;
    refresh_memory();
    if (!g_mem) return 0;

    uint32_t W = fb->width ? fb->width : 320;
    uint32_t H = fb->height ? fb->height : 240;
    uint32_t *vram = (uint32_t*)(g_mem + fb->pixels);

    for (uint32_t y = 0; y < H; y++) {
        for (uint32_t x = 0; x < W; x++) {
            uint32_t px = vram[y * W + x];
            uint8_t r = px & 0xFF;
            uint8_t g = (px >> 8) & 0xFF;
            uint8_t b = (px >> 16) & 0xFF;

            size_t out_idx = (y * W + x) * 3;
            out_rgb[out_idx + 0] = r;
            out_rgb[out_idx + 1] = g;
            out_rgb[out_idx + 2] = b;
        }
    }
    return 1;
}

static void capture_gif_frame(wframebuffer_t *fb) {
    if (!g_gif_path || !fb || fb->pixels == 0) return;
    uint32_t W = fb->width ? fb->width : 320;
    uint32_t H = fb->height ? fb->height : 240;

    if (!g_gif_encoder) {
        int loop_count = (g_max_frames == 1) ? -1 : 0;
        g_gif_encoder = gif_create(g_gif_path, (uint16_t)W, (uint16_t)H, loop_count);
        if (!g_gif_encoder) {
            fprintf(stderr, "Failed to create GIF file '%s'\n", g_gif_path);
            return;
        }
    }

    size_t req_sz = (size_t)W * H * 3;
    if (g_gif_rgb_buf_sz < req_sz) {
        g_gif_rgb_buf = (uint8_t*)realloc(g_gif_rgb_buf, req_sz);
        g_gif_rgb_buf_sz = req_sz;
    }

    if (render_to_rgb24(fb, g_gif_rgb_buf)) {
        gif_add_frame(g_gif_encoder, g_gif_rgb_buf, g_delay_cs);
        g_captured_frames++;
        if (g_gif_ptr && g_gif_ptr + sizeof(wgif_t) <= g_mem_len) {
            wgif_t *g = (wgif_t*)(g_mem + g_gif_ptr);
            g->frame_count = g_captured_frames;
        }
    }
}

static void render_surface(wframebuffer_t *s) {
    if (!s || s->pixels == 0) return;
    refresh_memory();
    if (!g_mem) return;

    uint32_t W = s->width ? s->width : 320;
    uint32_t H = s->height ? s->height : 240;
    uint8_t *vram = g_mem + s->pixels;

    if (!g_texture || g_prev_w != W || g_prev_h != H) {
        if (g_texture) SDL_DestroyTexture(g_texture);
        g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, (int)W, (int)H);
        SDL_SetTextureScaleMode(g_texture, SDL_ScaleModeNearest);
        SDL_SetTextureBlendMode(g_texture, SDL_BLENDMODE_BLEND);
        g_prev_w = W; g_prev_h = H;
    }

    SDL_UpdateTexture(g_texture, NULL, vram, W * 4);


    int win_w, win_h;
    SDL_GetWindowSize(g_window, &win_w, &win_h);
    SDL_Rect dst;
    calc_letterbox(win_w, win_h, W, H, &dst);

    SDL_RenderClear(g_renderer);
    SDL_RenderCopy(g_renderer, g_texture, NULL, &dst);
    SDL_RenderPresent(g_renderer);
}

/* ================================================================
 * Time Helpers
 * ================================================================ */

static double get_time_sec(void) {
#if defined(_WIN32)
    static LARGE_INTEGER freq = {0};
    LARGE_INTEGER count;
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
#endif
}

static void print_usage(const char* prog) {
    printf("Wagnostic 2.0 Native Runner (wasm3 + SDL2)\n\n");
    printf("Usage: %s [options] <rom.wasm>\n\n", prog);
    printf("Options:\n");
    printf("  -n <frames>       Run for N frames (0 = run indefinitely, default: 0)\n");
    printf("  -g, --gif <file>  Record output to animated GIF\n");
    printf("  --delay <cs>      GIF frame delay in centiseconds (default: 2 = 20ms)\n");
    printf("  --fps <fps>       Target frame rate (default: 60)\n");
    printf("  --skip <count>    Skip N frames between recorded GIF frames\n");
    printf("  --headless        Run headless without SDL2 window\n");
    printf("  -b, --benchmark   Benchmark execution FPS\n");
    printf("  -h, --help        Show this help message\n");
}

/* ================================================================
 * main
 * ================================================================ */

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    int force_gui = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            g_max_frames = strtoull(argv[++i], NULL, 10);
        } else if ((strcmp(argv[i], "-g") == 0 || strcmp(argv[i], "--gif") == 0) && i + 1 < argc) {
            g_gif_path = argv[++i];
        } else if (strcmp(argv[i], "--delay") == 0 && i + 1 < argc) {
            g_delay_cs = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--fps") == 0 && i + 1 < argc) {
            int fps = atoi(argv[++i]);
            if (fps > 0) {
                g_target_fps = (uint32_t)fps;
                g_delay_cs = (uint16_t)(100 / fps);
                if (g_delay_cs == 0) g_delay_cs = 1;
            }
        } else if (strcmp(argv[i], "--skip") == 0 && i + 1 < argc) {
            g_frame_skip = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--headless") == 0) {
            g_headless = 1;
        } else if (strcmp(argv[i], "--gui") == 0) {
            force_gui = 1;
        } else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--benchmark") == 0) {
            g_benchmark = 1;
        } else if (argv[i][0] != '-') {
            strncpy(g_rom_path, argv[i], sizeof(g_rom_path) - 1);
        }
    }

    if (g_rom_path[0] == '\0') {
        fprintf(stderr, "Error: No ROM file specified.\n");
        print_usage(argv[0]);
        return 1;
    }

    /* Auto-detect headless mode */
    if (!force_gui) {
        if (g_gif_path != NULL || g_max_frames > 0) {
            g_headless = 1;
        }
#if !defined(_WIN32)
        if (!getenv("DISPLAY") && !getenv("WAYLAND_DISPLAY")) {
            g_headless = 1;
        }
#endif
    }

    /* ---- Load ROM (TAR or Raw WASM) ---- */
    uint8_t *wasm_data = NULL;
    size_t sz = 0;

    wasm_data = tar_extract_file(g_rom_path, "main.wasm", &sz);
    if (wasm_data) {
        g_is_tar = 1;
    } else {
        FILE *f = fopen(g_rom_path, "rb");
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

    double start_time = get_time_sec();
    uint64_t frames_run = 0;

    /* ================================================================
     * Headless Mode
     * ================================================================ */
    if (g_headless) {
        while (g_max_frames == 0 || frames_run < g_max_frames) {
            refresh_memory();
            if (g_mem) {
                if (g_clock_ptr != 0 && g_clock_ptr + sizeof(wclock_t) <= g_mem_len) {
                    wclock_t *c = (wclock_t*)(g_mem + g_clock_ptr);
                    c->ticks = (uint64_t)(frames_run * 1000 / g_target_fps);
                    c->frequency = 1000;
                    c->delta = 1.0f / (float)g_target_fps;
                }
                if (g_gif_ptr != 0 && g_gif_ptr + sizeof(wgif_t) <= g_mem_len) {
                    wgif_t *g = (wgif_t*)(g_mem + g_gif_ptr);
                    g->recording = (g_gif_path != NULL) ? 1 : 0;
                    g->max_frames = (uint32_t)g_max_frames;
                    g->delay_cs = g_delay_cs;
                }
            }

            int32_t status = WUPDATE_OK;
            result = m3_CallV(f_wupdate);
            if (result) {
                fprintf(stderr, "wupdate() runtime error at frame %llu: %s\n", (unsigned long long)frames_run, result);
                break;
            }
            m3_GetResultsV(f_wupdate, &status);

            if (status == WUPDATE_EXIT) {
                break;
            }
            if (status < 0) {
                fprintf(stderr, "wupdate() returned error code %d at frame %llu\n", status, (unsigned long long)frames_run);
                break;
            }

            if (g_gif_path && (frames_run % (g_frame_skip + 1) == 0)) {
                refresh_memory();
                if (g_mem && g_fb_ptr != 0 && g_fb_ptr + sizeof(wframebuffer_t) <= g_mem_len) {
                    wframebuffer_t *fb = (wframebuffer_t*)(g_mem + g_fb_ptr);
                    capture_gif_frame(fb);
                }
            }

            frames_run++;
        }

        if (g_gif_encoder) {
            gif_close(g_gif_encoder);
            g_gif_encoder = NULL;
        }

        double elapsed = get_time_sec() - start_time;
        if (g_benchmark) {
            double fps = (elapsed > 0.0) ? ((double)frames_run / elapsed) : 0.0;
            printf("=== Native Runner Benchmark ===\n");
            printf("ROM:            %s\n", g_rom_path);
            printf("Frames Executed: %llu\n", (unsigned long long)frames_run);
            printf("Total Time:     %.4f seconds\n", elapsed);
            printf("Execution Rate: %.2f FPS\n", fps);
            printf("===============================\n");
        }

        m3_FreeRuntime(g_runtime);
        m3_FreeEnvironment(env);
        free(wasm_data);
        if (g_gif_rgb_buf) free(g_gif_rgb_buf);
        return 0;
    }

    /* ================================================================
     * GUI Mode (SDL2)
     * ================================================================ */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
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
    while (running && (g_max_frames == 0 || frames_run < g_max_frames)) {
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
            if (g_io_ptr != 0 && g_io_ptr + sizeof(wio_t) <= g_mem_len) {
                wio_t *io = (wio_t*)(g_mem + g_io_ptr);
                io->mouse_x = mouse_x;
                io->mouse_y = mouse_y;
                io->mouse_buttons = mouse_buttons;
                io->mouse_wheel_x = mouse_wheel_x;
                io->mouse_wheel_y = mouse_wheel_y;
                io->gamepad_buttons = gamepad_buttons;
                memcpy(io->gamepad_axes, gamepad_axes, sizeof(gamepad_axes));
                memcpy(io->keys, keys_state, 256);
            }
            if (g_gif_ptr != 0 && g_gif_ptr + sizeof(wgif_t) <= g_mem_len) {
                wgif_t *g = (wgif_t*)(g_mem + g_gif_ptr);
                g->recording = (g_gif_path != NULL) ? 1 : 0;
                g->max_frames = (uint32_t)g_max_frames;
                g->delay_cs = g_delay_cs;
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

        /* ---- Render Framebuffer if framebuffer active ---- */
        if (g_mem && g_fb_ptr != 0 && g_fb_ptr + sizeof(wframebuffer_t) <= g_mem_len) {
            wframebuffer_t *fb = (wframebuffer_t*)(g_mem + g_fb_ptr);
            render_surface(fb);
            if (g_gif_path && (frames_run % (g_frame_skip + 1) == 0)) {
                capture_gif_frame(fb);
            }
        }

        /* Reset relative deltas */
        mouse_wheel_x = 0;
        mouse_wheel_y = 0;
        if (g_mem && g_io_ptr != 0 && g_io_ptr + sizeof(wio_t) <= g_mem_len) {
            wio_t *io = (wio_t*)(g_mem + g_io_ptr);
            io->mouse_wheel_x = 0;
            io->mouse_wheel_y = 0;
        }

        frames_run++;

        /* FPS limiter */
        static uint32_t frame_start = 0;
        uint32_t now = SDL_GetTicks();
        uint32_t elapsed_ms = now - frame_start;
        int32_t delay = (1000 / (int32_t)g_target_fps) - (int32_t)elapsed_ms;
        if (delay > 0) SDL_Delay((uint32_t)delay);
        frame_start = SDL_GetTicks();
    }

    if (g_gif_encoder) {
        gif_close(g_gif_encoder);
        g_gif_encoder = NULL;
    }

    /* ================================================================
     * Cleanup
     * ================================================================ */

    if (g_texture) SDL_DestroyTexture(g_texture);
    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window) SDL_DestroyWindow(g_window);
    SDL_Quit();

    m3_FreeRuntime(g_runtime);
    m3_FreeEnvironment(env);
    free(wasm_data);
    if (g_gif_rgb_buf) free(g_gif_rgb_buf);

    return 0;
}
