// God of Thunder - Sprite Sheet Block Index Lookup
// Auto-generated - maps actor_id to block_idx in its sprite sheet.
// block_idx is used to compute SGDK anim/frame:
//   sheet_cols = number of actor blocks per row in the sprite sheet
//   anim  = (block_idx / sheet_cols) * MAX_DIRS + direction
//   frame = (block_idx % sheet_cols) * MAX_FRAMES + frame_idx
#ifndef SPRITE_BLOCKS_H
#define SPRITE_BLOCKS_H

#define SHEET_COLS_THOR   8
#define SHEET_COLS_HAMMER 8
#define SHEET_COLS_FX     8
#define SHEET_COLS_ENEMY  16
#define SHEET_COLS_NPC    16
#define MAX_DIRS_PER_BLOCK  4
#define MAX_FRAMES_PER_DIR  4

// Returns (block_idx << 8) | sheet_cols for a given actor_id.
// Returns 0xFFFF if actor_id not found in any sheet.
static inline unsigned short actor_sheet_block(unsigned char actor_id) {
    switch (actor_id) {
        case  98: return (0 << 8) | 8;  /* thor block 0 */
        case 100: return (1 << 8) | 8;  /* thor block 1 */
        case 101: return (2 << 8) | 8;  /* thor block 2 */
        case 102: return (3 << 8) | 8;  /* thor block 3 */
        case 110: return (4 << 8) | 8;  /* thor block 4 */
        case 103: return (0 << 8) | 8;  /* hammer block 0 */
        case 104: return (1 << 8) | 8;  /* hammer block 1 */
        case 105: return (2 << 8) | 8;  /* hammer block 2 */
        case 113: return (3 << 8) | 8;  /* hammer block 3 */
        case 106: return (0 << 8) | 8;  /* fx block 0 */
        case 107: return (1 << 8) | 8;  /* fx block 1 */
        case 108: return (2 << 8) | 8;  /* fx block 2 */
        case 109: return (3 << 8) | 8;  /* fx block 3 */
        case   1: return (0 << 8) | 16;  /* enemy block 0 */
        case   2: return (1 << 8) | 16;  /* enemy block 1 */
        case   3: return (2 << 8) | 16;  /* enemy block 2 */
        case   4: return (3 << 8) | 16;  /* enemy block 3 */
        case   5: return (4 << 8) | 16;  /* enemy block 4 */
        case   6: return (5 << 8) | 16;  /* enemy block 5 */
        case   7: return (6 << 8) | 16;  /* enemy block 6 */
        case   8: return (7 << 8) | 16;  /* enemy block 7 */
        case   9: return (8 << 8) | 16;  /* enemy block 8 */
        case  10: return (9 << 8) | 16;  /* enemy block 9 */
        case  11: return (10 << 8) | 16;  /* enemy block 10 */
        case  12: return (11 << 8) | 16;  /* enemy block 11 */
        case  13: return (12 << 8) | 16;  /* enemy block 12 */
        case  14: return (13 << 8) | 16;  /* enemy block 13 */
        case  15: return (14 << 8) | 16;  /* enemy block 14 */
        case  16: return (15 << 8) | 16;  /* enemy block 15 */
        case  17: return (16 << 8) | 16;  /* enemy block 16 */
        case  18: return (17 << 8) | 16;  /* enemy block 17 */
        case  19: return (18 << 8) | 16;  /* enemy block 18 */
        case  20: return (19 << 8) | 16;  /* enemy block 19 */
        case  21: return (20 << 8) | 16;  /* enemy block 20 */
        case  22: return (21 << 8) | 16;  /* enemy block 21 */
        case  23: return (22 << 8) | 16;  /* enemy block 22 */
        case  24: return (23 << 8) | 16;  /* enemy block 23 */
        case  25: return (24 << 8) | 16;  /* enemy block 24 */
        case  26: return (25 << 8) | 16;  /* enemy block 25 */
        case  27: return (26 << 8) | 16;  /* enemy block 26 */
        case  28: return (27 << 8) | 16;  /* enemy block 27 */
        case  29: return (28 << 8) | 16;  /* enemy block 28 */
        case  31: return (29 << 8) | 16;  /* enemy block 29 */
        case  32: return (30 << 8) | 16;  /* enemy block 30 */
        case  33: return (31 << 8) | 16;  /* enemy block 31 */
        case  34: return (32 << 8) | 16;  /* enemy block 32 */
        case  38: return (33 << 8) | 16;  /* enemy block 33 */
        case  39: return (34 << 8) | 16;  /* enemy block 34 */
        case  40: return (35 << 8) | 16;  /* enemy block 35 */
        case  59: return (36 << 8) | 16;  /* enemy block 36 */
        case  60: return (37 << 8) | 16;  /* enemy block 37 */
        case  61: return (38 << 8) | 16;  /* enemy block 38 */
        case  62: return (39 << 8) | 16;  /* enemy block 39 */
        case  64: return (40 << 8) | 16;  /* enemy block 40 */
        case  65: return (41 << 8) | 16;  /* enemy block 41 */
        case  66: return (42 << 8) | 16;  /* enemy block 42 */
        case  67: return (43 << 8) | 16;  /* enemy block 43 */
        case  68: return (44 << 8) | 16;  /* enemy block 44 */
        case  69: return (45 << 8) | 16;  /* enemy block 45 */
        case  70: return (46 << 8) | 16;  /* enemy block 46 */
        case  71: return (47 << 8) | 16;  /* enemy block 47 */
        case  74: return (48 << 8) | 16;  /* enemy block 48 */
        case  99: return (49 << 8) | 16;  /* enemy block 49 */
        case  35: return (0 << 8) | 16;  /* npc block 0 */
        case  36: return (1 << 8) | 16;  /* npc block 1 */
        case  37: return (2 << 8) | 16;  /* npc block 2 */
        case  41: return (3 << 8) | 16;  /* npc block 3 */
        case  42: return (4 << 8) | 16;  /* npc block 4 */
        case  43: return (5 << 8) | 16;  /* npc block 5 */
        case  44: return (6 << 8) | 16;  /* npc block 6 */
        case  45: return (7 << 8) | 16;  /* npc block 7 */
        case  46: return (8 << 8) | 16;  /* npc block 8 */
        case  47: return (9 << 8) | 16;  /* npc block 9 */
        case  48: return (10 << 8) | 16;  /* npc block 10 */
        case  49: return (11 << 8) | 16;  /* npc block 11 */
        case  50: return (12 << 8) | 16;  /* npc block 12 */
        case  51: return (13 << 8) | 16;  /* npc block 13 */
        case  52: return (14 << 8) | 16;  /* npc block 14 */
        case  53: return (15 << 8) | 16;  /* npc block 15 */
        case  54: return (16 << 8) | 16;  /* npc block 16 */
        case  55: return (17 << 8) | 16;  /* npc block 17 */
        case  56: return (18 << 8) | 16;  /* npc block 18 */
        case  57: return (19 << 8) | 16;  /* npc block 19 */
        case  58: return (20 << 8) | 16;  /* npc block 20 */
        case  63: return (21 << 8) | 16;  /* npc block 21 */
        case  72: return (22 << 8) | 16;  /* npc block 22 */
        case  73: return (23 << 8) | 16;  /* npc block 23 */
        case  75: return (24 << 8) | 16;  /* npc block 24 */
        case  76: return (25 << 8) | 16;  /* npc block 25 */
        default: return 0xFFFF;
    }
}

#endif // SPRITE_BLOCKS_H
