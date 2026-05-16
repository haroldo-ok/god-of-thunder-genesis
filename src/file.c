// file.c - SRAM save/load (God of Thunder Genesis Port)
// SGDK 1.70 SRAM API: byte-level only (no buffer functions)

#include <genesis.h>
#include "god_of_thunder.h"

// SaveData layout
typedef struct {
    u32  magic;
    u16  checksum;
    u8   area;
    u8   current_level_save;
    THOR_INFO thor_info_save;
    SETUP     setup_save;
} SaveData;

#define SAVE_MAGIC  0x474F5421UL
#define SRAM_OFFSET 0

static u16 calc_checksum(const SaveData *sd) {
    u16 sum = 0;
    const u8 *p = (const u8 *)sd + 6;
    u32 n = sizeof(SaveData) - 6;
    while (n--) sum += *p++;
    return sum;
}

// Write a buffer to SRAM byte by byte (SGDK 1.70 has no SRAM_writeBuffer)
static void sram_write_buf(u16 offset, const void *buf, u16 size) {
    const u8 *p = (const u8 *)buf;
    u16 i;
    SRAM_enable();
    for (i = 0; i < size; i++)
        SRAM_writeByte(offset + i, p[i]);
    SRAM_disable();
}

static void sram_read_buf(u16 offset, void *buf, u16 size) {
    u8 *p = (u8 *)buf;
    u16 i;
    SRAM_enable();
    for (i = 0; i < size; i++)
        p[i] = SRAM_readByte(offset + i);
    SRAM_disable();
}

void save_game(void) {
    SaveData sd;
    memset(&sd, 0, sizeof(sd));
    sd.magic              = SAVE_MAGIC;
    sd.area               = area;
    sd.current_level_save = (u8)current_level;
    memcpy(&sd.thor_info_save, &thor_info, sizeof(THOR_INFO));
    memcpy(&sd.setup_save,     &setup,     sizeof(SETUP));
    sd.checksum = calc_checksum(&sd);
    sram_write_buf(SRAM_OFFSET, &sd, sizeof(SaveData));
}

s16 load_game(s16 prompt) {
    SaveData sd;
    (void)prompt;
    sram_read_buf(SRAM_OFFSET, &sd, sizeof(SaveData));
    if (sd.magic != SAVE_MAGIC) return 0;
    if (sd.checksum != calc_checksum(&sd)) return 0;
    area          = sd.area;
    current_level = sd.current_level_save;
    new_level     = sd.current_level_save;
    memcpy(&thor_info, &sd.thor_info_save, sizeof(THOR_INFO));
    memcpy(&setup,     &sd.setup_save,     sizeof(SETUP));
    return 1;
}

s16 save_exists(void) {
    u32 magic = 0;
    u8 i;
    SRAM_enable();
    for (i = 0; i < 4; i++)
        ((u8*)&magic)[i] = SRAM_readByte(SRAM_OFFSET + i);
    SRAM_disable();
    return (magic == SAVE_MAGIC) ? 1 : 0;
}
