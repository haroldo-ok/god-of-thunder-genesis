# God of Thunder - Sega Genesis Port
# Makefile for SGDK 1.70
#
# Prerequisites:
#   export GDK=/opt/sgdk          (or wherever you installed SGDK 1.70)
#
# Build:
#   make              → produces out/rom.bin
#   make clean        → remove build artefacts
#
# Before first build, pack the raw DOS assets:
#   python3 tools/pack_assets.py
#
# Directory layout:
#   src/      C sources
#   inc/      generated headers  (actor_ids.h, actor_data.h, palette_gen.h)
#   res/      graphics + binary data + resources.res descriptor
#   tools/    asset pipeline scripts

GDK ?= /opt/sgdk

include $(GDK)/makefile.gen
