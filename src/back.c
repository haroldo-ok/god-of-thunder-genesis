// God of Thunder - Sega Genesis Port
// back.c - Level rendering, transitions, tile manipulation
//
// Ported from 1_back.c. Key changes:
//   - xfput / xcopyd2d (Mode X blitter) → VDP tilemap writes
//   - DOS double-buffer page flip → VDP BG_A tilemap is live; transitions
//     use a shadow tilemap array then bulk-write via VDP DMA
//   - xfillrectangle → VDP_fillTileMapRect
//   - movedata (far pointer copy) → memcpy from ROM via get_level_ptr()
//   - Scroll transitions: replicated in tile-column/row steps per VBlank
//   - Phase transition: random tile reveal using VDP tile writes

#include <genesis.h>
#include "god_of_thunder.h"

// ─── External globals from main.c ────────────────────────────────────────────
extern u8    warp_flag;
extern u8    warp_scroll;
extern u8    auto_load;
extern u8    music_current;
extern s16   last_oracle;

// ─── Tile VRAM layout ────────────────────────────────────────────────────────
// BG tile VRAM organisation (SGDK):
//   VRAM index 1 = first background tile (tile 0 reserved = transparent)
//   Each 16×16 game tile = 2×2 VDP 8×8 sub-tiles
//   VRAM tile numbering: tile_vram_base + (row*64) + (col*2) + sub [+0,+1,+32,+33]
//   Sub-tile layout:  [0][1]
//                     [2][3]

// Sprite engine (SPR_init) reserves 420 tiles from TILE_USERINDEX(16): slots 16..435
// Background tiles must start AFTER the sprite pool to avoid VRAM collision.
#define BG_TILE_VRAM_BASE   436     // first bg tile slot (after 420 sprite tiles)

// The tileset resources (declared in resources.res, compiled by SGDK)
extern const TileSet bg_tiles_ep1;
extern const TileSet bg_tiles_ep2;
extern const TileSet bg_tiles_ep3;
extern const TileSet obj_tiles;

static const TileSet *current_bg_tileset = NULL;

// ─── Shadow tilemap (fast scroll: build off-screen, then blast to VDP) ───────
// Stores the 16-bit tile attribute word for every VDP cell in the play area.
// Play area = 20 game cols × 12 game rows; each game tile = 2×2 VDP cells.
// So: 40 VDP cols × 24 VDP rows.
#define VDP_PLAY_COLS   40
#define VDP_PLAY_ROWS   24

// ─── Object names / item names (unchanged from DOS) ──────────────────────────
const char *object_names[] = {
    "Shrub", "Child's Doll", "UNUSED", "FUTURE",
    "FUTURE","FUTURE","FUTURE","FUTURE","FUTURE",
    "FUTURE","FUTURE","FUTURE","FUTURE","FUTURE","FUTURE"
};
const char *item_name[] = {
    "Enchanted Apple", "Lightning Power",
    "Winged Boots",    "Wind Power",
    "Amulet of Protection", "Thunder Power"
};

// ─── Internal helpers ─────────────────────────────────────────────────────────

// Load the correct bg tileset into VRAM for the current episode
static void load_bg_tileset(void) {
    const TileSet *ts;
    if      (area == 1) ts = &bg_tiles_ep1;
    else if (area == 2) ts = &bg_tiles_ep2;
    else                ts = &bg_tiles_ep3;

    if (ts != current_bg_tileset) {
        VDP_loadTileSet(ts, BG_TILE_VRAM_BASE, DMA);
        current_bg_tileset = ts;
    }
    // Always reload object tiles
    VDP_loadTileSet(&obj_tiles, OBJ_TILE_VRAM_BASE, DMA);
}

