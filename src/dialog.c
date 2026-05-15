// God of Thunder - Sega Genesis Port
// dialog.c - Dialog boxes, menus, item selection
//
// Ported from 1_panel.c (menus) + 1_back.c (display_speech).
// DOS used Mode X pixel drawing + busy-wait timers.
// Genesis uses VDP WINDOW plane text + VBlank waits.

#include <genesis.h>
#include <stdlib.h>
#include <string.h>
#include "god_of_thunder.h"

// ─── Wait helpers ─────────────────────────────────────────────────────────────
// DOS: wait_response() / wait_not_response() polled key_flag[] in a busy loop.
// Genesis: poll joypad until A/B/C/Start is pressed or released.

static void wait_response(void) {
    u16 btns;
    // Wait for any button release first (debounce)
    do { btns = JOY_readJoypad(JOY_1); SYS_doVBlankProcess(); }
    while (btns & (BUTTON_A|BUTTON_B|BUTTON_C|BUTTON_START));
    // Then wait for press
    do { btns = JOY_readJoypad(JOY_1); SYS_doVBlankProcess(); }
    while (!(btns & (BUTTON_A|BUTTON_B|BUTTON_C|BUTTON_START)));
}

static void wait_not_response(void) {
    u16 btns;
    do { btns = JOY_readJoypad(JOY_1); SYS_doVBlankProcess(); }
    while (btns & (BUTTON_A|BUTTON_B|BUTTON_C|BUTTON_START));
}

// ─── Dialog box area (WINDOW plane rows 0-7) ──────────────────────────────────
// We overlay a dialog box in the WINDOW plane, which sits above BG_A.
// The window covers the full screen when enabled; we enable it just for dialogs.
#define DIALOG_ROW_START  6
#define DIALOG_ROW_END   16
#define DIALOG_COL_START  4
#define DIALOG_COL_END   36

static void dialog_box_draw(void) {
    s16 r, c;
    // Flood fill dialog area with dark bg (PAL3 slot 0 = transparent → use slot 2)
    for (r = DIALOG_ROW_START; r <= DIALOG_ROW_END; r++)
        for (c = DIALOG_COL_START; c <= DIALOG_COL_END; c++)
            VDP_setTileMapXY(WINDOW, TILE_ATTR(PAL3, 0, 0, 0, 1), (u16)c, (u16)r);
    // Draw border using tile 1 with a different palette slot
    for (c = DIALOG_COL_START; c <= DIALOG_COL_END; c++) {
        VDP_setTileMapXY(WINDOW, TILE_ATTR(PAL1, 0, 0, 0, 1), (u16)c, (u16)DIALOG_ROW_START);
        VDP_setTileMapXY(WINDOW, TILE_ATTR(PAL1, 0, 0, 0, 1), (u16)c, (u16)DIALOG_ROW_END);
    }
}

static void dialog_box_clear(void) {
    s16 r, c;
    for (r = DIALOG_ROW_START; r <= DIALOG_ROW_END; r++)
        for (c = DIALOG_COL_START; c <= DIALOG_COL_END; c++)
            VDP_setTileMapXY(WINDOW, TILE_ATTR(PAL0, 0, 0, 0, 0), (u16)c, (u16)r);
}

// ─── show_dialog: print text with word-wrap into dialog box ──────────────────
void show_dialog(const char *text, s16 color) {
    (void)color;
    VDP_setWindowVPos(TRUE, 0);   // cover full screen with window for dialog

    dialog_box_draw();

    // Word-wrap into 30-char rows
    s16 row = DIALOG_ROW_START + 1;
    s16 col = DIALOG_COL_START + 1;
    const char *p = text;
    char line[32];
    s16  li = 0;

    while (*p && row < DIALOG_ROW_END) {
        if (*p == '\n' || li >= 30) {
            line[li] = 0;
            VDP_drawText(line, (u16)col, (u16)row);
            row++; li = 0;
            if (*p == '\n') { p++; continue; }
        }
        // Skip color escape sequences (~X)
        if (*p == '~' && *(p+1)) { p += 2; continue; }
        line[li++] = *p++;
    }
    if (li > 0) { line[li] = 0; VDP_drawText(line, (u16)col, (u16)row); }

    // "Press button to continue" prompt
    VDP_drawText("Press A to continue", (u16)(DIALOG_COL_START+1), (u16)(DIALOG_ROW_END-1));

    wait_response();
    wait_not_response();

    dialog_box_clear();
    VDP_setWindowVPos(TRUE, 24);  // restore HUD-only window
}

