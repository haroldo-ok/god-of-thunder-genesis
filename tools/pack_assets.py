#!/usr/bin/env python3
"""
God of Thunder - Genesis Port
tools/pack_assets.py

Packs raw DOS assets into binary files ready for SGDK's BINARY resource type.

Produces:
  res/level_data_ep1.bin   SDAT1 - 120 levels × 512 bytes
  res/level_data_ep2.bin   SDAT2
  res/level_data_ep3.bin   SDAT3
  res/actor_rom_data.bin   All ACTOR{n} files, ID-indexed (114 × 5200 bytes)

Usage (from project root):
  python3 tools/pack_assets.py [--assets /path/to/got-main/assets]
"""

import sys, argparse
from pathlib import Path

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--assets', default='/tmp/got-main/assets',
                    help='Path to got-main/assets directory')
    args = ap.parse_args()

    ASSETS = Path(args.assets)
    RES    = Path('res')
    RES.mkdir(exist_ok=True)

    if not ASSETS.exists():
        print(f"ERROR: assets dir not found: {ASSETS}", file=sys.stderr)
        sys.exit(1)

    # ── Level data ────────────────────────────────────────────────────────────
    for ep in range(1, 4):
        src = ASSETS / f"SDAT{ep}"
        dst = RES / f"level_data_ep{ep}.bin"
        if not src.exists():
            print(f"  WARNING: {src} not found, writing zero stub")
            dst.write_bytes(b'\x00' * 61440)
            continue
        data = src.read_bytes()
        if len(data) != 61440:
            print(f"  WARNING: {src}: expected 61440 bytes, got {len(data)}")
        dst.write_bytes(data)
        print(f"  {dst}  ({len(data):,} bytes, {len(data)//512} levels)")

    # ── Actor ROM data ─────────────────────────────────────────────────────────
    MAX_ACTOR_ID = 114
    ACTOR_SIZE   = 5200
    buf = bytearray(MAX_ACTOR_ID * ACTOR_SIZE)

    found = 0
    for n in range(1, MAX_ACTOR_ID):
        p = ASSETS / f"ACTOR{n}"
        if not p.exists():
            continue
        data = p.read_bytes()
        if len(data) != ACTOR_SIZE:
            print(f"  WARNING: ACTOR{n}: unexpected size {len(data)}, skipping")
            continue
        buf[n * ACTOR_SIZE : (n + 1) * ACTOR_SIZE] = data
        found += 1

    dst = RES / 'actor_rom_data.bin'
    dst.write_bytes(buf)
    print(f"  {dst}  ({len(buf):,} bytes, {found} actors packed)")

    print("\nAll binary assets packed. resources.res already includes BINARY entries.")

if __name__ == '__main__':
    main()
