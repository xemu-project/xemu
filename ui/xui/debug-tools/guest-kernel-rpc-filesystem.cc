//
// Production HDD filesystem entry points for Guest Kernel RPC.
//
// Kept separate from the experimental/diagnostic UI implementation. This file
// owns fresh-snapshot preflight and transitions confirmed HDD operations into
// the proven asynchronous GuestKernelRpcManager executor.
//
#include "qemu/osdep.h"

#include "guest-kernel-rpc.hh"
#include "current-game.hh"
#include "hdd-snapshot-service.hh"
#include "kernel-rpc-filesystem.hh"

#include <SDL3/SDL.h>

extern "C" {
#include "system/runstate.h"
}

#include <algorithm>
#include <cctype>
#include <utility>


bool GuestKernelRpcManager::FilesystemReady(std::string &reason) const
{
    reason.clear();
    if (IsBusy()) {
        reason = "A Kernel RPC filesystem operation is already running.";
        return false;
    }
    if (m_breakpoint_installed || xemu_guest_rpc_arena_active()) {
        reason = "A previous Kernel RPC cleanup is still owned; reset the Xbox/Xemu before retrying.";
        return false;
    }
    if (!runstate_is_running()) {
        reason = "Resume the Xbox before using HDD Kernel RPC operations.";
        return false;
    }
    const CurrentGameManager::GameInfo &game = current_game_manager.Get();
    if (!game.valid || game.title_id == 0 || game.xbe_image_size == 0) {
        reason = "A running XBE is required for HDD Kernel RPC operations.";
        return false;
    }
    return true;
}

GuestKernelRpcManager::HddOperationProgress
GuestKernelRpcManager::GetHddOperationProgress() const
{
    HddOperationProgress progress;
    if (!IsHddFrontendOperation() || !IsBusy()) {
        return progress;
    }
    progress.active = true;
    switch (m_filesystem_operation.kind) {
    case FilesystemOperationKind::HostImport:
        progress.label = "Importing";
        break;
    case FilesystemOperationKind::NewFolder:
        progress.label = "Creating Folder";
        break;
    case FilesystemOperationKind::FatxCopy:
        progress.label = "Copying";
        break;
    case FilesystemOperationKind::CrossVolumeMove:
        progress.label = m_filesystem_operation.phase == FilesystemOperationPhase::DeletingSource
            ? "Moving (Delete source phase)" : "Moving (Copy/verify phase)";
        break;
    case FilesystemOperationKind::Delete:
        progress.label = "Deleting";
        break;
    case FilesystemOperationKind::Rename:
        progress.label = "Renaming";
        break;
    case FilesystemOperationKind::SameVolumeMove:
        progress.label = "Moving";
        break;
    default:
        progress.label = "Filesystem operation";
        break;
    }

    const bool transfer = m_filesystem_operation.kind == FilesystemOperationKind::HostImport ||
        m_filesystem_operation.kind == FilesystemOperationKind::NewFolder ||
        m_filesystem_operation.kind == FilesystemOperationKind::FatxCopy ||
        (m_filesystem_operation.kind == FilesystemOperationKind::CrossVolumeMove &&
         m_filesystem_operation.phase != FilesystemOperationPhase::DeletingSource);
    const bool deleting = m_filesystem_operation.kind == FilesystemOperationKind::Delete ||
        (m_filesystem_operation.kind == FilesystemOperationKind::CrossVolumeMove &&
         m_filesystem_operation.phase == FilesystemOperationPhase::DeletingSource);
    if (transfer) {
        progress.completed_operations = m_import_completed_operations;
        progress.total_operations = m_import_preflight.total_operations;
        progress.completed_bytes = m_import_written_bytes;
        progress.total_bytes = m_import_preflight.total_bytes;
        progress.completed_files = m_import_completed_files;
        progress.total_files = m_import_preflight.file_count;
        progress.completed_directories = m_import_completed_directories;
        progress.total_directories = m_import_preflight.directory_count;
        if (m_import_plan_index < m_import_preflight.entries.size()) {
            progress.current_path = m_import_preflight.entries[m_import_plan_index].fatx_path;
        }
    } else if (deleting) {
        progress.completed_operations = m_recursive_deleted_files + m_recursive_deleted_directories;
        progress.total_operations = m_recursive_delete_plan.size();
        progress.completed_files = m_recursive_deleted_files;
        progress.completed_directories = m_recursive_deleted_directories;
        const XemuKernelFs::DeletePlanSummary summary =
            XemuKernelFs::SummarizeDeletePlan(m_recursive_delete_plan);
        progress.total_files = summary.file_count;
        progress.total_directories = summary.directory_count;
        if (m_recursive_delete_index < m_recursive_delete_plan.size()) {
            progress.current_path = m_recursive_delete_plan[m_recursive_delete_index].fatx_path;
        }
    } else if (m_filesystem_operation.kind == FilesystemOperationKind::Rename ||
               m_filesystem_operation.kind == FilesystemOperationKind::SameVolumeMove) {
        progress.total_operations = 1;
        progress.completed_operations = m_filesystem_operation.phase == FilesystemOperationPhase::Complete ? 1 : 0;
        progress.current_path = m_relocate_plan.destination_fatx_path;
    }
    progress.safe_point_samples = m_hdd_operation_safe_samples;
    if (m_hdd_operation_started_ms != 0) {
        progress.elapsed_ms = SDL_GetTicks() - m_hdd_operation_started_ms;
    }
    return progress;
}

