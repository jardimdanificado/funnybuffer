#!/usr/bin/env node

/**
 * Wagnostic 2.0 Single-File Node.js SDL2 Host (`wagnostic.js`)
 * 
 * Standalone Host written for Node.js.
 * Requires only @kmamal/sdl for native windowing, rendering, and audio.
 */

const fs = require('fs');
const path = require('path');
const sdl = require('@kmamal/sdl');

// ── Surface Formats & Wagnostic 2.0 Constants ─────────────
const WSURFACE_RGBA8888 = 1;
const WSURFACE_BGRA8888 = 2;
const WSURFACE_RGB565   = 3;
const WSURFACE_RGB888   = 4;

const WUPDATE_OK    =  0;
const WUPDATE_EXIT  =  1;
const WUPDATE_ERROR = -1;

// Gamepad Bitmasks
const GP_A             = 1 << 0;
const GP_B             = 1 << 1;
const GP_X             = 1 << 2;
const GP_Y             = 1 << 3;
const GP_LEFTSHOULDER  = 1 << 4;
const GP_RIGHTSHOULDER = 1 << 5;
const GP_SEL           = 1 << 6;
const GP_START         = 1 << 7;
const GP_LEFTSTICK     = 1 << 8;
const GP_RIGHTSTICK    = 1 << 9;
const GP_UP            = 1 << 10;
const GP_DOWN          = 1 << 11;
const GP_LEFT          = 1 << 12;
const GP_RIGHT         = 1 << 13;

// ── Parse Arguments ──────────────────────────────────────
function parseArgs() {
  const args = process.argv.slice(2);
  let romPath = null;
  let forcedScale = 0;
  let forcedFps = 0;

  for (const arg of args) {
    if (arg.startsWith('--scale=')) {
      forcedScale = parseInt(arg.split('=')[1], 10) || 0;
    } else if (arg.startsWith('--fps=')) {
      forcedFps = parseInt(arg.split('=')[1], 10) || 0;
    } else if (!arg.startsWith('-') && !romPath) {
      romPath = arg;
    }
  }

  if (!romPath) {
    console.log('Wagnostic 2.0 Single-File Node.js SDL2 Host');
    console.log('Usage: node wagnostic.js <path-to-rom.wasm> [--scale=N] [--fps=N]');
    process.exit(1);
  }

  return { romPath, forcedScale, forcedFps };
}

// ── Convert Pixels to RGBA32 ──────────────────────────────
function convertSurfaceToRgba32(format, vramRaw, width, height, stride, outBuffer) {
  const totalPixels = width * height;
  const out32 = new Uint32Array(outBuffer.buffer, outBuffer.byteOffset, totalPixels);

  if (format === WSURFACE_RGBA8888) {
    const in32 = new Uint32Array(vramRaw.buffer, vramRaw.byteOffset, stride * height);
    for (let y = 0; y < height; y++) {
      for (let x = 0; x < width; x++) {
        out32[y * width + x] = in32[y * stride + x];
      }
    }
  } else if (format === WSURFACE_BGRA8888) {
    const in32 = new Uint32Array(vramRaw.buffer, vramRaw.byteOffset, stride * height);
    for (let y = 0; y < height; y++) {
      for (let x = 0; x < width; x++) {
        const px = in32[y * stride + x];
        out32[y * width + x] = (px & 0xFF00FF00) | ((px & 0x00FF0000) >>> 16) | ((px & 0x000000FF) << 16);
      }
    }
  } else if (format === WSURFACE_RGB565) {
    const in16 = new Uint16Array(vramRaw.buffer, vramRaw.byteOffset, stride * height);
    for (let y = 0; y < height; y++) {
      for (let x = 0; x < width; x++) {
        const px = in16[y * stride + x];
        let r = (px >>> 11) & 0x1F; r = (r << 3) | (r >>> 2);
        let g = (px >>> 5)  & 0x3F; g = (g << 2) | (g >>> 4);
        let b = px & 0x1F;        b = (b << 3) | (b >>> 2);
        out32[y * width + x] = (255 << 24) | (b << 16) | (g << 8) | r;
      }
    }
  } else if (format === WSURFACE_RGB888) {
    for (let y = 0; y < height; y++) {
      for (let x = 0; x < width; x++) {
        const idx = (y * stride + x) * 3;
        const r = vramRaw[idx];
        const g = vramRaw[idx + 1];
        const b = vramRaw[idx + 2];
        out32[y * width + x] = (255 << 24) | (b << 16) | (g << 8) | r;
      }
    }
  } else {
    // Default RGBA8888
    const in32 = new Uint32Array(vramRaw.buffer, vramRaw.byteOffset, stride * height);
    for (let y = 0; y < height; y++) {
      for (let x = 0; x < width; x++) {
        out32[y * width + x] = in32[y * stride + x];
      }
    }
  }
}

