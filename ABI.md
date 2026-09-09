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
| `framebuffer` | 1 | Direct 32-bit RGBA8888 framebuffer (`0xAABBGGRR`), dimensions, stride | `framebuffer.h` |
| `clock` | 1 | Monotonic ticks, frequency, and frame delta time | `clock.h` |
| `keyboard` | 1 | 256-byte USB HID scancode state table | `keyboard.h` |
| `mouse` | 1 | Pointer coordinates (X, Y), buttons bitmask, wheel deltas | `mouse.h` |
| `audio` | 1 | Ring buffer audio stream (F32/S16 interleaved channels) | `audio.h` |
| `gif` | 1 | Headless GIF recording synchronization | `gif.h` |
| `logger` | 1 | Simple UTF-8 text message logging to host console | `logger.h` |
| `storage` | 1 | Persistent save-data memory region | `storage.h` |

---

### 4.1 Extension `"framebuffer"` (v1) — Visual Display & Framebuffer
- **Name:** `"framebuffer"` (also accepts legacy aliases `"surface"`, `"std:framebuffer"`, `"std:surface"`)
- **Version:** `1`
- **Total Size:** `24 bytes` | **Alignment:** `4 bytes`
- **Pixel Format:** Strictly 32-bit RGBA8888 (`0xAABBGGRR` in Little-Endian / `[R, G, B, A]` byte order, 4 bytes per pixel).

```c
typedef struct {
    uint32_t version;          /* Offset  0 (4B) - Host - Always 1 */
    uint32_t size;             /* Offset  4 (4B) - Host - Always 24 */
    uint32_t width;            /* Offset  8 (4B) - Host/Guest - Framebuffer width in pixels */
    uint32_t height;           /* Offset 12 (4B) - Host/Guest - Framebuffer height in pixels */
    uint32_t stride;           /* Offset 16 (4B) - Host/Guest - Row stride in pixels (0 = width) */
    uint32_t pixels;           /* Offset 20 (4B) - Host/Guest - WASM pointer to 32-bit RGBA8888 pixel buffer */
} wframebuffer_t;
```

#### Field Offset Table:
| Offset | Size | Type | Field | Written By | Description |
| :---: | :---: | :---: | :--- | :---: | :--- |
| `0` | 4 | `u32` | `version` | Host | Extension version (1) |
| `4` | 4 | `u32` | `size` | Host | Struct size in bytes (24) |
| `8` | 4 | `u32` | `width` | Host/Guest | Framebuffer width in pixels |
| `12` | 4 | `u32` | `height` | Host/Guest | Framebuffer height in pixels |
| `16` | 4 | `u32` | `stride` | Host/Guest | Row stride in pixels (0 = width) |
| `20` | 4 | `u32` | `pixels` | Host/Guest | WASM memory offset to 32-bit RGBA pixel buffer |

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
    uint32_t reserved;         /* Offset 28 (4B) - Host - Padding / alignment (0) */
} wclock_t;
```


#### Field Offset Table:
| Offset | Size | Type | Field | Written By | Description |
| :---: | :---: | :---: | :--- | :---: | :--- |
| `0` | 4 | `u32` | `version` | Host | Extension version (1) |
| `4` | 4 | `u32` | `size` | Host | Struct size in bytes (32) |
| `8` | 8 | `u64` | `ticks` | Host | Total elapsed monotonic ticks |
| `16` | 8 | `u64` | `frequency` | Host | Ticks per second (e.g. 1000) |
| `24` | 4 | `f32` | `delta` | Host | Delta time in seconds |
| `28` | 4 | `u32` | `reserved` | Host | 64-bit alignment padding |

---

### 4.2 Extension `"clock"` (v1) — High-Precision Time & Delta
- **Name:** `"clock"`
- **Version:** `1`
- **Total Size:** `32 bytes` | **Alignment:** `8 bytes`

```c
typedef struct {
    uint32_t version;          /* Offset  0 (4B) - Host - Always 1 */
    uint32_t size;             /* Offset  4 (4B) - Host - Always 32 */
    uint64_t ticks;            /* Offset  8 (8B) - Host - Total monotonic ticks elapsed */
    uint64_t frequency;        /* Offset 16 (8B) - Host - Ticks per second (e.g. 1000 for ms) */
    float    delta;            /* Offset 24 (4B) - Host - Elapsed seconds since last frame */
    uint32_t reserved;         /* Offset 28 (4B) - Host - Padding / alignment (0) */
} wclock_t;
```

---

### 4.3 Extension `"keyboard"` (v1) — USB HID Keyboard Table
- **Name:** `"keyboard"`
- **Version:** `1`
- **Total Size:** `264 bytes` | **Alignment:** `4 bytes`

```c
typedef struct {
    uint32_t version;          /* Offset  0 (4B) - Host - Always 1 */
    uint32_t size;             /* Offset  4 (4B) - Host - Always 264 */
    uint8_t  keys[256];        /* Offset  8 (256B) - Host - Scancode state (0=up, 1=down) */
} wkeyboard_t;
```

---

### 4.4 Extension `"mouse"` (v1) — Cursor, Buttons & Scroll
- **Name:** `"mouse"`
- **Version:** `1`
- **Total Size:** `28 bytes` | **Alignment:** `4 bytes`

```c
#define WMOUSE_BTN_LEFT   (1 << 0)
#define WMOUSE_BTN_RIGHT  (1 << 1)
#define WMOUSE_BTN_MIDDLE (1 << 2)

typedef struct {
    uint32_t version;          /* Offset  0 (4B) - Host - Always 1 */
    uint32_t size;             /* Offset  4 (4B) - Host - Always 28 */
    int32_t  x;                /* Offset  8 (4B) - Host - Cursor X coordinate */
    int32_t  y;                /* Offset 12 (4B) - Host - Cursor Y coordinate */
    uint32_t buttons;          /* Offset 16 (4B) - Host - Bitmask of active buttons */
    int32_t  wheel_x;          /* Offset 20 (4B) - Host - Horizontal scroll delta */
    int32_t  wheel_y;          /* Offset 24 (4B) - Host - Vertical scroll delta */
} wmouse_t;
```

---

### 4.5 Extension `"gif"` (v1) — GIF Recording & Capture Synchronization
- **Name:** `"gif"`
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

1. **Bare Runners (Minimal Hosts to extend freely)**:
   - **`examples/bare_runner.js`**: Zero-dependency bare JavaScript host (~60 lines).
   - **`examples/bare_runner.c`**: Minimal standalone C host using wasm3 (~90 lines).
2. **Full Multimedia Runners**:
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
        fb = (wframebuffer_t*)wextension("framebuffer", 1);
        if (fb) {
            fb->width  = 320;
            fb->height = 240;
            fb->stride = 320;
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


