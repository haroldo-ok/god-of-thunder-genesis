// God of Thunder - Sega Genesis Port
// sound.c - Sound effects and music
//
// Music tracks and SFX stubs: add real .xgm and .wav files to res/ and
// uncomment the corresponding lines in res/resources.res to enable audio.

#include <genesis.h>
#include "god_of_thunder.h"

// ─── Music track stubs ───────────────────────────────────────────────────────
// Uncomment each line below once you add the .xgm file to res/ and resources.res:
// extern const u8 music_track_0[];   // forest
// extern const u8 music_track_1[];   // cave
// extern const u8 music_track_2[];   // castle
// extern const u8 music_track_3[];   // boss
// extern const u8 music_track_4[];   // town
// extern const u8 music_track_5[];   // final

static const u8 *music_tracks[6] = {
    NULL, NULL, NULL, NULL, NULL, NULL
};

// ─── SFX stubs ───────────────────────────────────────────────────────────────
// Uncomment each line and add WAV resource entries once .wav files are ready.
// extern const u8 sfx_ow[];   extern const u32 sfx_ow_len;
// (repeat for each of the 19 sfx)

typedef struct { const u8 *data; u32 len; } SfxEntry;
static const SfxEntry sfx_table[NUM_SOUNDS] = {
    {NULL,0},{NULL,0},{NULL,0},{NULL,0},{NULL,0},
    {NULL,0},{NULL,0},{NULL,0},{NULL,0},{NULL,0},
    {NULL,0},{NULL,0},{NULL,0},{NULL,0},{NULL,0},
    {NULL,0},{NULL,0},{NULL,0},{NULL,0}
};

static const u8 sound_priority[NUM_SOUNDS] = {
    1,2,3,3,3,1,4,4,4,5,4,3,1,2,2,5,1,3,1
};

static u8  music_paused   = 0;
u8         music_current  = 255;

void sound_init(void) {
    // Register non-NULL SFX samples with XGM driver
    u8 i;
    for (i = 0; i < NUM_SOUNDS; i++) {
        if (sfx_table[i].data)
            SND_setPCM_XGM(i, sfx_table[i].data, sfx_table[i].len);
    }
    music_current = 255;
    music_paused  = 0;
}

void play_sound(s16 id, s16 priority_override) {
    if (id < 0 || id >= NUM_SOUNDS) return;
    if (!sfx_table[id].data) return;         // stub: no sound yet
    if (!priority_override && sound_priority[id] < 1) return;
    SND_startPlayPCM_XGM((u8)id, 1, SOUND_PCM_CH2);
}

void music_play(s16 track, s16 restart) {
    if (!setup.music) return;
    if (track < 0 || track >= 6) return;
    if ((u8)track == music_current && !restart) return;
    if (!music_tracks[track]) return;        // stub: no music yet
    music_current = (u8)track;
    music_paused  = 0;
    SND_startPlay_XGM(music_tracks[track]);
}

void music_pause(void) {
    if (!music_paused) { SND_pausePlay_XGM(); music_paused = 1; }
}

void music_resume(void) {
    if (music_paused) { SND_resumePlay_XGM(); music_paused = 0; }
}
