// Wagnostic 2.0 Web Runner - runs WASM ROMs in the browser
// Implements Wagnostic 2.0 ABI: wextension() + wupdate()
(function () {
  'use strict';

  // ── Constants & Formats ────────────────────────────────────────────────
  const WSURFACE_RGBA8888 = 1;
  const WSURFACE_BGRA8888 = 2;
  const WSURFACE_RGB565   = 3;
  const WSURFACE_RGB888   = 4;

  const WUPDATE_OK    =  0;
  const WUPDATE_EXIT  =  1;
  const WUPDATE_ERROR = -1;

  // Gamepad button bitmasks
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

  // KeyboardEvent.code -> USB HID scancode mapping
  const HID_SCANCODES = {
    'KeyA': 0x04, 'KeyB': 0x05, 'KeyC': 0x06, 'KeyD': 0x07,
    'KeyE': 0x08, 'KeyF': 0x09, 'KeyG': 0x0A, 'KeyH': 0x0B,
    'KeyI': 0x0C, 'KeyJ': 0x0D, 'KeyK': 0x0E, 'KeyL': 0x0F,
    'KeyM': 0x10, 'KeyN': 0x11, 'KeyO': 0x12, 'KeyP': 0x13,
    'KeyQ': 0x14, 'KeyR': 0x15, 'KeyS': 0x16, 'KeyT': 0x17,
    'KeyU': 0x18, 'KeyV': 0x19, 'KeyW': 0x1A, 'KeyX': 0x1B,
    'KeyY': 0x1C, 'KeyZ': 0x1D,
    'Digit1': 0x1E, 'Digit2': 0x1F, 'Digit3': 0x20, 'Digit4': 0x21,
    'Digit5': 0x22, 'Digit6': 0x23, 'Digit7': 0x24, 'Digit8': 0x25,
    'Digit9': 0x26, 'Digit0': 0x27,
    'Enter': 0x28, 'Escape': 0x29, 'Backspace': 0x2A, 'Tab': 0x2B,
    'Space': 0x2C, 'Minus': 0x2D, 'Equal': 0x2E, 'BracketLeft': 0x2F,
    'BracketRight': 0x30, 'Backslash': 0x31, 'Semicolon': 0x33,
    'Quote': 0x34, 'Backquote': 0x35, 'Comma': 0x36, 'Period': 0x37,
    'Slash': 0x38, 'CapsLock': 0x39,
    'F1': 0x3A, 'F2': 0x3B, 'F3': 0x3C, 'F4': 0x3D,
    'F5': 0x3E, 'F6': 0x3F, 'F7': 0x40, 'F8': 0x41,
    'F9': 0x42, 'F10': 0x43, 'F11': 0x44, 'F12': 0x45,
    'PrintScreen': 0x46, 'ScrollLock': 0x47, 'Pause': 0x48,
    'Insert': 0x49, 'Home': 0x4A, 'PageUp': 0x4B,
    'Delete': 0x4C, 'End': 0x4D, 'PageDown': 0x4E,
    'ArrowRight': 0x4F, 'ArrowLeft': 0x50, 'ArrowDown': 0x51, 'ArrowUp': 0x52,
    'NumLock': 0x53, 'NumpadDivide': 0x54, 'NumpadMultiply': 0x55,
    'NumpadSubtract': 0x56, 'NumpadAdd': 0x57,
    'NumpadEnter': 0x58, 'Numpad1': 0x59, 'Numpad2': 0x5A,
    'Numpad3': 0x5B, 'Numpad4': 0x5C, 'Numpad5': 0x5D,
    'Numpad6': 0x5E, 'Numpad7': 0x5F, 'Numpad8': 0x60,
    'Numpad9': 0x61, 'Numpad0': 0x62, 'NumpadDecimal': 0x63,
    'ShiftLeft': 0xE1, 'ShiftRight': 0xE5,
    'ControlLeft': 0xE0, 'ControlRight': 0xE4,
    'AltLeft': 0xE2, 'AltRight': 0xE6, 'MetaLeft': 0xE3, 'MetaRight': 0xE7
  };

  const GP_BTN_MAP = {
    'up': GP_UP, 'down': GP_DOWN, 'left': GP_LEFT, 'right': GP_RIGHT,
    'a': GP_A, 'b': GP_B, 'select': GP_SEL, 'start': GP_START
  };

  // ── DOM Elements ───────────────────────────────────────────────────────
  const fileInput   = document.getElementById('wasmInput');
  const canvas      = document.getElementById('gameCanvas');
  const ctx         = canvas ? canvas.getContext('2d', { alpha: false, desynchronized: true }) : null;
  const emptyState  = document.getElementById('emptyState');

  // State
  let wasmInstance = null;
  let wasmMemory   = null;
  let wasmExports  = null;
  let running      = false;

  let surfacePtr   = 0;
  let clockPtr     = 0;
  let keyboardPtr  = 0;
  let mousePtr     = 0;
  let gamepadPtr   = 0;
  let audioPtr     = 0;

  let defaultFbPtr    = 0;
  let defaultDirtyPtr = 0;
  let defaultAudioPtr = 0;
  let arenaOffset     = 0;

  // Screen config tracking
  let prevWidth  = 0;
  let prevHeight = 0;
  let prevScale  = 0;

  // Input state
  let mouseX       = 0;
  let mouseY       = 0;
  let mouseButtons = 0;
  let mouseWheelX  = 0;
  let mouseWheelY  = 0;
  let keysDown     = new Uint8Array(256);
  let gamepadBtns  = 0;
  let gamepadAxes  = new Int16Array(8);

  // Pre-allocated render buffers
  let imageData = null;

  // Timing
  let startTime = performance.now();
  let lastFrameTime = performance.now();

  // Audio Context
  let audioCtx = null;
  let audioNode = null;

  function initAudio() {
    if (audioCtx) return;
    try {
      const AudioContextClass = window.AudioContext || window.webkitAudioContext;
      if (!AudioContextClass) return;
      audioCtx = new AudioContextClass({ sampleRate: 44100 });
      const bufferSize = 1024;
      audioNode = audioCtx.createScriptProcessor(bufferSize, 0, 2);
      audioNode.onaudioprocess = function (e) {
        const outL = e.outputBuffer.getChannelData(0);
        const outR = e.outputBuffer.getChannelData(1);
        if (!wasmMemory || !audioPtr) {
          outL.fill(0);
          outR.fill(0);
          return;
        }

        const view = new DataView(wasmMemory.buffer, audioPtr, 36);
        const channels = view.getUint32(12, true) || 2;
        const format = view.getUint32(16, true) || 1;
        const bufPtr = view.getUint32(20, true);
        const capacity = view.getUint32(24, true) || 4096;
        let writeIdx = view.getUint32(28, true);
        let readIdx = view.getUint32(32, true);

        let available = (writeIdx >= readIdx) ? (writeIdx - readIdx) : 0;
        if (available > capacity) available = capacity;

        const count = Math.min(outL.length, available);
        if (format === 1 && bufPtr > 0) { // WAUDIO_F32
          const f32 = new Float32Array(wasmMemory.buffer, bufPtr, capacity * channels);
          for (let i = 0; i < count; i++) {
            const frame = (readIdx + i) % capacity;
            outL[i] = f32[frame * channels + 0];
            outR[i] = (channels > 1) ? f32[frame * channels + 1] : f32[frame * channels + 0];
          }
        }
        for (let i = count; i < outL.length; i++) {
          outL[i] = 0;
          outR[i] = 0;
        }
        view.setUint32(32, readIdx + count, true);
      };
      audioNode.connect(audioCtx.destination);
    } catch (e) {
      console.warn('WebAudio init failed:', e);
    }
  }

  function resumeAudio() {
    if (audioCtx && audioCtx.state === 'suspended') {
      audioCtx.resume();
    }
  }

  function hostAlloc(size, align = 4) {
    if (arenaOffset === 0) {
      arenaOffset = (wasmMemory.buffer.byteLength > 1048576) ? 0x20000 : 0x8000;
    }
    if (align > 1) {
      arenaOffset = (arenaOffset + align - 1) & ~(align - 1);
    }
    const ptr = arenaOffset;
    arenaOffset += size;
    return ptr;
  }

  function readWasmString(ptr) {
    if (!ptr || !wasmMemory) return '';
    const bytes = new Uint8Array(wasmMemory.buffer, ptr);
    let len = 0;
    while (len < 1024 && bytes[len] !== 0) len++;
    return new TextDecoder().decode(bytes.subarray(0, len));
  }

  // ── Canvas / Screen ────────────────────────────────────────────────────

  function resizeCanvas(w, h, scale) {
    if (!Number.isFinite(w) || w <= 0)   w = 320;
    if (!Number.isFinite(h) || h <= 0)   h = 240;
    if (!Number.isFinite(scale) || scale <= 0) scale = 1;
    if (canvas) {
      canvas.width  = w;
      canvas.height = h;
      canvas.style.width  = (w * scale) + 'px';
      canvas.style.height = (h * scale) + 'px';
      canvas.style.imageRendering = 'pixelated';
      imageData = ctx.createImageData(w, h);
    }
    prevWidth  = w;
    prevHeight = h;
    prevScale  = scale;
  }

  function renderSurface(surfaceOffset) {
    if (!wasmMemory || !surfaceOffset || !ctx) return;
    const view = new DataView(wasmMemory.buffer, surfaceOffset, 20);
    const width = view.getUint32(8, true) || 320;
    const height = view.getUint32(12, true) || 240;
    const pixelsPtr = view.getUint32(16, true);

    if (width !== prevWidth || height !== prevHeight) {
      resizeCanvas(width, height, 1);
    }
    if (!imageData || !pixelsPtr) return;

    const u8 = new Uint8Array(imageData.data.buffer);
    const v8 = new Uint8Array(wasmMemory.buffer, pixelsPtr, width * height * 4);
    u8.set(v8);

    ctx.putImageData(imageData, 0, 0);
  }

  // ── Main Loop ──────────────────────────────────────────────────────────

  function frame(now) {
    if (!running) return;

    const dt = (now - lastFrameTime) / 1000;
    lastFrameTime = now;

    // 1. Update Extensions Input & Clock
    if (clockPtr && clockPtr + 32 <= wasmMemory.buffer.byteLength) {
      const view = new DataView(wasmMemory.buffer, clockPtr, 32);
      view.setBigUint64(8, BigInt(Math.floor(now - startTime)), true);
      view.setFloat32(24, dt, true);
    }
    if (keyboardPtr && keyboardPtr + 264 <= wasmMemory.buffer.byteLength) {
      new Uint8Array(wasmMemory.buffer, keyboardPtr + 8, 256).set(keysDown);
    }
    if (mousePtr && mousePtr + 28 <= wasmMemory.buffer.byteLength) {
      const view = new DataView(wasmMemory.buffer, mousePtr, 28);
      view.setInt32(8, mouseX, true);
      view.setInt32(12, mouseY, true);
      view.setUint32(16, mouseButtons, true);
      view.setInt32(20, mouseWheelX, true);
      view.setInt32(24, mouseWheelY, true);
    }
    if (gamepadPtr && gamepadPtr + 28 <= wasmMemory.buffer.byteLength) {
      const view = new DataView(wasmMemory.buffer, gamepadPtr, 28);
      view.setUint32(8, gamepadBtns, true);
    }

    // 2. Call wupdate()
    let status = WUPDATE_OK;
    try {
      status = wasmExports.wupdate();
    } catch (err) {
      console.error('wupdate() threw:', err);
      running = false;
      return;
    }

    if (status === WUPDATE_EXIT) {
      running = false;
      console.log('ROM exited cleanly (WUPDATE_EXIT)');
      return;
    }
    if (status < 0) {
      running = false;
      console.error('ROM returned error (WUPDATE_ERROR):', status);
      return;
    }

    // 3. Render Surface
    if (surfacePtr) {
      renderSurface(surfacePtr);
    }

    // 4. Reset relative mouse wheel deltas
    mouseWheelX = 0;
    mouseWheelY = 0;

    requestAnimationFrame(frame);
  }

  // ── WASM Loading ───────────────────────────────────────────────────────

  function extractFromTar(buf, filename) {
    const dv = new DataView(buf);
    let offset = 0;
    let lastFound = null;
    while (offset + 512 <= buf.byteLength) {
      if (dv.getUint8(offset) === 0) break;
      let name = '';
      for (let i = 0; i < 100; i++) {
        let b = dv.getUint8(offset + i);
        if (b === 0) break;
        name += String.fromCharCode(b);
      }
      let sizeStr = '';
      for (let i = 124; i < 135; i++) {
        let b = dv.getUint8(offset + i);
        if (b >= 48 && b <= 55) sizeStr += String.fromCharCode(b);
      }
      let size = parseInt(sizeStr || '0', 8);
      if (name === filename) {
        lastFound = new Uint8Array(buf, offset + 512, size);
      }
      let skip = size + ((512 - (size % 512)) % 512);
      offset += 512 + skip;
    }
    return lastFound;
  }

  function loadRomFromBuffer(buf) {
    surfacePtr = 0;
    clockPtr = 0;
    keyboardPtr = 0;
    mousePtr = 0;
    gamepadPtr = 0;
    audioPtr = 0;
    arenaOffset = 0;

    let wasmBuffer = buf;
    const mainWasm = extractFromTar(buf, 'main.wasm');
    if (mainWasm) {
      wasmBuffer = mainWasm.buffer.slice(mainWasm.byteOffset, mainWasm.byteOffset + mainWasm.byteLength);
    }

    const wasmImports = {
      env: {
        wextension: function(namePtr, version) {
          const name = readWasmString(namePtr);
          if ((name === 'framebuffer' || name === 'std:surface' || name === 'surface') && version === 1) {
            if (!surfacePtr) {
              surfacePtr = hostAlloc(20, 4);
              defaultFbPtr = hostAlloc(640 * 480 * 4, 4);

              const view = new DataView(wasmMemory.buffer, surfacePtr, 20);
              view.setUint32(0, 1, true);               // version
              view.setUint32(4, 20, true);              // size
              view.setUint32(8, 320, true);             // width
              view.setUint32(12, 240, true);            // height
              view.setUint32(16, defaultFbPtr, true);   // pixels
            }
            return surfacePtr;
          }

          if ((name === 'std:clock' || name === 'clock') && version === 1) {
            if (!clockPtr) {
              clockPtr = hostAlloc(32, 8);
              const view = new DataView(wasmMemory.buffer, clockPtr, 32);
              view.setUint32(0, 1, true);
              view.setUint32(4, 32, true);
              view.setBigUint64(8, 0n, true);
              view.setBigUint64(16, 1000n, true);
              view.setFloat32(24, 0.0166667, true);
            }
            return clockPtr;
          }

          if ((name === 'std:keyboard' || name === 'keyboard') && version === 1) {
            if (!keyboardPtr) {
              keyboardPtr = hostAlloc(264, 4);
              const view = new DataView(wasmMemory.buffer, keyboardPtr, 264);
              view.setUint32(0, 1, true);
              view.setUint32(4, 264, true);
              new Uint8Array(wasmMemory.buffer, keyboardPtr + 8, 256).fill(0);
            }
            return keyboardPtr;
          }

          if ((name === 'std:mouse' || name === 'mouse') && version === 1) {
            if (!mousePtr) {
              mousePtr = hostAlloc(28, 4);
              const view = new DataView(wasmMemory.buffer, mousePtr, 28);
              view.setUint32(0, 1, true);
              view.setUint32(4, 28, true);
              view.setInt32(8, 0, true);
              view.setInt32(12, 0, true);
              view.setUint32(16, 0, true);
              view.setInt32(20, 0, true);
              view.setInt32(24, 0, true);
            }
            return mousePtr;
          }

          if ((name === 'std:gamepad' || name === 'gamepad') && version === 1) {
            if (!gamepadPtr) {
              gamepadPtr = hostAlloc(28, 4);
              const view = new DataView(wasmMemory.buffer, gamepadPtr, 28);
              view.setUint32(0, 1, true);
              view.setUint32(4, 28, true);
              view.setUint32(8, 0, true);
            }
            return gamepadPtr;
          }

          if ((name === 'std:audio' || name === 'audio') && version === 1) {
            initAudio();
            if (!audioPtr) {
              audioPtr = hostAlloc(36, 4);
              defaultAudioPtr = hostAlloc(4096 * 2 * 4, 4);
              const view = new DataView(wasmMemory.buffer, audioPtr, 36);
              view.setUint32(0, 1, true);
              view.setUint32(4, 36, true);
              view.setUint32(8, 44100, true);
              view.setUint32(12, 2, true);
              view.setUint32(16, 1, true); // F32
              view.setUint32(20, defaultAudioPtr, true);
              view.setUint32(24, 4096, true);
              view.setUint32(28, 0, true);
              view.setUint32(32, 0, true);
            }
            return audioPtr;
          }

          if ((name === 'std:dispatch' || name === 'wash:dispatch' || name === 'dispatch') && version === 1) {
            if (!dispatchPtr) {
              dispatchPtr = hostAlloc(60, 4);
              const view = new DataView(wasmMemory.buffer, dispatchPtr, 60);
              view.setUint32(0, 1, true);
              view.setUint32(4, 60, true);
              view.setUint32(8, 0, true);
              view.setUint32(12, 1, true);
              view.setUint32(16, 0, true);
              view.setUint32(20, 320 * 240, true);
              view.setUint32(24, 320 * 240, true);
              view.setUint32(28, 0, true);
              view.setUint32(32, 0, true);
              view.setUint32(36, 320, true);
              view.setUint32(40, 240, true);
              view.setUint32(44, 320, true);
              view.setUint32(48, 240, true);
              view.setUint32(52, 320, true);
              view.setUint32(56, defaultFbPtr || 0, true);
            }
            return dispatchPtr;
          }

          return 0;
        }
      }
    };

    WebAssembly.instantiate(wasmBuffer, wasmImports).then(function (result) {
      wasmInstance = result.instance;
      wasmExports  = wasmInstance.exports;
      wasmMemory   = wasmExports.memory || wasmExports.linear_memory;

      if (!wasmMemory) {
        console.error('ROM does not export memory');
        return;
      }

      if (typeof wasmExports.wupdate !== 'function') {
        console.error('ROM does not export wupdate() function');
        return;
      }

      resizeCanvas(320, 240, 1);

      if (emptyState) emptyState.style.display = 'none';
      if (canvas) canvas.style.display = 'block';

      console.log('Wagnostic 2.0 ROM loaded.');

      running = true;
      startTime = performance.now();
      lastFrameTime = performance.now();
      requestAnimationFrame(frame);
    }).catch(function (err) {
      console.error('Failed to load ROM:', err);
    });
  }

  function loadRom(wasmUrl) {
    running = false;
    keysDown.fill(0);
    gamepadBtns = 0;
    mouseButtons = 0;
    mouseWheelX = 0;
    mouseWheelY = 0;

    fetch(wasmUrl).then(function (r) { return r.arrayBuffer(); })
      .then(loadRomFromBuffer).catch(function (err) {
        console.error('Fetch failed:', err);
      });
  }

  // ── Event Listeners ────────────────────────────────────────────────────

  if (fileInput) {
    fileInput.addEventListener('change', function (e) {
      const file = e.target.files[0];
      if (!file) return;
      const reader = new FileReader();
      reader.onload = function(evt) {
        loadRomFromBuffer(evt.target.result);
      };
      reader.readAsArrayBuffer(file);
    });
  }

  document.addEventListener('keydown', function (e) {
    if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;
    resumeAudio();
    const hid = HID_SCANCODES[e.code];
    if (hid !== undefined) {
      keysDown[hid] = 1;
      e.preventDefault();
    }
    if (e.code === 'ArrowUp')    gamepadBtns |= GP_UP;
    if (e.code === 'ArrowDown')  gamepadBtns |= GP_DOWN;
    if (e.code === 'ArrowLeft')  gamepadBtns |= GP_LEFT;
    if (e.code === 'ArrowRight') gamepadBtns |= GP_RIGHT;
    if (e.code === 'KeyZ')       gamepadBtns |= GP_A;
    if (e.code === 'KeyX')       gamepadBtns |= GP_B;
    if (e.code === 'Enter')      gamepadBtns |= GP_START;
    if (e.code === 'ShiftLeft' || e.code === 'ShiftRight') gamepadBtns |= GP_SEL;
  });

  document.addEventListener('keyup', function (e) {
    if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;
    const hid = HID_SCANCODES[e.code];
    if (hid !== undefined) {
      keysDown[hid] = 0;
      e.preventDefault();
    }
    if (e.code === 'ArrowUp')    gamepadBtns &= ~GP_UP;
    if (e.code === 'ArrowDown')  gamepadBtns &= ~GP_DOWN;
    if (e.code === 'ArrowLeft')  gamepadBtns &= ~GP_LEFT;
    if (e.code === 'ArrowRight') gamepadBtns &= ~GP_RIGHT;
    if (e.code === 'KeyZ')       gamepadBtns &= ~GP_A;
    if (e.code === 'KeyX')       gamepadBtns &= ~GP_B;
    if (e.code === 'Enter')      gamepadBtns &= ~GP_START;
    if (e.code === 'ShiftLeft' || e.code === 'ShiftRight') gamepadBtns &= ~GP_SEL;
  });

  if (canvas) {
    canvas.addEventListener('mousemove', function (e) {
      const rect = canvas.getBoundingClientRect();
      const scaleX = canvas.width / rect.width;
      const scaleY = canvas.height / rect.height;
      mouseX = ((e.clientX - rect.left) * scaleX) | 0;
      mouseY = ((e.clientY - rect.top) * scaleY) | 0;
    });

    canvas.addEventListener('mousedown', function (e) {
      resumeAudio();
      if (e.button === 0) mouseButtons |= 1;
      if (e.button === 2) mouseButtons |= 2;
      if (e.button === 1) mouseButtons |= 4;
      e.preventDefault();
    });

    canvas.addEventListener('mouseup', function (e) {
      if (e.button === 0) mouseButtons &= ~1;
      if (e.button === 2) mouseButtons &= ~2;
      if (e.button === 1) mouseButtons &= ~4;
      e.preventDefault();
    });

    canvas.addEventListener('wheel', function (e) {
      mouseWheelX += Math.sign(e.deltaX);
      mouseWheelY -= Math.sign(e.deltaY);
      e.preventDefault();
    }, { passive: false });

    canvas.addEventListener('contextmenu', function (e) { e.preventDefault(); });
  }

  window.wagnosticLoadRomFromBuffer = loadRomFromBuffer;

  const urlParams = new URLSearchParams(window.location.search);
  const autoRom = urlParams.get('rom');
  if (autoRom) {
    const romUrl = autoRom.endsWith('.wasm') ? autoRom : autoRom + '.wasm';
    loadRom(romUrl);
  }
})();
