// God of Thunder - Sega Genesis Port
// object.c - Pickup objects, magic items, and item use logic
//
// Ported from 1_object.c. Changes:
//   - xfput / xcopyd2d → draw_bg_tile() + SGDK sprite for dropped objects
//   - far pointers removed
//   - xpset / xpoint (pixel-level VGA ops) for lightning effect →
//     replaced with a sprite-based lightning flash
//   - timer_cnt busy-waits → game_pause() calls
//   - PAGE2 references → removed (no multi-page buffering on Genesis)
//   - key_flag[key_magic] → JOY magic button read from input.c

#include <genesis.h>
#include <string.h>
#include "god_of_thunder.h"

// ─── Externals ────────────────────────────────────────────────────────────────
extern u8   magic_cnt;
extern u8   magic_inform, carry_inform;
extern const char *item_name[];
extern const char *object_names[];

// ─── Object sprites (displayed on BG as tiles, not hardware sprites) ──────────
// On Genesis: we place pickup objects as extra tiles on BG_A using the
// obj_tiles tileset, and track them in object_map[].

void show_objects(s16 level_num) {
    s16 i, p;
    (void)level_num;

    memset(object_map,   0, 240);
    memset(object_index, 0, 240);

    for (i = 0; i < LEVEL_MAX_STATIC_OBJ; i++) {
        if (scrn.static_obj[i]) {
            s16 sx = scrn.static_x[i];
            s16 sy = scrn.static_y[i];
            // Draw object tile on top of background tile
            // obj_tiles index = static_obj - 1  (objects are 1-based)
            u8 obj_tile_idx = (u8)(scrn.static_obj[i] - 1);
            // obj tiles start at OBJ_TILE_VRAM_BASE in VRAM
            // Use a helper that draws from the object tileset
            draw_obj_tile(sx, sy, obj_tile_idx);

            p = sx + sy * 20;
            object_index[p] = (u8)i;
            object_map[p]   = scrn.static_obj[i];
        }
    }
}

// Draw a single 16×16 object tile on BG_A (using OBJ_TILE_VRAM_BASE)
void draw_obj_tile(s16 col, s16 row, u8 tile_idx) {
    u16 base  = (u16)(OBJ_TILE_VRAM_BASE + (u32)tile_idx * 4);
    u16 vcol  = (u16)(col * 2);
    u16 vrow  = (u16)(row * 2);
    VDP_setTileMapXY(BG_A, TILE_ATTR(PAL0, 0, 0, 0, base+0), vcol,   vrow);
    VDP_setTileMapXY(BG_A, TILE_ATTR(PAL0, 0, 0, 0, base+1), vcol+1, vrow);
    VDP_setTileMapXY(BG_A, TILE_ATTR(PAL0, 0, 0, 0, base+2), vcol,   vrow+1);
    VDP_setTileMapXY(BG_A, TILE_ATTR(PAL0, 0, 0, 0, base+3), vcol+1, vrow+1);
}

// ─── Drop object on ground (called when enemy dies) ───────────────────────────
static s16 _drop_obj(ACTOR *actr, s16 o) {
    s16 x, y, p;
    p = (actr->x + (actr->size_x / 2)) / 16 +
        (((actr->y + (actr->size_y / 2)) / 16) * 20);
    if (p < 0 || p >= 240) return 0;
    if (!object_map[p] && scrn.icon[p / 20][p % 20] >= 140) {
        object_map[p]   = (u8)o;
        object_index[p] = (u8)(27 + actr->actor_num);
        x = (p % 20);
        y = (p / 20);
        draw_obj_tile(x, y, (u8)(o - 1));
        return 1;
    }
    return 0;
}

s16 drop_object(ACTOR *actr) {
    s16 o;
    s16 r1 = rnd(100), r2 = rnd(100);
    if      (r1 < 25)   o = 5;
    else if (r1 & 1)    o = (r2 < 10) ? 1 : 2;
    else                o = (r2 < 10) ? 3 : 4;
    return _drop_obj(actr, o);
}

// ─── Add health (capped at 150) ───────────────────────────────────────────────
void add_health(s16 amount) {
    s16 h = (s16)thor->health + amount;
    if (h > 150) h = 150;
    if (h < 0)   h = 0;
    thor->health = (u8)h;
    display_health();
}

// ─── Add keys ────────────────────────────────────────────────────────────────
void add_keys(s16 amount) {
    s16 k = (s16)thor_info.keys + amount;
    if (k < 0)   k = 0;
    if (k > 99)  k = 99;
    thor_info.keys = (u8)k;
    display_keys();
}

// ─── Fill score to multiple of 5 ─────────────────────────────────────────────
void fill_score(s16 bonus) {
    add_score((s32)bonus);
}

