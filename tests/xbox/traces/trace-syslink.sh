#!/usr/bin/env bash
# tests/xbox/traces/trace-syslink.sh
#
# Per-title trace capture script for System Link multiplayer titles:
#   MechAssault and Crimson Skies: High Road to Revenge.
#
# Usage (two-instance setup on the same host):
#
#   Instance A:
#     XEMU_ISO=/path/to/mechassault.iso \
#     XEMU_BIOS=/path/to/bios.bin \
#     XEMU_MCPX=/path/to/mcpx.bin \
#     INSTANCE=A PEER_IP=10.0.0.2 \
#     bash tests/xbox/traces/trace-syslink.sh --mechassault
#
#   Instance B:
#     XEMU_ISO=/path/to/mechassault.iso \
#     XEMU_BIOS=/path/to/bios.bin \
#     XEMU_MCPX=/path/to/mcpx.bin \
#     INSTANCE=B PEER_IP=10.0.0.1 \
#     bash tests/xbox/traces/trace-syslink.sh --mechassault
#
# Flags:
#   --mechassault   Trace MechAssault (default)
#   --crimson-skies Trace Crimson Skies
#
# Captures:
#   - nvnet broadcast / unicast packet counts (via xemu debug log)
#   - Session discovery timing (first "session found" log entry)
#   - Standard FPS / shader / underrun / GPU-warning metrics

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=trace-common.sh
source "$SCRIPT_DIR/trace-common.sh"

TITLE_ID="mechassault"
CAPTURE_SEC="${CAPTURE_SEC:-120}"
BOOT_TIMEOUT=45
INSTANCE="${INSTANCE:-A}"

for arg in "$@"; do
    case "$arg" in
        --mechassault)   TITLE_ID="mechassault" ;;
        --crimson-skies) TITLE_ID="crimson-skies"; BOOT_TIMEOUT=60 ;;
    esac
done

echo "[syslink] Capturing trace for '$TITLE_ID' (instance $INSTANCE)"

# Pass net backend args if a peer IP is configured
NET_ARGS=()
if [ -n "${PEER_IP:-}" ]; then
    NET_ARGS+=(-netdev "socket,id=net0,connect=${PEER_IP}:9301"
               -device "usb-net,netdev=net0")
    echo "[syslink] Peer IP: $PEER_IP"
else
    echo "[syslink] WARN: PEER_IP not set — System Link peer discovery will not be tested" >&2
fi

start_xemu "$TITLE_ID" \
    -device usb-xbox-gamepad \
    "${NET_ARGS[@]}"

if wait_for_boot "$BOOT_TIMEOUT"; then
    echo "[syslink] Observing for ${CAPTURE_SEC}s …"
    sleep "$CAPTURE_SEC"
fi

collect_metrics "$TITLE_ID"

# System-link specific: report broadcast packet counts from nvnet log lines
if [ -n "${XEMU_LOG:-}" ]; then
    bcast=$(grep -cE "nvnet:.*broadcast" "$XEMU_LOG" 2>/dev/null || echo 0)
    session=$(grep -cE "(session found|peer discovered)" "$XEMU_LOG" 2>/dev/null || echo 0)
    echo ""
    echo "=== System Link metrics ==="
    echo "  Broadcast packets logged: $bcast"
    echo "  Session discoveries:      $session"
    echo "==========================="
fi

stop_xemu
