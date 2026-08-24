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

void MemoryToolsWindow::LoadDebuggerPreferences()
{
    if (m_debug_preferences_initialized) {
        return;
    }

    const auto &prefs = g_config.display.debug.memory_tools;
    m_disasm_pane_height = std::clamp((float)prefs.disassembly_pane_height,
                                      160.0f, 1200.0f);
    m_disasm_full_page = prefs.disassembly_full_page;
    m_disasm_instruction_count =
        std::clamp(prefs.disassembly_instruction_count, 1, 128);
    m_follow_eip = prefs.follow_eip;
    m_labels_enabled = prefs.labels_enabled;
    m_register_view = std::clamp(prefs.register_view, 0, 3);
    m_register_view_selection_pending = true;
    m_debug_preferences_initialized = true;

    /* Clamp legacy/manually edited config values back to the supported range
     * in memory. xemu's normal exit-time settings save will persist them; no
     * debugger frame performs settings-file I/O. */
    StoreDebuggerPreferences();
}

void MemoryToolsWindow::StoreDebuggerPreferences()
{
    if (!m_debug_preferences_initialized) {
        return;
    }

    /* This function runs at the end of every debugger frame. Keep xemu's
     * in-memory configuration synchronized, but avoid dirtying the same six
     * fields when the UI values are already identical. Disk persistence still
     * belongs exclusively to xemu's normal exit-time settings save. */
    auto &prefs = g_config.display.debug.memory_tools;
    if (prefs.disassembly_pane_height != m_disasm_pane_height) {
        prefs.disassembly_pane_height = m_disasm_pane_height;
    }
    if (prefs.disassembly_full_page != m_disasm_full_page) {
        prefs.disassembly_full_page = m_disasm_full_page;
    }
    if (prefs.disassembly_instruction_count != m_disasm_instruction_count) {
        prefs.disassembly_instruction_count = m_disasm_instruction_count;
    }
    if (prefs.follow_eip != m_follow_eip) {
        prefs.follow_eip = m_follow_eip;
    }
    if (prefs.labels_enabled != m_labels_enabled) {
        prefs.labels_enabled = m_labels_enabled;
    }
    if (prefs.register_view != m_register_view) {
        prefs.register_view = m_register_view;
    }
}

void MemoryToolsWindow::ResetDebuggerPreferences()
{
    /* Display preferences only. Breakpoints/watchpoints, navigation history,
     * Inject state, addresses, guest execution state, and Cheat/F0 state are
     * intentionally untouched. */
    m_disasm_pane_height = 320.0f;
    m_disasm_full_page = true;
    m_disasm_instruction_count = 32;
    m_follow_eip = true;
    m_labels_enabled = true;
    m_register_view = 0;
    m_register_view_selection_pending = true;
    StoreDebuggerPreferences();
}

bool MemoryToolsWindow::RefreshRegisters(XemuCheatX86Registers &regs)
{
    if (!xemu_cheat_get_x86_registers(&regs)) {
        m_debug_status = "Could not read Xbox x86 registers";
        return false;
    }
    return true;
}

bool MemoryToolsWindow::ResolveWatchpointAccessInstruction(
    uint32_t stop_pc, XemuCheatDisasmRow &access_row)
{
    /* x86 hardware data breakpoints report #DB after the accessing
     * instruction. Decode the page containing the stop and resolve the
     * instruction whose end address is exactly the architectural stop EIP.
     * Reuse the debugger page buffer so a watchpoint hit does not allocate a
     * separate ~672 KiB temporary vector. */
    if (m_disassembly_page_scratch.size() != kPageSize) {
        m_disassembly_page_scratch.resize(kPageSize);
    }

    auto resolve_from_page = [&](uint32_t focus) -> bool {
        size_t row_count = 0;
        const int result = xemu_cheat_disassemble_page(
            focus, m_disassembly_page_scratch.data(),
            m_disassembly_page_scratch.size(), &row_count);
        if (result != XEMU_CHEAT_DISAS_OK) {
            return false;
        }

        for (size_t i = row_count; i > 0; --i) {
            const XemuCheatDisasmRow &row = m_disassembly_page_scratch[i - 1];
            const uint64_t row_end =
                (uint64_t)row.virtual_address + std::max<uint8_t>(row.size, 1);
            if (row_end == (uint64_t)stop_pc &&
                std::strcmp(row.mnemonic, "db") != 0) {
                access_row = row;
                return true;
            }
            if (row.virtual_address + 15u < stop_pc) {
                break;
            }
        }
        return false;
    };

    if (resolve_from_page(stop_pc)) {
        return true;
    }

    /* If the stop EIP is exactly at a page boundary, the accessing
     * instruction can live at the end of the previous page. */
    return stop_pc != 0 && resolve_from_page(stop_pc - 1u);
}

