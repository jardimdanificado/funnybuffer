
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef          int   int32_t;

#include "data/sprites_data.h"

extern void init(int w, int h, int vram, int ram);
extern uint32_t get_ticks();

#pragma pack(push, 1)
typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t ram;         
    uint32_t vram;        
    uint32_t redraw;    

    uint32_t gamepad_buttons; 
    int32_t  joystick_lx;     
    int32_t  joystick_ly;
    int32_t  joystick_rx;     
    int32_t  joystick_ry;
    uint8_t  keys[256];
} SystemConfig;
#pragma pack(pop)

#define _sys ((volatile SystemConfig*)0)
#define _fb ((volatile uint16_t*)296)

#define BTN_UP     (1 << 0)
#define BTN_DOWN   (1 << 1)
#define BTN_LEFT   (1 << 2)
#define BTN_RIGHT  (1 << 3)
#define BTN_A      (1 << 4)
#define BTN_B      (1 << 5)
#define BTN_START  (1 << 10)
#define BTN_SELECT (1 << 11)

#define RGB565(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

#define sheet_ptr ((const uint16_t*)image_raw)

// ============================================
// ROGUELIKE DEFINITIONS
// ============================================

#define SPRITE_FLOOR   10
#define SPRITE_WALL    25
#define SPRITE_PLAYER  1
#define SPRITE_MONSTER 45

#define MAP_W 30
#define MAP_H 21

#define VIEW_W 10
#define VIEW_H 7

static uint8_t map[MAP_H][MAP_W];
static uint8_t seen[MAP_H][MAP_W];

#pragma pack(push, 1)
typedef struct {
    int x, y;
    int sprite;
    int hp;
    int is_player;
    int active;
} Entity;
#pragma pack(pop)

#define MAX_ENTITIES 30
static Entity entities[MAX_ENTITIES];
static int num_entities = 0;
static int player_id = 0;

static int prev_buttons = 0;

#define STATE_PLAYING  0
#define STATE_GAMEOVER 1
static int game_state = STATE_PLAYING;
static int current_level = 1;
static int max_player_hp = 20;

static uint32_t prng_state = 0x9B4E3F1A;
static uint32_t rand_u32(void) {
    prng_state ^= prng_state << 13;
    prng_state ^= prng_state >> 17;
    prng_state ^= prng_state << 5;
    return prng_state;
}

static int rand_range(int min, int max) {
    if (max <= min) return min;
    return min + (rand_u32() % (max - min));
}

#pragma pack(push, 1)
typedef struct { int x, y, w, h; } Room;
#pragma pack(pop)
static Room rooms[15];
static int num_rooms = 0;

static void build_room(Room *r) {
    for (int y = r->y; y < r->y + r->h; y++) {
        for (int x = r->x; x < r->x + r->w; x++) {
            if (x >= 0 && x < MAP_W && y >= 0 && y < MAP_H) map[y][x] = 0; 
        }
    }
}

static void build_h_corridor(int x1, int x2, int y) {
    int start = x1 < x2 ? x1 : x2;
    int end = x1 > x2 ? x1 : x2;
    for (int x = start; x <= end; x++) {
        if (x >= 0 && x < MAP_W && y >= 0 && y < MAP_H) map[y][x] = 0;
    }
}

static void build_v_corridor(int y1, int y2, int x) {
    int start = y1 < y2 ? y1 : y2;
    int end = y1 > y2 ? y1 : y2;
    for (int y = start; y <= end; y++) {
        if (x >= 0 && x < MAP_W && y >= 0 && y < MAP_H) map[y][x] = 0;
    }
}

static int spawn_entity(int x, int y, int sprite, int is_player, int hp) {
    if (num_entities >= MAX_ENTITIES) return -1;
    int id = num_entities++;
    entities[id].x = x;
    entities[id].y = y;
    entities[id].sprite = sprite;
    entities[id].is_player = is_player;
    entities[id].hp = hp;
    entities[id].active = 1;
    return id;
}

