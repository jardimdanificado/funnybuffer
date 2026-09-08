# Wagnostic 2.0

Minimalist, modular, platform-agnostic WebAssembly multimedia runtime.

## Quick Start

```bash
mkdir -p build && cd build && cmake .. && cmake --build .
./wagnostic ../roms/audio_test.wasm
```

Headless GIF recording & testing:
```bash
./gifnostic -n 60 -g output.gif ../roms/display_test.wasm
```

Single-file Node.js host:
```bash
cd emulators/node && npm install
node wagnostic.js ../../roms/input_test.wasm
```

Web runner:
```bash
# Open emulators/web/index.html in any modern browser
```

---

## Architecture Overview

In Wagnostic 2.0, ROMs export a single lifecycle function `wupdate()` and request capabilities via standard extensions (`std:*`):

```c
#include "wagnostic.h"
#include "surface.h"
#include "clock.h"

static wsurface_t *surface;
static wclock_t   *clock_ext;

int32_t wupdate(void) {
    if (!surface) {
        surface   = (wsurface_t*)wextension("std:surface", 1);
        clock_ext = (wclock_t*)wextension("std:clock", 1);
        if (surface) {
            surface->width  = 320;
            surface->height = 240;
            surface->stride = 320;
            surface->format = WSURFACE_RGBA8888;
        }
    }

    if (surface && surface->pixels) {
        uint32_t *fb = (uint32_t*)surface->pixels;
        // Draw frame...
    }

    return WUPDATE_OK; // 0 = OK, 1 = EXIT, <0 = ERROR
}
```

---

## Standard Extensions (`std:*`)

| Extension | Version | Description | Header |
|---|---|---|---|
| `std:surface` | 1 | Display framebuffer, formats (RGBA8888, BGRA8888, RGB565, RGB888), dirty rects | `surface.h` |
| `std:clock` | 1 | High-precision ticks, frequency, and frame delta time | `clock.h` |
| `std:keyboard` | 1 | 256 USB HID scancode state table | `keyboard.h` |
| `std:mouse` | 1 | Pointer coordinates (X, Y), buttons bitmask, wheel deltas (X, Y) | `mouse.h` |
| `std:gamepad` | 1 | Digital gamepad buttons bitmask, 8 analog axes | `gamepad.h` |
| `std:audio` | 1 | PCM ring buffer streaming (F32, S16), multi-channel | `audio.h` |

---

## Directory Structure

- `include/`: Standard C headers for Wagnostic 2.0 (`wagnostic.h`, `surface.h`, `clock.h`, `keyboard.h`, `mouse.h`, `gamepad.h`, `audio.h`).
- `emulators/wasm3/`: Native SDL2 C host powered by WASM3.
- `emulators/gifnostic/`: Headless CLI host with GIF exporter.
- `emulators/node/`: Single-file Node.js SDL2 host.
- `emulators/web/`: Web host runner with HTML5 Canvas, Web Audio, Gamepad API, and full TAR bundle support.
- `roms/`: Test ROMs and demos (e.g. `audio_test`, `display_test`, `input_test`, `buttons_test`, `mquickjs_wagner`, `wextension_test`).
- `ABI.md`: Full specification for the Wagnostic 2.0 ABI.
