God of Thunder → Sega Genesis Port --- Project Summary
----------------------------------------------------

### What This Project Is

A full port of **God of Thunder** (1994 MS-DOS shareware game by Adept Software) to the **Sega Genesis / Mega Drive**, targeting **SGDK 1.70**. The original source code was released to the public domain in 2020. The game is a top-down action game across 3 episodes where Thor fights enemies, throws his hammer, collects jewels, and uses magic items.

* * * * *

### Session 1 --- Architecture Analysis & Porting Guide

**Read and analysed the full DOS source tree** (`got-main/`), covering ~11,500 lines across episodes 1-3 (`_g1/`, `_g2/`, `_g3/`) plus utility code.

**Produced a complete porting guide** (`GOT_Genesis_Porting_Guide.md`) documenting every system-level difference:

| DOS System | Genesis Equivalent |
| --- | --- |
| Mode X (320×200, planar VGA, 4 pages) | VDP tilemap + hardware sprites |
| 256-colour palette (8-bit) | 4×16-colour palettes (9-bit BGR) |
| Planar mask-blitter (`xfput`, `MASK_IMAGE`) | `SPR_addSprite` / `VDP_setTileMapXY` |
| Timer interrupt @ ~18 Hz | VBlank callback @ 60 fps, logic every 3rd frame |
| `far` heap (`farmalloc`) | Flat 64 KB RAM, static allocation |
| File I/O (`fopen`, `GOTRES.DAT`) | ROM arrays via `resources.res BINARY` |
| Sound Blaster / AdLib / PC Speaker | YM2612 FM (XGM driver) + PCM SFX |
| Keyboard scan codes | 3-button joypad (`JOY_readJoypad`) |
| SDAT1-3 on disk | ROM `level_data_ep{1,2,3}[]` arrays |

Key architectural decisions documented: ACTOR struct trimmed from 256 to ~100 bytes (removing `MASK_IMAGE pic[4][4]`), `Sprite*` handle added, timing scale factor of 3× for 60→20 Hz logic rate, SRAM save system replacing FAT file I/O.

* * * * *

### Session 2 --- Sprite & Background Tile Conversion

**Decoded two completely different binary formats** from `GOTRES.DAT` assets:

-   **ACTOR files** (5200 bytes each): `ACTOR_DATA` struct --- `pic[16][256]` (linear 8bpp, 16×16 frames) + `shot[4][256]` + two `ACTOR_NFO` metadata blocks. Format confirmed by reading `actordat.h`.
-   **BPICS files** (262 bytes/tile × 230 tiles): **Mode X planar format** --- 6-byte header (`w=4, h=16, offset`) followed by 4 × 64-byte planes. Each plane covers columns where `col % 4 == plane_idx`. Required custom interleave-decode logic.

