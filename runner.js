
// UI Elements
const canvas = document.getElementById('gameCanvas');
const wrapper = document.getElementById('canvasWrapper');

// Try to get context early
let ctx = null;
try {
    ctx = canvas.getContext('2d', { alpha: false, desynchronized: true });
} catch (e) {
    console.error("Failed to get 2D context:", e);
}

let worker = null;
let imageDataCache = null;

const wasmInput = document.getElementById('wasmInput');

wasmInput.addEventListener('change', async (e) => {
    const file = e.target.files[0];
    if (!file) return;

    if (worker) worker.terminate();
    
    // Setup worker
    worker = new Worker('worker.js');
    
    worker.onmessage = (msg) => {
        const { type, w, h, frame } = msg.data;
        
        if (type === 'init') {
            console.log(`WASM Initialized: ${w}x${h}`);
            canvas.width = w;
            canvas.height = h;
            wrapper.classList.remove('empty');
            canvas.focus();
        } else if (type === 'draw') {
            if (frame && ctx) {
                // Use w/h from message or fallback to canvas
                renderFrame(frame, w || canvas.width, h || canvas.height);
            }
        }
    };

    const buffer = await file.arrayBuffer();
    worker.postMessage({ type: 'load', buffer }, [buffer]);
});

function renderFrame(frame, w, h) {
    // Redimensiona o cache se o tamanho mudar
    if (!imageDataCache || imageDataCache.width !== w || imageDataCache.height !== h) {
        imageDataCache = ctx.createImageData(w, h);
    }
    
    const data = imageDataCache.data;
    const frame16 = new Uint16Array(frame);
    
    for (let i = 0; i < w * h; i++) {
        const color = frame16[i];
        
        // RGB565 to RGBA8888
        const r = (color >> 11) & 0x1F;
        const g = (color >> 5) & 0x3F;
        const b = color & 0x1F;
        
        const idx = i * 4;
        data[idx]     = (r << 3) | (r >> 2);
        data[idx + 1] = (g << 2) | (g >> 4);
        data[idx + 2] = (b << 3) | (b >> 2);
        data[idx + 3] = 255;
    }
    ctx.putImageData(imageDataCache, 0, 0);
}
