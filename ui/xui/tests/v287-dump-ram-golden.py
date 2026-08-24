#!/usr/bin/env python3
# v2.87 current regression ownership.
"""Source-level invariants for v1.69 split RAM dump modes."""
from __future__ import annotations

import argparse
import pathlib
import sys
from v287_source_test_utils import read_memory_tools_implementation


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise AssertionError(f"missing {label}: {needle}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    args = parser.parse_args()

    root = pathlib.Path(args.root).resolve()
    dbg = root / "ui/xui/debug-tools"
    memory = read_memory_tools_implementation(dbg)
    header = (dbg / "memory-tools.hh").read_text(encoding="utf-8")

    require(header, "void DumpPhysicalRam();", "physical dump entry point")
    require(header, "void DumpMappedVirtualRam();", "mapped virtual dump entry point")
    require(header, "void DumpRam(bool dump_physical, bool dump_virtual);",
            "shared dump implementation")

    require(memory, "DumpRam(true, false);", "physical-only routing")
    require(memory, "DumpRam(false, true);", "virtual-only routing")
    require(memory, "DumpRam(true, true);", "combined routing")
    require(memory, "if (dump_virtual && !ScanMappedVirtualRam",
            "virtual scan only when requested")
    require(memory, "if (dump_physical) {\n        gchar *physical_c",
            "physical file only when requested")
    require(memory, 'ImGui::MenuItem("DUMP PHYSICAL")',
            "context physical action")
    require(memory, 'ImGui::MenuItem("DUMP MAPPED VIRTUAL RAM")',
            "context virtual action")
    require(memory, 'ImGui::MenuItem("DUMP PHYSICAL + DUMP MAPPED VIRTUAL RAM")',
            "context combined action")
    require(memory, 'ImGui::Button("DUMP PHYSICAL"', "tab physical button")
    require(memory, 'ImGui::Button("DUMP MAPPED VIRTUAL RAM"',
            "tab virtual button")
    require(memory, 'ImGui::Button("DUMP PHYSICAL + DUMP MAPPED VIRTUAL RAM"',
            "tab combined button")
    require(memory, 'stem + "-PHYSICAL-00000000.bin"',
            "physical filename compatibility")
    require(memory, 'stem + "-VIRTUAL-MAP.txt"',
            "virtual map filename compatibility")
    require(memory, '"%s-VIRTUAL-%08llX-%08llX.bin"',
            "virtual region filename compatibility")

    print("PASS: v1.69 split RAM dump invariants")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