// ─── Pick up object at tile position p ───────────────────────────────────────
void pick_up_object(s16 p) {
    s16 r, x, y;
    u8  obj = object_map[p];

    switch (obj) {
        case 1:   // red jewel
            if (thor_info.jewels >= 999) { cannot_carry_more(); return; }
            add_jewels(10); break;
        case 2:   // blue jewel
            if (thor_info.jewels >= 999) { cannot_carry_more(); return; }
            add_jewels(1); break;
        case 3:   // red potion
            if (thor_info.magic >= 150) { cannot_carry_more(); return; }
            add_magic(10); break;
        case 4:   // blue potion
            if (thor_info.magic >= 150) { cannot_carry_more(); return; }
            add_magic(3); break;
        case 5:   // good apple
            if (thor->health >= 150) { cannot_carry_more(); return; }
            play_sound(GULP, 0); add_health(5); break;
        case 6:   // bad apple
            play_sound(OW, 0); add_health(-10); break;
        case 7:   // key
            add_keys(1); break;
        case 8:   // treasure
            if (thor_info.jewels >= 999) { cannot_carry_more(); return; }
            add_jewels(50); break;
        case 9:   // trophy
            add_score(100); break;
        case 10:  // crown
            add_score(1000); break;

        case 12: case 13: case 14: case 15: case 16:
        case 17: case 18: case 19: case 20: case 21:
        case 22: case 23: case 24: case 25: case 26:
            // Carried objects (Hermit's Doll, etc.)
            if (obj == 13 && HERMIT_HAS_DOLL) return;
            thor->num_moves    = 1;
            hammer->num_moves  = 2;
            actor[2].used      = 0;
            shield_on          = 0;
            tornado_used       = 0;
            thor_info.inventory |= 64;
            thor_info.item      = 7;
            thor_info.object    = (u8)(obj - 11);
            display_item();
            thor_info.object_name = object_names[thor_info.object - 1];
            odin_speaks((s16)((obj - 12) + 501), (s16)(obj - 1));
            break;

        case 27: case 28: case 29: case 30: case 31: case 32:
            // Magic items
            hourglass_flag = thunder_flag = shield_on = 0;
            lightning_used = tornado_used = 0;
            hammer->num_moves = 2;
            thor->num_moves   = 1;
            actor[2].used     = 0;
            {
                u8 bit = (u8)(1 << (obj - 27));
                thor_info.inventory |= bit;
            }
            odin_speaks((s16)((obj - 27) + 516), (s16)(obj - 1));
            thor_info.item = (u8)(obj - 26);
            display_item();
            add_magic(150);
            fill_score(5);
            break;
    }

    x = p % 20;
    y = p / 20;
    ox = (s16)(x * 16);
    oy = (s16)(y * 16);
    of = 1;

    // Erase object tile — redraw bg tile underneath
    draw_bg_tile(x, y, scrn.bg_color);
    draw_bg_tile(x, y, scrn.icon[y][x]);

    r = 1;
    play_sound(YAH, 0);
    object_map[p] = 0;
    if (r) {
        if (object_index[p] < 30) scrn.static_obj[object_index[p]] = 0;
        object_index[p] = 0;
    }
}

// ─── Delete carried object (used after delivering to NPC) ────────────────────
void delete_object(void) {
    thor_info.inventory &= (u16)(~64u);
    thor_info.item = 1;
    display_item();
}

// ─── Not enough magic / can't carry more ─────────────────────────────────────
void not_enough_magic(void) {
    if (!magic_inform) odin_speaks(2006, 0);
    magic_inform = 1;
}

void cannot_carry_more(void) {
    if (!carry_inform) odin_speaks(2007, 0);
    carry_inform = 1;
}

// ─── Lightning effect ─────────────────────────────────────────────────────────
// DOS: pixel-level VGA line drawing, timer busy-waits.
// Genesis: flash PAL1 bright yellow for a few frames, then damage nearby enemies.
static void throw_lightning(void) {
    s16 i, x, y, ax, ay;

    // Flash effect: alternate PAL1 to yellow-white for 10 ticks
    for (i = 0; i < 10; i++) {
        if (i & 1) {
            u16 flash[16];
            s16 j;
            for (j = 0; j < 16; j++) flash[j] = 0x0EEE;  // bright white-yellow
            PAL_setPalette(PAL1, flash, DMA);
        } else {
            PAL_setPalette(PAL1, got_pal_thor, DMA);
        }
        game_pause(1);
    }
    PAL_setPalette(PAL1, got_pal_thor, DMA);
    play_sound(ELECTRIC, 1);

    x = thor->x + 7;
    y = thor->y + 7;
    for (i = 3; i < MAX_ACTORS; i++) {
        if (!actor[i].used) continue;
        ax = actor[i].x + (actor[i].size_x / 2);
        ay = actor[i].y + (actor[i].size_y / 2);
        if (abs(ax - x) < 30 && abs(ay - y) < 30) {
            actor[i].magic_hit  = 1;
            actor[i].vunerable  = 0;
            actor_damaged(&actor[i], 254);
        }
    }
}

