
PAPAGAIOCC = ../../papagaio/papagaiocc
CLANG = clang
INPUT_IMAGE = image.webp
RAW_IMAGE = image.raw
HEADER_IMAGE = image_data.h
TARGET_WASM = game.wasm

WASM_FLAGS = --target=wasm32-unknown-unknown -ffreestanding -nostdlib \
             -Wl,--no-entry -Wl,--export=main -Wl,--export-dynamic \
             -Wl,--allow-undefined -Wl,--global-base=160000

.PHONY: all clean convert

all: convert $(TARGET_WASM)

convert: $(HEADER_IMAGE)

$(HEADER_IMAGE): $(INPUT_IMAGE)
	../../tools/img2fb.sh $(INPUT_IMAGE) image

$(TARGET_WASM): main.c $(HEADER_IMAGE)
	$(CLANG) $(WASM_FLAGS) main.c -o $(TARGET_WASM) -I.

clean:
	rm -f $(RAW_IMAGE) $(HEADER_IMAGE) $(TARGET_WASM) palette.png

