// God of Thunder - Sega Genesis Port
// init.c - Game initialization: load actor templates, set up player, level data
//
// Ported from 1_init.c + relevant parts of 1_image.c (load_standard_actors,
// setup_actor, load_objects). All DOS-specific code removed:
//   - No res_open / res_read (assets in ROM, accessed via get_level_ptr / extern arrays)
//   - No farmalloc / malloc (static storage)
//   - No keyboard interrupt hooking (SGDK handles input)
//   - No VGA mode init (SGDK handles VDP)
//   - No LZSS buffer (SGDK decompresses resources at link time)

#include <genesis.h>
#include "god_of_thunder.h"

// ─── Extra globals referenced from init ──────────────────────────────────────
u8   end_tile   = 0;
u8   thor_special_flag = 0;
u8   auto_load  = 0;
s16  last_oracle = 0;
u8   magic_cnt   = 0;
s16  thor_real_y1 = 0;
u8   music_current_init = 255;  // separate name to avoid duplicate with sound.c

// Additional info flags used by dialog/script
u8   story_flag = 1;

// ACTOR arrays for magic items (tornado=108, shield=109)
ACTOR  magic_item[2];

// ─── Actor data ROM arrays (from res/resources.res BINARY entries) ────────────
// These are the raw ACTOR{n} binary files packed into ROM by the asset pipeline.
// Format: ACTOR_DATA (5200 bytes) = pic[16][256] + shot[4][256] + NFO×2
// We use only the NFO (metadata) part at offset 5120.
extern const u8 actor_rom_data[];   // all ACTOR files concatenated, indexed by ID
// Helper: get pointer to ACTOR_DATA for actor id n
// In the resource file, actor files are stored sequentially.
// actor_rom_data layout: each slot is 5200 bytes, indexed 1..113
// (slots for missing IDs contain zeros)
#define ACTOR_DATA_SIZE   5200
#define ACTOR_NFO_OFFSET  (4096 + 1024)   // 5120

static inline const u8 *actor_nfo_ptr(u8 id) {
    return actor_rom_data + (u32)id * ACTOR_DATA_SIZE + ACTOR_NFO_OFFSET;
}

// ─── setup_actor: initialise runtime fields of an ACTOR struct ───────────────
// Mirrors the DOS setup_actor() from 1_image.c exactly.
void setup_actor(ACTOR *actr, u8 num, u8 dir, s16 x, s16 y) {
    actr->next        = 0;
    actr->frame_count = actr->frame_speed;
    actr->dir         = dir;
    actr->last_dir    = dir;
    if (actr->directions == 1) actr->dir = 0;
    if (actr->directions == 2) actr->dir &= 1;
    if (actr->directions == 4 && actr->frames == 1) {
        actr->dir  = 0;
        actr->next = dir;
    }
    actr->x          = x;
    actr->y          = y;
    actr->width      = 16;
    actr->height     = 16;
    actr->center     = 0;
    actr->last_x[0]  = actr->last_x[1] = x;
    actr->last_y[0]  = actr->last_y[1] = y;
    actr->used       = 1;
    actr->speed_count = 8;
    actr->vunerable  = STAMINA;
    actr->shot_cnt   = 20;
    actr->num_shots  = 0;
    actr->creator    = 0;
    actr->pause      = 0;
    actr->show       = 0;
    actr->actor_num  = num;
}

// ─── Copy metadata from ROM actor NFO into an ACTOR struct ───────────────────
// ─── Copy metadata from ROM actor NFO into an ACTOR struct ───────────────────
// Reads directly from actor_rom_data[] binary (packed ACTOR{n} files).
// ACTOR_NFO is 40 bytes at offset 5120 in each 5200-byte ACTOR_DATA entry.
// Layout matches ACTOR_NFO struct in utility/actornfo.h exactly.
#define ACTOR_DATA_STRIDE  5200u
#define ACTOR_NFO_OFF      5120u

void copy_actor_nfo(ACTOR *actr, u8 actor_id) {
    // Bounds check: valid IDs are 1-113
    if (actor_id == 0 || actor_id >= 114) return;

    const u8 *nfo = actor_rom_data + (u32)actor_id * ACTOR_DATA_STRIDE + ACTOR_NFO_OFF;

    // Read all fields from the binary NFO (signed bytes cast to appropriate types)
    actr->move          = (s8)nfo[0];
    actr->width         = nfo[1] ? nfo[1] : 16;
    actr->height        = nfo[2] ? nfo[2] : 16;
    actr->directions    = nfo[3] ? nfo[3] : 1;
    actr->frames        = nfo[4] ? nfo[4] : 1;
    actr->frame_speed   = nfo[5] ? nfo[5] : 6;
    actr->frame_sequence[0] = nfo[6];
    actr->frame_sequence[1] = nfo[7];
    actr->frame_sequence[2] = nfo[8];
    actr->frame_sequence[3] = nfo[9];
    actr->speed         = nfo[10] ? nfo[10] : 1;
    actr->size_x        = (s8)nfo[11];
    actr->size_y        = (s8)nfo[12];
    actr->strength      = (s8)nfo[13];
    actr->health        = nfo[14] ? nfo[14] : 50;
    actr->num_moves     = nfo[15] ? nfo[15] : 1;  // CRITICAL: was always 0 before
    actr->shot_type     = nfo[16];
    actr->shot_pattern  = nfo[17];
    actr->shots_allowed = nfo[18];
    actr->solid         = nfo[19];
    actr->flying        = nfo[20];
    actr->rating        = nfo[21];
    actr->type          = nfo[22];
    memcpy(actr->name, nfo + 23, 8);
    actr->name[8]       = 0;
    actr->func_num      = nfo[32];
    actr->func_pass     = nfo[33];
    actr->init_health   = actr->health;
}