void MemoryToolsWindow::RefreshDisassembly()
{
    uint32_t requested_address;
    if (!ParseHexAddress(m_disasm_address_text, requested_address)) {
        m_debug_status = "Invalid disassembly address";
        return;
    }

    /* v0.1.65 exact navigation:
     * First decode from the actual 4 KiB page boundary, not from the user's
     * requested byte. This establishes the instruction boundaries shown by
     * the page view. If the requested byte lands in the middle of an opcode,
     * select that opcode's real start instead of forcing a synthetic decode
     * boundary at the typed address. */
    const uint32_t page_base = requested_address & 0xFFFFF000u;
    /* Full-page mode can decode directly into the displayed vector. Reduced
     * mode still needs a full-page workspace first so an interior byte can be
     * aligned to the containing opcode before the smaller paired decode. */
    std::vector<XemuCheatDisasmRow> &page_rows =
        m_disasm_full_page ? m_disassembly_rows : m_disassembly_page_scratch;
    if (page_rows.size() != kPageSize) {
        page_rows.resize(kPageSize);
    }
    size_t page_row_count = 0;
    int result = xemu_cheat_disassemble_page(
        page_base, page_rows.data(), page_rows.size(), &page_row_count);

    if (result != XEMU_CHEAT_DISAS_OK) {
        m_disassembly_rows.clear();
        m_disassembly_flow_cache.clear();
        m_disassembly_virtual_text.clear();
        m_disassembly_physical_text.clear();
        if (result == XEMU_CHEAT_DISAS_UNMAPPED) {
            m_debug_status = "Virtual address is unmapped";
        } else if (result == XEMU_CHEAT_DISAS_NO_BACKEND) {
            m_debug_status =
                "x86 disassembler backend is not available in this build (Capstone missing)";
        } else {
            m_debug_status = "x86 disassembly failed";
        }
        return;
    }
    uint32_t resolved_address = requested_address;
    const size_t containing_row =
        find_disassembly_row(page_rows.data(), page_row_count, requested_address);
    if (containing_row != (size_t)-1) {
        resolved_address = page_rows[containing_row].virtual_address;
    }

    if (m_disasm_full_page) {
        m_disassembly_rows.resize(page_row_count);
    } else {
        const size_t count =
            (size_t)std::clamp(m_disasm_instruction_count, 1, 128);
        const uint64_t page_end = (uint64_t)page_base + kPageSize;
        bool reused_page_decode = false;

        /* The full-page decode above already established the exact opcode
         * boundary for Count mode. Reuse that decoded slice when every
         * requested instruction has its complete 15-byte x86 decode window
         * inside this page. Near the final 14 bytes, keep the paired path so
         * an instruction that crosses the page boundary is decoded exactly as
         * before rather than appearing as page-end db bytes. */
        if (containing_row != (size_t)-1 &&
            containing_row + count <= page_row_count) {
            const XemuCheatDisasmRow &last =
                page_rows[containing_row + count - 1];
            if ((uint64_t)last.virtual_address + 15u <= page_end) {
                m_disassembly_rows.assign(
                    page_rows.begin() + containing_row,
                    page_rows.begin() + containing_row + count);
                reused_page_decode = true;
            }
        }

        if (!reused_page_decode) {
            size_t row_count = 0;
            m_disassembly_rows.resize(count);
            result = xemu_cheat_disassemble_paired(
                resolved_address, (int)count, m_disassembly_rows.data(),
                m_disassembly_rows.size(), &row_count);
            if (result != XEMU_CHEAT_DISAS_OK) {
                m_disassembly_rows.clear();
                m_disassembly_flow_cache.clear();
                m_disassembly_virtual_text.clear();
                m_disassembly_physical_text.clear();
                if (result == XEMU_CHEAT_DISAS_UNMAPPED) {
                    m_debug_status = "Virtual address is unmapped";
                } else if (result == XEMU_CHEAT_DISAS_NO_BACKEND) {
                    m_debug_status =
                        "x86 disassembler backend is not available in this build (Capstone missing)";
                } else {
                    m_debug_status = "x86 disassembly failed";
                }
                return;
            }
            m_disassembly_rows.resize(row_count);
        }
    }

    RebuildDisassemblyFlowCache();
    RebuildDisassemblyRenderCache();

    m_disasm_address = resolved_address;
    SetHexText(m_disasm_address_text, sizeof(m_disasm_address_text),
               resolved_address);
    m_disasm_scroll_y = 0.0f;
    m_disasm_focus_virtual = resolved_address;
    m_disasm_scroll_to_focus = true;

    /* Keep the typed destination and the highlighted disassembly row unified.
     * This also makes Back/Forward and context-menu navigation land on the
     * exact opcode rather than on an arbitrary byte inside it. */
    m_have_disasm_selection = true;
    m_selected_disasm_virtual = resolved_address;
    m_selected_disasm_physical_valid = false;
    const size_t selected_row = find_disassembly_row(
        m_disassembly_rows.data(), m_disassembly_rows.size(), resolved_address);
    if (selected_row != (size_t)-1 &&
        m_disassembly_rows[selected_row].physical_valid) {
        m_selected_disasm_physical =
            m_disassembly_rows[selected_row].physical_address;
        m_selected_disasm_physical_valid = true;
    }

    if (resolved_address != requested_address) {
        char status[160];
        std::snprintf(status, sizeof(status),
                      "Address %08X is inside an instruction; aligned to opcode start %08X",
                      requested_address, resolved_address);
        m_debug_status = status;
    } else {
        m_debug_status.clear();
    }
}

void MemoryToolsWindow::RebuildDisassemblyFlowCache()
{
    m_disassembly_flow_cache.resize(m_disassembly_rows.size());
    for (size_t i = 0; i < m_disassembly_rows.size(); ++i) {
        DisassemblyFlowCache &cached = m_disassembly_flow_cache[i];
        cached = {};
        if (!AnalyzeControlFlow(m_disassembly_rows[i], cached.flow) ||
            !cached.flow.target_valid) {
            continue;
        }

        auto it = std::lower_bound(
            m_disassembly_rows.begin(), m_disassembly_rows.end(),
            cached.flow.target,
            [](const XemuCheatDisasmRow &row, uint32_t value) {
                return row.virtual_address < value;
            });
        if (it != m_disassembly_rows.end() &&
            it->virtual_address == cached.flow.target) {
            cached.target_index =
                (size_t)std::distance(m_disassembly_rows.begin(), it);
        }
    }
}

void MemoryToolsWindow::RebuildDisassemblyRenderCache()
{
    m_disassembly_virtual_text.resize(m_disassembly_rows.size());
    m_disassembly_physical_text.resize(m_disassembly_rows.size());

    /* Both the decoder rows and label database are ordered by Virtual
     * address. PrimaryLabelAt() performs a binary search, which was repeated
     * once for every decoded row whenever this render cache was rebuilt. Walk
     * the sorted label database forward instead: the first label at an exact
     * address is the same primary label lower_bound() returned previously. */
    const auto &label_database = current_game_manager.Labels();
    auto label_it = label_database.labels.begin();
    const auto label_end = label_database.labels.end();
    if (m_labels_enabled && !m_disassembly_rows.empty()) {
        label_it = std::lower_bound(
            label_it, label_end, m_disassembly_rows.front().virtual_address,
            [](const XemuXbeLabels::Label &label, uint32_t address) {
                return label.virtual_address < address;
            });
    }

    for (size_t i = 0; i < m_disassembly_rows.size(); ++i) {
        const XemuCheatDisasmRow &row = m_disassembly_rows[i];
        char bytes[64];
        format_disassembly_bytes(bytes, sizeof(bytes), row.bytes,
                                 std::min<size_t>(row.size, sizeof(row.bytes)));

        const XemuXbeLabels::Label *label = nullptr;
        if (m_labels_enabled) {
            while (label_it != label_end &&
                   label_it->virtual_address < row.virtual_address) {
                ++label_it;
            }
            if (label_it != label_end &&
                label_it->virtual_address == row.virtual_address) {
                label = &*label_it;
            }
        }

        auto format_line = [&](bool physical, std::string &out) {
            char line[512];
            if (physical) {
                if (row.physical_valid) {
                    std::snprintf(line, sizeof(line),
                                  "%08llX  %-45s %-8s %s",
                                  (unsigned long long)row.physical_address,
                                  bytes, row.mnemonic, row.operands);
                } else {
                    std::snprintf(line, sizeof(line),
                                  "--------  %-45s %-8s %s",
                                  bytes, row.mnemonic, row.operands);
                }
            } else {
                std::snprintf(line, sizeof(line),
                              "%08X  %-45s %-8s %s",
                              row.virtual_address, bytes,
                              row.mnemonic, row.operands);
            }

            if (label != nullptr) {
                const size_t used = std::strlen(line);
                if (used + label->name.size() + 6 < sizeof(line)) {
                    std::snprintf(line + used, sizeof(line) - used,
                                  "  ; %s", label->name.c_str());
                }
            }
            out.assign(line);
        };

        format_line(false, m_disassembly_virtual_text[i]);
        format_line(true, m_disassembly_physical_text[i]);
    }

    m_disassembly_label_generation = current_game_manager.LabelGeneration();
    m_disassembly_cached_labels_enabled = m_labels_enabled;
}

