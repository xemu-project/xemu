#!/usr/bin/env python3
# v2.87 current regression ownership.
"""Structural/model guard for v1.84 debugger/Cheat Engine streamlining."""
from __future__ import annotations

import argparse
import bisect
import pathlib
import random
from v287_source_test_utils import extract_function, read_memory_tools_implementation, read_cheat_engine_implementation


def old_find(rows: list[tuple[int, int]], address: int) -> int:
    for i, (start, size) in enumerate(rows):
        if start <= address < start + max(size, 1):
            return i
    return -1


def new_find(rows: list[tuple[int, int]], address: int) -> int:
    starts = [row[0] for row in rows]
    pos = bisect.bisect_left(starts, address)
    if pos < len(rows) and rows[pos][0] == address:
        return pos
    if pos == 0:
        return -1
    pos -= 1
    start, size = rows[pos]
    return pos if address < start + max(size, 1) else -1


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--heavy", action="store_true")
    args = ap.parse_args()
    root = pathlib.Path(args.root).resolve()
    randomized_iterations = 50_000 if args.heavy else 500
    debug = root / "ui/xui/debug-tools"

    memory = read_memory_tools_implementation(debug)
    cheat = read_cheat_engine_implementation(debug)

    # The already-processed RUN_STATE_DEBUG case must return before the
    # backend register read. DrawDebugger's 100 ms live refresh owns updates.
    hit = extract_function(memory, "void MemoryToolsWindow::UpdateBreakpointHitState()")
    paused = hit.find("if (m_was_debug_paused)")
    refresh = hit.find("if (!RefreshRegisters(regs))")
    assert paused >= 0 and refresh >= 0 and paused < refresh

    # Disassembly display strings/labels are rebuilt only when rows, label
    # generation, or the Labels checkbox changes. The hot render loop consumes
    # the cached strings and no longer formats bytes or queries labels per pane.
    rebuild = extract_function(memory, "void MemoryToolsWindow::RebuildDisassemblyRenderCache()")
    pane = extract_function(memory, "bool MemoryToolsWindow::DrawDisassemblyPane(bool physical)")
    draw = extract_function(memory, "void MemoryToolsWindow::DrawDebugger()")
    refresh_disasm = extract_function(memory, "void MemoryToolsWindow::RefreshDisassembly()")
    # v1.92 keeps the same cache ownership but walks the already-sorted label
    # database forward instead of doing one PrimaryLabelAt/lower_bound lookup
    # per decoded row.
    assert "const auto &label_database = current_game_manager.Labels();" in rebuild
    assert "label_it = std::lower_bound(" in rebuild
    assert "label_it->virtual_address == row.virtual_address" in rebuild
    assert "current_game_manager.PrimaryLabelAt" not in rebuild
    assert "m_disassembly_virtual_text" in rebuild
    assert "m_disassembly_physical_text" in rebuild
    assert "m_disassembly_label_generation = current_game_manager.LabelGeneration()" in rebuild
    assert "m_disassembly_cached_labels_enabled = m_labels_enabled" in rebuild
    assert "RebuildDisassemblyRenderCache();" in refresh_disasm
    assert "m_disassembly_label_generation != disassembly_label_generation" in draw
    assert "m_disassembly_cached_labels_enabled != m_labels_enabled" in draw
    assert "LabelGeneration()" not in pane
    assert "RebuildDisassemblyRenderCache();" not in pane
    assert "m_disassembly_physical_text[i].c_str()" in pane
    assert "m_disassembly_virtual_text[i].c_str()" in pane
    assert "format_disassembly_bytes(" not in pane
    assert "PrimaryLabelAt(" not in pane

    # The shared binary lookup replaces both former linear containing-row scans.
    assert memory.count("find_disassembly_row(") >= 3
    rng = random.Random(0x184D15A5)
    for _ in range(randomized_iterations):
        address = rng.randrange(0, 0x10000)
        rows: list[tuple[int, int]] = []
        pc = rng.randrange(0, 64)
        while pc < 0x10000 and len(rows) < 512:
            size = rng.randrange(1, 16)
            rows.append((pc, size))
            pc += size
            if rng.randrange(0, 8) == 0:
                pc += rng.randrange(0, 5)  # unmapped/gap-style model
        if old_find(rows, address) != new_find(rows, address):
            raise AssertionError("binary disassembly-row lookup changed containment semantics")

    # Tick must scan the hook table once for normal disabled owners instead of
    # once per cheat block. Debugger-owned hooks (owner outside m_blocks) remain
    # independent, and the old block-order deactivation is preserved.
    tick = extract_function(cheat, "void CheatEngineWindow::Tick()")
    assert tick.count("for (const auto &entry : m_f_hooks)") == 1
    assert "DeactivateFHooksForBlock(" not in tick
    assert "owner >= m_blocks.size()" in tick
    assert "std::stable_sort(" in tick
    assert "TypeFGuestPauseGuard guest_pause;" in tick
    assert "if (!run_live_blocks)" in tick

    # Model the selected normal cheat-owned hook set. The debugger owner is
    # represented by -1 and must never be selected by the global/live toggles.
    for _ in range(randomized_iterations):
        block_count = rng.randrange(0, 24)
        enabled = [bool(rng.getrandbits(1)) for _ in range(block_count)]
        run_live = bool(rng.getrandbits(1))
        owners = [rng.randrange(-1, block_count + 3) for _ in range(rng.randrange(0, 64))]
        tracked = [bool(rng.getrandbits(1)) for _ in owners]

        old = []
        for block in range(block_count):
            if not run_live or not enabled[block]:
                old.extend(i for i, owner in enumerate(owners)
                           if owner == block and tracked[i])

        selected = [(owner, i) for i, owner in enumerate(owners)
                    if 0 <= owner < block_count
                    and (not run_live or not enabled[owner])
                    and tracked[i]]
        selected.sort(key=lambda item: item[0])  # stable in Python
        new = [i for _, i in selected]
        if new != old:
            raise AssertionError("single-scan F-hook deactivation changed old block ordering")

    print("PASS: v1.84 debugger render/register + F-hook tick streamlining invariants")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