// ─── load_standard_actors: set up Thor, Hammer, sparkle, explosion, magic ────
// Mirrors load_standard_actors() from DOS 1_image.c, minus mask building.
void load_standard_actors(void) {
    u8 thor_id   = (u8)(100 + thor_info.armor);   // 100=iron, 102=gold, etc.
    u8 hammer_id = (u8)(103 + thor_info.armor);

    // Thor (actor[0])
    memset(&actor[0], 0, sizeof(ACTOR));
    copy_actor_nfo(&actor[0], thor_id);
    setup_actor(&actor[0], 0, DIR_DOWN, 100, 100);
    thor = &actor[0];

    // Hammer (actor[1])
    memset(&actor[1], 0, sizeof(ACTOR));
    copy_actor_nfo(&actor[1], hammer_id);
    setup_actor(&actor[1], 1, DIR_DOWN, 100, 100);
    actor[1].used = 0;
    hammer = &actor[1];

    // Shield placeholder (actor[2]) — starts unused
    memset(&actor[2], 0, sizeof(ACTOR));
    actor[2].actor_num = 2;

    // Sparkle (used as death VFX template)
    memset(&sparkle, 0, sizeof(ACTOR));
    copy_actor_nfo(&sparkle, 106);
    setup_actor(&sparkle, 20, 0, 100, 100);
    sparkle.used = 0;

    // Explosion (used as death VFX template for certain enemies)
    memset(&explosion, 0, sizeof(ACTOR));
    copy_actor_nfo(&explosion, 107);
    setup_actor(&explosion, 21, 0, 100, 100);
    explosion.used = 0;

    // Tornado magic item template (actor[2] when tornado is active)
    memset(&magic_item[0], 0, sizeof(ACTOR));
    copy_actor_nfo(&magic_item[0], 108);
    setup_actor(&magic_item[0], 20, 0, 0, 0);
    magic_item[0].used = 0;

    // Shield magic item template
    memset(&magic_item[1], 0, sizeof(ACTOR));
    copy_actor_nfo(&magic_item[1], 109);
    setup_actor(&magic_item[1], 20, 0, 0, 0);
    magic_item[1].used = 0;

    // Load shot templates from actor data (shot types 1..N)
    // The DOS game loaded these from the shot[] portion of each ACTOR file.
    // We use actor_meta to fill the shot structs from their actor IDs.
    // Shot actors are referenced by shot_type field (1-based index).
    // Map: common shot actors found by scanning actor table for ATYPE_SHOT
    {
        s16 si = 0;
        s16 i;
        for (i = 1; i < 120 && si < MAX_ENEMIES; i++) {
            const ACTOR_META *m = actor_meta_get((u8)i);
            if (m && m->atype == ATYPE_SHOT) {
                memset(&shot[si], 0, sizeof(ACTOR));
                copy_actor_nfo(&shot[si], (u8)i);
                setup_actor(&shot[si], (u8)(si + 20), 0, 0, 0);
                shot[si].used = 0;
                si++;
            }
        }
    }
}

// ─── load_objects: no-op on Genesis (object data is in resources.res) ────────
// DOS loaded OBJECTS resource file into a far buffer.
// On Genesis the obj_tiles tileset is compiled in; show_objects() handles display.
s16 load_objects(void) {
    return 1;   // always succeeds
}

// ─── setup_player: reset Thor stats for new game ─────────────────────────────
s16 setup_player(void) {
    memset(&thor_info, 0, sizeof(THOR_INFO));
    thor_info.inventory = 0;
    if (area > 1) thor_info.inventory |= (u16)(APPLE_MAGIC | LIGHTNING_MAGIC);
    if (area > 2) thor_info.inventory |= (u16)(BOOTS_MAGIC | WIND_MAGIC);

    thor->health       = 150;
    thor_info.magic    = 0;
    thor_info.jewels   = 0;
    thor_info.score    = 0;
    thor_info.keys     = 0;
    thor_info.last_item = 0;
    thor_info.object   = 0;
    thor_info.object_name = NULL;

    thor->x = 152;  thor->y = 96;
    thor->last_x[0] = thor->last_x[1] = thor->x;
    thor->last_y[0] = thor->last_y[1] = thor->y;
    thor_info.last_icon   = (u8)(6 * 20 + 8);
    thor_info.last_screen = 23;
    thor->dir = DIR_DOWN;

    display_health(); display_magic();
    display_jewels(); display_keys();
    display_score();
    return 1;
}

// ─── initialize: top-level init called from game_init() ──────────────────────
s16 initialize(void) {
    s16 i;

    // Clear all actor slots
    memset(actor,   0, sizeof(actor));
    memset(shot,    0, sizeof(shot));
    memset(enemy,   0, sizeof(enemy));
    for (i = 0; i < MAX_ACTORS; i++) actor[i].actor_num = (u8)i;

    // Set actor pointers
    thor   = &actor[0];
    hammer = &actor[1];

    // Load sprite definitions for Thor/Hammer/VFX
    load_standard_actors();

    // Load objects tileset (no-op on Genesis)
    load_objects();

    // Set up player stats
    setup_player();

    // Load initial level
    memcpy(&scrn, get_level_ptr(area, (u8)current_level), sizeof(LEVEL));
    level_type = scrn.type;

    // Spawn Thor sprite
    load_actor_sprite(thor);

    return 0;
}
