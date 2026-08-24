//
// xemu Memory Viewer / Search / x86 Debugger
//
// Copyright (C) 2026 xemu contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#include "memory-tools.hh"
#include "memory-tools-internal.hh"
#include "current-game.hh"
#include "cheat-engine-memory.h"
#include "cheat-engine.hh"
#include "../font-manager.hh"
#include "../misc.hh"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <utility>

#include <glib.h>
#include <glib/gstdio.h>

using namespace xemu_memory_tools_internal;

bool MemoryToolsWindow::InjectNop(const XemuCheatDisasmRow &row)
{
    if (row.size == 0 || row.size > sizeof(row.bytes)) {
        m_debug_status = "Inject NOP: selected instruction has an invalid size";
        return false;
    }

    size_t record_index = (size_t)-1;
    for (size_t i = 0; i < m_instruction_change_history.size(); ++i) {
        InstructionChangeRecord &candidate = m_instruction_change_history[i];
        const uint64_t start = candidate.address;
        const uint64_t end = start + candidate.span;
        if (candidate.active && row.virtual_address > start &&
            row.virtual_address < end) {
            m_debug_status =
                "Inject NOP: selected address is inside an active NOP/Change patch; restore the tracked patch first";
            return false;
        }
        if (candidate.address == row.virtual_address) {
            record_index = i;
            break;
        }
    }

    if (record_index == (size_t)-1) {
        InstructionChangeRecord record;
        record.address = row.virtual_address;
        record.span = row.size;
        std::memcpy(record.original_bytes, row.bytes, row.size);
        record.original_text = row.mnemonic;
        if (row.operands[0] != '\0') {
            if (!record.original_text.empty()) {
                record.original_text += " ";
            }
            record.original_text += row.operands;
        }
        m_instruction_change_history.push_back(std::move(record));
        record_index = m_instruction_change_history.size() - 1;
    }

    InstructionChangeRecord &record = m_instruction_change_history[record_index];
    if (record.span == 0 || record.span > sizeof(record.original_bytes)) {
        m_debug_status = "Inject NOP: saved original instruction state is invalid";
        return false;
    }

    if (record.active) {
        if (record.last_applied_bytes.size() != record.span) {
            m_debug_status = "Inject NOP: tracked patch state is invalid";
            return false;
        }
        uint8_t current[15] = {};
        if (!Read(AddressSpace::Virtual, record.address, current, record.span)) {
            m_debug_status = "Inject NOP: failed to read the current guest instruction bytes";
            return false;
        }
        if (std::memcmp(current, record.last_applied_bytes.data(), record.span) != 0) {
            m_debug_status =
                "Inject NOP: guest bytes no longer match the tracked NOP/Change patch; restore is safety-blocked";
            return false;
        }
    } else {
        // An inactive history entry can belong to an older live instruction at
        // the same virtual address. Re-baseline from the currently selected row
        // before creating a fresh NOP patch.
        record.span = row.size;
        std::memcpy(record.original_bytes, row.bytes, row.size);
        record.original_text = row.mnemonic;
        if (row.operands[0] != '\0') {
            if (!record.original_text.empty()) {
                record.original_text += " ";
            }
            record.original_text += row.operands;
        }
    }

    for (uint32_t i = 0; i < record.span; ++i) {
        if (cheat_engine_window.ActiveFHookOwnsAddress(record.address + i)) {
            m_debug_status =
                "Inject NOP: address is owned by an active Type-F hook/cave; restore it first";
            return false;
        }
    }

    std::vector<uint8_t> nops(record.span, 0x90);
    if (!xemu_cheat_patch_virtual(record.address, nops.data(), nops.size())) {
        m_debug_status = "Inject NOP: failed to patch guest instruction bytes";
        return false;
    }

    record.last_applied_bytes = nops;
    record.last_applied_text.clear();
    for (uint32_t i = 0; i < record.span; ++i) {
        if (!record.last_applied_text.empty()) record.last_applied_text += "; ";
        record.last_applied_text += "nop";
    }
    record.active = true;

    char address[16];
    std::snprintf(address, sizeof(address), "%08X", record.address);
    m_debug_status = std::string("Injected ") + std::to_string(record.span) +
                     " NOP byte" + (record.span == 1 ? "" : "s") +
                     " at " + address + ". Inject > Restore can restore the saved original instruction.";
    FollowDebuggerAddress(record.address, false);
    m_inject_disasm_refresh_pending = true;
    return true;
}

