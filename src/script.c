// God of Thunder - Sega Genesis Port
// script.c - Level event script interpreter
//
// The DOS script system was a tiny custom BASIC-like interpreter that read
// scripts stored inside GOTRES.DAT (TEXT resource = 94 strings × 72 chars,
// plus per-NPC/level scripts embedded in the resource file).
//
// For the Genesis port, scripts are compiled into ROM as a flat byte array
// (res/scripts.bin from the original TEXT resource + NPC dialog data).
// This is a faithful port of the core execute_script logic, with DOS file I/O
// and far pointer access replaced by ROM array reads.
//
// Script commands supported:
//   END, GOTO, GOSUB, RETURN, FOR, NEXT, IF, ELSE
//   ADDJEWELS, ADDHEALTH, ADDMAGIC, ADDKEYS, ADDSCORE
//   SAY, SOUND, PLACETILE, ITEMGIVE, ITEMTAKE, ITEMSAY
//   SETFLAG, PAUSE, VISIBLE, RANDOM

#include <genesis.h>
#include <stdlib.h>
#include <string.h>
#include "god_of_thunder.h"

// ─── Script data (compiled from original TEXT + NPC scripts) ─────────────────
// Declared extern; built by tools/compile_scripts.py from GOTRES.DAT extraction.
// Fallback: empty stub so game compiles without scripts.
// Format: series of null-terminated script records, each prefixed by a 4-byte index.
__attribute__((weak)) const u8  script_data[1]    = {0};
__attribute__((weak)) const u32 script_data_size  = 1;

// ─── Script interpreter state ─────────────────────────────────────────────────
static s32  num_var[26];
static char str_var[26][81];
static const u8 *buff_ptr;
static const u8 *buff_end;
static s32  scr_index;
static s32  lvalue;

#define MAX_LABELS  32
#define MAX_GOSUB   32
#define MAX_FOR     10

static const u8 *line_ptr[MAX_LABELS];
static char      line_label[MAX_LABELS][9];
static s16       num_labels;
static const u8 *gosub_stack[MAX_GOSUB];
static s16       gosub_ptr;
static const u8 *for_stack[MAX_FOR];
static s32       for_val[MAX_FOR];
static s16       for_ptr;
static s16       cur_dialog_color;

static char temps[255];
static s16  scr_pic_idx;
static s16 get_command(void);
static s32 calc_value(void);
static void get_str(void);
static s16 exec_command(s16 num);

// ─── Command table ─────────────────────────────────────────────────────────────
static const char *scr_command[] = {
    "!@#$%","END","GOTO","GOSUB","RETURN","FOR","NEXT",
    "IF","ELSE","RUN",
    "ADDJEWELS","ADDHEALTH","ADDMAGIC","ADDKEYS",
    "ADDSCORE","SAY","ASK","SOUND","PLACETILE",
    "ITEMGIVE","ITEMTAKE","ITEMSAY","SETFLAG","LTOA",
    "PAUSE","TEXT","EXEC","VISIBLE","RANDOM",
    NULL
};

static const char *internal_var[] = {
    "@JEWELS","@HEALTH","@MAGIC","@SCORE",
    "@SCREEN","@KEYS",
    "@OW","@GULP","@SWISH","@YAH","@ELECTRIC",
    "@THUNDER","@DOOR","@FALL","@ANGEL","@WOOP",
    "@DEAD","@BRAAPP","@WIND","@PUNCH","@CLANG",
    "@EXPLODE","@FLAG","@ITEM","@THORTILE","@THORPOS",
    NULL
};

// ─── Script data lookup ───────────────────────────────────────────────────────
// Find script at given index in the ROM script_data array.
// Format: each record = [4-byte LE index][script bytes...][\0]
static const u8 *find_script(s32 index) {
    const u8 *p = script_data;
    const u8 *end = script_data + script_data_size;
    while (p + 5 < end) {
        s32 idx = (s32)( (u32)p[0] | ((u32)p[1]<<8) | ((u32)p[2]<<16) | ((u32)p[3]<<24) );
        p += 4;
        if (idx == index) return p;
        // Skip to next null terminator
        while (p < end && *p) p++;
        p++;
    }
    return NULL;
}

// ─── Skip whitespace ─────────────────────────────────────────────────────────
static void skip_ws(void) {
    while (buff_ptr < buff_end && (*buff_ptr == ' ' || *buff_ptr == '\t'))
        buff_ptr++;
}

