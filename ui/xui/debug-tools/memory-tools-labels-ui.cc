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

void MemoryToolsWindow::DrawLabelBrowser()
{
    if (!m_label_browser_open) {
        return;
    }
    if (m_label_browser_focus_requested) {
        ImGui::SetNextWindowFocus();
        ImGui::SetNextWindowSize(ImVec2(1040.0f, 560.0f), ImGuiCond_Appearing);
        m_label_browser_focus_requested = false;
    }

    if (!ImGui::Begin("x86 Current Labels", &m_label_browser_open,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    const auto &database = current_game_manager.Labels();
    ImGui::Text("Current labels: %zu", database.labels.size());
    ImGui::SameLine();
    ImGui::TextDisabled("XBE/section location is persistent; Physical is resolved live.");

    if (ImGui::Button("LABELS FOLDER")) {
        std::string error;
        if (!current_game_manager.EnsureLabelDirectories(error)) {
            m_label_status = error;
        } else {
            const std::string root = current_game_manager.LabelRootDirectory();
            GError *uri_error = nullptr;
            gchar *absolute = g_canonicalize_filename(root.c_str(), nullptr);
            gchar *uri = absolute
                             ? g_filename_to_uri(absolute, nullptr, &uri_error)
                             : nullptr;
            if (uri != nullptr && SDL_OpenURL(uri)) {
                m_label_status = "Opened Labels folder: " + root;
            } else {
                m_label_status = "Unable to open Labels folder: ";
                m_label_status += uri_error ? uri_error->message : SDL_GetError();
            }
            if (uri_error) g_error_free(uri_error);
            g_free(uri);
            g_free(absolute);
        }
        m_debug_status = m_label_status;
    }
    ImGui::SameLine();
    if (ImGui::Button("LOAD .xlabel")) {
        std::string error;
        current_game_manager.EnsureLabelDirectories(error);
        static const SDL_DialogFileFilter filters[] = {
            {"XEMU Label Packs (*.xlabel)", "xlabel"},
            {"All Files", "*"},
        };
        const std::string dir = current_game_manager.LabelPackDirectory();
        ShowOpenFileDialog(filters, 2, dir.c_str(), [this](const char *path) {
            if (path != nullptr && path[0] != '\0') {
                current_game_manager.LoadLabelPackFile(path);
                m_label_status = current_game_manager.LabelStatus();
                m_debug_status = m_label_status;
            }
        });
    }
    ImGui::SameLine();
    if (ImGui::Button("LOAD .map")) {
        std::string error;
        current_game_manager.EnsureLabelDirectories(error);
        static const SDL_DialogFileFilter filters[] = {
            {"Microsoft Linker MAP (*.map)", "map"},
            {"All Files", "*"},
        };
        const std::string dir = current_game_manager.LabelPdbDirectory();
        ShowOpenFileDialog(filters, 2, dir.c_str(), [this](const char *path) {
            if (path != nullptr && path[0] != '\0') {
                current_game_manager.LoadMapFile(path);
                m_selected_label_index = -1;
                m_label_status = current_game_manager.LabelStatus();
                m_debug_status = m_label_status;
            }
        });
    }
    ImGui::SameLine();
    if (ImGui::Button("LOAD .pdb")) {
        std::string error;
        current_game_manager.EnsureLabelDirectories(error);
        static const SDL_DialogFileFilter filters[] = {
            {"Microsoft Program Database (*.pdb)", "pdb"},
            {"All Files", "*"},
        };
        const std::string dir = current_game_manager.LabelPdbDirectory();
        ShowOpenFileDialog(filters, 2, dir.c_str(), [this](const char *path) {
            if (path != nullptr && path[0] != '\0') {
                current_game_manager.LoadPdbFile(path);
                m_selected_label_index = -1;
                m_label_status = current_game_manager.LabelStatus();
                m_debug_status = m_label_status;
            }
        });
    }
    ImGui::SameLine();
    if (ImGui::Button("SAVE .xlabel")) {
        const std::string suggested =
            current_game_manager.SuggestedCurrentLabelPackPath();
        if (suggested.empty()) {
            m_label_status = "A running XBE is required before saving a label pack.";
            m_debug_status = m_label_status;
        } else {
            std::string error;
            current_game_manager.EnsureLabelDirectories(error);
            static const SDL_DialogFileFilter filters[] = {
                {"XEMU Label Packs (*.xlabel)", "xlabel"},
            };
            ShowSaveFileDialog(filters, 1, suggested.c_str(),
                               [this](const char *path) {
                if (path != nullptr && path[0] != '\0') {
                    std::string output(path);
                    if (output.size() < 7 ||
                        g_ascii_strcasecmp(output.c_str() + output.size() - 7,
                                           ".xlabel") != 0) {
                        output += ".xlabel";
                    }
                    current_game_manager.SaveLabelPackFile(output);
                    m_label_status = current_game_manager.LabelStatus();
                    m_debug_status = m_label_status;
                }
            });
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("RELOAD PACKS")) {
        current_game_manager.ReloadLabelPacks();
        m_selected_label_index = -1;
        m_label_status = current_game_manager.LabelStatus();
        m_debug_status = m_label_status;
    }
    ImGui::SameLine();
    if (ImGui::Button("BUILD / REFRESH XDK INDEX")) {
        current_game_manager.RefreshXdkLabels(true);
        m_selected_label_index = -1;
        const auto &xdk = current_game_manager.XdkStatus();
        m_label_status = xdk.message;
        m_debug_status = m_label_status;
    }

    const auto &xdk_status = current_game_manager.XdkStatus();
    if (xdk_status.build != 0) {
        ImGui::TextDisabled(
            "XDK %u | Libraries %zu/%zu | Signatures %zu | Exact labels %zu | Cache %s",
            (unsigned)xdk_status.build, xdk_status.found_libraries,
            xdk_status.required_libraries, xdk_status.signatures,
            xdk_status.exact_matches,
            xdk_status.cache_rebuilt ? "Rebuilt" :
            (xdk_status.cache_loaded ? "Loaded" :
             (xdk_status.cache_found ? "Found" : "Not Built")));
        if (!xdk_status.message.empty()) {
            ImGui::TextDisabled("%s", xdk_status.message.c_str());
        }
    }

    const auto &map_status = current_game_manager.MapStatus();
    if (map_status.parsed) {
        ImGui::TextDisabled(
            "MAP %08X (%s) | Symbols %zu | Sections %zu | Exact labels %zu",
            map_status.timestamp,
            map_status.timestamp_match ? "timestamp match" : "timestamp mismatch",
            map_status.parsed_symbols, map_status.mapped_segments,
            map_status.resolved_labels);
        if (!map_status.message.empty()) {
            ImGui::TextDisabled("%s", map_status.message.c_str());
        }
    }

    const auto &pdb_status = current_game_manager.PdbStatus();
    if (pdb_status.parsed) {
        ImGui::TextDisabled(
            "PDB GUID %s | Age %u (%s) | Publics %zu | Sections %zu | Exact labels %zu",
            pdb_status.guid_match ? "MATCH" : "MISMATCH",
            (unsigned)pdb_status.pdb_age,
            pdb_status.age_match ? "match" : "mismatch",
            pdb_status.public_symbols, pdb_status.mapped_sections,
            pdb_status.resolved_labels);
        if (!pdb_status.message.empty()) {
            ImGui::TextDisabled("%s", pdb_status.message.c_str());
        }
    }

    ImGui::SetNextItemWidth(340.0f);
    ImGui::InputTextWithHint("##label_search", "Search labels...",
                             m_label_search, sizeof(m_label_search));
    ImGui::SameLine();
    const char *filters[] = {
        "All", "Entry", "Section", "Kernel", "String", "XRef", "RTTI",
        "Inferred", "Function", "Symbol"
    };
    ImGui::SetNextItemWidth(130.0f);
    ImGui::Combo("Type##label_filter", &m_label_filter,
                 filters, IM_ARRAYSIZE(filters));
    ImGui::SameLine();
    const char *source_filters[] = {"All", "XBE", "XDK", "PDB", "MAP", "Manual"};
    ImGui::SetNextItemWidth(120.0f);
    ImGui::Combo("Source##label_source_filter", &m_label_source_filter,
                 source_filters, IM_ARRAYSIZE(source_filters));
    ImGui::SameLine();
    if (ImGui::Button("DUMP LABELS")) {
        DumpLabels();
    }

    ImGui::TextDisabled(".xlabel packs are matched like Cheat files and store section-relative locations, never launch-specific Physical addresses.");
    ImGui::TextDisabled("XDK indexes are generated locally from Labels/XDK and store symbol names/fingerprints only; original XDK code is never stored in the cache or .xlabel pack.");
    ImGui::TextDisabled("MAP imports are accepted only when their linker timestamp and segment layout match the current XBE; mismatched builds are never applied silently.");
    ImGui::TextDisabled("PDB imports require an exact RSDS GUID + Age match and compatible section layout; same-GUID/different-Age PDBs are identified but not applied.");
    ImGui::TextDisabled("Inferred labels begin with '~' and are heuristics from function-like XBE strings/xrefs, not original PDB/MAP symbols.");
    ImGui::Separator();

    const uint64_t label_generation = current_game_manager.LabelGeneration();
    const bool filter_changed =
        m_visible_label_generation != label_generation ||
        m_visible_label_filter != m_label_filter ||
        m_visible_label_source_filter != m_label_source_filter ||
        std::strcmp(m_visible_label_search, m_label_search) != 0;
    if (filter_changed) {
        m_visible_label_cache.clear();
        m_visible_label_cache.reserve(database.labels.size());
        for (size_t i = 0; i < database.labels.size(); ++i) {
            const XemuXbeLabels::Label &label = database.labels[i];
            if (m_label_filter != 0 &&
                (int)label.type != m_label_filter - 1) {
                continue;
            }
            if (m_label_source_filter != 0 &&
                (int)label.source != m_label_source_filter - 1) {
                continue;
            }
            if (!ascii_contains_case_insensitive(label.name, m_label_search) &&
                !ascii_contains_case_insensitive(
                    XemuXbeLabels::TypeName(label.type), m_label_search) &&
                !ascii_contains_case_insensitive(
                    XemuXbeLabels::SourceName(label.source), m_label_search) &&
                !ascii_contains_case_insensitive(
                    XemuXbeLabels::ConfidenceName(label.confidence), m_label_search)) {
                continue;
            }
            m_visible_label_cache.push_back(i);
        }
        m_visible_label_generation = label_generation;
        m_visible_label_filter = m_label_filter;
        m_visible_label_source_filter = m_label_source_filter;
        g_strlcpy(m_visible_label_search, m_label_search,
                  sizeof(m_visible_label_search));
    }
    const std::vector<size_t> &visible_labels = m_visible_label_cache;

    const bool can_translate = !database.labels.empty() &&
                               xemu_cheat_prepare_virtual_map() != 0;

    /* Physical label addresses are intentionally resolved live. While the
     * guest is stopped, the prepared page-table state cannot advance during
     * this DrawLabelBrowser() call, so visible labels sharing a 4 KiB Virtual
     * page may reuse that page translation. While running, keep the historical
     * direct-per-address translation path so no mapping change is hidden. */
    const bool cache_physical_pages = can_translate && !runstate_is_running();
    struct LabelPageTranslation {
        uint32_t virtual_page = 0;
        uint64_t physical_page = 0;
        bool physical_valid = false;
    };
    std::array<LabelPageTranslation, 32> label_page_translations = {};
    size_t label_page_translation_count = 0;

    auto translate_label_address = [&](uint32_t address,
                                       uint64_t &physical) -> bool {
        if (!can_translate) {
            return false;
        }
        if (!cache_physical_pages) {
            return xemu_cheat_virtual_to_physical(address, &physical) != 0;
        }

        const uint32_t virtual_page = address & 0xFFFFF000u;
        for (size_t i = 0; i < label_page_translation_count; ++i) {
            const LabelPageTranslation &cached = label_page_translations[i];
            if (cached.virtual_page == virtual_page) {
                if (cached.physical_valid) {
                    physical = cached.physical_page +
                               (uint64_t)(address - virtual_page);
                }
                return cached.physical_valid;
            }
        }

        uint64_t physical_page = 0;
        const bool physical_valid =
            xemu_cheat_virtual_to_physical(virtual_page, &physical_page) != 0;
        if (label_page_translation_count < label_page_translations.size()) {
            LabelPageTranslation &cached =
                label_page_translations[label_page_translation_count++];
            cached.virtual_page = virtual_page;
            cached.physical_page = physical_page;
            cached.physical_valid = physical_valid;
        }
        if (physical_valid) {
            physical = physical_page + (uint64_t)(address - virtual_page);
        }
        return physical_valid;
    };

    if (ImGui::BeginTable("current_label_table", 5,
                          ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_BordersInnerV |
                          ImGuiTableFlags_ScrollY |
                          ImGuiTableFlags_SizingStretchProp,
                          ImVec2(0.0f, 360.0f))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 85.0f);
        ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Virtual", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Physical", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin((int)visible_labels.size());
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const size_t label_index = visible_labels[(size_t)row];
                const XemuXbeLabels::Label &label = database.labels[label_index];
                uint64_t physical = 0;
                const bool physical_valid =
                    translate_label_address(label.virtual_address, physical);

                ImGui::TableNextRow();
                ImGui::PushID((int)label_index);
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(XemuXbeLabels::TypeName(label.type));
                if (label.type == XemuXbeLabels::Type::Inferred &&
                    ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Auto-inferred from a function-like XBE string/xref; not a confirmed PDB symbol.");
                }

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(XemuXbeLabels::SourceName(label.source));
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Source: %s\nConfidence: %s",
                                      XemuXbeLabels::SourceName(label.source),
                                      XemuXbeLabels::ConfidenceName(label.confidence));
                }

                ImGui::TableSetColumnIndex(2);
                const bool selected = m_selected_label_index == (int)label_index;
                if (ImGui::Selectable(label.name.c_str(), selected,
                                      ImGuiSelectableFlags_SpanAllColumns |
                                      ImGuiSelectableFlags_AllowDoubleClick)) {
                    m_selected_label_index = (int)label_index;
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        FollowDebuggerAddress(label.virtual_address, true);
                        m_debug_status = "Jumped to label " + label.name;
                    }
                }
                if (ImGui::BeginPopupContextItem("label_context")) {
                    if (ImGui::MenuItem("Jump Virtual")) {
                        FollowDebuggerAddress(label.virtual_address, true);
                    }
                    if (!physical_valid) {
                        ImGui::BeginDisabled();
                    }
                    if (ImGui::MenuItem("Jump Physical")) {
                        FollowDebuggerAddress(label.virtual_address, true);
                        char message[160];
                        std::snprintf(message, sizeof(message),
                                      "Jumped to %s at current Physical %08llX",
                                      label.name.c_str(),
                                      (unsigned long long)physical);
                        m_debug_status = message;
                    }
                    if (!physical_valid) {
                        ImGui::EndDisabled();
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Copy Label")) {
                        ImGui::SetClipboardText(label.name.c_str());
                    }
                    char address_text[32];
                    std::snprintf(address_text, sizeof(address_text), "%08X",
                                  label.virtual_address);
                    if (ImGui::MenuItem("Copy Virtual Address")) {
                        ImGui::SetClipboardText(address_text);
                    }
                    if (!physical_valid) {
                        ImGui::BeginDisabled();
                    }
                    std::snprintf(address_text, sizeof(address_text), "%08llX",
                                  (unsigned long long)physical);
                    if (ImGui::MenuItem("Copy Physical Address")) {
                        ImGui::SetClipboardText(address_text);
                    }
                    if (!physical_valid) {
                        ImGui::EndDisabled();
                    }
                    ImGui::EndPopup();
                }

                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%08X", label.virtual_address);
                ImGui::TableSetColumnIndex(4);
                if (physical_valid) {
                    ImGui::Text("%08llX", (unsigned long long)physical);
                } else {
                    ImGui::TextDisabled("UNMAPPED");
                }
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }

    const XemuXbeLabels::Label *selected_label = nullptr;
    uint64_t selected_physical = 0;
    bool selected_physical_valid = false;
    if (m_selected_label_index >= 0 &&
        (size_t)m_selected_label_index < database.labels.size()) {
        selected_label = &database.labels[(size_t)m_selected_label_index];
        selected_physical_valid =
            translate_label_address(selected_label->virtual_address,
                                    selected_physical);
    } else {
        m_selected_label_index = -1;
    }

    if (selected_label == nullptr) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("JUMP VIRTUAL", ImVec2(125.0f, 0.0f)) && selected_label) {
        FollowDebuggerAddress(selected_label->virtual_address, true);
        m_debug_status = "Jumped to label " + selected_label->name;
    }
    if (selected_label == nullptr) {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    if (selected_label == nullptr || !selected_physical_valid) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("JUMP PHYSICAL", ImVec2(125.0f, 0.0f)) && selected_label) {
        // The paired debugger is Virtual-mastered. Navigating the label's
        // stable Virtual address positions the Physical pane at its current
        // backing address without baking a launch-specific mapping into the DB.
        FollowDebuggerAddress(selected_label->virtual_address, true);
        char message[192];
        std::snprintf(message, sizeof(message),
                      "Jumped to %s at current Physical %08llX",
                      selected_label->name.c_str(),
                      (unsigned long long)selected_physical);
        m_debug_status = message;
    }
    if (selected_label == nullptr || !selected_physical_valid) {
        ImGui::EndDisabled();
    }
    if (selected_label != nullptr) {
        ImGui::SameLine();
        if (selected_physical_valid) {
            ImGui::TextDisabled("Selected V %08X -> P %08llX",
                                selected_label->virtual_address,
                                (unsigned long long)selected_physical);
        } else {
            ImGui::TextDisabled("Selected V %08X -> P UNMAPPED",
                                selected_label->virtual_address);
        }
    }

    if (!m_label_status.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", m_label_status.c_str());
    }
    ImGui::End();
}
