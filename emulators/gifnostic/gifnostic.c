#include "gifnostic.h"
#include "gif_encoder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "wasm3.h"
#include "m3_env.h"

struct WagnosticContext {
    IM3Environment env;
    IM3Runtime runtime;
    IM3Module module;
    IM3Function f_wupdate;

    uint8_t* wasm_data;
    size_t wasm_size;

    uint8_t* mem;
    uint32_t mem_len;

    uint32_t surface_ptr;
    uint32_t clock_ptr;
    uint32_t keyboard_ptr;
    uint32_t mouse_ptr;
    uint32_t gamepad_ptr;
    uint32_t audio_ptr;

    uint32_t default_fb_ptr;
    uint32_t default_dirty_ptr;
    uint32_t default_audio_ptr;
    uint32_t arena_offset;

    uint8_t keys[256];
    int32_t mouse_x;
    int32_t mouse_y;
    uint32_t mouse_buttons;
    int32_t mouse_wheel_x;
    int32_t mouse_wheel_y;
    uint32_t gamepad_buttons;
    int16_t gamepad_axes[8];
    uint64_t ticks;
    float delta;
    uint64_t frame_count;
};

static void refresh_memory(WagnosticContext* ctx) {
    if (!ctx || !ctx->runtime) return;
    ctx->mem = m3_GetMemory(ctx->runtime, &ctx->mem_len, 0);
}

static uint32_t host_alloc(WagnosticContext* ctx, uint32_t size, uint32_t align) {
    refresh_memory(ctx);
    if (ctx->arena_offset == 0) {
        ctx->arena_offset = (ctx->mem_len > 1048576) ? 0x20000 : 0x8000;
    }
    if (align > 1) {
        ctx->arena_offset = (ctx->arena_offset + align - 1) & ~(align - 1);
    }
    uint32_t ptr = ctx->arena_offset;
    ctx->arena_offset += size;
    if (ctx->arena_offset > ctx->mem_len && ctx->runtime) {
        uint32_t pages = (ctx->arena_offset + 65535) / 65536;
        ResizeMemory(ctx->runtime, pages);
        refresh_memory(ctx);
    }
    return ptr;
}

