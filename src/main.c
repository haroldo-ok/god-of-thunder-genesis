// God of Thunder - Sega Genesis Port
// main.c - Game initialization and main loop
//
// Ported from 1_main.c. Key changes:
//   - main() → game_init() + game_loop() called from SGDK int main()
//   - DOS timer interrupt → SYS_setVBlankCallback + frame counter
//   - VGA page flip (xshowpage) → SPR_update() in VBlank, automatic
//   - DOS file I/O (fopen, movedata) → ROM array reads via get_level_ptr()
//   - farmalloc/farfree → removed; all state is static
//   - run_gotm() (launch GOT.EXE) → removed (single ROM, no menu launcher)
//   - Demo recording/playback → removed
//   - Thunder VGA flicker effect → palette flash via PAL_setColor

#include <genesis.h>
#include "god_of_thunder.h"

// ─── Global game state ────────────────────────────────────────────────────────
ACTOR  actor[MAX_ACTORS];
ACTOR  shot[MAX_ENEMIES];
ACTOR  enemy[MAX_ENEMIES];
ACTOR  explosion;
ACTOR  sparkle;
ACTOR *thor   = &actor[0];
ACTOR *hammer = &actor[1];

THOR_INFO  thor_info;
SETUP      setup;
LEVEL      scrn;

s16   current_level   = 23;   // Episode 1 start level
s16   new_level       = 23;
s16   new_level_tile  = 0;
u8    current_area    = 1;

u8    exit_flag       = 0;
u8    boss_dead       = 0;
u8    boss_active     = 0;
u8    game_over       = 0;

s16   thor_x1, thor_y1, thor_x2, thor_y2;
s16   thor_pos        = 0;
s16   max_shot        = 0;

u8    hourglass_flag  = 0;
u8    thunder_flag    = 0;
u8    shield_on       = 0;
u8    lightning_used  = 0;
u8    tornado_used    = 0;
u8    apple_flag      = 0;
u8    bomb_flag       = 0;
u8    switch_flag     = 0;
u8    shot_ok         = 1;
u8    level_type      = 0;
s16   restore_screen  = 0;
u8    warp_flag       = 0;
u8    warp_scroll     = 0;
u8    startup         = 1;
u8    cheat           = 0;
u8    area            = 1;

u8    object_map[240];
u8    object_index[240];
s16   ox = 0, oy = 0;
u8    of  = 0;

s16   rand1 = 0, rand2 = 0;

u8    cash1_inform = 0, cash2_inform = 0;
u8    door_inform  = 0, magic_inform = 0, carry_inform = 0;
u8    killgg_inform = 0;
u8    last_setup[32];

// ─── Internal timing state ────────────────────────────────────────────────────
static u8  logic_tick    = 0;
static u16 frame_counter = 0;


// ─── Random number generator ──────────────────────────────────────────────────
// Replaces DOS rand()/srand(); simple LCG sufficient for gameplay.
static u32 rng_state = 1234;
s16 rnd(s16 max) {
    rng_state = rng_state * 1664525u + 1013904223u;
    if (max <= 0) return 0;
    return (s16)((rng_state >> 16) % (u16)max);
}

// ─── Score / resource helpers ─────────────────────────────────────────────────
void add_score(s32 delta) {
    thor_info.score += delta;
    if (thor_info.score < 0) thor_info.score = 0;
    display_score();
}

void add_magic(s16 amount) {
    { s16 _m = (s16)thor_info.magic + amount; if(_m<0)_m=0; if(_m>150)_m=150; thor_info.magic=(u8)_m; }
    display_magic();
}

void add_jewels(s16 amount) {
    thor_info.jewels += amount;
    if (thor_info.jewels > 999) thor_info.jewels = 999;
    if (thor_info.jewels < 0)   thor_info.jewels = 0;
    display_jewels();
}

// ─── Thor position helper ─────────────────────────────────────────────────────
void set_thor_vars(void) {
    thor_x1 = thor->x;
    thor_y1 = thor->y + 4;          // collision box is inset 4px from top
    thor_x2 = thor->x + thor->width - 1;
    thor_y2 = thor->y + thor->height - 1;
    thor_pos = (thor->x / 16) + ((thor->y / 16) * 20);
    thor->center_x = (s8)(thor_pos % 20);
    thor->center_y = (s8)(thor_pos / 20);
}

