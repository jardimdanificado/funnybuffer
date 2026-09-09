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

// ── Constants ─────────────────────────────────────────────
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
  let maxFrames = 0;
  let headless = false;

  for (let i = 0; i < args.length; i++) {
    const arg = args[i];
    if (arg.startsWith('--scale=')) {
      forcedScale = parseInt(arg.split('=')[1], 10) || 0;
    } else if (arg.startsWith('--fps=')) {
      forcedFps = parseInt(arg.split('=')[1], 10) || 0;
    } else if (arg === '-n' && i + 1 < args.length) {
      maxFrames = parseInt(args[++i], 10) || 0;
      headless = true;
    } else if (arg.startsWith('-n=')) {
      maxFrames = parseInt(arg.split('=')[1], 10) || 0;
      headless = true;
    } else if (arg === '--headless') {
      headless = true;
    } else if (!arg.startsWith('-') && !romPath) {
      romPath = arg;
    }
  }

  if (!romPath) {
    console.log('Wagnostic 2.0 Single-File Node.js Host');
    console.log('Usage: node wagnostic.js <path-to-rom.wasm> [-n N] [--headless] [--scale=N] [--fps=N]');
    process.exit(1);
  }

  return { romPath, forcedScale, forcedFps, maxFrames, headless };
}

