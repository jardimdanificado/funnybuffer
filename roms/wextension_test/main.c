#include "wagnostic.h"
#include "surface.h"
#include "clock.h"
#include "keyboard.h"
#include "mouse.h"
#include "gamepad.h"
#include "audio.h"

static wsurface_t  *surface;
static wclock_t    *clock_ext;
static wkeyboard_t *keyboard;
static wmouse_t    *mouse;
static wgamepad_t  *gamepad;
static waudio_t    *audio;

static int initialized = 0;
static int test_passed = 0;

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

int32_t wupdate(void) {
    if (!initialized) {
        // Test 1: Discover extensions
        surface   = (wsurface_t*)wextension("std:surface", 1);
        clock_ext = (wclock_t*)wextension("std:clock", 1);
        keyboard  = (wkeyboard_t*)wextension("std:keyboard", 1);
        mouse     = (wmouse_t*)wextension("std:mouse", 1);
        gamepad   = (wgamepad_t*)wextension("std:gamepad", 1);
        audio     = (waudio_t*)wextension("std:audio", 1);

        // Test 2: Unknown extension returns NULL
        void* unk = wextension("unknown:custom", 1);

        // Test 3: Unsupported version returns NULL
        void* inv_ver = wextension("std:surface", 999);

        test_passed = (surface != NULL) &&
                      (clock_ext != NULL) &&
                      (keyboard != NULL) &&
                      (mouse != NULL) &&
                      (gamepad != NULL) &&
                      (audio != NULL) &&
                      (unk == NULL) &&
                      (inv_ver == NULL) &&
                      (surface->version == 1) &&
                      (clock_ext->version == 1) &&
                      (keyboard->version == 1) &&
                      (mouse->version == 1) &&
                      (gamepad->version == 1) &&
                      (audio->version == 1);

        if (surface) {
            surface->format = WSURFACE_RGB565;
        }

        initialized = 1;
    }

    if (surface && surface->pixels) {
        uint16_t color = test_passed ? rgb565(30, 180, 50) : rgb565(200, 30, 30);
        uint16_t *fb = (uint16_t*)surface->pixels;
        uint32_t w = surface->width ? surface->width : 320;
        uint32_t h = surface->height ? surface->height : 240;
        uint32_t stride = surface->stride ? surface->stride : w;

        for (uint32_t y = 0; y < h; y++) {
            for (uint32_t x = 0; x < w; x++) {
                fb[y * stride + x] = color;
            }
        }
    }

    return WUPDATE_OK;
}
