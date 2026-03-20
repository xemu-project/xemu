#!/usr/bin/env bash
# tests/xbox/traces/trace-halo.sh
#
# Per-title trace capture script for Halo: Combat Evolved and Halo 2.
#
# Usage:
#   XEMU_ISO=/path/to/halo.iso \
#   XEMU_BIOS=/path/to/bios.bin \
#   XEMU_MCPX=/path/to/mcpx.bin \
#   bash tests/xbox/traces/trace-halo.sh [--halo2]
#
# Captures:
#   - xemu debug log (GPU, audio, CPU events)
#   - Per-frame FPS samples
#   - Shader compile events
#   - APU DSP audio underruns
#   - NV2A GPU warnings
#
# The --halo2 flag selects the Halo 2 title config; default is Halo CE.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=trace-common.sh
source "$SCRIPT_DIR/trace-common.sh"

TITLE_ID="halo-ce"
CAPTURE_SEC="${CAPTURE_SEC:-120}"
BOOT_TIMEOUT=60

for arg in "$@"; do
    case "$arg" in
        --halo2) TITLE_ID="halo-2"; BOOT_TIMEOUT=90 ;;
    esac
done

echo "[halo] Capturing trace for '$TITLE_ID'"

start_xemu "$TITLE_ID" \
    -device usb-xbox-gamepad

if wait_for_boot "$BOOT_TIMEOUT"; then
    echo "[halo] Observing for ${CAPTURE_SEC}s …"
    sleep "$CAPTURE_SEC"
fi

collect_metrics "$TITLE_ID"
stop_xemu
