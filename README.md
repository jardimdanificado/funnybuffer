# Funnybuffer Engine

Funnybuffer is a WebAssembly-based game engine designed to run binary cartridges (.wasm). It utilizes the Wasm3 interpreter and an OpenGL 2.1 / GLES 2.0 backend via SDL2.

## Supported Platforms
- **Native Desktop:** Linux, Windows, macOS (OpenGL 2.1).
- **Handheld Devices (R36S / ARM64):** Native execution via PortMaster (OpenGLES 2.0).

---

## Architecture

The engine consists of two primary components:
1. **The Host (host.c):** The native binary responsible for window management, WebAssembly VM instantiation, input handling, and hardware-accelerated rendering.
2. **The Cartridge (game.c):** The application logic, compiled into a standalone WebAssembly module.

Communication between the host and cartridge occurs via a shared memory structure (`SystemConfig`). This structure defines screen resolution, memory offsets for VRAM (Framebuffer) and Palette, and provides a bitmask for input states.

---

## Cartridge Development

Cartridges are WebAssembly modules that must export three lifecycle functions:

```c
// Returns the memory offset of the SystemConfig structure
uint32_t papagaio_system(void);

// Initialization routine called once at startup
void papagaio_init(void);

// Main loop called at 60Hz for logic and rendering
void papagaio_update(void);
```

### Compiling dedicated .wasm binaries

The project includes the `papagaio` compiler driver to generate optimized WASM modules:

```bash
./papagaio/papagaio game.c -o game.wasm
```

---

## Build and Execution

### Desktop Build

To compile the interpreter for development:
```bash
make
```

To execute a cartridge:
```bash
# Usage: ./funnybuffer <cartridge.wasm> [render_scale]
./funnybuffer game.wasm 4
```

### PortMaster Export (R36S / ARM64)

The build system supports cross-compilation for ARM64 using a Docker-based toolchain:

```bash
make portmaster
```

This procedure performs the following:
1. Initializes a cross-compilation environment using an Ubuntu 20.04 container.
2. Links against ARM64-specific libraries (`libGLESv2`).
3. Generates the `funnybuffer/` directory containing the native AArch64 binary and the associated shell script (`funnybuffer.sh`).

### Deployment

To install on a PortMaster-supported device:
1. Copy the `funnybuffer/` directory and `funnybuffer.sh` to the device's `ports/` path.
2. The shell script initializes `gptokeyb` for input mapping and launches the interpreter.
