// Button definitions mirroring host.c
const BTN_UP     = (1 << 0);
const BTN_DOWN   = (1 << 1);
const BTN_LEFT   = (1 << 2);
const BTN_RIGHT  = (1 << 3);
const BTN_A      = (1 << 4);
const BTN_B      = (1 << 5);
const BTN_X      = (1 << 6);
const BTN_Y      = (1 << 7);
const BTN_L1     = (1 << 8);
const BTN_R1     = (1 << 9);
const BTN_START  = (1 << 10);
const BTN_SELECT = (1 << 11);
const BTN_L2     = (1 << 12);
const BTN_R2     = (1 << 13);
const BTN_L3     = (1 << 14);
const BTN_R3     = (1 << 15);

// DOM Elements
const canvas = document.getElementById('gameCanvas');
const ctx = canvas.getContext('2d', { alpha: false });
const wasmInput = document.getElementById('wasmInput');
const wrapper = document.getElementById('canvasWrapper');

// WASM State
let wasmInstance = null;
let wasmMemory = null;
let loopId = null;

// System Config Offset (determined via papagaio_system)
let sysOffset = 0;

// Host imported functions
const env = {
    get_ticks: () => Math.floor(performance.now())
};

// Keyboard state
let gamepadButtons = 0;

// Keyboard mapping
const keyMap = {
    'ArrowUp': BTN_UP,
    'ArrowDown': BTN_DOWN,
    'ArrowLeft': BTN_LEFT,
    'ArrowRight': BTN_RIGHT,
    'z': BTN_A,
    'Z': BTN_A,
    'x': BTN_B,
    'X': BTN_B,
    'Enter': BTN_START,
    'Shift': BTN_SELECT,
    'Escape': BTN_SELECT
};

window.addEventListener('keydown', (e) => {
    if (keyMap[e.key]) {
        gamepadButtons |= keyMap[e.key];
        e.preventDefault();
    }
});

window.addEventListener('keyup', (e) => {
    if (keyMap[e.key]) {
        gamepadButtons &= ~keyMap[e.key];
        e.preventDefault();
    }
});

// Virtual Gamepad Mapping
const btnAttrMap = {
    'up': BTN_UP, 'down': BTN_DOWN, 'left': BTN_LEFT, 'right': BTN_RIGHT,
    'a': BTN_A, 'b': BTN_B, 'start': BTN_START, 'select': BTN_SELECT
};

document.querySelectorAll('.virtual-gamepad button').forEach(btn => {
    const bit = btnAttrMap[btn.getAttribute('data-btn')];
    if (!bit) return;
    
    const press = (e) => { gamepadButtons |= bit; e.preventDefault(); };
    const release = (e) => { gamepadButtons &= ~bit; e.preventDefault(); };
    
    btn.addEventListener('pointerdown', press);
    btn.addEventListener('pointerup', release);
    btn.addEventListener('pointercancel', release);
    btn.addEventListener('pointerout', release);
    btn.addEventListener('contextmenu', e => e.preventDefault());
});

// WASM Initialization
wasmInput.addEventListener('change', async (e) => {
    const file = e.target.files[0];
    if (!file) return;

    if (loopId) cancelAnimationFrame(loopId);

    const buffer = await file.arrayBuffer();
    try {
        const { instance } = await WebAssembly.instantiate(buffer, { env });
        wasmInstance = instance;
        wasmMemory = instance.exports.memory;
        
        startEngine();
    } catch (err) {
        alert("Failed to load WASM. Ensure it is a valid papagaio game.\n\n" + err);
        console.error(err);
    }
});

function startEngine() {
    wrapper.classList.remove('empty');
    canvas.focus(); // Grab focus for inputs

    // Call init
    if (wasmInstance.exports.papagaio_init) {
        wasmInstance.exports.papagaio_init();
    }

    // Get system config ptr
    sysOffset = wasmInstance.exports.papagaio_system();

    // Check memory requirements and grow if necessary (replicating host.c)
    const dv = new DataView(wasmMemory.buffer);
    const ram = dv.getUint32(sysOffset + 8, true);
    const ramPtr = dv.getUint32(sysOffset + 16, true);
    const totalNeeded = ramPtr + ram;
    const requiredPages = Math.floor((totalNeeded + 65535) / 65536);
    const currentPages = wasmMemory.buffer.byteLength / 65536;
    
    if (requiredPages > currentPages) {
        wasmMemory.grow(requiredPages - currentPages);
    }

    // Refresh DataView after memory growth, as the buffer reference might change
    const updatedDv = new DataView(wasmMemory.buffer);
    const w = updatedDv.getUint32(sysOffset + 0, true);
    const h = updatedDv.getUint32(sysOffset + 4, true);

    canvas.width = w;
    canvas.height = h;

    // Start loop
    lastTime = performance.now();
    loopId = requestAnimationFrame(gameLoop);
}

