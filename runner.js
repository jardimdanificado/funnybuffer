
// UI Elements
const canvas = document.getElementById('gameCanvas');
const wrapper = document.getElementById('canvasWrapper');
const ctx = canvas.getContext('2d', { alpha: false, desynchronized: true });

let wasmInstance = null;
let wasmMemory = null;
let animationFrameId = null;
let imageDataCache = null;
let lastFrameTime = 0;
const FRAME_MIN_TIME = 1000 / 60; // Target 60 FPS

const sysOffset = 0;

// Estado do Input
const input = {
    keys: new Uint8Array(256),
    buttons: 0,
    mouse: { x: 0, y: 0, buttons: 0, wheel: 0 }
};

// Mapeamento de Teclas
const keyMap = {
    'ArrowRight': 79, 'ArrowLeft': 80, 'ArrowDown': 81, 'ArrowUp': 82,
    'KeyZ': 29, 'KeyX': 27, 'Enter': 40, 'Escape': 41, 'ShiftLeft': 225
};

const btnMap = {
    'up': 1 << 0, 'down': 1 << 1, 'left': 1 << 2, 'right': 1 << 3,
    'a': 1 << 4, 'b': 1 << 5, 'select': 1 << 6, 'start': 1 << 7
};

const wasmInput = document.getElementById('wasmInput');
wasmInput.addEventListener('change', async (e) => {
    const file = e.target.files[0];
    if (!file) return;
    
    const buffer = await file.arrayBuffer();
    await loadWasm(buffer);
});

async function loadWasm(buffer) {
    if (animationFrameId) {
        cancelAnimationFrame(animationFrameId);
    }

    const importObject = {
        env: {
            init: (w, h, vram, ram) => {
                canvas.width = w;
                canvas.height = h;
                wrapper.classList.remove('empty');
                canvas.focus();
                
                const dv = new DataView(wasmMemory.buffer);
                dv.setUint32(sysOffset + 0, w, true);
                dv.setUint32(sysOffset + 4, h, true);
                dv.setUint32(sysOffset + 8, ram, true);
                dv.setUint32(sysOffset + 12, vram, true);
            },
            get_ticks: () => performance.now()
        }
    };

    const { instance } = await WebAssembly.instantiate(buffer, importObject);
    wasmInstance = instance;
    wasmMemory = instance.exports.memory;
    
    lastFrameTime = performance.now();
    animationFrameId = requestAnimationFrame(gameLoop);
}

function gameLoop(now) {
    if (!wasmInstance) return;

    const elapsed = now - lastFrameTime;

    if (elapsed >= FRAME_MIN_TIME) {
        lastFrameTime = now - (elapsed % FRAME_MIN_TIME);

        const dv = new DataView(wasmMemory.buffer);

        // Sincroniza Input
        const wasmKeys = new Uint8Array(wasmMemory.buffer, sysOffset + 40, 256);
        wasmKeys.set(input.keys);
        dv.setUint32(sysOffset + 20, input.buttons, true);

        // Sincroniza Mouse
        dv.setInt32(sysOffset + 296, input.mouse.x, true);
        dv.setInt32(sysOffset + 300, input.mouse.y, true);
        dv.setUint32(sysOffset + 304, input.mouse.buttons, true);
        dv.setInt32(sysOffset + 308, input.mouse.wheel, true);

        // Chama o frame update (game_frame ou main)
        const frameFunc = wasmInstance.exports.game_frame || wasmInstance.exports.main;
        if (frameFunc) {
            frameFunc();
        }

        // Renderização Automática baseada no flag redraw
        const redraw = dv.getUint32(sysOffset + 16, true);
        if (redraw) {
            renderFrame(dv);
            dv.setUint32(sysOffset + 16, 0, true);
        }

        // Atualiza título da página se necessário
        const titleBytes = new Uint8Array(wasmMemory.buffer, sysOffset + 312, 128);
        const firstZero = titleBytes.indexOf(0);
        const titleStr = new TextDecoder().decode(titleBytes.subarray(0, firstZero > -1 ? firstZero : 128)).trim();
        if (titleStr && document.title !== titleStr) {
            document.title = titleStr;
        }
    }

    animationFrameId = requestAnimationFrame(gameLoop);
}

function renderFrame(dv) {
    const w = dv.getUint32(sysOffset + 0, true);
    const h = dv.getUint32(sysOffset + 4, true);
    const bpp = dv.getUint32(sysOffset + 440, true); // 1, 2 or 4
    
    if (w === 0 || h === 0) return;

    if (!imageDataCache || imageDataCache.width !== w || imageDataCache.height !== h) {
        imageDataCache = ctx.createImageData(w, h);
    }
    
    const data = imageDataCache.data;
    const vramPtr = 512;
    const data32 = new Uint32Array(data.buffer);
    
    if (bpp === 4) {
        // 32bpp: RGBA8888
        const frame32 = new Uint32Array(wasmMemory.buffer, vramPtr, w * h);
        data32.set(frame32);
    } else if (bpp === 1) {
        // 8bpp: RGB332
        const frame8 = new Uint8Array(wasmMemory.buffer, vramPtr, w * h);
        for (let i = 0; i < w * h; i++) {
            const c = frame8[i];
            const r = ((c >> 5) & 0x07) * 255 / 7;
            const g = ((c >> 2) & 0x07) * 255 / 7;
            const b = (c & 0x03) * 255 / 3;
            data32[i] = (255 << 24) | (b << 16) | (g << 8) | r;
        }
    } else {
        // Default 16bpp: RGB565
        const frame16 = new Uint16Array(wasmMemory.buffer, vramPtr, w * h);
        for (let i = 0; i < w * h; i++) {
            const c = frame16[i];
            const r = ((c >> 11) & 0x1F) * 255 / 31;
            const g = ((c >> 5) & 0x3F) * 255 / 63;
            const b = (c & 0x1F) * 255 / 31;
            data32[i] = (255 << 24) | (b << 16) | (g << 8) | r;
        }
    }
    ctx.putImageData(imageDataCache, 0, 0);
}

// Input Handlers
window.addEventListener('keydown', (e) => {
    const code = keyMap[e.code];
    if (code !== undefined) input.keys[code] = 1;
});

window.addEventListener('keyup', (e) => {
    const code = keyMap[e.code];
    if (code !== undefined) input.keys[code] = 0;
});

document.querySelectorAll('.virtual-gamepad button').forEach(btn => {
    const bit = btnMap[btn.dataset.btn];
    if (!bit) return;

    const setBtn = (val) => {
        if (val) input.buttons |= bit;
        else input.buttons &= ~bit;
    };

    btn.addEventListener('pointerdown', (e) => { e.preventDefault(); setBtn(true); });
    btn.addEventListener('pointerup', (e) => { e.preventDefault(); setBtn(false); });
    btn.addEventListener('pointerleave', (e) => { e.preventDefault(); setBtn(false); });
});

// Mouse Handlers
canvas.addEventListener('mousemove', (e) => {
    const rect = canvas.getBoundingClientRect();
    input.mouse.x = Math.floor((e.clientX - rect.left) * (canvas.width / rect.width));
    input.mouse.y = Math.floor((e.clientY - rect.top) * (canvas.height / rect.height));
});

canvas.addEventListener('mousedown', (e) => {
    input.mouse.buttons |= (1 << e.button);
});

canvas.addEventListener('mouseup', (e) => {
    input.mouse.buttons &= ~(1 << e.button);
});

canvas.addEventListener('wheel', (e) => {
    input.mouse.wheel += Math.sign(e.deltaY);
}, { passive: true });
