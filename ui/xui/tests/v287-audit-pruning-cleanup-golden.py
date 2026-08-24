#!/usr/bin/env python3
# v2.87 current regression ownership.
"""Regression guard for v1.94 OPT Pass 9 audit/pruning cleanup."""
from __future__ import annotations

import argparse
import hashlib
import pathlib

from v287_source_test_utils import strip_preentry_cheat_header_additions


PROTECTED_V193_SHA256 = {
    "cheat-engine.hh": "3580c3193a0bfbe0e3ffbf6fc72b8c46f92a21614759e6f73304b59a96fbb6c1",
    "cheat-engine-memory.c": "64f7027881f3f29a0c95a4c1fcf350bf39d06e42f4508955b9bdeefde50a359d",
    "cheat-engine-memory.h": "e47bb779c456cc58bc3bd6e7852c1f72332af4c35d8d82f6195eb1e62bc79d90",
    "external-code-memory.c": "149d9da2cbe8187b6af0448d61024ca5f9ec2d9cc7364ce56188fd6a4eaf580d",
    "memory-tools.hh": "42df7062c462dd16bccd64fcaa7fe9b39de5808b6898ea89a975be84bfbeb847",
    "memory-tools.cc": "e0dfa3ab88e71cc4bc3f4b2879dc2c898c5987b39b87fe79c5e72cb5ac968a5c",
    "memory-tools-memory.cc": "cc342f973e9ecbc2f667b147f41686fd56063aa558a68035583d4f12e5987cda",
    "memory-tools-search.cc": "95bf09f34ed0e15525deeb6ff5524cc39ce26f760f9e5ed9bf23c378df799636",
    "memory-tools-inject.cc": "07ca236adbe9b879b69b0557cd11189e1deaa50e0509fb7525a82d418d9d4609",
    "backend/xemu-dbg.c": "0fff339f0c480f2654d8d8805820fae462cff11b85a1c7e11a23ca074429c1ef",
    "backend/whpx-debug.c": "7ba0d7e6d5efacf298ff07520c6bb40df7a947e925b8ef9c05f7e8cac8b66368",
    "x86-cheat-assembler.cc": "b921bce1fb3cbd9183a40351452960c3661b6010d4220ccb6544f82cf5a96f43",
    "x86-cheat-assembler.hh": "863e65c3b20d635f186bd3a047cce4a4ee0e29eaff95711d984eab0bcdbe27cc",
    "xbe-labels.cc": "84b4357567de9b78ba09b00059290f75fc21874aa14020dde363a6f75b0314ee",
    "xbe-labels.hh": "9fbc820301978642ff16bc05dd524fa53593512de936fd28d9b91c1e53e92085",
    "label-packs.cc": "221fac334e614a4e900cf9383d27c6862be3ad2e17841aa8b0bd4688fc2be126",
    "label-packs.hh": "e09a4c21522bcdbe6648fd8de5da150ea475ed8984f7ed64a989cb8f249d55da",
    "xdk-labels.cc": "cdece8af036195fa32d2ae9d834ab5b363822e8ff65e052ae254333e264cf47c",
    "xdk-labels.hh": "8ecaded714f181beefe37e54cdec19fe9e3bd8ae98604be5173d1b9636128126",
    "xdvdfs-disc.cc": "9546c72a5918b3aedb50ce713a53a7e6ee604f9f5b050b5c56e58c50dbce8b85",
    "xdvdfs-disc.hh": "eb859f9b28f47278098df89702d91db3567f6fd55b1d6b29bb4e3e83f028e24b",
}


