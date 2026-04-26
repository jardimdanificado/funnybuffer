# Wagnostic

**Wagnostic** is a minimalist real-time framebuffer and input communication protocol based on shared memory. It defines a standard interface between a **Host** (executor) and a **Cartridge** (WebAssembly) for 16-bit graphical rendering and peripheral handling.

## Characteristics
- **Agnostic**: The protocol does not dictate how the application should be written, only how it communicates.
- **Efficient**: Uses a Shared Memory Model for zero-copy inputs and direct access to the video buffer.
- **Portable**: Host implementations available for Desktop (C/SDL2), Handhelds (PortMaster), and Web (JavaScript).

---

## Architecture

The protocol is based on two primary components:
1. **The Host:** Responsible for window management, capturing hardware inputs, and instantiating the WebAssembly VM.
2. **The Cartridge:** A `.wasm` module that implements the logic and writes directly to the framebuffer memory.

### Memory Map
The cartridge access hardware state through a shared memory structure located at the beginning of the WASM memory (offset `0`):

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0      | 128  | `title` | Window Title (UTF-8 Null-terminated) |
| 128    | 4    | `width` | Screen width in pixels |
| 132    | 4    | `height`| Screen height in pixels |
| 136    | 4    | `bpp`   | Bits Per Pixel: 1 (8bpp), 2 (16bpp), 4 (32bpp) |
| 140    | 4    | `scale` | Rendering scale (for host window size) |
| 144    | 4    | `audio_size` | Audio Ring Buffer size in bytes |
| 148    | 4    | `audio_write`| Cartridge write pointer (byte offset) |
| 152    | 4    | `audio_read` | Host read pointer (byte offset) |
| 156    | 4    | `audio_rate` | Sample rate (e.g. 44100) |
| 160    | 4    | `audio_bpp`  | Bits per sample: 1 (8-bit), 2 (16-bit), 4 (32-bit float) |
| 164    | 4    | `redraw`| Set to 1 by cartridge to request a frame redraw |
| 168    | 4    | `gamepad_buttons` | Bitmask of active buttons |
| 172    | 16   | `joysticks` | 4x int32 for L/R sticks axes |
| 188    | 256  | `keys` | Array of 256 bytes for keyboard scancodes (1=down) |
| 444    | 4    | `mouse_x` | Mouse X coordinate |
| 448    | 4    | `mouse_y` | Mouse Y coordinate |
| 452    | 4    | `mouse_buttons` | Bitmask for mouse buttons (L, R, M...) |
| 456    | 4    | `mouse_wheel` | Mouse wheel vertical scroll |
| 460    | 52   | `reserved` | Reserved for future protocol expansion |
| **512**| -    | **VRAM** | **Framebuffer (Size: `width * height * bpp`)** |
| `512 + vram_size` | - | **Audio** | **Audio Ring Buffer (Size: `audio_size`)** |
| `512 + vram_size + audio_size` | - | **RAM** | **General purpose cartridge RAM (Heap)** |

### Supported Pixel Formats
- **8**: RGB332 (3 bits Red, 3 bits Green, 2 bits Blue).
- **16**: RGB565 (5 bits Red, 6 bits Green, 5 bits Blue). **[Default]**
- **32**: RGBA8888 (8 bits per channel, Alpha is ignored by host).

### Supported Audio Formats
- **1 (8-bit)**: Unsigned 8-bit PCM.
- **2 (16-bit)**: Signed 16-bit PCM (Little-endian). **[Default]**
- **4 (32-bit)**: 32-bit Float PCM (Little-endian).

---

## Cartridge Development

Cartridges must be compiled to WebAssembly (target `wasm32-unknown-unknown`) and export at least one of these functions:

```c
// Called every frame (60Hz)
void game_frame(void);

// OR
int main(void);
```

### Host Imports (env)
The host provides these functions to the cartridge:
- `void init(const char* title, int w, int h, int bpp, int scale, int audio_size, int audio_rate, int audio_bpp)`: Sets the window title, resolution, bpp, scale and audio allocation.
- `uint32_t get_ticks()`: Returns the number of milliseconds since the engine started.

### Input Constants
```c
#define BTN_UP     (1 << 0)
#define BTN_DOWN   (1 << 1)
#define BTN_LEFT   (1 << 2)
#define BTN_RIGHT  (1 << 3)
#define BTN_A      (1 << 4)
#define BTN_B      (1 << 5)
#define BTN_START  (1 << 10)
#define BTN_SELECT (1 << 11)
```

---

## Build and Execution

### Desktop Build
```bash
make
```

### Execution
```bash
# Usage: ./wagnostic <cartridge.wasm> [render_scale]
./wagnostic game.wasm 4
```

### PortMaster Export (ARM64)
```bash
make portmaster
```
This generates the `wagnostic/` directory and `wagnostic.sh` for deployment on handheld devices like the R36S.