// ── Main Host Execution ───────────────────────────────────
async function main() {
  const { romPath, forcedScale, forcedFps } = parseArgs();

  const absoluteRomPath = path.resolve(process.cwd(), romPath);
  if (!fs.existsSync(absoluteRomPath)) {
    console.error(`Error: ROM file not found: ${absoluteRomPath}`);
    process.exit(1);
  }

  const wasmBytes = fs.readFileSync(absoluteRomPath);

  let memory = null;
  let arenaOffset = 0;

  function hostAlloc(size, align = 4) {
    if (arenaOffset === 0) {
      arenaOffset = (memory.buffer.byteLength > 1048576) ? 0x20000 : 0x8000;
    }
    if (align > 1) {
      arenaOffset = (arenaOffset + align - 1) & ~(align - 1);
    }
    const ptr = arenaOffset;
    arenaOffset += size;
    return ptr;
  }

  function readWasmString(ptr) {
    if (!ptr || !memory) return '';
    const bytes = new Uint8Array(memory.buffer, ptr);
    let len = 0;
    while (len < 1024 && bytes[len] !== 0) len++;
    return new TextDecoder().decode(bytes.subarray(0, len));
  }

  let surfacePtr = 0;
  let clockPtr = 0;
  let keyboardPtr = 0;
  let mousePtr = 0;
  let gamepadPtr = 0;
  let audioPtr = 0;

  let defaultFbPtr = 0;
  let defaultDirtyPtr = 0;
  let defaultAudioPtr = 0;

  const importObject = {
    env: {
      memory: new WebAssembly.Memory({ initial: 16 }),
      wextension: (namePtr, version) => {
        const name = readWasmString(namePtr);
        if (name === 'std:surface' && version === 1) {
          if (!surfacePtr) {
            surfacePtr = hostAlloc(36, 4);
            defaultFbPtr = hostAlloc(640 * 480 * 4, 4);
            defaultDirtyPtr = hostAlloc(32 * 8, 4);

            const view = new DataView(memory.buffer, surfacePtr, 36);
            view.setUint32(0, 1, true);               // version
            view.setUint32(4, 36, true);              // size
            view.setUint32(8, 320, true);             // width
            view.setUint32(12, 240, true);            // height
            view.setUint32(16, WSURFACE_RGBA8888, true);// format
            view.setUint32(20, 320, true);            // stride
            view.setUint32(24, defaultFbPtr, true);   // pixels
            view.setUint32(28, 0, true);              // dirty_count
            view.setUint32(32, defaultDirtyPtr, true);// dirty_offset
          }
          return surfacePtr;
        }

        if (name === 'std:clock' && version === 1) {
          if (!clockPtr) {
            clockPtr = hostAlloc(32, 8);
            const view = new DataView(memory.buffer, clockPtr, 32);
            view.setUint32(0, 1, true);
            view.setUint32(4, 32, true);
            view.setBigUint64(8, 0n, true);           // ticks
            view.setBigUint64(16, 1000n, true);       // frequency
            view.setFloat32(24, 0.0166667, true);     // delta
          }
          return clockPtr;
        }

        if (name === 'std:keyboard' && version === 1) {
          if (!keyboardPtr) {
            keyboardPtr = hostAlloc(264, 4);
            const view = new DataView(memory.buffer, keyboardPtr, 264);
            view.setUint32(0, 1, true);
            view.setUint32(4, 264, true);
            new Uint8Array(memory.buffer, keyboardPtr + 8, 256).fill(0);
          }
          return keyboardPtr;
        }

        if (name === 'std:mouse' && version === 1) {
          if (!mousePtr) {
            mousePtr = hostAlloc(28, 4);
            const view = new DataView(memory.buffer, mousePtr, 28);
            view.setUint32(0, 1, true);
            view.setUint32(4, 28, true);
            view.setInt32(8, 0, true);                // x
            view.setInt32(12, 0, true);               // y
            view.setUint32(16, 0, true);              // buttons
            view.setInt32(20, 0, true);               // wheel_x
            view.setInt32(24, 0, true);               // wheel_y
          }
          return mousePtr;
        }

        if (name === 'std:gamepad' && version === 1) {
          if (!gamepadPtr) {
            gamepadPtr = hostAlloc(28, 4);
            const view = new DataView(memory.buffer, gamepadPtr, 28);
            view.setUint32(0, 1, true);
            view.setUint32(4, 28, true);
            view.setUint32(8, 0, true);               // buttons
          }
          return gamepadPtr;
        }

        if (name === 'std:audio' && version === 1) {
          if (!audioPtr) {
            audioPtr = hostAlloc(36, 4);
            defaultAudioPtr = hostAlloc(4096 * 2 * 4, 4);
            const view = new DataView(memory.buffer, audioPtr, 36);
            view.setUint32(0, 1, true);
            view.setUint32(4, 36, true);
            view.setUint32(8, 44100, true);           // sample_rate
            view.setUint32(12, 2, true);              // channels
            view.setUint32(16, 1, true);              // format (F32)
            view.setUint32(20, defaultAudioPtr, true);// buffer
            view.setUint32(24, 4096, true);           // capacity
            view.setUint32(28, 0, true);              // write
            view.setUint32(32, 0, true);              // read
          }
          return audioPtr;
        }

        return 0;
      },
      abort: () => console.error('WASM Aborted'),
    },
    wasi_snapshot_preview1: {
      fd_write: () => 0,
      fd_seek: () => 0,
      fd_close: () => 0,
      proc_exit: (code) => process.exit(code),
    }
  };

  let wasmModule;
  try {
    wasmModule = await WebAssembly.instantiate(wasmBytes, importObject);
  } catch (err) {
    try {
      wasmModule = await WebAssembly.instantiate(wasmBytes, {});
    } catch (e2) {
      console.error('Failed to instantiate WebAssembly module:', err.message);
      process.exit(1);
    }
  }

  const instance = wasmModule.instance;
  const exports = instance.exports;

  if (!exports.wupdate) {
    console.error('Error: ROM does not export "wupdate()" function.');
    process.exit(1);
  }

  memory = exports.memory || importObject.env.memory;

  let window = null;
  let currentWidth = 0;
  let currentHeight = 0;
  let currentScale = 1;

  let isRunning = true;
  let conversionBuffer = null;

  const keyBuffer = new Uint8Array(256);
  let mouseX = 0, mouseY = 0;
  let mouseButtonsMask = 0;
  let mouseWheelX = 0, mouseWheelY = 0;
  let gamepadMask = 0;
  const gamepadAxes = new Int16Array(8);

  function exitCleanly() {
    isRunning = false;
    if (window && !window.destroyed) {
      try { window.destroy(); } catch (e) {}
    }
    process.exit(0);
  }

  function updateGamepadKey(scancode, isDown) {
    let bit = 0;
    if (scancode === 0x52) bit = GP_UP;    // ArrowUp
    if (scancode === 0x51) bit = GP_DOWN;  // ArrowDown
    if (scancode === 0x50) bit = GP_LEFT;  // ArrowLeft
    if (scancode === 0x4F) bit = GP_RIGHT; // ArrowRight
    if (scancode === 0x1D) bit = GP_A;     // Z
    if (scancode === 0x1B) bit = GP_B;     // X
    if (scancode === 0x28) bit = GP_START; // Enter
    if (scancode === 0xE1 || scancode === 0xE5) bit = GP_SEL; // Shift

    if (bit) {
      if (isDown) gamepadMask |= bit;
      else gamepadMask &= ~bit;
    }
  }

  let lastFrameTime = process.hrtime.bigint();
  let targetFps = forcedFps || 60;
  let frameIntervalNs = BigInt(Math.floor(1e9 / targetFps));
  let startTime = Date.now();

  function gameLoop() {
    if (!isRunning) return;

    const now = process.hrtime.bigint();
    const elapsed = now - lastFrameTime;

    if (elapsed < frameIntervalNs) {
      setImmediate(gameLoop);
      return;
    }
    const dt = Number(elapsed) / 1e9;
    lastFrameTime = now;

    // Update Extension Buffers
    if (clockPtr && clockPtr + 32 <= memory.buffer.byteLength) {
      const view = new DataView(memory.buffer, clockPtr, 32);
      view.setBigUint64(8, BigInt(Date.now() - startTime), true);
      view.setFloat32(24, dt, true);
    }
    if (keyboardPtr && keyboardPtr + 264 <= memory.buffer.byteLength) {
      new Uint8Array(memory.buffer, keyboardPtr + 8, 256).set(keyBuffer);
    }
    if (mousePtr && mousePtr + 28 <= memory.buffer.byteLength) {
      const view = new DataView(memory.buffer, mousePtr, 28);
      view.setInt32(8, mouseX, true);
      view.setInt32(12, mouseY, true);
      view.setUint32(16, mouseButtonsMask, true);
      view.setInt32(20, mouseWheelX, true);
      view.setInt32(24, mouseWheelY, true);
    }
    if (gamepadPtr && gamepadPtr + 28 <= memory.buffer.byteLength) {
      const view = new DataView(memory.buffer, gamepadPtr, 28);
      view.setUint32(8, gamepadMask, true);
      new Int16Array(memory.buffer, gamepadPtr + 12, 8).set(gamepadAxes);
    }

    let status = WUPDATE_OK;
    try {
      status = exports.wupdate();
    } catch (err) {
      console.error('wupdate() threw an error:', err.message);
      exitCleanly();
      return;
    }

    if (status === WUPDATE_EXIT || status < 0) {
      exitCleanly();
      return;
    }

    // Reset relative wheel delta
    mouseWheelX = 0;
    mouseWheelY = 0;

    // Render Surface if registered
    if (surfacePtr && surfacePtr + 36 <= memory.buffer.byteLength) {
      const sView = new DataView(memory.buffer, surfacePtr, 36);
      const width = sView.getUint32(8, true) || 320;
      const height = sView.getUint32(12, true) || 240;
      const format = sView.getUint32(16, true) || WSURFACE_RGBA8888;
      const stride = sView.getUint32(20, true) || width;
      const pixelsPtr = sView.getUint32(24, true);
      const scale = forcedScale || 1;

      if (!window || currentWidth !== width || currentHeight !== height || currentScale !== scale) {
        if (window && !window.destroyed) {
          window.removeAllListeners('close');
          try { window.destroy(); } catch (e) {}
        }

        currentWidth = width;
        currentHeight = height;
        currentScale = scale;

        try {
          window = sdl.video.createWindow({
            title: 'Wagnostic 2.0 Host (Node.js)',
            width: width * scale,
            height: height * scale,
            resizable: true,
          });
        } catch (err) {
          console.error('Failed to create SDL window:', err.message);
          exitCleanly();
          return;
        }

        window.on('close', () => exitCleanly());

        window.on('keyDown', (e) => {
          const scancode = e.scancode;
          if (scancode >= 0 && scancode < 256) keyBuffer[scancode] = 1;
          updateGamepadKey(scancode, true);
        });

        window.on('keyUp', (e) => {
          const scancode = e.scancode;
          if (scancode >= 0 && scancode < 256) keyBuffer[scancode] = 0;
          updateGamepadKey(scancode, false);
        });

        window.on('mouseMove', (e) => {
          mouseX = Math.floor(e.x / currentScale);
          mouseY = Math.floor(e.y / currentScale);
        });

        window.on('mouseButtonDown', (e) => {
          if (e.button === 1) mouseButtonsMask |= 1;
          if (e.button === 3) mouseButtonsMask |= 2;
          if (e.button === 2) mouseButtonsMask |= 4;
        });

        window.on('mouseButtonUp', (e) => {
          if (e.button === 1) mouseButtonsMask &= ~1;
          if (e.button === 3) mouseButtonsMask &= ~2;
          if (e.button === 2) mouseButtonsMask &= ~4;
        });

        window.on('mouseWheel', (e) => {
          mouseWheelX += (e.dx || 0);
          mouseWheelY += (e.dy || 0);
        });
      }

      if (window && !window.destroyed && isRunning && pixelsPtr > 0) {
        let bppBytes = 4;
        if (format === WSURFACE_RGB565) bppBytes = 2;
        else if (format === WSURFACE_RGB888) bppBytes = 3;

        const vramRaw = new Uint8Array(memory.buffer, pixelsPtr, stride * height * bppBytes);
        const requiredSize = width * height * 4;

        if (!conversionBuffer || conversionBuffer.length !== requiredSize) {
          conversionBuffer = Buffer.alloc(requiredSize);
        }
        convertSurfaceToRgba32(format, vramRaw, width, height, stride, conversionBuffer);

        try {
          window.render(width, height, width * 4, 'rgba32', conversionBuffer);
        } catch (err) {}
      }
    }

    if (isRunning) {
      setImmediate(gameLoop);
    }
  }

  gameLoop();
}

main().catch(err => {
  console.error('Fatal error in Wagnostic Node Host:', err);
  process.exit(0);
});
