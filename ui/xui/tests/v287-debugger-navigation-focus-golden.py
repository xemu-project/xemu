#!/usr/bin/env python3
# v2.87 current regression ownership.
"""Regression guard for v1.87 debugger navigation keyboard-focus handoff."""
from __future__ import annotations

import argparse
import pathlib

from v287_source_test_utils import extract_function, read_memory_tools_implementation


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    root = pathlib.Path(parser.parse_args().root).resolve()
    debug = root / "ui/xui/debug-tools"

    memory_tools = read_memory_tools_implementation(debug)
    header = (debug / "memory-tools.hh").read_text(encoding="utf-8")

    for needle in (
        "bool m_disasm_keyboard_focus_requested = false;",
        "bool m_disasm_keyboard_focus_physical = false;",
        "bool m_disasm_last_keyboard_focus_physical = false;",
    ):
        if needle not in header:
            raise AssertionError(f"missing debugger navigation-focus state: {needle}")

    navigate = extract_function(
        memory_tools, "void MemoryToolsWindow::NavigateDebuggerAddress(")
    back = extract_function(
        memory_tools, "bool MemoryToolsWindow::NavigateDebuggerBack()")
    forward = extract_function(
        memory_tools, "bool MemoryToolsWindow::NavigateDebuggerForward()")
    follow = extract_function(
        memory_tools, "void MemoryToolsWindow::FollowDebuggerAddress(")
    pane = extract_function(
        memory_tools, "bool MemoryToolsWindow::DrawDisassemblyPane(bool physical)")

    focus_request = (
        "m_disasm_keyboard_focus_requested = true;",
        "m_disasm_keyboard_focus_physical = m_disasm_last_keyboard_focus_physical;",
    )
    for body, name in ((navigate, "Follow"), (back, "Back"), (forward, "Forward")):
        for needle in focus_request:
            if needle not in body:
                raise AssertionError(f"{name} does not hand keyboard focus to its destination: {needle}")

    # Generic address jumps (Go to EIP, breakpoint Go, label browser, etc.)
    # must not unexpectedly steal keyboard focus. The handoff belongs only to
    # browser-style Follow/Back/Forward navigation.
    if "m_disasm_keyboard_focus_requested = true;" in follow:
        raise AssertionError("generic FollowDebuggerAddress unexpectedly steals keyboard focus")

    for needle in (
        "if (focused)",
        "m_disasm_last_keyboard_focus_physical = physical;",
        "const bool restore_keyboard_focus =",
        "m_disasm_keyboard_focus_physical == physical",
        "row.virtual_address == m_selected_disasm_virtual",
        "ImGui::SetKeyboardFocusHere();",
        "m_disasm_keyboard_focus_requested = false;",
    ):
        if needle not in pane:
            raise AssertionError(f"missing destination keyboard-focus restore invariant: {needle}")

    print("PASS: debugger Follow/Back/Forward keyboard focus follows selected destination")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
