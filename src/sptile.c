// God of Thunder - Sega Genesis Port
// sptile.c - Special tile interactions (doors, warps, holes, arrows)
//
// Ported from 1_sptile.c. Pure logic — only xfput/xcopyd2d calls replaced
// with draw_bg_tile(). All gameplay semantics preserved exactly.

#include <genesis.h>
#include "god_of_thunder.h"

extern u8  diag;
extern u8  end_tile;
extern s16 thor_real_y1;

static s16 open_door1(s16 y, s16 x);
static s16 cash_door1(s16 y, s16 x, s16 amount);

// ─── Erase a door tile and replace with bg ───────────────────────────────────
void erase_door(s16 x, s16 y) {
    play_sound(DOOR_SND, 0);
    scrn.icon[y][x] = scrn.bg_color;
    draw_bg_tile(x, y, scrn.bg_color);
}

// ─── Open a key-locked door ──────────────────────────────────────────────────
static s16 open_door1(s16 y, s16 x) {
    if (thor_info.keys > 0) {
        erase_door(x, y);
        thor_info.keys--;
        display_keys();
        return 1;
    }
    if (!door_inform) { odin_speaks(2003, 0); door_inform = 1; }
    return 0;
}

// ─── Open a jewel-cost door ───────────────────────────────────────────────────
static s16 cash_door1(s16 y, s16 x, s16 amount) {
    if (thor_info.jewels >= amount) {
        erase_door(x, y);
        thor_info.jewels = (s16)(thor_info.jewels - amount);
        display_jewels();
        return 1;
    }
    if (amount == 10  && !cash1_inform) { odin_speaks(2005, 0); cash1_inform = 1; }
    if (amount == 100 && !cash2_inform) { odin_speaks(2004, 0); cash2_inform = 1; }
    return 0;
}

// ─── special_tile_thor: called when Thor steps on a special tile ──────────────
// Returns 1 if Thor can pass through, 0 if blocked/handled.
s16 special_tile_thor(s16 x, s16 y, s16 icon) {
    s16 cx, cy, f = 0;

    switch (icon) {
        case 201: return open_door1(x, y);
        case 202:
            if (thor->x > 300) end_tile = 1;
            return 1;
        case 203: return 0;
        case 204: return 0;
        case 205: if (!diag && thor->dir != DIR_DOWN)  return 1; break;
        case 206: if (!diag && thor->dir != DIR_UP)    return 1; break;
        case 207: if (!diag && thor->dir != DIR_RIGHT) return 1; break;
        case 208: if (!diag && thor->dir != DIR_LEFT)  return 1; break;
        case 209: return cash_door1(x, y, 10);
        case 210: return cash_door1(x, y, 100);
        case 211:
            place_tile(y, x, 79);
            exit_flag = 2;
            return 1;
        case 212:
        case 213: return 0;
        case 214: case 215: case 216: case 217: return 0;
        case 218: case 219:
            f = 1;
            /* fall through */
        case 220: case 221: case 222: case 223:
        case 224: case 225: case 226: case 227:
        case 228: case 229: {
            cx = (thor_x1 + 7) / 16;
            cy = (thor_real_y1 + 8) / 16;
            if (scrn.icon[cy][cx] == (u8)icon) {
                thor->vunerable = STAMINA;
                if (icon < 224 && icon > 219) play_sound(FALL_SND, 0);
                new_level = scrn.new_level[icon - 220 + (f * 6)];
                warp_scroll = 0;
                if (new_level > 119) {
                    warp_scroll = 1;
                    new_level  -= 128;
                }
                new_level_tile = scrn.new_level_loc[icon - 220 + (f * 6)];
                warp_flag = 1;
                if (warp_scroll) {
                    if      (thor->dir == DIR_UP)    thor->y = 175;
                    else if (thor->dir == DIR_DOWN)  thor->y = 0;
                    else if (thor->dir == DIR_LEFT)  thor->x = 304;
                    else                              thor->x = 0;
                } else {
                    thor->x = (s16)((new_level_tile % 20) * 16);
                    thor->y = (s16)(((new_level_tile / 20) * 16) - 2);
                }
                thor->last_x[0] = thor->last_x[1] = thor->x;
                thor->last_y[0] = thor->last_y[1] = thor->y;
                return 0;
            }
            return 1;
        }
    }
    return 0;
}

// ─── special_tile: called when an enemy steps on a special tile ───────────────
s16 special_tile(ACTOR *actr, s16 x, s16 y, s16 icon) {
    (void)actr; (void)x; (void)y;
    switch (icon) {
        case 201: case 202: case 203: case 204: break;
        case 205: case 206: case 207: case 208: return 1;
        case 209: case 210: return 0;
        case 214: case 215: case 216: case 217: return 0;
        case 224: case 225: case 226: case 227:
            if (!actr->flying) return 0;
            return 1;
        default: return 1;
    }
    return 0;
}