m3ApiRawFunction(host_wextension) {
    m3ApiReturnType(uint32_t);
    m3ApiGetArg(uint32_t, name_ptr);
    m3ApiGetArg(uint32_t, version);

    WagnosticContext* ctx = (WagnosticContext*)m3_GetUserData(runtime);
    if (!ctx) m3ApiReturn(0);

    refresh_memory(ctx);
    if (!ctx->mem || name_ptr >= ctx->mem_len) m3ApiReturn(0);

    const char* name = (const char*)(ctx->mem + name_ptr);

    if (strcmp(name, WSURFACE_EXTENSION) == 0 && version == WSURFACE_VERSION) {
        if (ctx->surface_ptr == 0) {
            ctx->surface_ptr = host_alloc(ctx, sizeof(wsurface_t), 4);
            ctx->default_fb_ptr = host_alloc(ctx, 640 * 480 * 4, 4);
            ctx->default_dirty_ptr = host_alloc(ctx, 32 * sizeof(wrect_t), 4);

            wsurface_t *s = (wsurface_t*)(ctx->mem + ctx->surface_ptr);
            s->version = 1;
            s->size = sizeof(wsurface_t);
            s->width = 320;
            s->height = 240;
            s->format = WSURFACE_RGBA8888;
            s->stride = 320;
            s->pixels = ctx->default_fb_ptr;
            s->dirty_count = 0;
            s->dirty_offset = ctx->default_dirty_ptr;
        }
        m3ApiReturn(ctx->surface_ptr);
    }

    if (strcmp(name, WCLOCK_EXTENSION) == 0 && version == WCLOCK_VERSION) {
        if (ctx->clock_ptr == 0) {
            ctx->clock_ptr = host_alloc(ctx, sizeof(wclock_t), 8);
            wclock_t *c = (wclock_t*)(ctx->mem + ctx->clock_ptr);
            c->version = 1;
            c->size = sizeof(wclock_t);
            c->ticks = 0;
            c->frequency = 1000;
            c->delta = 0.0166667f;
        }
        m3ApiReturn(ctx->clock_ptr);
    }

    if (strcmp(name, WKEYBOARD_EXTENSION) == 0 && version == WKEYBOARD_VERSION) {
        if (ctx->keyboard_ptr == 0) {
            ctx->keyboard_ptr = host_alloc(ctx, sizeof(wkeyboard_t), 4);
            wkeyboard_t *k = (wkeyboard_t*)(ctx->mem + ctx->keyboard_ptr);
            k->version = 1;
            k->size = sizeof(wkeyboard_t);
            memset(k->keys, 0, 256);
        }
        m3ApiReturn(ctx->keyboard_ptr);
    }

    if (strcmp(name, WMOUSE_EXTENSION) == 0 && version == WMOUSE_VERSION) {
        if (ctx->mouse_ptr == 0) {
            ctx->mouse_ptr = host_alloc(ctx, sizeof(wmouse_t), 4);
            wmouse_t *m = (wmouse_t*)(ctx->mem + ctx->mouse_ptr);
            m->version = 1;
            m->size = sizeof(wmouse_t);
            m->x = 0;
            m->y = 0;
            m->buttons = 0;
            m->wheel_x = 0;
            m->wheel_y = 0;
        }
        m3ApiReturn(ctx->mouse_ptr);
    }

    if (strcmp(name, WGAMEPAD_EXTENSION) == 0 && version == WGAMEPAD_VERSION) {
        if (ctx->gamepad_ptr == 0) {
            ctx->gamepad_ptr = host_alloc(ctx, sizeof(wgamepad_t), 4);
            wgamepad_t *gp = (wgamepad_t*)(ctx->mem + ctx->gamepad_ptr);
            gp->version = 1;
            gp->size = sizeof(wgamepad_t);
            gp->buttons = 0;
            memset(gp->axes, 0, sizeof(gp->axes));
        }
        m3ApiReturn(ctx->gamepad_ptr);
    }

    if (strcmp(name, WAUDIO_EXTENSION) == 0 && version == WAUDIO_VERSION) {
        if (ctx->audio_ptr == 0) {
            ctx->audio_ptr = host_alloc(ctx, sizeof(waudio_t), 4);
            ctx->default_audio_ptr = host_alloc(ctx, 4096 * 2 * sizeof(float), 4);
            waudio_t *a = (waudio_t*)(ctx->mem + ctx->audio_ptr);
            a->version = 1;
            a->size = sizeof(waudio_t);
            a->sample_rate = 44100;
            a->channels = 2;
            a->format = WAUDIO_F32;
            a->buffer = ctx->default_audio_ptr;
            a->capacity = 4096;
            a->write = 0;
            a->read = 0;
        }
        m3ApiReturn(ctx->audio_ptr);
    }

    m3ApiReturn(0);
}

WagnosticContext* wagnostic_create(const uint8_t* wasm_bytes, size_t wasm_size, uint32_t stack_size_bytes) {
    if (!wasm_bytes || wasm_size == 0) return NULL;

    WagnosticContext* ctx = (WagnosticContext*)calloc(1, sizeof(WagnosticContext));
    if (!ctx) return NULL;

    if (stack_size_bytes == 0) {
        stack_size_bytes = 64 * 1024 * 1024;
    }

    ctx->wasm_size = wasm_size;
    ctx->wasm_data = (uint8_t*)malloc(wasm_size);
    if (!ctx->wasm_data) {
        free(ctx);
        return NULL;
    }
    memcpy(ctx->wasm_data, wasm_bytes, wasm_size);

    ctx->env = m3_NewEnvironment();
    if (!ctx->env) {
        wagnostic_destroy(ctx);
        return NULL;
    }

    ctx->runtime = m3_NewRuntime(ctx->env, stack_size_bytes, ctx);
    if (!ctx->runtime) {
        wagnostic_destroy(ctx);
        return NULL;
    }

    M3Result res = m3_ParseModule(ctx->env, &ctx->module, ctx->wasm_data, ctx->wasm_size);
    if (res) {
        fprintf(stderr, "[gifnostic] m3_ParseModule failed: %s\n", res);
        wagnostic_destroy(ctx);
        return NULL;
    }

    res = m3_LoadModule(ctx->runtime, ctx->module);
    if (res) {
        fprintf(stderr, "[gifnostic] m3_LoadModule failed: %s\n", res);
        wagnostic_destroy(ctx);
        return NULL;
    }

    m3_LinkRawFunction(ctx->module, "env", "wextension", "i(ii)", &host_wextension);

    res = m3_FindFunction(&ctx->f_wupdate, ctx->runtime, "wupdate");
    if (res || !ctx->f_wupdate) {
        fprintf(stderr, "[gifnostic] wupdate() export not found in WASM module: %s\n", res ? res : "not found");
        wagnostic_destroy(ctx);
        return NULL;
    }

    refresh_memory(ctx);
    return ctx;
}