// ─── Pause (busy-wait for N logic ticks) ─────────────────────────────────────
void game_pause(s16 delay_ticks) {
    s16 count = 0;
    while (count < delay_ticks) {
        SPR_update();
        SYS_doVBlankProcess();
        if (++logic_tick >= TICKS_PER_LOGIC) {
            logic_tick = 0;
            count++;
        }
    }
}

// ─── Thor dies and respawns at last checkpoint ────────────────────────────────
void thor_dies(void) {
    s16 i;

    // Hide all actors during death animation
    for (i = 0; i < MAX_ACTORS; i++) actor[i].show = 0;
    update_sprites();

    play_sound(DEAD, 1);
    thor_spins(0);

    thor->used = 1;

    // Flash palette for ~3 seconds (~60 logic ticks)
    for (i = 0; i < 60; i++) {
        game_pause(1);
        // Alternate palette between normal and dark each tick
        if (i & 1) {
            got_load_all_palettes();
        } else {
            // Darken all palettes briefly (simple: load black palette)
            u16 dark[16];
            memset(dark, 0, sizeof(dark));
            PAL_setPalette(PAL0, dark, DMA);
            PAL_setPalette(PAL1, dark, DMA);
            PAL_setPalette(PAL2, dark, DMA);
            PAL_setPalette(PAL3, dark, DMA);
        }
    }
    got_load_all_palettes();

    // Restore Thor to last checkpoint
    new_level = thor_info.last_screen;
    thor->x = (s16)((thor_info.last_icon % 20) * 16);
    thor->y = (s16)(((thor_info.last_icon / 20) * 16) - 1);
    if (thor->x < 1)  thor->x = 1;
    if (thor->y < 0)  thor->y = 0;

    thor->last_x[0] = thor->last_x[1] = thor->x;
    thor->last_y[0] = thor->last_y[1] = thor->y;
    thor->dir       = thor_info.last_dir;
    thor->last_dir  = thor_info.last_dir;
    thor->health    = thor_info.last_health;
    thor_info.magic  = thor_info.last_magic;
    thor_info.jewels = thor_info.last_jewels;
    thor_info.keys   = thor_info.last_keys;
    thor_info.score  = thor_info.last_score;
    thor_info.object = thor_info.last_object;
    thor_info.object_name = thor_info.last_object_name;
    thor_info.item   = thor_info.last_item;
    thor_info.inventory = thor_info.last_inventory;

    memcpy(&setup, last_setup, 32);

    thor->num_moves    = 1;
    thor->vunerable    = 60;
    thor->show         = 60;
    hourglass_flag = 0;
    apple_flag     = 0;
    bomb_flag      = 0;
    thunder_flag   = 0;
    lightning_used = 0;
    tornado_used   = 0;
    shield_on      = 0;

    music_resume();
    actor[1].used = 0;   // remove hammer
    actor[2].used = 0;   // remove shield
    thor->speed_count = 6;

    memcpy(&scrn, get_level_ptr(area, (u8)new_level), sizeof(LEVEL));

    display_health();
    display_magic();
    display_jewels();
    display_keys();
    display_score();
    display_item();
    show_level(new_level);
    set_thor_vars();
}

// ─── Thor death spin animation ────────────────────────────────────────────────
void thor_spins(s16 flag) {
    static const u8 da[4] = {0, 2, 1, 3};
    s16 i, d = 0, c = 0;
    (void)flag;

    actor[2].used = 0;
    thor->next = 0;

    for (i = 0; i < 60; i++) {
        thor->dir      = da[d];
        thor->last_dir = da[d];
        c++;
        if (c > 4) { d = (d + 1) & 3; c = 0; }

        if (shield_on) actor[2].used = 0;
        if (i == 59)   thor->used = 0;

        actor_set_anim(thor);
        if (shield_on) actor[2].used = 1;

        // Wait one logic tick
        SPR_update();
        SYS_doVBlankProcess();
    }
}