// Reusable ImageData
let imageDataCache = null;

function gameLoop() {
    loopId = requestAnimationFrame(gameLoop);

    const memoryView = new DataView(wasmMemory.buffer);
    
    // Inject gamepad inputs
    memoryView.setUint32(sysOffset + 36, gamepadButtons, true);

    // Call Update
    wasmInstance.exports.papagaio_update();

    // Check Dirty flags
    const fbDirty = memoryView.getUint32(sysOffset + 28, true);
    
    if (fbDirty) {
        renderFrame(memoryView);
        // Clear dirty flag
        memoryView.setUint32(sysOffset + 28, 0, true);
    }
}

function renderFrame(memoryView) {
    const w = memoryView.getUint32(sysOffset + 0, true);
    const h = memoryView.getUint32(sysOffset + 4, true);
    const vramPtr = memoryView.getUint32(sysOffset + 20, true);
    const palPtr = memoryView.getUint32(sysOffset + 24, true);

    // Create or reuse ImageData
    if (!imageDataCache || imageDataCache.width !== w || imageDataCache.height !== h) {
        imageDataCache = ctx.createImageData(w, h);
    }

    // Read palette (256 * 4 bytes = 1024 bytes)
    // Little-endian WASM 32-bit colors. Assume RGBA or similar formats?
    // host.c palette texture is GL_RGBA. 
    // uint32_t sp[256]; for (int i=0; i<256; i++) sp[i] = __builtin_bswap32(pal[i]); -> on big endian?
    // In host.c __builtin_bswap32 was used depending on PORTMASTER.
    // Let's read pixels directly. 
    // The palette in game is typically: A B G R or R G B A? 
    // The previous shell script generated: 0xRRGGBBAA or 0xAARRGGBB depending on awk handling 000000ff
    // Our palette values match what was in palette.hex. 

    const mem8 = new Uint8Array(wasmMemory.buffer);
    const fb = mem8.subarray(vramPtr, vramPtr + (w * h));
    const pal = new Uint8Array(wasmMemory.buffer, palPtr, 1024); // 256 colors * 4 bytes
    
    const data = imageDataCache.data;
    let outIdx = 0;

    for (let i = 0; i < fb.length; i++) {
        const colorIdx = fb[i] * 4;
        // Assume the 4 bytes in palette memory are [ R, G, B, A ] if array is byte-addressed?
        // JS TypedArrays in Little Endian mapping: Wait, let's just copy bytes.
        // Assuming the game memory is Little-Endian and the palette integer is 0xAARRGGBB?
        // Actually, looking at `image_palette[256] = { 0x800000ff, ... }`
        // In little endian memory, 0x800000ff is:
        // Byte 0: 0xFF (A? or blue?)
        // Byte 1: 0x00
        // Byte 2: 0x00
        // Byte 3: 0x80
        // Let's read as 32-bit ints from memory:
        
        // Simpler: Just extract them:
        // Using Uint32Array on palette avoids endianness confusion if we shift
        
        const r = pal[colorIdx + 3];
        const g = pal[colorIdx + 2];
        const b = pal[colorIdx + 1];
        const a = pal[colorIdx + 0]; 

        // If the color format is 0xRRGGBBAA, little endian memory stores it as AA BB GG RR
        // That means: pal[+0] = AA, pal[+1] = BB, pal[+2] = GG, pal[+3] = RR
        // So R=pal[+3], G=[+2], B=[+1] A=[+0].
        
        data[outIdx++] = r;
        data[outIdx++] = g;
        data[outIdx++] = b;
        data[outIdx++] = 255; // Force opaque alpha for canvas unless it needs transparency
    }

    ctx.putImageData(imageDataCache, 0, 0);
}
