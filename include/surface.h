#ifndef WAGNOSTIC_SURFACE_H
#define WAGNOSTIC_SURFACE_H

#include <stdint.h>

#define WSURFACE_EXTENSION "std:surface"
#define WSURFACE_VERSION   1

#define WSURFACE_RGBA8888  1
#define WSURFACE_BGRA8888  2
#define WSURFACE_RGB565    3
#define WSURFACE_RGB888    4

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
} wrect_t;

typedef struct {
    uint32_t version;
    uint32_t size;

    uint32_t width;
    uint32_t height;

    uint32_t format;
    uint32_t stride;

    uint32_t pixels;

    uint32_t dirty_count;
    uint32_t dirty_offset;
} wsurface_t;

#endif /* WAGNOSTIC_SURFACE_H */
