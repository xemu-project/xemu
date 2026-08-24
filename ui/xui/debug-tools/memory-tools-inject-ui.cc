//
// xemu Memory Viewer / Search / x86 Debugger - Inject UI ownership
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
#include "xemu-xbe.h"
#include "cheat-engine.hh"
#include "../font-manager.hh"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <string>

using namespace xemu_memory_tools_internal;

void MemoryToolsWindow::DrawInstructionChanger()
{
    if (!m_change_instruction_open) {
        return;
    }

    if (m_change_instruction_focus_requested) {
        ImGui::SetNextWindowFocus();
        ImGui::SetNextWindowSize(ImVec2(650.0f, 390.0f), ImGuiCond_Appearing);
        m_change_instruction_focus_requested = false;
    }

    if (!ImGui::Begin("x86 Change Instruction", &m_change_instruction_open,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    ImGui::Text("Address: %08X", m_change_instruction_address);
    ImGui::SameLine();
    ImGui::TextDisabled("[%u byte%s]", m_change_instruction_span,
                        m_change_instruction_span == 1 ? "" : "s");

    char original_bytes[64];
    format_disassembly_bytes(original_bytes, sizeof(original_bytes),
                             m_change_instruction_original_bytes,
                             m_change_instruction_span);
    ImGui::TextUnformatted("Original instruction (remembered)");
    if (!m_detached_rendering) {
        ImGui::PushFont(g_font_mgr.m_fixed_width_font);
    }
    ImGui::Text("%-38s %s", original_bytes,
                m_change_instruction_original_text.c_str());
    if (!m_detached_rendering) {
        ImGui::PopFont();
    }

    char current_bytes[64] = {};
    if (!m_change_instruction_current_bytes.empty()) {
        format_disassembly_bytes(current_bytes, sizeof(current_bytes),
                                 m_change_instruction_current_bytes.data(),
                                 m_change_instruction_current_bytes.size());
    }
    ImGui::TextUnformatted("Current instruction");
    if (!m_detached_rendering) {
        ImGui::PushFont(g_font_mgr.m_fixed_width_font);
    }
    ImGui::Text("%-38s %s", current_bytes,
                m_change_instruction_current_text.c_str());
    if (!m_detached_rendering) {
        ImGui::PopFont();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Replacement instruction");
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputText("##change_instruction_source",
                         &m_change_instruction_source,
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        BuildInstructionChangePreview();
    }
    if (ImGui::IsItemEdited()) {
        BuildInstructionChangePreview();
    }

    ImGui::TextUnformatted("Replacement bytes");
    if (m_change_instruction_preview_valid) {
        char preview_bytes[64];
        format_disassembly_bytes(preview_bytes, sizeof(preview_bytes),
                                 m_change_instruction_preview_bytes.data(),
                                 m_change_instruction_preview_bytes.size());
        if (!m_detached_rendering) {
            ImGui::PushFont(g_font_mgr.m_fixed_width_font);
        }
        ImGui::TextUnformatted(preview_bytes);
        if (!m_detached_rendering) {
            ImGui::PopFont();
        }
    } else {
        ImGui::TextDisabled("No valid replacement preview.");
    }

    ImGui::Separator();
    ImGui::BeginDisabled(!m_change_instruction_preview_valid);
    if (ImGui::Button("APPLY", ImVec2(90.0f, 0.0f))) {
        ApplyInstructionChange();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("RESTORE", ImVec2(90.0f, 0.0f))) {
        RestoreInstructionChange();
    }

    if (!m_change_instruction_status.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", m_change_instruction_status.c_str());
    }

    ImGui::End();
}

void MemoryToolsWindow::DrawCodeCaveBuilder()
{
    if (!m_code_cave_builder_open) {
        return;
    }

    if (m_code_cave_builder_focus_requested) {
        ImGui::SetNextWindowFocus();
        ImGui::SetNextWindowSize(ImVec2(680.0f, 570.0f), ImGuiCond_Appearing);
        m_code_cave_builder_focus_requested = false;
    }

    if (!ImGui::Begin("x86 Code Cave Builder", &m_code_cave_builder_open,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    CheatEngineWindow::DebuggerF0HookInfo active_info;
    const bool have_debugger_hook =
        cheat_engine_window.GetDebuggerF0HookInfo(active_info) &&
        active_info.installed;
    const bool active_here = have_debugger_hook &&
                             active_info.hook_address == m_code_cave_hook_address;

    ImGui::Text("Hook: %08X", m_code_cave_hook_address);
    ImGui::SameLine();
    if (active_here) {
        ImGui::TextDisabled("[RUNNING -> %08X]", active_info.cave_address);
    } else if (have_debugger_hook) {
        ImGui::TextDisabled("[another debugger cave is running at %08X]",
                            active_info.hook_address);
    } else {
        ImGui::TextDisabled("[not running]");
    }

    ImGui::TextWrapped(
        "RUN uses the normal Type-F0 engine. The selected hook is replaced by a JMP, "
        "whole x86 instructions are overwritten until at least 5 bytes are available, "
        "and DEADCODE jumps back immediately after that span.");

    ImGui::Separator();
    ImGui::TextUnformatted("F0 Source");
    ImGui::InputTextMultiline("##debugger_code_cave_source", &m_code_cave_source,
                              ImVec2(-FLT_MIN, 225.0f),
                              ImGuiInputTextFlags_AllowTabInput);

    ImGui::Separator();
    ImGui::TextUnformatted("Disassembly Preview");
    if (m_code_cave_original_rows.empty() || m_code_cave_overwrite_length < 5u) {
        ImGui::TextDisabled("No valid hook preview is available.");
    } else {
        ImGui::Text("Current instructions replaced by RUN (%u bytes):",
                    m_code_cave_overwrite_length);
        if (!m_detached_rendering) {
            ImGui::PushFont(g_font_mgr.m_fixed_width_font);
        }
        for (const XemuCheatDisasmRow &row : m_code_cave_original_rows) {
            char bytes[64];
            format_disassembly_bytes(bytes, sizeof(bytes), row.bytes,
                                     std::min<size_t>(row.size, sizeof(row.bytes)));
            ImGui::Text("%08X  %-32s %-8s %s", row.virtual_address, bytes,
                        row.mnemonic, row.operands);
        }
        if (!m_detached_rendering) {
            ImGui::PopFont();
        }

        ImGui::Spacing();
        ImGui::TextUnformatted("Hook site after RUN:");
        if (active_here) {
            uint8_t hook[32];
            std::memset(hook, 0x90, active_info.overwrite_length);
            hook[0] = 0xE9;
            const uint32_t rel = active_info.cave_address -
                                 (active_info.hook_address + 5u);
            hook[1] = (uint8_t)(rel & 0xFFu);
            hook[2] = (uint8_t)((rel >> 8) & 0xFFu);
            hook[3] = (uint8_t)((rel >> 16) & 0xFFu);
            hook[4] = (uint8_t)((rel >> 24) & 0xFFu);
            char bytes[128];
            format_disassembly_bytes(bytes, sizeof(bytes), hook,
                                     active_info.overwrite_length);
            ImGui::Text("%08X  %-32s JMP %08X", active_info.hook_address,
                        bytes, active_info.cave_address);
            ImGui::Text("Cave returns to %08X", active_info.return_address);
        } else {
            ImGui::Text("%08X  E9 ?? ?? ?? ?? + NOP padding  JMP <allocated cave>",
                        m_code_cave_hook_address);
            ImGui::Text("Cave returns to %08X",
                        m_code_cave_hook_address + m_code_cave_overwrite_length);
        }
    }

    ImGui::Separator();
    const bool owned_by_other_hook =
        cheat_engine_window.ActiveFHookOwnsAddress(m_code_cave_hook_address) &&
        !active_here;
    if (owned_by_other_hook) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("RUN", ImVec2(90.0f, 0.0f))) {
        struct xbe *xbe = xemu_get_xbe_info();
        const uint32_t header_base = xbe && xbe->header ? xbe->header->m_base : 0u;
        const uint32_t header_size = xbe && xbe->header ? xbe->header->m_sizeof_headers : 0u;
        const uint64_t hook_start = m_code_cave_hook_address;
        const uint64_t hook_end = hook_start + m_code_cave_overwrite_length;
        const uint64_t header_start = header_base;
        const uint64_t header_end = header_start + header_size;
        const bool overlaps_xbe_headers =
            header_size != 0 && hook_start < header_end && header_start < hook_end;
        if (overlaps_xbe_headers) {
            char range[96];
            std::snprintf(range, sizeof(range),
                          "CodeCave RUN blocked: hook overlaps active XBE header %08X-%08llX.",
                          header_base, (unsigned long long)(header_end - 1u));
            m_code_cave_status = range;
            m_debug_status = m_code_cave_status;
        } else {
            CheatEngineWindow::DebuggerF0HookInfo installed;
            if (cheat_engine_window.InstallDebuggerF0(
                    m_code_cave_hook_address, m_code_cave_source,
                    installed, m_code_cave_status)) {
                RecordCodeCaveChange(installed.hook_address, installed.overwrite_length,
                                     installed.cave_address);
                m_debug_status = m_code_cave_status;
                FollowDebuggerAddress(m_code_cave_hook_address, false);
                m_inject_disasm_refresh_pending = true;
            }
        }
    }
    if (owned_by_other_hook) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (!have_debugger_hook) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("RESTORE", ImVec2(90.0f, 0.0f))) {
        if (cheat_engine_window.RemoveDebuggerF0(m_code_cave_status)) {
            ClearCodeCaveChange(m_code_cave_hook_address);
            m_debug_status = m_code_cave_status;
            FollowDebuggerAddress(m_code_cave_hook_address, false);
            m_inject_disasm_refresh_pending = true;
        }
    }
    if (!have_debugger_hook) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (ImGui::Button("RESET TEMPLATE")) {
        BuildCodeCaveTemplate(m_code_cave_hook_address);
    }

    if (!m_code_cave_status.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", m_code_cave_status.c_str());
    }

    ImGui::End();
}

void MemoryToolsWindow::DrawAddressContextMenu(
    AddressSpace space, uint32_t address, ContextOrigin origin,
    const char *copy_value, const XemuCheatDisasmRow *disasm_row,
    bool address_valid, bool have_breakpoint_virtual_override,
    uint32_t breakpoint_virtual_override)
{
    if (!ImGui::BeginPopupContextItem()) {
        return;
    }

    /* A debugger context-click is also a navigation selection. This makes
     * Follow -> Back return to the instruction whose menu the user opened,
     * even when it was not left-click selected first. */
    if (origin == ContextOrigin::Debugger && disasm_row != nullptr) {
        m_have_disasm_selection = true;
        m_selected_disasm_virtual = disasm_row->virtual_address;
        m_selected_disasm_physical_valid = disasm_row->physical_valid != 0;
        if (m_selected_disasm_physical_valid) {
            m_selected_disasm_physical = disasm_row->physical_address;
        }
    }

    if (address_valid) {
        ImGui::TextDisabled("%s %08X",
                            space == AddressSpace::Virtual ? "Virtual" : "Physical",
                            address);
    } else {
        ImGui::TextDisabled("Physical address unmapped");
    }
    ImGui::Separator();

    if (ImGui::BeginMenu("Dump Ram")) {
        if (ImGui::MenuItem("Current Page", nullptr, false, address_valid)) {
            DumpCurrentPage(space, address);
            SetContextStatus(origin, space, m_dump_status);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("DUMP PHYSICAL")) {
            DumpPhysicalRam();
            SetContextStatus(origin, space, m_dump_status);
        }
        if (ImGui::MenuItem("DUMP MAPPED VIRTUAL RAM")) {
            DumpMappedVirtualRam();
            SetContextStatus(origin, space, m_dump_status);
        }
        if (ImGui::MenuItem("DUMP PHYSICAL + DUMP MAPPED VIRTUAL RAM")) {
            DumpFullRam();
            SetContextStatus(origin, space, m_dump_status);
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Copy")) {
        if (ImGui::MenuItem("Address", nullptr, false, address_valid)) {
            char text[16];
            std::snprintf(text, sizeof(text), "%08X", address);
            ImGui::SetClipboardText(text);
            SetContextStatus(origin, space,
                             std::string("Copied address ") + text);
        }

        std::string value_text;
        bool have_value = false;
        if (copy_value != nullptr && copy_value[0] != '\0') {
            value_text = copy_value;
            have_value = true;
        } else if (disasm_row != nullptr && disasm_row->size != 0) {
            char byte_text[64] = {};
            size_t used = 0;
            for (size_t i = 0;
                 i < disasm_row->size && i < sizeof(disasm_row->bytes); ++i) {
                const int wrote = std::snprintf(
                    byte_text + used, sizeof(byte_text) - used,
                    i == 0 ? "%02X" : " %02X", disasm_row->bytes[i]);
                if (wrote <= 0 || (size_t)wrote >= sizeof(byte_text) - used) {
                    break;
                }
                used += (size_t)wrote;
            }
            value_text = byte_text;
            have_value = !value_text.empty();
        } else if (address_valid) {
            uint8_t value = 0;
            if (Read(space, address, &value, sizeof(value))) {
                char byte_text[4];
                std::snprintf(byte_text, sizeof(byte_text), "%02X", value);
                value_text = byte_text;
                have_value = true;
            }
        }

        if (ImGui::MenuItem("Value", nullptr, false, have_value)) {
            ImGui::SetClipboardText(value_text.c_str());
            SetContextStatus(origin, space, "Copied value");
        }

        const bool can_disassemble =
            disasm_row != nullptr || address_valid ||
            have_breakpoint_virtual_override;
        if (ImGui::MenuItem("x86 Instructions", nullptr, false,
                            can_disassemble)) {
            if (CopyContextInstruction(space, address, disasm_row,
                                       have_breakpoint_virtual_override,
                                       breakpoint_virtual_override)) {
                SetContextStatus(origin, space, "Copied x86 instruction");
            } else {
                SetContextStatus(origin, space,
                                 "Could not disassemble/copy x86 instruction");
            }
        }
        ImGui::EndMenu();
    }

    if (origin == ContextOrigin::Debugger && disasm_row != nullptr &&
        ImGui::BeginMenu("Inject")) {
        CheatEngineWindow::DebuggerF0HookInfo debugger_hook;
        const bool debugger_hook_here =
            cheat_engine_window.GetDebuggerF0HookInfo(debugger_hook) &&
            debugger_hook.installed &&
            debugger_hook.hook_address == disasm_row->virtual_address;
        const bool hook_owned =
            cheat_engine_window.ActiveFHookOwnsAddress(disasm_row->virtual_address);
        const bool can_inject = disasm_row->size > 0 &&
                                (!hook_owned || debugger_hook_here);

        bool restorable_instruction_patch = false;
        bool tracked_patch_starts_here = false;
        for (const InstructionChangeRecord &record : m_instruction_change_history) {
            const uint64_t start = record.address;
            const uint64_t end = start + record.span;
            if (record.active && disasm_row->virtual_address >= start &&
                disasm_row->virtual_address < end) {
                restorable_instruction_patch = true;
                tracked_patch_starts_here =
                    disasm_row->virtual_address == record.address;
                break;
            }
        }
        const bool tracked_patch_interior =
            restorable_instruction_patch && !tracked_patch_starts_here;

        if (ImGui::MenuItem("NOP", nullptr, false,
                            disasm_row->size > 0 && !hook_owned &&
                            !tracked_patch_interior)) {
            InjectNop(*disasm_row);
        }
        if (ImGui::MenuItem("Change", nullptr, false,
                            disasm_row->size > 0 && !hook_owned &&
                            !tracked_patch_interior)) {
            OpenInstructionChanger(*disasm_row);
        }
        if (restorable_instruction_patch && ImGui::MenuItem("Restore")) {
            RestoreTrackedInstructionPatch(disasm_row->virtual_address);
        }

        if (ImGui::MenuItem("CodeCave", nullptr, false, can_inject)) {
            OpenCodeCaveBuilder(*disasm_row);
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Break Point")) {
        const bool can_break = address_valid || have_breakpoint_virtual_override;
        const char *labels[] = {"Exe", "Read", "Write", "Read/Write"};
        for (int kind = 0; kind < 4; ++kind) {
            if (ImGui::MenuItem(labels[kind], nullptr, false, can_break)) {
                uint32_t virtual_address = 0;
                if (ResolveContextVirtualAddress(
                        space, address, have_breakpoint_virtual_override,
                        breakpoint_virtual_override, virtual_address)) {
                    AddBreakpointByKind(virtual_address, kind);
                }
            }
        }
        ImGui::EndMenu();
    }

    if (origin == ContextOrigin::Debugger && disasm_row != nullptr) {
        DebugFlowInfo flow;
        const bool have_flow = AnalyzeControlFlow(*disasm_row, flow);
        uint32_t resolved_target = 0;
        const bool target_resolved =
            have_flow && ResolveControlFlowTarget(*disasm_row, resolved_target);

        if (ImGui::BeginMenu("Follow")) {
            const bool branch_or_call =
                have_flow && flow.kind != DebugFlowKind::Return;
            if (ImGui::MenuItem("Branch / Call Target", nullptr, false,
                                branch_or_call && target_resolved)) {
                m_debug_nav_pending_action = 1;
                m_debug_nav_pending_address = resolved_target;
                m_debug_nav_pending_status = "Followed branch/call target";
            }
            if (ImGui::MenuItem("Fall Through", nullptr, false,
                                have_flow && flow.fallthrough_valid)) {
                m_debug_nav_pending_action = 1;
                m_debug_nav_pending_address = flow.fallthrough;
                m_debug_nav_pending_status = "Followed fall-through";
            }
            if (ImGui::MenuItem("Return Target", nullptr, false,
                                have_flow &&
                                    flow.kind == DebugFlowKind::Return &&
                                    target_resolved)) {
                m_debug_nav_pending_action = 1;
                m_debug_nav_pending_address = resolved_target;
                m_debug_nav_pending_status = "Followed return target";
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Navigation")) {
            const bool can_back = m_have_debug_nav_history &&
                                  !m_debug_nav_history.empty() &&
                                  m_debug_nav_index > 0;
            const bool can_forward = m_have_debug_nav_history &&
                                     m_debug_nav_index + 1 <
                                         m_debug_nav_history.size();
            if (ImGui::MenuItem("Back", "Left", false, can_back)) {
                m_debug_nav_pending_action = 2;
                m_debug_nav_pending_status = "Debugger navigation: Back";
            }
            if (ImGui::MenuItem("Forward", "Alt+Right", false,
                                can_forward)) {
                m_debug_nav_pending_action = 3;
                m_debug_nav_pending_status = "Debugger navigation: Forward";
            }
            ImGui::EndMenu();
        }
    }

    if (ImGui::BeginMenu("View In")) {
        if (origin == ContextOrigin::Memory ||
            origin == ContextOrigin::Search) {
            if (ImGui::MenuItem("x86 Debugger")) {
                uint32_t virtual_address = 0;
                if (ResolveContextVirtualAddress(
                        space, address, have_breakpoint_virtual_override,
                        breakpoint_virtual_override, virtual_address)) {
                    NavigateDebuggerAddress(virtual_address);
                    m_request_debugger_tab = true;
                }
            }
        }

        if (origin == ContextOrigin::Search ||
            origin == ContextOrigin::Debugger) {
            const bool can_view_memory =
                address_valid || have_breakpoint_virtual_override;
            if (ImGui::MenuItem("Memory", nullptr, false, can_view_memory)) {
                AddressSpace target_space = space;
                uint32_t target_address = address;
                if (!address_valid && have_breakpoint_virtual_override) {
                    target_space = AddressSpace::Virtual;
                    target_address = breakpoint_virtual_override;
                }
                JumpViewerTo(target_space, target_address);
                SelectMemoryByte(target_space, target_address);
                m_request_memory_tab = true;
            }
        }
        ImGui::EndMenu();
    }

    ImGui::EndPopup();
}
