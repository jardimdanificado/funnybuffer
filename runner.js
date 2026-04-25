
// UI Elements
const canvas = document.getElementById('gameCanvas');
const ctx = canvas.getContext('2d', { alpha: false });
const wasmInput = document.getElementById('wasmInput');
const wrapper = document.getElementById('canvasWrapper');

let worker = null;
let imageDataCache = null;

// WASM Initialization via Worker
wasmInput.addEventListener('change', async (e) => {
    const file = e.target.files[0];
    if (!file) return;

    if (worker) worker.terminate();
    
    // Create worker to run the blocking WASM loop
    worker = new Worker('worker.js');
    
    const buffer = await file.arrayBuffer();
    worker.postMessage({ type: 'load', buffer }, [buffer]);

    worker.onmessage = (msg) => {
        const { type, w, h, frame } = msg.data;
        
        if (type === 'init') {
            canvas.width = w;
            canvas.height = h;
            wrapper.classList.remove('empty');
            canvas.focus();
        } else if (type === 'draw') {
            renderFrame(frame, w || canvas.width, h || canvas.height);
        }
    };
});

function renderFrame(frame, w, h) {
    if (!imageDataCache || imageDataCache.width !== w || imageDataCache.height !== h) {
        imageDataCache = ctx.createImageData(w, h);
    }
    
    const data = imageDataCache.data;
    for (let i = 0; i < w * h; i++) {
        const color = frame[i];
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

// Note: To support inputs in a blocking WASM loop, we would need SharedArrayBuffer.
// Without it, the worker's message queue is never processed during while(1).
