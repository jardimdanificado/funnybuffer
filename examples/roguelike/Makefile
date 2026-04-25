
TARGET_WASM = game.wasm
CLANG = clang

WASM_FLAGS = --target=wasm32-unknown-unknown -ffreestanding -nostdlib \
             -Wl,--no-entry -Wl,--export=main -Wl,--export-dynamic \
             -Wl,--allow-undefined -Wl,--global-base=160000

.PHONY: all clean

all: $(TARGET_WASM)

$(TARGET_WASM): main.c data/sprites_data.h
	$(CLANG) $(WASM_FLAGS) main.c -o $(TARGET_WASM) -I.

clean:
	rm -f $(TARGET_WASM)
