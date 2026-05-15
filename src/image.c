// God of Thunder - Sega Genesis Port
// image.c - Hardware sprite management
//
// Full replacement for DOS 1_image.c (which was entirely Mode X / VGA blitter
// code — ALIGNED_MASK_IMAGE, make_mask, xcopys2d, etc.).
//
// On Genesis, all sprite rendering is done by the VDP sprite engine via SGDK.
// This file:
//   - Maps each active ACTOR to an SGDK Sprite* via load_actor_sprite()
//   - Calls SPR_setPosition / SPR_setAnim / SPR_setVisibility each logic tick
//   - Tracks which SpriteDefinition belongs to each actor category
//
// Sprite sheet layout (from convert_assets.py output):
//   Each actor occupies a block of (MAX_FRMS * 16) × (MAX_DIRS * 16) pixels
//   in its category sheet. Rows = directions (0-3 = up/down/left/right).
//   Cols = frames (0-3).
//
// SGDK SpriteDefinition: declared via SPRITE macro in resources.res.
//   Each SPRITE is a separate definition (one per sprite sheet file).
//   Animations = directions; frames per animation = frames.

#include <genesis.h>
#include <string.h>
#include "god_of_thunder.h"

// ─── SGDK sprite definitions (generated from resources.res) ──────────────────
// Declared extern — SGDK auto-generates these from the SPRITE lines in resources.res
extern const SpriteDefinition thor_spr;
extern const SpriteDefinition hammer_spr;
extern const SpriteDefinition fx_spr;
extern const SpriteDefinition enemy_spr;
extern const SpriteDefinition npc_spr;
extern const SpriteDefinition shot_spr;

// ─── Map actor IDs to SpriteDefinition pointers ───────────────────────────────
// For the enemy/npc sheets, each actor occupies column (actor_sheet_index * 4)
// in the sheet. SGDK doesn't natively support "pick which column" — instead we
// use separate SpriteDefinitions per actor type, or we use the frame offset.
// Simple approach: use a single enemy_spr definition and set the start frame
// offset using SPR_setFrame to jump to the right actor's block.
// Each actor block = 4 directions × 4 frames = 16 frames in SGDK's terms.

static const SpriteDefinition *get_sprite_def(const ACTOR *actr) {
    switch (actr->type) {
        case ATYPE_THOR:   return &thor_spr;
        case ATYPE_HAMMER: return &hammer_spr;
        case ATYPE_ENEMY:
            // FX actors (sparkle=106, explosion=107, tornado=108, shield=109)
            if (actr->actor_num == ACT_SPARKLE ||
                actr->actor_num == ACT_EXPLODE ||
                actr->actor_num == ACT_TORNADO ||
                actr->actor_num == ACT_SHIELD)
                return &fx_spr;
            return &enemy_spr;
        case ATYPE_SHOT:   return &shot_spr;
        case ATYPE_NPC:    return &npc_spr;
        default:           return &enemy_spr;
    }
}

// Return which PAL line an actor uses
static u8 get_pal_line(const ACTOR *actr) {
    switch (actr->type) {
        case ATYPE_THOR:   return PAL1;
        case ATYPE_HAMMER: return PAL1;
        case ATYPE_ENEMY:  return PAL2;
        case ATYPE_SHOT:   return PAL2;
        case ATYPE_NPC:    return PAL3;
        default:           return PAL2;
    }
}

// ─── Spawn / update / free ────────────────────────────────────────────────────

void load_actor_sprite(ACTOR *actr) {
    if (!actr || !actr->used) return;

    // Free existing sprite if any
    if (actr->spr) {
        SPR_releaseSprite(actr->spr);
        actr->spr = NULL;
    }

    const SpriteDefinition *def = get_sprite_def(actr);
    u8 pal = get_pal_line(actr);

    actr->spr = SPR_addSprite(def,
                               actr->x,
                               actr->y,
                               TILE_ATTR(pal, 1, 0, 0));

    if (actr->spr) {
        // For actors on the enemy/npc sheets: each actor occupies 16 SGDK
        // animation frames (4 dirs × 4 frames). Jump to this actor's block.
        // actor_sheet_offset: which block in the sprite sheet this actor is.
        // We store this as (actor_id index in its group) * 16.
        // For now: actors use their first animation (dir=0) by default.
        s16 n_dirs = (s16)(actr->directions == 0 ? 1 : actr->directions);
        if (n_dirs > 4) n_dirs = 4;
        SPR_setAnimAndFrame(actr->spr, 0, 0);
    }
}

void free_actor_sprite(ACTOR *actr) {
    if (actr && actr->spr) {
        SPR_releaseSprite(actr->spr);
        actr->spr = NULL;
    }
}

// Called every logic tick to sync sprite position, animation, visibility.
void actor_set_anim(ACTOR *actr) {
    if (!actr || !actr->spr) return;

    // Visibility: hide when blinking (show != 0 → blink every other tick)
    u8 visible = 1;
    if (actr->show) visible = (u8)(actr->show & 1);   // blink
    if (!actr->used) visible = 0;

    SPR_setVisibility(actr->spr, visible ? VISIBLE : HIDDEN);

    if (!visible) return;

    SPR_setPosition(actr->spr, actr->x, actr->y);

    // Animation: direction maps to animation row, frame maps to column.
    // Clamp to valid range.
    s16 anim  = actr->dir;
    s16 frame = actr->next;
    s16 n_dirs  = (s16)(actr->directions == 0 ? 1 : actr->directions);
    s16 n_frames = (s16)(actr->frames == 0 ? 1 : actr->frames);
    if (n_dirs  > 4) n_dirs  = 4;
    if (n_frames > 4) n_frames = 4;
    if (anim  >= n_dirs)   anim  = 0;
    if (frame >= n_frames) frame = 0;

    SPR_setAnimAndFrame(actr->spr, anim, frame);
}

// ─── Bulk update: called once per frame to sync all actors ───────────────────
void update_sprites(void) {
    s16 i;
    for (i = 0; i < MAX_ACTORS; i++) {
        if (actor[i].spr) {
            actor_set_anim(&actor[i]);
        }
    }
    // SPR_update() is called in the VBlank callback (main.c) — not here.
}

// ─── Initialise sprite engine ─────────────────────────────────────────────────
void init_sprites(void) {
    // SPR_init() already called in game_init(); nothing else needed here.
    // Ensure all actor sprite pointers start NULL.
    s16 i;
    for (i = 0; i < MAX_ACTORS; i++) actor[i].spr = NULL;
}
