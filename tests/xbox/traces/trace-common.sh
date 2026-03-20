#!/usr/bin/env bash
# tests/xbox/traces/trace-common.sh
#
# Common helper functions for per-title trace capture scripts.
# Source this file from a title-specific script; do not run directly.
#
# Requirements:
#   - xemu binary on PATH or XEMU_BIN env var
#   - strace (Linux) or ktrace (macOS) for syscall tracing
#   - socat / nc for reading xemu QMP socket output
#
# Environment variables consumed:
#   XEMU_BIN        Path to xemu binary (default: xemu)
#   XEMU_ISO        Path to title ISO image (required)
#   XEMU_BIOS       Path to Xbox BIOS image (required)
#   XEMU_MCPX       Path to MCPX boot ROM (required)
#   TRACE_DIR       Output directory for trace files (default: /tmp/xemu-traces)
#   CAPTURE_SEC     Seconds to capture after boot (default: 120)

set -euo pipefail

XEMU_BIN="${XEMU_BIN:-xemu}"
TRACE_DIR="${TRACE_DIR:-/tmp/xemu-traces}"
CAPTURE_SEC="${CAPTURE_SEC:-120}"

_TS=$(date +%Y%m%dT%H%M%S)

require_var() {
    local name="$1"
    if [ -z "${!name:-}" ]; then
        echo "ERROR: $name is not set. Export it before running this script." >&2
        exit 1
    fi
}

require_file() {
    local path="$1" label="$2"
    if [ ! -f "$path" ]; then
        echo "ERROR: $label not found at '$path'" >&2
        exit 1
    fi
}

# Start xemu with logging enabled and redirect stderr to a log file.
# Usage: start_xemu TITLE_ID [EXTRA_ARGS...]
start_xemu() {
    local title_id="$1"; shift
    local log_file="$TRACE_DIR/${title_id}-${_TS}.log"
    local qmp_sock="$TRACE_DIR/${title_id}-${_TS}.qmp"

    mkdir -p "$TRACE_DIR"

    require_var XEMU_ISO
    require_var XEMU_BIOS
    require_var XEMU_MCPX
    require_file "$XEMU_ISO"  "ISO image"
    require_file "$XEMU_BIOS" "BIOS image"
    require_file "$XEMU_MCPX" "MCPX ROM"

    echo "[trace] Starting xemu for title '$title_id' (log: $log_file)"

    "$XEMU_BIN" \
        -cpu pentium3 -m 64 \
        -bios "$XEMU_BIOS" \
        -mcpx "$XEMU_MCPX" \
        -dvd_path "$XEMU_ISO" \
        -xemu-perf-stats \
        -xemu-log-level debug \
        -qmp "unix:${qmp_sock},server,nowait" \
        "$@" \
        2>&1 | tee "$log_file" &

    XEMU_PID=$!
    XEMU_LOG="$log_file"
    XEMU_QMP="$qmp_sock"

    echo "[trace] xemu PID=$XEMU_PID"
}

# Wait for xemu to produce at least one FPS line (indicates title has booted).
# Usage: wait_for_boot TIMEOUT_SEC
wait_for_boot() {
    local timeout="${1:-60}"
    local deadline=$(( $(date +%s) + timeout ))

    echo "[trace] Waiting up to ${timeout}s for boot …"
    while [ "$(date +%s)" -lt "$deadline" ]; do
        if grep -q "perf: fps=" "$XEMU_LOG" 2>/dev/null; then
            echo "[trace] Title booted (FPS line detected)."
            return 0
        fi
        sleep 2
    done

    echo "[trace] WARN: boot timeout reached — no FPS line seen." >&2
    return 1
}

# Collect metrics from the log file and print a summary.
# Usage: collect_metrics TITLE_ID
collect_metrics() {
    local title_id="$1"
    local log="$XEMU_LOG"

    local fps_lines shader_count underrun_count gpu_warn_count fps_avg

    fps_lines=$(grep -oE "fps=[0-9]+(\.[0-9]+)?" "$log" 2>/dev/null \
                | sed 's/fps=//' || true)
    shader_count=$(grep -cE "nv2a:.*(compile|new)[[:space:]]+shader" "$log" 2>/dev/null || echo 0)
    underrun_count=$(grep -cE "(mcpx_apu|apu):.*underrun" "$log" 2>/dev/null || echo 0)
    gpu_warn_count=$(grep -ciE "nv2a:.*(pgraph_method_unhandled|TODO|FIXME|assert)" "$log" 2>/dev/null || echo 0)

    if [ -n "$fps_lines" ]; then
        fps_avg=$(echo "$fps_lines" | awk '{ sum += $1; n++ } END { if (n>0) printf "%.2f", sum/n; else print "N/A" }')
    else
        fps_avg="N/A"
    fi

    echo ""
    echo "=== Metrics for '$title_id' (${_TS}) ==="
    echo "  FPS avg:        $fps_avg"
    echo "  Shader compiles:$shader_count"
    echo "  Audio underruns:$underrun_count"
    echo "  GPU warnings:   $gpu_warn_count"
    echo "  Log:            $XEMU_LOG"
    echo "========================================"
}

# Stop the running xemu instance.
stop_xemu() {
    if [ -n "${XEMU_PID:-}" ] && kill -0 "$XEMU_PID" 2>/dev/null; then
        echo "[trace] Stopping xemu (PID=$XEMU_PID) …"
        kill "$XEMU_PID"
        wait "$XEMU_PID" 2>/dev/null || true
    fi
}

trap stop_xemu EXIT
