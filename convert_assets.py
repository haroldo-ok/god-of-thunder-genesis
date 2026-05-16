#!/usr/bin/env python3
"""
God of Thunder → Sega Genesis / SGDK 1.70
Asset Converter: Sprites & Background Tiles  (v2 - correct palette)

Fixes vs v1:
  - PALETTE file stores 8-bit RGB (0-255), NOT 6-bit VGA DAC (0-63). No *4 scaling.
  - palette index 0 = transparent in sprites; palette index 15 = visible near-white
  - Per-group palette selection: each Genesis palette is built from only the colors
    that group actually uses, with median-cut in 9-bit Genesis color space.
  - Transparent slot is always Genesis palette index 0; real colors start at index 1.
"""

import os, struct
from pathlib import Path
from PIL import Image
from collections import Counter

ASSETS_DIR = Path("/tmp/got-main/assets")
OUT_DIR    = Path("/home/claude/got_genesis")
RES_DIR    = OUT_DIR / "res"
INC_DIR    = OUT_DIR / "inc"
RES_DIR.mkdir(exist_ok=True)
INC_DIR.mkdir(exist_ok=True)

# ── Palette ──────────────────────────────────────────────────────────────────
# Values are 8-bit (0-255) - NOT VGA 6-bit DAC values. Use as-is.
with open(ASSETS_DIR / "PALETTE", "rb") as f:
    pal_raw = f.read()
VGA_PALETTE = [(pal_raw[i*3], pal_raw[i*3+1], pal_raw[i*3+2]) for i in range(256)]

TRANSPARENT_VGA_0  = 0    # black - transparent in enemy/background tiles
TRANSPARENT_VGA_15 = 15   # near-white - transparent in Thor/Hammer sprites
TRANSPARENT_VGA    = 0    # kept for any remaining references

# ── Genesis color conversion ──────────────────────────────────────────────────
def rgb8_to_genesis(r, g, b):
    """8-bit RGB → Genesis 9-bit BGR word (3 bits per channel)."""
    return ((b >> 5) << 8) | ((g >> 5) << 4) | (r >> 5)

def genesis_to_rgb8(gc):
    r3 = gc & 0x7
    g3 = (gc >> 4) & 0x7
    b3 = (gc >> 8) & 0x7
    # Expand 3-bit to 8-bit: replicate upper bits into lower
    def e(v): return (v << 5) | (v << 2) | (v >> 1)
    return (e(r3), e(g3), e(b3))

def color_dist_sq(c1, c2):
    return sum((a - b) ** 2 for a, b in zip(c1, c2))

# ── Median-cut palette builder ────────────────────────────────────────────────
def build_genesis_palette_for_colors(vga_indices: list, n_slots: int = 15) -> list:
    """
    Given a list of VGA palette indices (raw pixel values from sprites),
    build a Genesis palette of n_slots entries (slot 0 = transparent, excluded here).
    Returns list of n_slots Genesis color words (to be placed at slots 1..15).
    """
    # Count usage frequency in 9-bit Genesis space
    gen_counter = Counter()
    for idx in vga_indices:
        if idx == TRANSPARENT_VGA_0 or idx == TRANSPARENT_VGA_15:
            continue
        r, g, b = VGA_PALETTE[idx]
        gc = rgb8_to_genesis(r, g, b)
        gen_counter[gc] += 1

    if not gen_counter:
        return [0] * n_slots

    # Use frequency-weighted selection with spread: pick most-used colors but
    # enforce minimum perceptual distance to avoid picking near-duplicates.
    selected = []
    remaining = list(gen_counter.items())  # (gc, count)
    remaining.sort(key=lambda x: -x[1])

    MIN_DIST_SQ = 1  # only skip exact duplicates; keep all perceptually distinct colors

    for gc, count in remaining:
        if len(selected) >= n_slots:
            break
        rgb = genesis_to_rgb8(gc)
        # Check if this color is already well-represented
        too_close = any(
            color_dist_sq(rgb, genesis_to_rgb8(s)) < MIN_DIST_SQ
            for s in selected
        )
        if not too_close:
            selected.append(gc)

    # Pad if needed
    while len(selected) < n_slots:
        selected.append(0x000)  # black filler

    return selected[:n_slots]