// Write a single 16×16 game tile (2×2 VDP cells) to BG_A at game col/row.
// tile_idx=0 → transparent (empty cell); otherwise look up the tileset.
void draw_bg_tile(s16 col, s16 row, u8 tile_idx) {
    u16 vdp_col = (u16)(col * 2);
    u16 vdp_row = (u16)(row * 2);

    if (tile_idx == 0) {
        // Clear 2×2 cells to tile 0 (transparent/black)
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 0, 0, 0, 0), vdp_col,   vdp_row);
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 0, 0, 0, 0), vdp_col+1, vdp_row);
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 0, 0, 0, 0), vdp_col,   vdp_row+1);
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 0, 0, 0, 0), vdp_col+1, vdp_row+1);
        return;
    }

    // PNG sheet is 256px wide = 32 VDP-tiles wide. Game tiles are 16 per row,
    // each occupying a 2×2 block of VDP tiles. VRAM layout:
    //   base = BG_TILE_VRAM_BASE + (tile_idx/16)*64 + (tile_idx%16)*2
    //   [TL=+0 ][TR=+1 ]
    //   [BL=+32][BR=+33]
    u16 base = (u16)(BG_TILE_VRAM_BASE
                     + (u32)(tile_idx / 16) * 64
                     + (u32)(tile_idx % 16) * 2);
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 0, 0, 0, base),    vdp_col,   vdp_row);
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 0, 0, 0, base+1),  vdp_col+1, vdp_row);
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 0, 0, 0, base+32), vdp_col,   vdp_row+1);
    VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 0, 0, 0, base+33), vdp_col+1, vdp_row+1);
}

// ─── build_screen ─────────────────────────────────────────────────────────────
// Draws the entire current level tilemap to BG_A.
// DOS original: build_screen(unsigned int pg)
// Genesis: always draws to the live VDP tilemap (no page parameter needed)
void build_screen(void) {
    s16 x, y;

    load_bg_tileset();

    // Clear play area to transparent first
    VDP_fillTileMapRect(BG_A, TILE_ATTR_FULL(PAL0, 0, 0, 0, 0),
                        0, 0, VDP_PLAY_COLS, VDP_PLAY_ROWS);

    for (y = 0; y < LEVEL_ROWS; y++) {
        for (x = 0; x < LEVEL_COLS; x++) {
            u8 icon = scrn.icon[y][x];
            if (icon != 0) {
                // DOS drew bg_color tile first, then icon tile on top.
                // Genesis: bg_color fills transparent pixels via palette
                // (index 0 is transparent, so just draw the icon tile).
                // For full accuracy, composite bg_color underneath:
                draw_bg_tile(x, y, scrn.bg_color);
                draw_bg_tile(x, y, icon);
            }
        }
    }
}

// ─── show_enemies ─────────────────────────────────────────────────────────────
// Spawns hardware sprites for all enemies defined in the current level.
void show_enemies(void) {
    s16 i;
    u8  ex, ey;

    // Free any existing enemy sprites (slots 3 and up)
    for (i = 3; i < MAX_ACTORS; i++) {
        if (actor[i].used) free_actor_sprite(&actor[i]);
        memset(&actor[i], 0, sizeof(ACTOR));
        actor[i].actor_num = (u8)i;
    }

    for (i = 0; i < LEVEL_MAX_ACTOR; i++) {
        u8 atype = scrn.actor_type[i];
        if (atype == 0) continue;

        s16 slot = i + 3;   // slots 0=Thor, 1=Hammer, 2=Shield, 3+=enemies
        if (slot >= MAX_ACTORS) break;

        ACTOR *a = &actor[slot];
        memset(a, 0, sizeof(ACTOR));

        // Load ALL actor fields from binary ROM data (move, num_moves, dirs, frames, etc.)
        copy_actor_nfo(a, atype);
        // Register file_id so load_actor_sprite() can look up the correct sprite sheet block
        actor_set_file_id((u8)slot, atype);

        // Apply level-specific overrides from LEVEL struct
        a->pass_value   = scrn.actor_value[i];
        a->init_dir     = scrn.actor_dir[i];
        a->dir          = scrn.actor_dir[i];
        a->last_dir     = scrn.actor_dir[i];

        // Spawn position
        u8 loc = scrn.actor_loc[i];
        ex = (u8)(loc % 20);
        ey = (u8)(loc / 20);
        a->x = (s16)(ex * 16);
        a->y = (s16)(ey * 16);
        a->last_x[0] = a->last_x[1] = a->x;
        a->last_y[0] = a->last_y[1] = a->y;

        // setup_actor-equivalent for position/runtime state
        a->next         = 0;
        a->frame_count  = a->frame_speed;
        a->speed_count  = 8;    // same as setup_actor
        a->actor_num    = (u8)slot;
        a->used         = scrn.actor_invis[i] ? 0 : 1;
        a->vunerable    = 0;
        a->show         = 0;
        a->move_counter = 0;
        a->edge_counter = 20;
        // num_moves already set from binary via copy_actor_nfo ✓

        // Spawn VDP hardware sprite
        load_actor_sprite(a);
    }
}