**Bug found and fixed between v1 and v2:** The `PALETTE` file stores standard 8-bit RGB (0--255), not VGA DAC 6-bit values (0--63). The v1 converter multiplied by 4, blowing all colours well past 255 and producing wildly wrong hues (Thor's skin appeared white, greens became neon). Fixed by removing the `×4` scaling --- confirmed by comparing against the reference sprite sheet provided.

**Produced (v2, correct colours):**

| File | Contents |
| --- | --- |
| `res/thor.png` | 5 Thor variants, 4 dirs × 4 frames, PAL1 |
| `res/hammer.png` | 4 hammer variants, PAL1 |
| `res/fx.png` | Sparkle, explosion, tornado, shield, PAL1 |
| `res/enemies.png` | 50 enemies, PAL2 |
| `res/npcs.png` | 28 NPCs/townspeople, PAL3 |
| `res/objects.png` | 32 pickup tiles (jewels, keys, potions...), PAL0 |
| `res/bg_tiles_ep{1,2,3}.png` | 230 background tiles per episode, PAL0 |

**Also generated:**

-   `inc/palette_gen.h` --- 4 Genesis hardware palettes (9-bit BGR) + `got_load_all_palettes()` inline
-   `inc/actor_ids.h` --- `ActorID` enum for all 90 actors
-   `inc/actor_data.h` --- Full metadata table (dims, speed, AI func IDs, palette line, 90 actors)
-   `res/resources.res` --- SGDK 1.70 resource descriptor stub

**Palette strategy:** Four groups, each with its own 15-colour Genesis palette built by frequency-weighted selection with minimum perceptual distance enforcement to avoid near-duplicate colour slots. PAL0=background, PAL1=Thor+hammer+FX, PAL2=enemies+shots, PAL3=NPCs+objects+HUD.

* * * * *

### Session 3 --- Core Source Port (16 C files, ~6,800 lines)

Ported every gameplay module. Files written:

#### `src/god_of_thunder.h` --- Master header

Complete replacement for `1_define.h` + `1_proto.h`. Contains:

-   Trimmed `ACTOR` struct (DOS graphics fields removed, `Sprite *spr` added, `last_x[2]`/`last_y[2]` double-buffer positions kept for compatibility with movement logic)
-   `THOR_INFO`, `SETUP` structs (64-bit DOS bitfield replaced with two `u32` + helper macros)
-   `LEVEL` struct (binary-compatible with original, safe to `memcpy` from ROM)
-   All `#define` constants, sound IDs, direction codes, magic flags
-   Full prototype list for all functions

#### `src/main.c` --- Game loop

-   VBlank callback → `SPR_update()` call
-   3-frame logic tick accumulator (20 Hz game speed)
-   `game_init()` / `game_loop()` replacing DOS `main()`
-   `thor_dies()` --- palette-flash death + checkpoint restore
-   `thor_spins()` --- death spin animation
-   Score/magic/jewel helpers

#### `src/back.c` --- Level rendering & transitions

-   `build_screen()` --- draws 20×12 tilemap to VDP `BG_A` using 2×2 sub-tile decomposition
-   `show_level()` --- loads level from ROM, calls `build_screen()`, handles checkpoint save
-   Scroll transitions (`scroll_level_left/right/up/down`) --- animate VDP horizontal/vertical scroll register over N VBlanks
-   `phase_level()` --- random tile reveal (replicates DOS random-order blit)
-   `fade_in()` / `fade_out()` --- scale all 4 palettes 0→full over 8 steps
-   `switch_icons()` / `rotate_arrows()` --- tile state machines
-   `kill_enemies()` --- collision against newly-solid tiles
-   `place_tile()` / `remove_objects()` --- tile mutation helpers

#### `src/move.c` --- Movement & combat

Pure logic port from `1_move.c` --- zero hardware dependencies, only type changes:

-   `overlap()` / `point_within()` --- AABB collision
-   `move_actor()` --- per-tick update: invulnerability, shot cooldown, `movement_func[]` dispatch
-   `actor_damaged()` / `thor_damaged()` --- skill-scaled damage
-   `actor_destroyed()` --- replace with sparkle/explosion template
-   `_actor_shoots()` / `actor_shoots()` --- line-of-sight projectile spawning
-   `thor_shoots()` --- hammer throw

#### `src/movpat.c` --- 41 movement patterns (2,259 lines)

Automated port from `1_movpat.c` with targeted fixes:

-   `int` → `s16` throughout (sed transform)
-   `key_flag[key_up/down/left/right/fire]` → `JOY_readJoypad(JOY_1) & BUTTON_*`
-   DOS inline assembly (`asm mov dx,...`) stripped
-   `xfput` / `xshowpage` calls removed/replaced
-   Function pointer table `movement_func[]` reconstructed at end of file
-   Complete forward-declaration block added (required because `movement_twentynine` calls `movement_thirty` before its definition)

#### `src/shtmov.c` --- 11 shot movement functions

#### `src/shtpat.c` --- 8 shot pattern functions

Both ported with same automated transforms. Fully self-contained logic.

#### `src/image.c` --- Sprite management

Full replacement for `1_image.c` (which was entirely Mode X blitter code):

-   `load_actor_sprite()` → `SPR_addSprite()` with correct palette line
-   `free_actor_sprite()` → `SPR_releaseSprite()`
-   `actor_set_anim()` → `SPR_setPosition()` + `SPR_setAnimAndFrame()` + blink visibility
-   `update_sprites()` --- bulk sync called every logic tick

#### `src/panel.c` --- HUD

-   VDP `WINDOW` plane at bottom 32 screen lines (rows 24-27)
-   Health/magic bars as coloured tile rows (cached, only redrawn on change)
-   Jewels/keys/score/item as `VDP_drawText()` strings
-   `hud_init()` draws static labels

#### `src/object.c` --- Pickups & magic items

-   `show_objects()` --- draws all 30 static pickups per level as object tiles on BG_A
-   `pick_up_object()` --- full 32-case switch for all pickup types (jewels, potions, apples, keys, magic items, carried objects)
-   `use_item()` --- dispatches to `use_apple/lightning/boots/tornado/shield/thunder/object` based on `thor_info.item`
-   Lightning: palette flash on PAL1 instead of VGA pixel drawing

#### `src/sptile.c` --- Special tile interactions

-   `special_tile_thor()` --- doors, warp holes, one-way arrows, ending bridge, coin doors
-   `special_tile()` --- enemy collision with special tiles
-   `open_door1()` / `cash_door1()` --- key-locked and jewel-locked doors

#### `src/sound.c` --- Audio system

-   XGM music driver integration (6 tracks, one per `scrn.type`)
-   PCM sound effects via `XGM_setPCM` / `XGM_startPlayPCM` (19 SFX slots)
-   Priority system preserved from original
-   `music_pause()` / `music_resume()` wrappers

#### `src/file.c` --- Save/load via SRAM

-   `SaveData` struct: `THOR_INFO` + `SETUP` + level/area
-   Magic number `0x474F5421` ("GOT!") + 16-bit checksum for corruption detection
-   `SRAM_enable()` / `SRAM_writeBuffer()` / `SRAM_readBuffer()` / `SRAM_disable()`

#### `src/script.c` --- Level event script interpreter + boss stubs

-   Minimal but functional interpreter for the original BASIC-like script language
-   Supports: `END`, `GOTO`, `GOSUB/RETURN`, `FOR/NEXT`, `IF`, `ADDJEWELS/HEALTH/MAGIC/KEYS/SCORE`, `SAY`, `SOUND`, `PLACETILE`, `ITEMGIVE/TAKE`, `SETFLAG`, `PAUSE`, `VISIBLE`, `RANDOM`
-   Script data looked up by 32-bit index from ROM `script_data[]` array
-   `boss_level1()` / `closing_sequence1()` / `check_boss1_hit()` stubbed (compilable no-ops)

#### `src/dialog.c` --- Menus & dialog boxes

-   `show_dialog()` --- word-wrapped text in VDP WINDOW plane overlay, A-button to dismiss
-   `select_item()` --- D-pad item selection menu
-   `option_menu()` / `ask_exit()` / `select_skill()` / `help()` --- all functional

#### `src/init.c` --- Game initialisation

-   `setup_actor()` --- initialises all ACTOR runtime fields (direct port of DOS version)
-   `copy_actor_nfo()` --- copies metadata from `actor_meta_table[]` ROM array into live ACTOR
-   `load_standard_actors()` --- sets up Thor, Hammer, sparkle, explosion, tornado, shield templates + shot array from actor metadata
-   `setup_player()` --- resets `THOR_INFO` for new game
-   `initialize()` --- top-level init called from `game_init()`

#### `src/globals.c` --- Global definitions

-   `thor_icon1-4` (corner collision flags used by movement patterns)
-   `apple_drop` counter
-   `dialog_func[]` no-op table
-   `OBJ_TILE_VRAM_BASE_VAL` constant

* * * * *

### Session 4 --- Making It Compilable

**Diagnosed and fixed 20+ distinct compiler errors** via iterative `gcc -fsyntax-only` checks using a host-GCC SGDK stub (`/tmp/fakeinc/genesis.h`).

Key fixes applied:

| Error | Root cause | Fix |
| --- | --- | --- |
| `ACTOR has no member last_x` | Field added after wrong struct | Found correct struct, inserted `last_x[2]`/`last_y[2]` |
| `ACTOR_META has no member move` | Field never added to generated header | Removed `meta->move` references, default to 0 |
| `movement_thirty redeclared` | Orphaned function table tail from bad `tail -n +125` split | Removed fragment, rebuilt complete table at EOF |
| `movement_thirty conflicting types` | Called before declared, implicit int created | Added 50-entry forward-declaration block at top |
| `ANGEL`/`EXPLODE` undeclared | DOS sound name used, not in Genesis header | Mapped to `ANGEL_SND`/`EXPLODE_SND` |
| `execute_script(lind, odin)` --- too many args | Genesis version takes 1 arg (no face picture) | `re.sub` replaced all 2-arg calls |
| `key_flag[_ONE]` undeclared | DOS cheat key remnant | Replaced with `0` |
| `BOSS12` undeclared in shtpat.c | Sound constant not defined | Added `#define BOSS12 17` |
| `apple_drop` undeclared in shtmov.c | Global defined but `extern` missing | Added `extern u8 apple_drop` + defined in globals.c |
| `warp_scroll` undeclared in sptile.c | Missing from header | Added to `god_of_thunder.h` and `main.c` |
| `bgtile` conflicting types | `int` return vs `s16` declaration | Fixed definition to `s16` |
| `erase_door` static conflict | `static` in definition, non-static in header | Removed `static` |
| `TILE_ATTR` 4-arg call | Macro requires 5 args | Added missing tile index |
| `calc_value` conflicting types | Forward declared `s16`, defined `s32` | Unified to `s32` |
| Weak-symbol linker conflict | Stubs in globals.c conflicted with SGDK-generated symbols | Removed stubs, SGDK resources.res provides real symbols |

**Final result: 0 errors, 0 relevant warnings** across all 16 source files.

* * * * *

### Asset Pipeline

**`convert_assets.py`** --- Decodes raw DOS assets → Genesis-ready PNGs + C headers\
**`tools/pack_assets.py`** --- Packs SDAT1-3 and ACTOR files into BINARY resources\
**`res/resources.res`** --- Complete SGDK resource descriptor

Running `python3 tools/pack_assets.py` produces:

-   `res/level_data_ep{1,2,3}.bin` (60 KB each, 120 levels)
-   `res/actor_rom_data.bin` (579 KB, 90 actors × 5200 bytes)

* * * * *

### What Remains Before the Game Runs

The code compiles cleanly. Three things remain to reach a playable ROM:

1.  **Music & SFX** --- Add XGM music files (convert original MIDI/songs via `xgmtool`) and PCM WAV files (extract from `DIGSOUND` resource), uncomment the corresponding lines in `resources.res`
2.  **Boss fights** --- `boss_level1()` and friends in `script.c` are compilable stubs. Port `1_boss1.c`, `1_boss21.c`, `1_boss22.c` to fill them in
3.  **Script data** --- Write `tools/compile_scripts.py` to extract NPC dialog scripts from `GOTRES.DAT` and pack them into `res/script_data.bin`, then add `BINARY script_data "res/script_data.bin"` to `resources.res`