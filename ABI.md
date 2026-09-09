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

## 4. Canonical Extension Specifications

---

### 4.1 Extension `"surface"` (v1) — Visual Display & Framebuffer
- **Name:** `"surface"` (also aliases `"std:surface"`)
- **Version:** `1`
- **Total Size:** `36 bytes` | **Alignment:** `4 bytes`

```c
typedef enum {
    WSURFACE_RGBA8888 = 0,     /* 32-bit RGBA (8-bit per channel) */
    WSURFACE_BGRA8888 = 1,     /* 32-bit BGRA */
    WSURFACE_RGB565   = 2,     /* 16-bit packed RGB (5-6-5) */
    WSURFACE_RGB888   = 3,     /* 24-bit RGB */
} wsurface_format_t;

typedef struct {
    int32_t  x;
    int32_t  y;
    uint32_t w;
    uint32_t h;
} wrect_t;

typedef struct {
    uint32_t version;          /* Offset  0 (4B) - Host - Always 1 */
    uint32_t size;             /* Offset  4 (4B) - Host - Always 36 */
    uint32_t width;            /* Offset  8 (4B) - Host/Guest - Surface width in pixels */
    uint32_t height;           /* Offset 12 (4B) - Host/Guest - Surface height in pixels */
    uint32_t format;           /* Offset 16 (4B) - Host/Guest - wsurface_format_t */
    uint32_t stride;           /* Offset 20 (4B) - Host/Guest - Row stride (0 = width) */
    uint32_t pixels;           /* Offset 24 (4B) - Host/Guest - WASM pointer to pixel buffer */
    uint32_t dirty_count;      /* Offset 28 (4B) - Guest - 0 = full frame; >0 = dirty rects */
    uint32_t dirty_offset;     /* Offset 32 (4B) - Guest - WASM pointer to wrect_t[dirty_count] */
} wsurface_t;
```

#### Field Offset Table:
| Offset | Size | Type | Field | Written By | Description |
| :---: | :---: | :---: | :--- | :---: | :--- |
| `0` | 4 | `u32` | `version` | Host | Extension version (1) |
| `4` | 4 | `u32` | `size` | Host | Struct size in bytes (36) |
| `8` | 4 | `u32` | `width` | Host/Guest | Framebuffer width in pixels |
| `12` | 4 | `u32` | `height` | Host/Guest | Framebuffer height in pixels |
| `16` | 4 | `u32` | `format` | Host/Guest | Pixel format enum |
| `20` | 4 | `u32` | `stride` | Host/Guest | Row stride in pixels (0 = width) |
| `24` | 4 | `u32` | `pixels` | Host/Guest | WASM memory offset to pixel buffer |
| `28` | 4 | `u32` | `dirty_count` | Guest | Number of dirty rectangles |
| `32` | 4 | `u32` | `dirty_offset` | Guest | WASM memory offset to `wrect_t` array |

---

### 4.2 Extension `"clock"` (v1) — High-Precision Time & Delta
- **Name:** `"clock"` (also aliases `"std:clock"`)
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

### 4.3 Extension `"keyboard"` (v1) — USB HID Keyboard Table
- **Name:** `"keyboard"` (also aliases `"std:keyboard"`)
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

### 4.4 Extension `"mouse"` (v1) — Cursor, Buttons & Scroll
- **Name:** `"mouse"` (also aliases `"std:mouse"`)
- **Version:** `1`
- **Total Size:** `28 bytes` | **Alignment:** `4 bytes`

```c
typedef struct {
    uint32_t version;          /* Offset  0 (4B) - Host - Always 1 */
    uint32_t size;             /* Offset  4 (4B) - Host - Always 28 */
    int32_t  x;                /* Offset  8 (4B) - Host - Cursor X coordinate */
    int32_t  y;                /* Offset 12 (4B) - Host - Cursor Y coordinate */
    uint32_t buttons;          /* Offset 16 (4B) - Host - Bitmask: 1=Left, 2=Middle, 4=Right */
    int32_t  wheel_x;          /* Offset 20 (4B) - Host - Horizontal scroll delta */
    int32_t  wheel_y;          /* Offset 24 (4B) - Host - Vertical scroll delta */
} wmouse_t;
```

---

### 4.5 Extension `"gamepad"` (v1) — Digital Buttons & Analog Axes
- **Name:** `"gamepad"` (also aliases `"std:gamepad"`)
- **Version:** `1`
- **Total Size:** `28 bytes` | **Alignment:** `4 bytes`

```c
typedef struct {
    uint32_t version;          /* Offset  0 (4B) - Host - Always 1 */
    uint32_t size;             /* Offset  4 (4B) - Host - Always 28 */
    uint32_t buttons;          /* Offset  8 (4B) - Host - Bitmask of active buttons */
    int16_t  axes[8];          /* Offset 12 (16B) - Host - 8 Analog axes (-32768 to 32767) */
} wgamepad_t;
```

#### Gamepad Button Bits:
| Button | Bit Mask | Button | Bit Mask | Button | Bit Mask |
|---|---|---|---|---|---|
| `A` / Bottom | `1 << 0` | `B` / Right | `1 << 1` | `X` / Left | `1 << 2` |
| `Y` / Top | `1 << 3` | `L-Shoulder` | `1 << 4` | `R-Shoulder` | `1 << 5` |
| `Select` | `1 << 6` | `Start` | `1 << 7` | `L-Stick` | `1 << 8` |
| `R-Stick` | `1 << 9` | `D-Pad Up` | `1 << 10` | `D-Pad Down` | `1 << 11` |
| `D-Pad Left` | `1 << 12` | `D-Pad Right` | `1 << 13` | | |

---

