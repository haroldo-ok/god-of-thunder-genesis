// God of Thunder - Sega Genesis Port
// sound.c - Sound effects and music
//
// Ported from 1_sound.c / 1_music.c / 1_sbfx.c
// DOS: Sound Blaster digitized PCM + PC Speaker + AdLib FM (licensed library)
// Genesis: YM2612 FM music via XGM driver + PCM sound effects via XGM channels
//
// SFX are loaded as PCM resources in resources.res.
// Music tracks (level_type 0-5) are XGM files converted from the original songs.

#include <genesis.h>
#include "god_of_thunder.h"

// ─── XGM music tracks (compiled from resources.res) ──────────────────────────
// Each corresponds to level_type values 0-5 (scrn.type field).
// These are declared extern; SGDK generates them from the XGM lines in resources.res.
// Placeholder declarations — replace with actual resource names once XGMs are ready.
extern const u8 music_track_0[];    // forest theme
extern const u8 music_track_1[];    // cave theme
extern const u8 music_track_2[];    // castle theme
extern const u8 music_track_3[];    // boss theme
extern const u8 music_track_4[];    // town theme
extern const u8 music_track_5[];    // final theme

static const u8 *music_tracks[] = {
    music_track_0,
    music_track_1,
    music_track_2,
    music_track_3,
    music_track_4,
    music_track_5,
};
#define NUM_MUSIC_TRACKS 6

// ─── PCM sound effects (compiled from resources.res) ─────────────────────────
// One entry per SFX_* constant in god_of_thunder.h.
// Declare extern — SGDK generates from WAV lines in resources.res.
extern const u8 sfx_ow[];           extern const u32 sfx_ow_len;
extern const u8 sfx_gulp[];         extern const u32 sfx_gulp_len;
extern const u8 sfx_swish[];        extern const u32 sfx_swish_len;
extern const u8 sfx_yah[];          extern const u32 sfx_yah_len;
extern const u8 sfx_electric[];     extern const u32 sfx_electric_len;
extern const u8 sfx_thunder[];      extern const u32 sfx_thunder_len;
extern const u8 sfx_door[];         extern const u32 sfx_door_len;
extern const u8 sfx_fall[];         extern const u32 sfx_fall_len;
extern const u8 sfx_angel[];        extern const u32 sfx_angel_len;
extern const u8 sfx_woop[];         extern const u32 sfx_woop_len;
extern const u8 sfx_unused[];       extern const u32 sfx_unused_len;
extern const u8 sfx_braapp[];       extern const u32 sfx_braapp_len;
extern const u8 sfx_wind[];         extern const u32 sfx_wind_len;
extern const u8 sfx_punch[];        extern const u32 sfx_punch_len;
extern const u8 sfx_clang[];        extern const u32 sfx_clang_len;
extern const u8 sfx_explode[];      extern const u32 sfx_explode_len;
extern const u8 sfx_dead[];         extern const u32 sfx_dead_len;
extern const u8 sfx_boss1[];        extern const u32 sfx_boss1_len;
extern const u8 sfx_boss2[];        extern const u32 sfx_boss2_len;

typedef struct { const u8 *data; u32 len; } SfxEntry;
static const SfxEntry sfx_table[NUM_SOUNDS] = {
    {sfx_ow,       0},  // 0 OW
    {sfx_gulp,     0},  // 1 GULP
    {sfx_swish,    0},  // 2 SWISH
    {sfx_yah,      0},  // 3 YAH
    {sfx_electric, 0},  // 4 ELECTRIC
    {sfx_thunder,  0},  // 5 THUNDER
    {sfx_door,     0},  // 6 DOOR
    {sfx_fall,     0},  // 7 FALL
    {sfx_angel,    0},  // 8 ANGEL
    {sfx_woop,     0},  // 9 WOOP
    {sfx_unused,   0},  // 10 unused
    {sfx_braapp,   0},  // 11 BRAAPP
    {sfx_wind,     0},  // 12 WIND
    {sfx_punch,    0},  // 13 PUNCH
    {sfx_clang,    0},  // 14 CLANG
    {sfx_explode,  0},  // 15 EXPLODE
    {sfx_dead,     0},  // 16 DEAD
    {sfx_boss1,    0},  // 17 BOSS1
    {sfx_boss2,    0},  // 18 BOSS2
};

// XGM PCM slot assignments (XGM driver has 4 PCM channels: 1-4)
// We assign: channel 1 = high-priority SFX, channel 2 = low-priority SFX
#define SFX_CH_HI   SOUND_PCM_CH2
#define SFX_CH_LO   SOUND_PCM_CH3

// Priority tracking
static u8  current_priority  = 0;
static const u8 sound_priority[NUM_SOUNDS] = {
    1,2,3,3,3,1,4,4,4,5,4,3,1,2,2,5,1,3,1
};

static u8  music_paused = 0;
u8 music_current = 255;

// ─── Sound init ──────────────────────────────────────────────────────────────
void sound_init(void) {
    // SGDK XGM driver is initialised automatically; register our PCM samples.
    s16 i;
    for (i = 0; i < NUM_SOUNDS; i++) {
        if (sfx_table[i].data) {
            XGM_setPCM((u8)i, sfx_table[i].data, sfx_table[i].len);
        }
    }
    music_current = 255;
    music_paused  = 0;
}

// ─── Play SFX ────────────────────────────────────────────────────────────────
// priority_override: 0=use table priority, 1=always play
void play_sound(s16 id, s16 priority_override) {
    if (id < 0 || id >= NUM_SOUNDS) return;
    if (!sfx_table[id].data) return;
    u8 pri = sound_priority[id];
    if (!priority_override && pri < current_priority) return;
    current_priority = pri;
    XGM_startPlayPCM((u8)id, 1, SFX_CH_HI);
}

// ─── Music ───────────────────────────────────────────────────────────────────
void music_play(s16 track, s16 restart) {
    if (!setup.music) return;
    if (track < 0 || track >= NUM_MUSIC_TRACKS) return;
    if ((u8)track == music_current && !restart) return;
    music_current = (u8)track;
    music_paused  = 0;
    if (music_tracks[track])
        XGM_startPlay(music_tracks[track]);
}

void music_pause(void) {
    if (!music_paused) {
        XGM_pausePlay();
        music_paused = 1;
    }
}

void music_resume(void) {
    if (music_paused) {
        XGM_resumePlay();
        music_paused = 0;
    }
}