void MemoryToolsWindow::FollowDebuggerAddress(uint32_t address,
                                              bool refresh_disassembly)
{
    m_disasm_address = address;
    SetHexText(m_disasm_address_text, sizeof(m_disasm_address_text), address);

    uint64_t physical = 0;
    xemu_cheat_prepare_virtual_map();
    m_have_disasm_selection = true;
    m_selected_disasm_virtual = address;
    m_selected_disasm_physical_valid =
        xemu_cheat_virtual_to_physical(address, &physical) != 0;
    if (m_selected_disasm_physical_valid) {
        m_selected_disasm_physical = physical;
    }

    if (refresh_disassembly) {
        RefreshDisassembly();
    }
}

void MemoryToolsWindow::NavigateDebuggerAddress(uint32_t address)
{
    const uint32_t current = m_have_disasm_selection
                                 ? m_selected_disasm_virtual
                                 : m_disasm_address;

    if (!m_have_debug_nav_history) {
        m_debug_nav_history.clear();
        m_debug_nav_history.push_back(current);
        m_debug_nav_index = 0;
        m_have_debug_nav_history = true;
    } else if (m_debug_nav_history.empty()) {
        m_debug_nav_history.push_back(current);
        m_debug_nav_index = 0;
    } else {
        /* A single-click changes the active disassembly row without itself
         * navigating. If Follow is then used, that clicked branch instruction
         * must become the Back destination. Do not let an older history node
         * replace the actual branch source. */
        if (m_debug_nav_index >= m_debug_nav_history.size()) {
            m_debug_nav_index = m_debug_nav_history.size() - 1;
        }
        if (m_debug_nav_history[m_debug_nav_index] != current) {
            if (m_debug_nav_index + 1 < m_debug_nav_history.size()) {
                using HistoryDiff = std::vector<uint32_t>::difference_type;
                m_debug_nav_history.erase(
                    m_debug_nav_history.begin() +
                        (HistoryDiff)m_debug_nav_index + 1,
                    m_debug_nav_history.end());
            }
            if (m_debug_nav_history.empty() ||
                m_debug_nav_history.back() != current) {
                m_debug_nav_history.push_back(current);
            }
            m_debug_nav_index = m_debug_nav_history.size() - 1;
        }
    }

    if (m_debug_nav_index + 1 < m_debug_nav_history.size()) {
        using HistoryDiff = std::vector<uint32_t>::difference_type;
        m_debug_nav_history.erase(m_debug_nav_history.begin() +
                                      (HistoryDiff)m_debug_nav_index + 1,
                                  m_debug_nav_history.end());
    }

    if (m_debug_nav_history.empty() || m_debug_nav_history.back() != address) {
        m_debug_nav_history.push_back(address);
    }
    m_debug_nav_index = m_debug_nav_history.size() - 1;
    FollowDebuggerAddress(address, true);
    m_disasm_keyboard_focus_requested = true;
    m_disasm_keyboard_focus_physical = m_disasm_last_keyboard_focus_physical;

    /* RefreshDisassembly may align an interior byte to the containing opcode
     * start. Keep browser-style history on that exact resolved instruction so
     * Back/Forward does not reintroduce the unaligned byte address. */
    if (!m_debug_nav_history.empty() && m_disasm_address != address) {
        m_debug_nav_history.back() = m_disasm_address;
    }
}

bool MemoryToolsWindow::NavigateDebuggerBack()
{
    if (!m_have_debug_nav_history || m_debug_nav_index == 0 ||
        m_debug_nav_history.empty()) {
        return false;
    }

    --m_debug_nav_index;
    FollowDebuggerAddress(m_debug_nav_history[m_debug_nav_index], true);
    m_disasm_keyboard_focus_requested = true;
    m_disasm_keyboard_focus_physical = m_disasm_last_keyboard_focus_physical;
    return true;
}

bool MemoryToolsWindow::NavigateDebuggerForward()
{
    if (!m_have_debug_nav_history || m_debug_nav_history.empty() ||
        m_debug_nav_index + 1 >= m_debug_nav_history.size()) {
        return false;
    }

    ++m_debug_nav_index;
    FollowDebuggerAddress(m_debug_nav_history[m_debug_nav_index], true);
    m_disasm_keyboard_focus_requested = true;
    m_disasm_keyboard_focus_physical = m_disasm_last_keyboard_focus_physical;
    return true;
}

const XemuCheatDisasmRow *MemoryToolsWindow::SelectedDisassemblyRow() const
{
    if (!m_have_disasm_selection) {
        return nullptr;
    }
    auto it = std::lower_bound(
        m_disassembly_rows.begin(), m_disassembly_rows.end(),
        m_selected_disasm_virtual,
        [](const XemuCheatDisasmRow &row, uint32_t value) {
            return row.virtual_address < value;
        });
    return it != m_disassembly_rows.end() &&
                   it->virtual_address == m_selected_disasm_virtual
               ? &*it
               : nullptr;
}

bool MemoryToolsWindow::AnalyzeControlFlow(const XemuCheatDisasmRow &row,
                                           DebugFlowInfo &flow) const
{
    flow = {};
    const char *mnemonic = row.mnemonic;

    if (ascii_equal_case_insensitive(mnemonic, "jmp")) {
        flow.kind = DebugFlowKind::Jump;
    } else if (ascii_equal_case_insensitive(mnemonic, "call")) {
        flow.kind = DebugFlowKind::Call;
        flow.fallthrough_valid = true;
    } else if (ascii_starts_with_case_insensitive(mnemonic, "ret")) {
        flow.kind = DebugFlowKind::Return;
    } else if ((mnemonic[0] != '\0' && ascii_lower((unsigned char)mnemonic[0]) == 'j') ||
               ascii_starts_with_case_insensitive(mnemonic, "loop")) {
        flow.kind = DebugFlowKind::ConditionalJump;
        flow.fallthrough_valid = true;
    } else {
        return false;
    }

    flow.fallthrough = row.virtual_address + std::max<uint8_t>(row.size, 1);

    if (flow.kind == DebugFlowKind::Return || row.operands[0] == '\0') {
        return true;
    }

    char *end = nullptr;
    errno = 0;
    const unsigned long long parsed = std::strtoull(row.operands, &end, 0);
    if (end != row.operands && errno != ERANGE && parsed <= 0xFFFFFFFFull) {
        while (*end != '\0' && std::isspace((unsigned char)*end)) {
            ++end;
        }
        if (*end == '\0') {
            flow.target_valid = true;
            flow.target = (uint32_t)parsed;
        }
    }
    return true;
}

