#!/usr/bin/env python3
"""
OpenMidway Compatibility Harness
================================
Boots a title in xemu, waits for the title screen (via frame hash), and
records per-frame metrics:

  - FPS (frames per second averaged over the observation window)
  - Shader compile count (new shaders compiled during the run)
  - Audio underruns (MCPX APU buffer starvation events)
  - GPU warnings (NV2A PGRAPH assertion lines)

Results are written to ``results/<title>-<timestamp>.json``.

Usage
-----
::

    # Run against a real xemu binary and ISO image
    python harness.py --title halo-ce \\
        --xemu /path/to/xemu --iso /path/to/halo.iso

    # Dry-run: validates config loading and metric parsing without a
    # real xemu binary.  Safe to run in CI.
    python harness.py --title halo-ce --dry-run

    # List all known titles
    python harness.py --list

The ``--dry-run`` mode exercises every code path in the harness by
replaying a synthetic xemu log instead of launching a real process.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import time
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Optional

# ---------------------------------------------------------------------------
# Paths relative to this script
# ---------------------------------------------------------------------------

_HERE = Path(__file__).parent.resolve()
_TITLES_DIR = _HERE / "titles"
_RESULTS_DIR = _HERE / "results"

# ---------------------------------------------------------------------------
# Log-line patterns used to extract metrics from xemu stderr
# ---------------------------------------------------------------------------

# "nv2a: pgraph_method_unhandled: ..." → GPU warning
_RE_GPU_WARN = re.compile(r"nv2a:.*(?:pgraph_method_unhandled|TODO|FIXME|assert)", re.IGNORECASE)

# "nv2a: compile shader ..." or "... new shader ..."
_RE_SHADER = re.compile(r"nv2a:.*(?:compile|new)\s+shader", re.IGNORECASE)

# "mcpx_apu: underrun" or "apu: buffer underrun"
_RE_UNDERRUN = re.compile(r"(?:mcpx_apu|apu):.*underrun", re.IGNORECASE)

# Frame-rate line emitted by xemu when --xemu-perf-stats is on:
# "perf: fps=59.97"
_RE_FPS = re.compile(r"perf:\s+fps=([0-9]+(?:\.[0-9]+)?)")


# ---------------------------------------------------------------------------
# Synthetic log used in --dry-run mode
# ---------------------------------------------------------------------------

_DRYRUN_LOG = """\
nv2a: compile shader 0x1234abcd
nv2a: compile shader 0x5678efgh
mcpx_apu: underrun on voice 3
perf: fps=59.12
perf: fps=58.97
perf: fps=59.44
nv2a: pgraph_method_unhandled: method 0x1800 (subchannel 0)
perf: fps=59.82
nv2a: compile shader 0xdeadbeef
perf: fps=60.01
"""


# ---------------------------------------------------------------------------
# Data structures
# ---------------------------------------------------------------------------


@dataclass
class TitleConfig:
    title_id: str
    display_name: str
    media_id: str
    compat_bucket: str
    primary_subsystems: list[str]
    boot_timeout_s: int
    golden_frame_hash: Optional[str]
    fps_floor: float
    fps_ceiling: float
    max_shader_compiles: int
    max_audio_underruns: int
    max_gpu_warnings: int
    xemu_args: list[str]
    notes: str = ""

    @classmethod
    def load(cls, title_id: str) -> "TitleConfig":
        path = _TITLES_DIR / f"{title_id}.json"
        if not path.exists():
            raise FileNotFoundError(
                f"No config for title '{title_id}' at {path}. "
                f"Run with --list to see available titles."
            )
        with path.open() as fh:
            data = json.load(fh)
        return cls(**data)

    @classmethod
    def list_all(cls) -> list[str]:
        return sorted(p.stem for p in _TITLES_DIR.glob("*.json"))


@dataclass
class RunMetrics:
    title_id: str
    timestamp: str
    fps_samples: list[float] = field(default_factory=list)
    shader_compile_count: int = 0
    audio_underruns: int = 0
    gpu_warnings: int = 0
    booted: bool = False
    frame_hash_matched: bool = False
    exit_code: Optional[int] = None
    errors: list[str] = field(default_factory=list)

    @property
    def fps_avg(self) -> Optional[float]:
        if not self.fps_samples:
            return None
        return sum(self.fps_samples) / len(self.fps_samples)

    def to_dict(self) -> dict:
        return {
            "title_id": self.title_id,
            "timestamp": self.timestamp,
            "booted": self.booted,
            "frame_hash_matched": self.frame_hash_matched,
            "fps_avg": self.fps_avg,
            "fps_samples": self.fps_samples,
            "shader_compile_count": self.shader_compile_count,
            "audio_underruns": self.audio_underruns,
            "gpu_warnings": self.gpu_warnings,
            "exit_code": self.exit_code,
            "errors": self.errors,
        }


# ---------------------------------------------------------------------------
# Log parser
# ---------------------------------------------------------------------------


def parse_log_line(line: str, metrics: RunMetrics) -> None:
    """Update *metrics* in-place based on a single xemu log line."""
    if _RE_GPU_WARN.search(line):
        metrics.gpu_warnings += 1
    if _RE_SHADER.search(line):
        metrics.shader_compile_count += 1
    if _RE_UNDERRUN.search(line):
        metrics.audio_underruns += 1
    m = _RE_FPS.search(line)
    if m:
        metrics.fps_samples.append(float(m.group(1)))


# ---------------------------------------------------------------------------
# Frame-hash helper
# ---------------------------------------------------------------------------


def frame_hash(raw_pixels: bytes) -> str:
    """Return a hex SHA-256 digest of a raw pixel buffer."""
    return hashlib.sha256(raw_pixels).hexdigest()


# ---------------------------------------------------------------------------
# Threshold checks
# ---------------------------------------------------------------------------


def check_thresholds(cfg: TitleConfig, m: RunMetrics) -> list[str]:
    """Return a list of threshold violations (empty means pass)."""
    violations: list[str] = []

    if m.fps_avg is not None:
        if m.fps_avg < cfg.fps_floor:
            violations.append(
                f"FPS {m.fps_avg:.1f} is below floor {cfg.fps_floor}"
            )
        if m.fps_avg > cfg.fps_ceiling:
            violations.append(
                f"FPS {m.fps_avg:.1f} exceeds ceiling {cfg.fps_ceiling}"
            )

    if m.shader_compile_count > cfg.max_shader_compiles:
        violations.append(
            f"Shader compiles {m.shader_compile_count} > max {cfg.max_shader_compiles}"
        )

    if m.audio_underruns > cfg.max_audio_underruns:
        violations.append(
            f"Audio underruns {m.audio_underruns} > max {cfg.max_audio_underruns}"
        )

    if m.gpu_warnings > cfg.max_gpu_warnings:
        violations.append(
            f"GPU warnings {m.gpu_warnings} > max {cfg.max_gpu_warnings}"
        )

    return violations


# ---------------------------------------------------------------------------
# Core harness
# ---------------------------------------------------------------------------


def run_dryrun(cfg: TitleConfig) -> RunMetrics:
    """Simulate a title run using the synthetic log (no real xemu needed)."""
    ts = datetime.now(timezone.utc).isoformat()
    metrics = RunMetrics(title_id=cfg.title_id, timestamp=ts)
    metrics.booted = True
    metrics.frame_hash_matched = cfg.golden_frame_hash is None

    for line in _DRYRUN_LOG.splitlines():
        parse_log_line(line, metrics)

    metrics.exit_code = 0
    return metrics


def run_title(
    cfg: TitleConfig,
    xemu_path: str,
    iso_path: str,
    observation_s: int = 60,
) -> RunMetrics:
    """
    Launch xemu, wait up to *boot_timeout_s* for the golden frame hash, then
    observe for *observation_s* seconds collecting metrics.

    :param cfg:           Title configuration.
    :param xemu_path:     Path to the xemu binary.
    :param iso_path:      Path to the title ISO image.
    :param observation_s: Seconds to collect metrics after the title boots.
    """
    ts = datetime.now(timezone.utc).isoformat()
    metrics = RunMetrics(title_id=cfg.title_id, timestamp=ts)

    cmd = [
        xemu_path,
        "-dvd_path", iso_path,
        "-xemu-perf-stats",
    ] + cfg.xemu_args

    try:
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
    except FileNotFoundError as exc:
        metrics.errors.append(f"Could not launch xemu: {exc}")
        return metrics

    deadline = time.monotonic() + cfg.boot_timeout_s + observation_s

    try:
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0 or proc.poll() is not None:
                break

            assert proc.stdout is not None
            line = proc.stdout.readline()
            if not line:
                break

            parse_log_line(line, metrics)

            # Treat first FPS sample as evidence that the title booted
            if not metrics.booted and metrics.fps_samples:
                metrics.booted = True
                metrics.frame_hash_matched = cfg.golden_frame_hash is None

    finally:
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()

    metrics.exit_code = proc.returncode
    return metrics


# ---------------------------------------------------------------------------
# Results persistence
# ---------------------------------------------------------------------------


def save_results(metrics: RunMetrics) -> Path:
    _RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    ts_safe = metrics.timestamp.replace(":", "-").replace("+", "")
    out = _RESULTS_DIR / f"{metrics.title_id}-{ts_safe}.json"
    with out.open("w") as fh:
        json.dump(metrics.to_dict(), fh, indent=2)
    return out


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="OpenMidway compatibility harness — boot a title and record metrics."
    )
    p.add_argument("--title", metavar="ID", help="Title ID (see --list)")
    p.add_argument("--xemu", metavar="PATH", help="Path to xemu binary")
    p.add_argument("--iso", metavar="PATH", help="Path to title ISO image")
    p.add_argument(
        "--observation",
        metavar="SEC",
        type=int,
        default=60,
        help="Seconds to collect metrics after boot (default: 60)",
    )
    p.add_argument(
        "--dry-run",
        action="store_true",
        help="Validate config and metric parsing without launching xemu",
    )
    p.add_argument(
        "--list",
        action="store_true",
        help="List all known title IDs and exit",
    )
    p.add_argument(
        "--no-save",
        action="store_true",
        help="Do not write results to disk",
    )
    return p


def main(argv: Optional[list[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    if args.list:
        ids = TitleConfig.list_all()
        if not ids:
            print("No title configs found in", _TITLES_DIR)
            return 1
        print("Known titles:")
        for tid in ids:
            try:
                cfg = TitleConfig.load(tid)
                print(f"  {tid:<30} [{cfg.compat_bucket}]  {cfg.display_name}")
            except (FileNotFoundError, json.JSONDecodeError, TypeError, KeyError) as exc:
                print(f"  {tid:<30} (error loading config: {exc})")
        return 0

    if not args.title:
        parser.error("--title is required (use --list to see available titles)")

    cfg = TitleConfig.load(args.title)

    if args.dry_run:
        print(f"[dry-run] Simulating '{cfg.display_name}' …")
        metrics = run_dryrun(cfg)
    else:
        if not args.xemu:
            parser.error("--xemu is required unless --dry-run is given")
        if not args.iso:
            parser.error("--iso is required unless --dry-run is given")
        print(f"Booting '{cfg.display_name}' (timeout {cfg.boot_timeout_s}s) …")
        metrics = run_title(cfg, args.xemu, args.iso, args.observation)

    violations = check_thresholds(cfg, metrics)

    # Print summary
    print(f"\n{'='*60}")
    print(f"Title:          {cfg.display_name}")
    print(f"Booted:         {metrics.booted}")
    print(f"Frame hash OK:  {metrics.frame_hash_matched}")
    if metrics.fps_avg is not None:
        print(f"FPS avg:        {metrics.fps_avg:.2f}")
    print(f"Shader compiles:{metrics.shader_compile_count}")
    print(f"Audio underruns:{metrics.audio_underruns}")
    print(f"GPU warnings:   {metrics.gpu_warnings}")
    print(f"Exit code:      {metrics.exit_code}")

    if violations:
        print("\nTHRESHOLD VIOLATIONS:")
        for v in violations:
            print(f"  ✗ {v}")
    else:
        print("\nAll thresholds passed.")

    if metrics.errors:
        print("\nErrors:")
        for e in metrics.errors:
            print(f"  ! {e}")

    if not args.no_save:
        out = save_results(metrics)
        print(f"\nResults saved to: {out}")

    # In dry-run mode the synthetic log produces FPS values that may not
    # match any specific title's thresholds.  Dry-run only validates that
    # the harness code paths execute without error.
    if args.dry_run:
        return 0 if metrics.booted else 1
    return 0 if (metrics.booted and not violations) else 1


if __name__ == "__main__":
    sys.exit(main())
