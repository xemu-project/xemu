#!/usr/bin/env python3
"""
OpenMidway Crash Signature Clustering
======================================
Groups xemu crash logs by signature to surface the most common failure
modes across title runs.

A **crash signature** is the tuple of the first ``DEPTH`` frames in the
stack trace that belong to OpenMidway source files (i.e., not libc / libpthread
frames).  Two crashes share a signature if their trimmed traces are identical.

Usage
-----
::

    # Cluster all *.log files under tests/xbox/compat/results/
    python crash_cluster.py

    # Cluster a specific directory
    python crash_cluster.py --log-dir /path/to/logs

    # Run built-in self-test (no real logs required — safe in CI)
    python crash_cluster.py --self-test

    # Emit JSON instead of a human-readable report
    python crash_cluster.py --json

Output columns (human-readable report)
---------------------------------------
  COUNT  How many crashes share this signature
  SIG    Abbreviated signature (first frame only for display)
  TITLES Distinct titles that produced this crash
  FILES  Source-file paths in the signature frames
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import defaultdict
from pathlib import Path
from typing import Optional

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

_HERE = Path(__file__).parent.resolve()
_DEFAULT_LOG_DIR = _HERE.parent / "compat" / "results"

# Maximum stack depth used to form the signature
_SIG_DEPTH = 5

# Patterns for lines we want to include in signatures.  These match frames
# that come from the OpenMidway / QEMU source tree and skip system libraries.
_KEEP_FRAME_RE = re.compile(
    r"""
    (?:
        hw/xbox |
        hw/xbox/nv2a |
        hw/xbox/mcpx |
        target/ |
        accel/tcg |
        net/ |
        ui/xui
    )
    """,
    re.VERBOSE,
)

# Lines that look like a stack frame (from addr2line / QEMU crash output)
# Example: "  #3 0x00007f1234abcdef in pgraph_method hw/xbox/nv2a/pgraph/pgraph.c:4512"
_FRAME_RE = re.compile(
    r"^\s*#\d+\s+0x[0-9a-fA-F]+\s+in\s+(\S+)\s+(\S+):(\d+)"
)

# Simpler frame format without address: "#3 pgraph_method (pgraph.c:4512)"
_FRAME_SIMPLE_RE = re.compile(
    r"^\s*#\d+\s+(\w+)\s+\(([^:)]+):(\d+)\)"
)

# Line that introduces a new crash block (xemu typically prints "Aborted" or
# a signal description)
_CRASH_START_RE = re.compile(
    r"^(?:Aborted|Segmentation fault|Bus error|Illegal instruction"
    r"|qemu: fatal:|ERROR|ASSERT FAILED)",
    re.IGNORECASE,
)

# Lines that carry the title ID in harness result filenames
# e.g. "halo-ce-2025-01-01T12-00-00.json"
_TITLE_FROM_FILENAME_RE = re.compile(r"^([a-z][a-z0-9-]+?)-\d{4}-\d{2}-\d{2}")


# ---------------------------------------------------------------------------
# Data structures
# ---------------------------------------------------------------------------


class Frame:
    __slots__ = ("function", "source_file", "line")

    def __init__(self, function: str, source_file: str, line: str) -> None:
        self.function = function
        self.source_file = source_file
        self.line = line

    def __repr__(self) -> str:
        return f"{self.function} ({self.source_file}:{self.line})"

    def is_project_frame(self) -> bool:
        return bool(_KEEP_FRAME_RE.search(self.source_file))


class CrashRecord:
    def __init__(self, title_id: str, log_path: Path) -> None:
        self.title_id = title_id
        self.log_path = log_path
        self.frames: list[Frame] = []
        self.raw_lines: list[str] = []

    @property
    def signature(self) -> tuple[str, ...]:
        """Stable, depth-limited signature of OpenMidway frames."""
        project_frames = [f for f in self.frames if f.is_project_frame()]
        top = project_frames[:_SIG_DEPTH]
        return tuple(f"{f.function}@{f.source_file}" for f in top)


# ---------------------------------------------------------------------------
# Log parsing
# ---------------------------------------------------------------------------


def _title_from_path(path: Path) -> str:
    m = _TITLE_FROM_FILENAME_RE.match(path.stem)
    return m.group(1) if m else path.stem


def parse_crash_log(path: Path) -> list[CrashRecord]:
    """
    Parse *path* for one or more crash records.

    Supports plain text log files (xemu stderr) and JSON result files
    produced by the compatibility harness (which embed errors as strings).
    """
    records: list[CrashRecord] = []
    title_id = _title_from_path(path)

    if path.suffix == ".json":
        return _parse_json_result(path, title_id)

    try:
        text = path.read_text(errors="replace")
    except OSError:
        return records

    current: Optional[CrashRecord] = None

    for raw_line in text.splitlines():
        line = raw_line.rstrip()

        # Detect start of a new crash block
        if _CRASH_START_RE.match(line):
            current = CrashRecord(title_id=title_id, log_path=path)
            records.append(current)
            current.raw_lines.append(line)
            continue

        if current is None:
            continue

        current.raw_lines.append(line)

        # Try to parse a stack frame
        for pattern in (_FRAME_RE, _FRAME_SIMPLE_RE):
            m = pattern.match(line)
            if m:
                current.frames.append(
                    Frame(
                        function=m.group(1),
                        source_file=m.group(2),
                        line=m.group(3),
                    )
                )
                break

    return records


def _parse_json_result(path: Path, title_id: str) -> list[CrashRecord]:
    """Extract crash hints from a harness JSON result file."""
    records: list[CrashRecord] = []
    try:
        data = json.loads(path.read_text())
    except (OSError, json.JSONDecodeError):
        return records

    errors = data.get("errors", [])
    for err in errors:
        rec = CrashRecord(title_id=title_id, log_path=path)
        rec.raw_lines = [err]
        # Best-effort: try to parse any stack frame lines embedded in the error
        for line in err.splitlines():
            for pattern in (_FRAME_RE, _FRAME_SIMPLE_RE):
                m = pattern.match(line)
                if m:
                    rec.frames.append(
                        Frame(
                            function=m.group(1),
                            source_file=m.group(2),
                            line=m.group(3),
                        )
                    )
                    break
        records.append(rec)

    return records


# ---------------------------------------------------------------------------
# Clustering
# ---------------------------------------------------------------------------


def cluster(records: list[CrashRecord]) -> dict[tuple, list[CrashRecord]]:
    """Group crash records by signature."""
    groups: dict[tuple, list[CrashRecord]] = defaultdict(list)
    for rec in records:
        groups[rec.signature].append(rec)
    # Sort by descending count
    return dict(sorted(groups.items(), key=lambda kv: len(kv[1]), reverse=True))


# ---------------------------------------------------------------------------
# Reporting
# ---------------------------------------------------------------------------


def report_text(groups: dict[tuple, list[CrashRecord]]) -> str:
    if not groups:
        return "No crash records found.\n"

    lines = [
        f"{'COUNT':>5}  {'SIGNATURE (first frame)':50}  TITLES",
        "-" * 80,
    ]
    for sig, recs in groups.items():
        titles = sorted({r.title_id for r in recs})
        first_frame = sig[0] if sig else "<no openmidway frame>"
        # Trim long signatures for display
        if len(first_frame) > 50:
            first_frame = first_frame[:47] + "…"
        lines.append(
            f"{len(recs):>5}  {first_frame:50}  {', '.join(titles)}"
        )
        if len(sig) > 1:
            for frame_str in sig[1:]:
                lines.append(f"{'':>5}  {frame_str}")
        lines.append("")

    return "\n".join(lines)


def report_json(groups: dict[tuple, list[CrashRecord]]) -> str:
    out = []
    for sig, recs in groups.items():
        out.append(
            {
                "count": len(recs),
                "signature": list(sig),
                "titles": sorted({r.title_id for r in recs}),
                "log_paths": [str(r.log_path) for r in recs],
            }
        )
    return json.dumps(out, indent=2)


# ---------------------------------------------------------------------------
# Self-test
# ---------------------------------------------------------------------------

_SELF_TEST_LOG = """\
qemu: fatal: unhandled exception in pgraph
  #0 0x00007f0000000001 in error_report util/error.c:100
  #1 0x00007f0000000002 in pgraph_method_unhandled hw/xbox/nv2a/pgraph/pgraph.c:4512
  #2 0x00007f0000000003 in pgraph_method hw/xbox/nv2a/pgraph/pgraph.c:4600
  #3 0x00007f0000000004 in nv2a_pfifo_run_pusher hw/xbox/nv2a/pfifo.c:887