bool MemoryToolsWindow::RegisterValueByName(const char *name,
                                            uint32_t &value) const
{
    if (name == nullptr || !m_have_registers) {
        return false;
    }
#define MATCH_REG(regname, field) \
    if (g_ascii_strcasecmp(name, regname) == 0) { value = m_registers.field; return true; }
    MATCH_REG("eax", eax)
    MATCH_REG("ebx", ebx)
    MATCH_REG("ecx", ecx)
    MATCH_REG("edx", edx)
    MATCH_REG("esi", esi)
    MATCH_REG("edi", edi)
    MATCH_REG("esp", esp)
    MATCH_REG("ebp", ebp)
    MATCH_REG("eip", eip)
    MATCH_REG("pc", pc)
    MATCH_REG("eflags", eflags)
    MATCH_REG("cr0", cr0)
    MATCH_REG("cr2", cr2)
    MATCH_REG("cr3", cr3)
    MATCH_REG("cr4", cr4)
    MATCH_REG("cs", cs)
    MATCH_REG("ds", ds)
    MATCH_REG("es", es)
    MATCH_REG("fs", fs)
    MATCH_REG("gs", gs)
    MATCH_REG("ss", ss)
#undef MATCH_REG
    return false;
}

bool MemoryToolsWindow::ResolveIndirectControlFlowTarget(
    const char *operand, uint32_t &target) const
{
    if (operand == nullptr || operand[0] == '\0' || !m_have_registers) {
        return false;
    }

    std::string text = operand;
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char ch) { return (char)std::tolower(ch); });
    text.erase(std::remove_if(text.begin(), text.end(),
                              [](unsigned char ch) { return std::isspace(ch); }),
               text.end());

    /* Register-indirect JMP/CALL, e.g. jmp eax. */
    if (text.find('[') == std::string::npos) {
        return RegisterValueByName(text.c_str(), target);
    }

    const size_t open = text.find('[');
    const size_t close = text.rfind(']');
    if (open == std::string::npos || close == std::string::npos || close <= open) {
        return false;
    }
    /* Segment-base-aware effective addresses require descriptor state that this
     * lightweight navigator intentionally does not guess. */
    if (text.substr(0, open).find(':') != std::string::npos) {
        return false;
    }

    const std::string expr = text.substr(open + 1, close - open - 1);
    int64_t total = 0;
    size_t pos = 0;
    int sign = 1;

    while (pos < expr.size()) {
        if (expr[pos] == '+') {
            sign = 1;
            ++pos;
            continue;
        }
        if (expr[pos] == '-') {
            sign = -1;
            ++pos;
            continue;
        }

        size_t end = pos;
        while (end < expr.size() && expr[end] != '+' && expr[end] != '-') {
            ++end;
        }
        const std::string term = expr.substr(pos, end - pos);
        if (term.empty()) {
            return false;
        }

        uint64_t term_value = 0;
        const size_t star = term.find('*');
        if (star != std::string::npos) {
            uint32_t reg_value = 0;
            if (!RegisterValueByName(term.substr(0, star).c_str(), reg_value)) {
                return false;
            }
            const std::string scale_text = term.substr(star + 1);
            char *scale_end = nullptr;
            const unsigned long scale =
                std::strtoul(scale_text.c_str(), &scale_end, 0);
            if (scale_end == scale_text.c_str() || *scale_end != '\0' ||
                (scale != 1 && scale != 2 && scale != 4 && scale != 8)) {
                return false;
            }
            term_value = (uint64_t)reg_value * scale;
        } else {
            uint32_t reg_value = 0;
            if (RegisterValueByName(term.c_str(), reg_value)) {
                term_value = reg_value;
            } else {
                char *number_end = nullptr;
                errno = 0;
                const unsigned long long number =
                    std::strtoull(term.c_str(), &number_end, 0);
                if (number_end == term.c_str() || *number_end != '\0' ||
                    errno == ERANGE || number > 0xFFFFFFFFull) {
                    return false;
                }
                term_value = number;
            }
        }

        total += sign * (int64_t)term_value;
        sign = 1;
        pos = end;
    }

    const uint32_t effective_address = (uint32_t)total;
    uint8_t pointer_bytes[4];
    if (!Read(AddressSpace::Virtual, effective_address,
              pointer_bytes, sizeof(pointer_bytes))) {
        return false;
    }
    target = load_le(pointer_bytes, sizeof(pointer_bytes));
    return true;
}

bool MemoryToolsWindow::ResolveControlFlowTarget(
    const XemuCheatDisasmRow &row, uint32_t &target) const
{
    DebugFlowInfo flow;
    if (!AnalyzeControlFlow(row, flow)) {
        return false;
    }
    if (flow.target_valid) {
        target = flow.target;
        return true;
    }

    /* Dynamic register/stack state is meaningful only for the instruction the
     * paused CPU is actually about to execute. Do not use Current Registers to
     * guess the target of an arbitrary historical disassembly row. */
    if (!m_have_registers || row.virtual_address != m_registers.pc) {
        return false;
    }

    if (flow.kind == DebugFlowKind::Return) {
        uint8_t return_bytes[4];
        if (!Read(AddressSpace::Virtual, m_registers.esp,
                  return_bytes, sizeof(return_bytes))) {
            return false;
        }
        target = load_le(return_bytes, sizeof(return_bytes));
        return true;
    }
    return ResolveIndirectControlFlowTarget(row.operands, target);
}

void MemoryToolsWindow::HandleDebuggerNavigationKeys()
{
    if (m_debug_nav_key_consumed || ImGui::GetIO().WantTextInput) {
        return;
    }

    const XemuCheatDisasmRow *row = SelectedDisassemblyRow();
    const bool right = ImGui::IsKeyPressed(ImGuiKey_RightArrow, false);
    const bool left = ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false);
    const bool shift = ImGui::GetIO().KeyShift;
    const bool alt = ImGui::GetIO().KeyAlt;

    if (alt && right) {
        if (NavigateDebuggerForward()) {
            m_debug_status = "Debugger navigation: Forward";
        }
        m_debug_nav_key_consumed = true;
        return;
    }
    if (left) {
        if (NavigateDebuggerBack()) {
            m_debug_status = "Debugger navigation: Back";
        }
        m_debug_nav_key_consumed = true;
        return;
    }
    if (!right || row == nullptr) {
        return;
    }

    DebugFlowInfo flow;
    if (!AnalyzeControlFlow(*row, flow)) {
        m_debug_status = "Selected instruction has no control-flow target";
        m_debug_nav_key_consumed = true;
        return;
    }

    if (shift) {
        if (flow.fallthrough_valid) {
            NavigateDebuggerAddress(flow.fallthrough);
            m_debug_status = "Followed fall-through";
        } else {
            m_debug_status = "Selected instruction has no fall-through target";
        }
        m_debug_nav_key_consumed = true;
        return;
    }

    uint32_t target = 0;
    if (ResolveControlFlowTarget(*row, target)) {
        NavigateDebuggerAddress(target);
        m_debug_status = "Followed branch/call/return target";
    } else {
        m_debug_status = "Control-flow target is unresolved in the current CPU state";
    }
    m_debug_nav_key_consumed = true;
}

void MemoryToolsWindow::BeginRegisterEdit(const char *name, uint32_t value)
{
    if (name == nullptr || name[0] == '\0') {
        return;
    }
    g_strlcpy(m_register_edit_name, name, sizeof(m_register_edit_name));
    std::snprintf(m_register_edit_text, sizeof(m_register_edit_text), "%08X", value);
    m_register_edit_is_temp = false;
    m_register_edit_temp_address = 0;
    m_register_edit_active = true;
    m_register_edit_focus_requested = true;
}

