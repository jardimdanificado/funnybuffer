# Wagnostic 2.0 ABI Specification

## 1. Core Architecture

Wagnostic 2.0 is an ultra-minimalist, modular, platform-agnostic WebAssembly multimedia runtime.

Unlike monolithic runtime designs, Wagnostic 2.0 decouples state and features into **independent, versioned extensions**.

### 1.1 Core Interface

A Wagnostic 2.0 ROM is a WebAssembly binary that exports **one function** and optionally imports **one dispatcher**:

- **Export:**
  ```c
  int32_t wupdate(void);
  ```
  Called by the host once per frame / tick.
  - Returns `0` (`WUPDATE_OK`): Frame processed successfully.
  - Returns `1` (`WUPDATE_EXIT`): ROM requests a clean shutdown.
  - Returns `<0` (`WUPDATE_ERROR`): Fatal error in ROM execution.

- **Import:**
  ```c
  void* wextension(const char *name, uint32_t version);
  ```
  WASM signature: `(import "env" "wextension" (func (param i32 i32) (result i32)))`.
  Queries the host for a named extension interface at a requested version. Returns a pointer (`i32` offset into WASM linear memory) to the extension struct, or `NULL` (`0`) if unsupported.

---

## 2. Standard Extensions (`std:*`)

Every standard extension struct begins with two 32-bit fields for forward/backward compatibility:
- `uint32_t version`: Extension protocol version.
- `uint32_t size`: Size of the struct in bytes.

```c
typedef struct {
    uint32_t version;
    uint32_t size;
    // ... extension-specific fields ...
} wextension_header_t;
```

---

### 2.1 `std:surface` — Graphics & Framebuffer

- **Name:** `"std:surface"`
- **Version:** `1`

```c
#define WSURFACE_EXTENSION "std:surface"
#define WSURFACE_VERSION   1

typedef enum {
    WSURFACE_RGBA8888 = 0,
    WSURFACE_BGRA8888 = 1,
    WSURFACE_RGB565   = 2,
    WSURFACE_RGB888   = 3,
} wsurface_format_t;

typedef struct {
    int32_t x;
    int32_t y;
    uint32_t w;
    uint32_t h;
} wrect_t;

typedef struct {
    uint32_t version;       /* 1 */
    uint32_t size;          /* sizeof(wsurface_t) = 36 */
    uint32_t width;         /* Surface width in pixels */
    uint32_t height;        /* Surface height in pixels */
    uint32_t format;        /* wsurface_format_t */
    uint32_t stride;        /* Row stride in pixels (0 = width) */
    uint32_t pixels;        /* WASM memory pointer to pixel buffer */
    uint32_t dirty_count;   /* Number of dirty rectangles (0 = full frame) */
    uint32_t dirty_offset;  /* WASM memory pointer to wrect_t[dirty_count] */
} wsurface_t;
```

---

### 2.2 `std:clock` — Timing & Delta

- **Name:** `"std:clock"`
- **Version:** `1`

```c
#define WCLOCK_EXTENSION "std:clock"
#define WCLOCK_VERSION   1

typedef struct {
    uint32_t version;       /* 1 */
    uint32_t size;          /* sizeof(wclock_t) = 32 */
    uint64_t ticks;         /* Total elapsed ticks */
    uint64_t frequency;     /* Ticks per second (e.g. 1000 for ms, 1000000 for µs) */
    float    delta;         /* Time elapsed since last frame in seconds */
    uint32_t reserved;      /* Padding / alignment */
} wclock_t;
```

---

### 2.3 `std:keyboard` — USB HID Keyboard

- **Name:** `"std:keyboard"`
- **Version:** `1`

```c
#define WKEYBOARD_EXTENSION "std:keyboard"
#define WKEYBOARD_VERSION   1

typedef struct {
    uint32_t version;       /* 1 */
    uint32_t size;          /* sizeof(wkeyboard_t) = 264 */
    uint8_t  keys[256];     /* USB HID scancode state table (0 = released, 1 = pressed) */
} wkeyboard_t;
```

