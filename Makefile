
CC = gcc
CFLAGS = -O3 -Wall -I./papagaio/lib/wasm3
LDFLAGS = -lSDL2 -lGL -lm

# List of all necessary Wasm3 source files
WASM3_SRC = $(wildcard papagaio/lib/wasm3/*.c)
SRC = host.c $(WASM3_SRC)

TARGET = funnybuffer

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