WagnosticContext* wagnostic_create_from_file(const char* file_path, uint32_t stack_size_bytes) {
    if (!file_path) return NULL;

    FILE* f = fopen(file_path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    size_t sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t* wasm_data = (uint8_t*)malloc(sz);
    if (!wasm_data) {
        fclose(f);
        return NULL;
    }

    if (fread(wasm_data, 1, sz, f) != sz) {
        free(wasm_data);
        fclose(f);
        return NULL;
    }
    fclose(f);

    WagnosticContext* ctx = wagnostic_create(wasm_data, sz, stack_size_bytes);
    free(wasm_data);
    return ctx;
}

void wagnostic_destroy(WagnosticContext* ctx) {
    if (!ctx) return;
    if (ctx->runtime) m3_FreeRuntime(ctx->runtime);
    if (ctx->env) m3_FreeEnvironment(ctx->env);
    if (ctx->wasm_data) free(ctx->wasm_data);
    free(ctx);
}

int wagnostic_step(WagnosticContext* ctx) {
    if (!ctx || !ctx->f_wupdate) return 0;

    refresh_memory(ctx);
    if (ctx->mem) {
        if (ctx->clock_ptr != 0 && ctx->clock_ptr + sizeof(wclock_t) <= ctx->mem_len) {
            wclock_t* c = (wclock_t*)(ctx->mem + ctx->clock_ptr);
            c->ticks = ctx->ticks;
            c->frequency = 1000;
            c->delta = ctx->delta > 0.0f ? ctx->delta : (1.0f / 60.0f);
        }
        if (ctx->keyboard_ptr != 0 && ctx->keyboard_ptr + sizeof(wkeyboard_t) <= ctx->mem_len) {
            wkeyboard_t* k = (wkeyboard_t*)(ctx->mem + ctx->keyboard_ptr);
            memcpy(k->keys, ctx->keys, 256);
        }
        if (ctx->mouse_ptr != 0 && ctx->mouse_ptr + sizeof(wmouse_t) <= ctx->mem_len) {
            wmouse_t* m = (wmouse_t*)(ctx->mem + ctx->mouse_ptr);
            m->x = ctx->mouse_x;
            m->y = ctx->mouse_y;
            m->buttons = ctx->mouse_buttons;
            m->wheel_x = ctx->mouse_wheel_x;
            m->wheel_y = ctx->mouse_wheel_y;
        }
        if (ctx->gamepad_ptr != 0 && ctx->gamepad_ptr + sizeof(wgamepad_t) <= ctx->mem_len) {
            wgamepad_t* gp = (wgamepad_t*)(ctx->mem + ctx->gamepad_ptr);
            gp->buttons = ctx->gamepad_buttons;
            memcpy(gp->axes, ctx->gamepad_axes, sizeof(ctx->gamepad_axes));
        }
    }

    M3Result call_res = m3_CallV(ctx->f_wupdate);
    if (call_res) {
        fprintf(stderr, "[gifnostic] WASM Execution Error in wupdate(): %s\n", call_res);
        return 0;
    }

    int32_t status = WUPDATE_OK;
    m3_GetResultsV(ctx->f_wupdate, &status);

    if (status == WUPDATE_EXIT) {
        return 0;
    }
    if (status < 0) {
        fprintf(stderr, "[gifnostic] wupdate() returned error %d\n", status);
        return 0;
    }

    refresh_memory(ctx);
    ctx->frame_count++;
    ctx->mouse_wheel_x = 0;
    ctx->mouse_wheel_y = 0;

    return 1;
}

wsurface_t* wagnostic_get_surface(WagnosticContext* ctx) {
    if (!ctx || !ctx->mem || ctx->surface_ptr == 0) return NULL;
    if (ctx->surface_ptr + sizeof(wsurface_t) > ctx->mem_len) return NULL;
    return (wsurface_t*)(ctx->mem + ctx->surface_ptr);
}

wclock_t* wagnostic_get_clock(WagnosticContext* ctx) {
    if (!ctx || !ctx->mem || ctx->clock_ptr == 0) return NULL;
    if (ctx->clock_ptr + sizeof(wclock_t) > ctx->mem_len) return NULL;
    return (wclock_t*)(ctx->mem + ctx->clock_ptr);
}

wkeyboard_t* wagnostic_get_keyboard(WagnosticContext* ctx) {
    if (!ctx || !ctx->mem || ctx->keyboard_ptr == 0) return NULL;
    if (ctx->keyboard_ptr + sizeof(wkeyboard_t) > ctx->mem_len) return NULL;
    return (wkeyboard_t*)(ctx->mem + ctx->keyboard_ptr);
}

wmouse_t* wagnostic_get_mouse(WagnosticContext* ctx) {
    if (!ctx || !ctx->mem || ctx->mouse_ptr == 0) return NULL;
    if (ctx->mouse_ptr + sizeof(wmouse_t) > ctx->mem_len) return NULL;
    return (wmouse_t*)(ctx->mem + ctx->mouse_ptr);
}

wgamepad_t* wagnostic_get_gamepad(WagnosticContext* ctx) {
    if (!ctx || !ctx->mem || ctx->gamepad_ptr == 0) return NULL;
    if (ctx->gamepad_ptr + sizeof(wgamepad_t) > ctx->mem_len) return NULL;
    return (wgamepad_t*)(ctx->mem + ctx->gamepad_ptr);
}

waudio_t* wagnostic_get_audio(WagnosticContext* ctx) {
    if (!ctx || !ctx->mem || ctx->audio_ptr == 0) return NULL;
    if (ctx->audio_ptr + sizeof(waudio_t) > ctx->mem_len) return NULL;
    return (waudio_t*)(ctx->mem + ctx->audio_ptr);
}

uint8_t* wagnostic_get_wasm_memory(WagnosticContext* ctx, uint32_t* out_len) {
    if (!ctx) {
        if (out_len) *out_len = 0;
        return NULL;
    }
    refresh_memory(ctx);
    if (out_len) *out_len = ctx->mem_len;
    return ctx->mem;
}

void wagnostic_set_key(WagnosticContext* ctx, uint8_t scancode, uint8_t is_down) {
    if (!ctx) return;
    ctx->keys[scancode] = is_down ? 1 : 0;
}

void wagnostic_set_mouse(WagnosticContext* ctx, int32_t x, int32_t y, uint32_t buttons, int32_t wheel_x, int32_t wheel_y) {
    if (!ctx) return;
    ctx->mouse_x = x;
    ctx->mouse_y = y;
    ctx->mouse_buttons = buttons;
    ctx->mouse_wheel_x += wheel_x;
    ctx->mouse_wheel_y += wheel_y;
}

void wagnostic_set_gamepad(WagnosticContext* ctx, uint32_t buttons) {
    if (!ctx) return;
    ctx->gamepad_buttons = buttons;
}

void wagnostic_set_ticks(WagnosticContext* ctx, uint64_t ticks_ms, float dt) {
    if (!ctx) return;
    ctx->ticks = ticks_ms;
    ctx->delta = dt;
}

int wagnostic_render_rgb24(WagnosticContext* ctx, uint8_t* out_rgb_buffer, size_t buffer_size) {
    wsurface_t* s = wagnostic_get_surface(ctx);
    if (!s || s->pixels == 0 || !out_rgb_buffer) return 0;
    refresh_memory(ctx);
    if (!ctx->mem) return 0;

    uint32_t W = s->width ? s->width : 320;
    uint32_t H = s->height ? s->height : 240;
    uint32_t stride = s->stride ? s->stride : W;
    uint8_t* vram = ctx->mem + s->pixels;

    size_t required_sz = (size_t)W * H * 3;
    if (buffer_size < required_sz) return 0;

    for (uint32_t y = 0; y < H; y++) {
        for (uint32_t x = 0; x < W; x++) {
            size_t src_idx = y * stride + x;
            uint8_t r = 0, g = 0, b = 0;

            if (s->format == WSURFACE_RGBA8888) {
                uint32_t px = ((uint32_t*)vram)[src_idx];
                r = px & 0xFF;
                g = (px >> 8) & 0xFF;
                b = (px >> 16) & 0xFF;
            } else if (s->format == WSURFACE_BGRA8888) {
                uint32_t px = ((uint32_t*)vram)[src_idx];
                b = px & 0xFF;
                g = (px >> 8) & 0xFF;
                r = (px >> 16) & 0xFF;
            } else if (s->format == WSURFACE_RGB565) {
                uint16_t px = ((uint16_t*)vram)[src_idx];
                r = (px >> 11) & 0x1F; r = (r << 3) | (r >> 2);
                g = (px >> 5)  & 0x3F; g = (g << 2) | (g >> 4);
                b = px & 0x1F;        b = (b << 3) | (b >> 2);
            } else if (s->format == WSURFACE_RGB888) {
                uint8_t* p = vram + src_idx * 3;
                r = p[0]; g = p[1]; b = p[2];
            }

            size_t out_idx = (y * W + x) * 3;
            out_rgb_buffer[out_idx + 0] = r;
            out_rgb_buffer[out_idx + 1] = g;
            out_rgb_buffer[out_idx + 2] = b;
        }
    }
    return 1;
}

int wagnostic_render_rgba32(WagnosticContext* ctx, uint8_t* out_rgba_buffer, size_t buffer_size) {
    wsurface_t* s = wagnostic_get_surface(ctx);
    if (!s || s->pixels == 0 || !out_rgba_buffer) return 0;
    refresh_memory(ctx);
    if (!ctx->mem) return 0;

    uint32_t W = s->width ? s->width : 320;
    uint32_t H = s->height ? s->height : 240;
    uint32_t stride = s->stride ? s->stride : W;
    uint8_t* vram = ctx->mem + s->pixels;

    size_t required_sz = (size_t)W * H * 4;
    if (buffer_size < required_sz) return 0;

    for (uint32_t y = 0; y < H; y++) {
        for (uint32_t x = 0; x < W; x++) {
            size_t src_idx = y * stride + x;
            uint8_t r = 0, g = 0, b = 0, a = 255;

            if (s->format == WSURFACE_RGBA8888) {
                uint32_t px = ((uint32_t*)vram)[src_idx];
                r = px & 0xFF;
                g = (px >> 8) & 0xFF;
                b = (px >> 16) & 0xFF;
                a = (px >> 24) & 0xFF;
            } else if (s->format == WSURFACE_BGRA8888) {
                uint32_t px = ((uint32_t*)vram)[src_idx];
                b = px & 0xFF;
                g = (px >> 8) & 0xFF;
                r = (px >> 16) & 0xFF;
                a = (px >> 24) & 0xFF;
            } else if (s->format == WSURFACE_RGB565) {
                uint16_t px = ((uint16_t*)vram)[src_idx];
                r = (px >> 11) & 0x1F; r = (r << 3) | (r >> 2);
                g = (px >> 5)  & 0x3F; g = (g << 2) | (g >> 4);
                b = px & 0x1F;        b = (b << 3) | (b >> 2);
            } else if (s->format == WSURFACE_RGB888) {
                uint8_t* p = vram + src_idx * 3;
                r = p[0]; g = p[1]; b = p[2];
            }

            size_t out_idx = (y * W + x) * 4;
            out_rgba_buffer[out_idx + 0] = r;
            out_rgba_buffer[out_idx + 1] = g;
            out_rgba_buffer[out_idx + 2] = b;
            out_rgba_buffer[out_idx + 3] = a;
        }
    }
    return 1;
}

int wagnostic_dump_ppm(WagnosticContext* ctx, const char* out_filename) {
    wsurface_t* s = wagnostic_get_surface(ctx);
    if (!s || !out_filename) return 0;

    uint32_t W = s->width ? s->width : 320;
    uint32_t H = s->height ? s->height : 240;

    size_t rgb_sz = (size_t)W * H * 3;
    uint8_t* rgb_buf = (uint8_t*)malloc(rgb_sz);
    if (!rgb_buf) return 0;

    if (!wagnostic_render_rgb24(ctx, rgb_buf, rgb_sz)) {
        free(rgb_buf);
        return 0;
    }

    FILE* f = fopen(out_filename, "wb");
    if (!f) {
        free(rgb_buf);
        return 0;
    }

    fprintf(f, "P6\n%u %u\n255\n", W, H);
    fwrite(rgb_buf, 1, rgb_sz, f);
    fclose(f);
    free(rgb_buf);

    return 1;
}

int wagnostic_record_gif(WagnosticContext* ctx, const char* out_filename, uint32_t total_frames, uint32_t frame_skip, uint16_t delay_cs) {
    wsurface_t* s = wagnostic_get_surface(ctx);
    if (!s || !out_filename) return 0;

    uint32_t W = s->width ? s->width : 320;
    uint32_t H = s->height ? s->height : 240;

    size_t rgb_sz = (size_t)W * H * 3;
    uint8_t* rgb_buf = (uint8_t*)malloc(rgb_sz);
    if (!rgb_buf) return 0;

    int loop_count = (total_frames == 1) ? -1 : 0;
    GIFEncoder* gif = gif_create(out_filename, (uint16_t)W, (uint16_t)H, loop_count);
    if (!gif) {
        free(rgb_buf);
        return 0;
    }

    if (delay_cs == 0) delay_cs = 2;

    uint32_t captured_frames = 0;
    for (uint32_t f = 0; total_frames == 0 || f < total_frames; f++) {
        uint64_t current_ticks = (uint64_t)(f * 1000 / 60);
        wagnostic_set_ticks(ctx, current_ticks, 1.0f / 60.0f);

        if (!wagnostic_step(ctx)) break;

        if (f % (frame_skip + 1) == 0) {
            if (wagnostic_render_rgb24(ctx, rgb_buf, rgb_sz)) {
                gif_add_frame(gif, rgb_buf, delay_cs);
                captured_frames++;
            }
        }
    }

    gif_close(gif);
    free(rgb_buf);

    return (captured_frames > 0) ? 1 : 0;
}

void wagnostic_print_debug(WagnosticContext* ctx, FILE* stream) {
    if (!stream) stream = stdout;
    if (!ctx) {
        fprintf(stream, "[gifnostic] Context: NULL\n");
        return;
    }

    wsurface_t* s = wagnostic_get_surface(ctx);
    fprintf(stream, "=== Gifnostic Headless Debug Info ===\n");
    fprintf(stream, "Frame Count:   %llu\n", (unsigned long long)ctx->frame_count);
    fprintf(stream, "Linear Memory: %u bytes\n", ctx->mem_len);

    if (!s) {
        fprintf(stream, "Surface Extension: NOT REGISTERED\n");
    } else {
        fprintf(stream, "Surface Config: %ux%u (Format: %u, Stride: %u, Pixels: 0x%08X)\n",
                s->width, s->height, s->format, s->stride, s->pixels);
    }
    fprintf(stream, "Extensions: Keyboard=0x%08X Mouse=0x%08X Gamepad=0x%08X Clock=0x%08X Audio=0x%08X\n",
            ctx->keyboard_ptr, ctx->mouse_ptr, ctx->gamepad_ptr, ctx->clock_ptr, ctx->audio_ptr);
    fprintf(stream, "=====================================\n");
}
