#!/bin/bash
# Uso: ./converter.sh input.webp
IN=${1:-image.webp}
RAW_OUT="image.raw"
HEADER_OUT="image_data.h"

if [ ! -f "$IN" ]; then echo "Erro: $IN não encontrado"; exit 1; fi

# 1. Gera a paleta PNG de referência a partir do palette.hex (256x1)
echo "--- Preparando paleta de referência ---"
# Limpa e gera 768 bytes binários (256 * 3)
python3 -c "
import sys
colors = open('palette.hex').readlines()
with open('palette.rgb', 'wb') as f:
    for c in colors[:256]:
        f.write(bytes.fromhex(c.strip()[:6]))
    # Padding se tiver menos de 256
    for _ in range(256 - len(colors)):
        f.write(b'\x00\x00\x00')
"
convert -size 256x1 -depth 8 rgb:palette.rgb palette_ref.png
rm palette.rgb

# 2. Converte a imagem para os índices EXATOS da paleta (sem dithering para teste inicial)
echo "--- Remapeando imagem para índices da paleta (Dither: None) ---"
ffmpeg -v error -i "$IN" -i palette_ref.png \
    -filter_complex "[0:v][1:v]paletteuse=dither=none:diff_mode=0" \
    -f rawvideo -pix_fmt gray -y "$RAW_OUT.tmp"

# 3. Pega largura e altura reais
W=$(ffprobe -v error -select_streams v:0 -show_entries stream=width -of default=noprint_wrappers=1:nokey=1 "$IN")
H=$(ffprobe -v error -select_streams v:0 -show_entries stream=height -of default=noprint_wrappers=1:nokey=1 "$IN")

# 4. Monta o arquivo RAW final com o cabeçalho de 4 bytes (W, H)
perl -e "print pack('S', $W)" > "$RAW_OUT"
perl -e "print pack('S', $H)" >> "$RAW_OUT"
cat "$RAW_OUT.tmp" >> "$RAW_OUT"
rm "$RAW_OUT.tmp" palette_ref.png

# 5. Gera o .h
echo "--- Atualizando $HEADER_OUT ---"
{
    echo "/* Gerado automaticamente */"
    echo "#ifndef IMAGE_DATA_H"
    echo "#define IMAGE_DATA_H"
    echo "static const int image_width = $W;"
    echo "static const int image_height = $H;"
    echo "static uint32_t image_palette[256] = {"
    sed 's/^/0x/' palette.hex | sed 's/$/,/' | xargs -n 8
    echo "};"
    echo "static const unsigned char image_raw[] = {"
    xxd -i < "$RAW_OUT"
    echo "};"
    echo "#endif"
} > "$HEADER_OUT"

echo "Pronto! $HEADER_OUT e $RAW_OUT atualizados ($W x $H)."