// ─── Magic item use functions ─────────────────────────────────────────────────

// setup_actor is defined in init.c

static s16 use_apple(s16 flag) {
    if (thor->health == 150) return 0;
    if (flag && thor_info.magic > 0) {
        if (!apple_flag) {
            magic_cnt = 0;
            add_magic(-2); add_health(1);
            play_sound(ANGEL_SND, 0);
        } else if (magic_cnt > 8) {
            magic_cnt = 0;
            add_magic(-2); add_health(1);
        }
        apple_flag = 1;
        thor->num_moves   = 2;
        hammer->num_moves = 1;
    } else {
        apple_flag = 0;
        thor->num_moves = 1; hammer->num_moves = 2;
    }
    return 0;
}

static s16 use_lightning(s16 flag) {
    if (flag) {
        if (thor_info.magic > 14) {
            add_magic(-15);
            throw_lightning();
        } else {
            not_enough_magic();
            return 0;
        }
    }
    return 1;
}

static s16 use_boots(s16 flag) {
    if (flag) {
        if (thor_info.magic > 0) {
            if (magic_cnt > 8) { magic_cnt = 0; add_magic(-1); }
            thor->num_moves   = 2;
            hammer->num_moves = 1;
        } else {
            not_enough_magic();
            thor->num_moves = 1; hammer->num_moves = 2;
        }
    } else {
        thor->num_moves = 1; hammer->num_moves = 2;
    }
    return 0;
}

static s16 use_shield(s16 flag) {
    if (flag) {
        if (thor_info.magic) {
            if (!shield_on) {
                magic_cnt = 0;
                add_magic(-1);
                memcpy(&actor[2], &sparkle, sizeof(ACTOR)); // use sparkle as base
                setup_actor(&actor[2], 2, DIR_DOWN, thor->x, thor->y);
                actor[2].speed_count = 1;
                actor[2].speed       = 1;
                shield_on = 1;
            } else if (magic_cnt > 8) {
                magic_cnt = 0; add_magic(-1);
            }
            return 1;
        } else {
            not_enough_magic();
        }
    }
    if (shield_on) {
        actor[2].dead = 2; actor[2].used = 0;
        free_actor_sprite(&actor[2]);
        shield_on = 0;
    }
    return 0;
}

static s16 use_tornado(s16 flag) {
    if (flag) {
        if (thor_info.magic > 10) {
            if (!tornado_used && !actor[2].dead && magic_cnt > 20) {
                magic_cnt = 0; add_magic(-10);
                memcpy(&actor[2], &magic_item[0], sizeof(ACTOR));
                setup_actor(&actor[2], 2, DIR_DOWN, thor->x, thor->y);
                actor[2].last_dir = thor->dir;
                actor[2].move     = 16;
                tornado_used      = 1;
                play_sound(WIND_SND, 0);
            }
        } else if (!tornado_used) {
            not_enough_magic(); return 0;
        }
        if (magic_cnt > 8 && tornado_used) { magic_cnt = 0; add_magic(-1); }
        if (thor_info.magic < 1) {
            actor_destroyed(&actor[2]); tornado_used = 0;
            not_enough_magic(); return 0;
        }
        return 1;
    }
    return 0;
}

static s16 use_thunder_power(s16 flag) {
    if (flag && !thunder_flag) {
        if (thor_info.magic > 19) {
            add_magic(-20);
            thunder_flag = MAX_ACTORS;
            play_sound(THUNDER_SND, 1);
        } else {
            not_enough_magic(); return 0;
        }
    }
    return 1;
}

static s16 use_object_item(s16 flag) {
    if (!flag) return 0;
    if (!(thor_info.inventory & 64)) return 0;
    odin_speaks((s16)((thor_info.object - 1) + 5501), (s16)(thor_info.object - 1));
    return 1;
}

// ─── use_item: called every game tick ────────────────────────────────────────
void use_item(void) {
    static u8 flag = 0;
    u16 buttons = JOY_readJoypad(JOY_1);
    s16 kf  = (buttons & BUTTON_B) ? 1 : 0;
    s16 ret = 0;
    s16 mf  = magic_inform;

    if (!kf && tornado_used) {
        actor_destroyed(&actor[2]);
        tornado_used = 0;
    }

    switch (thor_info.item) {
        case 1: ret = use_apple(kf);         break;
        case 2: ret = use_lightning(kf);     break;
        case 3: ret = use_boots(kf);         break;
        case 4: ret = use_tornado(kf);       break;
        case 5: ret = use_shield(kf);        break;
        case 6: ret = use_thunder_power(kf); break;
        case 7: ret = use_object_item(kf);   break;
    }

    if (kf) {
        if (!ret && !flag) {
            if (mf) play_sound(BRAAPP, 0);
            flag = 1;
        }
    } else {
        flag = 0;
    }
}
