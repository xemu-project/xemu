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
#include "tab-style.hh"
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

MemoryToolsWindow memory_tools_window;

MemoryToolsWindow::MemoryToolsWindow()
{
    m_physical_viewer.address = 0x00000000u;
    m_virtual_viewer.address = 0x00010000u;
    SetHexText(m_physical_viewer.address_text,
               sizeof(m_physical_viewer.address_text),
               m_physical_viewer.address);
    SetHexText(m_virtual_viewer.address_text,
               sizeof(m_virtual_viewer.address_text),
               m_virtual_viewer.address);

    SetHexText(m_scan_start_text, sizeof(m_scan_start_text), m_scan_start);
    SetHexText(m_scan_end_text, sizeof(m_scan_end_text), m_scan_end);
    std::snprintf(m_search_value_text, sizeof(m_search_value_text), "0");

    SetHexText(m_disasm_address_text, sizeof(m_disasm_address_text),
               m_disasm_address);
    std::snprintf(m_breakpoint_address_text,
                  sizeof(m_breakpoint_address_text), "00010000");

    SetHexText(m_map_virtual_text, sizeof(m_map_virtual_text), 0x00010000u);
    SetHexText(m_map_physical_text, sizeof(m_map_physical_text), 0x00000000u);
    m_code_patch_generation = xemu_cheat_code_patch_generation();
}

bool MemoryToolsWindow::Read(AddressSpace space, uint32_t address,
                             void *buffer, size_t size) const
{
    return xemu_cheat_memory_read(space == AddressSpace::Virtual,
                                  address, buffer, size) != 0;
}

bool MemoryToolsWindow::Write(AddressSpace space, uint32_t address,
                              const void *buffer, size_t size) const
{
    return xemu_cheat_memory_write(space == AddressSpace::Virtual,
                                   address, buffer, size) != 0;
}

bool MemoryToolsWindow::ParseHexAddress(const char *text, uint32_t &value)
{
    if (!text || !*text) {
        return false;
    }
    errno = 0;
    char *end = nullptr;
    unsigned long long v = std::strtoull(text, &end, 16);
    while (end && *end == ' ') {
        ++end;
    }
    if (errno != 0 || end == text || (end && *end != '\0') ||
        v > 0xFFFFFFFFull) {
        return false;
    }
    value = (uint32_t)v;
    return true;
}

void MemoryToolsWindow::SetHexText(char *dst, size_t dst_size, uint32_t value)
{
    std::snprintf(dst, dst_size, "%08X", value);
}

void MemoryToolsWindow::Draw(bool detached)
{
    if (!is_open) {
        return;
    }

    /* Settings are loaded before the xui frame loop, so this is the first safe
     * point to import debugger-owned preferences. Loading here also ensures
     * Memory/Search context-menu jumps use the persisted disassembly mode
     * before the x86 Debugger tab is first drawn. */
    LoadDebuggerPreferences();

    m_detached_rendering = detached;

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_None;
    const char *window_name = "Memory Viewer / Search";
    bool *window_open = &is_open;
    if (detached) {
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
        window_name = "##DetachedMemoryViewerSearch";
        window_open = nullptr;
    } else {
        ImGui::SetNextWindowSize(ImVec2(1180, 720), ImGuiCond_FirstUseEver);
    }
    if (!ImGui::Begin(window_name, window_open, window_flags)) {
        ImGui::End();
        m_detached_rendering = false;
        return;
    }

    current_game_manager.DrawInlineSummary("memory-tools-current-game");
    ImGui::Separator();

    if (!xemu_cheat_cpu_available()) {
        ImGui::TextUnformatted("Xbox CPU is not available yet. Start a game to use virtual memory tools.");
    }

    XemuDebugUi::ScopedTabStyle tab_style;
    if (ImGui::BeginTabBar("memory_tools_tabs")) {
        const bool select_memory_tab = m_request_memory_tab;
        const bool select_debugger_tab = m_request_debugger_tab;
        if (select_memory_tab) {
            m_request_memory_tab = false;
        }
        if (select_debugger_tab) {
            m_request_debugger_tab = false;
        }
        if (ImGui::BeginTabItem(
                "Memory", nullptr,
                select_memory_tab ? ImGuiTabItemFlags_SetSelected
                                  : ImGuiTabItemFlags_None)) {
            DrawMemoryWorkspace();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Search / Compare")) {
            DrawSearch();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(
                "x86 Debugger", nullptr,
                select_debugger_tab ? ImGuiTabItemFlags_SetSelected
                                    : ImGuiTabItemFlags_None)) {
            DrawDebugger();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Dump RAM")) {
            DrawDumpRam();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    tab_style.Restore();

    ImGui::End();

    /* Keep debugger Inject editors independent of the active tab. Closing an
     * editor never silently changes guest memory; RESTORE remains explicit. */
    DrawInstructionChanger();
    DrawCodeCaveBuilder();
    DrawLabelBrowser();
    DrawBreakpointConditionEditor();
    m_detached_rendering = false;
}
