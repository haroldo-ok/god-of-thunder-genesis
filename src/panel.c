// God of Thunder - Sega Genesis Port
// panel.c - HUD rendering (health bar, magic bar, jewels, keys, score, item)
//
// Ported from 1_panel.c. Key changes:
//   - xfillrectangle / xprint → VDP Window plane tile writes
//   - DOS status bar was 320×48 px at top of screen (y=0..47)
//   - Genesis: HUD lives in the VDP WINDOW plane at the bottom 32 lines
//     (y rows 24-27 in tile coordinates = pixel rows 192-223)
//   - Health / magic bars: drawn as rows of colored tiles (16 steps each)
//   - Numbers: drawn with VDP_drawText using the built-in SGDK font
//   - Object icon displayed via a dedicated HUD sprite

#include <genesis.h>
#include "god_of_thunder.h"

// ─── HUD layout constants (in VDP tile rows/cols on WINDOW plane) ─────────────
// The Window plane covers the bottom 32 pixels (rows 24-27 in 8-px tiles).
// We use rows 24-27 of BG_B for the HUD background (or the WINDOW plane).
#define HUD_TILE_ROW    24          // first VDP tile row of HUD (y=192)
#define HUD_COLS        40          // 40 tile columns = 320 px

// Bar positions (in pixels within the HUD strip, for reference):
//   DOS: health bar x=59..209, y=8..12
//        magic bar  x=59..209, y=20..24
//        jewels     x=59..85,  y=32..42
//        keys       x=94..120, y=32..42
//        score      x=223..279, y=32..42
//        item icon  x=280..296, y=0..16
// Genesis HUD (pixel coords within 320×32 HUD strip):
#define HUD_BAR_X       8           // bar left edge (px)
#define HUD_BAR_W       150         // bar width (px) = 150 units = 1px per HP/MP
#define HUD_HLTH_Y      2           // health bar row (pixels into HUD)
#define HUD_MAGIC_Y     10          // magic bar row
#define HUD_TEXT_Y      20          // text row (jewels / keys / score)

// ─── HUD tile indices (built-in SGDK solid-color tiles or custom tiles) ───────
// We use VDP_fillTileMapRect with colored tile attributes for bars.
// For simplicity: define two tile VRAM slots for "full" and "empty" bar pixels.
// These are 8×8 solid-color tiles — we generate them procedurally.
#define HUD_TILE_BASE   (BG_TILE_VRAM_BASE + 230*4 + 32*4 + 4)  // after bg + obj + hud border
#define HUD_TILE_SOLID   0    // tile 0 = solid (use palette color directly)

// ─── HUD border tile indices in bg tileset ────────────────────────────────────
// Original used tiles 192-199 for dialog box borders; same for HUD frame.
// We'll just draw a flat colored background for the HUD using SGDK text plane.

// ─── Cached last-drawn values (avoid redundant VDP writes) ───────────────────
static u8  last_health = 255;
static u8  last_magic  = 255;
static s16 last_jewels = -1;
static u8  last_keys   = 255;
static s32 last_score  = -1;
static u8  last_item   = 255;

// ─── Helper: draw a horizontal bar on the WINDOW plane ───────────────────────
// x_tile, y_tile: top-left in WINDOW tile coords
// filled_tiles:  number of tiles to fill with 'full' color
// total_tiles:   total bar width in tiles
// full_pal, empty_pal: palette line for filled / empty portions
static void draw_bar(s16 x_tile, s16 y_tile, s16 filled_tiles,
                     s16 total_tiles, u8 pal_full, u8 pal_empty) {
    s16 i;
    for (i = 0; i < total_tiles; i++) {
        u8 pal = (i < filled_tiles) ? pal_full : pal_empty;
        // Use tile index 1 (first tile in VRAM — a solid block from our tileset)
        VDP_setTileMapXY(WINDOW,
            TILE_ATTR_FULL(pal, 0, 0, 0, 1),
            (u16)(x_tile + i), (u16)y_tile);
    }
}