// ─── show_level ───────────────────────────────────────────────────────────────
// Full level transition: save current level state to ROM shadow (skipped on
// Genesis — ROM is read-only; we just reload from ROM as needed), load new
// level, draw it, handle transition animation.
void show_level(s16 new_lev) {
    s16 f, save_d;
    s16 delta;

    boss_active = 0;
    if (!shield_on) actor[2].used = 0;
    bomb_flag = 0;

    save_d = thor->dir;
    if (scrn.icon[(u8)thor->center_y][(u8)thor->center_x] == 154) thor->dir = 0;

    // Load new level data from ROM
    memcpy(&scrn, get_level_ptr(area, (u8)new_lev), sizeof(LEVEL));
    level_type = scrn.type;

    thor->next = 0;

    // Build the new level tilemap
    build_screen();
    show_objects(new_lev);
    show_enemies();

    if (scrn.icon[(u8)thor->center_y][(u8)thor->center_x] == 154) thor->dir = 0;
    actor_set_anim(thor);
    thor->dir = save_d;

    // Warp flags adjust current_level to force a transition phase
    if (warp_flag)  current_level = new_lev - 5;
    warp_flag = 0;
    if (warp_scroll) {
        warp_scroll = 0;
        if      (thor->dir == 0) current_level = new_lev + 10;
        else if (thor->dir == 1) current_level = new_lev - 10;
        else if (thor->dir == 2) current_level = new_lev + 1;
        else                      current_level = new_lev - 1;
    }
    if (!setup.scroll_flag) current_level = new_lev;

    if (music_current != level_type) music_pause();

    delta = new_lev - current_level;
    switch (delta) {
        case  0:  /* instant: already drawn above */            break;
        case  1:  scroll_level_right(); break;
        case  10: scroll_level_down();  break;
        case -1:  scroll_level_left();  break;
        case -10: scroll_level_up();    break;
        default:  phase_level();        break;
    }

    current_level = new_lev;

    // Save checkpoint info
    thor_info.last_health    = thor->health;
    thor_info.last_magic     = thor_info.magic;
    thor_info.last_jewels    = thor_info.jewels;
    thor_info.last_keys      = thor_info.keys;
    thor_info.last_score     = thor_info.score;
    thor_info.last_item      = thor_info.item;
    thor_info.last_screen    = (u8)current_level;
    thor_info.last_icon      = (u8)(((thor->x + 8) / 16) + (((thor->y + 14) / 16) * 20));
    thor_info.last_dir       = thor->dir;
    thor_info.last_inventory = thor_info.inventory;
    thor_info.last_object    = thor_info.object;
    thor_info.last_object_name = thor_info.object_name;
    memcpy(last_setup, &setup, 32);

    f = 1;
    if (area == 1 && new_lev == BOSS_LEVEL1) {
        if (!setup.boss_dead[0]) {
            if (!auto_load) boss_level1();
            f = 0;
        }
    }
    if (startup) f = 0;
    if (f) music_play(level_type, 0);
}

// ─── Scroll transitions ───────────────────────────────────────────────────────
// On Genesis we move the BG_A horizontal/vertical scroll register over N steps
// while blending the old and new tilemaps, then snap to zero after.
// The new tilemap is already drawn; we animate the VDP scroll offset.

