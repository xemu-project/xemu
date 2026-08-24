#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "cheat-engine-memory.h"
#include "guest-kernel-rpc-memory.h"
#include "kernel-rpc-utils.hh"
#include "kernel-rpc-filesystem.hh"

class GuestKernelRpcManager
{
public:
    void Tick();
    void DrawTestPanel();

    // v2.15 HDD-browser frontend entry points. These reuse the proven async
    // PASSIVE_LEVEL executor; the browser owns only selection/confirmation UI.
    bool PrepareHddDelete(char partition,
                          const std::vector<std::string> &components,
                          bool directory,
                          std::vector<XemuKernelFs::DeleteEntry> &plan,
                          std::string &error);
    bool StartHddDelete(char partition,
                        const std::vector<std::string> &components,
                        bool directory,
                        const std::vector<XemuKernelFs::DeleteEntry> &confirmed_plan);
    bool PrepareHddImport(const std::string &host_path,
                          bool host_is_directory,
                          char partition,
                          const std::vector<std::string> &destination_components,
                          XemuKernelFs::ImportPlan &plan,
                          std::string &error);
    bool PrepareHddCreateDirectory(const std::string &name,
                                   char partition,
                                   const std::vector<std::string> &destination_components,
                                   XemuKernelFs::ImportPlan &plan,
                                   std::string &error);
    bool PrepareHddRename(char partition,
                          const std::vector<std::string> &components,
                          bool directory, const std::string &new_name,
                          XemuKernelFs::RelocatePlan &plan,
                          std::string &error);
    bool PrepareHddMove(char partition,
                        const std::vector<std::string> &components,
                        bool directory,
                        const std::vector<std::string> &destination_parent,
                        XemuKernelFs::RelocatePlan &plan,
                        std::string &error);
    bool StartHddRelocate(const XemuKernelFs::RelocatePlan &plan);
    bool PrepareHddCopy(char source_partition,
                        const std::vector<std::string> &source_components,
                        bool source_directory, char destination_partition,
                        const std::vector<std::string> &destination_components,
                        bool delete_source_after_copy,
                        XemuKernelFs::ImportPlan &plan, std::string &error);
    bool StartHddImport(const XemuKernelFs::ImportPlan &plan);
    struct HddOperationProgress {
        bool active = false;
        std::string label;
        std::string current_path;
        uint64_t completed_operations = 0;
        uint64_t total_operations = 0;
        uint64_t completed_bytes = 0;
        uint64_t total_bytes = 0;
        uint32_t completed_files = 0;
        uint32_t total_files = 0;
        uint32_t completed_directories = 0;
        uint32_t total_directories = 0;
        uint64_t elapsed_ms = 0;
        uint64_t safe_point_samples = 0;
    };

    bool OperationBusy() const { return IsBusy(); }
    bool FilesystemReady(std::string &reason) const;
    HddOperationProgress GetHddOperationProgress() const;
    const std::string &Status() const { return m_status; }

private:
    enum class State {
        Idle,
        Running,
        WaitingSafePoint,
        RunningFsQuery,
        Passed,
        Failed,
    };

    enum class FsTestMode {
        None,
        SingleReadOnlyFile,
        NamespaceDiagnostics,
        KernelDeleteRecursiveFolder,
        KernelImportFolder,
        KernelRelocate,
    };

    // Production HDD operation identity is kept separate from the low-level
    // RPC test mode. This prevents Copy/Move/Delete frontend state from being
    // represented by unrelated booleans that can leak across transactions.
    enum class FilesystemOperationKind {
        None,
        Delete,
        HostImport,
        NewFolder,
        FatxCopy,
        Rename,
        SameVolumeMove,
        CrossVolumeMove,
    };

    enum class FilesystemOperationPhase {
        Idle,
        Preparing,
        WaitingSafePoint,
        DeletingSource,
        Complete,
        Failed,
    };

    struct FilesystemOperationContext {
        FilesystemOperationKind kind = FilesystemOperationKind::None;
        FilesystemOperationPhase phase = FilesystemOperationPhase::Idle;
    };

    struct PathDiagnostic {
        std::string label;
        std::string path;
        uint32_t root_directory = 0;
        uint32_t ntstatus = 0xccccccccu;
        uint64_t file_size = 0;
        uint32_t attributes = 0;
        uint32_t safe_samples = 0;
        bool completed = false;
        bool query_ran = false;
    };

    // Filesystem plan entry ownership lives outside the test UI so the HDD
    // browser can reuse the same preflight/planning layer later.
    using RecursiveDeleteEntry = XemuKernelFs::DeleteEntry;
    using TransferEntry = XemuKernelFs::TransferEntry;

    State m_state = State::Idle;
    FsTestMode m_fs_test_mode = FsTestMode::None;
    std::string m_status = "Not tested.";
    XemuCheatX86Registers m_saved = {};
    XemuGuestRpcArenaInfo m_arena = {};
    uint32_t m_export_address = 0;
    uint32_t m_query_export_address = 0;
    uint32_t m_stub_address = 0;
    uint32_t m_completion_address = 0;
    uint32_t m_result_offset = 0x1000u;
    uint32_t m_irql = 0xffffffffu;
    bool m_was_running = false;
    bool m_breakpoint_installed = false;
    uint64_t m_started_ms = 0;
    uint64_t m_hdd_operation_started_ms = 0;
    uint64_t m_hdd_operation_safe_samples = 0;

