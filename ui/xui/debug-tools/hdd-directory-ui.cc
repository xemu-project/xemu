//
// xemu Xbox HDD Directory Viewer - rendering / popup ownership
//
// Copyright (C) 2026 xemu contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#include "hdd-directory.hh"
#include "tab-style.hh"

#include "guest-kernel-rpc.hh"
#include "hdd-snapshot-service.hh"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <utility>

namespace {

using XemuKernelFs::EqualsNoCase;
using XemuKernelFs::FindChildNoCase;

bool ContainsNoCase(const std::string &text, const char *filter)
{
    if (!filter || filter[0] == '\0') {
        return true;
    }
    const size_t filter_len = std::strlen(filter);
    if (filter_len > text.size()) {
        return false;
    }
    for (size_t start = 0; start + filter_len <= text.size(); ++start) {
        bool same = true;
        for (size_t i = 0; i < filter_len; ++i) {
            if (std::tolower(static_cast<unsigned char>(text[start + i])) !=
                std::tolower(static_cast<unsigned char>(filter[i]))) {
                same = false;
                break;
            }
        }
        if (same) {
            return true;
        }
    }
    return false;
}

bool EntryOrDescendantMatches(const XemuFatxHdd::Entry &entry,
                              const char *filter)
{
    if (ContainsNoCase(XemuFatxHdd::DisplayName(entry), filter)) {
        return true;
    }
    if (entry.directory) {
        for (const XemuFatxHdd::Entry &child : entry.children) {
            if (EntryOrDescendantMatches(child, filter)) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

void HddDirectoryWindow::DrawDeleteConfirmation()
{
    if (!m_delete_pending) {
        m_delete_popup_open_requested = false;
        return;
    }

    if (m_delete_popup_open_requested) {
        std::string error;
        std::vector<XemuKernelFs::DeleteEntry> fresh_plan;
        if (!guest_kernel_rpc_manager.PrepareHddDelete(
                m_delete_target.partition, m_delete_target.path,
                m_delete_target.directory, fresh_plan, error)) {
            m_operation_status = "Delete preflight failed: " + error;
            m_kernel_status_visible = true;
            m_delete_pending = false;
            m_delete_popup_open_requested = false;
            m_delete_confirm_plan.clear();
            return;
        }
        m_delete_confirm_plan = std::move(fresh_plan);
        ImGui::OpenPopup("Confirm Xbox Kernel HDD Delete");
        m_delete_popup_open_requested = false;
    }

    ImGui::SetNextWindowSize(ImVec2(600, 0), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Confirm Xbox Kernel HDD Delete", nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    const std::string target_path = XemuKernelFs::FatxPathForPartition(
        m_delete_target.partition, m_delete_target.path);
    const XemuKernelFs::DeletePlanSummary summary =
        XemuKernelFs::SummarizeDeletePlan(m_delete_confirm_plan);

    ImGui::TextWrapped("Permanently delete '%s' through the Xbox kernel/FATX driver?",
                       m_delete_display.c_str());
    ImGui::TextWrapped("Target: %s", target_path.c_str());
    ImGui::Spacing();
    if (m_delete_target.directory) {
        ImGui::Text("Planned contents: %u file(s), %u folder(s)",
                    summary.file_count, summary.directory_count);
        ImGui::TextWrapped(
            "The directory tree is deleted leaf-first. Every file and directory is a separate PASSIVE_LEVEL-gated Xbox-kernel operation and is verified against a fresh FATX snapshot.");
    } else {
        ImGui::TextWrapped("This file will be permanently deleted through the Xbox kernel.");
    }
    if (m_delete_target.partition == 'E' && m_delete_target.path.size() == 2u &&
        (EqualsNoCase(m_delete_target.path[0], "UDATA") ||
         EqualsNoCase(m_delete_target.path[0], "TDATA"))) {
        ImGui::Spacing();
        ImGui::TextWrapped(
            "WARNING: This is the complete Title-ID directory. Delete All removes every item below this title root.");
    }
    ImGui::Spacing();
    ImGui::TextDisabled(
        "No raw FATX fallback is used and no Xbox restart is requested. If the fresh plan changes after confirmation, nothing is deleted and confirmation is required again.");
    ImGui::Separator();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.10f, 0.10f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.15f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.55f, 0.05f, 0.05f, 1.0f));
    const bool confirm = ImGui::Button(
        m_delete_target.directory ? "DELETE VIA XBOX KERNEL" : "DELETE FILE VIA XBOX KERNEL");
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    const bool cancel = ImGui::Button("CANCEL");

    if (confirm) {
        const HddTarget target = m_delete_target;
        std::vector<XemuKernelFs::DeleteEntry> confirmed_plan =
            std::move(m_delete_confirm_plan);
        m_delete_pending = false;
        ImGui::CloseCurrentPopup();
        m_kernel_status_visible = true;
        if (guest_kernel_rpc_manager.StartHddDelete(
                target.partition, target.path, target.directory, confirmed_plan)) {
            m_operation_status = "Xbox Kernel Delete started for " + target_path + ".";
        } else {
            m_operation_status = "Delete did not start: " + guest_kernel_rpc_manager.Status();
        }
    } else if (cancel) {
        m_delete_pending = false;
        m_delete_confirm_plan.clear();
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}
void HddDirectoryWindow::DrawExportContext(
    const XemuFatxHdd::Partition &partition, const XemuFatxHdd::Entry &entry,
    const std::vector<std::string> &path, bool save_folder,
    bool current_game_view)
{
    if (!ImGui::BeginPopupContextItem()) {
        return;
    }

    HddTarget target;
    target.partition = partition.letter;
    target.path = path;
    target.directory = entry.directory;

    HddTarget title_root;
    const bool has_title_root = current_game_view && path.size() >= 2u;
    if (has_title_root) {
        title_root.partition = partition.letter;
        title_root.path = {path[0], path[1]};
        title_root.directory = true;
    }

    if (ImGui::BeginMenu("Export")) {
        const char *label = entry.directory
            ? (save_folder ? "Export Save Folder..." : "Export Folder...")
            : "Export File...";
        if (ImGui::MenuItem(label)) {
            RequestExport(target);
        }
        if (has_title_root) {
            ImGui::Separator();
            const bool saves = EqualsNoCase(path[0], "UDATA");
            if (ImGui::MenuItem(saves ? "Export All Saves..."
                                      : "Export All Title Data...")) {
                RequestExport(title_root);
            }
        }
        ImGui::EndMenu();
    }

    HddTarget import_destination = target;
    import_destination.directory = true;
    if (!entry.directory && !import_destination.path.empty()) {
        import_destination.path.pop_back();
    }
    if (ImGui::BeginMenu("Import",
                         XemuKernelFs::IsKernelWritablePartition(partition.letter))) {
        DrawImportMenuItems(import_destination);
        ImGui::EndMenu();
    }

    if (ImGui::MenuItem("Copy FATX Path")) {
        const std::string fatx_path =
            XemuKernelFs::FatxPathForPartition(target.partition, target.path);
        ImGui::SetClipboardText(fatx_path.c_str());
        m_operation_status = "Copied FATX path: " + fatx_path;
    }

    std::string rename_reason;
    const bool rename_ready = !target.path.empty() &&
        XemuKernelFs::IsKernelWritablePartition(target.partition) &&
        guest_kernel_rpc_manager.FilesystemReady(rename_reason);
    if (ImGui::MenuItem("Rename...", nullptr, false, rename_ready)) {
        RequestRename(target);
    }
    if (!rename_ready && !target.path.empty() && !rename_reason.empty()) {
        ImGui::TextDisabled("%s", rename_reason.c_str());
    }

    if (ImGui::BeginMenu("Copy")) {
        std::string copy_reason;
        const bool can_select_copy = !target.path.empty() &&
            guest_kernel_rpc_manager.FilesystemReady(copy_reason);
        if (ImGui::MenuItem("Select for Copy", nullptr, false, can_select_copy)) {
            SelectCopySource(target);
        }
        if (m_transfer_selection_mode == TransferSelectionMode::Copy && entry.directory) {
            ImGui::Separator();
            if (ImGui::MenuItem("Copy Selected Here...", nullptr, false, can_select_copy)) {
                RequestCopyHere(target);
            }
            ImGui::TextDisabled("Selected: %s", m_transfer_source_display.c_str());
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Move")) {
        std::string move_reason;
        const bool can_select_move = !target.path.empty() &&
            guest_kernel_rpc_manager.FilesystemReady(move_reason);
        if (ImGui::MenuItem("Select for Move", nullptr, false, can_select_move)) {
            SelectMoveSource(target);
        }
        if (m_transfer_selection_mode == TransferSelectionMode::Move && entry.directory) {
            ImGui::Separator();
            if (ImGui::MenuItem("Move Selected Here...", nullptr, false, can_select_move)) {
                RequestMoveHere(target);
            }
            ImGui::TextDisabled("Selected: %s", m_transfer_source_display.c_str());
        }
        ImGui::EndMenu();
    }

    std::string delete_disabled_reason;
    const bool delete_ready = DeleteAllowed(target) &&
        guest_kernel_rpc_manager.FilesystemReady(delete_disabled_reason);
    if (ImGui::BeginMenu("Delete", delete_ready)) {
        const char *delete_label = entry.directory
            ? (save_folder ? "Delete Save Folder..." : "Delete Folder...")
            : "Delete File...";
        if (ImGui::MenuItem(delete_label)) {
            RequestDelete(target, XemuFatxHdd::DisplayName(entry));
        }
        if (has_title_root) {
            ImGui::Separator();
            const bool saves = EqualsNoCase(path[0], "UDATA");
            if (ImGui::MenuItem(saves ? "Delete All Saves..."
                                      : "Delete All Title Data...")) {
                RequestDelete(title_root,
                              saves ? "all saves for this Title ID"
                                    : "all title data for this Title ID");
            }
        }
        ImGui::EndMenu();
    }
    if (DeleteAllowed(target) && !delete_ready && !delete_disabled_reason.empty()) {
        ImGui::TextDisabled("%s", delete_disabled_reason.c_str());
    }
    if (!XemuKernelFs::IsKernelWritablePartition(partition.letter)) {
        ImGui::TextDisabled("Xbox-kernel write mapping is unavailable for this volume.");
    } else {
        ImGui::TextDisabled("Import/Delete use Xbox Kernel RPC; no raw FATX fallback or reset.");
    }
    ImGui::EndPopup();
}
void HddDirectoryWindow::DrawImportMenuItems(const HddTarget &destination)
{
    std::string reason;
    const bool enabled = guest_kernel_rpc_manager.FilesystemReady(reason);
    if (ImGui::MenuItem("Import Folder...", nullptr, false, enabled)) {
        RequestImport(destination, true);
    }
    if (ImGui::MenuItem("Import File...", nullptr, false, enabled)) {
        RequestImport(destination, false);
    }
    ImGui::Separator();
    if (ImGui::MenuItem("New Folder...", nullptr, false, enabled)) {
        RequestNewFolder(destination);
    }
    if (!enabled && !reason.empty()) {
        ImGui::TextDisabled("%s", reason.c_str());
    }
}
void HddDirectoryWindow::DrawRootImportButton(
    const XemuFatxHdd::Partition &partition,
    const std::vector<std::string> &destination, const char *popup_id)
{
    if (!XemuKernelFs::IsKernelWritablePartition(partition.letter)) {
        return;
    }
    if (ImGui::Button("+ ADD / IMPORT")) {
        ImGui::OpenPopup(popup_id);
    }
    if (ImGui::BeginPopup(popup_id)) {
        HddTarget target;
        target.partition = partition.letter;
        target.path = destination;
        target.directory = true;
        DrawImportMenuItems(target);
        if (m_transfer_selection_mode == TransferSelectionMode::Copy) {
            ImGui::Separator();
            std::string reason;
            const bool enabled = guest_kernel_rpc_manager.FilesystemReady(reason);
            if (ImGui::MenuItem("Copy Selected Here...", nullptr, false, enabled)) {
                RequestCopyHere(target);
            }
            ImGui::TextDisabled("Copy selected: %s", m_transfer_source_display.c_str());
        }
        if (m_transfer_selection_mode == TransferSelectionMode::Move) {
            ImGui::Separator();
            std::string reason;
            const bool enabled = guest_kernel_rpc_manager.FilesystemReady(reason);
            if (ImGui::MenuItem("Move Selected Here...", nullptr, false, enabled)) {
                RequestMoveHere(target);
            }
            ImGui::TextDisabled("Move selected: %s", m_transfer_source_display.c_str());
        }
        ImGui::EndPopup();
    }
}
void HddDirectoryWindow::DrawMoveConfirmation()
{
    if (!m_move_confirm_pending) {
        m_move_popup_open_requested = false;
        return;
    }
    if (m_move_popup_open_requested) {
        ImGui::OpenPopup("Confirm FATX Move");
        m_move_popup_open_requested = false;
    }
    if (!ImGui::BeginPopupModal("Confirm FATX Move", nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) return;
    ImGui::TextWrapped("Move this item through the Xbox kernel/FATX driver?");
    ImGui::TextWrapped("Source: %s", m_move_confirm_plan.source_fatx_path.c_str());
    ImGui::TextWrapped("Destination: %s", m_move_confirm_plan.destination_fatx_path.c_str());
    ImGui::TextDisabled("Same-volume move uses FileRenameInformation. Destination replacement is disabled and the result is verified from a fresh FATX snapshot.");
    const bool move = ImGui::Button("MOVE VIA XBOX KERNEL");
    ImGui::SameLine();
    const bool cancel = ImGui::Button("CANCEL");
    if (move) {
        if (guest_kernel_rpc_manager.StartHddRelocate(m_move_confirm_plan)) {
            m_operation_status = "Xbox Kernel Move started.";
            m_kernel_status_visible = true;
            ClearTransferSelection();
            m_move_confirm_pending = false;
            ImGui::CloseCurrentPopup();
        } else {
            m_operation_status = "Move did not start: " + guest_kernel_rpc_manager.Status();
        }
    } else if (cancel) {
        m_move_confirm_pending = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
void HddDirectoryWindow::DrawRenamePopup()
{
    if (!m_rename_pending) {
        m_rename_popup_open_requested = false;
        return;
    }
    if (m_rename_popup_open_requested) {
        ImGui::OpenPopup("Rename FATX Item");
        m_rename_popup_open_requested = false;
    }
    if (!ImGui::BeginPopupModal("Rename FATX Item", nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) return;
    ImGui::TextWrapped("Rename through the Xbox kernel/FATX driver. Existing destinations are never replaced.");
    ImGui::InputText("New name", m_rename_name, sizeof(m_rename_name));
    const bool rename = ImGui::Button("RENAME VIA XBOX KERNEL");
    ImGui::SameLine();
    const bool cancel = ImGui::Button("CANCEL");
    if (rename) {
        XemuKernelFs::RelocatePlan plan;
        std::string error;
        if (guest_kernel_rpc_manager.PrepareHddRename(
                m_rename_target.partition, m_rename_target.path,
                m_rename_target.directory, m_rename_name, plan, error)) {
            const std::string destination = plan.destination_fatx_path;
            if (guest_kernel_rpc_manager.StartHddRelocate(plan)) {
                m_operation_status = "Xbox Kernel Rename started for " + destination + ".";
                m_kernel_status_visible = true;
                m_rename_pending = false;
                ImGui::CloseCurrentPopup();
            } else {
                m_operation_status = "Rename did not start: " + guest_kernel_rpc_manager.Status();
            }
        } else {
            m_operation_status = "Rename preflight failed: " + error;
        }
    } else if (cancel) {
        m_rename_pending = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
void HddDirectoryWindow::DrawNewFolderPopup()
{
    if (!m_new_folder_pending) {
        m_new_folder_popup_open_requested = false;
        return;
    }
    if (m_new_folder_popup_open_requested) {
        ImGui::OpenPopup("New FATX Folder");
        m_new_folder_popup_open_requested = false;
    }
    if (!ImGui::BeginPopupModal("New FATX Folder", nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    const std::string destination = XemuKernelFs::FatxPathForPartition(
        m_new_folder_destination.partition, m_new_folder_destination.path);
    ImGui::TextWrapped("Create a new folder through the Xbox kernel in %s", destination.c_str());
    ImGui::InputText("Folder name", m_new_folder_name, sizeof(m_new_folder_name));
    const bool create = ImGui::Button("CREATE FOLDER");
    ImGui::SameLine();
    const bool cancel = ImGui::Button("CANCEL");
    if (create) {
        XemuKernelFs::ImportPlan plan;
        std::string error;
        if (guest_kernel_rpc_manager.PrepareHddCreateDirectory(
                m_new_folder_name, m_new_folder_destination.partition,
                m_new_folder_destination.path, plan, error)) {
            m_new_folder_pending = false;
            ImGui::CloseCurrentPopup();
            m_import_confirm_plan = std::move(plan);
            m_import_pending = true;
            m_import_popup_open_requested = true;
        } else {
            m_operation_status = "New Folder preflight failed: " + error;
        }
    } else if (cancel) {
        m_new_folder_pending = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
void HddDirectoryWindow::DrawImportConfirmation()
{
    if (!m_import_pending) {
        m_import_popup_open_requested = false;
        return;
    }
    if (m_import_popup_open_requested) {
        ImGui::OpenPopup("Confirm Xbox Kernel HDD Import");
        m_import_popup_open_requested = false;
    }

    ImGui::SetNextWindowSize(ImVec2(620, 0), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Confirm Xbox Kernel HDD Import", nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    const std::string destination = XemuKernelFs::FatxPathForPartition(
        m_import_confirm_plan.partition,
        m_import_confirm_plan.destination_components);
    const std::string target = XemuKernelFs::FatxPathForPartition(
        m_import_confirm_plan.partition,
        m_import_confirm_plan.entries.front().components);
    if (m_import_confirm_plan.source_from_fatx) {
        ImGui::TextWrapped("%s this FATX %s through the Xbox kernel/FATX driver?",
            m_import_confirm_plan.delete_source_after_copy ? "Move" : "Copy",
            m_import_confirm_plan.source_is_directory ? "folder" : "file");
        ImGui::TextWrapped("Source: %s", XemuKernelFs::FatxPathForPartition(
            m_import_confirm_plan.source_partition, m_import_confirm_plan.source_components).c_str());
    } else if (m_import_confirm_plan.synthetic_directory) {
        ImGui::TextWrapped("Create this new folder through the Xbox kernel/FATX driver?");
    } else {
        ImGui::TextWrapped("Import this host %s through the Xbox kernel/FATX driver?",
                           m_import_confirm_plan.source_is_directory ? "folder" : "file");
        ImGui::TextWrapped("Host: %s", m_import_confirm_plan.source_path.c_str());
    }
    ImGui::TextWrapped("Destination directory: %s", destination.c_str());
    ImGui::TextWrapped("Create: %s", target.c_str());
    ImGui::Spacing();
    ImGui::Text("Plan: %u file(s), %u folder(s), %s, %llu kernel operation(s)",
                m_import_confirm_plan.file_count,
                m_import_confirm_plan.directory_count,
                FormatByteSize(m_import_confirm_plan.total_bytes).c_str(),
                (unsigned long long)m_import_confirm_plan.total_operations);
    ImGui::TextWrapped(
        "Import is create-only: an existing destination is never merged or overwritten. Host items and the destination are revalidated immediately before mutation.");
    ImGui::TextDisabled(
        "Each create/write/flush/close step is PASSIVE_LEVEL-gated and verified against a fresh FATX snapshot. No raw FATX write fallback and no Xbox reset are used.");
    ImGui::Separator();

    const char *confirm_label = m_import_confirm_plan.source_from_fatx
        ? (m_import_confirm_plan.delete_source_after_copy
            ? "MOVE VIA COPY + VERIFY + DELETE" : "COPY VIA XBOX KERNEL")
        : (m_import_confirm_plan.synthetic_directory
            ? "CREATE VIA XBOX KERNEL" : "IMPORT VIA XBOX KERNEL");
    const bool confirm = ImGui::Button(confirm_label);
    ImGui::SameLine();
    const bool cancel = ImGui::Button("CANCEL");
    if (confirm) {
        XemuKernelFs::ImportPlan plan = std::move(m_import_confirm_plan);
        m_import_pending = false;
        ImGui::CloseCurrentPopup();
        m_kernel_status_visible = true;
        if (guest_kernel_rpc_manager.StartHddImport(plan)) {
            if (plan.source_from_fatx) {
                ClearTransferSelection();
            }
            m_operation_status = plan.source_from_fatx
                ? std::string(plan.delete_source_after_copy ? "Cross-volume Move" : "Copy") + " started for " + target + "."
                : "Xbox Kernel Import started for " + target + ".";
        } else {
            m_operation_status = "Import did not start: " + guest_kernel_rpc_manager.Status();
        }
    } else if (cancel) {
        m_import_pending = false;
        m_import_confirm_plan = {};
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
void HddDirectoryWindow::DrawKernelStatus()
{
    if (!m_kernel_status_visible) {
        return;
    }

    const GuestKernelRpcManager::HddOperationProgress progress =
        guest_kernel_rpc_manager.GetHddOperationProgress();
    if (progress.active) {
        ImGui::Text("%s...", progress.label.c_str());
        float fraction = 0.0f;
        if (progress.total_operations != 0) {
            fraction = static_cast<float>(progress.completed_operations) /
                       static_cast<float>(progress.total_operations);
        }
        ImGui::ProgressBar(std::clamp(fraction, 0.0f, 1.0f), ImVec2(-1.0f, 0.0f));
        ImGui::Text("Operations: %llu / %llu",
                    (unsigned long long)progress.completed_operations,
                    (unsigned long long)progress.total_operations);
        if (progress.total_files || progress.total_directories) {
            ImGui::Text("Files: %u / %u    Folders: %u / %u",
                        progress.completed_files, progress.total_files,
                        progress.completed_directories, progress.total_directories);
        }
        if (progress.total_bytes) {
            ImGui::Text("Written: %s / %s",
                        FormatByteSize(progress.completed_bytes).c_str(),
                        FormatByteSize(progress.total_bytes).c_str());
        }
        ImGui::Text("Elapsed: %.2f s    PASSIVE_LEVEL samples: %llu",
                    progress.elapsed_ms / 1000.0,
                    (unsigned long long)progress.safe_point_samples);
        if (!progress.current_path.empty()) {
            ImGui::TextWrapped("Current: %s", progress.current_path.c_str());
        }
    }

    const std::string &status = guest_kernel_rpc_manager.Status();
    if (!status.empty() && status != "Not tested." &&
        ImGui::TreeNodeEx("Kernel RPC Details", ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::TextWrapped("%s", status.c_str());
        ImGui::TreePop();
    }
}
void HddDirectoryWindow::DrawEntries(
    const XemuFatxHdd::Partition &partition,
    const std::vector<XemuFatxHdd::Entry> &entries,
    std::vector<std::string> &path, bool current_game_view)
{
    for (const XemuFatxHdd::Entry &entry : entries) {
        if (m_name_filter[0] != '\0' &&
            !EntryOrDescendantMatches(entry, m_name_filter)) {
            continue;
        }
        std::vector<std::string> entry_path = path;
        entry_path.push_back(entry.name);
        const bool save_folder = current_game_view && entry.directory &&
            entry_path.size() == 3 && EqualsNoCase(entry_path[0], "UDATA");
        const std::string display_name = XemuFatxHdd::DisplayName(entry);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);

        if (entry.directory) {
            const bool open = ImGui::TreeNodeEx(
                (const void *)&entry,
                ImGuiTreeNodeFlags_SpanFullWidth |
                    ImGuiTreeNodeFlags_OpenOnArrow |
                    ImGuiTreeNodeFlags_OpenOnDoubleClick,
                "%s", display_name.c_str());
            DrawExportContext(partition, entry, entry_path, save_folder, current_game_view);

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(save_folder ? "Save" : "Directory");
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted("-");
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%08X", entry.first_cluster);
            ImGui::TableSetColumnIndex(4);
            const std::string modified = XemuFatxHdd::FormatTimestamp(
                entry.modified_date, entry.modified_time);
            ImGui::TextUnformatted(modified.empty() ? "-" : modified.c_str());
            ImGui::TableSetColumnIndex(5);
            const std::string attrs = FormatAttributes(entry.attributes);
            ImGui::TextUnformatted(attrs.c_str());

            if (open) {
                path.push_back(entry.name);
                DrawEntries(partition, entry.children, path, current_game_view);
                path.pop_back();
                ImGui::TableSetColumnIndex(0);
                ImGui::TreePop();
            }
        } else {
            ImGui::TreeNodeEx(
                (const void *)&entry,
                ImGuiTreeNodeFlags_Leaf |
                    ImGuiTreeNodeFlags_NoTreePushOnOpen |
                    ImGuiTreeNodeFlags_SpanFullWidth,
                "%s", display_name.c_str());
            DrawExportContext(partition, entry, entry_path, false, current_game_view);

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted((entry.attributes & 0x08) ? "Volume" : "File");
            ImGui::TableSetColumnIndex(2);
            const std::string size = FormatByteSize(entry.file_size);
            ImGui::TextUnformatted(size.c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%08X", entry.first_cluster);
            ImGui::TableSetColumnIndex(4);
            const std::string modified = XemuFatxHdd::FormatTimestamp(
                entry.modified_date, entry.modified_time);
            ImGui::TextUnformatted(modified.empty() ? "-" : modified.c_str());
            ImGui::TableSetColumnIndex(5);
            const std::string attrs = FormatAttributes(entry.attributes);
            ImGui::TextUnformatted(attrs.c_str());
        }
    }
}
void HddDirectoryWindow::DrawCurrentGameArea(
    const XemuFatxHdd::Partition &partition, const XemuFatxHdd::Entry *area,
    const std::string &title_id, const char *area_name, const char *description)
{
    ImGui::TextDisabled("%s", description);
    if (!area || !area->directory) {
        ImGui::TextDisabled("E:\\%s is not present on this HDD.", area_name);
        return;
    }

    const XemuFatxHdd::Entry *title = FindChildNoCase(area->children, title_id);
    if (!title || !title->directory) {
        ImGui::TextDisabled("No %s data found for Title ID %s.", area_name,
                            title_id.c_str());
        HddTarget area_root;
        area_root.partition = partition.letter;
        area_root.path = {area->name};
        area_root.directory = true;
        if (ImGui::Button("IMPORT EXPORTED TITLE-ID FOLDER...")) {
            RequestImport(area_root, true, title_id);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Select a host folder named %s to recreate E:\\%s\\%s.",
                            title_id.c_str(), area_name, title_id.c_str());
        return;
    }

    const std::string title_display = XemuFatxHdd::DisplayName(*title);
    ImGui::Text("E:\\%s\\%s", area_name, title_display.c_str());
    ImGui::SameLine();
    HddTarget whole;
    whole.partition = partition.letter;
    whole.path = {area->name, title->name};
    whole.directory = true;
    const char *button = EqualsNoCase(area_name, "UDATA")
        ? "EXPORT ALL SAVES..." : "EXPORT TITLE DATA...";
    if (ImGui::Button(button)) {
        RequestExport(whole);
    }
    ImGui::SameLine();
    const std::string import_popup = std::string("##current_game_import_") + area_name;
    DrawRootImportButton(partition, whole.path, import_popup.c_str());
    ImGui::SameLine();
    const char *delete_all_label = EqualsNoCase(area_name, "UDATA")
        ? "DELETE ALL SAVES..." : "DELETE ALL TITLE DATA...";
    if (ImGui::Button(delete_all_label)) {
        RequestDelete(whole, EqualsNoCase(area_name, "UDATA")
                                ? "all saves for this Title ID"
                                : "all title data for this Title ID");
    }
    ImGui::Separator();

    const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
                                  ImGuiTableFlags_SizingStretchProp;
    if (ImGui::BeginTable("##current_game_hdd_entries", 6, flags, ImVec2(0, 0))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 3.0f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Cluster", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("Attr", ImGuiTableColumnFlags_WidthFixed, 55.0f);
        ImGui::TableHeadersRow();
        std::vector<std::string> path{area->name, title->name};
        DrawEntries(partition, title->children, path, true);
        ImGui::EndTable();
    }
}
void HddDirectoryWindow::DrawCurrentGameHdd(uint32_t title_id)
{
    RefreshIfStale();

    if (title_id == 0) {
        ImGui::TextUnformatted("No running XBE detected.");
        ImGui::TextDisabled("Start a game to filter E:\\UDATA and E:\\TDATA by its Title ID.");
        return;
    }

    if (ImGui::Button("REFRESH HDD")) {
        Refresh();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("FATX snapshot; Import/Delete use the live Xbox kernel/FATX driver with no raw fallback or reset.");
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputTextWithHint("##current_game_hdd_filter", "Filter names...",
                             m_name_filter, sizeof(m_name_filter));
    if (m_name_filter[0] != '\0') {
        ImGui::SameLine();
        if (ImGui::SmallButton("CLEAR FILTER##current_game")) {
            m_name_filter[0] = '\0';
        }
    }
    if (!m_status.empty()) {
        ImGui::TextWrapped("%s", m_status.c_str());
    }
    if (!m_operation_status.empty()) {
        ImGui::TextWrapped("%s", m_operation_status.c_str());
    }
    if (m_transfer_selection_mode != TransferSelectionMode::None) {
        ImGui::Text("Selected for %s: %s",
                    m_transfer_selection_mode == TransferSelectionMode::Copy ? "Copy" : "Move",
                    m_transfer_source_display.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("CLEAR SELECTION")) {
            ClearTransferSelection();
        }
    }
    DrawKernelStatus();

    const XemuFatxHdd::Partition *data = XemuFatxHdd::FindPartition(m_snapshot, 'E');
    if (!data || !data->available) {
        ImGui::TextDisabled("The E: FATX partition is not available.");
        return;
    }

    char title_text[16];
    std::snprintf(title_text, sizeof(title_text), "%08X", title_id);
    const XemuFatxHdd::Entry *udata = FindChildNoCase(data->entries, "UDATA");
    const XemuFatxHdd::Entry *tdata = FindChildNoCase(data->entries, "TDATA");

    XemuDebugUi::ScopedTabStyle tab_style;
    if (ImGui::BeginTabBar("##current_game_hdd_tabs")) {
        if (ImGui::BeginTabItem("Saves / UDATA")) {
            DrawCurrentGameArea(*data, udata, title_text, "UDATA",
                                "Xbox game saves for the running Title ID.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("DLC / TDATA")) {
            DrawCurrentGameArea(*data, tdata, title_text, "TDATA",
                                "Title-specific data / DLC for the running Title ID.");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    tab_style.Restore();
    DrawDeleteConfirmation();
    DrawMoveConfirmation();
    DrawRenamePopup();
    DrawNewFolderPopup();
    DrawImportConfirmation();
}
void HddDirectoryWindow::Draw(bool detached)
{
    if (!is_open) {
        return;
    }

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
    const char *window_name = "Xbox HDD Directory";
    bool *window_open = &is_open;
    if (detached) {
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
        flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
        window_name = "##DetachedXboxHddDirectory";
        window_open = nullptr;
    } else {
        ImGui::SetNextWindowSize(ImVec2(1000, 680), ImGuiCond_FirstUseEver);
    }

    if (!ImGui::Begin(window_name, window_open, flags)) {
        ImGui::End();
        return;
    }

    RefreshIfStale();

    if (ImGui::Button("REFRESH")) {
        Refresh();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("FATX snapshot of the mounted Xbox HDD (ide0-hd0)");

    ImGui::SetNextItemWidth(260.0f);
    ImGui::InputTextWithHint("##hdd_name_filter", "Filter names...",
                             m_name_filter, sizeof(m_name_filter));
    if (m_name_filter[0] != '\0') {
        ImGui::SameLine();
        if (ImGui::SmallButton("CLEAR FILTER##hdd")) {
            m_name_filter[0] = '\0';
        }
    }

    if (!m_status.empty()) {
        ImGui::TextWrapped("%s", m_status.c_str());
    }
    if (m_snapshot.hdd_available) {
        ImGui::SameLine();
        ImGui::TextDisabled("HDD size: %s",
                            FormatByteSize(m_snapshot.image_size).c_str());
    }
    if (!m_operation_status.empty()) {
        ImGui::TextWrapped("%s", m_operation_status.c_str());
    }
    if (m_transfer_selection_mode != TransferSelectionMode::None) {
        ImGui::Text("Selected for %s: %s",
                    m_transfer_selection_mode == TransferSelectionMode::Copy ? "Copy" : "Move",
                    m_transfer_source_display.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("CLEAR SELECTION")) {
            ClearTransferSelection();
        }
    }
    DrawKernelStatus();
    ImGui::TextDisabled("Right-click any FATX item for Export / Import / Delete / Rename / Move / Copy. + ADD / IMPORT writes into the currently viewed drive root. File writes preserve Create/Open -> Write -> Flush -> Close -> fresh FATX Verify.");
    if (ImGui::TreeNodeEx("HDD Performance", ImGuiTreeNodeFlags_SpanAvailWidth)) {
        const HddSnapshotService::PerformanceStats stats =
            hdd_snapshot_service.GetPerformanceStats();
        const auto avg = [](uint64_t total, uint64_t calls) { return calls ? total / calls : 0; };
        ImGui::Text("Raw FATX snapshots: %llu | avg %llu us | max %llu us",
                    (unsigned long long)stats.raw_calls,
                    (unsigned long long)avg(stats.raw_total_us, stats.raw_calls),
                    (unsigned long long)stats.raw_max_us);
        ImGui::Text("Partition snapshots: %llu | avg %llu us | max %llu us",
                    (unsigned long long)stats.partition_calls,
                    (unsigned long long)avg(stats.partition_total_us, stats.partition_calls),
                    (unsigned long long)stats.partition_max_us);
        ImGui::Text("Partition-set preflights: %llu | %llu partition(s) | avg %llu us | max %llu us",
                    (unsigned long long)stats.partition_set_calls,
                    (unsigned long long)stats.partition_set_partitions,
                    (unsigned long long)avg(stats.partition_set_total_us, stats.partition_set_calls),
                    (unsigned long long)stats.partition_set_max_us);
        ImGui::Text("Display snapshots: %llu | avg %llu us | max %llu us",
                    (unsigned long long)stats.display_calls,
                    (unsigned long long)avg(stats.display_total_us, stats.display_calls),
                    (unsigned long long)stats.display_max_us);
        ImGui::Text("FATX source chunks: %llu | %s | avg %llu us",
                    (unsigned long long)stats.source_chunk_calls,
                    FormatByteSize(stats.source_chunk_bytes).c_str(),
                    (unsigned long long)avg(stats.source_chunk_total_us, stats.source_chunk_calls));
        ImGui::Text("Byte-verify passes: %llu | %s | avg %llu us",
                    (unsigned long long)stats.content_verify_calls,
                    FormatByteSize(stats.content_verify_bytes).c_str(),
                    (unsigned long long)avg(stats.content_verify_total_us, stats.content_verify_calls));
        if (ImGui::Button("RESET HDD PERFORMANCE COUNTERS")) {
            hdd_snapshot_service.ResetPerformanceStats();
        }
        ImGui::TextDisabled("These counters measure snapshot cost only; write transaction/verification semantics are not reduced by optimization.");
        ImGui::TreePop();
    }
    ImGui::Separator();

    if (m_snapshot.partitions.empty()) {
        ImGui::TextDisabled("No FATX partition snapshot is available.");
        ImGui::End();
        return;
    }

    XemuDebugUi::ScopedTabStyle tab_style;
    if (ImGui::BeginTabBar("##hdd_partitions")) {
        for (const XemuFatxHdd::Partition &part : m_snapshot.partitions) {
            char tab_name[48];
            std::snprintf(tab_name, sizeof(tab_name), "%c: %s", part.letter,
                          part.label.c_str());
            if (!ImGui::BeginTabItem(tab_name)) {
                continue;
            }

            ImGui::Text("Offset: %016llX    Size: %s",
                        (unsigned long long)part.offset,
                        FormatByteSize(part.size).c_str());
            if (part.available) {
                ImGui::SameLine();
                ImGui::TextDisabled("Volume %08X | FAT%u | Cluster %u KiB",
                                    part.volume_id, part.fat_bits,
                                    part.bytes_per_cluster / 1024);
            }
            ImGui::TextWrapped("%s", part.status.c_str());

            if (part.available && XemuKernelFs::IsKernelWritablePartition(part.letter)) {
                char root_popup[48];
                std::snprintf(root_popup, sizeof(root_popup), "##root_import_%c", part.letter);
                const std::vector<std::string> root_destination;
                DrawRootImportButton(part, root_destination, root_popup);
                ImGui::SameLine();
                ImGui::TextDisabled("Destination: %c:\\", part.letter);
            }

            if (part.available) {
                const ImGuiTableFlags table_flags =
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
                    ImGuiTableFlags_SizingStretchProp;
                if (ImGui::BeginTable("##fatx_directory", 6, table_flags,
                                      ImVec2(0, 0))) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch,
                                            3.0f);
                    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed,
                                            90.0f);
                    ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed,
                                            90.0f);
                    ImGui::TableSetupColumn("Cluster", ImGuiTableColumnFlags_WidthFixed,
                                            90.0f);
                    ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed,
                                            150.0f);
                    ImGui::TableSetupColumn("Attr", ImGuiTableColumnFlags_WidthFixed,
                                            55.0f);
                    ImGui::TableHeadersRow();
                    std::vector<std::string> path;
                    DrawEntries(part, part.entries, path, false);
                    ImGui::EndTable();
                }
            }

            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    tab_style.Restore();

    DrawDeleteConfirmation();
    DrawMoveConfirmation();
    DrawRenamePopup();
    DrawNewFolderPopup();
    DrawImportConfirmation();
    ImGui::End();
}