void MemoryToolsWindow::BeginTempRegisterEdit(const char *name, uint32_t value,
                                              uint32_t storage_address)
{
    if (name == nullptr || name[0] == '\0') {
        return;
    }
    g_strlcpy(m_register_edit_name, name, sizeof(m_register_edit_name));
    std::snprintf(m_register_edit_text, sizeof(m_register_edit_text), "%08X", value);
    m_register_edit_is_temp = true;
    m_register_edit_temp_address = storage_address;
    m_register_edit_active = true;
    m_register_edit_focus_requested = true;
}

bool MemoryToolsWindow::CommitRegisterEdit()
{
    if (!m_register_edit_active) {
        return false;
    }

    uint32_t value = 0;
    if (!ParseHexAddress(m_register_edit_text, value)) {
        m_debug_status = "Invalid register value";
        return false;
    }
    if (m_register_edit_is_temp) {
        if (runstate_is_running()) {
            m_debug_status = "Pause the Xbox before editing F0 temp registers";
            return false;
        }
        uint8_t bytes[4];
        store_le(bytes, sizeof(bytes), value);
        if (!Write(AddressSpace::Virtual, m_register_edit_temp_address,
                   bytes, sizeof(bytes))) {
            m_debug_status = "Could not write F0 temp register storage";
            return false;
        }
        char status[112];
        std::snprintf(status, sizeof(status), "%s changed to %08X",
                      m_register_edit_name, value);
        m_debug_status = status;
        m_register_edit_active = false;
        m_register_edit_focus_requested = false;
        m_register_edit_is_temp = false;
        m_register_edit_temp_address = 0;
        return true;
    }
    if (g_ascii_strcasecmp(m_register_edit_name, "PC") == 0) {
        m_debug_status = "PC is derived from CS:EIP; edit EIP instead";
        return false;
    }
    if (!xemu_cheat_set_x86_register(m_register_edit_name, value)) {
        m_debug_status = runstate_is_running()
                             ? "Pause the Xbox before editing live registers"
                             : "Could not write live x86 register";
        return false;
    }

    XemuCheatX86Registers refreshed = {};
    if (RefreshRegisters(refreshed)) {
        m_registers = refreshed;
        m_have_registers = true;
    }
    char status[96];
    std::snprintf(status, sizeof(status), "%s changed to %08X",
                  m_register_edit_name, value);
    m_debug_status = status;
    m_register_edit_active = false;
    m_register_edit_focus_requested = false;
    m_register_edit_is_temp = false;
    m_register_edit_temp_address = 0;
    return true;
}

bool MemoryToolsWindow::IsEnabledBreakpointAt(uint32_t address) const
{
    return std::any_of(m_breakpoints.begin(), m_breakpoints.end(),
                       [address](const ExecuteBreakpoint &bp) {
                           return bp.enabled && bp.address == address;
                       });
}

bool MemoryToolsWindow::StartDebugStep(DebugStepMode mode)
{
    if (runstate_is_running()) {
        return false;
    }

    m_debug_step_mode = mode;
    m_was_debug_paused = false;

    if (!xemu_cheat_start_single_step()) {
        m_debug_step_mode = DebugStepMode::None;
        m_debug_status = "Could not start x86 single-step";
        return false;
    }
    return true;
}

bool MemoryToolsWindow::ContinueFilteredExecuteBreakpoint(uint32_t address)
{
    const int backend = xemu_cheat_debug_backend();
    m_breakpoint_status = "Breakpoint condition not met; continuing...";

    if (backend == XEMU_CHEAT_DEBUG_BACKEND_TCG) {
        return StartDebugStep(DebugStepMode::ContinuePastBreakpoint);
    }

    if (backend == XEMU_CHEAT_DEBUG_BACKEND_KVM) {
        if (!xemu_cheat_breakpoint_remove(address)) {
            m_breakpoint_status =
                "Breakpoint condition not met, but the KVM execute breakpoint could not be stepped over";
            return false;
        }
        m_resume_breakpoint_restore_pending = true;
        m_resume_breakpoint_restore_address = address;
        if (StartDebugStep(DebugStepMode::ContinuePastBreakpoint)) {
            return true;
        }
        xemu_cheat_breakpoint_insert(address);
        m_resume_breakpoint_restore_pending = false;
        return false;
    }

    /* WHPX performs its proven native breakpoint step-over in whpx_vcpu_run.
     * Any future backend reaching this branch has already delivered the debug
     * stop, so resume normally rather than adding another breakpoint scheme. */
    xemu_cheat_single_step(0);
    m_debug_step_mode = DebugStepMode::None;
    m_was_debug_paused = false;
    vm_start();
    return true;
}

void MemoryToolsWindow::OpenBreakpointConditionEditor(
    const ExecuteBreakpoint &bp)
{
    m_condition_target = BreakpointConditionTarget::Execute;
    m_condition_target_address = bp.address;
    m_condition_target_length = 0;
    m_condition_target_access_flags = 0;
    m_condition_editor_text = bp.condition_text;
    xemu_breakpoint_conditions_parse(m_condition_editor_text,
                                     m_condition_editor_preview,
                                     m_condition_editor_error);
    m_condition_editor_open = true;
    m_condition_editor_focus_requested = true;
}

void MemoryToolsWindow::OpenBreakpointConditionEditor(
    const DataWatchpoint &wp)
{
    m_condition_target = BreakpointConditionTarget::Watchpoint;
    m_condition_target_address = wp.address;
    m_condition_target_length = wp.length;
    m_condition_target_access_flags = wp.access_flags;
    m_condition_editor_text = wp.condition_text;
    xemu_breakpoint_conditions_parse(m_condition_editor_text,
                                     m_condition_editor_preview,
                                     m_condition_editor_error);
    m_condition_editor_open = true;
    m_condition_editor_focus_requested = true;
}

bool MemoryToolsWindow::ApplyBreakpointConditionEditor()
{
    std::vector<XemuBreakpointCondition> parsed;
    std::string error;
    if (!xemu_breakpoint_conditions_parse(m_condition_editor_text,
                                          parsed, error)) {
        m_condition_editor_error = error;
        return false;
    }

    if (m_condition_target == BreakpointConditionTarget::Execute) {
        auto it = std::find_if(
            m_breakpoints.begin(), m_breakpoints.end(),
            [this](const ExecuteBreakpoint &bp) {
                return bp.address == m_condition_target_address;
            });
        if (it == m_breakpoints.end()) {
            m_condition_editor_error = "The execute breakpoint no longer exists.";
            return false;
        }
        it->conditions = parsed;
        it->condition_text = parsed.empty() ? std::string() : m_condition_editor_text;
    } else if (m_condition_target == BreakpointConditionTarget::Watchpoint) {
        auto it = std::find_if(
            m_watchpoints.begin(), m_watchpoints.end(),
            [this](const DataWatchpoint &wp) {
                return wp.address == m_condition_target_address &&
                       wp.length == m_condition_target_length &&
                       wp.access_flags == m_condition_target_access_flags;
            });
        if (it == m_watchpoints.end()) {
            m_condition_editor_error = "The data breakpoint no longer exists.";
            return false;
        }
        it->conditions = parsed;
        it->condition_text = parsed.empty() ? std::string() : m_condition_editor_text;
    } else {
        m_condition_editor_error = "No breakpoint is selected.";
        return false;
    }

    m_condition_editor_preview = parsed;
    m_condition_editor_error.clear();
    char status[128];
    std::snprintf(status, sizeof(status),
                  "%s conditions at %08X (%zu condition%s)",
                  parsed.empty() ? "Cleared" : "Updated",
                  m_condition_target_address, parsed.size(),
                  parsed.size() == 1 ? "" : "s");
    m_breakpoint_status = status;
    return true;
}