bool GuestKernelRpcManager::PrepareHddDelete(
    char partition, const std::vector<std::string> &components, bool directory,
    std::vector<XemuKernelFs::DeleteEntry> &plan, std::string &error)
{
    error.clear();
    plan.clear();
    XemuFatxHdd::Snapshot snapshot;
    std::string snapshot_status;
    if (!hdd_snapshot_service.BuildRawPartitionSetSnapshot({partition}, snapshot, snapshot_status)) {
        error = snapshot_status.empty()
            ? "Xbox HDD snapshot is unavailable for Kernel Delete preflight."
            : snapshot_status;
        return false;
    }
    return XemuKernelFs::BuildDeletePlan(
        snapshot, partition, components, directory, plan, error);
}


bool GuestKernelRpcManager::StartHddDelete(
    char partition, const std::vector<std::string> &components, bool directory,
    const std::vector<XemuKernelFs::DeleteEntry> &confirmed_plan)
{
    // Every production start owns an explicit fresh operation context.
    ResetFilesystemOperationContext();
    if (IsBusy()) {
        m_status = "Another Guest Kernel RPC filesystem operation is already running.";
        return false;
    }
    if (m_breakpoint_installed || xemu_guest_rpc_arena_active()) {
        m_state = State::Failed;
        m_status = "A previous Kernel RPC cleanup is still owned; reset the Xbox/Xemu before retrying.";
        return false;
    }
    if (!runstate_is_running()) {
        m_state = State::Failed;
        m_status = "Resume the Xbox before starting the HDD Kernel Delete operation.";
        return false;
    }
    const CurrentGameManager::GameInfo &game = current_game_manager.Get();
    if (!game.valid || game.title_id == 0 || game.xbe_image_size == 0) {
        m_state = State::Failed;
        m_status = "A running XBE is required to borrow a safe PASSIVE_LEVEL thread for HDD Kernel RPC.";
        return false;
    }

    XemuFatxHdd::Snapshot snapshot;
    std::string snapshot_status;
    if (!hdd_snapshot_service.BuildRawPartitionSetSnapshot({partition}, snapshot, snapshot_status)) {
        m_state = State::Failed;
        m_status = snapshot_status.empty()
            ? "Xbox HDD snapshot is unavailable for Kernel Delete start preflight."
            : snapshot_status;
        return false;
    }
    std::vector<RecursiveDeleteEntry> fresh_plan;
    std::string error;
    if (!XemuKernelFs::BuildDeletePlan(snapshot, partition, components, directory,
                                       fresh_plan, error)) {
        m_state = State::Failed;
        m_status = error;
        return false;
    }
    if (confirmed_plan.empty() ||
        !XemuKernelFs::SameDeletePlan(confirmed_plan, fresh_plan)) {
        const XemuKernelFs::DeletePlanSummary summary =
            XemuKernelFs::SummarizeDeletePlan(fresh_plan);
        m_state = State::Failed;
        m_status = "HDD Kernel Delete contents changed after confirmation. The fresh plan is now " +
                   std::to_string(summary.file_count) + " file(s), " +
                   std::to_string(summary.directory_count) +
                   " folder(s). Nothing was deleted; open Delete again to review and reconfirm.";
        return false;
    }

    m_recursive_delete_plan = std::move(fresh_plan);
    m_fs_test_mode = FsTestMode::KernelDeleteRecursiveFolder;
    m_fs_partition = static_cast<char>(
        std::toupper(static_cast<unsigned char>(partition)));
    m_fs_title_id = game.title_id;
    m_fs_area.clear();
    m_fs_title_directory.clear();
    m_filesystem_operation.kind = FilesystemOperationKind::Delete;
    m_filesystem_operation.phase = FilesystemOperationPhase::Preparing;
    m_hdd_operation_started_ms = SDL_GetTicks();
    m_hdd_operation_safe_samples = 0;
    m_hdd_delete_root_directory = directory;
    m_path_diagnostics.clear();
    m_path_diagnostic_index = 0;
    m_export_address = 0;
    m_query_export_address = 0;
    m_delete_open_export_address = 0;
    m_delete_setinfo_export_address = 0;
    m_delete_close_export_address = 0;
    m_stub_address = 0;
    m_completion_address = 0;
    m_breakpoint_installed = false;
    m_arena = {};
    m_recursive_deleted_files = 0;
    m_recursive_deleted_directories = 0;
    m_was_running = true;
    if (!LoadRecursiveDeleteEntry(0u, error)) {
        m_state = State::Failed;
        m_status = error;
        return false;
    }
    m_state = State::WaitingSafePoint;
    m_filesystem_operation.phase = FilesystemOperationPhase::WaitingSafePoint;
    m_status = "HDD Kernel Delete plan confirmed. Waiting for PASSIVE_LEVEL to delete item 1 of " +
               std::to_string(m_recursive_delete_plan.size()) + " (leaf-first)...";
    return true;
}


