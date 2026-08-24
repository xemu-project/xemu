#!/usr/bin/env python3
# v2.87 current regression ownership.
"""Regression guard for v1.91 OPT Pass 6 debugger steady-state cleanup."""
from __future__ import annotations

import argparse
import hashlib
import pathlib

from v287_source_test_utils import (
    extract_function, extract_member_functions, read_memory_tools_implementation,
)


EXPECTED_PROTECTED_METHOD_COUNT = 83
EXPECTED_PROTECTED_METHOD_DIGEST = (
    "193fa8301cb790863418075860d1ce7176895dad3649b9338bf4ea51ff203b38"
)
EXPECTED_MEMORY_TOOLS_HEADER_SHA256 = (
    "ae506fdde510c404b27e9ceee0e8578703ee44a68c3af4afd6122be1218e8b3b"
)
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
MUTABLE_METHODS = {
    "DrawBreakpoints",
    "DrawBreakpointContents",
    "DrawDebugger",
    "DrawRegisters",
    "StoreDebuggerPreferences",
    # Later Pass 7 display/cache-only methods are excluded here so this older
    # guard continues to protect the actual Pass-6 ownership changes.
    "RebuildDisassemblyRenderCache",
    "DrawDisassemblyPane",
    "DrawLabelBrowser",
    # Later Pass 8 presentation-cache-only methods are separately guarded.
    "DrawSearch",
    "DrawF0TempRegisters",
    # Pass 10 lifecycle-only pause ownership changes are separately guarded.
    "DumpCurrentPage",
    "DumpLabels",
    # v2.49 Debug-Tools-local tab styling is separately guarded.
    "Draw",
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


def protected_digest(text: str) -> tuple[int, str]:
    functions = sorted(
        (item for item in extract_member_functions(text, "MemoryToolsWindow")
         if item[0] not in MUTABLE_METHODS),
        key=lambda item: item[0],
    )
    records = [f"{name}#{index}\n{body}" for index, (name, body) in enumerate(functions)]
    blob = "\n\0\n".join(records)
    return len(records), hashlib.sha256(blob.encode("utf-8")).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    root = pathlib.Path(parser.parse_args().root).resolve()
    debug = root / "ui/xui/debug-tools"
    implementation = read_memory_tools_implementation(debug)
    debugger = "\n\n".join((debug / name).read_text(encoding="utf-8") for name in (
        "memory-tools-debugger.cc", "memory-tools-debugger-ui.cc"))
    header = (debug / "memory-tools.hh").read_text(encoding="utf-8")

    store = extract_function(
        implementation, "void MemoryToolsWindow::StoreDebuggerPreferences()")
    draw = extract_function(implementation, "void MemoryToolsWindow::DrawDebugger()")
    registers = extract_function(
        implementation, "void MemoryToolsWindow::DrawRegisters(")
    breakpoints = extract_function(
        implementation, "void MemoryToolsWindow::DrawBreakpointContents()")

    # Preference persistence still updates the same six in-memory xemu config
    # values, but the frame-end path no longer writes identical values again.
    for field, value in (
        ("disassembly_pane_height", "m_disasm_pane_height"),
        ("disassembly_full_page", "m_disasm_full_page"),
        ("disassembly_instruction_count", "m_disasm_instruction_count"),
        ("follow_eip", "m_follow_eip"),
        ("labels_enabled", "m_labels_enabled"),
        ("register_view", "m_register_view"),
    ):
        guard = f"if (prefs.{field} != {value})"
        assign = f"prefs.{field} = {value};"
        if guard not in store or assign not in store:
            raise AssertionError(f"unchanged preference-write guard missing: {field}")
    if "xemu_settings_save" in store:
        raise AssertionError("Pass 6 introduced settings-file I/O in debugger frame path")

    # General architectural registers remain on the established 100 ms live
    # cadence because EIP/highlights use them. Hidden x87/MMX/SSE state is not
    # synchronized until an extra-register tab is visible.
    for needle in (
        "now - m_last_live_register_refresh >= 0.10",
        "xemu_cheat_get_x86_registers(&live_regs)",
        "if (m_register_view != 0)",
        "xemu_cheat_get_x86_extra_registers(&extra_regs)",
    ):
        if needle not in draw:
            raise AssertionError(f"live-register cadence invariant missing: {needle}")
    extra_guard = draw.index("if (m_register_view != 0)")
    extra_fetch = draw.index("xemu_cheat_get_x86_extra_registers(&extra_regs)")
    if extra_fetch < extra_guard:
        raise AssertionError("hidden extra-register backend state is still fetched unconditionally")
    if draw.count("xemu_cheat_debug_backend()") != 1:
        raise AssertionError("DrawDebugger no longer uses one frame-local backend snapshot")

    # Leaving General must refresh extra state in the same DrawRegisters frame,
    # before the newly selected x87/MMX/SSE table is rendered.
    for needle in (
        "const int previous_register_view = m_register_view;",
        "auto refresh_extra_on_general_exit =",
        "previous_register_view != 0 || new_view == 0",
        "xemu_cheat_get_x86_extra_registers(&extra_regs)",
        "refresh_extra_on_general_exit(m_register_view);",
    ):
        if needle not in registers:
            raise AssertionError(f"same-frame extra-register activation invariant missing: {needle}")
    if registers.count("refresh_extra_on_general_exit(m_register_view);") != 3:
        raise AssertionError("all three extra-register tabs must use same-frame activation refresh")

    # Breakpoint Physical columns remain live. The optimization is strictly a
    # one-call page cache while stopped; no mapping result survives a frame and
    # running-state translation always falls back to the historical direct path.
    for needle in (
        "const bool guest_running = runstate_is_running();",
        "!guest_running",
        "virtual_map_prepared = xemu_cheat_prepare_virtual_map() != 0;",
        "std::array<BreakpointPageTranslation, 16> page_translations = {};",
        "if (!virtual_map_prepared)",
        "return xemu_cheat_virtual_to_physical(address, &physical) != 0;",
        "const uint32_t virtual_page = address & 0xFFFFF000u;",
        "xemu_cheat_virtual_to_physical(virtual_page, &physical_page)",
        "physical_page + (uint64_t)(address - virtual_page)",
        "page_translation_count < page_translations.size()",
        "translate_breakpoint_address(bp.address, physical)",
        "translate_breakpoint_address(wp.address, physical)",
    ):
        if needle not in breakpoints:
            raise AssertionError(f"frame-local breakpoint translation invariant missing: {needle}")
    for forbidden in (
        "m_breakpoint_page_translation",
        "static std::array<BreakpointPageTranslation",
        "static BreakpointPageTranslation",
    ):
        if forbidden in breakpoints or forbidden in debugger:
            raise AssertionError("breakpoint translation cache escaped frame-local ownership")
    if breakpoints.count("xemu_cheat_debug_backend()") != 1:
        raise AssertionError("DrawBreakpoints no longer uses one frame-local backend snapshot")

    # Pass 6 requires no new persistent MemoryTools state. This is important for
    # keeping the Pass-3 split/state ownership and Pass-4 preferences unchanged.
    normalized_header = header
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
    v274_header_additions = (
        "    bool RestoreTrackedInstructionPatch(uint32_t address);\n",
    )
    for addition in PASS8_HEADER_ADDITIONS + v274_header_additions:
        if addition not in normalized_header:
            raise AssertionError(f"missing allowed Pass-8 header addition: {addition.strip()}")
        normalized_header = normalized_header.replace(addition, "", 1)
    if hashlib.sha256(normalized_header.encode("utf-8")).hexdigest() != EXPECTED_MEMORY_TOOLS_HEADER_SHA256:
        raise AssertionError("MemoryTools public/private state layout changed outside Pass 8 / separately guarded v2.74 Inject addition")

    count, digest = protected_digest(implementation)
    if count != EXPECTED_PROTECTED_METHOD_COUNT:
        raise AssertionError(
            f"protected MemoryTools method count changed: {count} != {EXPECTED_PROTECTED_METHOD_COUNT}")
    if digest != EXPECTED_PROTECTED_METHOD_DIGEST:
        raise AssertionError("MemoryTools behavior changed outside the Pass-6/Pass-7 display methods")

    print("PASS: v1.91 debugger steady-state/render-path streamlining invariants")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
