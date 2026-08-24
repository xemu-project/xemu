//
// xemu Xbox HDD Directory Viewer
//
// Copyright (C) 2026 xemu contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#include "hdd-directory.hh"

#include "hdd-export-service.hh"
#include "guest-kernel-rpc.hh"
#include "hdd-snapshot-service.hh"
#include "../misc.hh"


#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <utility>

HddDirectoryWindow hdd_directory_window;

namespace {

using XemuKernelFs::EqualsNoCase;
using XemuKernelFs::FindChildNoCase;



} // namespace

std::string HddDirectoryWindow::FormatByteSize(uint64_t bytes)
{
    char buffer[64];
    if (bytes < 1024) {
        std::snprintf(buffer, sizeof(buffer), "%llu B",
                      (unsigned long long)bytes);
    } else if (bytes < 1024ull * 1024ull) {
        std::snprintf(buffer, sizeof(buffer), "%.1f KiB", bytes / 1024.0);
    } else if (bytes < 1024ull * 1024ull * 1024ull) {
        std::snprintf(buffer, sizeof(buffer), "%.2f MiB",
                      bytes / (1024.0 * 1024.0));
    } else {
        std::snprintf(buffer, sizeof(buffer), "%.2f GiB",
                      bytes / (1024.0 * 1024.0 * 1024.0));
    }
    return buffer;
}

std::string HddDirectoryWindow::FormatAttributes(uint8_t attributes)
{
    std::string out;
    if (attributes & 0x01) out += 'R';
    if (attributes & 0x02) out += 'S';
    if (attributes & 0x04) out += 'H';
    if (attributes & 0x08) out += 'V';
    if (attributes & 0x10) out += 'D';
    return out.empty() ? "-" : out;
}

void HddDirectoryWindow::Refresh()
{
    m_has_snapshot =
        hdd_snapshot_service.BuildDisplaySnapshot(m_snapshot, m_status);
    m_snapshot_change_generation = hdd_snapshot_service.ChangeGeneration();
}

void HddDirectoryWindow::RefreshIfStale()
{
    if (guest_kernel_rpc_manager.OperationBusy()) {
        return;
    }
    if (!m_has_snapshot ||
        m_snapshot_change_generation != hdd_snapshot_service.ChangeGeneration()) {
        Refresh();
    }
}

void HddDirectoryWindow::RequestExport(const HddTarget &target)
{
    ShowOpenFolderDialog(nullptr, [this, target](const char *path) {
        XemuHddExport::Target export_target;
        export_target.partition = target.partition;
        export_target.components = target.path;
        export_target.directory = target.directory;
        XemuHddExport::Result result;
        std::string error;
        if (!XemuHddExport::ExportToHost(export_target, path, result, error)) {
            m_operation_status = "Export failed: " + error;
            return;
        }
        if (result.directory) {
            m_operation_status = "Exported " + std::to_string(result.file_count) +
                " file(s), " + std::to_string(result.directory_count) +
                " folder(s), " + FormatByteSize(result.byte_count) + " to " +
                result.host_path;
        } else {
            m_operation_status = "Exported " + FormatByteSize(result.byte_count) +
                " to " + result.host_path;
        }
    });
}

bool HddDirectoryWindow::DeleteAllowed(const HddTarget &target)
{
    // All FATX volumes exposed by the HDD browser may use the Xbox-kernel
    // delete path. The partition root itself is never a selectable delete
    // target and remains hard-blocked in the backend as a second guard.
    return !target.path.empty() &&
           XemuKernelFs::IsKernelWritablePartition(target.partition);
}

void HddDirectoryWindow::RequestDelete(const HddTarget &target,
                                       const std::string &display_name)
{
    if (!DeleteAllowed(target)) {
        m_operation_status = "The selected HDD item cannot be deleted through Xbox Kernel RPC.";
        return;
    }
    if (guest_kernel_rpc_manager.OperationBusy()) {
        m_operation_status = "Another Guest Kernel RPC filesystem operation is already running.";
        m_kernel_status_visible = true;
        return;
    }

    // Do not refresh the shared FATX snapshot from inside an ImGui row/context
    // callback: DrawEntries() still owns references into that snapshot for the
    // remainder of the frame. Defer the fresh preflight until the root-scope
    // confirmation renderer runs after the table is complete.
    m_delete_target = target;
    m_delete_display = display_name;
    m_delete_confirm_plan.clear();
    m_delete_pending = true;
    m_delete_popup_open_requested = true;
}





void HddDirectoryWindow::ClearTransferSelection()
{
    m_transfer_selection_mode = TransferSelectionMode::None;
    m_transfer_source = {};
    m_transfer_source_display.clear();
}

void HddDirectoryWindow::SelectCopySource(const HddTarget &target)
{
    if (target.path.empty()) {
        m_operation_status = "A FATX volume root cannot be copied as an item.";
        return;
    }
    m_transfer_source = target;
    m_transfer_selection_mode = TransferSelectionMode::Copy;
    m_transfer_source_display = XemuKernelFs::FatxPathForPartition(target.partition, target.path);
    m_operation_status = "Selected for Copy: " + m_transfer_source_display + ". Right-click a destination folder or use the root + menu.";
}