// ── Surface Render Helpers ─────────────────────────────────
function copySurfaceRgba32(vramRaw, width, height, stride, outBuffer) {
  if (stride === width) {
    outBuffer.set(vramRaw.subarray(0, width * height * 4));
  } else {
    for (let y = 0; y < height; y++) {
      const srcOffset = y * stride * 4;
      const dstOffset = y * width * 4;
      outBuffer.set(vramRaw.subarray(srcOffset, srcOffset + width * 4), dstOffset);
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
  let gifPtr = 0;
  let gamepadPtr = 0;
  let audioPtr = 0;
  let dispatchPtr = 0;

  let defaultFbPtr = 0;
  let defaultDirtyPtr = 0;
  let defaultAudioPtr = 0;

  const importObject = {
    env: {
      memory: new WebAssembly.Memory({ initial: 16 }),
      wextension: (namePtr, version) => {
        const name = readWasmString(namePtr);

        // 1. Framebuffer: framebuffer / surface
        if ((name === 'framebuffer' || name === 'surface' || name === 'std:framebuffer' || name === 'std:surface') && version === 1) {
          if (!surfacePtr) {
            surfacePtr = hostAlloc(24, 4);
            defaultFbPtr = hostAlloc(640 * 480 * 4, 4);

            const view = new DataView(memory.buffer, surfacePtr, 24);
            view.setUint32(0, 1, true);               // version
            view.setUint32(4, 24, true);              // size
            view.setUint32(8, 320, true);             // width
            view.setUint32(12, 240, true);            // height
            view.setUint32(16, 320, true);            // stride
            view.setUint32(20, defaultFbPtr, true);   // pixels
          }
          return surfacePtr;
        }

        // 2. Clock: std:clock
        if ((name === 'std:clock' || name === 'clock') && version === 1) {
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

        // 3. Keyboard: std:keyboard
        if ((name === 'std:keyboard' || name === 'keyboard') && version === 1) {
          if (!keyboardPtr) {
            keyboardPtr = hostAlloc(264, 4);
            const view = new DataView(memory.buffer, keyboardPtr, 264);
            view.setUint32(0, 1, true);
            view.setUint32(4, 264, true);
            new Uint8Array(memory.buffer, keyboardPtr + 8, 256).fill(0);
          }
          return keyboardPtr;
        }

        // 4. Mouse: std:mouse
        if ((name === 'std:mouse' || name === 'mouse') && version === 1) {
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

        // 5. GIF: std:gif
        if ((name === 'std:gif' || name === 'gif') && version === 1) {
          if (!gifPtr) {
            gifPtr = hostAlloc(28, 4);
            const view = new DataView(memory.buffer, gifPtr, 28);
            view.setUint32(0, 1, true);               // version
            view.setUint32(4, 28, true);              // size
            view.setUint32(8, 0, true);               // recording
            view.setUint32(12, 0, true);              // frame_count
            view.setUint32(16, maxFrames, true);      // max_frames
            view.setUint32(20, 2, true);              // delay_cs
            view.setUint32(24, 0, true);              // save_trigger
          }
          return gifPtr;
        }

        if ((name === 'std:gamepad' || name === 'gamepad') && version === 1) {
          if (!gamepadPtr) {
            gamepadPtr = hostAlloc(28, 4);
            const view = new DataView(memory.buffer, gamepadPtr, 28);
            view.setUint32(0, 1, true);
            view.setUint32(4, 28, true);
            view.setUint32(8, 0, true);               // buttons
          }
          return gamepadPtr;
        }

        if ((name === 'std:audio' || name === 'audio') && version === 1) {
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

        if ((name === 'std:dispatch' || name === 'wash:dispatch' || name === 'dispatch') && version === 1) {
          if (!dispatchPtr) {
            dispatchPtr = hostAlloc(60, 4);
            const view = new DataView(memory.buffer, dispatchPtr, 60);
            view.setUint32(0, 1, true);               // version
            view.setUint32(4, 60, true);              // size
            view.setUint32(8, 0, true);               // worker_id
            view.setUint32(12, 1, true);              // worker_count
            view.setUint32(16, 0, true);              // global_offset
            view.setUint32(20, 320 * 240, true);      // global_length
            view.setUint32(24, 320 * 240, true);      // total_elements
            view.setUint32(28, 0, true);              // tile_x
            view.setUint32(32, 0, true);              // tile_y
            view.setUint32(36, 320, true);            // tile_w
            view.setUint32(40, 240, true);            // tile_h
            view.setUint32(44, 320, true);            // full_w
            view.setUint32(48, 240, true);            // full_h
            view.setUint32(52, 320, true);            // stride
            view.setUint32(56, defaultFbPtr || 0, true); // data_ptr
          }
          return dispatchPtr;
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

  // ── Headless Mode Execution ─────────────────────────────────
  if (headless) {
    let frameCount = 0;
    const targetFps = forcedFps || 60;
    const dt = 1.0 / targetFps;

    while (maxFrames === 0 || frameCount < maxFrames) {
      if (clockPtr && clockPtr + 32 <= memory.buffer.byteLength) {
        const view = new DataView(memory.buffer, clockPtr, 32);
        view.setBigUint64(8, BigInt(Math.floor(frameCount * 1000 / targetFps)), true);
        view.setFloat32(24, dt, true);
      }

      let status = WUPDATE_OK;
      try {
        status = exports.wupdate();
      } catch (err) {
        console.error('wupdate() runtime error at frame ' + frameCount + ':', err.message);
        process.exit(1);
      }

      if (status === WUPDATE_EXIT) break;
      if (status < 0) {
        console.error('wupdate() returned error code ' + status + ' at frame ' + frameCount);
        process.exit(1);
      }
      frameCount++;
    }
    process.exit(0);
  }

  // ── GUI Mode Execution (SDL2) ───────────────────────────────
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
  let framesRun = 0;

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
    if (surfacePtr && surfacePtr + 24 <= memory.buffer.byteLength) {
      const sView = new DataView(memory.buffer, surfacePtr, 24);
      const width = sView.getUint32(8, true) || 320;
      const height = sView.getUint32(12, true) || 240;
      const stride = sView.getUint32(16, true) || width;
      const pixelsPtr = sView.getUint32(20, true);
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
        const vramRaw = new Uint8Array(memory.buffer, pixelsPtr, stride * height * 4);
        const requiredSize = width * height * 4;

        if (stride === width) {
          try {
            window.render(width, height, width * 4, 'rgba32', vramRaw);
          } catch (err) {}
        } else {
          if (!conversionBuffer || conversionBuffer.length !== requiredSize) {
            conversionBuffer = Buffer.alloc(requiredSize);
          }
          copySurfaceRgba32(vramRaw, width, height, stride, conversionBuffer);
          try {
            window.render(width, height, width * 4, 'rgba32', conversionBuffer);
          } catch (err) {}
        }
      }
    }

    framesRun++;
    if (maxFrames > 0 && framesRun >= maxFrames) {
      exitCleanly();
      return;
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

