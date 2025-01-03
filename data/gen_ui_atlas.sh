#!/bin/bash
set -e

# Rasterize svg objects
SVGS="ports.svg duke.svg xmu.svg"
PNGS=""
for image in $SVGS; do
	for obj in $(inkscape --query-all $image | grep -oP "obj_\w+"); do
		outfile=${obj}.png
		inkscape --export-id-only --export-id=$obj --export-filename=$outfile $image
		PNGS="$PNGS $outfile"
	done
done

# Build texture atlas
# pip install https://github.com/mborgerson/textureatlas/archive/refs/heads/master.zip
python -m textureatlas -o ui_objs.png -m ui_objs.json -mf=json $PNGS

# Build accessory structs
python <<EOF >ui_objs.h
import json
with open("ui_objs.json", "r", encoding="utf-8") as file:
	atlas = json.load(file)
	print("const struct { int x, y, w, h; } ui_objs[] = {")
	names = []
	for name, frames in atlas.items():
		for x, y, w, h in frames:
			print("  {%4d, %4d, %4d, %4d}, // %s" % (x, y, w, h, name))
		names.append(name)
	print("};")
	print("enum ui_objs_idx {")
	for idx, name in enumerate(names):
		print(f"    ui_{name}_idx = {idx},")
	print("};")
EOF