Aborted
  #0 0x00007f0000000010 in error_report util/error.c:100
  #1 0x00007f0000000011 in dsp_run hw/xbox/mcpx/apu/dsp/dsp.c:200
  #2 0x00007f0000000012 in mcpx_apu_vp_process hw/xbox/mcpx/apu/vp/vp.c:500
"""


def self_test() -> bool:
    """Return True if all assertions pass."""
    import tempfile

    # Write synthetic log to a real temporary file so we avoid
    # relying on Path private internals.
    with tempfile.NamedTemporaryFile(
        mode="w",
        suffix=".log",
        prefix="halo-ce-2025-01-01T120000-",
        delete=False,
    ) as tmp:
        tmp.write(_SELF_TEST_LOG)
        tmp_path = Path(tmp.name)

    # Rename so the stem matches the expected title-id pattern
    dest = tmp_path.parent / "halo-ce-2025-01-01T120000.log"
    tmp_path.rename(dest)
    tmp_path = dest

    try:
        records = parse_crash_log(tmp_path)
    finally:
        tmp_path.unlink(missing_ok=True)

    assert len(records) == 2, f"Expected 2 records, got {len(records)}"

    # First record: pgraph crash — should have openmidway frames
    rec0 = records[0]
    assert rec0.title_id == "halo-ce", f"Bad title_id: {rec0.title_id}"
    assert len(rec0.signature) >= 1, "First record should have openmidway frames"
    assert "pgraph_method_unhandled" in rec0.signature[0]

    # Second record: DSP crash
    rec1 = records[1]
    assert len(rec1.signature) >= 1, "Second record should have openmidway frames"

    groups = cluster(records)
    assert len(groups) == 2, f"Expected 2 clusters, got {len(groups)}"

    txt = report_text(groups)
    assert "pgraph_method_unhandled" in txt

    print("Self-test PASSED — 2 crash records parsed, 2 clusters formed.")
    return True


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        description="Cluster xemu crash logs by stack-trace signature."
    )
    p.add_argument(
        "--log-dir",
        metavar="DIR",
        default=str(_DEFAULT_LOG_DIR),
        help=f"Directory to scan for crash logs (default: {_DEFAULT_LOG_DIR})",
    )
    p.add_argument(
        "--json",
        action="store_true",
        help="Emit JSON output instead of a human-readable report",
    )
    p.add_argument(
        "--self-test",
        action="store_true",
        help="Run built-in self-test and exit (safe in CI)",
    )
    return p


def main(argv: Optional[list[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    if args.self_test:
        ok = self_test()
        return 0 if ok else 1

    log_dir = Path(args.log_dir)
    if not log_dir.exists():
        print(f"Log directory not found: {log_dir}", file=sys.stderr)
        print("Use --log-dir to specify a directory, or --self-test to run tests.")
        return 1

    records: list[CrashRecord] = []
    for path in sorted(log_dir.glob("**/*")):
        if path.suffix in (".log", ".txt", ".json") and path.is_file():
            records.extend(parse_crash_log(path))

    if not records:
        print(f"No crash records found in {log_dir}")
        return 0

    groups = cluster(records)

    if args.json:
        print(report_json(groups))
    else:
        print(report_text(groups))

    return 0


if __name__ == "__main__":
    sys.exit(main())
