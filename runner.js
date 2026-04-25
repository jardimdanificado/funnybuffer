
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
    buttons: 0
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
    }

    animationFrameId = requestAnimationFrame(gameLoop);
}

function renderFrame(dv) {
    const w = dv.getUint32(sysOffset + 0, true);
    const h = dv.getUint32(sysOffset + 4, true);
    if (w === 0 || h === 0) return;

    if (!imageDataCache || imageDataCache.width !== w || imageDataCache.height !== h) {
        imageDataCache = ctx.createImageData(w, h);
    }
    
    const data = imageDataCache.data;
    const vramPtr = 296; // sizeof(SystemConfig)
    
    const frame16 = new Uint16Array(wasmMemory.buffer, vramPtr, w * h);
    const data32 = new Uint32Array(data.buffer);
    
    for (let i = 0; i < w * h; i++) {
        const color = frame16[i];
        const r = ((color >> 11) & 0x1F) << 3;
        const g = ((color >> 5) & 0x3F) << 2;
        const b = (color & 0x1F) << 3;
        data32[i] = (255 << 24) | (b << 16) | (g << 8) | r;
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