void MemoryToolsWindow::ClearBreakpointConditionEditor()
{
    m_condition_editor_text.clear();
    m_condition_editor_preview.clear();
    m_condition_editor_error.clear();
    ApplyBreakpointConditionEditor();
}

bool MemoryToolsWindow::AddExecuteBreakpoint(uint32_t address)
{
    auto it = std::find_if(m_breakpoints.begin(), m_breakpoints.end(),
                           [address](const ExecuteBreakpoint &bp) {
                               return bp.address == address;
                           });
    if (it != m_breakpoints.end()) {
        if (!it->enabled) {
            if (!xemu_cheat_breakpoint_insert(address)) {
                m_breakpoint_status = "Could not enable execute breakpoint";
                return false;
            }
            it->enabled = true;
        }
        m_breakpoint_status = "Breakpoint already exists";
        return true;
    }

    if (!xemu_cheat_breakpoint_insert(address)) {
        m_breakpoint_status =
            xemu_cheat_debug_backend() == XEMU_CHEAT_DEBUG_BACKEND_KVM
                ? "Could not insert execute breakpoint (KVM has four shared hardware debug slots)"
                : "Could not insert execute breakpoint";
        return false;
    }
    m_breakpoints.push_back({address, true});

    uint64_t physical = 0;
    char text[128];
    xemu_cheat_prepare_virtual_map();
    if (xemu_cheat_virtual_to_physical(address, &physical)) {
        std::snprintf(text, sizeof(text),
                      "Execute breakpoint added: V %08X -> P %08llX",
                      address, (unsigned long long)physical);
    } else {
        std::snprintf(text, sizeof(text),
                      "Execute breakpoint added at V %08X (currently unmapped)",
                      address);
    }
    m_breakpoint_status = text;
    return true;
}

void MemoryToolsWindow::RemoveExecuteBreakpoint(size_t index)
{
    if (index >= m_breakpoints.size()) {
        return;
    }
    const uint32_t address = m_breakpoints[index].address;
    if (m_breakpoints[index].enabled) {
        xemu_cheat_breakpoint_remove(address);
    }
    if (m_condition_editor_open &&
        m_condition_target == BreakpointConditionTarget::Execute &&
        m_condition_target_address == address) {
        m_condition_editor_open = false;
        m_condition_target = BreakpointConditionTarget::None;
    }
    m_breakpoints.erase(m_breakpoints.begin() + (ptrdiff_t)index);
    m_breakpoint_status = "Breakpoint removed";
}

bool MemoryToolsWindow::AddDataWatchpoint(uint32_t address, uint32_t length,
                                              int access_flags)
{
    auto it = std::find_if(m_watchpoints.begin(), m_watchpoints.end(),
                           [address, length, access_flags](const DataWatchpoint &wp) {
                               return wp.address == address &&
                                      wp.length == length &&
                                      wp.access_flags == access_flags;
                           });
    if (it != m_watchpoints.end()) {
        if (!it->enabled) {
            if (!xemu_cheat_watchpoint_access_supported(access_flags)) {
                m_breakpoint_status =
                    xemu_cheat_debug_backend() == XEMU_CHEAT_DEBUG_BACKEND_KVM &&
                            access_flags == XEMU_CHEAT_WATCH_READ
                        ? "KVM does not provide a reliable Read-only x86 hardware watchpoint; use Read/Write or TCG"
                        : "This watchpoint access type is not supported by the active debugger backend";
                return false;
            }
            if (!xemu_cheat_watchpoint_insert(address, length, access_flags)) {
                const int backend = xemu_cheat_debug_backend();
                m_breakpoint_status =
                    backend == XEMU_CHEAT_DEBUG_BACKEND_WHPX
                        ? "Could not enable data watchpoint (WHPX hardware slots exhausted or range unsupported)"
                        : (backend == XEMU_CHEAT_DEBUG_BACKEND_KVM
                               ? "Could not enable data watchpoint (KVM has four shared hardware debug slots; range may need multiple slots)"
                               : "Could not enable data watchpoint");
                return false;
            }
            it->enabled = true;
        }
        m_breakpoint_status = "Data watchpoint already exists";
        return true;
    }

    if (!xemu_cheat_watchpoint_supported()) {
        m_breakpoint_status = "Read/Write watchpoints are not supported by the active debugger backend";
        return false;
    }
    if (!xemu_cheat_watchpoint_access_supported(access_flags)) {
        m_breakpoint_status =
            xemu_cheat_debug_backend() == XEMU_CHEAT_DEBUG_BACKEND_KVM &&
                    access_flags == XEMU_CHEAT_WATCH_READ
                ? "KVM does not provide a reliable Read-only x86 hardware watchpoint; use Read/Write or run the debugger with TCG"
                : "This watchpoint access type is not supported by the active debugger backend";
        return false;
    }

    if (!xemu_cheat_watchpoint_insert(address, length, access_flags)) {
        const int backend = xemu_cheat_debug_backend();
        m_breakpoint_status =
            backend == XEMU_CHEAT_DEBUG_BACKEND_WHPX
                ? "Could not insert data watchpoint (WHPX has four hardware slots; Read-only can use two per range chunk)"
                : (backend == XEMU_CHEAT_DEBUG_BACKEND_KVM
                       ? "Could not insert data watchpoint (KVM has four shared hardware debug slots; range may need multiple slots)"
                       : "Could not insert data watchpoint");
        return false;
    }

    m_watchpoints.push_back({address, length, access_flags, true});

    uint64_t physical = 0;
    const char *kind = access_flags == XEMU_CHEAT_WATCH_READ
                           ? "Read"
                           : (access_flags == XEMU_CHEAT_WATCH_WRITE ? "Write"
                                                                     : "Read/Write");
    char text[176];
    xemu_cheat_prepare_virtual_map();
    if (xemu_cheat_virtual_to_physical(address, &physical)) {
        std::snprintf(text, sizeof(text),
                      "%s watchpoint added: V %08X -> P %08llX, len %u",
                      kind, address, (unsigned long long)physical, length);
    } else {
        std::snprintf(text, sizeof(text),
                      "%s watchpoint added at V %08X, len %u (currently unmapped)",
                      kind, address, length);
    }
    m_breakpoint_status = text;
    return true;
}

