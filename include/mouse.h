#ifndef WAGNOSTIC_MOUSE_H
#define WAGNOSTIC_MOUSE_H

#include <stdint.h>

#define WMOUSE_EXTENSION "mouse"
#define WMOUSE_VERSION   1

#define WMOUSE_BTN_LEFT   (1 << 0)
#define WMOUSE_BTN_RIGHT  (1 << 1)
#define WMOUSE_BTN_MIDDLE (1 << 2)

typedef struct {
    uint32_t version;
    uint32_t size;

    int32_t x;
    int32_t y;

    uint32_t buttons;

    int32_t wheel_x;
    int32_t wheel_y;
} wmouse_t;

#endif /* WAGNOSTIC_MOUSE_H */
