
PAPAGAIOCC = ../../papagaio/papagaiocc
INPUT_IMAGE = image.webp
RAW_IMAGE = image.raw
HEADER_IMAGE = image_data.h
TARGET_WASM = game.wasm

.PHONY: all clean convert

all: convert $(TARGET_WASM)

convert: $(HEADER_IMAGE)

$(HEADER_IMAGE): $(INPUT_IMAGE) converter.py
	python3 converter.py $(INPUT_IMAGE) palette.hex

$(TARGET_WASM): main.c $(HEADER_IMAGE)
	$(PAPAGAIOCC) main.c -o $(TARGET_WASM) -I$(shell pwd)

clean:
	rm -f $(RAW_IMAGE) $(HEADER_IMAGE) $(TARGET_WASM) palette.png