static void generate_map(void) {
    num_entities = 0;
    num_rooms = 0;
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            map[y][x] = 1; // Wall
            seen[y][x] = 0; // Clear Fog
        }
    }
    
    int total_rooms = 5 + (current_level / 2);
    if (total_rooms > 12) total_rooms = 12;

    for (int i = 0; i < total_rooms; i++) {
        Room r;
        r.w = rand_range(4, 8);
        r.h = rand_range(3, 6);
        r.x = rand_range(1, MAP_W - r.w - 1);
        r.y = rand_range(1, MAP_H - r.h - 1);
        
        build_room(&r);
        
        if (num_rooms > 0) {
            Room prev = rooms[num_rooms - 1];
            int cx1 = r.x + r.w / 2;
            int cy1 = r.y + r.h / 2;
            int cx2 = prev.x + prev.w / 2;
            int cy2 = prev.y + prev.h / 2;
            
            if (rand_u32() % 2 == 0) {
                build_h_corridor(cx1, cx2, cy1);
                build_v_corridor(cy1, cy2, cx2);
            } else {
                build_v_corridor(cy1, cy2, cx1);
                build_h_corridor(cx1, cx2, cy2);
            }
        }
        rooms[num_rooms++] = r;
    }
    
    int px = rooms[0].x + rooms[0].w / 2;
    int py = rooms[0].y + rooms[0].h / 2;
    player_id = spawn_entity(px, py, SPRITE_PLAYER, 1, max_player_hp);
    
    for (int i = 1; i < num_rooms; i++) {
        int mx = rooms[i].x + rooms[i].w / 2;
        int my = rooms[i].y + rooms[i].h / 2;
        int monster_hp = 2 + (current_level / 2);
        spawn_entity(mx, my, SPRITE_MONSTER, 0, monster_hp);
    }
}

static int get_entity_at(int x, int y) {
    for (int i = 0; i < num_entities; i++) {
        if (entities[i].active && entities[i].x == x && entities[i].y == y) return i;
    }
    return -1;
}

static void try_move(int ent_id, int dx, int dy) {
    Entity *e = &entities[ent_id];
    int nx = e->x + dx;
    int ny = e->y + dy;
    
    if (nx < 0 || ny < 0 || nx >= MAP_W || ny >= MAP_H) return;
    if (map[ny][nx] == 1) return;
    
    int hit_id = get_entity_at(nx, ny);
    if (hit_id != -1) {
        if (e->is_player != entities[hit_id].is_player) {
            entities[hit_id].hp -= 1;
            if (entities[hit_id].hp <= 0) {
                entities[hit_id].active = 0;
                if (e->is_player) {
                    e->hp += 2;
                    if (e->hp > max_player_hp) e->hp = max_player_hp;
                } else if (entities[hit_id].is_player) {
                    game_state = STATE_GAMEOVER;
                }
            }
        }
        return; 
    }
    e->x = nx;
    e->y = ny;
}

static void process_turn(void) {
    int monsters_alive = 0;
    for (int i = 0; i < num_entities; i++) {
        if (!entities[i].active) continue;
        if (entities[i].is_player) continue;
        monsters_alive++;
        if (rand_u32() % 2 == 0) continue; 
        int r = rand_u32() % 4;
        int dx = 0, dy = 0;
        if (r == 0) dx = 1;
        else if (r == 1) dx = -1;
        else if (r == 2) dy = 1;
        else dy = -1;
        try_move(i, dx, dy);
    }
    if (monsters_alive == 0) {
        current_level++;
        int saved_hp = entities[player_id].hp;
        generate_map();
        entities[player_id].hp = saved_hp;
    }
}

static void update_fov(int px, int py) {
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            int dx = x - px;
            int dy = y - py;
            if (dx * dx + dy * dy <= 20) {
                seen[y][x] = 1;
            }
        }
    }
}

static void draw_rect(int x, int y, int w, int h, uint16_t color) {
    for (int iy = y; iy < y + h; iy++) {
        for (int ix = x; ix < x + w; ix++) {
            if (ix >= 0 && ix < (int)_sys->width && iy >= 0 && iy < (int)_sys->height) {
                _fb[iy * _sys->width + ix] = color;
            }
        }
    }
}