void MemoryToolsWindow::OpenInstructionChanger(const XemuCheatDisasmRow &row)
{
    if (row.size == 0 || row.size > sizeof(row.bytes) ||
        row.size > sizeof(m_change_instruction_original_bytes)) {
        m_debug_status = "Inject Change: selected instruction has an invalid size";
        return;
    }

    size_t record_index = (size_t)-1;
    for (size_t i = 0; i < m_instruction_change_history.size(); ++i) {
        if (m_instruction_change_history[i].address == row.virtual_address) {
            record_index = i;
            break;
        }
    }

    if (record_index == (size_t)-1) {
        InstructionChangeRecord record;
        record.address = row.virtual_address;
        record.span = row.size;
        std::memcpy(record.original_bytes, row.bytes, row.size);
        record.original_text = row.mnemonic;
        if (row.operands[0] != '\0') {
            if (!record.original_text.empty()) {
                record.original_text += " ";
            }
            record.original_text += row.operands;
        }
        m_instruction_change_history.push_back(std::move(record));
        record_index = m_instruction_change_history.size() - 1;
    }

    InstructionChangeRecord &record = m_instruction_change_history[record_index];
    if (record.span == 0 || record.span > sizeof(m_change_instruction_original_bytes)) {
        m_debug_status = "Inject Change: saved original instruction state is invalid";
        return;
    }

    for (uint32_t i = 0; i < record.span; ++i) {
        if (cheat_engine_window.ActiveFHookOwnsAddress(record.address + i)) {
            m_debug_status =
                "Inject Change: selected instruction overlaps an active Type-F hook/cave; restore it first";
            return;
        }
    }

    uint8_t current[15] = {};
    if (!Read(AddressSpace::Virtual, record.address, current, record.span)) {
        m_debug_status = "Inject Change: failed to read the current guest instruction bytes";
        return;
    }

    bool matches_original =
        std::memcmp(current, record.original_bytes, record.span) == 0;
    bool matches_applied =
        record.active && record.last_applied_bytes.size() == record.span &&
        std::memcmp(current, record.last_applied_bytes.data(), record.span) == 0;

    if (matches_original) {
        record.active = false;
        record.last_applied_bytes.clear();
        record.last_applied_text.clear();
    } else if (!record.active) {
        // No Change patch is active, so a different live instruction means the
        // old history entry is stale (for example after loading another title
        // at the same virtual address). Re-baseline only when there is no
        // tracked patch to preserve/restore.
        record.span = row.size;
        std::memcpy(record.original_bytes, row.bytes, row.size);
        record.original_text = row.mnemonic;
        if (row.operands[0] != '\0') {
            if (!record.original_text.empty()) {
                record.original_text += " ";
            }
            record.original_text += row.operands;
        }
        std::memcpy(current, row.bytes, row.size);
        matches_original = true;
        matches_applied = false;
    }

    m_change_instruction_record_index = record_index;
    m_change_instruction_address = record.address;
    m_change_instruction_span = record.span;
    std::memcpy(m_change_instruction_original_bytes, record.original_bytes, record.span);
    m_change_instruction_original_text = record.original_text;
    m_change_instruction_current_bytes.assign(current, current + record.span);
    m_change_instruction_current_text = row.mnemonic;
    if (row.operands[0] != '\0') {
        if (!m_change_instruction_current_text.empty()) {
            m_change_instruction_current_text += " ";
        }
        m_change_instruction_current_text += row.operands;
    }
    m_change_instruction_source = m_change_instruction_current_text;
    m_change_instruction_preview_bytes.clear();
    m_change_instruction_applied_bytes = matches_applied ? record.last_applied_bytes
                                                         : std::vector<uint8_t>();
    m_change_instruction_preview_valid = false;
    m_change_instruction_applied = matches_applied;
    m_change_instruction_status.clear();
    if (record.active && !matches_applied) {
        m_change_instruction_status =
            "Change: the saved original is still remembered, but live bytes no longer match the last Change patch. Restore is blocked to avoid overwriting a newer/external patch.";
    } else if (matches_applied) {
        m_change_instruction_status =
            "Change: this address has an active saved patch. RESTORE will place the remembered original instruction into Replacement and apply its exact original bytes.";
    }
    m_change_instruction_open = true;
    m_change_instruction_focus_requested = true;
    BuildInstructionChangePreview();
    if (record.active && !matches_applied) {
        m_change_instruction_status +=
            " Saved original retained; live bytes differ from the last tracked Change patch, so APPLY/RESTORE are safety-blocked.";
    } else if (matches_applied) {
        m_change_instruction_status +=
            " Saved original is available; RESTORE remains available after reopening this window.";
    }
}

