TARGET_WASM = game.wasm
PAPAGAIOCC = ../../papagaio/papagaiocc
HOST_RUNNER = ../../papagame

all: $(TARGET_WASM) run

$(TARGET_WASM): main.c olive.c
	$(PAPAGAIOCC) main.c -o $(TARGET_WASM) -I$(shell pwd)

run: $(TARGET_WASM)
	$(HOST_RUNNER) $(TARGET_WASM)

clean:
	rm -f $(TARGET_WASM)

.PHONY: all run clean