    char m_fs_partition = 'E';
    FilesystemOperationContext m_filesystem_operation;
    bool m_hdd_delete_root_directory = false;
    std::string m_fs_path;
    std::string m_fs_area;
    std::string m_fs_title_directory;
    std::string m_fs_relative_path;
    uint32_t m_fs_title_id = 0;
    uint64_t m_expected_file_size = 0;
    uint32_t m_expected_attributes = 0;
    uint64_t m_kernel_file_size = 0;
    uint32_t m_kernel_attributes = 0;
    uint32_t m_query_status = 0xccccccccu;
    uint32_t m_query_ran = 0;
    uint32_t m_safe_point_attempts = 0;
    uint32_t m_last_sample_eip = 0;
    uint64_t m_safe_point_started_ms = 0;
    uint64_t m_next_safe_point_sample_ms = 0;

    std::string m_query_path;
    uint32_t m_query_root_directory = 0xfffffffdu;
    uint16_t m_query_ansi_length = 0;
    uint16_t m_query_ansi_maximum = 0;
    uint32_t m_query_ansi_buffer = 0;
    uint32_t m_query_object_name = 0;
    uint32_t m_query_object_attributes = 0x40u;
    std::vector<PathDiagnostic> m_path_diagnostics;
    size_t m_path_diagnostic_index = 0;

    uint32_t m_delete_open_export_address = 0;
    uint32_t m_delete_setinfo_export_address = 0;
    uint32_t m_delete_close_export_address = 0;
    uint32_t m_delete_open_status = 0xccccccccu;
    uint32_t m_delete_setinfo_status = 0xccccccccu;
    uint32_t m_delete_close_status = 0xccccccccu;
    uint32_t m_delete_operation_ran = 0;
    bool m_delete_current_is_directory = false;

    std::vector<RecursiveDeleteEntry> m_recursive_delete_plan;
    size_t m_recursive_delete_index = 0;
    uint32_t m_recursive_deleted_files = 0;
    uint32_t m_recursive_deleted_directories = 0;


    XemuKernelFs::TransferPlan m_import_preflight;
    size_t m_import_plan_index = 0;
    uint64_t m_import_file_offset = 0;
    std::vector<uint8_t> m_import_chunk;
    XemuKernelFs::ImportHostStream m_import_host_stream;
    uint32_t m_import_current_chunk_bytes = 0;
    uint64_t m_import_written_bytes = 0;
    uint64_t m_import_current_source_hash = XemuKernelFs::kContentHashBasis;
    uint32_t m_import_completed_files = 0;
    uint32_t m_import_completed_directories = 0;
    uint64_t m_import_completed_operations = 0;
    bool m_import_current_is_directory = false;
    uint32_t m_import_create_disposition = 0;
    uint32_t m_import_create_export_address = 0;
    uint32_t m_import_write_export_address = 0;
    uint32_t m_import_flush_export_address = 0;
    uint32_t m_import_close_export_address = 0;
    uint32_t m_import_create_status = 0xccccccccu;
    uint32_t m_import_write_status = 0xccccccccu;
    uint32_t m_import_flush_status = 0xccccccccu;
    uint32_t m_import_close_status = 0xccccccccu;
    uint32_t m_import_operation_ran = 0;
    uint32_t m_import_write_information = 0;

    XemuKernelFs::RelocatePlan m_relocate_plan;

    bool IsHddFrontendOperation() const
    {
        return m_filesystem_operation.kind != FilesystemOperationKind::None;
    }
    bool IsCrossVolumeMoveDeletePhase() const
    {
        return m_filesystem_operation.kind == FilesystemOperationKind::CrossVolumeMove &&
               m_filesystem_operation.phase == FilesystemOperationPhase::DeletingSource;
    }
    void ResetFilesystemOperationContext() { m_filesystem_operation = {}; }

    bool StartIrqlTest();
    bool StartReadOnlyFsTest();
    bool StartPathDiagnostics();
    bool LoadRecursiveDeleteEntry(size_t index, std::string &error);
    bool LoadKernelImportOperation(std::string &error);
    bool BeginReadOnlyFsAttempt();
    bool ResolveKernelDeleteExports(std::string &error);
    bool ResolveKernelImportExports(std::string &error);
    bool BeginKernelDeleteAttempt();
    bool BeginKernelImportAttempt();
    bool BeginKernelRelocateAttempt();
    void TickWaitingSafePoint();
    void HandleIrqlCompletion(const XemuCheatX86Registers &after);
    void HandleReadOnlyFsCompletion(const XemuCheatX86Registers &after);
    void HandleKernelDeleteCompletion(const XemuCheatX86Registers &after);
    void HandleKernelImportCompletion(const XemuCheatX86Registers &after);
    void HandleKernelRelocateCompletion(const XemuCheatX86Registers &after);
    bool CleanupAttemptForRetry(std::string &error);
    bool PrepareReadOnlyTarget(std::string &error);
    bool BuildPathDiagnostics(std::string &error);
    void ActivatePathDiagnostic(size_t index);
    void CompletePathDiagnostic(const XemuKernelRpc::FileNetworkOpenResult &parsed);
    bool ValidateCapturedContext(const XemuCheatX86Registers &regs,
                                 std::string &error) const;
    bool IsTitleExecutionPoint(uint32_t eip) const;
    bool IsBusy() const;

    void AbortBeforeRun(const std::string &message, bool restore_context);
    void Finish(bool success, const std::string &message,
                bool resume_original_state);
    bool RestoreSavedContext();
    static bool PreservedStateMatches(const XemuCheatX86Registers &before,
                                      const XemuCheatX86Registers &after,
                                      std::string &difference);
    static bool PreservedFilesystemStateMatches(
        const XemuCheatX86Registers &before,
        const XemuCheatX86Registers &after, std::string &difference);
};

extern GuestKernelRpcManager guest_kernel_rpc_manager;