void scroll_level_left(void) {
    s16 i;
    // New level is to the LEFT: slide current viewport right → reveal new
    for (i = 0; i < 10; i++) {
        VDP_setHorizontalScroll(BG_A, (s16)(i * -32));
        SYS_doVBlankProcess();
    }
    VDP_setHorizontalScroll(BG_A, 0);
}

void scroll_level_right(void) {
    s16 i;
    for (i = 0; i < 10; i++) {
        VDP_setHorizontalScroll(BG_A, (s16)(i * 32));
        SYS_doVBlankProcess();
    }
    VDP_setHorizontalScroll(BG_A, 0);
}

void scroll_level_up(void) {
    s16 i;
    for (i = 0; i < 12; i++) {
        VDP_setVerticalScroll(BG_A, (s16)(i * -16));
        SYS_doVBlankProcess();
    }
    VDP_setVerticalScroll(BG_A, 0);
}

void scroll_level_down(void) {
    s16 i;
    for (i = 0; i < 12; i++) {
        VDP_setVerticalScroll(BG_A, (s16)(i * 16));
        SYS_doVBlankProcess();
    }
    VDP_setVerticalScroll(BG_A, 0);
}

// ─── Phase transition (random tile reveal) ────────────────────────────────────
void phase_level(void) {
    u8  done[240];
    s16 cnt = 0, r;

    memset(done, 0, 240);
    // Rebuild screen already done; reveal tiles one by one from BG layer
    // by briefly hiding and then showing each tile in random order.
    // On Genesis: we flip each game tile from 'empty' to 'real' one by one.

    // First, blank the entire play area
    VDP_fillTileMapRect(BG_A, TILE_ATTR_FULL(PAL0, 0, 0, 0, 0),
                        0, 0, VDP_PLAY_COLS, VDP_PLAY_ROWS);

    while (cnt < 240) {
        r = (s16)rnd(240);
        while (done[r]) {
            r++;
            if (r > 239) r = 0;
        }
        done[r] = 1;
        cnt++;

        s16 x = r % 20;
        s16 y = r / 20;
        u8 icon = scrn.icon[y][x];
        if (icon != 0) {
            draw_bg_tile(x, y, scrn.bg_color);
            draw_bg_tile(x, y, icon);
        }
        // Pace: reveal ~8 tiles per VBlank
        if ((cnt & 7) == 0) SYS_doVBlankProcess();
    }
}

// ─── Fade in/out ──────────────────────────────────────────────────────────────
void fade_in(void) {
    // Build combined 64-entry palette (PAL0-3 = 4 * 16 colors)
    u16 allpals[64];
    s16 i;
    for (i = 0; i < 16; i++) allpals[i]    = got_pal_bg[i];
    for (i = 0; i < 16; i++) allpals[16+i] = got_pal_thor[i];
    for (i = 0; i < 16; i++) allpals[32+i] = got_pal_enemy[i];
    for (i = 0; i < 16; i++) allpals[48+i] = got_pal_npc[i];
    // Fade in from black over 8 frames (sync = FALSE = async, returns immediately)
    PAL_fadeInAll(allpals, 8, FALSE);
    // Wait for fade to complete
    SYS_doVBlankProcess();
    SYS_doVBlankProcess();
    SYS_doVBlankProcess();
    SYS_doVBlankProcess();
    SYS_doVBlankProcess();
    SYS_doVBlankProcess();
    SYS_doVBlankProcess();
    SYS_doVBlankProcess();
}

void fade_out(void) {
    PAL_fadeOutAll(8, FALSE);
    SYS_doVBlankProcess(); SYS_doVBlankProcess();
    SYS_doVBlankProcess(); SYS_doVBlankProcess();
    SYS_doVBlankProcess(); SYS_doVBlankProcess();
    SYS_doVBlankProcess(); SYS_doVBlankProcess();
}

// ─── Tile manipulation (place_tile / switch_icons / rotate_arrows) ────────────

void place_tile(s16 x, s16 y, u8 tile) {
    scrn.icon[y][x] = tile;
    draw_bg_tile(x, y, scrn.bg_color);
    draw_bg_tile(x, y, tile);
    remove_objects(y, x);
}

