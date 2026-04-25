
CLANG = clang
CFLAGS = --target=wasm32 -nostdlib -fno-delete-null-pointer-checks -O3
LDFLAGS = -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined -Wl,--global-base=1048576 -Wl,--initial-memory=8388608

.PHONY: all clean

all: buttons_test.wasm

buttons_test.wasm: buttons_test.c
	$(CLANG) $(CFLAGS) $(LDFLAGS) buttons_test.c -o buttons_test.wasm

clean:
	rm -f buttons_test.wasm