bool GuestKernelRpcManager::PrepareHddImport(
    const std::string &host_path, bool host_is_directory, char partition,
    const std::vector<std::string> &destination_components,
    XemuKernelFs::TransferPlan &plan, std::string &error)
{
    error.clear();
    plan = {};

    XemuFatxHdd::Snapshot snapshot;
    std::string snapshot_status;
    if (!hdd_snapshot_service.BuildRawPartitionSetSnapshot({partition}, snapshot, snapshot_status)) {
        error = snapshot_status.empty()
            ? "Xbox HDD snapshot is unavailable for Kernel Import preflight."
            : snapshot_status;
        return false;
    }
    if (host_is_directory) {
        return XemuKernelFs::BuildImportFolderPlanAtDestination(
            host_path, snapshot, partition, destination_components, plan, error);
    }
    return XemuKernelFs::BuildImportFilePlanAtDestination(
        host_path, snapshot, partition, destination_components, plan, error);
}


bool GuestKernelRpcManager::PrepareHddCreateDirectory(
    const std::string &name, char partition,
    const std::vector<std::string> &destination_components,
    XemuKernelFs::TransferPlan &plan, std::string &error)
{
    error.clear();
    plan = {};
    XemuFatxHdd::Snapshot snapshot;
    std::string snapshot_status;
    if (!hdd_snapshot_service.BuildRawPartitionSetSnapshot({partition}, snapshot, snapshot_status)) {
        error = snapshot_status.empty()
            ? "Xbox HDD snapshot is unavailable for New Folder preflight."
            : snapshot_status;
        return false;
    }
    return XemuKernelFs::BuildCreateDirectoryPlanAtDestination(
        name, snapshot, partition, destination_components, plan, error);
}