bool MemoryToolsWindow::BuildInstructionChangePreview()
{
    m_change_instruction_preview_valid = false;
    m_change_instruction_preview_bytes.clear();

    if (m_change_instruction_span == 0 ||
        m_change_instruction_span > sizeof(m_change_instruction_original_bytes)) {
        m_change_instruction_status = "Change: no valid instruction is selected.";
        return false;
    }
    if (m_change_instruction_source.empty()) {
        m_change_instruction_status = "Change: enter one replacement x86 instruction.";
        return false;
    }

    XemuCheatAsmResult assembled;
    if (!xemu_cheat_assemble_x86_32_change_instruction(
            m_change_instruction_source, m_change_instruction_address,
            m_change_instruction_span, assembled)) {
        m_change_instruction_status = "Change assembler error";
        if (assembled.error_line > 0) {
            m_change_instruction_status += " on line " +
                                           std::to_string(assembled.error_line);
        }
        if (!assembled.error.empty()) {
            m_change_instruction_status += ": " + assembled.error;
        }
        return false;
    }

    if (assembled.uses_preserve || assembled.uses_temp || !assembled.data.empty()) {
        m_change_instruction_status =
            "Change: PRESERVE/T-register/data directives are CodeCave features and cannot replace one instruction in place.";
        return false;
    }
    if (assembled.bytes.empty()) {
        m_change_instruction_status =
            "Change: the replacement did not assemble to an executable instruction.";
        return false;
    }
    if (assembled.bytes.size() > m_change_instruction_span) {
        m_change_instruction_status =
            "Change: replacement is " + std::to_string(assembled.bytes.size()) +
            " bytes but the selected instruction is only " +
            std::to_string(m_change_instruction_span) +
            " bytes. Use Inject > CodeCave for a longer replacement.";
        return false;
    }

    m_change_instruction_preview_bytes = assembled.bytes;
    m_change_instruction_preview_bytes.resize(m_change_instruction_span, 0x90);
    m_change_instruction_preview_valid = true;
    const size_t padding = m_change_instruction_span - assembled.bytes.size();
    m_change_instruction_status =
        "Preview ready: " + std::to_string(assembled.bytes.size()) +
        " replacement byte" + (assembled.bytes.size() == 1 ? "" : "s");
    if (padding != 0) {
        m_change_instruction_status += ", padded with " + std::to_string(padding) +
                                       " NOP byte" + (padding == 1 ? "" : "s");
    }
    m_change_instruction_status += ".";
    return true;
}