def sha256(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    root = pathlib.Path(parser.parse_args().root).resolve()
    debug = root / "ui/xui/debug-tools"
    tests = debug / "tests"

    # Behavior-sensitive runtime paths outside the explicitly audited label
    # parser/dead API files, later Pass-10 lifecycle ownership files, and the
    # separately guarded v1.97 detached-window playback fix, v1.98 register-copy
    # UI path, and v2.04 BlockBackend FATX delete bridge remain byte-identical
    # to v1.93. Later guards fingerprint their explicitly allowed files.
    later_scoped_runtime = {
        "cheat-engine-memory.h",  # v2.86 removes only a stale orphan comment.
        # v2.83 Combined Phase 8 owns the split Inject UI and x86 assembler files.
        "memory-tools-inject.cc", "memory-tools-inject-ui.cc",
        "x86-cheat-assembler.cc", "x86-cheat-assembler-encode.cc",
        "x86-cheat-assembler-internal.hh",
        "xdvdfs-disc.cc", "xdvdfs-disc.hh",
        # v2.73 consolidates duplicate parser helpers; its dedicated guard
        # freezes the exact replacement and both parsers remain native-tested.
        "xbe-labels.cc", "xdk-labels.cc",
        "label-packs.cc",  # v2.86 reuses shared ASCII uppercase helper.
        # v2.74 Inject Restore / Change crash fix is separately fingerprinted.
        "memory-tools.hh", "memory-tools-inject.cc",
        # v2.79 Memory Viewer UI/core ownership split.
        "memory-tools-memory.cc",
        # v2.82 Combined Phase 7 Search UI ownership split.
        "memory-tools-search.cc",
    }
    for rel, expected in PROTECTED_V193_SHA256.items():
        if rel in later_scoped_runtime:
            continue
        if rel == "cheat-engine.hh":
            data = (debug / rel).read_text(encoding="utf-8")
            actual = hashlib.sha256(
                strip_preentry_cheat_header_additions(data).encode("utf-8")
            ).hexdigest()
        elif rel == "memory-tools.cc":
            data = (debug / rel).read_text(encoding="utf-8")
            include = '#include "tab-style.hh"\n'
            if data.count(include) != 1:
                raise AssertionError("Debug Tools memory-tools tab-style include changed unexpectedly")
            data = data.replace(include, "", 1)
            scoped = '    XemuDebugUi::ScopedTabStyle tab_style;\n'
            restore = '    tab_style.Restore();\n'
            push = '    XemuDebugUi::PushTabStyle();\n'
            pop = '    XemuDebugUi::PopTabStyle();\n'
            if data.count(scoped) == 1 and push not in data and pop not in data:
                data = data.replace(scoped, "", 1)
                if data.count(restore) == 1:
                    data = data.replace(restore, "", 1)
            elif data.count(push) == 1 and data.count(pop) == 1 and scoped not in data:
                data = data.replace(push, "", 1).replace(pop, "", 1)
            else:
                raise AssertionError("Debug Tools memory-tools tab-style ownership changed unexpectedly")
            actual = hashlib.sha256(data.encode("utf-8")).hexdigest()
        else:
            actual = sha256(debug / rel)
        if actual != expected:
            raise AssertionError(f"protected v1.93 runtime file changed: {rel}")

    shared = (debug / "label-symbol-utils.hh").read_text(encoding="utf-8")
    map_cc = (debug / "map-labels.cc").read_text(encoding="utf-8")
    pdb_cc = (debug / "pdb-labels.cc").read_text(encoding="utf-8")
    bp_cc = (debug / "breakpoint-conditions.cc").read_text(encoding="utf-8")
    bp_hh = (debug / "breakpoint-conditions.hh").read_text(encoding="utf-8")

    for helper in ("all_digits", "clean_c_symbol", "split_at", "simple_msvc_name",
                   "compiler_internal_symbol", "display_microsoft_symbol"):
        if f"inline " not in shared or f"{helper}(" not in shared:
            raise AssertionError(f"shared Microsoft symbol helper missing: {helper}")

    if "XemuLabelSymbolUtils::display_microsoft_symbol(symbol.name)" not in map_cc:
        raise AssertionError("MAP parser is not using shared Microsoft display helper")
    if "XemuLabelSymbolUtils::display_microsoft_symbol(name)" not in pdb_cc:
        raise AssertionError("PDB parser is not using shared Microsoft display helper")
    for cc, label in ((map_cc, "MAP"), (pdb_cc, "PDB")):
        if "static bool compiler_internal_symbol(" in cc:
            raise AssertionError(f"{label} parser still owns duplicate compiler filter")

    # MAP and PDB intentionally retain distinct compiler-internal filters.
    if 'name.rfind("___@@_PchSym_", 0)' in map_cc or 'name.rfind("??_C@", 0)' in map_cc:
        raise AssertionError("PDB-only internal-symbol filters leaked into MAP behavior")
    for needle in ('name.rfind("___@@_PchSym_", 0)', 'name.rfind("??_C@", 0)'):
        if needle not in pdb_cc:
            raise AssertionError(f"PDB internal-symbol filter changed: {needle}")

    # This exported register-name helper had no source caller in v1.93. Pass 9
    # removes the dead declaration/definition without touching parse/evaluate.
    dead_api = "xemu_breakpoint_condition_register_name"
    if dead_api in bp_cc or dead_api in bp_hh:
        raise AssertionError("dead breakpoint-condition register-name API returned")
    for live_api in ("xemu_breakpoint_conditions_parse", "xemu_breakpoint_conditions_evaluate"):
        if live_api not in bp_cc or live_api not in bp_hh:
            raise AssertionError(f"live breakpoint-condition API missing: {live_api}")

    source_utils = (tests / "v287_source_test_utils.py").read_text(encoding="utf-8")
    if "def extract_member_functions(text: str, class_name: str)" not in source_utils:
        raise AssertionError("shared class-member source-test extractor missing")
    for name in (
        "v287-memory-tools-structural-refactor-golden.py",
        "v287-runtime-xbe-polling-golden.py",
        "v287-debugger-steady-state-render-golden.py",
        "v287-display-label-cache-golden.py",
        "v287-table-list-render-golden.py",
    ):
        text = (tests / name).read_text(encoding="utf-8")
        if "def member_functions(" in text:
            raise AssertionError(f"duplicated member-function extractor returned: {name}")
        if "extract_member_functions" not in text:
            raise AssertionError(f"shared member extractor not used: {name}")

    print("PASS: v1.94 Pass-9 audit/pruning/technical-debt invariants")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
