// God of Thunder - Sega Genesis Port
// image.c - Hardware sprite management
//
// Each ACTOR maps to an SGDK Sprite* handle.
// Sprite sheets pack multiple actors side by side (block_idx per actor).
// SGDK SpriteDefinition sees the whole sheet as a flat animation grid:
//   anim  = (block_idx / sheet_cols) * MAX_DIRS + direction
//   frame = (block_idx % sheet_cols) * MAX_FRAMES + frame_idx

#include <genesis.h>
#include "god_of_thunder.h"
#include "sprite_blocks.h"

// SGDK sprite definitions (from resources.res)
extern const SpriteDefinition thor_spr;
extern const SpriteDefinition hammer_spr;
extern const SpriteDefinition fx_spr;
extern const SpriteDefinition enemy_spr;
extern const SpriteDefinition npc_spr;

// FX actor file IDs (sparkle/explode/tornado/shield)
#define IS_FX_ACTOR(id) ((id)==106||(id)==107||(id)==108||(id)==109)

// actor_id stored per ACTOR so we can look up block_idx
// We repurpose the unused 'rand' field to store the actor file ID.
// OR: we track it per-slot. Use a simple array:
static u8 actor_file_id[MAX_ACTORS];  // actor file number (1-113) for each slot

static const SpriteDefinition *get_sprite_def(const ACTOR *actr) {
    switch (actr->type) {
        case ATYPE_THOR:   return &thor_spr;
        case ATYPE_HAMMER: return &hammer_spr;
        case ATYPE_ENEMY:
            if (IS_FX_ACTOR(actor_file_id[actr->actor_num])) return &fx_spr;
            return &enemy_spr;
        case ATYPE_SHOT:   return &enemy_spr;
        case ATYPE_NPC:    return &npc_spr;
        default:           return &enemy_spr;
    }
}

static u8 get_pal_line(const ACTOR *actr) {
    switch (actr->type) {
        case ATYPE_THOR:
        case ATYPE_HAMMER: return PAL1;
        case ATYPE_ENEMY:
        case ATYPE_SHOT:   return PAL2;
        case ATYPE_NPC:    return PAL3;
        default:           return PAL2;
    }
}

// Compute SGDK anim and frame from actor file id, direction, and frame index.
// Returns (anim << 8) | frame.
static u16 compute_sgdk_anim_frame(u8 file_id, u8 dir, u8 frm, u8 n_dirs, u8 n_frms) {
    u16 info = actor_sheet_block(file_id);
    if (info == 0xFFFF) return 0;  // unknown actor

    u16 block_idx  = info >> 8;
    u16 sheet_cols = info & 0xFF;

    // Clamp direction and frame to valid range
    u8 clamped_dir = (n_dirs > 0 && dir < n_dirs) ? dir : 0;
    u8 clamped_frm = (n_frms > 0 && frm < n_frms) ? frm : 0;

    u16 anim  = (block_idx / sheet_cols) * MAX_DIRS_PER_BLOCK + clamped_dir;
    u16 frame = (block_idx % sheet_cols) * MAX_FRAMES_PER_DIR + clamped_frm;
    return (anim << 8) | frame;
}

void load_actor_sprite(ACTOR *actr) {
    if (!actr) return;

    if (actr->spr) {
        SPR_releaseSprite(actr->spr);
        actr->spr = NULL;
    }

    if (!actr->used && actr->type != ATYPE_THOR) return;

    // Store the file ID for this slot so we can look up block_idx later.
    // For standard actors: init.c sets actor_file_id[] before calling this.
    // Fallback: use actor_num as a guess (wrong but visible).
    u8 file_id = actor_file_id[actr->actor_num];

    const SpriteDefinition *def = get_sprite_def(actr);
    u8 pal = get_pal_line(actr);

    actr->spr = SPR_addSprite(def, actr->x, actr->y, TILE_ATTR(pal, 1, 0, 0));
    if (!actr->spr) return;

    // Set correct anim/frame immediately
    u16 af = compute_sgdk_anim_frame(file_id, actr->dir, actr->next,
                                      actr->directions, actr->frames);
    SPR_setAnimAndFrame(actr->spr, af >> 8, af & 0xFF);
}

void free_actor_sprite(ACTOR *actr) {
    if (actr && actr->spr) {
        SPR_releaseSprite(actr->spr);
        actr->spr = NULL;
    }
}

void actor_set_anim(ACTOR *actr) {
    if (!actr || !actr->spr) return;

    u8 visible = 1;
    if (actr->show & 1) visible = 0;   // blink on odd show counts
    if (!actr->used)    visible = 0;

    SPR_setVisibility(actr->spr, visible ? VISIBLE : HIDDEN);
    if (!visible) return;

    SPR_setPosition(actr->spr, actr->x, actr->y);

    u8 file_id = actor_file_id[actr->actor_num];
    u16 af = compute_sgdk_anim_frame(file_id, actr->dir, actr->next,
                                      actr->directions, actr->frames);
    SPR_setAnimAndFrame(actr->spr, af >> 8, af & 0xFF);
}

void update_sprites(void) {
    s16 i;
    for (i = 0; i < MAX_ACTORS; i++) {
        if (actor[i].spr) actor_set_anim(&actor[i]);
    }
}

void init_sprites(void) {
    s16 i;
    for (i = 0; i < MAX_ACTORS; i++) {
        actor[i].spr = NULL;
        actor_file_id[i] = 0;
    }
}

// Called by init.c / back.c before load_actor_sprite() to register the file ID.
void actor_set_file_id(u8 slot, u8 file_id) {
    if (slot < MAX_ACTORS) actor_file_id[slot] = file_id;
}
