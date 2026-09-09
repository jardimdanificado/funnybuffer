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

## 4. Canonical Standard Extensions

The standard Wagnostic 2.0 core defines 5 canonical extensions:

| Extension Name | Version | Description | Header |
|---|---|---|---|
| `std:framebuffer` | 1 | Visual display buffer, pixel formats, dimensions, dirty rectangles | `framebuffer.h` |
| `std:clock` | 1 | Monotonic ticks, frequency, and frame delta time | `clock.h` |
| `std:keyboard` | 1 | 256-byte USB HID scancode state table | `keyboard.h` |
| `std:mouse` | 1 | Pointer coordinates (X, Y), buttons bitmask, wheel deltas | `mouse.h` |
| `std:gif` | 1 | GIF recording status, frame counts, frame capture synchronization | `gif.h` |

---

### 4.1 Extension `"std:framebuffer"` (v1) — Visual Display & Framebuffer
- **Name:** `"std:framebuffer"` (also accepts legacy alias `"std:surface"`)
- **Version:** `1`
- **Total Size:** `32 bytes` | **Alignment:** `4 bytes`
- **Pixel Format:** Strictly 32-bit RGBA8888 (`0xAABBGGRR` in Little-Endian / `[R, G, B, A]` byte order, 4 bytes per pixel).

```c
typedef struct {
    int32_t  x;
    int32_t  y;
    uint32_t w;
    uint32_t h;
} wrect_t;

typedef struct {
    uint32_t version;          /* Offset  0 (4B) - Host - Always 1 */
    uint32_t size;             /* Offset  4 (4B) - Host - Always 32 */
    uint32_t width;            /* Offset  8 (4B) - Host/Guest - Framebuffer width in pixels */
    uint32_t height;           /* Offset 12 (4B) - Host/Guest - Framebuffer height in pixels */
    uint32_t stride;           /* Offset 16 (4B) - Host/Guest - Row stride in pixels (0 = width) */
    uint32_t pixels;           /* Offset 20 (4B) - Host/Guest - WASM pointer to 32-bit RGBA8888 pixel buffer */
    uint32_t dirty_count;      /* Offset 24 (4B) - Guest - 0 = full frame; >0 = count of dirty rects */
    uint32_t dirty_offset;     /* Offset 28 (4B) - Guest - WASM pointer to wrect_t[dirty_count] */
} wframebuffer_t;
```

#### Field Offset Table:
| Offset | Size | Type | Field | Written By | Description |
| :---: | :---: | :---: | :--- | :---: | :--- |
| `0` | 4 | `u32` | `version` | Host | Extension version (1) |
| `4` | 4 | `u32` | `size` | Host | Struct size in bytes (32) |
| `8` | 4 | `u32` | `width` | Host/Guest | Framebuffer width in pixels |
| `12` | 4 | `u32` | `height` | Host/Guest | Framebuffer height in pixels |
| `16` | 4 | `u32` | `stride` | Host/Guest | Row stride in pixels (0 = width) |
| `20` | 4 | `u32` | `pixels` | Host/Guest | WASM memory offset to 32-bit RGBA pixel buffer |
| `24` | 4 | `u32` | `dirty_count` | Guest | Number of dirty rectangles |
| `28` | 4 | `u32` | `dirty_offset` | Guest | WASM memory offset to `wrect_t` array |

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

### 4.3 Extension `"std:keyboard"` (v1) — USB HID Keyboard Table
- **Name:** `"std:keyboard"` (also accepts `"keyboard"`)
- **Version:** `1`
- **Total Size:** `264 bytes` | **Alignment:** `4 bytes`

```c
typedef struct {
    uint32_t version;          /* Offset  0 (4B) - Host - Always 1 */
    uint32_t size;             /* Offset  4 (4B) - Host - Always 264 */
    uint8_t  keys[256];        /* Offset  8 (256B) - Host - Scancode state (0=up, 1=down) */
} wkeyboard_t;
```

#### Common USB HID Keycodes:
| Key | Scancode | Key | Scancode | Key | Scancode |
|---|---|---|---|---|---|
| `A` - `Z` | `0x04` - `0x1D` | `1` - `9`, `0` | `0x1E` - `0x27` | `Enter` | `0x28` |
| `Escape` | `0x29` | `Space` | `0x2C` | `Right / Left` | `0x4F / 0x50` |
| `Down / Up` | `0x51 / 0x52` | `L-Ctrl / L-Shift` | `0xE0 / 0xE1` | `L-Alt / L-GUI` | `0xE2 / 0xE3` |

---

### 4.4 Extension `"std:mouse"` (v1) — Cursor, Buttons & Scroll
- **Name:** `"std:mouse"` (also accepts `"mouse"`)
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

## 5. Official Runners

Wagnostic maintains **2 official reference runners**:

1. **`native` (`build/wagnostic`)**
   - Implemented in standard C11 using [wasm3](https://github.com/wasm3/wasm3) and SDL2.
   - Supports interactive desktop windowing OR headless execution & animated GIF export via CLI flags (`-g out.gif`, `-n frames`, `--headless`).
2. **`node` (`emulators/node/wagnostic.js`)**
   - Single-file host for Node.js using `@kmamal/sdl` for GUI or native headless execution (`-n frames`, `--headless`).

---

## 6. Minimal Guest ROM Example

```c
#include "wagnostic.h"
#include "framebuffer.h"

static wframebuffer_t *fb;

int32_t wupdate(void) {
    if (!fb) {
        fb = (wframebuffer_t*)wextension("std:framebuffer", 1);
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


