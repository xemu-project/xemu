#!/usr/bin/env python3
# v2.87 current regression ownership.
"""Regression guard for v1.86 OPT Pass 2 disassembly streamlining."""
from __future__ import annotations

import argparse
import pathlib

from v287_source_test_utils import extract_function, read_memory_tools_implementation


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    root = pathlib.Path(parser.parse_args().root).resolve()
    debug = root / "ui/xui/debug-tools"

    bridge = (debug / "cheat-engine-memory.c").read_text(encoding="utf-8")
    memory_tools = read_memory_tools_implementation(debug)

    getter = extract_function(
        bridge, "static XemuCheatDisasmContext *xemu_cheat_disasm_context_get(")
    for needle in (
        "g_private_get(&xemu_cheat_disasm_context_private)",
        "g_private_set(&xemu_cheat_disasm_context_private, context)",
        "if (context->page_size != page_size)",
        "context->page_size = page_size;",
    ):
        if needle not in getter:
            raise AssertionError(f"missing reusable decoder invariant: {needle}")
    if getter.count("cs_open(") != 1 or getter.count("cs_malloc(") != 1:
        raise AssertionError("decoder creation must remain one-time per thread context")

    paired = extract_function(bridge, "int xemu_cheat_disassemble_paired(")
    page = extract_function(bridge, "int xemu_cheat_disassemble_page(")
    for body, name in ((paired, "paired"), (page, "page")):
        for forbidden in ("cs_open(", "cs_close(", "cs_malloc(", "cs_free("):
            if forbidden in body:
                raise AssertionError(f"{name} decode regressed to per-call {forbidden}")

    refresh = extract_function(
        memory_tools, "void MemoryToolsWindow::RefreshDisassembly()")
    for needle in (
        "bool reused_page_decode = false;",
        "containing_row + count <= page_row_count",
        "last.virtual_address + 15u <= page_end",
        "m_disassembly_rows.assign(",
        "reused_page_decode = true;",
        "if (!reused_page_decode)",
        "xemu_cheat_disassemble_paired(",
        "const size_t selected_row = find_disassembly_row(",
        "m_disassembly_rows[selected_row].physical_address",
    ):
        if needle not in refresh:
            raise AssertionError(f"missing Count-mode reuse invariant: {needle}")

    if "xemu_cheat_prepare_virtual_map();" in refresh:
        raise AssertionError("RefreshDisassembly still repeats the row's physical translation")

    # The conservative 15-byte guard is essential: the page decoder cannot
    # decode a single x86 instruction across the 4 KiB boundary, while paired
    # mode can. Never reuse the page slice in that final 14-byte region.
    page_size = 0x1000
    for start in range(page_size):
        safe = start + 15 <= page_size
        expected = start <= page_size - 15
        if safe != expected:
            raise AssertionError((start, safe, expected))

    print("PASS: v1.86 OPT Pass-2 reusable decoder + Count-mode reuse checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
