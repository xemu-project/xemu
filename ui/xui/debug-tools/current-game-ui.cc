//
// xemu Current Game Manager UI
//
// Copyright (C) 2026 xemu contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#include "current-game.hh"
#include "tab-style.hh"
#include "hdd-directory.hh"
#include "guest-kernel-rpc.hh"
#include "../font-manager.hh"

#include <string>
#include <vector>

void CurrentGameManager::DrawInlineSummary(const char *id) const
{
    ImGui::PushID(id);
    if (!m_info.valid) {
        ImGui::TextDisabled("Current Game: No XBE detected");
        ImGui::PopID();
        return;
    }

    const std::string title_id = FormatTitleId(m_info.title_id);
    const char *name = m_info.title_name.empty() ? "Unknown title" : m_info.title_name.c_str();
    ImGui::Text("Current Game: %s", name);
    ImGui::SameLine();
    ImGui::TextDisabled("[%s]", title_id.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("Rev %08X / Disc %u", m_info.version,
                        m_info.disc_number);
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Region: %s", FormatRegion(m_info.region).c_str());
        ImGui::Text("Loaded Header SHA-256: %s", m_info.header_sha256.c_str());
        if (!m_info.disc_xbe_sha256.empty()) {
            ImGui::Text("Disc default.xbe SHA-256: %s",
                        m_info.disc_xbe_sha256.c_str());
        }
        ImGui::Text("Revision key: %s", m_info.revision_key.c_str());
        ImGui::EndTooltip();
    }
    ImGui::PopID();
}
void CurrentGameManager::DrawGameInfoTab(bool detached)
{
    if (!m_info.valid) {
        ImGui::TextUnformatted("No running XBE detected.");
        ImGui::TextDisabled("Start a game or dashboard XBE and it will appear here automatically.");
        if (!m_info.disc_xbe_sha256.empty()) {
            ImGui::Separator();
            ImGui::TextWrapped("Disc default.xbe SHA-256   %s",
                               m_info.disc_xbe_sha256.c_str());
        }
        return;
    }

    const bool use_main_fixed_font = !detached && g_font_mgr.m_fixed_width_font != nullptr;
    if (use_main_fixed_font) {
        ImGui::PushFont(g_font_mgr.m_fixed_width_font);
    }
    ImGui::Text("Title Name       %s", m_info.title_name.empty() ? "<unknown>" : m_info.title_name.c_str());
    ImGui::Text("Title ID         %s", FormatTitleId(m_info.title_id).c_str());
    ImGui::Text("Database GameID  %s", FormatDatabaseGameId(m_info.title_id).c_str());
    ImGui::Text("Region           %s", FormatRegion(m_info.region).c_str());
    ImGui::Text("Disc Number      %u", m_info.disc_number);
    ImGui::Text("Version          %08X", m_info.version);
    ImGui::Text("XBE Base         %08X", m_info.xbe_base);
    ImGui::Text("XBE Image Size   %08X", m_info.xbe_image_size);
    ImGui::Text("PE Checksum      %08X", m_info.pe_checksum);
    ImGui::Text("PE Timestamp     %08X", m_info.pe_timestamp);
    ImGui::Separator();
    ImGui::TextWrapped("Header SHA-256   %s", m_info.header_sha256.c_str());
    ImGui::TextWrapped("XBE SHA-256      %s",
                       m_info.disc_xbe_sha256.empty()
                           ? "<default.xbe unavailable>"
                           : m_info.disc_xbe_sha256.c_str());
    if (!m_info.disc_xbe_sha256.empty()) {
        ImGui::Text("XBE Disc Size    %08X", m_info.disc_xbe_size);
        ImGui::Text("XBE Start Sector %08X", m_info.disc_xbe_start_sector);
        ImGui::Text("XBE Disc Offset  %016llX",
                    (unsigned long long)m_info.disc_xbe_offset);
    }
    if (m_xbe_pdb_identity.valid) {
        ImGui::Separator();
        const std::string pdb_guid = XemuPdbLabels::FormatGuid(m_xbe_pdb_identity.guid);
        ImGui::TextWrapped("XBE PDB Path     %s", m_xbe_pdb_identity.path.c_str());
        ImGui::Text("XBE PDB GUID     %s", pdb_guid.c_str());
        ImGui::Text("XBE PDB Age      %u", (unsigned)m_xbe_pdb_identity.age);
    }
    ImGui::Text("Revision Key     %s", m_info.revision_key.c_str());
    ImGui::Text("Current Labels   %zu", m_labels.labels.size());
    ImGui::Text("Label Packs      %zu", m_loaded_label_packs.size());
    if (m_xdk_status.build != 0) {
        ImGui::Separator();
        ImGui::Text("XDK Build        %u", (unsigned)m_xdk_status.build);
        ImGui::Text("XDK Libraries    %zu/%zu", m_xdk_status.found_libraries,
                    m_xdk_status.required_libraries);
        ImGui::Text("XDK Signatures   %zu", m_xdk_status.signatures);
        ImGui::Text("XDK Exact Labels %zu", m_xdk_status.exact_matches);
        ImGui::Text("XDK Cache        %s",
                    m_xdk_status.cache_rebuilt ? "Rebuilt" :
                    (m_xdk_status.cache_loaded ? "Loaded" :
                     (m_xdk_status.cache_found ? "Found" : "Not Built")));
    }
    if (m_map_status.parsed) {
        ImGui::Separator();
        ImGui::Text("MAP Timestamp    %08X (%s)", m_map_status.timestamp,
                    m_map_status.timestamp_match ? "MATCH" : "MISMATCH");
        ImGui::Text("MAP Symbols      %zu", m_map_status.parsed_symbols);
        ImGui::Text("MAP Sections     %zu", m_map_status.mapped_segments);
        ImGui::Text("MAP Exact Labels %zu", m_map_status.resolved_labels);
        if (!m_loaded_map_path.empty()) {
            ImGui::TextWrapped("MAP File         %s", m_loaded_map_path.c_str());
        }
    }
    if (m_pdb_status.parsed) {
        ImGui::Separator();
        ImGui::Text("PDB GUID         %s (%s)", m_pdb_status.pdb_guid.c_str(),
                    m_pdb_status.guid_match ? "MATCH" : "MISMATCH");
        ImGui::Text("PDB Age          %u / XBE %u (%s)",
                    (unsigned)m_pdb_status.pdb_age,
                    (unsigned)m_pdb_status.xbe_age,
                    m_pdb_status.age_match ? "MATCH" : "MISMATCH");
        ImGui::Text("PDB Publics      %zu", m_pdb_status.public_symbols);
        ImGui::Text("PDB Sections     %zu (%s)", m_pdb_status.mapped_sections,
                    m_pdb_status.layout_match ? "MATCH" : "MISMATCH");
        ImGui::Text("PDB Exact Labels %zu", m_pdb_status.resolved_labels);
        if (!m_loaded_pdb_path.empty()) {
            ImGui::TextWrapped("PDB File         %s", m_loaded_pdb_path.c_str());
        }
    }
    if (use_main_fixed_font) {
        ImGui::PopFont();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Header SHA-256 hashes the loaded in-memory XBE header block.");
    ImGui::TextDisabled("XBE SHA-256 hashes the complete default.xbe file from the currently mounted DVD.");
    ImGui::TextDisabled(".xlabel packs use XBE section-relative locations; Physical addresses are resolved live and are never persisted.");
    if (!m_disc_status.empty()) {
        ImGui::TextDisabled("Disc: %s", m_disc_status.c_str());
    }
    if (!m_label_status.empty()) {
        ImGui::TextDisabled("Labels: %s", m_label_status.c_str());
    }
    if (!m_xdk_status.message.empty()) {
        ImGui::TextDisabled("XDK: %s", m_xdk_status.message.c_str());
    }
    if (!m_map_status.message.empty()) {
        ImGui::TextDisabled("MAP: %s", m_map_status.message.c_str());
    }
    if (!m_pdb_status.message.empty()) {
        ImGui::TextDisabled("PDB: %s", m_pdb_status.message.c_str());
    }
}
void CurrentGameManager::DrawDiscEntry(const XemuXdvdfs::Entry &entry,
                                       std::vector<std::string> &path)
{
    std::vector<std::string> entry_path = path;
    entry_path.push_back(entry.name);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                               ImGuiTreeNodeFlags_OpenOnDoubleClick |
                               ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!entry.IsDirectory()) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    const bool open = ImGui::TreeNodeEx((const void *)&entry, flags,
                                        "%s", entry.name.c_str());
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::BeginMenu("Export")) {
            const char *label = entry.IsDirectory() ? "Export Folder..." : "Export File...";
            if (ImGui::MenuItem(label)) {
                RequestDiscExport(entry, entry_path);
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Copy Disc Path")) {
            std::string disc_path = "D:\\";
            for (size_t i = 0; i < entry_path.size(); ++i) {
                if (i != 0) {
                    disc_path += "\\";
                }
                disc_path += entry_path[i];
            }
            ImGui::SetClipboardText(disc_path.c_str());
            m_disc_export_status = "Copied disc path: " + disc_path;
        }
        ImGui::TextDisabled("Read-only XDVDFS export; the disc image is never modified.");
        ImGui::EndPopup();
    }

    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted(entry.IsDirectory() ? "Folder" : "File");

    ImGui::TableSetColumnIndex(2);
    const std::string size = FormatByteSize(entry.size);
    ImGui::TextUnformatted(entry.IsDirectory() ? "-" : size.c_str());

    ImGui::TableSetColumnIndex(3);
    ImGui::Text("%08X", entry.start_sector);

    ImGui::TableSetColumnIndex(4);
    ImGui::Text("%016llX", (unsigned long long)entry.disc_offset);

    if (entry.IsDirectory() && open) {
        path.push_back(entry.name);
        for (const XemuXdvdfs::Entry &child : entry.children) {
            DrawDiscEntry(child, path);
        }
        path.pop_back();
        ImGui::TableSetColumnIndex(0);
        ImGui::TreePop();
    }
}
void CurrentGameManager::DrawDiscContentsTab()
{
    if (!m_disc.valid) {
        ImGui::TextUnformatted("Disc contents are not available.");
        if (!m_disc_status.empty()) {
            ImGui::TextWrapped("%s", m_disc_status.c_str());
        }
        ImGui::Spacing();
        ImGui::TextDisabled("This viewer reads the XDVDFS filesystem directly from the currently mounted ide0-cd1 DVD backend.");
        return;
    }

    ImGui::Text("Xbox DVD (D:\\)   %zu files / %zu folders",
                m_disc.file_count, m_disc.directory_count);
    ImGui::SameLine();
    ImGui::TextDisabled("XDVDFS base %016llX",
                        (unsigned long long)m_disc.filesystem_base);
    if (!m_info.disc_xbe_sha256.empty()) {
        ImGui::TextDisabled("default.xbe SHA-256: %s",
                            m_info.disc_xbe_sha256.c_str());
    }
    ImGui::Separator();
    ImGui::TextDisabled("Right-click a disc file/folder to Export or copy its D:\\ path.");
    if (!m_disc_export_status.empty()) {
        ImGui::TextWrapped("%s", m_disc_export_status.c_str());
    }

    const ImGuiTableFlags table_flags = ImGuiTableFlags_BordersInnerV |
                                        ImGuiTableFlags_BordersOuter |
                                        ImGuiTableFlags_RowBg |
                                        ImGuiTableFlags_Resizable |
                                        ImGuiTableFlags_ScrollY |
                                        ImGuiTableFlags_SizingStretchProp;
    if (ImGui::BeginTable("##xdvdfs_contents", 5, table_flags,
                          ImVec2(0.0f, 0.0f))) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 3.0f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 95.0f);
        ImGui::TableSetupColumn("Sector", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Disc Offset", ImGuiTableColumnFlags_WidthFixed, 145.0f);
        ImGui::TableHeadersRow();

        std::vector<std::string> path;
        for (const XemuXdvdfs::Entry &entry : m_disc.root_entries) {
            DrawDiscEntry(entry, path);
        }
        ImGui::EndTable();
    }
}
void CurrentGameManager::Draw(bool detached)
{
    if (!is_open) {
        return;
    }

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
    const char *window_name = "Current Game";
    bool *window_open = &is_open;
    if (detached) {
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
        flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
        window_name = "##DetachedCurrentGame";
        window_open = nullptr;
    } else {
        ImGui::SetNextWindowSize(ImVec2(860, 520), ImGuiCond_FirstUseEver);
    }
    if (!ImGui::Begin(window_name, window_open, flags)) {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Refresh Now")) {
        Refresh(true);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Reads the running XBE and currently mounted Xbox DVD");
    ImGui::Separator();

    XemuDebugUi::ScopedTabStyle tab_style;
    if (ImGui::BeginTabBar("##current_game_tabs")) {
        if (ImGui::BeginTabItem("Game Info")) {
            DrawGameInfoTab(detached);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Disc Contents")) {
            DrawDiscContentsTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("HDD")) {
            hdd_directory_window.DrawCurrentGameHdd(
                m_info.valid ? m_info.title_id : 0);
            ImGui::EndTabItem();
        }
        if (m_show_kernel_rpc_diagnostics &&
            ImGui::BeginTabItem("Kernel RPC Diagnostics")) {
            guest_kernel_rpc_manager.DrawTestPanel();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    tab_style.Restore();

    ImGui::Separator();
    ImGui::Checkbox("Show Kernel RPC diagnostics",
                    &m_show_kernel_rpc_diagnostics);
    if (!m_show_kernel_rpc_diagnostics) {
        ImGui::SameLine();
        ImGui::TextDisabled("Advanced/developer filesystem RPC test surface");
    }

    ImGui::End();
}
