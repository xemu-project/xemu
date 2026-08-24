//
// Production HDD Kernel RPC completion handlers.
//
// Split from guest-kernel-rpc.cc in v2.41 so mutation result handling, fresh
// FATX verification, and transfer/delete phase transitions have one focused
// ownership unit while the core file retains RPC scheduling/diagnostics.
//
#include "qemu/osdep.h"

#include "guest-kernel-rpc.hh"
#include "guest-kernel-rpc-status.hh"
#include "hdd-snapshot-service.hh"
#include "kernel-rpc-filesystem.hh"
#include "kernel-rpc-utils.hh"

#include <SDL3/SDL.h>

#include <cstdio>
#include <string>
#include <vector>

namespace {

using XemuGuestKernelRpcStatus::IrqlName;
using XemuGuestKernelRpcStatus::NtStatusName;
using XemuGuestKernelRpcStatus::kDeleteIrqlOffset;
using XemuGuestKernelRpcStatus::kImportIrqlOffset;
using XemuGuestKernelRpcStatus::kSafePointSampleIntervalMs;


} // namespace

void GuestKernelRpcManager::HandleKernelDeleteCompletion(
    const XemuCheatX86Registers &after)
{
    uint8_t results[20] = {};
    if (!xemu_guest_rpc_arena_read(kDeleteIrqlOffset, results, sizeof(results))) {
        Finish(false, "Kernel Delete RPC reached completion but its result data could not be read.", false);
        return;
    }

    m_irql = XemuKernelRpc::Le32(results + 0) & 0xffu;
    m_delete_open_status = XemuKernelRpc::Le32(results + 4);
    m_delete_setinfo_status = XemuKernelRpc::Le32(results + 8);
    m_delete_close_status = XemuKernelRpc::Le32(results + 12);
    m_delete_operation_ran = XemuKernelRpc::Le32(results + 16);

    std::string difference;
    if (!PreservedFilesystemStateMatches(m_saved, after, difference)) {
        Finish(false,
               "Kernel Delete RPC preserved-state check failed: " + difference,
               false);
        return;
    }

    if (m_irql != 0 || m_delete_operation_ran == 0) {
        std::string cleanup_error;
        if (!CleanupAttemptForRetry(cleanup_error)) {
            m_state = State::Failed;
            m_status = "Kernel Delete safe-point probe returned " +
                       std::to_string(m_irql) + " (" + IrqlName(m_irql) +
                       "). " + cleanup_error;
            return;
        }
        m_state = State::WaitingSafePoint;
        m_next_safe_point_sample_ms = SDL_GetTicks() + kSafePointSampleIntervalMs;
        char text[384];
        std::snprintf(text, sizeof(text),
                      "Sample %u reached title code but IRQL was %u (%s); delete calls were skipped. Waiting for PASSIVE_LEVEL for the current %s...",
                      m_safe_point_attempts, m_irql, IrqlName(m_irql),
                      m_delete_current_is_directory ? "directory" : "file");
        m_status = text;
        return;
    }

    const bool open_success = (int32_t)m_delete_open_status >= 0;
    const bool set_success = open_success && (int32_t)m_delete_setinfo_status >= 0;
    const bool close_success = open_success && (int32_t)m_delete_close_status >= 0;
    const bool kernel_success = open_success && set_success && close_success;

    // The guest is still paused at the private completion breakpoint. Read the
    // raw HDD now, before resuming, to prove that the Xbox FATX driver made the
    // mutation visible to the block backend without a console reset.
    XemuFatxHdd::Snapshot verify_snapshot;
    std::string snapshot_status;
    const bool snapshot_available =
        hdd_snapshot_service.BuildRawPartitionSnapshot(
            m_fs_partition, verify_snapshot, snapshot_status);
    bool entry_present_after = true;
    const std::vector<std::string> *verify_components = nullptr;
    if (m_fs_test_mode == FsTestMode::KernelDeleteRecursiveFolder &&
        m_recursive_delete_index < m_recursive_delete_plan.size()) {
        verify_components = &m_recursive_delete_plan[m_recursive_delete_index].components;
    }
    if (snapshot_available && verify_components) {
        const XemuFatxHdd::Partition *partition =
            XemuFatxHdd::FindPartition(verify_snapshot, m_fs_partition);
        entry_present_after = partition &&
            XemuFatxHdd::FindEntry(*partition, *verify_components) != nullptr;
    }
    // Invalidate the browser snapshot whenever the direct FATX refresh proves
    // a delete mutation, even if a later NtClose/status check failed. A failed
    // RPC can still have changed the filesystem and the UI must not stay stale.
    const bool delete_mutation_observed = snapshot_available &&
        verify_components && !entry_present_after;
    if (kernel_success || delete_mutation_observed) {
        hdd_snapshot_service.NotifyFilesystemChanged();
    }

    if (m_fs_test_mode == FsTestMode::KernelDeleteRecursiveFolder) {
        const size_t item_number = m_recursive_delete_index + 1u;
        const size_t total = m_recursive_delete_plan.size();
        if (!kernel_success) {
            char text[896];
            std::snprintf(text, sizeof(text),
                          "Recursive Kernel Delete stopped at item %zu/%zu (%s): NtOpenFile=%08X (%s), NtSetInformationFile=%08X (%s), NtClose=%08X (%s). %u file(s) and %u folder(s) were already deleted. No raw FATX fallback and no Xbox reset were performed.",
                          item_number, total,
                          m_delete_current_is_directory ? "directory" : "file",
                          m_delete_open_status, NtStatusName(m_delete_open_status),
                          m_delete_setinfo_status, NtStatusName(m_delete_setinfo_status),
                          m_delete_close_status, NtStatusName(m_delete_close_status),
                          m_recursive_deleted_files, m_recursive_deleted_directories);
            Finish(false, text, true);
            return;
        }
        if (entry_present_after) {
            char text[768];
            std::snprintf(text, sizeof(text),
                          "Recursive Kernel Delete item %zu/%zu returned success, but the direct FATX snapshot still sees the %s. %u file(s) and %u folder(s) were already deleted. No raw fallback and no Xbox reset were performed.",
                          item_number, total,
                          m_delete_current_is_directory ? "directory" : "file",
                          m_recursive_deleted_files, m_recursive_deleted_directories);
            Finish(false, text, true);
            return;
        }

        if (m_delete_current_is_directory) {
            ++m_recursive_deleted_directories;
        } else {
            ++m_recursive_deleted_files;
        }

        const size_t next = m_recursive_delete_index + 1u;
        if (next >= total) {
            char text[896];
            if (IsCrossVolumeMoveDeletePhase()) {
                std::snprintf(text, sizeof(text),
                              "PASS: Cross-volume Move completed without reset: destination COPY was fully verified first, then the unchanged source was deleted leaf-first (%u file(s), %u folder(s), %zu delete operation(s)). No raw FATX fallback was used.",
                              m_recursive_deleted_files, m_recursive_deleted_directories, total);
            } else if (IsHddFrontendOperation()) {
                std::snprintf(
                    text, sizeof(text),
                    m_hdd_delete_root_directory
                        ? "PASS: Xbox kernel recursively deleted the selected HDD folder without a reset: %u file(s), %u folder(s), %zu total operation(s). Every delete step returned success and each entry disappeared from the direct FATX snapshot."
                        : "PASS: Xbox kernel deleted the selected HDD file without a reset: %u file(s), %u folder(s), %zu total operation(s). NtOpenFile -> NtSetInformationFile(Delete) -> NtClose returned success and the direct FATX snapshot confirms the file is gone.",
                    m_recursive_deleted_files, m_recursive_deleted_directories, total);
            } else {
                std::snprintf(text, sizeof(text),
                              "PASS: Xbox kernel recursively deleted the selected folder without a reset: %u file(s), %u folder(s), %zu total operation(s). Every NtOpenFile -> NtSetInformationFile(Delete) -> NtClose step returned success, each entry disappeared from the direct FATX snapshot, and CPU state matched every pre-call snapshot.",
                              m_recursive_deleted_files, m_recursive_deleted_directories,
                              total);
            }
            Finish(true, text, true);
            return;
        }

        std::string cleanup_error;
        if (!CleanupAttemptForRetry(cleanup_error)) {
            m_state = State::Failed;
            m_status = "Recursive Kernel Delete completed item " +
                       std::to_string(item_number) + "/" + std::to_string(total) +
                       ", but RPC cleanup failed. " + cleanup_error;
            return;
        }
        std::string load_error;
        if (!LoadRecursiveDeleteEntry(next, load_error)) {
            m_state = State::Failed;
            m_status = "Recursive Kernel Delete could not prepare the next leaf-first item: " + load_error;
            return;
        }
        m_state = State::WaitingSafePoint;
        m_status = "Deleted item " + std::to_string(item_number) + "/" +
                   std::to_string(total) + ". Waiting for PASSIVE_LEVEL for item " +
                   std::to_string(next + 1u) + "/" + std::to_string(total) +
                   " (" + (m_delete_current_is_directory ? "directory" : "file") + ")...";
        return;
    }

    char text[768];
    if (kernel_success && !entry_present_after) {
        std::snprintf(text, sizeof(text),
                      "PASS: Xbox kernel deleted the file without a reset. NtOpenFile=%08X, NtSetInformationFile=%08X, NtClose=%08X. Direct FATX refresh confirms the file is gone and CPU state matched the pre-call snapshot.",
                      m_delete_open_status, m_delete_setinfo_status,
                      m_delete_close_status);
        Finish(true, text, true);
    } else if (kernel_success) {
        std::snprintf(text, sizeof(text),
                      "Xbox kernel returned success for delete (NtOpenFile=%08X, NtSetInformationFile=%08X, NtClose=%08X), but the direct FATX snapshot still sees the file. No raw fallback and no Xbox reset were performed.",
                      m_delete_open_status, m_delete_setinfo_status,
                      m_delete_close_status);
        Finish(false, text, true);
    } else {
        std::snprintf(text, sizeof(text),
                      "Kernel Delete was refused/failed: NtOpenFile=%08X (%s), NtSetInformationFile=%08X (%s), NtClose=%08X (%s). File present after=%s. No raw FATX fallback and no Xbox reset were performed.",
                      m_delete_open_status, NtStatusName(m_delete_open_status),
                      m_delete_setinfo_status, NtStatusName(m_delete_setinfo_status),
                      m_delete_close_status, NtStatusName(m_delete_close_status),
                      entry_present_after ? "YES" : "NO");
        Finish(false, text, true);
    }
}