---

### 2.4 `std:mouse` — Mouse & Cursor

- **Name:** `"std:mouse"`
- **Version:** `1`

```c
#define WMOUSE_EXTENSION "std:mouse"
#define WMOUSE_VERSION   1

typedef struct {
    uint32_t version;       /* 1 */
    uint32_t size;          /* sizeof(wmouse_t) = 28 */
    int32_t  x;             /* Pointer X coordinate relative to surface */
    int32_t  y;             /* Pointer Y coordinate relative to surface */
    uint32_t buttons;       /* Bitmask: bit 0=Left, bit 1=Middle, bit 2=Right */
    int32_t  wheel_x;       /* Horizontal scroll delta */
    int32_t  wheel_y;       /* Vertical scroll delta */
} wmouse_t;
```

---

### 2.5 `std:gamepad` — Game Controller

- **Name:** `"std:gamepad"`
- **Version:** `1`

```c
#define WGAMEPAD_EXTENSION "std:gamepad"
#define WGAMEPAD_VERSION   1

typedef struct {
    uint32_t version;       /* 1 */
    uint32_t size;          /* sizeof(wgamepad_t) = 28 */
    uint32_t buttons;       /* Bitmask of pressed buttons */
    int16_t  axes[8];       /* Normalized analog axes (-32768 to 32767) */
} wgamepad_t;
```

#### Gamepad Button Bits:
| Button | Bit Mask | Description |
|---|---|---|
| `GP_A` | `1 << 0` | Action Bottom |
| `GP_B` | `1 << 1` | Action Right |
| `GP_X` | `1 << 2` | Action Left |
| `GP_Y` | `1 << 3` | Action Top |
| `GP_LEFTSHOULDER` | `1 << 4` | Left Bumper / L1 |
| `GP_RIGHTSHOULDER` | `1 << 5` | Right Bumper / R1 |
| `GP_SEL` | `1 << 6` | Select / Back |
| `GP_START` | `1 << 7` | Start / Options |
| `GP_LEFTSTICK` | `1 << 8` | Left Stick Press |
| `GP_RIGHTSTICK` | `1 << 9` | Right Stick Press |
| `GP_UP` | `1 << 10` | D-Pad Up |
| `GP_DOWN` | `1 << 11` | D-Pad Down |
| `GP_LEFT` | `1 << 12` | D-Pad Left |
| `GP_RIGHT` | `1 << 13` | D-Pad Right |

---

### 2.6 `std:audio` — PCM Audio Streaming

- **Name:** `"std:audio"`
- **Version:** `1`

```c
#define WAUDIO_EXTENSION "std:audio"
#define WAUDIO_VERSION   1

typedef enum {
    WAUDIO_F32 = 0,         /* 32-bit floating point PCM (-1.0 to 1.0) */
    WAUDIO_S16 = 1,         /* 16-bit signed integer PCM (-32768 to 32767) */
} waudio_format_t;

typedef struct {
    uint32_t version;       /* 1 */
    uint32_t size;          /* sizeof(waudio_t) = 36 */
    uint32_t sample_rate;   /* Sampling frequency (e.g. 44100, 48000) */
    uint32_t channels;      /* Channel count (1 = mono, 2 = stereo) */
    uint32_t format;        /* waudio_format_t */
    uint32_t buffer;        /* WASM memory pointer to ring buffer */
    uint32_t capacity;      /* Ring buffer capacity in frames */
    uint32_t write;         /* Write index in frames (updated by ROM) */
    uint32_t read;          /* Read index in frames (updated by Host) */
} waudio_t;
```

---

## 3. Minimal ROM Example

```c
#include "wagnostic.h"
#include "surface.h"

static wsurface_t *surface;

int32_t wupdate(void) {
    if (!surface) {
        surface = (wsurface_t*)wextension("std:surface", 1);
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
