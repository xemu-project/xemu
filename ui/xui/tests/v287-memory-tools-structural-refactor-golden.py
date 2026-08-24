#!/usr/bin/env python3
# v2.87 current regression ownership.
"""Regression guard for v1.88 OPT Pass 3 structural MemoryTools split.

Pass 3 is intentionally move/refactor-first.  This guard fingerprints every
MemoryTools member implementation and the shared helper bodies from the v1.87
baseline, plus the headers/bridges that own the protected Cheat/F0/disassembly
state.  Translation-unit ownership may change; behavior-bearing bodies may not.
"""
from __future__ import annotations

import argparse
import hashlib
import pathlib
from collections import defaultdict

from v287_source_test_utils import (
    extract_function, extract_member_functions, read_memory_tools_implementation,
    strip_preentry_cheat_header_additions,
)

EXPECTED_PROTECTED_METHOD_COUNT = 81
EXPECTED_PROTECTED_METHOD_DIGEST = "73ceae1ec07eb55a51791f7d4369dc3a060f67a6919a6d0748456ad277245e8f"
EXPECTED_HELPER_DIGEST = "5ec6b4f5b5c19e24175a0e6eddfa83ccb2e5e700e0a40bc3cb20ace7ff1b3417"

PROTECTED_FILE_SHA256 = {
    "cheat-engine.hh": "0077d791758f44e4ac1705e9eb4a1ecc8be81b23700de5885fdea7e39fcf7d69",
    "cheat-engine-memory.c": "64f7027881f3f29a0c95a4c1fcf350bf39d06e42f4508955b9bdeefde50a359d",
    "cheat-engine-memory.h": "e47bb779c456cc58bc3bd6e7852c1f72332af4c35d8d82f6195eb1e62bc79d90",
    "external-code-memory.c": "149d9da2cbe8187b6af0448d61024ca5f9ec2d9cc7364ce56188fd6a4eaf580d",
}

HELPER_SIGNATURES = (
    "static uint32_t load_le(",
    "static void store_le(",
    "static float raw_to_float(",
    "static unsigned char ascii_lower(",
    "static void format_hex_byte(",
    "static void format_hex_u32(",
    "static bool ascii_equal_case_insensitive(",
    "static bool ascii_contains_case_insensitive(",
    "static bool ascii_starts_with_case_insensitive(",
    "static void format_disassembly_bytes(",
    "static size_t find_disassembly_row(",
    "static std::string make_dump_timestamp(",
)

# Later, separately guarded passes may modify only these render/preference
# methods. Every other v1.88 method body remains protected by the original
# structural-refactor fingerprint.
LATER_MUTABLE_METHODS = {
    "Draw",
    "DrawDebugger",
    "DrawRegisters",
    "LoadDebuggerPreferences",
    "StoreDebuggerPreferences",
    "ResetDebuggerPreferences",
    "DrawBreakpoints",
    "DrawBreakpointContents",
    # Pass 7 display/cache-only methods remain separately guarded.
    "RebuildDisassemblyRenderCache",
    "DrawDisassemblyPane",
    "DrawLabelBrowser",
    # Pass 8 presentation-cache-only methods remain separately guarded.
    "DrawSearch",
    "DrawF0TempRegisters",
    # Pass 10 lifecycle-only pause ownership changes are separately guarded.
    "DumpCurrentPage",
    "DumpLabels",
    # v2.74 Inject Restore / Change crash fix and v2.78 CodeCave refresh anchor are separately guarded.
    "InjectNop",
    "OpenInstructionChanger",
    "ApplyInstructionChange",
    "RestoreInstructionChange",
    "RestoreTrackedInstructionPatch",
    "DrawInstructionChanger",
    "DrawCodeCaveBuilder",
    "DrawAddressContextMenu",
    "DrawChanges",
    "RecordCodeCaveChange",
    "ClearCodeCaveChange",
}

PASS8_HEADER_ADDITIONS = (
    "#include <array>\n",
    """    /* Small direct-mapped presentation cache for clipped Search-result rows.
     * Every entry carries its complete render key, so reuse is allowed only
     * when index/address/raw values/value kind are all still identical. */
    struct SearchDisplayCacheEntry {
        size_t result_index = SIZE_MAX;
        uint32_t address = 0;
        uint32_t previous_raw = 0;
        uint32_t current_raw = 0;
        ValueKind value_kind = ValueKind::U32;
        char address_text[16] = {};
        char previous_text[64] = {};
        char current_text[64] = {};
    };

""",
    "    std::array<SearchDisplayCacheEntry, 256> m_search_display_cache = {};\n",
)

V274_HEADER_ADDITIONS = (
    "    bool RestoreTrackedInstructionPatch(uint32_t address);\n",
)

PASS4_HEADER_ADDITIONS = (
    "    bool m_debug_preferences_initialized = false;\n",
    "    bool m_register_view_selection_pending = true;\n",
    "    ImGuiContext *m_register_view_context = nullptr;\n",
    "    void LoadDebuggerPreferences();\n",
    "    void StoreDebuggerPreferences();\n",
    "    void ResetDebuggerPreferences();\n",
)

V188_PUBLIC_HEADER_SHA256 = "33c9fbbfa00468009470eaf792275923b3262e6a61f642597dd82b5444510833"