void MemoryToolsWindow::RemoveDataWatchpoint(size_t index)
{
    if (index >= m_watchpoints.size()) {
        return;
    }
    const DataWatchpoint wp = m_watchpoints[index];
    if (wp.enabled) {
        xemu_cheat_watchpoint_remove(wp.address, wp.length, wp.access_flags);
    }
    if (m_condition_editor_open &&
        m_condition_target == BreakpointConditionTarget::Watchpoint &&
        m_condition_target_address == wp.address &&
        m_condition_target_length == wp.length &&
        m_condition_target_access_flags == wp.access_flags) {
        m_condition_editor_open = false;
        m_condition_target = BreakpointConditionTarget::None;
    }
    m_watchpoints.erase(m_watchpoints.begin() + (ptrdiff_t)index);
    m_breakpoint_status = "Data watchpoint removed";
}

bool MemoryToolsWindow::ResolveBreakpointVirtualAddress(
    AddressSpace space, uint32_t address, uint32_t &virtual_address)
{
    if (space == AddressSpace::Virtual) {
        virtual_address = address;
        return true;
    }

    if (!m_memory_map_valid && !RefreshMemoryMap()) {
        m_breakpoint_status =
            "Could not build the Memory Map needed to translate this Physical address";
        return false;
    }

    const size_t index = FindRegionForPhysical(address);
    if (index == (size_t)-1) {
        m_breakpoint_status = "Physical address has no mapped Virtual alias";
        return false;
    }

    const MemoryMapRegion &region = m_memory_map_regions[index];
    const uint64_t resolved =
        region.virtual_start + ((uint64_t)address - region.physical_start);
    if (resolved > 0xFFFFFFFFull) {
        m_breakpoint_status = "Mapped Virtual breakpoint address is out of range";
        return false;
    }

    m_active_map_region = index;
    virtual_address = (uint32_t)resolved;
    return true;
}

bool MemoryToolsWindow::AddBreakpointByKind(uint32_t virtual_address, int kind)
{
    SetHexText(m_breakpoint_address_text,
               sizeof(m_breakpoint_address_text), virtual_address);
    m_breakpoint_kind = std::clamp(kind, 0, 3);

    if (m_breakpoint_kind == 0) {
        return AddExecuteBreakpoint(virtual_address);
    }

    const uint32_t length = (uint32_t)std::max(m_watchpoint_length, 1);
    const int flags =
        m_breakpoint_kind == 1
            ? XEMU_CHEAT_WATCH_READ
            : (m_breakpoint_kind == 2 ? XEMU_CHEAT_WATCH_WRITE
                                      : XEMU_CHEAT_WATCH_ACCESS);
    return AddDataWatchpoint(virtual_address, length, flags);
}

void MemoryToolsWindow::SetContextStatus(ContextOrigin origin,
                                             AddressSpace space,
                                             const std::string &status)
{
    switch (origin) {
    case ContextOrigin::Memory:
        (space == AddressSpace::Virtual ? m_virtual_viewer : m_physical_viewer)
            .status = status;
        break;
    case ContextOrigin::Search:
        m_search_status = status;
        break;
    case ContextOrigin::Debugger:
        m_debug_status = status;
        break;
    }
}

bool MemoryToolsWindow::ResolveContextVirtualAddress(
    AddressSpace space, uint32_t address, bool have_override,
    uint32_t override_address, uint32_t &virtual_address)
{
    if (have_override) {
        virtual_address = override_address;
        return true;
    }
    return ResolveBreakpointVirtualAddress(space, address, virtual_address);
}

bool MemoryToolsWindow::CopyContextInstruction(
    AddressSpace space, uint32_t address,
    const XemuCheatDisasmRow *disasm_row,
    bool have_breakpoint_virtual_override,
    uint32_t breakpoint_virtual_override)
{
    XemuCheatDisasmRow decoded = {};
    const XemuCheatDisasmRow *row = disasm_row;

    if (row == nullptr) {
        uint32_t virtual_address = 0;
        if (!ResolveContextVirtualAddress(space, address,
                                          have_breakpoint_virtual_override,
                                          breakpoint_virtual_override,
                                          virtual_address)) {
            return false;
        }

        size_t row_count = 0;
        if (xemu_cheat_disassemble_paired(virtual_address, 1, &decoded, 1,
                                          &row_count) != XEMU_CHEAT_DISAS_OK ||
            row_count == 0) {
            return false;
        }
        row = &decoded;
    }

    std::string text = row->mnemonic;
    if (row->operands[0] != '\0') {
        if (!text.empty()) {
            text += " ";
        }
        text += row->operands;
    }
    if (text.empty()) {
        return false;
    }

    ImGui::SetClipboardText(text.c_str());
    return true;
}