// ─── select_item: item selection menu ────────────────────────────────────────
void select_item(void) {
    extern const char *item_name[];  // defined in back.c
    s16 p, op, b;
    u16 btns, prev = 0xFFFF;

    if (tornado_used || lightning_used || thunder_flag ||
        hourglass_flag || thor->num_moves > 1 || shield_on) return;

    if (!thor_info.inventory) {
        show_dialog("No Items Found", 14);
        return;
    }

    // Find first owned item
    p = 0; b = 1;
    while (!(thor_info.inventory & (u16)b) && p < 8) { p++; b <<= 1; }

    op = p;
    VDP_setWindowVPos(TRUE, 0);
    dialog_box_draw();

    // Draw item names
    {
        s16 row = DIALOG_ROW_START + 2;
        s16 i; s16 bi = 1;
        for (i = 0; i < 6; i++, bi <<= 1) {
            if (thor_info.inventory & (u16)bi)
                VDP_drawText(item_name[i], (u16)(DIALOG_COL_START + 1 + i*5), (u16)row);
        }
    }

    // Show current selection
    {
        const char *nm = (p < 6) ? item_name[p] : thor_info.object_name;
        if (!nm) nm = "???";
        VDP_drawText(nm, (u16)(DIALOG_COL_START+1), (u16)(DIALOG_ROW_START+4));
    }

    wait_not_response();

    while (1) {
        SYS_doVBlankProcess();
        btns = JOY_readJoypad(JOY_1);
        u16 pressed = btns & ~prev;
        prev = btns;

        if (pressed & BUTTON_START) break;
        if (pressed & (BUTTON_A|BUTTON_B|BUTTON_C)) {
            thor_info.item = (u8)(p + 1);
            display_item();
            break;
        }
        if (pressed & BUTTON_RIGHT) {
            do {
                p = (p + 1) & 7;
                b = 1 << p;
            } while (!(thor_info.inventory & (u16)b));
        }
        if (pressed & BUTTON_LEFT) {
            do {
                p = (p - 1 + 8) & 7;
                b = 1 << p;
            } while (!(thor_info.inventory & (u16)b));
        }
        if (p != op) {
            op = p;
            VDP_clearText((u16)(DIALOG_COL_START+1), (u16)(DIALOG_ROW_START+4), 28);
            const char *nm = (p < 6) ? item_name[p] : thor_info.object_name;
            if (!nm) nm = "???";
            VDP_drawText(nm, (u16)(DIALOG_COL_START+1), (u16)(DIALOG_ROW_START+4));
        }
    }

    wait_not_response();
    dialog_box_clear();
    VDP_setWindowVPos(TRUE, 24);
}

// ─── Simple option menu stubs ─────────────────────────────────────────────────
// These are shown on START press. Minimal implementation: just show text options,
// navigate with D-pad, confirm with A.

static s16 simple_menu(const char *title, const char **opts) {
    s16 n = 0, sel = 0;
    u16 btns, prev = 0xFFFF;

    while (opts[n]) n++;

    VDP_setWindowVPos(TRUE, 0);
    dialog_box_draw();
    VDP_drawText(title, (u16)(DIALOG_COL_START+1), (u16)(DIALOG_ROW_START+1));

    s16 i;
    for (i = 0; i < n; i++)
        VDP_drawText(opts[i], (u16)(DIALOG_COL_START+2), (u16)(DIALOG_ROW_START+3+i));

    wait_not_response();

    while (1) {
        SYS_doVBlankProcess();
        btns = JOY_readJoypad(JOY_1);
        u16 pressed = btns & ~prev;
        prev = btns;

        // Highlight current selection with '>'
        for (i = 0; i < n; i++)
            VDP_drawText(i==sel ? ">" : " ",
                         (u16)(DIALOG_COL_START+1), (u16)(DIALOG_ROW_START+3+i));

        if ((pressed & BUTTON_DOWN) && sel < n-1) sel++;
        if ((pressed & BUTTON_UP)   && sel > 0)   sel--;
        if (pressed & (BUTTON_A|BUTTON_START)) break;
        if (pressed & BUTTON_B)     { sel = -1; break; } // cancel
    }

    wait_not_response();
    dialog_box_clear();
    VDP_setWindowVPos(TRUE, 24);
    return sel;
}

s16 option_menu(void) {
    static const char *opts[] = {
        "Sound/Music","Skill Level","Save Game","Load Game",
        "Die","Help","Quit",NULL
    };
    s16 r = simple_menu("OPTIONS", opts);
    return (r >= 0) ? r + 1 : 0;
}

s16 ask_exit(void) {
    static const char *opts[] = {"Continue","Quit to Title","Quit",NULL};
    s16 r = simple_menu("QUIT?", opts);
    return (r >= 0) ? r + 1 : 0;
}

void select_skill(void) {
    static const char *opts[] = {"Easy","Normal","Hard",NULL};
    s16 r = simple_menu("SKILL", opts);
    if (r >= 0) setup.skill = (u8)r;
}

void help(void) {
    show_dialog(
        "GOD OF THUNDER\n"
        "Arrow keys: Move\n"
        "A: Throw Hammer\n"
        "B: Use Magic Item\n"
        "C: Select Item\n"
        "Start: Options",
        14);
}

void select_fastmode(void) { /* no-op on Genesis */ }
void select_music(void)    { setup.music = !setup.music; }
s16  select_sound(void)    { return 1; }
s16  sound_playing(void)   { return XGM_isPlayingPCM(SOUND_PCM_CH_ALL); }

// ─── d_restore: refresh display after dialog ─────────────────────────────────
// DOS: redraws both VGA pages. Genesis: nothing needed (VDP is live).
void d_restore(void) { /* no-op */ }
