#ifndef WAGNOSTIC_FRAMEBUFFER_H
#define WAGNOSTIC_FRAMEBUFFER_H

#include <stdint.h>

#define WFRAMEBUFFER_EXTENSION "framebuffer"
#define WFRAMEBUFFER_VERSION   1

// Aliases
#define WSURFACE_EXTENSION "framebuffer"
#define WSURFACE_VERSION   1

typedef struct {
    uint32_t version;       /* 1 */
    uint32_t size;          /* sizeof(wframebuffer_t) = 24 */
    uint32_t width;         /* Framebuffer width in pixels */
    uint32_t height;        /* Framebuffer height in pixels */
    uint32_t stride;        /* Row stride in pixels (0 = width) */
    uint32_t pixels;        /* WASM memory pointer to 32-bit RGBA8888 pixel buffer (4 bytes/pixel: 0xAABBGGRR) */
} wframebuffer_t;

typedef wframebuffer_t wsurface_t;

#endif