// ─── Load game state after loading a save ────────────────────────────────────
void setup_load(void) {
    thor->used = 1;
    new_level  = thor_info.last_screen;
    thor->x    = (s16)((thor_info.last_icon % 20) * 16);
    thor->y    = (s16)(((thor_info.last_icon / 20) * 16) - 1);
    if (thor->x < 1)  thor->x = 1;
    if (thor->y < 0)  thor->y = 0;
    thor->last_x[0] = thor->last_x[1] = thor->x;
    thor->last_y[0] = thor->last_y[1] = thor->y;
    thor->dir = thor_info.last_dir;
    thor->last_dir = thor_info.last_dir;
    thor->health = thor_info.last_health;
    thor_info.magic     = thor_info.last_magic;
    thor_info.jewels    = thor_info.last_jewels;
    thor_info.keys      = thor_info.last_keys;
    thor_info.score     = thor_info.last_score;
    thor_info.item      = thor_info.last_item;
    thor_info.inventory = thor_info.last_inventory;
    thor_info.object    = thor_info.last_object;
    thor_info.object_name = thor_info.last_object_name;
    thor->num_moves  = 1;
    thor->vunerable  = 60;
    thor->show       = 60;
    hourglass_flag = apple_flag = bomb_flag = 0;
    thunder_flag = lightning_used = tornado_used = shield_on = 0;
    actor[1].used = 0;
    actor[2].used = 0;
    thor->speed_count = 6;
    memcpy(&scrn, get_level_ptr(area, (u8)new_level), sizeof(LEVEL));
    display_health(); display_magic(); display_jewels();
    display_keys(); display_score(); display_item();
    current_level = new_level;
    show_level(new_level);
}

// ─── Initialization ───────────────────────────────────────────────────────────
void game_init(void) {
    startup = 1;

    // Zero all game state
    memset(actor,    0, sizeof(actor));
    memset(shot,     0, sizeof(shot));
    memset(enemy,    0, sizeof(enemy));
    memset(&thor_info, 0, sizeof(thor_info));
    memset(&setup,   0, sizeof(setup));
    memset(object_map,   0, sizeof(object_map));
    memset(object_index, 0, sizeof(object_index));

    thor   = &actor[0];
    hammer = &actor[1];

    // Default options
    setup.music      = 1;
    setup.sound      = 1;
    setup.scroll_flag = 1;
    setup.skill      = 1;    // normal
    setup.area       = 1;
    area             = 1;
    current_level    = 23;
    new_level        = 23;

    hourglass_flag = thunder_flag = shield_on = 0;
    lightning_used = tornado_used = 0;
    switch_flag = boss_dead = boss_active = 0;
    game_over = warp_flag = 0;
    exit_flag = 0;
    cheat     = 0;

    // SGDK VDP setup
    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    // Load palettes
    got_load_all_palettes();

    // Initialise sprite engine (SGDK)
    SPR_init();


    // Initialise subsystems
    sound_init();
    hud_init();

    // Run game-specific init (loads level data, actor templates, etc.)
    initialize();

    startup = 0;
}