bool MemoryToolsWindow::ApplyInstructionChange()
{
    if (!m_change_instruction_preview_valid && !BuildInstructionChangePreview()) {
        return false;
    }

    for (uint32_t i = 0; i < m_change_instruction_span; ++i) {
        if (cheat_engine_window.ActiveFHookOwnsAddress(m_change_instruction_address + i)) {
            m_change_instruction_status =
                "Change: selected instruction overlaps an active Type-F hook/cave; restore it first.";
            return false;
        }
    }

    uint8_t current[15] = {};
    if (!Read(AddressSpace::Virtual, m_change_instruction_address, current,
              m_change_instruction_span)) {
        m_change_instruction_status = "Change: failed to read the current guest instruction bytes.";
        return false;
    }

    const uint8_t *expected = m_change_instruction_original_bytes;
    if (m_change_instruction_applied) {
        if (m_change_instruction_applied_bytes.size() != m_change_instruction_span) {
            m_change_instruction_status = "Change: internal applied-byte state is invalid.";
            return false;
        }
        expected = m_change_instruction_applied_bytes.data();
    }
    if (std::memcmp(current, expected, m_change_instruction_span) != 0) {
        m_change_instruction_status =
            "Change: guest bytes changed since this window captured them. Close/reopen Change on the live instruction before applying.";
        return false;
    }

    if (!xemu_cheat_patch_virtual(m_change_instruction_address,
                                  m_change_instruction_preview_bytes.data(),
                                  m_change_instruction_preview_bytes.size())) {
        m_change_instruction_status = "Change: failed to patch guest instruction bytes.";
        return false;
    }

    const bool restoring_original =
        m_change_instruction_preview_bytes.size() == m_change_instruction_span &&
        std::memcmp(m_change_instruction_preview_bytes.data(),
                    m_change_instruction_original_bytes,
                    m_change_instruction_span) == 0;

    m_change_instruction_current_bytes = m_change_instruction_preview_bytes;
    m_change_instruction_current_text = m_change_instruction_source;
    m_change_instruction_applied = !restoring_original;
    if (restoring_original) {
        m_change_instruction_applied_bytes.clear();
    } else {
        m_change_instruction_applied_bytes = m_change_instruction_preview_bytes;
    }

    if (m_change_instruction_record_index < m_instruction_change_history.size()) {
        InstructionChangeRecord &record =
            m_instruction_change_history[m_change_instruction_record_index];
        if (record.address == m_change_instruction_address &&
            record.span == m_change_instruction_span) {
            if (restoring_original) {
                record.last_applied_bytes.clear();
                record.last_applied_text.clear();
                record.active = false;
            } else {
                record.last_applied_bytes = m_change_instruction_preview_bytes;
                record.last_applied_text = m_change_instruction_source;
                record.active = true;
            }
        }
    }

    char address[16];
    std::snprintf(address, sizeof(address), "%08X", m_change_instruction_address);
    if (restoring_original) {
        m_change_instruction_status =
            std::string("Original instruction restored at ") + address + ".";
    } else {
        m_change_instruction_status = std::string("Change applied at ") + address + ".";
    }
    m_debug_status = m_change_instruction_status;
    FollowDebuggerAddress(m_change_instruction_address, false);
    m_inject_disasm_refresh_pending = true;
    return true;
}

bool MemoryToolsWindow::RestoreInstructionChange()
{
    if (m_change_instruction_span == 0 ||
        m_change_instruction_span > sizeof(m_change_instruction_original_bytes)) {
        m_change_instruction_status = "Change: no valid original instruction is available to restore.";
        return false;
    }

    // Keep the requested Restore behavior visible in the editor: put the
    // remembered original instruction back into Replacement, use the exact
    // remembered bytes as the preview, then pass through the same transactional
    // Apply path and safety checks as a normal Change.
    m_change_instruction_source = m_change_instruction_original_text;
    m_change_instruction_preview_bytes.assign(
        m_change_instruction_original_bytes,
        m_change_instruction_original_bytes + m_change_instruction_span);
    m_change_instruction_preview_valid = true;
    return ApplyInstructionChange();
}

