# Wagnostic 2.0 — Formal Architecture & Extension Specification

## 1. Core Architecture & Philosophy

Wagnostic 2.0 is an ultra-minimalist, host-agnostic, language-neutral WebAssembly runtime specification.

The host environment can be implemented in **any programming language** (C, Rust, Go, Zig, JavaScript, Python, Swift, C#) on **any platform** (Desktop, Web, Embedded Microcontrollers, Headless Cloud).

All capabilities are negotiated at runtime via **decoupled, optional extensions** using zero-copy shared linear memory.

---

## 2. Core Binary ABI

Every Wagnostic 2.0 WebAssembly module exports **one execution entry point** and imports **one capability dispatcher**:

### 2.1 Module Export: `wupdate`
```c
int32_t wupdate(void);
```
- **WASM Signature:** `(func (export "wupdate") (result i32))`
- Called by the Host periodically (e.g. at 60 Hz or per compute step).
- **Return Codes:**
  - `0` (`WUPDATE_OK`): Step completed successfully.
  - `1` (`WUPDATE_EXIT`): Clean termination requested by the guest.
  - `<0` (`WUPDATE_ERROR`): Fatal error during guest execution.

### 2.2 Host Import: `wextension`
```c
void* wextension(const char *name, uint32_t version);
```
- **WASM Signature:** `(import "env" "wextension" (func (param i32 i32) (result i32)))`
- Queries the host for a named extension capability at a requested version.
- **Return Value:** 32-bit byte offset into WASM linear memory pointing to the extension structure, or `0` (`NULL`) if unsupported by the Host.

---

## 3. Extension Layout Rules

Every standard extension structure begins with an 8-byte header:
- `uint32_t version`: Extension specification version.
- `uint32_t size`: Size of the extension structure in bytes.

All integers and floats use **32-bit Little-Endian** encoding. All structures are tightly packed with explicit 4-byte or 8-byte alignment.

---

## 4. Built-in Multimedia & Utility Extensions

Wagnostic defines clean, modular extensions for common capabilities. All extensions are completely optional:

| Extension Name | Version | Description | Header |
|---|---|---|---|
| `std:framebuffer` | 1 | Direct 32-bit RGBA8888 framebuffer (`0xAABBGGRR`) and dimensions | `framebuffer.h` |
| `std:clock` | 1 | Monotonic ticks, frequency, and frame delta time | `clock.h` |
| `std:io` | 1 | Unified I/O: Mouse/Pointer (X, Y, buttons, wheel), Gamepad (buttons, 8 axes), and Keyboard (256 scancodes) | `io.h` |
| `std:gif` | 1 | Headless GIF recording synchronization | `gif.h` |
| `logger` | 1 | Simple UTF-8 text message logging to host console | `logger.h` |

---

### 4.1 Extension `"std:framebuffer"` (v1) — Visual Display & Framebuffer
- **Name:** `"std:framebuffer"` (also accepts legacy alias `"framebuffer"`)
- **Version:** `1`
- **Total Size:** `20 bytes` | **Alignment:** `4 bytes`
- **Pixel Format:** Strictly 32-bit RGBA8888 (`0xAABBGGRR` in Little-Endian / `[R, G, B, A]` byte order, 4 bytes per pixel).

```c
typedef struct {
    uint32_t version;          /* Offset  0 (4B) - Host - Always 1 */
    uint32_t size;             /* Offset  4 (4B) - Host - Always 20 */
    uint32_t width;            /* Offset  8 (4B) - Host/Guest - Framebuffer width in pixels */
    uint32_t height;           /* Offset 12 (4B) - Host/Guest - Framebuffer height in pixels */
    uint32_t pixels;           /* Offset 16 (4B) - Host/Guest - WASM pointer to 32-bit RGBA8888 pixel buffer */
} wframebuffer_t;
```

#### Field Offset Table:
| Offset | Size | Type | Field | Written By | Description |
| :---: | :---: | :---: | :--- | :---: | :--- |
| `0` | 4 | `u32` | `version` | Host | Extension version (1) |
| `4` | 4 | `u32` | `size` | Host | Struct size in bytes (20) |
| `8` | 4 | `u32` | `width` | Host/Guest | Framebuffer width in pixels |
| `12` | 4 | `u32` | `height` | Host/Guest | Framebuffer height in pixels |
| `16` | 4 | `u32` | `pixels` | Host/Guest | WASM memory offset to 32-bit RGBA pixel buffer |

---

### 4.2 Extension `"std:clock"` (v1) — High-Precision Time & Delta
- **Name:** `"std:clock"` (also accepts `"clock"`)
- **Version:** `1`
- **Total Size:** `32 bytes` | **Alignment:** `8 bytes`

```c
typedef struct {
    uint32_t version;          /* Offset  0 (4B) - Host - Always 1 */
    uint32_t size;             /* Offset  4 (4B) - Host - Always 32 */
    uint64_t ticks;            /* Offset  8 (8B) - Host - Total monotonic ticks elapsed */
    uint64_t frequency;        /* Offset 16 (8B) - Host - Ticks per second (e.g. 1000 for ms) */
    float    delta;            /* Offset 24 (4B) - Host - Elapsed seconds since last frame */
} wclock_t;
```

---

### 4.3 Extension `"std:io"` (v1) — Unified Input (Mouse, Gamepad, Keyboard)
- **Name:** `"std:io"` (also accepts `"io"`, `"std:keyboard"`, `"std:mouse"`, `"std:gamepad"`)
- **Version:** `1`
- **Total Size:** `304 bytes` | **Alignment:** `4 bytes`

```c
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
    uint32_t version;          /* Offset   0 (4B) - Host - Always 1 */
    uint32_t size;             /* Offset   4 (4B) - Host - Always 304 */

    /* Pointer / Mouse */
    int32_t  mouse_x;          /* Offset   8 (4B) - Host - Cursor X coordinate */
    int32_t  mouse_y;          /* Offset  12 (4B) - Host - Cursor Y coordinate */
    uint32_t mouse_buttons;    /* Offset  16 (4B) - Host - Buttons bitmask (1=L, 2=R, 4=M) */
    int32_t  mouse_wheel_x;    /* Offset  20 (4B) - Host - Horizontal scroll delta */
    int32_t  mouse_wheel_y;    /* Offset  24 (4B) - Host - Vertical scroll delta */

    /* Gamepad */
    uint32_t gamepad_buttons;  /* Offset  28 (4B) - Host - Gamepad buttons bitmask */
    int16_t  gamepad_axes[8];  /* Offset  32 (16B) - Host - 8 analog axes (-32768..32767) */

    /* Keyboard */
    uint8_t  keys[256];        /* Offset  48 (256B) - Host - USB HID scancodes (0=up, 1=down) */
} wio_t;
```

#### Field Offset Table:
| Offset | Size | Type | Field | Written By | Description |
| :---: | :---: | :---: | :--- | :---: | :--- |
| `0` | 4 | `u32` | `version` | Host | Extension version (1) |
| `4` | 4 | `u32` | `size` | Host | Struct size in bytes (304) |
| `8` | 4 | `i32` | `mouse_x` | Host | Mouse/Pointer X coordinate |
| `12` | 4 | `i32` | `mouse_y` | Host | Mouse/Pointer Y coordinate |
| `16` | 4 | `u32` | `mouse_buttons` | Host | Mouse buttons bitmask (bit 0=Left, 1=Right, 2=Middle) |
| `20` | 4 | `i32` | `mouse_wheel_x` | Host | Horizontal scroll delta |
| `24` | 4 | `i32` | `mouse_wheel_y` | Host | Vertical scroll delta |
| `28` | 4 | `u32` | `gamepad_buttons` | Host | Gamepad buttons bitmask (`WGAMEPAD_BTN_*`) |
| `32` | 16 | `i16[8]` | `gamepad_axes` | Host | 8 analog axes (`-32768` to `32767`) |
| `48` | 256 | `u8[256]` | `keys` | Host | USB HID keyboard scancode state table (0=up, 1=down) |

---

### 4.5 Extension `"std:gif"` (v1) — GIF Recording & Capture Synchronization
- **Name:** `"std:gif"` (also accepts `"gif"`)
- **Version:** `1`
- **Total Size:** `28 bytes` | **Alignment:** `4 bytes`

```c
typedef struct {
    uint32_t version;          /* Offset  0 (4B) - Host - Always 1 */
    uint32_t size;             /* Offset  4 (4B) - Host - Always 28 */
    uint32_t recording;        /* Offset  8 (4B) - Host - 1 if host is recording GIF, 0 otherwise */
    uint32_t frame_count;      /* Offset 12 (4B) - Host - Number of frames captured so far */
    uint32_t max_frames;       /* Offset 16 (4B) - Host - Max frames to record (0 = unlimited) */
    uint32_t delay_cs;         /* Offset 20 (4B) - Host - Frame delay in centiseconds (1/100s) */
    uint32_t save_trigger;     /* Offset 24 (4B) - Guest - ROM can set to 1 to signal frame capture */
} wgif_t;
```

---

### 4.6 Extension `"logger"` (v1) — Text Console Logging
- **Name:** `"logger"`
- **Version:** `1`
- **Total Size:** `20 bytes` | **Alignment:** `4 bytes`

```c
typedef struct {
    uint32_t version;          /* Offset  0 (4B) - Host - Always 1 */
    uint32_t size;             /* Offset  4 (4B) - Host - Always 20 */
    uint32_t buffer;           /* Offset  8 (4B) - Host - WASM pointer to text buffer */
    uint32_t capacity;         /* Offset 12 (4B) - Host - Buffer capacity in bytes */
    uint32_t length;           /* Offset 16 (4B) - Guest - Length of text written by ROM */
} wlogger_t;
```

---

## 5. Official Runners & Bare Templates

Wagnostic provides both full-featured runners and bare template hosts:

1. **Bare & Minimal Runners (Zero dependencies, fully customizable)**:
   - **`examples/bare_runner.js`**: Zero-dependency bare JavaScript host (~60 lines).
   - **`examples/bare_runner.c`**: Minimal standalone C host using wasm3 (~90 lines).
   - **`examples/terminal_runner.js`**: Zero-dependency ANSI terminal runner (renders 32-bit RGBA directly in terminal using Unicode half-blocks).
2. **Official Multimedia Runners**:
   - **`native` (`build/wagnostic`)**: C11 + wasm3 + SDL2 for desktop windowing & headless GIF rendering.
   - **`node` (`emulators/node/wagnostic.js`)**: Single-file Node.js host with `@kmamal/sdl`.

---

## 6. Minimal Guest ROM Example

```c
#include "wagnostic.h"
#include "framebuffer.h"

static wframebuffer_t *fb;

int32_t wupdate(void) {
    if (!fb) {
        fb = (wframebuffer_t*)wextension(WFRAMEBUFFER_EXTENSION, WFRAMEBUFFER_VERSION);
        if (fb) {
            fb->width  = 320;
            fb->height = 240;
        }
    }

    if (fb && fb->pixels) {
        uint32_t *pixels = (uint32_t*)fb->pixels;
        for (int i = 0; i < 320 * 240; i++) {
            pixels[i] = 0xFF0000FF; // Red (0xAABBGGRR)
        }
    }

    return WUPDATE_OK;
}
```


