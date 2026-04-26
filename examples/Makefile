
CLANG = clang
CFLAGS = --target=wasm32 -nostdlib -fno-delete-null-pointer-checks -O3
LDFLAGS = -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined -Wl,--global-base=1048576 -Wl,--initial-memory=8388608

.PHONY: all clean

all: buttons_test.wasm test_8bpp.wasm test_16bpp.wasm test_32bpp.wasm

buttons_test.wasm: buttons_test.c
	$(CLANG) $(CFLAGS) $(LDFLAGS) buttons_test.c -o buttons_test.wasm

test_8bpp.wasm: test_8bpp.c
	$(CLANG) $(CFLAGS) $(LDFLAGS) test_8bpp.c -o test_8bpp.wasm

test_16bpp.wasm: test_16bpp.c
	$(CLANG) $(CFLAGS) $(LDFLAGS) test_16bpp.c -o test_16bpp.wasm

test_32bpp.wasm: test_32bpp.c
	$(CLANG) $(CFLAGS) $(LDFLAGS) test_32bpp.c -o test_32bpp.wasm

clean:
	rm -f *.wasm
