#ifndef WAGNOSTIC_FRAMEBUFFER_H
#define WAGNOSTIC_FRAMEBUFFER_H

#include <stdint.h>

#define WFRAMEBUFFER_EXTENSION "std:framebuffer"
#define WFRAMEBUFFER_VERSION   1

// Legacy / alias
#define WSURFACE_EXTENSION "std:framebuffer"
#define WSURFACE_VERSION   1

typedef struct {
    int32_t  x;
    int32_t  y;
    uint32_t w;
    uint32_t h;
} wrect_t;

typedef struct {
    uint32_t version;       /* 1 */
    uint32_t size;          /* sizeof(wframebuffer_t) = 32 */
    uint32_t width;         /* Framebuffer width in pixels */
    uint32_t height;        /* Framebuffer height in pixels */
    uint32_t stride;        /* Row stride in pixels (0 = width) */
    uint32_t pixels;        /* WASM memory pointer to 32-bit RGBA8888 pixel buffer (4 bytes/pixel: 0xAABBGGRR) */
    uint32_t dirty_count;   /* 0 = full frame; >0 = count of dirty rectangles */
    uint32_t dirty_offset;  /* WASM memory pointer to wrect_t[dirty_count] */
} wframebuffer_t;

typedef wframebuffer_t wsurface_t;

#endif

