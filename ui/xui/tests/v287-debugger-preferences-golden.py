#!/usr/bin/env python3
# v2.87 current regression ownership.
"""Regression guard for v1.89 OPT Pass 4 debugger-owned preferences."""
from __future__ import annotations

import argparse
import pathlib

from v287_source_test_utils import extract_function, read_memory_tools_implementation


CONFIG_DEFAULTS = (
    "disassembly_pane_height:\n        type: number\n        default: 320.0",
    "disassembly_full_page:\n        type: bool\n        default: true",
    "disassembly_instruction_count:\n        type: integer\n        default: 32",
    "follow_eip:\n        type: bool\n        default: true",
    "labels_enabled:\n        type: bool\n        default: true",
    "register_view:\n        type: integer\n        default: 0",
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    root = pathlib.Path(parser.parse_args().root).resolve()
    debug = root / "ui/xui/debug-tools"

    config = (root / "config_spec.yml").read_text(encoding="utf-8")
    if "    memory_tools:\n" not in config:
        raise AssertionError("display.debug.memory_tools config section is missing")
    for needle in CONFIG_DEFAULTS:
        if needle not in config:
            raise AssertionError(f"missing debugger preference/default: {needle.split(':', 1)[0]}")

    implementation = read_memory_tools_implementation(debug)
    header = (debug / "memory-tools.hh").read_text(encoding="utf-8")
    detached = (debug / "detached-tools.cc").read_text(encoding="utf-8")
    main_ui = (root / "ui/xui/main.cc").read_text(encoding="utf-8")

    for needle in (
        "bool m_debug_preferences_initialized = false;",
        "bool m_register_view_selection_pending = true;",
        "ImGuiContext *m_register_view_context = nullptr;",
        "void LoadDebuggerPreferences();",
        "void StoreDebuggerPreferences();",
        "void ResetDebuggerPreferences();",
    ):
        if needle not in header:
            raise AssertionError(f"missing preference ownership state/declaration: {needle}")

    load = extract_function(
        implementation, "void MemoryToolsWindow::LoadDebuggerPreferences()")
    store = extract_function(
        implementation, "void MemoryToolsWindow::StoreDebuggerPreferences()")
    reset = extract_function(
        implementation, "void MemoryToolsWindow::ResetDebuggerPreferences()")
    window_draw = extract_function(implementation, "void MemoryToolsWindow::Draw(bool detached)")
    draw = extract_function(implementation, "void MemoryToolsWindow::DrawDebugger()")
    registers = extract_function(
        implementation, "void MemoryToolsWindow::DrawRegisters(")

    for needle in (
        "g_config.display.debug.memory_tools",
        "std::clamp((float)prefs.disassembly_pane_height",
        "160.0f, 1200.0f",
        "std::clamp(prefs.disassembly_instruction_count, 1, 128)",
        "std::clamp(prefs.register_view, 0, 3)",
        "StoreDebuggerPreferences();",
    ):
        if needle not in load:
            raise AssertionError(f"preference load/sanitization invariant missing: {needle}")

    for needle in (
        "prefs.disassembly_pane_height = m_disasm_pane_height;",
        "prefs.disassembly_full_page = m_disasm_full_page;",
        "prefs.disassembly_instruction_count = m_disasm_instruction_count;",
        "prefs.follow_eip = m_follow_eip;",
        "prefs.labels_enabled = m_labels_enabled;",
        "prefs.register_view = m_register_view;",
    ):
        if needle not in store:
            raise AssertionError(f"preference store invariant missing: {needle}")
    if "xemu_settings_save" in load + store + draw:
        raise AssertionError("debugger preference hot path performs settings-file I/O")

    for needle in (
        "m_disasm_pane_height = 320.0f;",
        "m_disasm_full_page = true;",
        "m_disasm_instruction_count = 32;",
        "m_follow_eip = true;",
        "m_labels_enabled = true;",
        "m_register_view = 0;",
        "m_register_view_selection_pending = true;",
        "StoreDebuggerPreferences();",
    ):
        if needle not in reset:
            raise AssertionError(f"Reset UI default missing: {needle}")
    for forbidden in (
        "m_breakpoints", "m_watchpoints", "m_debug_nav_history",
        "m_code_cave", "m_change_instruction", "vm_start", "vm_stop",
        "xemu_cheat_", "m_registers", "m_break_registers",
    ):
        if forbidden in reset:
            raise AssertionError(f"Reset UI touches non-display state: {forbidden}")

    if "LoadDebuggerPreferences();" not in window_draw:
        raise AssertionError("MemoryTools Draw does not load preferences before tab/context actions")
    if not draw.lstrip().startswith("void MemoryToolsWindow::DrawDebugger()"):
        raise AssertionError("could not inspect DrawDebugger")
    if "LoadDebuggerPreferences();" not in draw:
        raise AssertionError("DrawDebugger does not defensively lazy-load persisted preferences")
    if "ImGui::Button(\"RESET UI\")" not in draw or "ResetDebuggerPreferences();" not in draw:
        raise AssertionError("RESET UI action is missing")
    if draw.rfind("StoreDebuggerPreferences();") < draw.rfind("DrawBreakpoints();"):
        raise AssertionError("DrawDebugger stores preferences before register/splitter UI finishes")

    for needle in (
        "ImGui::GetCurrentContext()",
        "m_register_view_context != current_context",
        "m_register_view_selection_pending = true;",
        "requested_register_view",
        "ImGuiTabItemFlags_SetSelected",
        "m_register_view_selection_pending = false;",
    ):
        if needle not in registers:
            raise AssertionError(f"register-tab persistence/context handoff missing: {needle}")

    # Pass 4 must not switch either main or detached ImGui contexts to global
    # ImGui .ini persistence; only the debugger-owned config keys are persisted.
    if "io.IniFilename = nullptr;" not in detached:
        raise AssertionError("detached debug context unexpectedly enabled ImGui ini persistence")
    if "io.IniFilename = NULL;" not in main_ui:
        raise AssertionError("main UI unexpectedly enabled ImGui ini persistence")

    print("PASS: v1.89 debugger-owned persistent preferences/reset invariants")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