void GuestKernelRpcManager::HandleKernelRelocateCompletion(
    const XemuCheatX86Registers &after)
{
    uint8_t results[20] = {};
    if (!xemu_guest_rpc_arena_read(kDeleteIrqlOffset, results, sizeof(results))) {
        Finish(false, "Kernel Rename/Move RPC reached completion but result data could not be read.", false);
        return;
    }
    m_irql = XemuKernelRpc::Le32(results+0) & 0xffu;
    m_delete_open_status = XemuKernelRpc::Le32(results+4);
    m_delete_setinfo_status = XemuKernelRpc::Le32(results+8);
    m_delete_close_status = XemuKernelRpc::Le32(results+12);
    m_delete_operation_ran = XemuKernelRpc::Le32(results+16);
    std::string difference;
    if (!PreservedFilesystemStateMatches(m_saved, after, difference)) {
        Finish(false, "Kernel Rename/Move preserved-state check failed: " + difference, false);
        return;
    }
    if (m_irql != 0 || m_delete_operation_ran == 0) {
        std::string cleanup_error;
        if (!CleanupAttemptForRetry(cleanup_error)) {
            m_state=State::Failed; m_status="Kernel Rename/Move safe-point cleanup failed. "+cleanup_error; return;
        }
        m_state=State::WaitingSafePoint;
        m_next_safe_point_sample_ms=SDL_GetTicks()+kSafePointSampleIntervalMs;
        m_status="Rename/Move skipped because the sampled title thread was not at PASSIVE_LEVEL; retrying...";
        return;
    }
    const bool kernel_success=(int32_t)m_delete_open_status>=0 &&
        (int32_t)m_delete_setinfo_status>=0 && (int32_t)m_delete_close_status>=0;
    XemuFatxHdd::Snapshot verify;
    std::string status;
    const bool snap=hdd_snapshot_service.BuildRawPartitionSnapshot(
        m_relocate_plan.partition, verify, status);
    const XemuFatxHdd::Partition *volume=snap ? XemuFatxHdd::FindPartition(verify,m_relocate_plan.partition) : nullptr;
    const XemuFatxHdd::Entry *old_entry=volume ? XemuFatxHdd::FindEntry(*volume,m_relocate_plan.source_components) : nullptr;
    const XemuFatxHdd::Entry *new_entry=volume ? XemuFatxHdd::FindEntry(*volume,m_relocate_plan.destination_components) : nullptr;
    const bool verify_ok=kernel_success && old_entry==nullptr && new_entry &&
        new_entry->directory==m_relocate_plan.directory &&
        new_entry->first_cluster==m_relocate_plan.first_cluster &&
        (m_relocate_plan.directory || new_entry->file_size==m_relocate_plan.file_size);
    const bool relocate_mutation_observed = snap &&
        (old_entry == nullptr || new_entry != nullptr);
    if (relocate_mutation_observed) {
        hdd_snapshot_service.NotifyFilesystemChanged();
    }
    char text[768];
    if (verify_ok) {
        std::snprintf(text,sizeof(text),
            "PASS: Xbox kernel renamed/moved %s -> %s without reset. NtOpenFile=%08X, NtSetInformationFile=%08X, NtClose=%08X. Fresh FATX verification confirms the source is gone and destination identity matches.",
            m_relocate_plan.source_fatx_path.c_str(), m_relocate_plan.destination_fatx_path.c_str(),
            m_delete_open_status,m_delete_setinfo_status,m_delete_close_status);
        Finish(true,text,true);
    } else {
        std::snprintf(text,sizeof(text),
            "Kernel Rename/Move failed verification: NtOpenFile=%08X, NtSetInformationFile=%08X, NtClose=%08X, source present=%s, destination present=%s. No raw FATX fallback was used.",
            m_delete_open_status,m_delete_setinfo_status,m_delete_close_status,
            old_entry?"YES":"NO",new_entry?"YES":"NO");
        Finish(false,text,true);
    }
}



