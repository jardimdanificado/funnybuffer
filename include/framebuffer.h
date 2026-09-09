#ifndef WAGNOSTIC_FRAMEBUFFER_H
#define WAGNOSTIC_FRAMEBUFFER_H

#include <stdint.h>

#define WFRAMEBUFFER_EXTENSION "std:framebuffer"
#define WFRAMEBUFFER_VERSION   1

// Aliases
#define WSURFACE_EXTENSION "std:framebuffer"
#define WSURFACE_VERSION   1

typedef struct {
    uint32_t version;       /* 1 */
    uint32_t size;          /* sizeof(wframebuffer_t) = 20 */
    uint32_t width;         /* Framebuffer width in pixels */
    uint32_t height;        /* Framebuffer height in pixels */
    uint32_t pixels;        /* WASM memory pointer to 32-bit RGBA8888 pixel buffer (uint32_t[width * height]) */
} wframebuffer_t;

typedef wframebuffer_t wsurface_t;

#endif

