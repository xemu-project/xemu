#!/bin/bash
set -ex
# uv tool install fonttools
FONT_INPUT=font_awesome_6_1_1_solid.otf
FONT_OUTPUT=font_awesome_6_1_1_solid.min.otf
GLYPHS=$(grep -rhoE "ICON_FA_\w+" ../ui/xui | sort | uniq | sed -E 's/^ICON_FA_//; s/_/-/g; s/.*/\L&/' | paste -sd ",")
pyftsubset $FONT_INPUT --output-file=$FONT_OUTPUT --glyphs=$GLYPHS