### 4.6 Extension `"audio"` (v1) — PCM Lockless Ring Buffer Streaming
- **Name:** `"audio"` (also aliases `"std:audio"`)
- **Version:** `1`
- **Total Size:** `36 bytes` | **Alignment:** `4 bytes`

```c
typedef enum {
    WAUDIO_F32 = 0,            /* 32-bit float PCM (-1.0 to 1.0) */
    WAUDIO_S16 = 1,            /* 16-bit signed integer PCM (-32768 to 32767) */
} waudio_format_t;

typedef struct {
    uint32_t version;          /* Offset  0 (4B) - Host - Always 1 */
    uint32_t size;             /* Offset  4 (4B) - Host - Always 36 */
    uint32_t sample_rate;      /* Offset  8 (4B) - Host/Guest - Sample rate (e.g. 44100) */
    uint32_t channels;         /* Offset 12 (4B) - Host/Guest - Channel count (1=mono, 2=stereo) */
    uint32_t format;           /* Offset 16 (4B) - Host/Guest - waudio_format_t */
    uint32_t buffer;           /* Offset 20 (4B) - Host/Guest - WASM pointer to PCM ring buffer */
    uint32_t capacity;         /* Offset 24 (4B) - Host/Guest - Capacity in audio frames */
    uint32_t write;            /* Offset 28 (4B) - Guest - Write cursor in frames */
    uint32_t read;             /* Offset 32 (4B) - Host - Read cursor in frames */
} waudio_t;
```

#### Concurrency Protocol:
- **Lockless Streaming:** The guest increments `write` as it generates samples. The host audio thread increments `read` as it sends samples to the sound card.
- **Available Frames to Write:** `capacity - (write - read) - 1`.
- **Available Frames to Read:** `write - read`.

---

### 4.7 Extension `"dispatch"` (v1) — Parallel Workgroups & Compute Tiles (Wash)
- **Name:** `"dispatch"` (also aliases `"std:dispatch"` and `"wash:dispatch"`)
- **Version:** `1`
- **Total Size:** `60 bytes` | **Alignment:** `4 bytes`

```c
typedef struct {
    uint32_t version;          /* Offset  0 (4B) - Host - Always 1 */
    uint32_t size;             /* Offset  4 (4B) - Host - Always 60 */
    uint32_t worker_id;        /* Offset  8 (4B) - Host - Current worker index (0..worker_count-1) */
    uint32_t worker_count;     /* Offset 12 (4B) - Host - Total concurrent workers */
    uint32_t global_offset;    /* Offset 16 (4B) - Host - 1D start index */
    uint32_t global_length;    /* Offset 20 (4B) - Host - 1D element count for this worker */
    uint32_t total_elements;   /* Offset 24 (4B) - Host - 1D total dataset size */
    uint32_t tile_x;           /* Offset 28 (4B) - Host - 2D tile left coordinate */
    uint32_t tile_y;           /* Offset 32 (4B) - Host - 2D tile top coordinate */
    uint32_t tile_w;           /* Offset 36 (4B) - Host - 2D tile width */
    uint32_t tile_h;           /* Offset 40 (4B) - Host - 2D tile height */
    uint32_t full_w;           /* Offset 44 (4B) - Host - Full surface/matrix width */
    uint32_t full_h;           /* Offset 48 (4B) - Host - Full surface/matrix height */
    uint32_t stride;           /* Offset 52 (4B) - Host - Row stride in elements/pixels */
    uint32_t data_ptr;         /* Offset 56 (4B) - Host - WASM pointer to shared buffer */
} wdispatch_t;
```

---

## 5. Implementation Guide for New Hosts

Implementing a complete Wagnostic 2.0 host in any new language (e.g. Rust, Go, Python, Zig, Swift) requires only 3 steps:

### Step 1: Embed a WebAssembly Runtime
Load the target `.wasm` binary using your language's WASM engine (Wasm3, Wasmtime, Wasmer, V8, etc.).

### Step 2: Implement the `env.wextension` Host Import
```
function host_wextension(name: string, version: u32) -> u32 {
    switch (name) {
        case "surface":  return allocate_and_init_surface_struct();
        case "clock":    return allocate_and_init_clock_struct();
        case "keyboard": return allocate_and_init_keyboard_struct();
        case "mouse":    return allocate_and_init_mouse_struct();
        case "gamepad":  return allocate_and_init_gamepad_struct();
        case "audio":    return allocate_and_init_audio_struct();
        case "dispatch": return allocate_and_init_dispatch_struct();
        default:         return 0; // Return NULL for unsupported extensions
    }
}
```

### Step 3: Main Loop Execution
1. Update mapped peripheral structures (`clock.ticks`, `keyboard.keys`, `mouse.x/y`).
2. Call exported `wupdate()`.
3. If `surface.pixels != 0`, present the framebuffer to your platform's display (Window, Canvas, DirectFB, Framebuffer device).
4. If `wupdate()` returns `1` (`WUPDATE_EXIT`), break the loop and exit cleanly.

---

## 6. Minimal Guest ROM Example

```c
#include "wagnostic.h"
#include "surface.h"

static wsurface_t *surface;

int32_t wupdate(void) {
    if (!surface) {
        surface = (wsurface_t*)wextension("surface", 1);
        if (surface) {
            surface->width  = 320;
            surface->height = 240;
            surface->stride = 320;
            surface->format = WSURFACE_RGBA8888;
        }
    }

    if (surface && surface->pixels) {
        uint32_t *fb = (uint32_t*)surface->pixels;
        for (int i = 0; i < 320 * 240; i++) {
            fb[i] = 0xFF0000FF; // Red
        }
    }

    return WUPDATE_OK;
}
```

