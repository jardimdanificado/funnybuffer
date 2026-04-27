
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef          int   int32_t;
typedef          int   bool;
#define true 1
#define false 0

#include "data/sprites.h"

#pragma pack(push, 1)
typedef struct {
    char     message[128];      // 0
    uint32_t width;            // 128
    uint32_t height;           // 132
    uint32_t bpp;              // 136
    uint32_t scale;             // 140
    uint32_t audio_size;        // 144
    uint32_t audio_write_ptr;   // 148
    uint32_t audio_read_ptr;    // 152
    uint32_t audio_sample_rate; // 156
    uint32_t audio_bpp;         // 160
    uint32_t audio_channels;    // 164
    uint32_t signal_count;      // 168
    
    uint32_t gamepad_buttons;   // 172
    int32_t  joystick_lx, joystick_ly, joystick_rx, joystick_ry; // 176
    uint8_t  keys[256];         // 192
    
    int32_t  mouse_x;           // 448
    int32_t  mouse_y;           // 452
    uint32_t mouse_buttons;     // 456
    int32_t  mouse_wheel;       // 460

    uint8_t  reserved[48];      // 464
} SystemConfig;
#pragma pack(pop)

#define _sys ((volatile SystemConfig*)0)
#define _sig ((volatile uint8_t*)512)
static uint16_t* _fb;

#define BTN_UP     (1 << 0)
#define BTN_DOWN   (1 << 1)
#define BTN_LEFT   (1 << 2)
#define BTN_RIGHT  (1 << 3)
#define BTN_START  (1 << 7)

#define RGB565(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))
#define sheet_ptr ((const uint16_t*)image_raw)

// ============================================
// ROGUELIKE LOGIC
// ============================================
#define SPRITE_FLOOR   10
#define SPRITE_WALL    25
#define SPRITE_PLAYER  1
#define SPRITE_MONSTER 45
#define MAP_W 20
#define MAP_H 15

static uint8_t map[MAP_H][MAP_W];
typedef struct { int x, y, hp; } Entity;
static Entity player;
#define MAX_MONSTERS 10
static Entity monsters[MAX_MONSTERS];
static int num_monsters = 0;
static uint32_t _seed = 12345;
static uint32_t rand_u32() { _seed = _seed * 1103515245 + 12345; return (_seed / 65536) % 32768; }
static int rand_range(int min, int max) { if (max <= min) return min; return min + (rand_u32() % (max - min)); }

typedef struct { int x, y, w, h; } Room;
static Room rooms[15];
static int num_rooms = 0;

static void build_room(Room *r) {
    for (int y = r->y; y < r->y + r->h; y++)
        for (int x = r->x; x < r->x + r->w; x++)
            if (x >= 0 && x < MAP_W && y >= 0 && y < MAP_H) map[y][x] = 0;
}

static void generate_map() {
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) map[y][x] = 1;
    num_rooms = 0;
    for (int i = 0; i < 10; i++) {
        Room r; r.w = rand_range(4, 8); r.h = rand_range(4, 6);
        r.x = rand_range(1, MAP_W - r.w - 1); r.y = rand_range(1, MAP_H - r.h - 1);
        bool overlap = false;
        for (int j = 0; j < num_rooms; j++) {
            if (r.x < rooms[j].x + rooms[j].w + 1 && r.x + r.w + 1 > rooms[j].x &&
                r.y < rooms[j].y + rooms[j].h + 1 && r.y + r.h + 1 > rooms[j].y) { overlap = true; break; }
        }
        if (!overlap) {
            build_room(&r);
            if (num_rooms > 0) {
                int cx1 = rooms[num_rooms-1].x + rooms[num_rooms-1].w/2;
                int cy1 = rooms[num_rooms-1].y + rooms[num_rooms-1].h/2;
                int cx2 = r.x + r.w/2; int cy2 = r.y + r.h/2;
                for (int x = (cx1 < cx2 ? cx1 : cx2); x <= (cx1 < cx2 ? cx2 : cx1); x++) map[cy1][x] = 0;
                for (int y = (cy1 < cy2 ? cy1 : cy2); y <= (cy1 < cy2 ? cy2 : cy1); y++) map[y][cx2] = 0;
            }
            rooms[num_rooms++] = r;
        }
    }
    player.x = rooms[0].x + rooms[0].w / 2; player.y = rooms[0].y + rooms[0].h / 2; player.hp = 10;
    num_monsters = 0;
    for (int i = 1; i < num_rooms; i++) {
        if (num_monsters < MAX_MONSTERS) {
            monsters[num_monsters].x = rooms[i].x + rooms[i].w / 2;
            monsters[num_monsters].y = rooms[i].y + rooms[i].h / 2;
            monsters[num_monsters].hp = 3; num_monsters++;
        }
    }
}