bool GuestKernelRpcManager::PrepareHddRename(
    char partition, const std::vector<std::string> &components, bool directory,
    const std::string &new_name, XemuKernelFs::RelocatePlan &plan,
    std::string &error)
{
    XemuFatxHdd::Snapshot snapshot;
    std::string status;
    if (!hdd_snapshot_service.BuildRawPartitionSetSnapshot({partition}, snapshot, status)) {
        error = status.empty() ? "Xbox HDD snapshot is unavailable for Rename preflight." : status;
        return false;
    }
    if (components.empty()) {
        error = "A FATX volume root cannot be renamed.";
        return false;
    }
    std::vector<std::string> parent = components;
    parent.pop_back();
    return XemuKernelFs::BuildRelocatePlan(snapshot, partition, components,
                                           directory, parent, new_name,
                                           plan, error);
}

bool GuestKernelRpcManager::PrepareHddMove(
    char partition, const std::vector<std::string> &components, bool directory,
    const std::vector<std::string> &destination_parent,
    XemuKernelFs::RelocatePlan &plan, std::string &error)
{
    XemuFatxHdd::Snapshot snapshot;
    std::string status;
    if (!hdd_snapshot_service.BuildRawPartitionSetSnapshot({partition}, snapshot, status)) {
        error = status.empty() ? "Xbox HDD snapshot is unavailable for Move preflight." : status;
        return false;
    }
    if (components.empty()) {
        error = "A FATX volume root cannot be moved.";
        return false;
    }
    return XemuKernelFs::BuildRelocatePlan(snapshot, partition, components,
                                           directory, destination_parent,
                                           components.back(), plan, error);
}

bool GuestKernelRpcManager::StartHddRelocate(const XemuKernelFs::RelocatePlan &plan)
{
    ResetFilesystemOperationContext();
    std::string reason;
    if (!FilesystemReady(reason)) {
        m_state = State::Failed;
        m_status = reason;
        return false;
    }
    XemuFatxHdd::Snapshot snapshot;
    std::string status;
    if (!hdd_snapshot_service.BuildRawPartitionSetSnapshot({plan.partition}, snapshot, status)) {
        m_state = State::Failed;
        m_status = status.empty() ? "Xbox HDD snapshot is unavailable for Rename/Move start preflight." : status;
        return false;
    }
    std::vector<std::string> parent = plan.destination_components;
    if (parent.empty()) {
        m_state = State::Failed;
        m_status = "Rename/Move destination is invalid.";
        return false;
    }
    const std::string leaf = parent.back();
    parent.pop_back();
    XemuKernelFs::RelocatePlan fresh;
    std::string error;
    if (!XemuKernelFs::BuildRelocatePlan(snapshot, plan.partition,
                                          plan.source_components, plan.directory,
                                          parent, leaf, fresh, error) ||
        !XemuKernelFs::SameRelocatePlan(plan, fresh)) {
        m_state = State::Failed;
        m_status = "HDD Kernel Rename/Move changed after confirmation: " + error +
                   " Nothing was changed; review and reconfirm.";
        return false;
    }
    const CurrentGameManager::GameInfo &game = current_game_manager.Get();
    m_relocate_plan = std::move(fresh);
    m_fs_test_mode = FsTestMode::KernelRelocate;
    m_fs_partition = m_relocate_plan.partition;
    m_fs_title_id = game.title_id;
    const bool same_parent =
        m_relocate_plan.source_components.size() == m_relocate_plan.destination_components.size() &&
        std::equal(m_relocate_plan.source_components.begin(),
                   m_relocate_plan.source_components.end() - 1,
                   m_relocate_plan.destination_components.begin(),
                   [](const std::string &a, const std::string &b) {
                       return XemuKernelFs::EqualsNoCase(a, b);
                   });
    m_filesystem_operation.kind = same_parent
        ? FilesystemOperationKind::Rename
        : FilesystemOperationKind::SameVolumeMove;
    m_filesystem_operation.phase = FilesystemOperationPhase::Preparing;
    m_hdd_operation_started_ms = SDL_GetTicks();
    m_hdd_operation_safe_samples = 0;
    m_query_path = m_relocate_plan.source_native_path;
    m_fs_path = m_relocate_plan.source_fatx_path;
    m_delete_current_is_directory = m_relocate_plan.directory;
    m_export_address = 0;
    m_delete_open_export_address = 0;
    m_delete_setinfo_export_address = 0;
    m_delete_close_export_address = 0;
    m_breakpoint_installed = false;
    m_arena = {};
    m_delete_open_status = m_delete_setinfo_status = m_delete_close_status = 0xccccccccu;
    m_delete_operation_ran = 0;
    m_was_running = true;
    m_state = State::WaitingSafePoint;
    m_filesystem_operation.phase = FilesystemOperationPhase::WaitingSafePoint;
    m_status = "HDD Kernel Rename/Move confirmed. Waiting for PASSIVE_LEVEL...";
    return true;
}