# ── Mode X tile decoder ───────────────────────────────────────────────────────
def decode_modex_tile(data: bytes) -> list:
    """262-byte Mode X tile → list of 16 rows × 16 palette indices."""
    pixels = [[0] * 16 for _ in range(16)]
    for plane in range(4):
        base = 6 + plane * 64
        for row in range(16):
            for col_idx in range(4):
                pixels[row][col_idx * 4 + plane] = data[base + row * 4 + col_idx]
    return pixels

# ── Load all ACTOR files ──────────────────────────────────────────────────────
ACTOR_TYPE_NAMES = {0: "THOR", 1: "HAMMER", 2: "ENEMY", 3: "SHOT", 4: "NPC"}
NFO_OFFSET = 4096 + 1024

def load_actor(n):
    path = ASSETS_DIR / f"ACTOR{n}"
    if not path.exists():
        return None
    with open(path, "rb") as f:
        data = f.read()
    nfo = data[NFO_OFFSET:NFO_OFFSET + 40]
    move, width, height, directions, frames, frame_speed = struct.unpack_from("6b", nfo, 0)
    frame_sequence = list(struct.unpack_from("4b", nfo, 6))
    speed = struct.unpack_from("b", nfo, 10)[0]
    solid, flying, rating, atype = struct.unpack_from("4b", nfo, 19)
    name = nfo[23:32].split(b'\x00')[0].decode("latin-1", errors="replace").strip()
    func_num, func_pass = struct.unpack_from("2b", nfo, 32)
    shot_nfo = data[NFO_OFFSET + 40:NFO_OFFSET + 80]
    shot_width  = struct.unpack_from("b", shot_nfo, 1)[0] & 0xFF
    shot_height = struct.unpack_from("b", shot_nfo, 2)[0] & 0xFF
    strength    = struct.unpack_from("b", nfo, 11)[0]
    health      = struct.unpack_from("b", nfo, 12)[0]
    shot_type   = struct.unpack_from("b", nfo, 14)[0] & 0xFF

    frames_data = [list(data[fi * 256:(fi + 1) * 256]) for fi in range(16)]
    shot_frames = [list(data[4096 + fi * 256:4096 + (fi + 1) * 256]) for fi in range(4)]

    return dict(
        id=n, name=name,
        width=width & 0xFF, height=height & 0xFF,
        directions=directions & 0xFF, frames=frames & 0xFF,
        frame_speed=frame_speed & 0xFF, frame_sequence=[x & 0xFF for x in frame_sequence],
        speed=speed & 0xFF, type=atype & 0xFF,
        type_name=ACTOR_TYPE_NAMES.get(atype & 0xFF, f"UNK{atype}"),
        solid=solid & 0xFF, shot_type=shot_type,
        strength=strength, health=health & 0xFF,
        func_num=func_num & 0xFF, func_pass=func_pass & 0xFF,
        frames_data=frames_data, shot_frames=shot_frames,
        shot_width=shot_width, shot_height=shot_height,
    )

print("Loading actors...")
actors = {i: a for i in range(1, 120) if (a := load_actor(i))}
print(f"  {len(actors)} actors loaded")

# ── Group actors ──────────────────────────────────────────────────────────────
thor_ids    = [98, 100, 101, 102, 110]
hammer_ids  = [103, 104, 105, 113]
fx_ids      = [106, 107, 108, 109]
pal1_ids    = thor_ids + hammer_ids + fx_ids
enemy_ids   = [i for i, a in actors.items()
               if a["type_name"] == "ENEMY" and i not in fx_ids]
npc_ids     = [i for i, a in actors.items() if a["type_name"] == "NPC"]
shot_ids    = [i for i, a in actors.items() if a["type_name"] == "SHOT"]

def collect_actor_pixels(id_list):
    pixels = []
    for aid in id_list:
        if aid not in actors:
            continue
        a = actors[aid]
        n = max(1, min(16, a["directions"] * a["frames"]))
        for fi in range(n):
            pixels.extend(a["frames_data"][fi])
        for frame in a["shot_frames"]:
            pixels.extend(frame)
    return pixels

# ── Build Genesis palettes ────────────────────────────────────────────────────
print("Building Genesis palettes...")