// ─── Read a token into temps ─────────────────────────────────────────────────
static void read_token(void) {
    s16 i = 0;
    skip_ws();
    while (buff_ptr < buff_end && *buff_ptr != ' ' && *buff_ptr != '\t' &&
           *buff_ptr != '\r' && *buff_ptr != '\n' && *buff_ptr != 0 &&
           *buff_ptr != ':' && *buff_ptr != ',' && i < 254)
        temps[i++] = (char)*buff_ptr++;
    temps[i] = 0;
}

// ─── Get command number ───────────────────────────────────────────────────────
static s16 get_command(void) {
    s16 i;
    // Skip to start of line
    while (buff_ptr < buff_end && (*buff_ptr == '\r' || *buff_ptr == '\n' || *buff_ptr == ' '))
        buff_ptr++;
    if (buff_ptr >= buff_end || *buff_ptr == 0) return 1; // END
    read_token();
    for (i = 1; scr_command[i]; i++)
        if (!strcmp(temps, scr_command[i])) return i;
    return 0;
}

// ─── Parse an integer value ───────────────────────────────────────────────────
static s32 calc_value(void) {
    s16 i;
    read_token();
    // Internal variable?
    for (i = 0; internal_var[i]; i++) {
        if (!strcmp(temps, internal_var[i])) {
            switch (i) {
                case 0: return thor_info.jewels;
                case 1: return thor->health;
                case 2: return thor_info.magic;
                case 3: return thor_info.score;
                case 4: return current_level;
                case 5: return thor_info.keys;
                default: return (s32)i - 6; // sound IDs
            }
        }
    }
    // Numeric variable A-Z?
    if (temps[0] >= 'A' && temps[0] <= 'Z' && temps[1] == 0)
        return num_var[temps[0] - 'A'];
    return atoi(temps);
}

// ─── Parse a string ───────────────────────────────────────────────────────────
static void get_str(void) {
    s16 i = 0;
    skip_ws();
    // Quoted string
    if (*buff_ptr == '"') {
        buff_ptr++;
        while (buff_ptr < buff_end && *buff_ptr != '"' && *buff_ptr != 0 && i < 254)
            temps[i++] = (char)*buff_ptr++;
        if (*buff_ptr == '"') buff_ptr++;
    } else {
        read_token();
        // String variable?
        if (temps[0] >= 'A' && temps[0] <= 'Z' && temps[1] == '$' && temps[2] == 0) {
            strncpy(temps, str_var[temps[0]-'A'], 254);
            return;
        }
    }
    temps[i] = 0;
}

// ─── Get label index ─────────────────────────────────────────────────────────
static s16 find_label(const char *lbl) {
    s16 i;
    for (i = 0; i < num_labels; i++)
        if (!strcmp(line_label[i], lbl)) return i;
    return -1;
}

// ─── Pre-scan buffer for labels ──────────────────────────────────────────────
static void scan_labels(const u8 *buf, const u8 *end) {
    const u8 *p = buf;
    num_labels = 0;
    while (p < end && *p) {
        // A label line starts with ':' at column 0
        if (*p == ':') {
            p++;
            s16 i = 0;
            while (p < end && *p != '\r' && *p != '\n' && *p && i < 8)
                line_label[num_labels][i++] = (char)*p++;
            line_label[num_labels][i] = 0;
            line_ptr[num_labels] = p;
            if (++num_labels >= MAX_LABELS) break;
        }
        while (p < end && *p != '\n' && *p) p++;
        if (*p == '\n') p++;
    }
}

