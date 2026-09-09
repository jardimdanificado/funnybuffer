#include "wagnostic.h"
#include "framebuffer.h"
#include "clock.h"
#include "keyboard.h"
#include "mouse.h"
#include "gif.h"

static wframebuffer_t *framebuffer;
static wclock_t       *clock_ext;
static wkeyboard_t    *keyboard;
static wmouse_t       *mouse;
static wgif_t         *gif;

static int initialized = 0;
static int test_passed = 0;

#define RGBA(r, g, b, a) ((uint32_t)(((uint8_t)(a) << 24) | ((uint8_t)(b) << 16) | ((uint8_t)(g) << 8) | (uint8_t)(r)))
#define RGB(r, g, b) RGBA(r, g, b, 255)

int32_t wupdate(void) {
    if (!initialized) {
        // Test 1: Discover standard 5 extensions
        framebuffer = (wframebuffer_t*)wextension("std:framebuffer", 1);
        clock_ext   = (wclock_t*)wextension("std:clock", 1);
        keyboard    = (wkeyboard_t*)wextension("std:keyboard", 1);
        mouse       = (wmouse_t*)wextension("std:mouse", 1);
        gif         = (wgif_t*)wextension("std:gif", 1);

        // Test 2: Unknown extension returns NULL
        void* unk = wextension("unknown:custom", 1);

        // Test 3: Unsupported version returns NULL
        void* inv_ver = wextension("std:framebuffer", 999);

        // Test 4: Legacy alias std:surface also works
        void* surf_alias = wextension("std:surface", 1);

        test_passed = (framebuffer != NULL) &&
                      (clock_ext != NULL) &&
                      (keyboard != NULL) &&
                      (mouse != NULL) &&
                      (gif != NULL) &&
                      (unk == NULL) &&
                      (inv_ver == NULL) &&
                      (surf_alias != NULL) &&
                      (framebuffer->version == 1) &&
                      (clock_ext->version == 1) &&
                      (keyboard->version == 1) &&
                      (mouse->version == 1) &&
                      (gif->version == 1);

        initialized = 1;
    }

    if (framebuffer && framebuffer->pixels) {
        uint32_t color = test_passed ? RGB(30, 180, 50) : RGB(200, 30, 30);
        uint32_t *fb = (uint32_t*)framebuffer->pixels;
        uint32_t w = framebuffer->width ? framebuffer->width : 320;
        uint32_t h = framebuffer->height ? framebuffer->height : 240;
        uint32_t stride = framebuffer->stride ? framebuffer->stride : w;

        for (uint32_t y = 0; y < h; y++) {
            for (uint32_t x = 0; x < w; x++) {
                fb[y * stride + x] = color;
            }
        }
    }

    return test_passed ? WUPDATE_OK : WUPDATE_ERROR;
}

