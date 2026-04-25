
let wasmInstance = null;
let wasmMemory = null;
let sysOffset = 0;

self.onmessage = async (e) => {
    const { type, buffer, sys_offset } = e.data;
    
    if (type === 'load') {
        sysOffset = sys_offset || 0;

        const importObject = {
            env: {
                init: (w, h, vram, ram) => {
                    self.postMessage({ type: 'init', w, h, vram, ram });
                },
                draw: () => {
                    const vramPtr = sysOffset + 296;
                    const dv = new DataView(wasmMemory.buffer);
                    const w = dv.getUint32(sysOffset + 0, true);
                    const h = dv.getUint32(sysOffset + 4, true);
                    
                    // Copy framebuffer to send to main thread
                    const frame = new Uint16Array(wasmMemory.buffer, vramPtr, w * h);
                    const frameCopy = new Uint16Array(frame);
                    self.postMessage({ type: 'draw', frame: frameCopy }, [frameCopy.buffer]);
                },
                get_ticks: () => performance.now()
            }
        };

        const { instance } = await WebAssembly.instantiate(buffer, importObject);
        wasmInstance = instance;
        wasmMemory = instance.exports.memory;
        
        if (instance.exports.main) {
            instance.exports.main();
        }
    }
};