bool MemoryToolsWindow::RestoreTrackedInstructionPatch(uint32_t address)
{
    size_t record_index = (size_t)-1;
    for (size_t i = 0; i < m_instruction_change_history.size(); ++i) {
        const InstructionChangeRecord &candidate = m_instruction_change_history[i];
        const uint64_t start = candidate.address;
        const uint64_t end = start + candidate.span;
        if (candidate.active && address >= start && address < end) {
            record_index = i;
            break;
        }
    }

    if (record_index == (size_t)-1) {
        m_debug_status = "Inject Restore: no tracked NOP/Change patch owns this address";
        return false;
    }

    InstructionChangeRecord &record = m_instruction_change_history[record_index];
    if (record.span == 0 || record.span > sizeof(record.original_bytes) ||
        record.last_applied_bytes.size() != record.span) {
        m_debug_status = "Inject Restore: tracked patch state is invalid";
        return false;
    }

    for (uint32_t i = 0; i < record.span; ++i) {
        if (cheat_engine_window.ActiveFHookOwnsAddress(record.address + i)) {
            m_debug_status =
                "Inject Restore: tracked NOP/Change patch overlaps an active Type-F hook/cave; restore the Type-F hook first";
            return false;
        }
    }

    uint8_t current[15] = {};
    if (!Read(AddressSpace::Virtual, record.address, current, record.span)) {
        m_debug_status = "Inject Restore: failed to read the current guest instruction bytes";
        return false;
    }
    if (std::memcmp(current, record.last_applied_bytes.data(), record.span) != 0) {
        m_debug_status =
            "Inject Restore: guest bytes no longer match the tracked NOP/Change patch; restore was blocked to avoid overwriting a newer/external patch";
        return false;
    }

    if (!xemu_cheat_patch_virtual(record.address, record.original_bytes,
                                  record.span)) {
        m_debug_status = "Inject Restore: failed to restore the original instruction bytes";
        return false;
    }

    record.last_applied_bytes.clear();
    record.last_applied_text.clear();
    record.active = false;

    if (m_change_instruction_record_index == record_index &&
        m_change_instruction_address == record.address) {
        m_change_instruction_source = record.original_text;
        m_change_instruction_current_text = record.original_text;
        m_change_instruction_current_bytes.assign(record.original_bytes,
                                                  record.original_bytes + record.span);
        m_change_instruction_preview_bytes.assign(record.original_bytes,
                                                  record.original_bytes + record.span);
        m_change_instruction_preview_valid = true;
        m_change_instruction_applied_bytes.clear();
        m_change_instruction_applied = false;
        m_change_instruction_status = "Original instruction restored from Inject > Restore.";
    }

    char restored_address[16];
    std::snprintf(restored_address, sizeof(restored_address), "%08X", record.address);
    m_debug_status = std::string("Original instruction restored at ") +
                     restored_address + ".";
    FollowDebuggerAddress(record.address, false);
    m_inject_disasm_refresh_pending = true;
    return true;
}

void MemoryToolsWindow::RecordCodeCaveChange(
    uint32_t hook_address, uint32_t overwrite_length, uint32_t cave_address)
{
    if (overwrite_length < 5u || overwrite_length > 32u) return;
    const bool preserve_hex = m_code_cave_change.address == hook_address &&
                              m_code_cave_change.display_hex;
    CodeCaveChangeRecord record;
    record.address = hook_address;
    record.display_hex = preserve_hex;
    size_t remaining = overwrite_length;
    for (const XemuCheatDisasmRow &row : m_code_cave_original_rows) {
        if (remaining == 0) break;
        const size_t take = std::min<size_t>(row.size, remaining);
        record.original_bytes.insert(record.original_bytes.end(), row.bytes,
                                     row.bytes + take);
        if (!record.original_text.empty()) record.original_text += "; ";
        record.original_text += row.mnemonic;
        if (row.operands[0] != '\0') {
            record.original_text += " ";
            record.original_text += row.operands;
        }
        remaining -= take;
    }
    if (record.original_bytes.size() != overwrite_length) return;
    record.changed_bytes.resize(overwrite_length);
    if (!Read(AddressSpace::Virtual, hook_address, record.changed_bytes.data(),
              record.changed_bytes.size())) return;
    char cave_text[32];
    std::snprintf(cave_text, sizeof(cave_text), "jmp %08X", cave_address);
    record.changed_text = cave_text;
    for (uint32_t i = 5; i < overwrite_length; ++i) record.changed_text += "; nop";
    record.active = true;
    m_code_cave_change = std::move(record);
}