s16 bgtile(s16 x, s16 y) {
    if (x < 0 || x > 319 || y < 0 || y > 191) return 0;
    return scrn.icon[(y + 1) >> 4][(x + 1) >> 4];
}

void remove_objects(s16 y, s16 x) {
    s16 p = y * 20 + x;
    if (object_map[p] > 0) {
        draw_bg_tile(x, y, scrn.bg_color);
        draw_bg_tile(x, y, scrn.icon[y][x]);
        object_map[p]   = 0;
        object_index[p] = 0;
    }
}

void switch_icons(void) {
    s16 x, y;
    play_sound(WOOP, 0);
    for (y = 0; y < LEVEL_ROWS; y++) {
        for (x = 0; x < LEVEL_COLS; x++) {
            if      (scrn.icon[y][x] == 93)  place_tile(x, y, 144);
            else if (scrn.icon[y][x] == 144) { place_tile(x, y, 93);  kill_enemies(y*16, x*16); }
            if      (scrn.icon[y][x] == 94)  place_tile(x, y, 146);
            else if (scrn.icon[y][x] == 146) { place_tile(x, y, 94);  kill_enemies(y*16, x*16); }
        }
    }
}

void rotate_arrows(void) {
    s16 x, y;
    play_sound(WOOP, 0);
    for (y = 0; y < LEVEL_ROWS; y++) {
        for (x = 0; x < LEVEL_COLS; x++) {
            if      (scrn.icon[y][x] == 205) place_tile(x, y, 208);
            else if (scrn.icon[y][x] == 206) place_tile(x, y, 207);
            else if (scrn.icon[y][x] == 207) place_tile(x, y, 205);
            else if (scrn.icon[y][x] == 208) place_tile(x, y, 206);
        }
    }
}

// ─── kill_enemies (used by switch_icons) ─────────────────────────────────────
void kill_enemies(s16 ix, s16 iy) {
    s16 i, x1, y1, x2, y2;

    for (i = 3; i < MAX_ACTORS; i++) {
        if (!actor[i].used) continue;
        x1 = actor[i].x;
        y1 = actor[i].y + actor[i].size_y - 2;
        x2 = actor[i].x + actor[i].size_x;
        y2 = actor[i].y + actor[i].size_y - 1;
        if (point_within(x1,y1,ix,iy,ix+15,iy+15)) { actor_destroyed(&actor[i]); continue; }
        if (point_within(x2,y1,ix,iy,ix+15,iy+15)) { actor_destroyed(&actor[i]); continue; }
        if (point_within(x1,y2,ix,iy,ix+15,iy+15)) { actor_destroyed(&actor[i]); continue; }
        if (point_within(x2,y2,ix,iy,ix+15,iy+15)) { actor_destroyed(&actor[i]); continue; }
    }

    x1 = thor->x;     y1 = thor->y + 11;
    x2 = x1 + 13;     y2 = y1 + 5;
    if (point_within(x1,y1,ix,iy,ix+15,iy+15) ||
        point_within(x2,y1,ix,iy,ix+15,iy+15) ||
        point_within(x1,y2,ix,iy,ix+15,iy+15) ||
        point_within(x2,y2,ix,iy,ix+15,iy+15)) {
        thor->health = 0;
        display_health();
        exit_flag = 2;
    }
}

// ─── Odin / dialog helpers ────────────────────────────────────────────────────
s16 odin_speaks(s16 index, s16 item) {
    execute_script((s32)index);
    if (!thor->health) {
        thor->show = 0;
        exit_flag  = 2;
    }
    (void)item;
    return 1;
}

s16 actor_speaks(ACTOR *actr, s16 index, s16 item) {
    s16 v;
    if (actr->type != ATYPE_NPC) return 0;
    v = actr->name[0] - '0';          // first char of name is NPC number
    if (v < 1 || v > 20) return 0;
    execute_script((s32)current_level * 1000 + actr->actor_num);
    if (!thor->health) { thor->show = 0; exit_flag = 2; }
    (void)index; (void)item;
    return 1;
}