// ─── HUD initialisation ───────────────────────────────────────────────────────
void hud_init(void) {
    // Enable WINDOW plane at the bottom of the screen (rows 24-27)
    VDP_setWindowVPos(TRUE, 24);   // TRUE = window covers rows 24 onward

    // Fill HUD background with a dark color (PAL3 slot 1 = dark grey)
    VDP_fillTileMapRect(WINDOW, TILE_ATTR_FULL(PAL3, 0, 0, 0, 0),
                        0, 0, HUD_COLS, 4);

    // Draw static labels using SGDK built-in text
    VDP_setTextPalette(PAL3);
    VDP_drawText("HP",  1, HUD_TILE_ROW + 0);
    VDP_drawText("MP",  1, HUD_TILE_ROW + 1);
    VDP_drawText("J:",  1, HUD_TILE_ROW + 2);
    VDP_drawText("K:",  9, HUD_TILE_ROW + 2);
    VDP_drawText("SC:", 22, HUD_TILE_ROW + 2);

    // Force full redraw of dynamic elements
    last_health = 255; last_magic = 255;
    last_jewels = -1;  last_keys  = 255;
    last_score  = -1;  last_item  = 255;
}

// ─── Health bar ───────────────────────────────────────────────────────────────
void display_health(void) {
    if (last_health == thor->health) return;
    last_health = thor->health;
    // Bar: 19 tiles wide, each tile = ~5 HP (health 0-100)
    s16 filled = (s16)((s32)thor->health * 19 / 100);
    draw_bar(3, 0, filled, 19, PAL1, PAL3);   // PAL1=bright red, PAL3=dark
}

// ─── Magic bar ────────────────────────────────────────────────────────────────
void display_magic(void) {
    if (last_magic == thor_info.magic) return;
    last_magic = thor_info.magic;
    // Magic 0-150; bar 19 tiles
    s16 filled = (s16)((s32)thor_info.magic * 19 / 150);
    draw_bar(3, 1, filled, 19, PAL0, PAL3);   // PAL0=blue/green tones
}

// ─── Jewel count ──────────────────────────────────────────────────────────────
void display_jewels(void) {
    if (last_jewels == thor_info.jewels) return;
    last_jewels = thor_info.jewels;
    char s[8];
    intToStr((s32)thor_info.jewels, s, 1);
    // Clear field then write
    VDP_clearText(3, 2, 5);
    VDP_drawText(s, 3, 2);
}

// ─── Key count ────────────────────────────────────────────────────────────────
void display_keys(void) {
    if (last_keys == thor_info.keys) return;
    last_keys = thor_info.keys;
    char s[4];
    intToStr((s32)thor_info.keys, s, 1);
    VDP_clearText(11, 2, 3);
    VDP_drawText(s, 11, 2);
}

// ─── Score ────────────────────────────────────────────────────────────────────
void display_score(void) {
    if (last_score == thor_info.score) return;
    last_score = thor_info.score;
    char s[12];
    intToStr(thor_info.score, s, 1);
    VDP_clearText(25, 2, 8);
    VDP_drawText(s, 25, 2);
}

// ─── Magic item icon ──────────────────────────────────────────────────────────
// In DOS: an object tile (16×16 px) drawn at top-right of HUD.
// On Genesis: we use a dedicated HUD sprite positioned at (296, 192).
// The item sprite is one of the object tiles (index 26-31 = magic items).
extern Sprite *hud_item_spr;
Sprite *hud_item_spr = NULL;

void display_item(void) {
    if (last_item == thor_info.item) return;
    last_item = thor_info.item;

    // Show item name in HUD text area
    const char *name = "";
    if (thor_info.item >= 1 && thor_info.item <= 6) {
        extern const char *item_name[];
        name = item_name[thor_info.item - 1];
    }
    VDP_clearText(33, 2, 7);
    // Truncate to 7 chars to fit
    char s[8];
    s16 i;
    for (i = 0; i < 7 && name[i]; i++) s[i] = name[i];
    s[i] = 0;
    VDP_drawText(s, 33, 2);
}

// ─── Full HUD refresh ─────────────────────────────────────────────────────────
void update_hud(void) {
    // Only call display_* if values have changed (cached internally)
    display_health();
    display_magic();
    display_jewels();
    display_keys();
    display_score();
    display_item();
}