void MemoryToolsWindow::UpdateBreakpointHitState()
{
    const bool debug_paused = runstate_get() == RUN_STATE_DEBUG;
    if (!debug_paused) {
        if (runstate_is_running()) {
            m_was_debug_paused = false;
        }
        return;
    }

    /* Once a debug stop has been processed, DrawDebugger() already refreshes
     * the live register panes on its 100 ms cadence. Avoid a second backend
     * register fetch on every rendered frame while the VM remains stopped. */
    if (m_was_debug_paused) {
        return;
    }

    XemuCheatX86Registers regs = {};
    if (!RefreshRegisters(regs)) {
        return;
    }
    m_registers = regs;
    m_have_registers = true;

    /* A debugger-requested single step always stops with RUN_STATE_DEBUG.
     * For a normal Step Into we leave the VM paused at the new EIP. For
     * Continue from a breakpoint this stop is intentionally invisible: the
     * breakpoint instruction has now executed once, so disable stepping and
     * immediately continue normal execution. */
    if (m_debug_step_mode != DebugStepMode::None) {
        const DebugStepMode completed_mode = m_debug_step_mode;
        m_debug_step_mode = DebugStepMode::None;
        xemu_cheat_single_step(0);

        if (m_resume_breakpoint_restore_pending) {
            const uint32_t restore_address = m_resume_breakpoint_restore_address;
            m_resume_breakpoint_restore_pending = false;
            if (!xemu_cheat_breakpoint_insert(restore_address)) {
                char status[160];
                std::snprintf(status, sizeof(status),
                              "KVM step completed, but execute breakpoint %08X could not be restored",
                              restore_address);
                m_breakpoint_status = status;
                m_was_debug_paused = true;
                return;
            }
        }

        if (completed_mode == DebugStepMode::ContinuePastBreakpoint) {
            m_breakpoint_status = "Continued cleanly past execute breakpoint";
            m_was_debug_paused = false;
            vm_start();
            return;
        }

        uint64_t physical = 0;
        const bool physical_valid =
            xemu_cheat_virtual_to_physical(regs.pc, &physical) != 0;
        m_have_disasm_selection = true;
        m_selected_disasm_virtual = regs.pc;
        m_selected_disasm_physical_valid = physical_valid;
        if (physical_valid) {
            m_selected_disasm_physical = physical;
        }

        char status[128];
        if (physical_valid) {
            std::snprintf(status, sizeof(status),
                          "Step complete: V %08X -> P %08llX",
                          regs.pc, (unsigned long long)physical);
        } else {
            std::snprintf(status, sizeof(status),
                          "Step complete at V %08X (physical unmapped)",
                          regs.pc);
        }
        m_breakpoint_status = status;

        if (m_follow_eip) {
            FollowDebuggerAddress(regs.pc, true);
        }
        m_was_debug_paused = true;
        return;
    }

    /* QEMU/TCG/WHPX records the exact data watchpoint and byte address that
     * triggered the debug stop. x86 hardware data breakpoints report #DB
     * after the memory-access instruction has completed, so regs.pc is the
     * architectural stop EIP while hit_address is the watched data address. */
    XemuCheatWatchpointHit watch_hit = {};
    const int watch_hit_result = xemu_cheat_watchpoint_get_hit(&watch_hit);
    if (watch_hit_result == XEMU_CHEAT_WATCH_HIT_REPORTED) {
        auto condition_watch = std::find_if(
            m_watchpoints.begin(), m_watchpoints.end(),
            [&watch_hit](const DataWatchpoint &wp) {
                return wp.enabled && wp.address == watch_hit.watch_address &&
                       wp.length == watch_hit.length &&
                       (wp.access_flags & watch_hit.access_flags) != 0;
            });
        if (condition_watch != m_watchpoints.end() &&
            !xemu_breakpoint_conditions_evaluate(condition_watch->conditions, regs)) {
            m_breakpoint_status =
                "Data breakpoint condition not met; continuing...";
            m_was_debug_paused = false;
            vm_start();
            return;
        }

        m_break_registers = regs;
        m_have_break_registers = true;
        CaptureBreakpointExtraRegisters();
        m_last_break_pc = regs.pc;
        m_have_break_highlight = true;
        m_last_break_highlight_pc = regs.pc;
        m_last_break_highlight_is_access = false;
        m_last_break_physical_valid =
            xemu_cheat_virtual_to_physical(regs.pc,
                                           &m_last_break_physical) != 0;

        XemuCheatDisasmRow access_row = {};
        const bool access_resolved =
            ResolveWatchpointAccessInstruction(regs.pc, access_row);
        if (access_resolved) {
            m_last_break_highlight_pc = access_row.virtual_address;
            m_last_break_highlight_is_access = true;
        }

        m_have_disasm_selection = true;
        m_selected_disasm_virtual = m_last_break_highlight_pc;
        if (access_resolved) {
            m_selected_disasm_physical_valid = access_row.physical_valid != 0;
            if (m_selected_disasm_physical_valid) {
                m_selected_disasm_physical = access_row.physical_address;
            }
        } else {
            m_selected_disasm_physical_valid = m_last_break_physical_valid;
            if (m_last_break_physical_valid) {
                m_selected_disasm_physical = m_last_break_physical;
            }
        }

        uint64_t watched_physical = 0;
        const bool watched_physical_valid =
            xemu_cheat_virtual_to_physical(watch_hit.hit_address,
                                           &watched_physical) != 0;
        const char *kind =
            watch_hit.access_flags == XEMU_CHEAT_WATCH_READ
                ? "READ"
                : (watch_hit.access_flags == XEMU_CHEAT_WATCH_WRITE
                       ? "WRITE"
                       : "READ/WRITE");

        char status[320];
        if (access_resolved) {
            if (watched_physical_valid && access_row.physical_valid &&
                m_last_break_physical_valid) {
                std::snprintf(status, sizeof(status),
                              "%s watchpoint hit: data V %08X -> P %08llX; access V %08X -> P %08llX; current EIP V %08X -> P %08llX",
                              kind, watch_hit.hit_address,
                              (unsigned long long)watched_physical,
                              access_row.virtual_address,
                              (unsigned long long)access_row.physical_address,
                              regs.pc,
                              (unsigned long long)m_last_break_physical);
            } else {
                std::snprintf(status, sizeof(status),
                              "%s watchpoint hit: data V %08X; access V %08X; current EIP V %08X",
                              kind, watch_hit.hit_address,
                              access_row.virtual_address, regs.pc);
            }
        } else if (watched_physical_valid && m_last_break_physical_valid) {
            std::snprintf(status, sizeof(status),
                          "%s watchpoint hit: data V %08X -> P %08llX; current EIP V %08X -> P %08llX (access instruction unresolved)",
                          kind, watch_hit.hit_address,
                          (unsigned long long)watched_physical, regs.pc,
                          (unsigned long long)m_last_break_physical);
        } else {
            std::snprintf(status, sizeof(status),
                          "%s watchpoint hit: data V %08X; current EIP V %08X (access instruction unresolved)",
                          kind, watch_hit.hit_address, regs.pc);
        }
        m_breakpoint_status = status;

        if (m_follow_eip) {
            m_disasm_full_page = true;
            FollowDebuggerAddress(m_last_break_highlight_pc, true);
        }
        m_was_debug_paused = true;
        return;
    }

    auto it = std::find_if(m_breakpoints.begin(), m_breakpoints.end(),
                           [&regs](const ExecuteBreakpoint &bp) {
                               return bp.enabled && bp.address == regs.pc;
                           });
    if (it != m_breakpoints.end()) {
        if (!xemu_breakpoint_conditions_evaluate(it->conditions, regs)) {
            if (!ContinueFilteredExecuteBreakpoint(regs.pc)) {
                m_was_debug_paused = true;
            }
            return;
        }

        m_break_registers = regs;
        m_have_break_registers = true;
        CaptureBreakpointExtraRegisters();
        m_last_break_pc = regs.pc;
        m_have_break_highlight = true;
        m_last_break_highlight_pc = regs.pc;
        m_last_break_highlight_is_access = false;
        m_last_break_physical_valid =
            xemu_cheat_virtual_to_physical(regs.pc,
                                           &m_last_break_physical) != 0;

        m_have_disasm_selection = true;
        m_selected_disasm_virtual = regs.pc;
        m_selected_disasm_physical_valid = m_last_break_physical_valid;
        if (m_last_break_physical_valid) {
            m_selected_disasm_physical = m_last_break_physical;
        }

        char status[144];
        if (m_last_break_physical_valid) {
            std::snprintf(status, sizeof(status),
                          "Breakpoint hit: V %08X -> P %08llX",
                          regs.pc,
                          (unsigned long long)m_last_break_physical);
        } else {
            std::snprintf(status, sizeof(status),
                          "Breakpoint hit at V %08X (physical unmapped)",
                          regs.pc);
        }
        m_breakpoint_status = status;

        if (m_follow_eip) {
            m_disasm_full_page = true;
            FollowDebuggerAddress(regs.pc, true);
        }
    }

    m_was_debug_paused = true;
}

// Debugger rendering/UI methods are owned by memory-tools-debugger-ui.cc.

void MemoryToolsWindow::CaptureBreakpointExtraRegisters()
{
    XemuCheatX86ExtraRegisters extra = {};
    if (xemu_cheat_get_x86_extra_registers(&extra)) {
        m_break_extra_registers = extra;
        m_have_break_extra_registers = true;
    } else {
        m_break_extra_registers = {};
        m_have_break_extra_registers = false;
    }
}

