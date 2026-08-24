#!/usr/bin/env python3
# v2.87 current regression ownership.
"""Static invariants for the v1.77 XBE/.xlabel/XDK/MAP/PDB label UI/integration."""
from __future__ import annotations
import argparse
from pathlib import Path
from v287_source_test_utils import read_memory_tools_implementation


def need(text: str, token: str, where: str) -> None:
    if token not in text:
        raise SystemExit(f"FAIL: missing {token!r} in {where}")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    root = Path(ap.parse_args().root).resolve()
    dt = root / "ui/xui/debug-tools"
    meson = (dt / "meson.build").read_text()
    current = ((dt / "current-game.cc").read_text() + "\n" + (dt / "current-game-ui.cc").read_text())
    mem = read_memory_tools_implementation(dt)
    labels = (dt / "xbe-labels.cc").read_text()

    need(meson, "'xbe-labels.cc'", "meson.build")
    need(meson, "'label-packs.cc'", "meson.build")
    need(meson, "'xdk-labels.cc'", "meson.build")
    need(meson, "'map-labels.cc'", "meson.build")
    need(meson, "'pdb-labels.cc'", "meson.build")
    need(current, "XemuXbeLabels::Build", "current-game.cc")
    need(current, "ReloadLabelPacks", "current-game.cc")
    need(current, '"XDK", "PDB", "Packs", "Cache"', "current-game.cc")
    need(current, 'XemuXdkLabels::Process', "current-game.cc")
    need(current, 'XemuMapLabels::ParseAndResolve', "current-game.cc")
    need(current, 'XemuPdbLabels::ParseAndResolve', "current-game.cc")
    need(current, 'XemuPdbLabels::ExtractXbeIdentity', "current-game.cc")
    need(current, 'ImGui::Text("XDK Build', "current-game.cc")
    need(mem, 'ImGui::Checkbox("Labels"', "MemoryTools implementation")
    need(mem, 'ImGui::Button("LABELS")', "MemoryTools implementation")
    need(mem, 'ImGui::Button("DUMP LABELS")', "MemoryTools implementation")
    need(mem, 'ImGui::Button("LOAD .xlabel")', "MemoryTools implementation")
    need(mem, 'ImGui::Button("SAVE .xlabel")', "MemoryTools implementation")
    need(mem, 'ImGui::Button("LOAD .map")', "MemoryTools implementation")
    need(mem, 'ImGui::Button("LOAD .pdb")', "MemoryTools implementation")
    need(mem, 'ImGui::Button("RELOAD PACKS")', "MemoryTools implementation")
    need(mem, 'ImGui::Button("BUILD / REFRESH XDK INDEX")', "MemoryTools implementation")
    need(mem, 'Source##label_source_filter', "MemoryTools implementation")
    need(mem, "label_it->virtual_address == row.virtual_address", "MemoryTools implementation")
    need(mem, "translate_label_address(label.virtual_address, physical)", "MemoryTools implementation")
    need(mem, "UNMAPPED", "MemoryTools implementation")
    need(labels, '"~" + candidate.label_stem', "xbe-labels.cc")
    need(labels, '"kernel_"', "xbe-labels.cc")
    need(labels, 'section.name != ".text"', "xbe-labels.cc")
    need(mem, 'ImGui::TableSetupColumn("Source"', "MemoryTools implementation")
    need(mem, '"MAP", "Manual"', "MemoryTools implementation")
    print("PASS: v1.77 XBE/.xlabel/XDK/MAP/PDB label UI/source invariants")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