typedef enum { STATE_PLAYING, STATE_GAMEOVER } GameState;
static GameState game_state = STATE_PLAYING;
static uint32_t prev_buttons = 0;

void reset_game() { game_state = STATE_PLAYING; generate_map(); }

void winit() {
    _sys->width = 320; _sys->height = 240; _sys->bpp = 16; _sys->scale = 4;
    _sys->signal_count = 4;
    _fb = (uint16_t*)(512 + _sys->signal_count);
    const char* t = "Wagnostic - Roguelike";
    for (int i = 0; i < 127 && t[i]; i++) ((char*)_sys->message)[i] = t[i];
    _sig[1] = 3; generate_map();
}

__attribute__((visibility("default")))
void wupdate() {
    _fb = (uint16_t*)(512 + _sys->signal_count); // Ensure alignment
    if (game_state == STATE_GAMEOVER) {
        for (int i=0; i<320*240; i++) _fb[i] = RGB565(255, 0, 0);
        uint32_t pressed = _sys->gamepad_buttons & ~prev_buttons;
        prev_buttons = _sys->gamepad_buttons;
        if (pressed & BTN_START) reset_game(); 
        _sig[0] = 1; return;
    }
    uint32_t pressed = _sys->gamepad_buttons & ~prev_buttons;
    prev_buttons = _sys->gamepad_buttons;
    int dx = 0, dy = 0;
    if (pressed & BTN_LEFT) dx = -1; if (pressed & BTN_RIGHT) dx = 1;
    if (pressed & BTN_UP) dy = -1; if (pressed & BTN_DOWN) dy = 1;
    if (dx != 0 || dy != 0) {
        int nx = player.x + dx; int ny = player.y + dy;
        if (nx >= 0 && nx < MAP_W && ny >= 0 && ny < MAP_H && map[ny][nx] == 0) {
            bool monster_there = false;
            for (int i = 0; i < num_monsters; i++) {
                if (monsters[i].hp > 0 && monsters[i].x == nx && monsters[i].y == ny) { monsters[i].hp--; monster_there = true; break; }
            }
            if (!monster_there) { player.x = nx; player.y = ny; }
            for (int i = 0; i < num_monsters; i++) {
                if (monsters[i].hp > 0) {
                    int mdx = (player.x > monsters[i].x) ? 1 : (player.x < monsters[i].x ? -1 : 0);
                    int mdy = (player.y > monsters[i].y) ? 1 : (player.y < monsters[i].y ? -1 : 0);
                    int mnx = monsters[i].x + mdx; int mny = monsters[i].y + mdy;
                    if (mnx == player.x && mny == player.y) { player.hp--; if (player.hp <= 0) game_state = STATE_GAMEOVER; }
                    else if (map[mny][mnx] == 0) { monsters[i].x = mnx; monsters[i].y = mny; }
                }
            }
        }
    }
    for (int i = 0; i < 320 * 240; i++) _fb[i] = RGB565(10, 10, 15);
    int off_x = (320 - MAP_W * 16) / 2; int off_y = (240 - MAP_H * 16) / 2;
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            int id = (map[y][x] == 1) ? SPRITE_WALL : SPRITE_FLOOR;
            int sx = (id % 16) * 16; int sy = (id / 16) * 16;
            for (int j = 0; j < 16; j++) {
                for (int i = 0; i < 16; i++) {
                    _fb[(off_y + y*16 + j) * 320 + (off_x + x*16 + i)] = sheet_ptr[(sy + j) * 256 + (sx + i)];
                }
            }
        }
    }
    int psx = (SPRITE_PLAYER % 16) * 16; int psy = (SPRITE_PLAYER / 16) * 16;
    for (int j = 0; j < 16; j++) for (int i = 0; i < 16; i++) {
        uint16_t c = sheet_ptr[(psy + j) * 256 + (psx + i)];
        if (c != 0xF81F) _fb[(off_y + player.y*16 + j) * 320 + (off_x + player.x*16 + i)] = c;
    }
    for (int m = 0; m < num_monsters; m++) {
        if (monsters[m].hp > 0) {
            int msx = (SPRITE_MONSTER % 16) * 16; int msy = (SPRITE_MONSTER / 16) * 16;
            for (int j = 0; j < 16; j++) for (int i = 0; i < 16; i++) {
                uint16_t c = sheet_ptr[(msy + j) * 256 + (msx + i)];
                if (c != 0xF81F) _fb[(off_y + monsters[m].y*16 + j) * 320 + (off_x + monsters[m].x*16 + i)] = c;
            }
        }
    }
    _sig[0] = 1;
}
