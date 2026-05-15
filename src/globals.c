// God of Thunder - Sega Genesis Port
// globals.c - Global variables and small utility functions that don't belong
//             to a single translation unit. Collected here to avoid
//             duplicate-definition linker errors.

#include <genesis.h>
#include <string.h>
#include "god_of_thunder.h"

// ─── Globals declared extern in god_of_thunder.h, defined here ───────────────
// (Everything defined in other .c files is NOT listed here)

// These were scattered across DOS globals in 1_main.c / 1_movpat.c / 1_init.c
u8   apple_drop = 0;
u8   thor_icon1 = 0, thor_icon2 = 0, thor_icon3 = 0, thor_icon4 = 0;
u8   diag       = 0;

// Level data arrays (Episode 1-3 packed from SDAT1-3 assets via resources.res)
// Declared BINARY in resources.res; SGDK exports them as const u8 arrays.
// Weak-linked here so the project compiles even before the binary resources exist.
// level_data_ep1/2/3 and actor_rom_data are provided by SGDK via resources.res BINARY entries

// OBJ_TILE_VRAM_BASE as a variable (back.c uses it as a #define referring to the
// computed position; object.c also needs it as a runtime value)
const u16 OBJ_TILE_VRAM_BASE_VAL = (u16)(1 + 230 * 4);  // after 230 bg tiles

// ─── clamp_s16: missing from SGDK ────────────────────────────────────────────
s16 clamp_s16(s16 v, s16 lo, s16 hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// ─── printt: debug display (no-op on release builds) ─────────────────────────
void printt(s16 val) { (void)val; }

// ─── atoi: not always available in SGDK libc ─────────────────────────────────
// SGDK's newlib does include atoi, but if it's missing on your toolchain:
// s16 atoi_s16(const char *s) { ... }  ← not needed if newlib is present

// ─── dialog_func table (referenced from movpat.c) ────────────────────────────
// In the original game dialog_func[] mapped actor func_num to a dialog handler.
// On Genesis, script execution replaces this; we provide a table of no-ops.
s16 dialog_func_stub(ACTOR *actr) { (void)actr; return 0; }
s16 (*dialog_func[])(ACTOR *actr) = {
    dialog_func_stub, dialog_func_stub, dialog_func_stub,
    dialog_func_stub, dialog_func_stub, dialog_func_stub,
    dialog_func_stub, dialog_func_stub, dialog_func_stub,
    dialog_func_stub, dialog_func_stub, dialog_func_stub,
    dialog_func_stub, dialog_func_stub, dialog_func_stub,
    dialog_func_stub, dialog_func_stub, dialog_func_stub,
    dialog_func_stub, dialog_func_stub,
};
