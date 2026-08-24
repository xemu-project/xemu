//
// xemu Memory Viewer / Search / x86 Debugger - Memory Search UI
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
#include "../misc.hh"

#include <algorithm>
#include <cstdio>

using namespace xemu_memory_tools_internal;

void MemoryToolsWindow::DrawSearch()
{
    int space = (int)m_search_space;
    if (ImGui::Combo("Address Space", &space, "Physical\0Virtual\0")) {
        m_search_space = (AddressSpace)space;
        ResetSearch();
    }

    ImGui::SameLine();
    if (ImGui::Button("Physical 64 MB")) {
        m_search_space = AddressSpace::Physical;
        m_scan_start = 0x00000000u;
        m_scan_end = 0x03FFFFFFu;
        SetHexText(m_scan_start_text, sizeof(m_scan_start_text), m_scan_start);
        SetHexText(m_scan_end_text, sizeof(m_scan_end_text), m_scan_end);
        ResetSearch();
    }
    ImGui::SameLine();
    if (ImGui::Button("Physical 128 MB")) {
        m_search_space = AddressSpace::Physical;
        m_scan_start = 0x00000000u;
        m_scan_end = 0x07FFFFFFu;
        SetHexText(m_scan_start_text, sizeof(m_scan_start_text), m_scan_start);
        SetHexText(m_scan_end_text, sizeof(m_scan_end_text), m_scan_end);
        ResetSearch();
    }
    ImGui::SameLine();
    if (ImGui::Button("VA 80000000-83FFFFFF")) {
        m_search_space = AddressSpace::Virtual;
        m_scan_start = 0x80000000u;
        m_scan_end = 0x83FFFFFFu;
        SetHexText(m_scan_start_text, sizeof(m_scan_start_text), m_scan_start);
        SetHexText(m_scan_end_text, sizeof(m_scan_end_text), m_scan_end);
        ResetSearch();
    }

    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputText("Start", m_scan_start_text, sizeof(m_scan_start_text),
                     ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputText("End", m_scan_end_text, sizeof(m_scan_end_text),
                     ImGuiInputTextFlags_CharsHexadecimal);

    int kind = (int)m_value_kind;
    ImGui::SetNextItemWidth(110.0f);
    if (ImGui::Combo("Value Type", &kind,
                     "UInt8\0UInt16\0UInt32\0Int8\0Int16\0Int32\0Float32\0")) {
        m_value_kind = (ValueKind)kind;
        ResetSearch();
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Aligned", &m_aligned)) {
        ResetSearch();
    }
    if (m_value_kind != ValueKind::Float32) {
        ImGui::SameLine();
        ImGui::Checkbox("Hex Value", &m_value_hex);
    }

    if (!m_have_first_scan) {
        int mode = (int)m_first_mode;
        ImGui::SetNextItemWidth(150.0f);
        ImGui::Combo("Mode##first_scan_mode", &mode,
                     "Exact Value\0Unknown Initial\0");
        m_first_mode = (FirstScanMode)mode;
        if (m_first_mode == FirstScanMode::Exact) {
            ImGui::SetNextItemWidth(160.0f);
            ImGui::InputText("Value", m_search_value_text,
                             sizeof(m_search_value_text));
        }
        if (ImGui::Button("First Scan##first_scan_button")) {
            uint32_t start, end;
            if (ParseHexAddress(m_scan_start_text, start) &&
                ParseHexAddress(m_scan_end_text, end)) {
                m_scan_start = start;
                m_scan_end = end;
            }
            FirstScan();
        }
    } else {
        int mode = (int)m_next_mode;
        ImGui::SetNextItemWidth(150.0f);
        ImGui::Combo("Compare", &mode,
                     "Exact Value\0Not Equal Value\0Changed\0Unchanged\0Increased\0Decreased\0Greater Than Value\0Less Than Value\0");
        m_next_mode = (NextScanMode)mode;
        if (m_next_mode == NextScanMode::Exact ||
            m_next_mode == NextScanMode::NotEqual ||
            m_next_mode == NextScanMode::GreaterThan ||
            m_next_mode == NextScanMode::LessThan) {
            ImGui::SetNextItemWidth(160.0f);
            ImGui::InputText("Value", m_search_value_text,
                             sizeof(m_search_value_text));
        }
        if (ImGui::Button("Next Scan")) {
            NextScan();
        }
        ImGui::SameLine();
        if (ImGui::Button("New Scan")) {
            ResetSearch();
        }
    }

    if (!m_search_status.empty()) {
        ImGui::TextWrapped("%s", m_search_status.c_str());
    }

    ImGui::Separator();
    if (m_snapshot_mode) {
        ImGui::Text("Snapshot ready: %zu bytes", m_snapshot.size());
        ImGui::TextUnformatted("Change the value in-game, choose a comparison, then press Next Scan.");
        return;
    }

    ImGui::Text("Results: %zu", m_results.size());
    if (m_results.size() > kMaxDisplayedResults) {
        ImGui::SameLine();
        ImGui::Text("(showing first %zu)", kMaxDisplayedResults);
    }

    if (ImGui::BeginTable("search_results", 3,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_ScrollY,
                          ImVec2(0, 300.0f))) {
        ImGui::TableSetupColumn("Address");
        ImGui::TableSetupColumn("Previous");
        ImGui::TableSetupColumn("Current");
        ImGui::TableHeadersRow();

        const size_t count = std::min(m_results.size(), kMaxDisplayedResults);
        ImGuiListClipper clipper;
        clipper.Begin((int)count);
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                const size_t result_index = (size_t)i;
                const SearchResult &r = m_results[result_index];

                /* The clipper normally revisits the same small row window for
                 * many frames. Reuse formatting only when the complete source
                 * tuple is identical; scan mutations and value-kind changes
                 * therefore self-invalidate without a separate generation. */
                SearchDisplayCacheEntry &display =
                    m_search_display_cache[result_index %
                                           m_search_display_cache.size()];
                if (display.result_index != result_index ||
                    display.address != r.address ||
                    display.previous_raw != r.previous_raw ||
                    display.current_raw != r.current_raw ||
                    display.value_kind != m_value_kind) {
                    display.result_index = result_index;
                    display.address = r.address;
                    display.previous_raw = r.previous_raw;
                    display.current_raw = r.current_raw;
                    display.value_kind = m_value_kind;
                    std::snprintf(display.address_text,
                                  sizeof(display.address_text), "%08X",
                                  r.address);
                    FormatValue(display.previous_text,
                                sizeof(display.previous_text),
                                r.previous_raw, m_value_kind);
                    FormatValue(display.current_text,
                                sizeof(display.current_text),
                                r.current_raw, m_value_kind);
                }

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::PushID(i);
                if (ImGui::Selectable(display.address_text, false,
                                      ImGuiSelectableFlags_AllowDoubleClick)) {
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        JumpViewerTo(m_search_space, r.address);
                        SelectMemoryByte(m_search_space, r.address);
                        m_request_memory_tab = true;
                    }
                }
                DrawAddressContextMenu(m_search_space, r.address,
                                       ContextOrigin::Search,
                                       display.current_text);
                ImGui::PopID();
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(display.previous_text);
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(display.current_text);
            }
        }
        ImGui::EndTable();
    }
}