void GuestKernelRpcManager::HandleKernelImportCompletion(
    const XemuCheatX86Registers &after)
{
    uint8_t results[28] = {};
    if (!xemu_guest_rpc_arena_read(kImportIrqlOffset, results, sizeof(results))) {
        Finish(false, "Kernel Import RPC reached completion but its result data could not be read.", false);
        return;
    }

    m_irql = XemuKernelRpc::Le32(results + 0) & 0xffu;
    m_import_create_status = XemuKernelRpc::Le32(results + 4);
    m_import_write_status = XemuKernelRpc::Le32(results + 8);
    m_import_flush_status = XemuKernelRpc::Le32(results + 12);
    m_import_close_status = XemuKernelRpc::Le32(results + 16);
    m_import_operation_ran = XemuKernelRpc::Le32(results + 20);
    m_import_write_information = XemuKernelRpc::Le32(results + 24);

    std::string difference;
    if (!PreservedFilesystemStateMatches(m_saved, after, difference)) {
        Finish(false,
               "Kernel Import RPC preserved-state check failed: " + difference,
               false);
        return;
    }

    if (m_irql != 0 || m_import_operation_ran == 0) {
        std::string cleanup_error;
        if (!CleanupAttemptForRetry(cleanup_error)) {
            m_state = State::Failed;
            m_status = "Kernel Import safe-point probe returned " +
                       std::to_string(m_irql) + " (" + IrqlName(m_irql) +
                       "). " + cleanup_error;
            return;
        }
        m_state = State::WaitingSafePoint;
        m_next_safe_point_sample_ms = SDL_GetTicks() + kSafePointSampleIntervalMs;
        char text[384];
        std::snprintf(text, sizeof(text),
                      "Sample %u reached title code but IRQL was %u (%s); create/write calls were skipped. Waiting for PASSIVE_LEVEL for the current %s...",
                      m_safe_point_attempts, m_irql, IrqlName(m_irql),
                      m_import_current_is_directory ? "directory" : "file chunk");
        m_status = text;
        return;
    }

    const auto completed_successfully = [](uint32_t status) {
        return (int32_t)status >= 0 && status != 0x00000103u; // STATUS_PENDING is not completion.
    };
    const bool create_success = completed_successfully(m_import_create_status);
    const bool write_success = m_import_current_is_directory ||
        completed_successfully(m_import_write_status);
    const bool write_size_match = m_import_current_is_directory ||
        m_import_current_chunk_bytes == 0u ||
        m_import_write_information == m_import_current_chunk_bytes;
    const bool flush_success = m_import_current_is_directory ||
        completed_successfully(m_import_flush_status);
    const bool close_success = create_success &&
        completed_successfully(m_import_close_status);
    const bool kernel_success = create_success && write_success &&
                                write_size_match && flush_success && close_success;

    XemuFatxHdd::Snapshot verify_snapshot;
    std::string snapshot_status;
    const bool snapshot_available =
        hdd_snapshot_service.BuildRawPartitionSnapshot(
            m_import_preflight.partition, verify_snapshot, snapshot_status);
    bool snapshot_verified = false;
    bool entry_present = false;
    bool type_matches = false;
    bool size_matches = false;
    if (snapshot_available &&
        m_import_plan_index < m_import_preflight.entries.size()) {
        const XemuFatxHdd::Partition *partition =
            XemuFatxHdd::FindPartition(verify_snapshot, m_import_preflight.partition);
        const TransferEntry &current = m_import_preflight.entries[m_import_plan_index];
        const XemuFatxHdd::Entry *entry = partition
            ? XemuFatxHdd::FindEntry(*partition, current.components) : nullptr;
        snapshot_verified = partition && partition->available;
        entry_present = entry != nullptr;
        if (entry) {
            type_matches = entry->directory == m_import_current_is_directory;
            size_matches = m_import_current_is_directory ||
                           entry->file_size == m_expected_file_size;
        }
    }

    // Create/Write can be visible even when Flush/Close later fails. Any
    // freshly observed destination entry means the browser snapshot is stale.
    const bool import_mutation_observed = snapshot_available && entry_present;
    if (kernel_success || import_mutation_observed) {
        hdd_snapshot_service.NotifyFilesystemChanged();
    }
    const uint64_t operation_number = m_import_completed_operations + 1u;
    if (!kernel_success || !snapshot_verified || !entry_present ||
        !type_matches || !size_matches) {
        char text[1200];
        std::snprintf(text, sizeof(text),
                      "Kernel Import stopped at operation %llu/%llu (%s): NtCreateFile=%08X (%s), NtWriteFile=%08X (%s), bytes=%u/%u, NtFlushBuffersFile=%08X (%s), NtClose=%08X (%s). FATX verify: snapshot=%s present=%s type=%s size=%s. Already created: %u file(s), %u folder(s), %llu byte(s). Existing entries were never overwritten; any items already created remain on the HDD.",
                      (unsigned long long)operation_number,
                      (unsigned long long)m_import_preflight.total_operations,
                      m_import_current_is_directory ? "directory" : "file chunk",
                      m_import_create_status, NtStatusName(m_import_create_status),
                      m_import_write_status, NtStatusName(m_import_write_status),
                      m_import_write_information, m_import_current_chunk_bytes,
                      m_import_flush_status, NtStatusName(m_import_flush_status),
                      m_import_close_status, NtStatusName(m_import_close_status),
                      snapshot_verified ? "YES" : "NO",
                      entry_present ? "YES" : "NO",
                      type_matches ? "YES" : "NO",
                      size_matches ? "YES" : "NO",
                      m_import_completed_files, m_import_completed_directories,
                      (unsigned long long)m_import_written_bytes);
        Finish(false, text, true);
        return;
    }

    ++m_import_completed_operations;
    const TransferEntry &current = m_import_preflight.entries[m_import_plan_index];
    if (m_import_current_is_directory) {
        ++m_import_completed_directories;
        ++m_import_plan_index;
        m_import_file_offset = 0;
    } else {
        m_import_written_bytes += m_import_current_chunk_bytes;
        const uint64_t next_offset = m_import_file_offset + m_import_current_chunk_bytes;
        if (current.file_size == 0u || next_offset >= current.file_size) {
            if (!current.source_from_fatx &&
                m_import_current_source_hash != current.host_content_hash) {
                Finish(false,
                    "Kernel Import stopped because the host file content changed while it was being copied. The created destination was kept for inspection and no existing FATX item was overwritten.",
                    true);
                return;
            }
            ++m_import_completed_files;
            ++m_import_plan_index;
            m_import_file_offset = 0;
            m_import_current_source_hash = XemuKernelFs::kContentHashBasis;
        } else {
            m_import_file_offset = next_offset;
        }
    }

    if (m_import_plan_index >= m_import_preflight.entries.size()) {
        if (m_import_preflight.source_from_fatx) {
            std::vector<HddSnapshotService::FatxCompareItem> compare_items;
            compare_items.reserve(m_import_preflight.entries.size());
            for (const TransferEntry &item : m_import_preflight.entries) {
                HddSnapshotService::FatxCompareItem compare;
                compare.source_partition = item.source_partition;
                compare.source_components = item.source_components;
                compare.source_directory_entry_offset = item.source_directory_entry_offset;
                compare.source_first_cluster = item.source_first_cluster;
                compare.source_modified_time = item.source_modified_time;
                compare.source_modified_date = item.source_modified_date;
                compare.source_attributes = item.source_attributes;
                compare.destination_partition = m_import_preflight.partition;
                compare.destination_components = item.components;
                compare.file_size = item.file_size;
                compare.directory = item.directory;
                compare_items.push_back(std::move(compare));
            }
            std::string compare_error;
            if (!hdd_snapshot_service.VerifyFatxCopyContents(compare_items, compare_error)) {
                Finish(false,
                    "COPY writes completed with per-operation FATX verification, but the final whole-tree/source-data verification failed. The source was NOT deleted. " + compare_error,
                    true);
                return;
            }
        }
        if (m_import_preflight.delete_source_after_copy) {
            XemuFatxHdd::Snapshot source_snapshot;
            std::string source_status;
            std::vector<RecursiveDeleteEntry> source_plan;
            std::string source_error;
            const bool source_unchanged =
                hdd_snapshot_service.BuildRawPartitionSnapshot(
                    m_import_preflight.source_partition, source_snapshot, source_status) &&
                XemuKernelFs::BuildDeletePlan(
                    source_snapshot, m_import_preflight.source_partition,
                    m_import_preflight.source_components,
                    m_import_preflight.source_is_directory, source_plan, source_error) &&
                XemuKernelFs::SameDeletePlan(
                    m_import_preflight.source_delete_plan, source_plan);
            if (!source_unchanged) {
                Finish(false,
                    "COPY completed and every destination item verified, but the source changed before the cross-volume Move delete phase. The verified destination was kept and the source was NOT deleted.",
                    true);
                return;
            }
            std::string cleanup_error;
            if (!CleanupAttemptForRetry(cleanup_error)) {
                m_state = State::Failed;
                m_status = "COPY completed and verified, but RPC cleanup failed before the cross-volume Move delete phase. Source was not deleted. " + cleanup_error;
                return;
            }
            m_recursive_delete_plan = std::move(source_plan);
            m_recursive_delete_index = 0;
            m_recursive_deleted_files = 0;
            m_recursive_deleted_directories = 0;
            m_fs_test_mode = FsTestMode::KernelDeleteRecursiveFolder;
            m_fs_partition = m_import_preflight.source_partition;
            m_hdd_delete_root_directory = m_import_preflight.source_is_directory;
            m_filesystem_operation.phase = FilesystemOperationPhase::DeletingSource;
            m_export_address = 0;
            m_delete_open_export_address = 0;
            m_delete_setinfo_export_address = 0;
            m_delete_close_export_address = 0;
            std::string load_error;
            if (!LoadRecursiveDeleteEntry(0u, load_error)) {
                m_state = State::Failed;
                m_status = "COPY completed and verified, but the cross-volume Move source delete could not start. Source was not deleted: " + load_error;
                return;
            }
            m_state = State::WaitingSafePoint;
            m_status = "COPY phase verified completely. Waiting for PASSIVE_LEVEL to delete the unchanged source tree and finish the cross-volume Move...";
            return;
        }
        char text[1024];
        if (IsHddFrontendOperation() && m_import_preflight.source_from_fatx) {
            std::snprintf(text, sizeof(text),
                          "PASS: Xbox kernel copied the selected FATX %s to %c: without a reset: %u file(s), %u folder(s), %llu byte(s), %llu kernel operation(s). Every destination entry passed fresh FATX verification, the complete directory tree exists, and all file data matched the unchanged FATX source byte-for-byte.",
                          m_import_preflight.source_is_directory ? "folder" : "file",
                          m_import_preflight.partition,
                          m_import_completed_files, m_import_completed_directories,
                          (unsigned long long)m_import_written_bytes,
                          (unsigned long long)m_import_completed_operations);
        } else if (IsHddFrontendOperation()) {
            std::snprintf(text, sizeof(text),
                          "PASS: Xbox kernel imported the selected host %s to %c: without a reset: %u file(s), %u folder(s), %llu byte(s), %llu kernel operation(s). Direct FATX refresh verified every created item; no existing destination was merged/overwritten and no raw FATX write fallback was used.",
                          m_import_preflight.source_is_directory ? "folder" : "file",
                          m_import_preflight.partition,
                          m_import_completed_files, m_import_completed_directories,
                          (unsigned long long)m_import_written_bytes,
                          (unsigned long long)m_import_completed_operations);
        } else {
            std::snprintf(text, sizeof(text),
                          "PASS: Xbox kernel imported the host folder without a reset: %u file(s), %u folder(s), %llu byte(s), %llu kernel operation(s). Direct FATX refresh verified every created directory and every written file size. The Kernel Import path did not overwrite or merge an existing destination and used no raw FATX writes.",
                          m_import_completed_files, m_import_completed_directories,
                          (unsigned long long)m_import_written_bytes,
                          (unsigned long long)m_import_completed_operations);
        }
        Finish(true, text, true);
        return;
    }

    std::string cleanup_error;
    if (!CleanupAttemptForRetry(cleanup_error)) {
        m_state = State::Failed;
        m_status = "Kernel Import completed operation " +
                   std::to_string(m_import_completed_operations) + "/" +
                   std::to_string(m_import_preflight.total_operations) +
                   ", but RPC cleanup failed. " + cleanup_error;
        return;
    }

    std::string load_error;
    if (!LoadKernelImportOperation(load_error)) {
        m_state = State::Failed;
        m_status = "Kernel Import could not prepare the next operation: " + load_error;
        return;
    }
    m_state = State::WaitingSafePoint;
    m_status = "Created/wrote operation " +
               std::to_string(m_import_completed_operations) + "/" +
               std::to_string(m_import_preflight.total_operations) +
               ". Waiting for PASSIVE_LEVEL for operation " +
               std::to_string(m_import_completed_operations + 1u) + "/" +
               std::to_string(m_import_preflight.total_operations) + " (" +
               (m_import_current_is_directory ? "directory" : "file chunk") + ")...";
}