static void draw_tile(int tile_idx, int tx, int ty) {
    if (tile_idx < 0 || tile_idx > 63) return;
    int spritesheet_x = (tile_idx % 8) * 32;
    int spritesheet_y = (tile_idx / 8) * 32;
    int screen_px = tx * 32;
    int screen_py = ty * 32;
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            uint16_t pixel = sheet_ptr[(spritesheet_y + y) * 256 + (spritesheet_x + x)];
            if (pixel != 0) { 
                int scr_idx = (screen_py + y) * _sys->width + (screen_px + x);
                if (scr_idx >= 0 && scr_idx < (int)(_sys->width * _sys->height)) {
                    _fb[scr_idx] = pixel;
                }
            }
        }
    }
}

static void reset_game(void) {
    current_level = 1;
    game_state = STATE_PLAYING;
    generate_map();
}

__attribute__((visibility("default")))
int main() {
    if (_sys->width == 0) {
        init(320, 240, 320 * 240 * 2, 65536 * 4);
        reset_game();
    }

    if (game_state == STATE_GAMEOVER) {
        for (int i=0; i<(int)(_sys->width*_sys->height); i++) _fb[i] = RGB565(255, 0, 0);
        uint32_t pressed = _sys->gamepad_buttons & ~prev_buttons;
        prev_buttons = _sys->gamepad_buttons;
        if (pressed & BTN_START) reset_game(); 
        _sys->redraw = 1;
        return 0;
    }

    if (_sys->gamepad_buttons & BTN_SELECT) {
        for (int i=0; i<(int)(_sys->width*_sys->height); i++) _fb[i] = 0;
        int idx = 0;
        for (int ty = 0; ty < 8; ty++) {
            for (int tx = 0; tx < 8; tx++) {
                draw_tile(idx, tx + 1, ty);
                idx++;
            }
        }
        _sys->redraw = 1;
        return 0;
    }

    uint32_t pressed = _sys->gamepad_buttons & ~prev_buttons;
    prev_buttons = _sys->gamepad_buttons;
    int turn_taken = 0;
    int dx = 0, dy = 0;
    if (pressed & BTN_LEFT)  dx = -1;
    if (pressed & BTN_RIGHT) dx = 1;
    if (pressed & BTN_UP)    dy = -1;
    if (pressed & BTN_DOWN)  dy = 1;
    if (dx != 0 || dy != 0) {
        if (entities[player_id].active) {
            try_move(player_id, dx, dy);
            turn_taken = 1;
        }
    }
    if (turn_taken) process_turn();

    for (int i=0; i<(int)(_sys->width*_sys->height); i++) _fb[i] = 0;
    int px = entities[player_id].x;
    int py = entities[player_id].y;
    update_fov(px, py);
    int cam_x = px - VIEW_W / 2;
    int cam_y = py - VIEW_H / 2;
    if (cam_x < 0) cam_x = 0;
    if (cam_y < 0) cam_y = 0;
    if (cam_x > MAP_W - VIEW_W) cam_x = MAP_W - VIEW_W;
    if (cam_y > MAP_H - VIEW_H) cam_y = MAP_H - VIEW_H;

    for (int y = 0; y < VIEW_H; y++) {
        for (int x = 0; x < VIEW_W; x++) {
            int map_x = cam_x + x;
            int map_y = cam_y + y;
            if (map_x >= 0 && map_x < MAP_W && map_y >= 0 && map_y < MAP_H) {
                if (seen[map_y][map_x]) {
                    if (map[map_y][map_x] == 1) draw_tile(SPRITE_WALL, x, y);
                    else draw_tile(SPRITE_FLOOR, x, y);
                }
            }
        }
    }
    for (int i = 0; i < num_entities; i++) {
        if (entities[i].active) {
            int map_x = entities[i].x;
            int map_y = entities[i].y;
            int cx = map_x - px;
            int cy = map_y - py;
            if (cx * cx + cy * cy <= 20) {
                int sx = map_x - cam_x;
                int sy = map_y - cam_y;
                if (sx >= 0 && sx < VIEW_W && sy >= 0 && sy < VIEW_H) draw_tile(entities[i].sprite, sx, sy);
            }
        }
    }
    draw_rect(10, 10, max_player_hp * 6, 12, RGB565(255, 0, 0));
    int current_hp = entities[player_id].hp;
    if (current_hp > 0) draw_rect(10, 10, current_hp * 6, 12, RGB565(0, 255, 0));

    _sys->redraw = 1;
    return 0;
}