# PAL0 - background tiles (BPICS1)
with open(ASSETS_DIR / "BPICS1", "rb") as f:
    bpics1_raw = f.read()
bg_pixels = []
for ti in range(230):
    td = bpics1_raw[ti * 262:(ti + 1) * 262]
    for plane in range(4):
        bg_pixels.extend(td[6 + plane * 64: 6 + (plane + 1) * 64])

# Objects tileset (loaded separately for PAL0 mapping)
with open(ASSETS_DIR / "OBJECTS", "rb") as f:
    obj_raw = f.read()

pal1_pixels = collect_actor_pixels(pal1_ids)
pal2_pixels = collect_actor_pixels(enemy_ids + shot_ids)
pal3_pixels = collect_actor_pixels(npc_ids)

# PAL0: usage-weighted analysis of Episode 1 shows these 15 colors are optimal.
# Grass green (0x0050) is the #1 most-used color but gets crowded out by
# frequency-only selection - usage weighting correctly promotes it to rank 1.
GEN_PAL0_COLORS = [
    0x0050,  # #00b600 bright grass (most used tile in ep1)
    0x0040,  # #009200 medium grass
    0x0012,  # #492400 brown dirt
    0x0020,  # #004900 dark green (tree shadows)
    0x0011,  # #242400 dark brown/olive
    0x0235,  # #b66d49 light brown
    0x0246,  # #db9249 tan/sand
    0x0030,  # #006d00 tree green
    0x0122,  # #494924 olive
    0x0630,  # #006ddb light blue water
    0x0247,  # #ff9249 light tan
    0x0700,  # #0000ff blue water
    0x0024,  # #924900 dark brown
    0x0037,  # #ff6d00 orange
    0x0111,  # #242424 dark grey
]
GEN_PAL1_COLORS = build_genesis_palette_for_colors(pal1_pixels, 15)
GEN_PAL2_COLORS = build_genesis_palette_for_colors(pal2_pixels, 15)
GEN_PAL3_COLORS = build_genesis_palette_for_colors(pal3_pixels, 15)

# Full 16-entry palettes (slot 0 = 0x0000 transparent)
GEN_PAL0 = [0x0000] + GEN_PAL0_COLORS
GEN_PAL1 = [0x0000] + GEN_PAL1_COLORS
GEN_PAL2 = [0x0000] + GEN_PAL2_COLORS
GEN_PAL3 = [0x0000] + GEN_PAL3_COLORS

PAL0_RGB = [genesis_to_rgb8(c) for c in GEN_PAL0]
PAL1_RGB = [genesis_to_rgb8(c) for c in GEN_PAL1]
PAL2_RGB = [genesis_to_rgb8(c) for c in GEN_PAL2]
PAL3_RGB = [genesis_to_rgb8(c) for c in GEN_PAL3]

def pal_summary(gen_pal, pal_rgb, name):
    n = sum(1 for c in gen_pal if c != 0)
    samples = [f"#{r:02x}{g:02x}{b:02x}" for r,g,b in pal_rgb[1:6]]
    print(f"  {name}: {n} colors  e.g. {' '.join(samples)}")

pal_summary(GEN_PAL0, PAL0_RGB, "PAL0 bg    ")
pal_summary(GEN_PAL1, PAL1_RGB, "PAL1 thor  ")
pal_summary(GEN_PAL2, PAL2_RGB, "PAL2 enemy ")
pal_summary(GEN_PAL3, PAL3_RGB, "PAL3 npc   ")

# ── Pixel remapping ───────────────────────────────────────────────────────────
# Build a fast lookup table: VGA index → Genesis palette index, per palette
def build_remap_lut(target_pal_rgb: list) -> list:
    """Returns list of 256 entries: VGA index → Genesis palette slot (0-15)."""
    lut = [0] * 256
    for vga_idx in range(256):
        if vga_idx == TRANSPARENT_VGA_0 or vga_idx == TRANSPARENT_VGA_15:
            lut[vga_idx] = 0  # transparent (both black and near-white are bg in sprites)
            continue
        r, g, b = VGA_PALETTE[vga_idx]
        best_slot = 1
        best_dist = float('inf')
        for slot in range(1, 16):  # skip slot 0 (transparent)
            pr, pg, pb = target_pal_rgb[slot]
            d = (r - pr) ** 2 + (g - pg) ** 2 + (b - pb) ** 2
            if d < best_dist:
                best_dist = d
                best_slot = slot
        lut[vga_idx] = best_slot
    return lut

