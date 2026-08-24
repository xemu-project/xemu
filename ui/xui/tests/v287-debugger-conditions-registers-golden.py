#!/usr/bin/env python3
# v2.87 current regression ownership.
"""Source-level invariants for conditional breakpoints and Current Registers tabs."""
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
    bridge_h = (dbg / "cheat-engine-memory.h").read_text(encoding="utf-8")
    backend = (dbg / "backend/xemu-dbg.c").read_text(encoding="utf-8")
    whpx_backend = (dbg / "backend/whpx-debug.c").read_text(encoding="utf-8")
    meson = (dbg / "meson.build").read_text(encoding="utf-8")

    require(memory, 'ImGui::TableSetupColumn("Condition"', "Condition column")
    require(memory, 'bp.conditions.empty() ? "NO" : "YES"', "execute YES/NO button")
    require(memory, 'wp.conditions.empty() ? "NO" : "YES"', "watchpoint YES/NO button")
    require(memory, 'ImGui::Begin("Breakpoint Conditions"', "condition editor")
    require(memory, 'One condition per line.', "multi-line condition help")
    require(memory, '"==", "Equal to"', "operator hover help")
    require(memory, '"!=", "Not equal to"', "not-equal hover help")
    require(memory, 'xemu_breakpoint_conditions_evaluate(condition_watch->conditions, regs)',
            "watchpoint filtering")
    require(memory, 'xemu_breakpoint_conditions_evaluate(it->conditions, regs)',
            "execute filtering")
    require(memory, 'ContinueFilteredExecuteBreakpoint(regs.pc)',
            "safe execute auto-resume")

    require(memory, 'ImGui::BeginTabBar("current_register_tabs")',
            "tabs inside Current Registers")
    require(memory, '"General", nullptr,', "General tab")
    require(memory, '"x87 / FPU", nullptr,', "x87/FPU tab")
    require(memory, '"MMX", nullptr,', "MMX tab")
    require(memory, '"SSE", nullptr,', "SSE tab")
    require(memory, 'm_register_view = 0;', "shared General register view")
    require(memory, 'm_register_view = 1;', "shared x87/FPU register view")
    require(memory, 'm_register_view = 2;', "shared MMX register view")
    require(memory, 'm_register_view = 3;', "shared SSE register view")
    require(memory, 'DrawExtraRegisterTable(m_break_extra_registers,',
            "Last BP follows extra-register tab")
    require(memory, 'CaptureBreakpointExtraRegisters();',
            "Last BP captures FP/MMX/SSE snapshot")
    require(header, 'XemuCheatX86ExtraRegisters m_break_extra_registers = {};',
            "Last BP extra-register storage")
    require(header, 'int m_register_view = 0;', "shared register tab selection")
    require(memory, 'ImGui::Dummy(ImVec2(0.0f, ImGui::GetFrameHeight()));',
            "Last BP reserves Current tab-row height")
    require(memory, 'ImGuiTableColumnFlags_WidthFixed, 320.0f);',
            "wider paired register columns")
    require(bridge_h, 'typedef struct XemuCheatX86ExtraRegisters',
            "plain extra-register bridge")
    require(backend, 'const int st_index = (i + env->fpstt) & 7;',
            "architectural x87 stack order")
    require(backend, 'mmx[i] = env->fpregs[i].mmx.MMX_Q(0);',
            "MMX aliases")
    require(backend, 'xmm[i][0] = env->xmm_regs[i].ZMM_L(0);',
            "SSE state")
    require(backend, 'return xemu_whpx_get_extra_registers(cpu, st_low, st_high, mmx, xmm,',
            "WHPX exact FP/SIMD route")
    require(whpx_backend, 'WHvX64RegisterFpMmx0 + i',
            "WHPX direct FP/MMX read")
    require(whpx_backend, 'Fp.AsUINT128.High64 & 0xFFFFu',
            "WHPX preserves x87 upper 16 bits")
    require(whpx_backend, 'WHvX64RegisterXmm0 + i',
            "WHPX direct XMM read")
    require(meson, "'breakpoint-conditions.cc'", "condition parser build ownership")

    print("PASS: conditional breakpoint + Current Registers tab invariants")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
