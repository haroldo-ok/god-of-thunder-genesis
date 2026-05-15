// God of Thunder - Sega Genesis Port
// file.c - Save / Load game state using cartridge SRAM
//
// Ported from 1_file.c.
// DOS: fopen/fread/fwrite to XXXXXXXX.SAV on FAT filesystem
// Genesis: SRAM (battery-backed RAM at 0x200001-0x20FFFF, odd bytes only)
//          using SGDK's SRAM API.
//
// Save slot 0 is always used (single-save games, like the original).
// A simple checksum guards against corrupt saves.

#include <genesis.h>
#include <string.h>
#include "god_of_thunder.h"

// ─── Save data layout ─────────────────────────────────────────────────────────
// We save only THOR_INFO + SETUP + current_level. Total ~80 bytes.
typedef struct {
    u32       magic;            // 0x474F5421 = "GOT!" — validity marker
    u16       checksum;
    u8        area;
    u8        current_level;
    THOR_INFO thor_info;
    SETUP     setup;
} SaveData;

#define SAVE_MAGIC   0x474F5421UL
#define SRAM_OFFSET  0           // byte offset into SRAM

static u16 calc_checksum(const SaveData *sd) {
    u16 sum = 0;
    const u8 *p = (const u8 *)sd + 6;  // skip magic + checksum field
    u32 n = sizeof(SaveData) - 6;
    while (n--) sum += *p++;
    return sum;
}

// ─── Save ────────────────────────────────────────────────────────────────────
void save_game(void) {
    SaveData sd;
    memset(&sd, 0, sizeof(sd));
    sd.magic         = SAVE_MAGIC;
    sd.area          = area;
    sd.current_level = (u8)current_level;
    memcpy(&sd.thor_info, &thor_info, sizeof(THOR_INFO));
    memcpy(&sd.setup,     &setup,     sizeof(SETUP));
    sd.checksum = calc_checksum(&sd);

    SRAM_enable();
    SRAM_writeBuffer(SRAM_OFFSET, &sd, sizeof(SaveData));
    SRAM_disable();
}

// ─── Load ────────────────────────────────────────────────────────────────────
// Returns 1 if a valid save was found and loaded, 0 otherwise.
// prompt: 0=silent load, 1=show prompt (not yet implemented → same as silent)
s16 load_game(s16 prompt) {
    SaveData sd;
    (void)prompt;

    SRAM_enable();
    SRAM_readBuffer(SRAM_OFFSET, &sd, sizeof(SaveData));
    SRAM_disable();

    if (sd.magic != SAVE_MAGIC) return 0;
    if (sd.checksum != calc_checksum(&sd)) return 0;

    area          = sd.area;
    current_level = sd.current_level;
    new_level     = sd.current_level;
    memcpy(&thor_info, &sd.thor_info, sizeof(THOR_INFO));
    memcpy(&setup,     &sd.setup,     sizeof(SETUP));

    return 1;
}

// ─── Check save exists ───────────────────────────────────────────────────────
s16 save_exists(void) {
    u32 magic;
    SRAM_enable();
    SRAM_readBuffer(SRAM_OFFSET, &magic, 4);
    SRAM_disable();
    return (magic == SAVE_MAGIC) ? 1 : 0;
}
