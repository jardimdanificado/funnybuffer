#ifndef WAGNOSTIC_KEYBOARD_H
#define WAGNOSTIC_KEYBOARD_H

#include <stdint.h>

#define WKEYBOARD_EXTENSION "std:keyboard"
#define WKEYBOARD_VERSION   1

typedef struct {
    uint32_t version;
    uint32_t size;

    uint8_t keys[256];
} wkeyboard_t;

#endif /* WAGNOSTIC_KEYBOARD_H */