LUT0 = build_remap_lut(PAL0_RGB)
LUT1 = build_remap_lut(PAL1_RGB)
LUT2 = build_remap_lut(PAL2_RGB)
LUT3 = build_remap_lut(PAL3_RGB)

# ── Sprite sheet builder ──────────────────────────────────────────────────────
TILE = 16  # sprite dimension in pixels
MAX_DIRS  = 4
MAX_FRMS  = 4

def make_sprite_sheet(actor_list, lut, pal_rgb, filename, cols=16):
    if not actor_list:
        print(f"  SKIP (empty): {filename}")
        return
    n = len(actor_list)
    sheet_cols = min(n, cols)
    sheet_rows = (n + cols - 1) // cols
    img = Image.new("P", (sheet_cols * MAX_FRMS * TILE, sheet_rows * MAX_DIRS * TILE), 0)
    flat = []
    for r, g, b in pal_rgb:
        flat += [r, g, b]
    flat += [0] * (768 - len(flat))
    img.putpalette(flat)

    for ai, actor in enumerate(actor_list):
        bx = (ai % cols) * MAX_FRMS * TILE
        by = (ai // cols) * MAX_DIRS * TILE
        n_dirs = max(1, min(MAX_DIRS, actor["directions"]))
        n_frms = max(1, min(MAX_FRMS, actor["frames"]))
        for d in range(MAX_DIRS):
            for f in range(MAX_FRMS):
                if d < n_dirs and f < n_frms:
                    fi = min(d * n_frms + f, 15)
                    raw = actor["frames_data"][fi]
                else:
                    raw = [0] * 256
                px = bx + f * TILE
                py = by + d * TILE
                for row in range(TILE):
                    for col in range(TILE):
                        img.putpixel((px + col, py + row), lut[raw[row * TILE + col]])

    img.save(filename)
    print(f"  {Path(filename).name}  {img.width}×{img.height}  ({n} actors)")

# ── Background tileset builder ────────────────────────────────────────────────
def make_bg_tileset(bpics_raw, lut, pal_rgb, filename, n_tiles=230, per_row=16):
    rows = (n_tiles + per_row - 1) // per_row
    img = Image.new("P", (per_row * TILE, rows * TILE), 0)
    flat = []
    for r, g, b in pal_rgb:
        flat += [r, g, b]
    flat += [0] * (768 - len(flat))
    img.putpalette(flat)
    for ti in range(n_tiles):
        td = bpics_raw[ti * 262:(ti + 1) * 262]
        pxs = decode_modex_tile(td)
        tx = (ti % per_row) * TILE
        ty = (ti // per_row) * TILE
        for row in range(16):
            for col in range(16):
                img.putpixel((tx + col, ty + row), lut[pxs[row][col]])
    img.save(filename)
    print(f"  {Path(filename).name}  {img.width}×{img.height}  ({n_tiles} tiles)")

def make_obj_tileset(obj_raw, lut, pal_rgb, filename, n_tiles=32):
    per_row = 16
    rows = (n_tiles + per_row - 1) // per_row
    img = Image.new("P", (per_row * TILE, rows * TILE), 0)
    flat = []
    for r, g, b in pal_rgb:
        flat += [r, g, b]
    flat += [0] * (768 - len(flat))
    img.putpalette(flat)
    for ti in range(n_tiles):
        td = obj_raw[ti * 262:(ti + 1) * 262]
        pxs = decode_modex_tile(td)
        tx = (ti % per_row) * TILE
        ty = (ti // per_row) * TILE
        for row in range(16):
            for col in range(16):
                img.putpixel((tx + col, ty + row), lut[pxs[row][col]])
    img.save(filename)
    print(f"  {Path(filename).name}  {img.width}×{img.height}  ({n_tiles} tiles)")

# ── Generate all images ───────────────────────────────────────────────────────
print("\nGenerating sprite sheets...")

def al(ids): return [actors[i] for i in sorted(ids) if i in actors]

make_sprite_sheet(al(thor_ids),   LUT1, PAL1_RGB, str(RES_DIR/"thor.png"),   cols=8)
make_sprite_sheet(al(hammer_ids), LUT1, PAL1_RGB, str(RES_DIR/"hammer.png"), cols=8)
make_sprite_sheet(al(fx_ids),     LUT1, PAL1_RGB, str(RES_DIR/"fx.png"),     cols=8)
make_sprite_sheet(al(enemy_ids),  LUT2, PAL2_RGB, str(RES_DIR/"enemies.png"),cols=16)
make_sprite_sheet(al(npc_ids),    LUT3, PAL3_RGB, str(RES_DIR/"npcs.png"),   cols=16)
make_sprite_sheet(al(shot_ids),   LUT2, PAL2_RGB, str(RES_DIR/"shots.png"),  cols=8)

print("\nGenerating background tilesets...")
for ep in range(1, 4):
    p = ASSETS_DIR / f"BPICS{ep}"
    if p.exists():
        with open(p, "rb") as f:
            make_bg_tileset(f.read(), LUT0, PAL0_RGB, str(RES_DIR/f"bg_tiles_ep{ep}.png"))

print("\nGenerating object tileset...")
make_obj_tileset(obj_raw, LUT0, PAL0_RGB, str(RES_DIR/"objects.png"))

# ── Generate C headers ────────────────────────────────────────────────────────
print("\nGenerating C headers...")

# palette_gen.h
with open(INC_DIR / "palette_gen.h", "w") as f:
    f.write("// God of Thunder - Genesis Hardware Palettes\n")
    f.write("// Auto-generated by convert_assets.py  (v2 - correct 8-bit palette)\n")
    f.write("// Format: 0x0BGR  (Genesis native, 9-bit: 3 bits per channel)\n")
    f.write("// Slot 0 in every palette = transparent (never rendered by VDP)\n\n")
    f.write("#ifndef PALETTE_GEN_H\n#define PALETTE_GEN_H\n\n#include <genesis.h>\n\n")
    for label, pal, desc in [
        ("bg",    GEN_PAL0, "Background / environment tiles (BPICS)"),
        ("thor",  GEN_PAL1, "Thor, Hammer, FX (sparkle, explosion)"),
        ("enemy", GEN_PAL2, "Enemies and shot projectiles"),
        ("npc",   GEN_PAL3, "NPCs, pickup objects, HUD"),
    ]:
        f.write(f"// {desc}\nstatic const u16 got_pal_{label}[16] = {{\n")
        for gc in pal:
            r, g, b = genesis_to_rgb8(gc)
            f.write(f"    0x{gc:04X},  /* #{r:02x}{g:02x}{b:02x} */\n")
        f.write("};\n\n")
    f.write("static inline void got_load_all_palettes(void) {\n")
    f.write("    PAL_setPalette(PAL0, got_pal_bg,    DMA);\n")
    f.write("    PAL_setPalette(PAL1, got_pal_thor,  DMA);\n")
    f.write("    PAL_setPalette(PAL2, got_pal_enemy, DMA);\n")
    f.write("    PAL_setPalette(PAL3, got_pal_npc,   DMA);\n")
    f.write("}\n\n#endif // PALETTE_GEN_H\n")
print("  inc/palette_gen.h")

# actor_ids.h
with open(INC_DIR / "actor_ids.h", "w") as f:
    f.write("// God of Thunder - Actor IDs\n// Auto-generated\n\n")
    f.write("#ifndef ACTOR_IDS_H\n#define ACTOR_IDS_H\n\n")
    f.write("#define ATYPE_THOR   0\n#define ATYPE_HAMMER 1\n")
    f.write("#define ATYPE_ENEMY  2\n#define ATYPE_SHOT   3\n#define ATYPE_NPC    4\n\n")
    f.write("// Well-known actor IDs\n")
    f.write("#define ACT_THOR_IRON    98\n#define ACT_THOR_NORMAL 100\n")
    f.write("#define ACT_THOR_BLUE   101\n#define ACT_THOR_GOLD   102\n")
    f.write("#define ACT_THOR_GOLD2  110\n")
    f.write("#define ACT_HAMMER_IRON  103\n#define ACT_HAMMER_STONE 104\n")
    f.write("#define ACT_HAMMER_GOLD  105\n#define ACT_HAMMER_GOLD2 113\n")
    f.write("#define ACT_SPARKLE 106\n#define ACT_EXPLODE 107\n")
    f.write("#define ACT_TORNADO 108\n#define ACT_SHIELD  109\n\n")
    f.write("typedef enum {\n")
    for aid, a in sorted(actors.items()):
        tag = ''.join(c if c.isalnum() or c == '_' else '_' for c in a['name'].upper())
        if not tag or tag[0].isdigit(): tag = f"A{tag}"
        f.write(f"    ACT_{tag}_{aid} = {aid},\n")
    f.write("} ActorID;\n\n#endif\n")
print("  inc/actor_ids.h")

# actor_data.h
def actor_pal_line(actor):
    if actor["id"] in pal1_ids: return 1
    if actor["type_name"] in ("ENEMY", "SHOT"): return 2
    if actor["type_name"] == "NPC": return 3
    return 2

with open(INC_DIR / "actor_data.h", "w") as f:
    f.write("// God of Thunder - Actor Metadata\n// Auto-generated\n\n")
    f.write("#ifndef ACTOR_DATA_H\n#define ACTOR_DATA_H\n\n")
    f.write('#include "actor_ids.h"\n\n')
    f.write("typedef struct {\n  u8 id, atype, width, height;\n")
    f.write("  u8 directions, frames, frame_speed;\n  u8 frame_seq[4];\n")
    f.write("  u8 speed, solid, shot_type;\n")
    f.write("  u8 func_num, func_pass, pal_line;\n")
    f.write("  const char* name;\n} ACTOR_META;\n\n")
    f.write("static const ACTOR_META actor_meta_table[] = {\n")
    for aid, a in sorted(actors.items()):
        seq = a["frame_sequence"]
        nm = a["name"].replace('"', '\\"')
        f.write(f"    {{{aid},{a['type']&0xFF},{a['width']},{a['height']},"
                f"{a['directions']},{a['frames']},{a['frame_speed']},"
                f"{{{seq[0]},{seq[1]},{seq[2]},{seq[3]}}},"
                f"{a['speed']},{a['solid']&0xFF},{a['shot_type']},"
                f"{a['func_num']&0xFF},{a['func_pass']&0xFF},"
                f"{actor_pal_line(a)},\"{nm}\"}},\n")
    f.write("    {0}\n};\n")
    f.write(f"#define ACTOR_META_COUNT {len(actors)}\n\n")
    f.write("static inline const ACTOR_META* actor_meta_get(u8 id) {\n")
    f.write("    for(int i=0;i<ACTOR_META_COUNT;i++)\n")
    f.write("        if(actor_meta_table[i].id==id) return &actor_meta_table[i];\n")
    f.write("    return NULL;\n}\n\n#endif\n")
print("  inc/actor_data.h")

# resources.res
with open(RES_DIR / "resources.res", "w") as f:
    f.write("// God of Thunder - SGDK 1.70 Resource Descriptor\n// Auto-generated\n\n")
    f.write("// Background tilesets (230 tiles × 16×16 px, PAL0)\n")
    for ep in range(1, 4):
        if (ASSETS_DIR / f"BPICS{ep}").exists():
            f.write(f'TILESET bg_tiles_ep{ep}  "bg_tiles_ep{ep}.png"  BEST 0\n')
    f.write('\nTILESET obj_tiles  "objects.png"  BEST 0\n\n')
    f.write("// Sprites: 2×2 tiles = 16×16 px per frame\n")
    f.write('SPRITE thor_spr    "thor.png"    2 2  BEST 0\n')
    f.write('SPRITE hammer_spr  "hammer.png"  2 2  BEST 0\n')
    f.write('SPRITE fx_spr      "fx.png"      2 2  BEST 0\n')
    f.write('SPRITE enemy_spr   "enemies.png" 2 2  BEST 0\n')
    f.write('SPRITE npc_spr     "npcs.png"    2 2  BEST 0\n')
    if (RES_DIR / "shots.png").exists():
        f.write('SPRITE shot_spr    "shots.png"   2 2  BEST 0\n')
print("  res/resources.res")

print("\n✓ Done — all assets regenerated with corrected palette.")