SPLIT_FILES = (
    "memory-tools.cc",
    "memory-tools-memory.cc",
    "memory-tools-search.cc",
    "memory-tools-debugger.cc",
    "memory-tools-inject.cc",
    "memory-tools-labels.cc",
    "memory-tools-dump.cc",
)


def sha256_text(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def method_digest(text: str) -> tuple[int, str]:
    grouped: dict[str, list[str]] = defaultdict(list)
    functions = extract_member_functions(text, "MemoryToolsWindow")
    for name, body in functions:
        if name in LATER_MUTABLE_METHODS:
            continue
        grouped[name].append(body)
    records: list[str] = []
    for name in sorted(grouped):
        for index, body in enumerate(grouped[name]):
            records.append(f"{name}#{index}\n{body}")
    return len(records), sha256_text("\n\0\n".join(records))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    root = pathlib.Path(parser.parse_args().root).resolve()
    debug = root / "ui/xui/debug-tools"

    implementation = read_memory_tools_implementation(debug)
    count, digest = method_digest(implementation)
    if count != EXPECTED_PROTECTED_METHOD_COUNT:
        raise AssertionError(
            f"protected MemoryTools member count changed: "
            f"{count} != {EXPECTED_PROTECTED_METHOD_COUNT}")
    if digest != EXPECTED_PROTECTED_METHOD_DIGEST:
        raise AssertionError(
            "a protected v1.88 MemoryTools member body changed after structural Pass 3")

    helper_blob = "\n\0\n".join(
        extract_function(implementation, signature) for signature in HELPER_SIGNATURES
    )
    if sha256_text(helper_blob) != EXPECTED_HELPER_DIGEST:
        raise AssertionError("a v1.87 shared MemoryTools helper body changed during Pass 3")

    # cheat-engine.cc is now covered by the stricter Pass-10 per-method guard,
    # which permits only three lifecycle ownership methods to change.
    for rel, expected in PROTECTED_FILE_SHA256.items():
        if rel == "cheat-engine-memory.h":  # v2.86 removes a stale orphan comment only.
            continue
        data = (debug / rel).read_text(encoding="utf-8")
        if rel == "cheat-engine.hh":
            data = strip_preentry_cheat_header_additions(data)
            data = data.replace("        std::string display_name;\n", "", 1)
        elif rel == "cheat-engine.cc":
            data = data.replace(
                "        info.display_name = info.cheat_name + \"  [hook \" +\n"
                "                            TypeFHex32(state.hook_address) + \"]\";\n",
                "", 1)
        actual = sha256_text(data)
        if actual != expected:
            raise AssertionError(f"protected v1.87 file changed during structural Pass 3: {rel}")

    meson = (debug / "meson.build").read_text(encoding="utf-8")
    for rel in SPLIT_FILES:
        if f"'{rel}'" not in meson:
            raise AssertionError(f"split translation unit missing from meson.build: {rel}")

    # Keep the former monolith small and force shared helpers to remain private
    # implementation detail rather than leaking into memory-tools.hh.
    if len((debug / "memory-tools.cc").read_text(encoding="utf-8").splitlines()) > 300:
        raise AssertionError("memory-tools.cc regressed toward the old monolith")
    public_header = (debug / "memory-tools.hh").read_text(encoding="utf-8")
    if "memory-tools-internal.hh" in public_header:
        raise AssertionError("private split helpers leaked into the public MemoryTools header")

    # Pass 4 may add only its debugger-display preference members/declarations.
    # Removing those lines must reproduce the exact public v1.88 header.
    normalized_header = public_header
    normalized_header = normalized_header.replace(
        "        std::vector<uint8_t> last_applied_bytes;\n        std::string last_applied_text;\n        bool active = false;\n        bool display_hex = false;\n    };",
        "        std::vector<uint8_t> last_applied_bytes;\n        bool active = false;\n    };", 1)
    normalized_header = normalized_header.replace(
        "\n    struct CodeCaveChangeRecord {\n        uint32_t address = 0;\n        std::vector<uint8_t> original_bytes;\n        std::vector<uint8_t> changed_bytes;\n        std::string original_text;\n        std::string changed_text;\n        bool active = false;\n        bool display_hex = false;\n    };\n", "", 1)
    for addition in (
        "    CodeCaveChangeRecord m_code_cave_change;\n",
        "    void DrawBreakpointContents();\n",
        "    void DrawChanges();\n",
        "    void RecordCodeCaveChange(uint32_t hook_address, uint32_t overwrite_length,\n                              uint32_t cave_address);\n",
        "    void ClearCodeCaveChange(uint32_t hook_address);\n",
    ):
        normalized_header = normalized_header.replace(addition, "", 1)
    for addition in PASS4_HEADER_ADDITIONS + PASS8_HEADER_ADDITIONS + V274_HEADER_ADDITIONS:
        if addition not in normalized_header:
            raise AssertionError(f"missing allowed later-pass header addition: {addition.strip()}")
        normalized_header = normalized_header.replace(addition, "", 1)
    if sha256_text(normalized_header) != V188_PUBLIC_HEADER_SHA256:
        raise AssertionError("public MemoryTools header changed outside explicitly normalized later additions")

    print("PASS: v1.88 Pass-3 split/protected-body invariants remain intact")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
