#ifndef WAGNOSTIC_IO_H
#define WAGNOSTIC_IO_H

#include <stdint.h>

#define WIO_EXTENSION "std:io"

/* Mouse Buttons */
#define WMOUSE_BTN_LEFT   (1 << 0)
#define WMOUSE_BTN_RIGHT  (1 << 1)
#define WMOUSE_BTN_MIDDLE (1 << 2)

/* Gamepad Buttons */
#define WGAMEPAD_BTN_A             (1 << 0)
#define WGAMEPAD_BTN_B             (1 << 1)
#define WGAMEPAD_BTN_X             (1 << 2)
#define WGAMEPAD_BTN_Y             (1 << 3)
#define WGAMEPAD_BTN_LEFTSHOULDER  (1 << 4)
#define WGAMEPAD_BTN_RIGHTSHOULDER (1 << 5)
#define WGAMEPAD_BTN_SELECT        (1 << 6)
#define WGAMEPAD_BTN_START         (1 << 7)
#define WGAMEPAD_BTN_LEFTSTICK     (1 << 8)
#define WGAMEPAD_BTN_RIGHTSTICK    (1 << 9)
#define WGAMEPAD_BTN_DPAD_UP       (1 << 10)
#define WGAMEPAD_BTN_DPAD_DOWN     (1 << 11)
#define WGAMEPAD_BTN_DPAD_LEFT     (1 << 12)
#define WGAMEPAD_BTN_DPAD_RIGHT    (1 << 13)

typedef struct {
    /* Pointer / Mouse */
    int32_t  mouse_x;          /* Offset   0 (4B) - Cursor X */
    int32_t  mouse_y;          /* Offset   4 (4B) - Cursor Y */
    uint32_t mouse_buttons;    /* Offset   8 (4B) - Buttons bitmask (1=L, 2=R, 4=M) */
    int32_t  mouse_wheel_x;    /* Offset  12 (4B) - Horizontal scroll delta */
    int32_t  mouse_wheel_y;    /* Offset  16 (4B) - Vertical scroll delta */

    /* Gamepad */
    uint32_t gamepad_buttons;  /* Offset  20 (4B) - Gamepad buttons bitmask */
    int16_t  gamepad_axes[8];  /* Offset  24 (16B) - 8 analog axes (-32768..32767) */

    /* Keyboard */
    uint8_t  keys[256];        /* Offset  40 (256B) - Scancodes (0=up, 1=down) */
} wio_t;

#endif /* WAGNOSTIC_IO_H */
