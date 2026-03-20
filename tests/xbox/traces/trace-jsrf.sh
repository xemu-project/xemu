#!/usr/bin/env bash
# tests/xbox/traces/trace-jsrf.sh
#
# Per-title trace capture script for Jet Set Radio Future.
#
# Usage:
#   XEMU_ISO=/path/to/jsrf.iso \
#   XEMU_BIOS=/path/to/bios.bin \
#   XEMU_MCPX=/path/to/mcpx.bin \
#   bash tests/xbox/traces/trace-jsrf.sh
#
# JSRF-specific focus:
#   - Frame-rate stability (target 60 fps; spikes indicate timing regression)
#   - Cel-shading combiner pass (GPU warnings point to NV2A stencil/combiner bugs)
#   - TCG interrupt latency (causes hitching on stage transitions)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=trace-common.sh
source "$SCRIPT_DIR/trace-common.sh"

TITLE_ID="jsrf"
CAPTURE_SEC="${CAPTURE_SEC:-120}"
BOOT_TIMEOUT=45

echo "[jsrf] Capturing trace for '$TITLE_ID'"

start_xemu "$TITLE_ID" \
    -device usb-xbox-gamepad

if wait_for_boot "$BOOT_TIMEOUT"; then
    echo "[jsrf] Observing for ${CAPTURE_SEC}s …"
    sleep "$CAPTURE_SEC"
fi

collect_metrics "$TITLE_ID"

# JSRF-specific: flag if FPS average is below 55 (hard regression indicator)
if [ -n "${XEMU_LOG:-}" ]; then
    fps_avg=$(grep -oE "fps=[0-9]+(\.[0-9]+)?" "$XEMU_LOG" 2>/dev/null \
              | sed 's/fps=//' \
              | awk '{ sum += $1; n++ } END { if (n>0) printf "%.2f", sum/n }')
    if [ -n "$fps_avg" ] && awk "BEGIN { exit !($fps_avg < 55.0) }"; then
        echo "[jsrf] WARN: FPS avg $fps_avg < 55.0 — possible timing regression" >&2
    fi
fi

stop_xemu