// ─── Execute one command ──────────────────────────────────────────────────────
static s16 exec_command(s16 cmd) {
    s16 i;
    s32 v;
    char lbl[16];

    switch (cmd) {
        case 1: // END
            return 0;

        case 2: // GOTO label
            read_token();
            i = find_label(temps);
            if (i >= 0) buff_ptr = line_ptr[i];
            break;

        case 3: // GOSUB label
            read_token();
            i = find_label(temps);
            if (i >= 0 && gosub_ptr < MAX_GOSUB) {
                gosub_stack[gosub_ptr++] = buff_ptr;
                buff_ptr = line_ptr[i];
            }
            break;

        case 4: // RETURN
            if (gosub_ptr > 0) buff_ptr = gosub_stack[--gosub_ptr];
            break;

        case 5: // FOR var = start TO end
            read_token();
            i = temps[0] - 'A';
            skip_ws(); buff_ptr++; // skip '='
            v = calc_value();
            num_var[i] = v;
            skip_ws(); buff_ptr += 2; // skip 'TO'
            v = calc_value();
            if (for_ptr < MAX_FOR) {
                for_stack[for_ptr] = buff_ptr;
                for_val[for_ptr]   = v;
                for_ptr++;
            }
            break;

        case 6: // NEXT var
            read_token();
            i = temps[0] - 'A';
            if (for_ptr > 0) {
                num_var[i]++;
                if (num_var[i] <= for_val[for_ptr-1])
                    buff_ptr = for_stack[for_ptr-1];
                else
                    for_ptr--;
            }
            break;

        case 7: { // IF expr THEN ...
            s32 a = calc_value();
            // read operator
            read_token();
            char op[3]; strncpy(op, temps, 3);
            s32 b = calc_value();
            s16 cond = 0;
            if      (!strcmp(op, "="))  cond = (a == b);
            else if (!strcmp(op, "<>")) cond = (a != b);
            else if (!strcmp(op, "<"))  cond = (a < b);
            else if (!strcmp(op, ">"))  cond = (a > b);
            else if (!strcmp(op, "<=")) cond = (a <= b);
            else if (!strcmp(op, ">=")) cond = (a >= b);
            skip_ws(); // skip THEN
            read_token();
            if (!cond) {
                // Skip to ELSE or end of line
                while (buff_ptr < buff_end && *buff_ptr != '\n' && *buff_ptr) buff_ptr++;
            }
            break;
        }

        case 10: // ADDJEWELS n
            v = calc_value(); add_jewels((s16)v); break;
        case 11: // ADDHEALTH n
            v = calc_value(); add_health((s16)v); break;
        case 12: // ADDMAGIC n
            v = calc_value(); add_magic((s16)v); break;
        case 13: // ADDKEYS n
            v = calc_value(); add_keys((s16)v); break;
        case 14: // ADDSCORE n
            v = calc_value(); add_score(v); break;

        case 15: // SAY "text"
            get_str();
            show_dialog(temps, cur_dialog_color);
            break;

        case 17: // SOUND n
            v = calc_value(); play_sound((s16)v, 1); break;

        case 18: { // PLACETILE x y tile
            s16 px = (s16)calc_value();
            s16 py = (s16)calc_value();
            s16 pt = (s16)calc_value();
            place_tile(px, py, (u8)pt);
            break;
        }

        case 19: // ITEMGIVE item
            v = calc_value();
            thor_info.inventory |= (u16)(1 << (v - 1));
            thor_info.item = (u8)v;
            display_item();
            break;

        case 20: // ITEMTAKE item
            v = calc_value();
            thor_info.inventory &= (u16)(~(1u << (v - 1)));
            display_item();
            break;

        case 22: // SETFLAG n v
            { s16 fn = (s16)calc_value(); s16 fv = (s16)calc_value();
              if (fv) SETUP_SET_FLAG(setup, fn);
              else    SETUP_CLR_FLAG(setup, fn); }
            break;

        case 24: // PAUSE n
            v = calc_value(); game_pause((s16)v); break;

        case 27: // VISIBLE actornum 0|1
            { s16 an = (s16)calc_value(); s16 vis = (s16)calc_value();
              if (an < MAX_ACTORS) { actor[an].used = (u8)vis; actor[an].show = 0; } }
            break;

        case 28: // RANDOM var min max
            read_token();
            { s16 vi = temps[0]-'A';
              s32 mn = calc_value(), mx = calc_value();
              num_var[vi] = mn + rnd((s16)(mx - mn + 1)); }
            break;

        default:
            // Skip unknown command to end of line
            while (buff_ptr < buff_end && *buff_ptr != '\n' && *buff_ptr) buff_ptr++;
            break;
    }
    return 1;
}

// ─── execute_script: main entry point ────────────────────────────────────────
void execute_script(s32 index) {
    const u8 *buf = find_script(index);
    if (!buf) return;

    const u8 *end = buf;
    while (*end) end++;   // find null terminator

    // Pre-scan labels
    scan_labels(buf, end);
    buff_ptr = buf;
    buff_end = end;

    // Reset interpreter state
    gosub_ptr = 0; for_ptr = 0;
    memset(num_var, 0, sizeof(num_var));
    cur_dialog_color = 14;

    s16 iterations = 0;
    while (buff_ptr < buff_end && *buff_ptr && iterations++ < 10000) {
        s16 cmd = get_command();
        if (cmd == 1) break;  // END
        if (!exec_command(cmd)) break;
    }
}

// ─── Boss stubs (filled in when boss files are ported) ───────────────────────
void boss_level1(void)            { /* TODO: port 1_boss1.c */ }
void closing_sequence1(void)      { /* TODO: port 1_boss1.c */ }

s16 boss1_movement(ACTOR *actr) { (void)actr; return 0; }
void check_boss1_hit(ACTOR *actr, s16 x1, s16 y1, s16 x2, s16 y2, s16 i) {
    (void)actr; (void)x1; (void)y1; (void)x2; (void)y2; (void)i;
}