// ─── Main game loop ───────────────────────────────────────────────────────────
void game_loop(void) {
    s16 i, loop;
    u16 buttons, prev_buttons = 0;

    new_level = current_level;
    memcpy(&scrn, get_level_ptr(area, (u8)new_level), sizeof(LEVEL));
    show_level(current_level);
    exit_flag = 0;
    thor->speed_count = 6;
    music_play(level_type, 1);
    fade_in();

    while (1) {
        // ── Wait for VBlank, update input, flush DMA ─────────────────────
        // SPR_update() queues sprite DMA, then SYS_doVBlankProcess() waits
        // for VBlank and also updates JOY_readJoypad() state buffers.
        SPR_update();
        SYS_doVBlankProcess();
        frame_counter++;

        // ── Run logic every TICKS_PER_LOGIC frames (~20 Hz) ──────────────
        if (++logic_tick < TICKS_PER_LOGIC) continue;
        logic_tick = 0;

        rand1 = rnd(100);
        rand2 = rnd(100);

        // ── Read joypad ───────────────────────────────────────────────────
        buttons = JOY_readJoypad(JOY_1);
        u16 pressed = buttons & ~prev_buttons;   // newly pressed this tick
        prev_buttons = buttons;

        // ── Thunder screen-shake (replaces VGA palette flicker) ───────────
        if (thunder_flag) {
            // Shake by offsetting the BG plane horizontally a few pixels
            s16 shake = (rand1 % 5) - 2;
            VDP_setHorizontalScroll(BG_A, shake);
            thunder_flag--;
            if ((thunder_flag < MAX_ACTORS) && thunder_flag > 2)
                if (actor[thunder_flag].used) {
                    actor[thunder_flag].vunerable = 0;
                    actor_damaged(&actor[thunder_flag], 20);
                }
        } else {
            VDP_setHorizontalScroll(BG_A, 0);
        }

        // ── Redraw tile under picked-up object ────────────────────────────
        if (of) {
            // Re-stamp the background tile at (ox, oy) onto the tilemap
            s16 tx = ox / TILE_W;
            s16 ty = oy / TILE_H;
            if (tx >= 0 && tx < LEVEL_COLS && ty >= 0 && ty < LEVEL_ROWS) {
                u8 tile_idx = scrn.icon[ty][tx];
                // Redraw 2×2 VDP tiles at (tx*2, ty*2)
                extern void draw_bg_tile(s16 col, s16 row, u8 tile_idx);
                draw_bg_tile(tx, ty, scrn.bg_color);
                draw_bg_tile(tx, ty, tile_idx);
            }
            of = 0;
        }

        // ── Deferred death ────────────────────────────────────────────────
        if (exit_flag == 2) {
            thor_dies();
            exit_flag = 0;
        }

        // ── Switch/rotate deferred actions ───────────────────────────────
        if (switch_flag) {
            if (switch_flag == 1) switch_icons();
            else if (switch_flag == 2) rotate_arrows();
            switch_flag = 0;
        }

        // ── Object pickup check ───────────────────────────────────────────
        thor_pos = (thor->x / 16) + ((thor->y / 16) * 20);
        if (thor_pos >= 0 && thor_pos < 240)
            if (object_map[thor_pos]) pick_up_object(thor_pos);

        shot_ok = 1;

        // ── Menu / in-game buttons ────────────────────────────────────────
        if (!boss_dead) {
            if (pressed & BUTTON_START) {
                // Pause menu (todo: option_menu())
            }
        }

        // ── Move all active actors ────────────────────────────────────────
        for (i = 0; i < MAX_ACTORS; i++) {
            if (!actor[i].used) continue;

            if (hourglass_flag) {
                // Hourglass freezes enemies (not Thor, hammer, or shield)
                if ((i > 2) && (!(actor[i].magic_hurts & HOURGLASS_MAGIC)))
                    continue;
            }

            actor[i].move_count = actor[i].num_moves;
            while (actor[i].move_count--) {
                move_actor(&actor[i]);
            }

            if (i == 0) set_thor_vars();
            if (new_level != current_level) goto level_changed;
        }
        goto after_move;

    level_changed:
        ; // fall through to level transition code below

    after_move:
        if (exit_flag == 2) {
            thor_dies();
            exit_flag = 0;
        }

        // ── Update sprite positions ───────────────────────────────────────
        update_sprites();

        // ── Level transition ──────────────────────────────────────────────
        if (current_level != new_level) {
            u8 prev_type = (u8)level_type;
            (void)prev_type;
            thor->show   = 0;
            hammer->used = 0;
            show_level(new_level);
            tornado_used = 0;
        }

        // ── Use magic item ────────────────────────────────────────────────
        use_item();

        // ── Boss cleared check ────────────────────────────────────────────
        if (boss_dead) {
            // Check if all boss actors (slots 3-6) are gone
            for (loop = 3; loop < 7; loop++)
                if (actor[loop].used) break;
            if (loop == 7) {
                exit_flag = 0;
                if (boss_active == 1) {
                    closing_sequence1();
                    boss_active = 0;
                }
                if (exit_flag) break;
            }
        }

        // ── Exit conditions ───────────────────────────────────────────────
        if (exit_flag == 1) break;
        if (exit_flag == 5) break;    // chapter end
    }

    // Save and return to title (no GOT.EXE on Genesis — loop back or show credits)
    if (!game_over) save_game();
    music_pause();
    fade_out();
}

// ─── SGDK entry point ─────────────────────────────────────────────────────────
bool main(bool hardReset) {
    (void)hardReset;
    game_init();
    game_loop();
    // After game ends: show title screen / loop
    while (1) {
        game_loop();
    }
    return 0;
}
