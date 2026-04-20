#!/usr/bin/env python3
import sys
import struct
from PIL import Image

def main():
    if len(sys.argv) < 3:
        print("Uso: ./converter.py input.webp palette.hex")
        return

    in_file = sys.argv[1]
    pal_file = sys.argv[2]
    raw_out = "image.raw"
    header_out = "image_data.h"

    # Ler a paleta C exatamente como está no hex
    pal_rgb = []
    pal_hex_c = []
    with open(pal_file, 'r') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'): continue
            r = int(line[0:2], 16)
            g = int(line[2:4], 16)
            b = int(line[4:6], 16)
            pal_rgb.extend([r, g, b])
            # Vamos garantir o alpha FF ou como estiver no arquivo
            pal_hex_c.append("0x" + line)
            if len(pal_rgb) >= 256 * 3:
                break
    
    # Preencher com zeros se faltar cor para chegar a 256
    while len(pal_rgb) < 256 * 3:
        pal_rgb.extend([0, 0, 0])
        pal_hex_c.append("0x00000000")

    # Cria a imagem paleta de referência (1x1 pixel indexado)
    pal_img = Image.new("P", (1, 1))
    pal_img.putpalette(pal_rgb)

    print(f"--- Convertendo '{in_file}' usando '{pal_file}' ---")
    img = Image.open(in_file).convert("RGB")
    width, height = img.size

    # O dither=1 (FloydSteinberg) ou 0 (None)
    # quantize converte a imagem usando os indices ESTritos da paleta
    mapped_img = img.quantize(palette=pal_img, dither=0)
    
    print(f"--- Salvando {raw_out} ({width} x {height}) ---")
    with open(raw_out, "wb") as f:
        f.write(struct.pack("<H", width))
        f.write(struct.pack("<H", height))
        f.write(mapped_img.tobytes())

    print(f"--- Gerando {header_out} ---")
    with open(header_out, "w") as f:
        f.write("/* Gerado automaticamente por converter.py */\n")
        f.write("#ifndef IMAGE_DATA_H\n")
        f.write("#define IMAGE_DATA_H\n\n")
        f.write(f"static const int image_width = {width};\n")
        f.write(f"static const int image_height = {height};\n\n")
        
        f.write("static uint32_t image_palette[256] = {\n")
        for i in range(0, 256, 8):
            chunk = pal_hex_c[i:i+8]
            f.write("    " + ", ".join(chunk) + ",\n")
        f.write("};\n\n")

        f.write("static const unsigned char image_raw[] = {\n")
        with open(raw_out, "rb") as rf:
            data = rf.read()
        
        for i in range(0, len(data), 12):
            chunk = data[i:i+12]
            f.write("    " + ", ".join(f"0x{b:02x}" for b in chunk) + ",\n")
        f.write("};\n\n")
        f.write("#endif\n")
        print("Pronto!")

if __name__ == '__main__':
    main()