void MemoryToolsWindow::ClearCodeCaveChange(uint32_t hook_address)
{
    if (m_code_cave_change.active && m_code_cave_change.address == hook_address) {
        m_code_cave_change.active = false;
        m_code_cave_change.changed_bytes.clear();
        m_code_cave_change.changed_text.clear();
    }
}

bool MemoryToolsWindow::BuildCodeCaveTemplate(uint32_t hook_address)
{
    XemuCheatDisasmRow rows[16] = {};
    size_t row_count = 0;
    m_code_cave_original_rows.clear();
    m_code_cave_overwrite_length = 0;

    if (xemu_cheat_disassemble_paired(hook_address,
                                      (int)(sizeof(rows) / sizeof(rows[0])),
                                      rows, sizeof(rows) / sizeof(rows[0]),
                                      &row_count) != XEMU_CHEAT_DISAS_OK ||
        row_count == 0) {
        m_code_cave_status =
            "Code Cave: could not disassemble the selected virtual hook address.";
        return false;
    }

    for (size_t i = 0; i < row_count && m_code_cave_overwrite_length < 5u; ++i) {
        if (rows[i].size == 0 || rows[i].size > sizeof(rows[i].bytes) ||
            m_code_cave_overwrite_length + rows[i].size > 32u) {
            break;
        }
        m_code_cave_original_rows.push_back(rows[i]);
        m_code_cave_overwrite_length += rows[i].size;
    }
    if (m_code_cave_overwrite_length < 5u ||
        m_code_cave_overwrite_length > 32u) {
        m_code_cave_status =
            "Code Cave: could not find a complete 5-32 byte instruction span for the JMP hook.";
        m_code_cave_original_rows.clear();
        m_code_cave_overwrite_length = 0;
        return false;
    }

    char header[64];
    std::snprintf(header, sizeof(header), "$F0000000 %08X\n", hook_address);
    m_code_cave_source = header;
    m_code_cave_source +=
        "$// Auto-copied instructions that the 5-byte JMP will replace.\n"
        "$// Edit/add your cave code, but preserve any original behavior you still need.\n";

    bool copied_control_flow = false;
    for (const XemuCheatDisasmRow &row : m_code_cave_original_rows) {
        DebugFlowInfo flow;
        copied_control_flow |= AnalyzeControlFlow(row, flow);
        m_code_cave_source += "$";
        m_code_cave_source += row.mnemonic;
        if (row.operands[0] != '\0') {
            m_code_cave_source += " ";
            m_code_cave_source += row.operands;
        }
        m_code_cave_source += "\n";
    }
    m_code_cave_source += "$DEADCODE\n";

    m_code_cave_status =
        "Template ready: RUN will replace " +
        std::to_string(m_code_cave_overwrite_length) + " complete instruction bytes and return at ";
    char return_address[16];
    std::snprintf(return_address, sizeof(return_address), "%08X",
                  hook_address + m_code_cave_overwrite_length);
    m_code_cave_status += return_address;
    if (copied_control_flow) {
        m_code_cave_status +=
            ". WARNING: the copied span contains JMP/Jcc/CALL/RET/LOOP control flow; review it before RUN.";
    } else {
        m_code_cave_status += ".";
    }
    return true;
}

void MemoryToolsWindow::OpenCodeCaveBuilder(const XemuCheatDisasmRow &row)
{
    const bool new_hook = m_code_cave_hook_address != row.virtual_address;
    m_code_cave_hook_address = row.virtual_address;
    m_code_cave_builder_open = true;
    m_code_cave_builder_focus_requested = true;

    if (new_hook || m_code_cave_source.empty() ||
        m_code_cave_original_rows.empty()) {
        BuildCodeCaveTemplate(row.virtual_address);
    }
}

// Inject/context rendering UI is owned by memory-tools-inject-ui.cc.
