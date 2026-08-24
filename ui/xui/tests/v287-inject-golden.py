#!/usr/bin/env python3
# v2.87 current regression ownership.
"""Source-level invariants for x86 Debugger Inject > NOP / Change / CodeCave."""
from __future__ import annotations

import argparse
import pathlib
import sys
from v287_source_test_utils import extract_function, read_memory_tools_implementation, read_cheat_engine_implementation


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
    bridge = (dbg / "cheat-engine-memory.c").read_text(encoding="utf-8")
    bridge_h = (dbg / "cheat-engine-memory.h").read_text(encoding="utf-8")
    external = (dbg / "external-code-memory.c").read_text(encoding="utf-8")
    backend = (dbg / "backend/xemu-dbg.c").read_text(encoding="utf-8")
    engine = read_cheat_engine_implementation(dbg)
    actions = (root / "ui/xui/actions.cc").read_text(encoding="utf-8")
    inject_ui = (dbg / "memory-tools-inject-ui.cc").read_text(encoding="utf-8")
    assembler = "\n\n".join(
        (dbg / name).read_text(encoding="utf-8")
        for name in ("x86-cheat-assembler.cc", "x86-cheat-assembler-encode.cc")
    )

    require(memory, 'ImGui::BeginMenu("Inject")', "Inject submenu")
    require(memory, 'ImGui::MenuItem("NOP"', "NOP action")
    require(memory, 'ImGui::MenuItem("Change"', "Change action")
    require(memory, 'ImGui::MenuItem("CodeCave"', "CodeCave action")
    require(memory, 'xemu_cheat_assemble_x86_32_change_instruction(',
            "Change uses address-aware production assembler helper")
    require(memory, 'm_change_instruction_preview_bytes.resize(m_change_instruction_span, 0x90)',
            "short replacement NOP padding")
    require(memory, 'Use Inject > CodeCave for a longer replacement.',
            "oversize replacement guard")
    require(memory, 'guest bytes no longer match the tracked NOP/Change patch',
            "safe tracked Inject restore guard")
    require(memory, 'm_instruction_change_history',
            "persistent NOP/Change original-history state")
    require(memory, 'Original instruction (remembered)',
            "Change original bytes/ASM display")
    require(memory, 'ImGui::Button("RESTORE", ImVec2(90.0f, 0.0f))',
            "Change exact-original Restore action")
    require(memory, 'm_change_instruction_source = m_change_instruction_original_text;',
            "Restore repopulates Replacement with remembered original instruction")
    require(memory, 'm_change_instruction_preview_bytes.assign(',
            "Restore uses exact remembered original bytes")
    require(memory, 'ImGui::BeginDisabled(!m_change_instruction_preview_valid);',
            "Change Apply uses balanced unconditional ImGui disabled scope")
    require(memory, 'std::vector<uint8_t> nops(record.span, 0x90);',
            "tracked instruction-span NOP patch")
    require(memory, 'record.last_applied_bytes = nops;',
            "NOP records exact applied bytes for Restore")
    require(memory, 'record.active = true;',
            "NOP marks tracked patch active")
    require(memory, 'RestoreTrackedInstructionPatch(disasm_row->virtual_address);',
            "Inject Restore routes through tracked original-byte restore")
    require(memory, 'restorable_instruction_patch && ImGui::MenuItem("Restore")',
            "Inject Restore is shown only for a tracked NOP/Change patch")
    for removed in ('REVERT TO ORIGINAL', 'RESET TO CURRENT', 'USE ORIGINAL'):
        if removed in memory:
            raise AssertionError(f"obsolete Change button returned: {removed}")
    require(memory, 'm_code_cave_overwrite_length < 5u',
            "whole-instruction JMP sizing")
    require(memory, 'cheat_engine_window.InstallDebuggerF0(',
            "CodeCave production Type-F route")
    require(memory, 'cheat_engine_window.RemoveDebuggerF0(',
            "CodeCave explicit restore")

    # v2.88 Breakpoints | Changes debugger panel. Changes is presentation-only
    # over the already-owned Restore history and the one debugger F0 hook.
    require(memory, 'ImGui::BeginTabBar("debugger_right_panel_tabs")',
            "Breakpoints/Changes tab bar")
    require(memory, 'ImGui::BeginTabItem("Breakpoints")',
            "Breakpoints tab")
    require(memory, 'ImGui::BeginTabItem("Changes")',
            "Changes tab")
    for column in ('"Address"', '"Original"', '"Changed"', '"HEX"'):
        require(memory, f'ImGui::TableSetupColumn({column}',
                f"Changes table {column} column")
    require(header, 'bool display_hex = false;',
            "Changes HEX defaults unchecked")
    require(memory, 'DrawAddressContextMenu(AddressSpace::Virtual, address, ContextOrigin::Debugger,',
            "Changes Address reuses debugger context menu")
    require(memory, 'FollowDebuggerAddress(address, true);',
            "Changes address double-click follow")
    require(memory, 'RecordCodeCaveChange(installed.hook_address, installed.overwrite_length,',
            "CodeCave Changes row records only installed hook address")
    require(memory, 'not the F0 cave body.',
            "CodeCave table excludes generated cave body")

    # v2.88.4 Reset discards debugger-only Changes ownership immediately, but
    # defers the existing disassembler refresh until QEMU has consumed the reset.
    # PREENTRY remains a separate lifecycle call.
    require(actions, 'void xemu_memory_tools_notify_game_reset();',
            "Reset notification bridge declaration")
    reset = extract_function(actions, "void ActionReset(void)")
    reset_order = (
        'xemu_memory_tools_notify_game_reset();',
        'cheat_engine_window.NotifyGameResetRequested();',
        'qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);',
    )
    reset_pos = -1
    for token in reset_order:
        pos = reset.find(token, reset_pos + 1)
        if pos < 0:
            raise AssertionError(f"Reset lifecycle call/order missing: {token}")
        reset_pos = pos
    draw_debugger = extract_function(memory, "void MemoryToolsWindow::DrawDebugger()")
    for token in (
        'if (g_forget_debugger_changes_on_next_debugger_draw)',
        'm_instruction_change_history.clear();',
        'm_change_instruction_record_index = (size_t)-1;',
        'm_change_instruction_applied_bytes.clear();',
        'm_change_instruction_applied = false;',
        'm_code_cave_change = CodeCaveChangeRecord{};',
        'g_refresh_disassembly_after_reset = true;',
        'g_forget_debugger_changes_on_next_debugger_draw = false;',
        'if (g_refresh_disassembly_after_reset &&',
        'qemu_reset_requested_get() == SHUTDOWN_CAUSE_NONE)',
        'g_refresh_disassembly_after_reset = false;',
        'm_inject_disasm_refresh_pending = true;',
    ):
        require(draw_debugger, token, f"Reset Changes/deferred refresh {token}")
    require(memory, '#include "system/runstate.h"', "QEMU reset-state dependency")

    forget_block_start = draw_debugger.find('if (g_forget_debugger_changes_on_next_debugger_draw)')
    deferred_start = draw_debugger.find('if (g_refresh_disassembly_after_reset &&')
    refresh_queue = draw_debugger.find('m_inject_disasm_refresh_pending = true;', deferred_start)
    refresh_consume = draw_debugger.find('if (m_inject_disasm_refresh_pending)')
    if min(forget_block_start, deferred_start, refresh_queue, refresh_consume) < 0:
        raise AssertionError('Reset deferred refresh lifecycle is incomplete')
    if not (forget_block_start < deferred_start < refresh_queue < refresh_consume):
        raise AssertionError('Reset must clear Changes, wait for reset completion, then queue the existing refresh consumer')

    forget_block = draw_debugger[forget_block_start:deferred_start]
    if 'm_inject_disasm_refresh_pending = true;' in forget_block:
        raise AssertionError('v2.88.3 immediate reset refresh returned; refresh must remain deferred')
    for forbidden in ('Write(', 'xemu_cheat_patch', 'RemoveDebuggerF0', 'm_preentry'):
        if forbidden in forget_block:
            raise AssertionError(f"Reset Changes cleanup must not touch guest/PREENTRY state: {forbidden}")

    # v2.88.2 CodeCave RUN refuses any overwrite span overlapping the live XBE
    # header. The range comes from the active XBE m_base/m_sizeof_headers and
    # is therefore not hard-coded to the common 0x10000-0x11FFF case.
    require(inject_ui, '#include "xemu-xbe.h"', "live XBE header dependency")
    cave = extract_function(inject_ui, "void MemoryToolsWindow::DrawCodeCaveBuilder()")
    for token in (
        'struct xbe *xbe = xemu_get_xbe_info();',
        'xbe->header->m_base',
        'xbe->header->m_sizeof_headers',
        'header_size != 0 && hook_start < header_end && header_start < hook_end',
        'CodeCave RUN blocked: hook overlaps active XBE header',
    ):
        require(cave, token, f"XBE-header CodeCave guard {token}")
    if cave.index('if (overlaps_xbe_headers)') > cave.index('cheat_engine_window.InstallDebuggerF0('):
        raise AssertionError("XBE-header overlap must be rejected before F0 installation")

    require(bridge, 'const bool was_running = runstate_is_running()',
            "running-state preservation")
    require(bridge, 'vm_stop(RUN_STATE_PAUSED)', "transactional patch pause")
    require(bridge, 'if (was_running) {\n        vm_start();',
            "transactional patch resume")
    require(bridge, 'cpu_synchronize_state(cpu);',
            "executable patch current-CR3 synchronization")
    require(bridge, '(void)xemu_dbg_flush_guest_translation();',
            "executable patch accelerator translation synchronization")
    require(bridge, 'xemu_cheat_notify_code_patch();',
            "executable patch generation notification")
    require(bridge_h, 'uint64_t xemu_cheat_code_patch_generation(void);',
            "code patch generation bridge declaration")
    require(memory, 'code_patch_generation != m_code_patch_generation',
            "debugger executable-patch generation tracking")
    require(memory, 'refresh_disassembly = true;',
            "automatic disassembly invalidation")
    if external.count('xemu_cheat_notify_code_patch();') < 2:
        raise AssertionError("external cave write/free paths do not notify debugger refresh")

    require(backend, '#include "exec/tb-flush.h"',
            "TCG translation-block flush API")
    require(backend, 'if (!xemu_dbg_pause_for_accel_update(&was_running))',
            "TCG TB flush serial-context guard")
    require(backend, 'tlb_flush(cpu);',
            "TCG software TLB invalidation")
    require(backend, 'tb_flush__exclusive_or_serial();',
            "TCG translated-code invalidation")
    tcg_flush = backend.find('if (tcg_enabled()) {',
                             backend.find('int xemu_dbg_flush_guest_translation(void)'))
    if tcg_flush < 0:
        raise AssertionError("TCG executable-patch synchronization block missing")
    tcg_end = backend.find('return 1;', tcg_flush)
    if tcg_end < 0 or backend.find('tlb_flush(cpu);', tcg_flush, tcg_end) < 0 or \
            backend.find('tb_flush__exclusive_or_serial();', tcg_flush, tcg_end) < 0:
        raise AssertionError("TCG synchronization must flush both TLB and translated code")

    require(engine, 'InstallFHook(kDebuggerFHookOwner, kDebuggerFHookKey',
            "shared Type-F hook installer")
    require(engine, 'ActiveFHookOwnsAddress', "active cave ownership guard")
    require(engine, 'xemu_cheat_patch_virtual(hook_address, hook,',
            "F0 hook synchronized executable write")
    require(engine, 'xemu_cheat_patch_virtual(state.hook_address,',
            "F0 restore synchronized executable write")

    require(assembler, 'target.kind == OperandKind::Label || target.kind == OperandKind::Imm',
            "JMP/CALL direct-address targets")
    require(assembler, 'target must be an address or label',
            "Jcc/LOOP direct-address targets")

    print("PASS: x86 Debugger Inject invariants")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
