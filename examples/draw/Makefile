
TARGET_WASM = game.wasm
HOST_RUNNER = ../../funnybuffer
CLANG = clang

# Flags for WASM compilation
WASM_FLAGS = --target=wasm32-unknown-unknown -ffreestanding -nostdlib \
             -Wl,--no-entry -Wl,--export=main -Wl,--export-dynamic \
             -Wl,--allow-undefined -Wl,--global-base=1048576

all: $(TARGET_WASM) run

$(TARGET_WASM): main.c olive.c
	$(CLANG) $(WASM_FLAGS) main.c -o $(TARGET_WASM) -I.

run: $(TARGET_WASM)
	$(HOST_RUNNER) $(TARGET_WASM)

clean:
	rm -f $(TARGET_WASM)

.PHONY: all run clean