bool GuestKernelRpcManager::PrepareHddCopy(
    char source_partition, const std::vector<std::string> &source_components,
    bool source_directory, char destination_partition,
    const std::vector<std::string> &destination_components,
    bool delete_source_after_copy, XemuKernelFs::TransferPlan &plan,
    std::string &error)
{
    XemuFatxHdd::Snapshot snapshot;
    std::string status;
    if (!hdd_snapshot_service.BuildRawPartitionSetSnapshot({source_partition, destination_partition}, snapshot, status)) {
        error = status.empty() ? "Xbox HDD snapshot is unavailable for Copy preflight." : status;
        return false;
    }
    return XemuKernelFs::BuildFatxCopyPlan(snapshot, source_partition,
        source_components, source_directory, destination_partition,
        destination_components, delete_source_after_copy, plan, error);
}

bool GuestKernelRpcManager::StartHddImport(const XemuKernelFs::TransferPlan &plan)
{
    ResetFilesystemOperationContext();
    if (IsBusy()) {
        m_status = "Another Guest Kernel RPC filesystem operation is already running.";
        return false;
    }
    if (m_breakpoint_installed || xemu_guest_rpc_arena_active()) {
        m_state = State::Failed;
        m_status = "A previous Kernel RPC cleanup is still owned; reset the Xbox/Xemu before retrying.";
        return false;
    }
    if (!runstate_is_running()) {
        m_state = State::Failed;
        m_status = "Resume the Xbox before starting the HDD Kernel Import operation.";
        return false;
    }
    const CurrentGameManager::GameInfo &game = current_game_manager.Get();
    if (!game.valid || game.title_id == 0 || game.xbe_image_size == 0) {
        m_state = State::Failed;
        m_status = "A running XBE is required to borrow a safe PASSIVE_LEVEL thread for HDD Kernel RPC.";
        return false;
    }
    if (plan.entries.empty() ||
        !XemuKernelFs::IsKernelWritablePartition(plan.partition)) {
        m_state = State::Failed;
        m_status = "The HDD Kernel Import plan is incomplete.";
        return false;
    }

    std::string error;
    XemuFatxHdd::Snapshot snapshot;
    std::string snapshot_status;
    if (!hdd_snapshot_service.BuildRawPartitionSetSnapshot(
            plan.source_from_fatx
                ? std::vector<char>{plan.source_partition, plan.partition}
                : std::vector<char>{plan.partition},
            snapshot, snapshot_status)) {
        m_state = State::Failed;
        m_status = snapshot_status.empty()
            ? "Xbox HDD snapshot is unavailable for Kernel Import start preflight."
            : snapshot_status;
        return false;
    }

    XemuKernelFs::TransferPlan fresh_plan;
    const bool rebuilt = plan.source_from_fatx
        ? XemuKernelFs::BuildFatxCopyPlan(
              snapshot, plan.source_partition, plan.source_components,
              plan.source_is_directory, plan.partition,
              plan.destination_components, plan.delete_source_after_copy,
              fresh_plan, error)
        : (plan.synthetic_directory
            ? XemuKernelFs::BuildCreateDirectoryPlanAtDestination(
                  plan.root_name, snapshot, plan.partition,
                  plan.destination_components, fresh_plan, error)
            : (plan.source_is_directory
                ? XemuKernelFs::BuildImportFolderPlanAtDestination(
                      plan.source_path, snapshot, plan.partition,
                      plan.destination_components, fresh_plan, error)
                : XemuKernelFs::BuildImportFilePlanAtDestination(
                      plan.source_path, snapshot, plan.partition,
                      plan.destination_components, fresh_plan, error)));
    if (!rebuilt) {
        m_state = State::Failed;
        m_status = "HDD Kernel Import changed after confirmation: " + error +
                   " Nothing was created; open Import again to review and reconfirm.";
        return false;
    }
    if (!XemuKernelFs::SameImportPlan(plan, fresh_plan)) {
        m_state = State::Failed;
        m_status = "HDD Kernel Import source changed after confirmation. Nothing was created; open Import again to review and reconfirm.";
        return false;
    }
    m_import_preflight = std::move(fresh_plan);

    HddSnapshotService::CapacityInfo capacity;
    if (!hdd_snapshot_service.QueryPartitionCapacity(
            m_import_preflight.partition, capacity, error)) {
        m_state = State::Failed;
        m_status = "HDD Kernel Import could not verify destination free space: " + error;
        return false;
    }
    const uint64_t required_bytes = XemuKernelFs::EstimateTransferRequiredBytes(
        m_import_preflight, capacity.bytes_per_cluster);
    if (required_bytes > capacity.free_bytes) {
        m_state = State::Failed;
        m_status = "HDD Kernel Import/Copy requires approximately " +
            std::to_string(required_bytes) + " FATX allocation byte(s), but only " +
            std::to_string(capacity.free_bytes) +
            " byte(s) are free. Nothing was created.";
        return false;
    }

    switch (m_import_preflight.kind) {
    case XemuKernelFs::TransferKind::CrossVolumeMove:
        m_filesystem_operation.kind = FilesystemOperationKind::CrossVolumeMove; break;
    case XemuKernelFs::TransferKind::FatxCopy:
        m_filesystem_operation.kind = FilesystemOperationKind::FatxCopy; break;
    case XemuKernelFs::TransferKind::CreateFatxDirectory:
        m_filesystem_operation.kind = FilesystemOperationKind::NewFolder; break;
    case XemuKernelFs::TransferKind::HostImport:
        m_filesystem_operation.kind = FilesystemOperationKind::HostImport; break;
    }
    m_filesystem_operation.phase = FilesystemOperationPhase::Preparing;
    m_hdd_operation_started_ms = SDL_GetTicks();
    m_hdd_operation_safe_samples = 0;

    m_fs_test_mode = FsTestMode::KernelImportFolder;
    m_fs_partition = m_import_preflight.partition;
    m_fs_title_id = game.title_id;
    m_fs_area.clear();
    m_fs_title_directory.clear();
    m_hdd_delete_root_directory = false;
    m_path_diagnostics.clear();
    m_path_diagnostic_index = 0;
    m_export_address = 0;
    m_query_export_address = 0;
    m_import_create_export_address = 0;
    m_import_write_export_address = 0;
    m_import_flush_export_address = 0;
    m_import_close_export_address = 0;
    m_stub_address = 0;
    m_completion_address = 0;
    m_breakpoint_installed = false;
    m_arena = {};
    m_import_host_stream.Reset();
    m_import_plan_index = 0;
    m_import_file_offset = 0;
    m_import_written_bytes = 0;
    m_import_completed_files = 0;
    m_import_completed_directories = 0;
    m_import_completed_operations = 0;
    m_was_running = true;

    if (!LoadKernelImportOperation(error)) {
        m_state = State::Failed;
        m_status = error;
        return false;
    }
    m_state = State::WaitingSafePoint;
    m_filesystem_operation.phase = FilesystemOperationPhase::WaitingSafePoint;
    m_status = "HDD Kernel Import plan confirmed. Waiting for PASSIVE_LEVEL to create operation 1 of " +
               std::to_string(m_import_preflight.total_operations) + "...";
    return true;
}

