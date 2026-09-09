CC = clang
CFLAGS = --target=wasm32 -nostdlib -fno-delete-null-pointer-checks -O3 -w -I../../include
LDFLAGS = -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined -Wl,--initial-memory=131072

all: ../bare_counter.wasm

../bare_counter.wasm: main.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<