void HddDirectoryWindow::RequestCopyHere(const HddTarget &destination)
{
    if (m_transfer_selection_mode != TransferSelectionMode::Copy || !destination.directory) return;
    XemuKernelFs::ImportPlan plan;
    std::string error;
    if (!guest_kernel_rpc_manager.PrepareHddCopy(
            m_transfer_source.partition, m_transfer_source.path, m_transfer_source.directory,
            destination.partition, destination.path, false, plan, error)) {
        m_operation_status = "Copy preflight failed: " + error;
        return;
    }
    m_import_confirm_plan = std::move(plan);
    m_import_pending = true;
    m_import_popup_open_requested = true;
}

void HddDirectoryWindow::SelectMoveSource(const HddTarget &target)
{
    if (target.path.empty()) {
        m_operation_status = "A FATX volume root cannot be moved.";
        return;
    }
    m_transfer_source = target;
    m_transfer_selection_mode = TransferSelectionMode::Move;
    m_transfer_source_display = XemuKernelFs::FatxPathForPartition(target.partition, target.path);
    m_operation_status = "Selected for same-volume Move: " + m_transfer_source_display + ". Right-click a destination folder or use the root + menu.";
}

void HddDirectoryWindow::RequestMoveHere(const HddTarget &destination)
{
    if (m_transfer_selection_mode != TransferSelectionMode::Move || !destination.directory) return;
    std::string error;
    if (m_transfer_source.partition != destination.partition) {
        XemuKernelFs::ImportPlan plan;
        if (!guest_kernel_rpc_manager.PrepareHddCopy(
                m_transfer_source.partition, m_transfer_source.path, m_transfer_source.directory,
                destination.partition, destination.path, true, plan, error)) {
            m_operation_status = "Cross-volume Move preflight failed: " + error;
            return;
        }
        m_import_confirm_plan = std::move(plan);
        m_import_pending = true;
        m_import_popup_open_requested = true;
        return;
    }
    XemuKernelFs::RelocatePlan plan;
    if (!guest_kernel_rpc_manager.PrepareHddMove(
            m_transfer_source.partition, m_transfer_source.path, m_transfer_source.directory,
            destination.path, plan, error)) {
        m_operation_status = "Move preflight failed: " + error;
        return;
    }
    m_move_confirm_plan = std::move(plan);
    m_move_confirm_pending = true;
    m_move_popup_open_requested = true;
}


void HddDirectoryWindow::RequestRename(const HddTarget &target)
{
    if (target.path.empty()) {
        m_operation_status = "A FATX volume root cannot be renamed.";
        return;
    }
    std::string reason;
    if (!guest_kernel_rpc_manager.FilesystemReady(reason)) {
        m_operation_status = reason;
        return;
    }
    m_rename_target = target;
    std::snprintf(m_rename_name, sizeof(m_rename_name), "%s", target.path.back().c_str());
    m_rename_pending = true;
    m_rename_popup_open_requested = true;
}


void HddDirectoryWindow::RequestNewFolder(const HddTarget &destination)
{
    std::string reason;
    if (!XemuKernelFs::IsKernelWritablePartition(destination.partition) ||
        !guest_kernel_rpc_manager.FilesystemReady(reason)) {
        m_operation_status = reason.empty()
            ? "The selected HDD volume has no supported Xbox-kernel write mapping."
            : reason;
        return;
    }
    m_new_folder_destination = destination;
    m_new_folder_name[0] = '\0';
    m_new_folder_pending = true;
    m_new_folder_popup_open_requested = true;
}


void HddDirectoryWindow::RequestImport(const HddTarget &destination, bool folder,
                                       const std::string &required_root_name)
{
    if (!XemuKernelFs::IsKernelWritablePartition(destination.partition)) {
        m_operation_status = "The selected HDD volume has no supported Xbox-kernel write mapping.";
        return;
    }
    if (guest_kernel_rpc_manager.OperationBusy()) {
        m_operation_status = "Another Guest Kernel RPC filesystem operation is already running.";
        m_kernel_status_visible = true;
        return;
    }

    const auto selected = [this, destination, folder, required_root_name](const char *path) {
        if (!path || path[0] == '\0') {
            return;
        }
        XemuKernelFs::ImportPlan plan;
        std::string error;
        if (!guest_kernel_rpc_manager.PrepareHddImport(
                path, folder, destination.partition, destination.path,
                plan, error)) {
            m_operation_status = "Import preflight failed: " + error;
            m_kernel_status_visible = true;
            return;
        }
        if (!required_root_name.empty() &&
            !EqualsNoCase(plan.root_name, required_root_name)) {
            m_operation_status = "Import preflight failed: select the exported Title-ID folder named " +
                              required_root_name + ".";
            return;
        }
        m_import_confirm_plan = std::move(plan);
        m_import_pending = true;
        m_import_popup_open_requested = true;
    };

    if (folder) {
        ShowOpenFolderDialog(nullptr, selected);
    } else {
        ShowOpenFileDialog(nullptr, 0, nullptr, selected);
    }
}






