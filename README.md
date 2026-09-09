# Wagnostic 2.0

Minimalist, modular, platform-agnostic WebAssembly multimedia runtime.

## Quick Start

### 1. Native Runner (C + wasm3 + SDL2)
Interactive Window:
```bash
mkdir -p build && cd build && cmake .. && cmake --build .
./wagnostic ../roms/display_test.wasm
```

Headless Execution & GIF Export:
```bash
./wagnostic -g output.gif -n 60 ../roms/display_test.wasm
```

### 2. Node.js Runner
Interactive or Headless:
```bash
cd emulators/node && npm install
node wagnostic.js ../../roms/display_test.wasm
# Headless run:
node wagnostic.js -n 30 ../../roms/display_test.wasm
```

---

## Architecture Overview

In Wagnostic 2.0, modules export a single lifecycle function `wupdate()` and request capabilities dynamically via named extensions:

```c
#include "wagnostic.h"
#include "framebuffer.h"
#include "clock.h"

static wframebuffer_t *fb;
static wclock_t       *clock_ext;

int32_t wupdate(void) {
    if (!fb) {
        fb        = (wframebuffer_t*)wextension("std:framebuffer", 1);
        clock_ext = (wclock_t*)wextension("std:clock", 1);
        if (fb) {
            fb->width  = 320;
            fb->height = 240;
        }
    }

    if (fb && fb->pixels) {
        uint32_t *pixels = (uint32_t*)fb->pixels;
        // Draw 32-bit RGBA8888 pixels (0xAABBGGRR)...
    }

    return WUPDATE_OK; // 0 = OK, 1 = EXIT, <0 = ERROR
}
```

---

## Modular Extensions

| Extension Name | Version | Description | Header |
|---|---|---|---|
| `std:framebuffer` | 1 | Direct 32-bit RGBA8888 framebuffer (`0xAABBGGRR`) and dimensions | `framebuffer.h` |
| `std:clock` | 1 | Monotonic ticks, frequency, and frame delta time | `clock.h` |
| `std:keyboard` | 1 | 256-byte USB HID scancode state table | `keyboard.h` |
| `std:mouse` | 1 | Pointer coordinates (X, Y), buttons bitmask, wheel deltas (X, Y) | `mouse.h` |
| `std:gif` | 1 | GIF recording status, frame count, delay, and frame capture synchronization | `gif.h` |
| `logger` | 1 | Simple UTF-8 text message logging to host console | `logger.h` |

---

## Runners & Templates

1. **Bare & Minimal Runners (Zero dependencies, fully customizable)**:
   - **`examples/bare_runner.js`**: Pure JavaScript Node.js host (~60 lines) with custom extension support.
   - **`examples/bare_runner.c`**: Pure C host using wasm3 (~90 lines).
   - **`examples/terminal_runner.js`**: ANSI terminal runner rendering 32-bit RGBA directly in terminal using Unicode half-blocks.
2. **Official Multimedia Runners**:
   - **`native` (`build/wagnostic`)**: Standard C11 + wasm3 + SDL2 with windowing and headless GIF export (`-g file.gif`, `-n frames`, `--headless`).
   - **`node` (`emulators/node/wagnostic.js`)**: Single-file host for Node.js using `@kmamal/sdl`.

---

## Running Test Suite

```bash
cd roms
make test-native   # Runs all 15 test ROMs through native runner
make test-node     # Runs all 15 test ROMs through Node.js runner
```

