#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <palette.rgb>\n", argv[0]);
        return 1;
    }
    FILE *fpal = fopen(argv[1], "rb");
    if (!fpal) {
        fprintf(stderr, "Error opening %s\n", argv[1]);
        return 1;
    }
    unsigned char pal[768];
    size_t pal_len = fread(pal, 1, 768, fpal);
    fclose(fpal);
    
    int num_colors = pal_len / 3;
    if (num_colors == 0) return 1;

    unsigned char rgb[3];
    while (fread(rgb, 1, 3, stdin) == 3) {
        int best_i = 0;
        int best_dist = 1000000;
        for (int i = 0; i < num_colors; ++i) {
            int dr = rgb[0] - pal[i*3];
            int dg = rgb[1] - pal[i*3+1];
            int db = rgb[2] - pal[i*3+2];
            int dist = dr*dr + dg*dg + db*db;
            if (dist < best_dist) {
                best_dist = dist;
                best_i = i;
            }
        }
        fputc(best_i, stdout);
    }
    return 0;
}
