#include "qemu/osdep.h"

#include "guest-kernel-rpc.hh"
#include "guest-kernel-rpc-status.hh"
#include "current-game.hh"
#include "hdd-snapshot-service.hh"
#include "kernel-rpc-utils.hh"
#include "kernel-rpc-filesystem.hh"

extern "C" {
#include "system/runstate.h"
}

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

GuestKernelRpcManager guest_kernel_rpc_manager;

namespace {

using XemuGuestKernelRpcStatus::IrqlName;
using XemuGuestKernelRpcStatus::NtStatusName;
using XemuGuestKernelRpcStatus::kDeleteIrqlOffset;
using XemuGuestKernelRpcStatus::kImportIrqlOffset;
using XemuGuestKernelRpcStatus::kSafePointSampleIntervalMs;

using XemuKernelFs::EqualsNoCase;
using XemuKernelFs::FindChildNoCase;

constexpr uint32_t kStackSafetyBytes = 64u;
constexpr uint32_t kFsIrqlOffset = 0x1000u;
constexpr uint32_t kFsStatusOffset = 0x1004u;
constexpr uint32_t kFsQueryRanOffset = 0x1008u;
constexpr uint32_t kFsAnsiStringOffset = 0x1100u;
constexpr uint32_t kFsObjectAttributesOffset = 0x1120u;
constexpr uint32_t kFsFileInformationOffset = 0x1140u;
constexpr uint32_t kFsPathOffset = 0x1200u;
constexpr size_t kFsFileInformationBytes = 64u;

constexpr uint32_t kDeleteOpenStatusOffset = 0x1004u;
constexpr uint32_t kDeleteSetInfoStatusOffset = 0x1008u;
constexpr uint32_t kDeleteCloseStatusOffset = 0x100cu;
constexpr uint32_t kDeleteOperationRanOffset = 0x1010u;
constexpr uint32_t kDeleteFileHandleOffset = 0x1014u;
constexpr uint32_t kDeleteIoStatusBlockOffset = 0x1020u;
constexpr uint32_t kDeleteDispositionOffset = 0x1030u;
constexpr uint32_t kDeleteAnsiStringOffset = 0x1100u;
constexpr uint32_t kDeleteObjectAttributesOffset = 0x1120u;
constexpr uint32_t kDeletePathOffset = 0x1200u;

constexpr uint32_t kImportCreateStatusOffset = 0x1004u;
constexpr uint32_t kImportWriteStatusOffset = 0x1008u;
constexpr uint32_t kImportFlushStatusOffset = 0x100cu;
constexpr uint32_t kImportCloseStatusOffset = 0x1010u;
constexpr uint32_t kImportOperationRanOffset = 0x1014u;
constexpr uint32_t kImportWriteInformationOffset = 0x1018u;
constexpr uint32_t kImportFileHandleOffset = 0x1020u;
constexpr uint32_t kImportIoStatusBlockOffset = 0x1030u;
constexpr uint32_t kImportByteOffsetOffset = 0x1040u;
constexpr uint32_t kImportAnsiStringOffset = 0x1100u;
constexpr uint32_t kImportObjectAttributesOffset = 0x1120u;
constexpr uint32_t kImportPathOffset = 0x1200u;
constexpr uint32_t kImportDataOffset = 0x2000u;

constexpr uint32_t kRenameInformationOffset = 0x1040u;
constexpr uint32_t kRenameSourceAnsiOffset = 0x1100u;
constexpr uint32_t kRenameSourceObjectAttributesOffset = 0x1120u;
constexpr uint32_t kRenameDestinationAnsiOffset = 0x1140u;
constexpr uint32_t kRenameSourcePathOffset = 0x1200u;
constexpr uint32_t kRenameDestinationPathOffset = 0x1600u;

constexpr uint64_t kSafePointTimeoutMs = 5000u;

bool ReadGuest(uint32_t address, void *buffer, size_t size)
{
    return xemu_cheat_memory_read(1, address, buffer, size) != 0;
}

bool SameField(const char *name, uint32_t a, uint32_t b, std::string &difference)
{
    if (a == b) {
        return true;
    }
    char text[96];
    std::snprintf(text, sizeof(text), "%s changed: %08X -> %08X",
                  name, a, b);
    difference = text;
    return false;
}

bool SameMaskedField(const char *name, uint32_t a, uint32_t b, uint32_t mask,
                     std::string &difference)
{
    if ((a & mask) == (b & mask)) {
        return true;
    }
    char text[128];
    std::snprintf(text, sizeof(text),
                  "%s invariant bits changed: %08X -> %08X (mask %08X)",
                  name, a, b, mask);
    difference = text;
    return false;
}

bool FindFirstFile(const XemuFatxHdd::Entry &entry,
                   std::vector<std::string> &relative_path,
                   const XemuFatxHdd::Entry *&file,
                   unsigned depth = 0)
{
    if (depth > 16) {
        return false;
    }
    if (!entry.directory) {
        file = &entry;
        return true;
    }
    for (const XemuFatxHdd::Entry &child : entry.children) {
        relative_path.push_back(child.name);
        if (FindFirstFile(child, relative_path, file, depth + 1)) {
            return true;
        }
        relative_path.pop_back();
    }
    return false;
}




} // namespace

bool GuestKernelRpcManager::PreservedStateMatches(
    const XemuCheatX86Registers &a, const XemuCheatX86Registers &b,
    std::string &difference)
{
#define RPC_CHECK(field) if (!SameField(#field, a.field, b.field, difference)) return false
    RPC_CHECK(eax); RPC_CHECK(ebx); RPC_CHECK(ecx); RPC_CHECK(edx);
    RPC_CHECK(esi); RPC_CHECK(edi); RPC_CHECK(esp); RPC_CHECK(ebp);
    RPC_CHECK(eflags);
    RPC_CHECK(cs); RPC_CHECK(ds); RPC_CHECK(es); RPC_CHECK(fs);
    RPC_CHECK(gs); RPC_CHECK(ss);
    RPC_CHECK(cr0); RPC_CHECK(cr2); RPC_CHECK(cr3); RPC_CHECK(cr4);
#undef RPC_CHECK
    return true;
}

bool GuestKernelRpcManager::PreservedFilesystemStateMatches(
    const XemuCheatX86Registers &a, const XemuCheatX86Registers &b,
    std::string &difference)
{
#define RPC_FS_CHECK(field) if (!SameField(#field, a.field, b.field, difference)) return false
    RPC_FS_CHECK(eax); RPC_FS_CHECK(ebx); RPC_FS_CHECK(ecx); RPC_FS_CHECK(edx);
    RPC_FS_CHECK(esi); RPC_FS_CHECK(edi); RPC_FS_CHECK(esp); RPC_FS_CHECK(ebp);
    RPC_FS_CHECK(eflags);
    RPC_FS_CHECK(cs); RPC_FS_CHECK(ds); RPC_FS_CHECK(es); RPC_FS_CHECK(fs);
    RPC_FS_CHECK(gs); RPC_FS_CHECK(ss);
    // The Xbox kernel may legitimately update CR0's MP/EM/TS floating-point
    // task-management bits while servicing/scheduling a filesystem call.  They
    // are kernel-owned lazy-FPU state, not state touched by our RPC stub.  Keep
    // every other CR0 bit invariant and continue to require exact CR3/CR4.
    if (!SameMaskedField("cr0", a.cr0, b.cr0, ~0x0000000eu, difference)) return false;
    RPC_FS_CHECK(cr3); RPC_FS_CHECK(cr4);
#undef RPC_FS_CHECK
    // CR2 records the most recent page-fault linear address and may legitimately
    // change during a synchronous filesystem call if the kernel faults in code
    // or data. It is diagnostic state, not part of the borrowed thread's
    // execution context that pushad/pushfd can preserve.
    return true;
}

bool GuestKernelRpcManager::RestoreSavedContext()
{
    struct RegisterValue { const char *name; uint32_t value; };
    const RegisterValue values[] = {
        {"eax", m_saved.eax}, {"ebx", m_saved.ebx},
        {"ecx", m_saved.ecx}, {"edx", m_saved.edx},
        {"esi", m_saved.esi}, {"edi", m_saved.edi},
        {"esp", m_saved.esp}, {"ebp", m_saved.ebp},
        {"eflags", m_saved.eflags}, {"eip", m_saved.eip},
    };
    for (const RegisterValue &entry : values) {
        if (!xemu_cheat_set_x86_register(entry.name, entry.value)) {
            return false;
        }
    }
    return true;
}

bool GuestKernelRpcManager::IsBusy() const
{
    return m_state == State::Running ||
           m_state == State::WaitingSafePoint ||
           m_state == State::RunningFsQuery;
}

bool GuestKernelRpcManager::IsTitleExecutionPoint(uint32_t eip) const
{
    const CurrentGameManager::GameInfo &game = current_game_manager.Get();
    if (!game.valid || game.xbe_image_size == 0) {
        return false;
    }
    const uint64_t begin = game.xbe_base;
    const uint64_t end = begin + game.xbe_image_size;
    return end <= 0x100000000ull && eip >= begin && (uint64_t)eip < end;
}

bool GuestKernelRpcManager::ValidateCapturedContext(
    const XemuCheatX86Registers &regs, std::string &error) const
{
    error.clear();
    if ((regs.cs & 3u) != 0) {
        error = "Kernel RPC requires a ring-0 Xbox execution context.";
        return false;
    }
    if (regs.eip != regs.pc) {
        error = "Kernel RPC requires the normal flat Xbox code-segment mapping (EIP == PC).";
        return false;
    }
    if (regs.esp < kStackSafetyBytes || !xemu_cheat_prepare_virtual_map()) {
        error = "Could not validate the current Xbox stack for Kernel RPC.";
        return false;
    }

    uint64_t stack_low_physical = 0;
    uint64_t stack_high_physical = 0;
    const uint32_t stack_low = regs.esp - kStackSafetyBytes;
    const uint32_t stack_high = regs.esp - 1u;
    const uint64_t ram_size = xemu_cheat_ram_size();
    if (!xemu_cheat_virtual_to_physical(stack_low, &stack_low_physical) ||
        !xemu_cheat_virtual_to_physical(stack_high, &stack_high_physical) ||
        stack_low_physical >= ram_size || stack_high_physical >= ram_size) {
        error = "Kernel RPC stack preflight did not resolve into installed Xbox RAM.";
        return false;
    }
    return true;
}

void GuestKernelRpcManager::AbortBeforeRun(const std::string &message,
                                           bool restore_context)
{
    m_import_host_stream.Reset();
    bool breakpoint_removed = true;
    if (m_breakpoint_installed) {
        breakpoint_removed =
            xemu_cheat_breakpoint_remove(m_completion_address) != 0;
        if (breakpoint_removed) {
            m_breakpoint_installed = false;
        }
    }
    const bool context_restored = !restore_context || RestoreSavedContext();
    const bool arena_released = !xemu_guest_rpc_arena_active() ||
                                xemu_guest_rpc_arena_release() != 0;

    m_state = State::Failed;
    m_status = message;
    if (!breakpoint_removed) {
        m_status += " Completion breakpoint cleanup failed.";
    }
    if (!context_restored) {
        m_status += " CPU context restoration failed.";
    }
    if (!arena_released) {
        const char *error = xemu_guest_rpc_arena_last_error();
        m_status += " RPC mapping rollback failed";
        if (error != nullptr) {
            m_status += ": ";
            m_status += error;
        }
        m_status += ".";
    }

    if (m_was_running && breakpoint_removed && context_restored &&
        arena_released && !xemu_guest_rpc_arena_active()) {
        vm_start();
    } else if (!breakpoint_removed || !context_restored || !arena_released) {
        m_status += " Xbox left paused; reset before another RPC test.";
    }
}

void GuestKernelRpcManager::Finish(bool success, const std::string &message,
                                   bool resume_original_state)
{
    m_import_host_stream.Reset();
    bool breakpoint_removed = true;
    if (m_breakpoint_installed) {
        breakpoint_removed =
            xemu_cheat_breakpoint_remove(m_completion_address) != 0;
        if (breakpoint_removed) {
            m_breakpoint_installed = false;
        }
    }

    const bool context_restored = RestoreSavedContext();
    const bool arena_released = xemu_guest_rpc_arena_release() != 0;

    m_state = success && breakpoint_removed && context_restored && arena_released
                  ? State::Passed : State::Failed;
    if (IsHddFrontendOperation()) {
        m_filesystem_operation.phase =
            m_state == State::Passed ? FilesystemOperationPhase::Complete
                                     : FilesystemOperationPhase::Failed;
    }
    m_status = message;
    if (!breakpoint_removed) {
        m_status += " Completion breakpoint cleanup failed; Xbox left paused.";
    }
    if (!context_restored) {
        m_status += " CPU context restoration failed; Xbox left paused.";
    }
    if (!arena_released) {
        const char *error = xemu_guest_rpc_arena_last_error();
        m_status += " RPC mapping cleanup failed";
        if (error != nullptr) {
            m_status += ": ";
            m_status += error;
        }
        m_status += ". Xbox left paused.";
    }

    if (resume_original_state && m_was_running && breakpoint_removed &&
        context_restored && arena_released) {
        vm_start();
    }
}

bool GuestKernelRpcManager::CleanupAttemptForRetry(std::string &error)
{
    error.clear();
    bool breakpoint_removed = true;
    if (m_breakpoint_installed) {
        breakpoint_removed =
            xemu_cheat_breakpoint_remove(m_completion_address) != 0;
        if (breakpoint_removed) {
            m_breakpoint_installed = false;
        }
    }
    const bool context_restored = RestoreSavedContext();
    const bool arena_released = xemu_guest_rpc_arena_release() != 0;
    if (!breakpoint_removed) {
        error += "Completion breakpoint cleanup failed. ";
    }
    if (!context_restored) {
        error += "CPU context restoration failed. ";
    }
    if (!arena_released) {
        error += "RPC mapping cleanup failed";
        const char *arena_error = xemu_guest_rpc_arena_last_error();
        if (arena_error) {
            error += ": ";
            error += arena_error;
        }
        error += ". ";
    }
    if (!breakpoint_removed || !context_restored || !arena_released) {
        error += "Xbox left paused; reset before another RPC test.";
        return false;
    }
    if (m_was_running) {
        vm_start();
    }
    return true;
}

bool GuestKernelRpcManager::StartIrqlTest()
{
    if (IsBusy()) {
        return false;
    }
    if (m_breakpoint_installed || xemu_guest_rpc_arena_active()) {
        m_state = State::Failed;
        m_status = "A previous Kernel RPC cleanup is still owned; reset the Xbox/Xemu before retrying.";
        return false;
    }

    m_fs_test_mode = FsTestMode::None;
    m_fs_path.clear();
    m_query_path.clear();
    m_path_diagnostics.clear();
    m_path_diagnostic_index = 0;
    m_irql = 0xffffffffu;
    m_export_address = 0;
    m_query_export_address = 0;
    m_stub_address = 0;
    m_completion_address = 0;
    m_breakpoint_installed = false;
    m_arena = {};
    m_was_running = runstate_is_running();

    if (m_was_running && vm_stop(RUN_STATE_PAUSED) != 0) {
        m_state = State::Failed;
        m_status = "Could not pause the Xbox for the Kernel RPC test.";
        return false;
    }
    if (!xemu_cheat_get_x86_registers(&m_saved)) {
        m_state = State::Failed;
        m_status = "Could not capture the Xbox CPU state.";
        if (m_was_running) vm_start();
        return false;
    }

    std::string error;
    if (!ValidateCapturedContext(m_saved, error)) {
        m_state = State::Failed;
        m_status = error;
        if (m_was_running) vm_start();
        return false;
    }
    if (!XemuKernelRpc::ResolveKernelOrdinal(
            ReadGuest, XemuKernelRpc::kKeGetCurrentIrqlOrdinal,
            m_export_address, error)) {
        m_state = State::Failed;
        m_status = error;
        if (m_was_running) vm_start();
        return false;
    }

    if (!xemu_guest_rpc_arena_acquire(&m_arena)) {
        const char *arena_error = xemu_guest_rpc_arena_last_error();
        std::string message = "Could not acquire the private Guest Kernel RPC arena";
        if (arena_error != nullptr) {
            message += ": ";
            message += arena_error;
        }
        AbortBeforeRun(message, false);
        return false;
    }

    m_stub_address = m_arena.virtual_base;
    XemuKernelRpc::IrqlStub stub;
    const uint32_t result_address = m_arena.virtual_base + m_result_offset;
    if (!XemuKernelRpc::BuildIrqlStub(m_stub_address, m_export_address,
                                      result_address, stub, error)) {
        AbortBeforeRun(error, false);
        return false;
    }
    m_completion_address = stub.completion_address;

    uint32_t sentinel = 0xccccccccu;
    if (!xemu_guest_rpc_arena_write(0, stub.bytes.data(), stub.bytes.size()) ||
        !xemu_guest_rpc_arena_write(m_result_offset, &sentinel, sizeof(sentinel))) {
        const char *arena_error = xemu_guest_rpc_arena_last_error();
        std::string message = "Could not write the Kernel RPC test stub";
        if (arena_error != nullptr) {
            message += ": ";
            message += arena_error;
        }
        AbortBeforeRun(message, false);
        return false;
    }

    if (!xemu_cheat_breakpoint_insert(m_completion_address)) {
        AbortBeforeRun("Could not install the private Kernel RPC completion breakpoint.", false);
        return false;
    }
    m_breakpoint_installed = true;

    if (!xemu_cheat_set_x86_register("eip", m_stub_address)) {
        AbortBeforeRun("Could not redirect EIP to the Kernel RPC test stub.", true);
        return false;
    }

    m_state = State::Running;
    m_status = "Running KeGetCurrentIrql through the private Guest Kernel RPC arena...";
    m_started_ms = SDL_GetTicks();
    vm_start();
    return true;
}

bool GuestKernelRpcManager::PrepareReadOnlyTarget(std::string &error)
{
    error.clear();
    m_fs_path.clear();
    m_fs_area.clear();
    m_fs_title_directory.clear();
    m_fs_relative_path.clear();

    const CurrentGameManager::GameInfo &game = current_game_manager.Get();
    if (!game.valid || game.title_id == 0) {
        error = "Start a game before running the read-only filesystem RPC test.";
        return false;
    }

    XemuFatxHdd::Snapshot snapshot;
    std::string snapshot_status;
    if (!hdd_snapshot_service.BuildRawSnapshot(snapshot, snapshot_status)) {
        error = snapshot_status.empty()
            ? "Xbox HDD snapshot is not available for the filesystem comparison."
            : snapshot_status;
        return false;
    }
    const XemuFatxHdd::Partition *partition = XemuFatxHdd::FindPartition(snapshot, 'E');
    if (!partition || !partition->available) {
        error = "Xbox E: FATX partition is not available.";
        return false;
    }

    char title_text[16];
    std::snprintf(title_text, sizeof(title_text), "%08X", game.title_id);
    const char *areas[] = {"UDATA", "TDATA"};
    for (const char *area_name : areas) {
        const XemuFatxHdd::Entry *area = FindChildNoCase(partition->entries, area_name);
        if (!area || !area->directory) {
            continue;
        }
        const XemuFatxHdd::Entry *title = FindChildNoCase(area->children, title_text);
        if (!title || !title->directory) {
            continue;
        }

        const XemuFatxHdd::Entry *target = FindChildNoCase(title->children, "TitleMeta.xbx");
        std::vector<std::string> relative;
        if (!target || target->directory) {
            target = nullptr;
            if (!FindFirstFile(*title, relative, target) || !target) {
                continue;
            }
        } else {
            relative.push_back(target->name);
        }

        m_fs_title_id = game.title_id;
        m_fs_area = area->name;
        m_fs_title_directory = title->name;
        for (size_t i = 0; i < relative.size(); ++i) {
            if (i != 0) {
                m_fs_relative_path += "\\";
            }
            m_fs_relative_path += relative[i];
        }

        m_fs_path = "E:\\";
        m_fs_path += m_fs_area;
        m_fs_path += "\\";
        m_fs_path += m_fs_title_directory;
        if (!m_fs_relative_path.empty()) {
            m_fs_path += "\\";
            m_fs_path += m_fs_relative_path;
        }
        m_expected_file_size = target->file_size;
        m_expected_attributes = target->attributes;
        return true;
    }

    error = "No current-game file was found under E:\\UDATA or E:\\TDATA for a read-only query.";
    return false;
}

bool GuestKernelRpcManager::BuildPathDiagnostics(std::string &error)
{
    error.clear();
    m_path_diagnostics.clear();
    m_path_diagnostic_index = 0;
    if (m_fs_area.empty() || m_fs_title_directory.empty() ||
        m_fs_relative_path.empty()) {
        error = "Current-title FATX target metadata is incomplete for namespace diagnostics.";
        return false;
    }

    const std::string dos_area = "E:\\" + m_fs_area;
    const std::string dos_title = dos_area + "\\" + m_fs_title_directory;
    const std::string dos_file = dos_title + "\\" + m_fs_relative_path;
    const char title_drive_letter = EqualsNoCase(m_fs_area, "UDATA") ? 'U' : 'T';
    const std::string title_drive_root = std::string(1, title_drive_letter) + ":\\";
    const std::string title_drive_file = title_drive_root + m_fs_relative_path;
    const std::string native_root = "\\Device\\Harddisk0\\Partition1";
    const std::string native_area = native_root + "\\" + m_fs_area;
    const std::string native_title = native_area + "\\" + m_fs_title_directory;
    const std::string native_file = native_title + "\\" + m_fs_relative_path;
    const std::string explicit_dos_file = "\\??\\" + dos_file;

    auto add = [&](const char *label, const std::string &path, uint32_t root) {
        PathDiagnostic item;
        item.label = label;
        item.path = path;
        item.root_directory = root;
        m_path_diagnostics.push_back(std::move(item));
    };

    add("DOS E root", "E:\\", XemuKernelRpc::kObDosDevicesDirectory);
    add("DOS E area", dos_area, XemuKernelRpc::kObDosDevicesDirectory);
    add("DOS E title", dos_title, XemuKernelRpc::kObDosDevicesDirectory);
    add("DOS E file", dos_file, XemuKernelRpc::kObDosDevicesDirectory);
    add("Title drive root", title_drive_root, XemuKernelRpc::kObDosDevicesDirectory);
    add("Title drive file", title_drive_file, XemuKernelRpc::kObDosDevicesDirectory);
    add("Native E root", native_root, 0u);
    add("Native E area", native_area, 0u);
    add("Native E title", native_title, 0u);
    add("Native E file", native_file, 0u);
    add("Explicit DOS file", explicit_dos_file, 0u);
    return true;
}

void GuestKernelRpcManager::ActivatePathDiagnostic(size_t index)
{
    if (index >= m_path_diagnostics.size()) {
        return;
    }
    m_path_diagnostic_index = index;
    PathDiagnostic &item = m_path_diagnostics[index];
    m_query_path = item.path;
    m_query_root_directory = item.root_directory;
    m_query_status = 0xccccccccu;
    m_query_ran = 0;
    m_kernel_file_size = 0;
    m_kernel_attributes = 0;
    m_irql = 0xffffffffu;
    m_safe_point_attempts = 0;
    m_last_sample_eip = 0;
    m_safe_point_started_ms = SDL_GetTicks();
    m_next_safe_point_sample_ms = m_safe_point_started_ms;
    m_state = State::WaitingSafePoint;
    m_status = "Namespace diagnostic: waiting for PASSIVE_LEVEL for " + item.label + "...";
}

bool GuestKernelRpcManager::StartPathDiagnostics()
{
    if (IsBusy()) {
        return false;
    }
    if (m_breakpoint_installed || xemu_guest_rpc_arena_active()) {
        m_state = State::Failed;
        m_status = "A previous Kernel RPC cleanup is still owned; reset the Xbox/Xemu before retrying.";
        return false;
    }
    if (!runstate_is_running()) {
        m_state = State::Failed;
        m_status = "Resume the Xbox before starting the namespace diagnostic.";
        return false;
    }

    std::string error;
    if (!PrepareReadOnlyTarget(error) || !BuildPathDiagnostics(error)) {
        m_state = State::Failed;
        m_status = error;
        return false;
    }

    m_fs_test_mode = FsTestMode::NamespaceDiagnostics;
    m_export_address = 0;
    m_query_export_address = 0;
    m_stub_address = 0;
    m_completion_address = 0;
    m_breakpoint_installed = false;
    m_arena = {};
    m_was_running = true;
    ActivatePathDiagnostic(0);
    return true;
}


bool GuestKernelRpcManager::StartReadOnlyFsTest()
{
    if (IsBusy()) {
        return false;
    }
    if (m_breakpoint_installed || xemu_guest_rpc_arena_active()) {
        m_state = State::Failed;
        m_status = "A previous Kernel RPC cleanup is still owned; reset the Xbox/Xemu before retrying.";
        return false;
    }
    if (!runstate_is_running()) {
        m_state = State::Failed;
        m_status = "Resume the Xbox before starting the safe-point filesystem RPC test.";
        return false;
    }

    std::string error;
    if (!PrepareReadOnlyTarget(error)) {
        m_state = State::Failed;
        m_status = error;
        return false;
    }

    m_fs_test_mode = FsTestMode::SingleReadOnlyFile;
    m_query_path = m_fs_path;
    m_query_root_directory = XemuKernelRpc::kObDosDevicesDirectory;
    m_path_diagnostics.clear();
    m_path_diagnostic_index = 0;
    m_irql = 0xffffffffu;
    m_export_address = 0;
    m_query_export_address = 0;
    m_stub_address = 0;
    m_completion_address = 0;
    m_breakpoint_installed = false;
    m_arena = {};
    m_query_status = 0xccccccccu;
    m_query_ran = 0;
    m_kernel_file_size = 0;
    m_kernel_attributes = 0;
    m_safe_point_attempts = 0;
    m_last_sample_eip = 0;
    m_safe_point_started_ms = SDL_GetTicks();
    m_next_safe_point_sample_ms = m_safe_point_started_ms;
    m_was_running = true;
    m_state = State::WaitingSafePoint;
    m_status = "Waiting for current-title execution at PASSIVE_LEVEL before the read-only filesystem query...";
    return true;
}

bool GuestKernelRpcManager::LoadRecursiveDeleteEntry(size_t index,
                                                     std::string &error)
{
    error.clear();
    if (index >= m_recursive_delete_plan.size()) {
        error = "Recursive Kernel Delete plan index is invalid.";
        return false;
    }
    m_recursive_delete_index = index;
    const RecursiveDeleteEntry &item = m_recursive_delete_plan[index];
    m_fs_path = item.fatx_path;
    m_query_path = item.native_path;
    m_query_root_directory = 0u;
    m_expected_file_size = item.file_size;
    m_expected_attributes = item.attributes;
    m_delete_current_is_directory = item.directory;
    m_irql = 0xffffffffu;
    m_delete_open_status = 0xccccccccu;
    m_delete_setinfo_status = 0xccccccccu;
    m_delete_close_status = 0xccccccccu;
    m_delete_operation_ran = 0;
    m_safe_point_attempts = 0;
    m_last_sample_eip = 0;
    m_safe_point_started_ms = SDL_GetTicks();
    m_next_safe_point_sample_ms = m_safe_point_started_ms;
    return true;
}

bool GuestKernelRpcManager::LoadKernelImportOperation(std::string &error)
{
    error.clear();
    if (m_import_plan_index >= m_import_preflight.entries.size()) {
        error = "Kernel Import plan index is invalid.";
        return false;
    }

    const TransferEntry &item = m_import_preflight.entries[m_import_plan_index];
    m_fs_path = item.fatx_path;
    m_query_path = item.native_path;
    m_query_root_directory = 0u;
    m_import_current_is_directory = item.directory;
    m_expected_attributes = 0;

    if (item.source_from_fatx && !item.directory) {
        if (!hdd_snapshot_service.ReadFatxFileChunk(
                item.source_partition, item.source_components,
                item.source_directory_entry_offset, item.source_first_cluster,
                item.source_modified_time, item.source_modified_date,
                item.source_attributes, item.file_size, m_import_file_offset,
                XemuKernelFs::kImportChunkBytes, m_import_chunk, error)) {
            return false;
        }
        m_import_current_chunk_bytes = static_cast<uint32_t>(m_import_chunk.size());
        // v2.27: verification is intentionally progressive for multi-chunk
        // FATX-to-FATX transfers. After each Create/Open -> Write -> Flush ->
        // Close transaction, the destination must equal exactly the bytes
        // committed so far, not the source file's eventual final size.
        m_expected_file_size = XemuKernelFs::ExpectedCommittedFileSize(
            m_import_file_offset, m_import_current_chunk_bytes);
    } else if (!item.directory) {
        if (!XemuKernelFs::ValidateImportHostEntryMetadata(item, error)) {
            return false;
        }
        if (!XemuKernelFs::LoadImportFileChunk(
                m_import_host_stream, item, m_import_file_offset, m_import_chunk,
                m_import_current_chunk_bytes, m_expected_file_size, error)) {
            return false;
        }
        if (m_import_file_offset == 0) {
            m_import_current_source_hash = XemuKernelFs::kContentHashBasis;
        }
        m_import_current_source_hash = XemuKernelFs::UpdateContentHash(
            m_import_current_source_hash, m_import_chunk.data(), m_import_chunk.size());
    } else if (!XemuKernelFs::LoadImportFileChunk(
            item, m_import_file_offset, m_import_chunk,
            m_import_current_chunk_bytes, m_expected_file_size, error)) {
        return false;
    }
    m_import_create_disposition = item.directory || m_import_file_offset == 0
        ? XemuKernelRpc::kFileCreate : XemuKernelRpc::kFileOpen;
    if (item.directory) {
        m_import_file_offset = 0;
        m_expected_file_size = 0;
    }

    m_irql = 0xffffffffu;
    m_import_create_status = 0xccccccccu;
    m_import_write_status = m_import_current_is_directory ||
                            m_import_current_chunk_bytes == 0u
                                ? 0u : 0xccccccccu;
    m_import_flush_status = m_import_current_is_directory ? 0u : 0xccccccccu;
    m_import_close_status = 0xccccccccu;
    m_import_operation_ran = 0;
    m_import_write_information = 0;
    m_safe_point_attempts = 0;
    m_last_sample_eip = 0;
    m_safe_point_started_ms = SDL_GetTicks();
    m_next_safe_point_sample_ms = m_safe_point_started_ms;
    return true;
}

bool GuestKernelRpcManager::ResolveKernelDeleteExports(std::string &error)
{
    if (m_export_address != 0 && m_delete_open_export_address != 0 &&
        m_delete_setinfo_export_address != 0 && m_delete_close_export_address != 0) {
        return true;
    }

    // A high-level delete resets these once. Keep the resolved addresses across
    // PASSIVE_LEVEL retries and every leaf-first entry in that same operation.
    m_export_address = 0;
    m_delete_open_export_address = 0;
    m_delete_setinfo_export_address = 0;
    m_delete_close_export_address = 0;
    return XemuKernelRpc::ResolveKernelOrdinal(
               ReadGuest, XemuKernelRpc::kKeGetCurrentIrqlOrdinal,
               m_export_address, error) &&
           XemuKernelRpc::ResolveKernelOrdinal(
               ReadGuest, XemuKernelRpc::kNtOpenFileOrdinal,
               m_delete_open_export_address, error) &&
           XemuKernelRpc::ResolveKernelOrdinal(
               ReadGuest, XemuKernelRpc::kNtSetInformationFileOrdinal,
               m_delete_setinfo_export_address, error) &&
           XemuKernelRpc::ResolveKernelOrdinal(
               ReadGuest, XemuKernelRpc::kNtCloseOrdinal,
               m_delete_close_export_address, error);
}

bool GuestKernelRpcManager::ResolveKernelImportExports(std::string &error)
{
    if (m_export_address != 0 && m_import_create_export_address != 0 &&
        m_import_write_export_address != 0 && m_import_flush_export_address != 0 &&
        m_import_close_export_address != 0) {
        return true;
    }

    // Resolve once per confirmed import, not once per 0xD000 file chunk.
    m_export_address = 0;
    m_import_create_export_address = 0;
    m_import_write_export_address = 0;
    m_import_flush_export_address = 0;
    m_import_close_export_address = 0;
    return XemuKernelRpc::ResolveKernelOrdinal(
               ReadGuest, XemuKernelRpc::kKeGetCurrentIrqlOrdinal,
               m_export_address, error) &&
           XemuKernelRpc::ResolveKernelOrdinal(
               ReadGuest, XemuKernelRpc::kNtCreateFileOrdinal,
               m_import_create_export_address, error) &&
           XemuKernelRpc::ResolveKernelOrdinal(
               ReadGuest, XemuKernelRpc::kNtWriteFileOrdinal,
               m_import_write_export_address, error) &&
           XemuKernelRpc::ResolveKernelOrdinal(
               ReadGuest, XemuKernelRpc::kNtFlushBuffersFileOrdinal,
               m_import_flush_export_address, error) &&
           XemuKernelRpc::ResolveKernelOrdinal(
               ReadGuest, XemuKernelRpc::kNtCloseOrdinal,
               m_import_close_export_address, error);
}

bool GuestKernelRpcManager::BeginKernelDeleteAttempt()
{
    std::string error;
    if (!ResolveKernelDeleteExports(error)) {
        m_state = State::Failed;
        m_status = error;
        vm_start();
        return false;
    }

    if (!xemu_guest_rpc_arena_acquire(&m_arena)) {
        const char *arena_error = xemu_guest_rpc_arena_last_error();
        std::string message = "Could not acquire the private Guest Kernel RPC arena";
        if (arena_error) {
            message += ": ";
            message += arena_error;
        }
        AbortBeforeRun(message, false);
        return false;
    }

    const uint32_t base = m_arena.virtual_base;
    m_stub_address = base;
    const uint32_t irql_address = base + kDeleteIrqlOffset;
    const uint32_t open_status_address = base + kDeleteOpenStatusOffset;
    const uint32_t set_status_address = base + kDeleteSetInfoStatusOffset;
    const uint32_t close_status_address = base + kDeleteCloseStatusOffset;
    const uint32_t operation_ran_address = base + kDeleteOperationRanOffset;
    const uint32_t file_handle_address = base + kDeleteFileHandleOffset;
    const uint32_t io_status_address = base + kDeleteIoStatusBlockOffset;
    const uint32_t disposition_address = base + kDeleteDispositionOffset;
    const uint32_t ansi_address = base + kDeleteAnsiStringOffset;
    const uint32_t object_attributes_address = base + kDeleteObjectAttributesOffset;
    const uint32_t path_address = base + kDeletePathOffset;

    XemuKernelRpc::KernelDeleteFileStub stub;
    const bool stub_built = m_delete_current_is_directory
        ? XemuKernelRpc::BuildKernelDeleteDirectoryStub(
              base, m_export_address, m_delete_open_export_address,
              m_delete_setinfo_export_address, m_delete_close_export_address,
              irql_address, open_status_address, set_status_address,
              close_status_address, operation_ran_address, file_handle_address,
              io_status_address, object_attributes_address, disposition_address,
              stub, error)
        : XemuKernelRpc::BuildKernelDeleteFileStub(
              base, m_export_address, m_delete_open_export_address,
              m_delete_setinfo_export_address, m_delete_close_export_address,
              irql_address, open_status_address, set_status_address,
              close_status_address, operation_ran_address, file_handle_address,
              io_status_address, object_attributes_address, disposition_address,
              stub, error);
    if (!stub_built) {
        AbortBeforeRun(error, false);
        return false;
    }
    m_completion_address = stub.completion_address;

    std::array<uint8_t, 8> ansi = {};
    std::array<uint8_t, 12> object_attributes = {};
    if (!XemuKernelRpc::BuildFileQueryObjects(
            m_query_path, path_address, ansi_address, ansi, object_attributes,
            error, 0u)) {
        AbortBeforeRun(error, false);
        return false;
    }
    m_query_ansi_length = XemuKernelRpc::Le16(ansi.data());
    m_query_ansi_maximum = XemuKernelRpc::Le16(ansi.data() + 2);
    m_query_ansi_buffer = XemuKernelRpc::Le32(ansi.data() + 4);
    m_query_object_name = XemuKernelRpc::Le32(object_attributes.data() + 4);
    m_query_object_attributes = XemuKernelRpc::Le32(object_attributes.data() + 8);

    const size_t payload_size = kDeletePathOffset + m_query_path.size() + 1u;
    if (payload_size > m_arena.size) {
        AbortBeforeRun("Kernel Delete RPC payload exceeds the private arena.", false);
        return false;
    }
    std::vector<uint8_t> payload(payload_size, 0);
    std::copy(stub.bytes.begin(), stub.bytes.end(), payload.begin());
    XemuKernelRpc::StoreLe32(payload.data() + kDeleteIrqlOffset, 0xffffffffu);
    XemuKernelRpc::StoreLe32(payload.data() + kDeleteOpenStatusOffset, 0xccccccccu);
    XemuKernelRpc::StoreLe32(payload.data() + kDeleteSetInfoStatusOffset, 0xccccccccu);
    XemuKernelRpc::StoreLe32(payload.data() + kDeleteCloseStatusOffset, 0xccccccccu);
    XemuKernelRpc::StoreLe32(payload.data() + kDeleteOperationRanOffset, 0u);
    XemuKernelRpc::StoreLe32(payload.data() + kDeleteFileHandleOffset, 0u);
    payload[kDeleteDispositionOffset] = 1u; // FILE_DISPOSITION_INFORMATION.DeleteFile
    std::copy(ansi.begin(), ansi.end(), payload.begin() + kDeleteAnsiStringOffset);
    std::copy(object_attributes.begin(), object_attributes.end(),
              payload.begin() + kDeleteObjectAttributesOffset);
    std::memcpy(payload.data() + kDeletePathOffset, m_query_path.c_str(),
                m_query_path.size() + 1u);

    if (!xemu_guest_rpc_arena_write(0, payload.data(), payload.size())) {
        const char *arena_error = xemu_guest_rpc_arena_last_error();
        std::string message = "Could not write the Kernel Delete RPC payload";
        if (arena_error) {
            message += ": ";
            message += arena_error;
        }
        AbortBeforeRun(message, false);
        return false;
    }
    if (!xemu_cheat_breakpoint_insert(m_completion_address)) {
        AbortBeforeRun("Could not install the Kernel Delete RPC completion breakpoint.", false);
        return false;
    }
    m_breakpoint_installed = true;
    if (!xemu_cheat_set_x86_register("eip", m_stub_address)) {
        AbortBeforeRun("Could not redirect EIP to the Kernel Delete RPC stub.", true);
        return false;
    }

    m_state = State::RunningFsQuery;
    m_status = std::string("PASSIVE_LEVEL reached; Xbox kernel is deleting the current ") +
               (m_delete_current_is_directory ? "directory" : "file") +
               " via NtOpenFile -> NtSetInformationFile(Delete) -> NtClose...";
    m_started_ms = SDL_GetTicks();
    vm_start();
    return true;
}


bool GuestKernelRpcManager::BeginKernelRelocateAttempt()
{
    std::string error;
    if (!ResolveKernelDeleteExports(error)) {
        m_state = State::Failed; m_status = error; vm_start(); return false;
    }
    if (!xemu_guest_rpc_arena_acquire(&m_arena)) {
        AbortBeforeRun("Could not acquire the private Guest Kernel RPC arena for Rename/Move.", false);
        return false;
    }
    const uint32_t base = m_arena.virtual_base;
    m_stub_address = base;
    const uint32_t irql_address = base + kDeleteIrqlOffset;
    const uint32_t open_status_address = base + kDeleteOpenStatusOffset;
    const uint32_t set_status_address = base + kDeleteSetInfoStatusOffset;
    const uint32_t close_status_address = base + kDeleteCloseStatusOffset;
    const uint32_t operation_ran_address = base + kDeleteOperationRanOffset;
    const uint32_t file_handle_address = base + kDeleteFileHandleOffset;
    const uint32_t io_status_address = base + kDeleteIoStatusBlockOffset;
    const uint32_t source_ansi_address = base + kRenameSourceAnsiOffset;
    const uint32_t source_oa_address = base + kRenameSourceObjectAttributesOffset;
    const uint32_t destination_ansi_address = base + kRenameDestinationAnsiOffset;
    const uint32_t rename_info_address = base + kRenameInformationOffset;
    const uint32_t source_path_address = base + kRenameSourcePathOffset;
    const uint32_t destination_path_address = base + kRenameDestinationPathOffset;

    XemuKernelRpc::KernelRenameStub stub;
    if (!XemuKernelRpc::BuildKernelRenameStub(
            base, m_export_address, m_delete_open_export_address,
            m_delete_setinfo_export_address, m_delete_close_export_address,
            irql_address, open_status_address, set_status_address,
            close_status_address, operation_ran_address, file_handle_address,
            io_status_address, source_oa_address, rename_info_address,
            m_relocate_plan.directory, stub, error)) {
        AbortBeforeRun(error, false); return false;
    }
    m_completion_address = stub.completion_address;

    std::array<uint8_t, 8> source_ansi = {};
    std::array<uint8_t, 12> source_oa = {};
    if (!XemuKernelRpc::BuildFileQueryObjects(
            m_relocate_plan.source_native_path, source_path_address,
            source_ansi_address, source_ansi, source_oa, error, 0u)) {
        AbortBeforeRun(error, false); return false;
    }
    if (m_relocate_plan.destination_native_path.empty() ||
        m_relocate_plan.destination_native_path.size() > 0xfffeu) {
        AbortBeforeRun("Rename/Move destination path is invalid.", false); return false;
    }
    std::array<uint8_t, 8> dest_ansi = {};
    XemuKernelRpc::StoreLe16(dest_ansi.data(), (uint16_t)m_relocate_plan.destination_native_path.size());
    XemuKernelRpc::StoreLe16(dest_ansi.data()+2, (uint16_t)(m_relocate_plan.destination_native_path.size()+1u));
    XemuKernelRpc::StoreLe32(dest_ansi.data()+4, destination_path_address);
    std::array<uint8_t, 16> rename_info = {};
    rename_info[0] = 0u; // ReplaceIfExists = FALSE
    XemuKernelRpc::StoreLe32(rename_info.data()+4, 0u); // RootDirectory
    std::copy(dest_ansi.begin(), dest_ansi.end(), rename_info.begin()+8);

    const size_t payload_size = kRenameDestinationPathOffset +
        m_relocate_plan.destination_native_path.size() + 1u;
    if (payload_size > m_arena.size) {
        AbortBeforeRun("Kernel Rename/Move RPC payload exceeds the private arena.", false); return false;
    }
    std::vector<uint8_t> payload(payload_size, 0);
    std::copy(stub.bytes.begin(), stub.bytes.end(), payload.begin());
    XemuKernelRpc::StoreLe32(payload.data()+kDeleteIrqlOffset, 0xffffffffu);
    XemuKernelRpc::StoreLe32(payload.data()+kDeleteOpenStatusOffset, 0xccccccccu);
    XemuKernelRpc::StoreLe32(payload.data()+kDeleteSetInfoStatusOffset, 0xccccccccu);
    XemuKernelRpc::StoreLe32(payload.data()+kDeleteCloseStatusOffset, 0xccccccccu);
    XemuKernelRpc::StoreLe32(payload.data()+kDeleteOperationRanOffset, 0u);
    XemuKernelRpc::StoreLe32(payload.data()+kDeleteFileHandleOffset, 0u);
    std::copy(rename_info.begin(), rename_info.end(), payload.begin()+kRenameInformationOffset);
    std::copy(source_ansi.begin(), source_ansi.end(), payload.begin()+kRenameSourceAnsiOffset);
    std::copy(source_oa.begin(), source_oa.end(), payload.begin()+kRenameSourceObjectAttributesOffset);
    std::copy(dest_ansi.begin(), dest_ansi.end(), payload.begin()+kRenameDestinationAnsiOffset);
    std::memcpy(payload.data()+kRenameSourcePathOffset,
                m_relocate_plan.source_native_path.c_str(),
                m_relocate_plan.source_native_path.size()+1u);
    std::memcpy(payload.data()+kRenameDestinationPathOffset,
                m_relocate_plan.destination_native_path.c_str(),
                m_relocate_plan.destination_native_path.size()+1u);
    if (!xemu_guest_rpc_arena_write(0, payload.data(), payload.size())) {
        AbortBeforeRun("Could not write the Kernel Rename/Move RPC payload.", false); return false;
    }
    if (!xemu_cheat_breakpoint_insert(m_completion_address)) {
        AbortBeforeRun("Could not install the Kernel Rename/Move completion breakpoint.", false); return false;
    }
    m_breakpoint_installed = true;
    if (!xemu_cheat_set_x86_register("eip", m_stub_address)) {
        AbortBeforeRun("Could not redirect EIP to the Kernel Rename/Move RPC stub.", true); return false;
    }
    m_state = State::RunningFsQuery;
    m_status = "PASSIVE_LEVEL reached; Xbox kernel is renaming/moving via NtOpenFile -> NtSetInformationFile(FileRenameInformation) -> NtClose...";
    m_started_ms = SDL_GetTicks();
    vm_start();
    return true;
}

bool GuestKernelRpcManager::BeginKernelImportAttempt()
{
    std::string error;
    if (!ResolveKernelImportExports(error)) {
        m_state = State::Failed;
        m_status = error;
        vm_start();
        return false;
    }

    if (!xemu_guest_rpc_arena_acquire(&m_arena)) {
        const char *arena_error = xemu_guest_rpc_arena_last_error();
        std::string message = "Could not acquire the private Guest Kernel RPC arena";
        if (arena_error) {
            message += ": ";
            message += arena_error;
        }
        AbortBeforeRun(message, false);
        return false;
    }

    const uint32_t base = m_arena.virtual_base;
    m_stub_address = base;
    const uint32_t irql_address = base + kImportIrqlOffset;
    const uint32_t create_status_address = base + kImportCreateStatusOffset;
    const uint32_t write_status_address = base + kImportWriteStatusOffset;
    const uint32_t flush_status_address = base + kImportFlushStatusOffset;
    const uint32_t close_status_address = base + kImportCloseStatusOffset;
    const uint32_t operation_ran_address = base + kImportOperationRanOffset;
    const uint32_t write_information_address = base + kImportWriteInformationOffset;
    const uint32_t file_handle_address = base + kImportFileHandleOffset;
    const uint32_t io_status_address = base + kImportIoStatusBlockOffset;
    const uint32_t byte_offset_address = base + kImportByteOffsetOffset;
    const uint32_t ansi_address = base + kImportAnsiStringOffset;
    const uint32_t object_attributes_address = base + kImportObjectAttributesOffset;
    const uint32_t path_address = base + kImportPathOffset;
    const uint32_t data_address = base + kImportDataOffset;

    XemuKernelRpc::KernelCreateWriteStub stub;
    if (!XemuKernelRpc::BuildKernelCreateWriteStub(
            base, m_export_address, m_import_create_export_address,
            m_import_write_export_address, m_import_flush_export_address,
            m_import_close_export_address, irql_address, create_status_address,
            write_status_address, flush_status_address, close_status_address,
            operation_ran_address, write_information_address,
            file_handle_address, io_status_address, object_attributes_address,
            byte_offset_address, data_address, m_import_current_chunk_bytes,
            m_import_create_disposition, m_import_current_is_directory,
            stub, error)) {
        AbortBeforeRun(error, false);
        return false;
    }
    m_completion_address = stub.completion_address;

    std::array<uint8_t, 8> ansi = {};
    std::array<uint8_t, 12> object_attributes = {};
    if (!XemuKernelRpc::BuildFileQueryObjects(
            m_query_path, path_address, ansi_address, ansi, object_attributes,
            error, 0u)) {
        AbortBeforeRun(error, false);
        return false;
    }
    m_query_ansi_length = XemuKernelRpc::Le16(ansi.data());
    m_query_ansi_maximum = XemuKernelRpc::Le16(ansi.data() + 2);
    m_query_ansi_buffer = XemuKernelRpc::Le32(ansi.data() + 4);
    m_query_object_name = XemuKernelRpc::Le32(object_attributes.data() + 4);
    m_query_object_attributes = XemuKernelRpc::Le32(object_attributes.data() + 8);

    const size_t path_end = kImportPathOffset + m_query_path.size() + 1u;
    const size_t data_end = kImportDataOffset + m_import_chunk.size();
    const size_t payload_size = std::max(path_end, data_end);
    if (path_end > kImportDataOffset || payload_size > m_arena.size) {
        AbortBeforeRun("Kernel Import RPC payload exceeds the private arena/layout.", false);
        return false;
    }

    std::vector<uint8_t> payload(payload_size, 0);
    std::copy(stub.bytes.begin(), stub.bytes.end(), payload.begin());
    XemuKernelRpc::StoreLe32(payload.data() + kImportIrqlOffset, 0xffffffffu);
    XemuKernelRpc::StoreLe32(payload.data() + kImportCreateStatusOffset, 0xccccccccu);
    XemuKernelRpc::StoreLe32(payload.data() + kImportWriteStatusOffset,
                            m_import_current_is_directory ||
                            m_import_current_chunk_bytes == 0u ? 0u : 0xccccccccu);
    XemuKernelRpc::StoreLe32(payload.data() + kImportFlushStatusOffset,
                            m_import_current_is_directory ? 0u : 0xccccccccu);
    XemuKernelRpc::StoreLe32(payload.data() + kImportCloseStatusOffset, 0xccccccccu);
    XemuKernelRpc::StoreLe32(payload.data() + kImportOperationRanOffset, 0u);
    XemuKernelRpc::StoreLe32(payload.data() + kImportWriteInformationOffset, 0u);
    XemuKernelRpc::StoreLe32(payload.data() + kImportFileHandleOffset, 0u);
    XemuKernelRpc::StoreLe32(payload.data() + kImportByteOffsetOffset,
                            (uint32_t)(m_import_file_offset & 0xffffffffu));
    XemuKernelRpc::StoreLe32(payload.data() + kImportByteOffsetOffset + 4u,
                            (uint32_t)(m_import_file_offset >> 32));
    std::copy(ansi.begin(), ansi.end(), payload.begin() + kImportAnsiStringOffset);
    std::copy(object_attributes.begin(), object_attributes.end(),
              payload.begin() + kImportObjectAttributesOffset);
    std::memcpy(payload.data() + kImportPathOffset, m_query_path.c_str(),
                m_query_path.size() + 1u);
    if (!m_import_chunk.empty()) {
        std::copy(m_import_chunk.begin(), m_import_chunk.end(),
                  payload.begin() + kImportDataOffset);
    }

    if (!xemu_guest_rpc_arena_write(0, payload.data(), payload.size())) {
        const char *arena_error = xemu_guest_rpc_arena_last_error();
        std::string message = "Could not write the Kernel Import RPC payload";
        if (arena_error) {
            message += ": ";
            message += arena_error;
        }
        AbortBeforeRun(message, false);
        return false;
    }
    if (!xemu_cheat_breakpoint_insert(m_completion_address)) {
        AbortBeforeRun("Could not install the Kernel Import RPC completion breakpoint.", false);
        return false;
    }
    m_breakpoint_installed = true;
    if (!xemu_cheat_set_x86_register("eip", m_stub_address)) {
        AbortBeforeRun("Could not redirect EIP to the Kernel Import RPC stub.", true);
        return false;
    }

    m_state = State::RunningFsQuery;
    if (m_import_current_is_directory) {
        m_status = "PASSIVE_LEVEL reached; Xbox kernel is creating the current directory via NtCreateFile(FILE_CREATE) -> NtClose...";
    } else if (m_import_current_chunk_bytes == 0u) {
        m_status = "PASSIVE_LEVEL reached; Xbox kernel is creating the current empty file via NtCreateFile(FILE_CREATE) -> NtFlushBuffersFile -> NtClose...";
    } else {
        m_status = "PASSIVE_LEVEL reached; Xbox kernel is writing the current host-file chunk via NtCreateFile -> NtWriteFile -> NtFlushBuffersFile -> NtClose...";
    }
    m_started_ms = SDL_GetTicks();
    vm_start();
    return true;
}

bool GuestKernelRpcManager::BeginReadOnlyFsAttempt()
{
    std::string error;
    if (!XemuKernelRpc::ResolveKernelOrdinal(
            ReadGuest, XemuKernelRpc::kKeGetCurrentIrqlOrdinal,
            m_export_address, error) ||
        !XemuKernelRpc::ResolveKernelOrdinal(
            ReadGuest, XemuKernelRpc::kNtQueryFullAttributesFileOrdinal,
            m_query_export_address, error)) {
        m_state = State::Failed;
        m_status = error;
        vm_start();
        return false;
    }

    if (!xemu_guest_rpc_arena_acquire(&m_arena)) {
        const char *arena_error = xemu_guest_rpc_arena_last_error();
        std::string message = "Could not acquire the private Guest Kernel RPC arena";
        if (arena_error) {
            message += ": ";
            message += arena_error;
        }
        AbortBeforeRun(message, false);
        return false;
    }

    const uint32_t base = m_arena.virtual_base;
    m_stub_address = base;
    const uint32_t irql_address = base + kFsIrqlOffset;
    const uint32_t status_address = base + kFsStatusOffset;
    const uint32_t query_ran_address = base + kFsQueryRanOffset;
    const uint32_t ansi_address = base + kFsAnsiStringOffset;
    const uint32_t object_attributes_address = base + kFsObjectAttributesOffset;
    const uint32_t file_information_address = base + kFsFileInformationOffset;
    const uint32_t path_address = base + kFsPathOffset;

    XemuKernelRpc::ReadOnlyFileQueryStub stub;
    if (!XemuKernelRpc::BuildReadOnlyFileQueryStub(
            base, m_export_address, m_query_export_address, irql_address,
            status_address, query_ran_address, object_attributes_address,
            file_information_address, stub, error)) {
        AbortBeforeRun(error, false);
        return false;
    }
    m_completion_address = stub.completion_address;

    std::array<uint8_t, 8> ansi = {};
    std::array<uint8_t, 12> object_attributes = {};
    if (!XemuKernelRpc::BuildFileQueryObjects(
            m_query_path, path_address, ansi_address, ansi, object_attributes,
            error, m_query_root_directory)) {
        AbortBeforeRun(error, false);
        return false;
    }

    m_query_ansi_length = XemuKernelRpc::Le16(ansi.data());
    m_query_ansi_maximum = XemuKernelRpc::Le16(ansi.data() + 2);
    m_query_ansi_buffer = XemuKernelRpc::Le32(ansi.data() + 4);
    m_query_object_name = XemuKernelRpc::Le32(object_attributes.data() + 4);
    m_query_object_attributes = XemuKernelRpc::Le32(object_attributes.data() + 8);

    const size_t payload_size = kFsPathOffset + m_query_path.size() + 1u;
    if (payload_size > m_arena.size) {
        AbortBeforeRun("Read-only filesystem RPC payload exceeds the private arena.", false);
        return false;
    }
    std::vector<uint8_t> payload(payload_size, 0);
    std::copy(stub.bytes.begin(), stub.bytes.end(), payload.begin());
    XemuKernelRpc::StoreLe32(payload.data() + kFsIrqlOffset, 0xffffffffu);
    XemuKernelRpc::StoreLe32(payload.data() + kFsStatusOffset, 0xccccccccu);
    XemuKernelRpc::StoreLe32(payload.data() + kFsQueryRanOffset, 0u);
    std::copy(ansi.begin(), ansi.end(), payload.begin() + kFsAnsiStringOffset);
    std::copy(object_attributes.begin(), object_attributes.end(),
              payload.begin() + kFsObjectAttributesOffset);
    std::memcpy(payload.data() + kFsPathOffset, m_query_path.c_str(),
                m_query_path.size() + 1u);

    if (!xemu_guest_rpc_arena_write(0, payload.data(), payload.size())) {
        const char *arena_error = xemu_guest_rpc_arena_last_error();
        std::string message = "Could not write the read-only filesystem RPC payload";
        if (arena_error) {
            message += ": ";
            message += arena_error;
        }
        AbortBeforeRun(message, false);
        return false;
    }
    if (!xemu_cheat_breakpoint_insert(m_completion_address)) {
        AbortBeforeRun("Could not install the read-only RPC completion breakpoint.", false);
        return false;
    }
    m_breakpoint_installed = true;
    if (!xemu_cheat_set_x86_register("eip", m_stub_address)) {
        AbortBeforeRun("Could not redirect EIP to the read-only filesystem RPC stub.", true);
        return false;
    }

    m_state = State::RunningFsQuery;
    m_status = "Checking PASSIVE_LEVEL and, only if safe, querying the file through NtQueryFullAttributesFile...";
    m_started_ms = SDL_GetTicks();
    vm_start();
    return true;
}

void GuestKernelRpcManager::TickWaitingSafePoint()
{
    if (m_state != State::WaitingSafePoint || !runstate_is_running()) {
        return;
    }
    const CurrentGameManager::GameInfo &game = current_game_manager.Get();
    if (!game.valid || game.title_id != m_fs_title_id) {
        m_state = State::Failed;
        m_status = "The running title changed while waiting for a Kernel RPC safe point; no filesystem call was made.";
        return;
    }
    const uint64_t now = SDL_GetTicks();
    if (now - m_safe_point_started_ms > kSafePointTimeoutMs) {
        m_state = State::Failed;
        char text[256];
        std::snprintf(text, sizeof(text),
                      "No PASSIVE_LEVEL current-title execution point was found within 5 seconds (%u sample(s)); no filesystem call was made.",
                      m_safe_point_attempts);
        m_status = text;
        return;
    }
    if (now < m_next_safe_point_sample_ms) {
        return;
    }
    m_next_safe_point_sample_ms = now + kSafePointSampleIntervalMs;

    if (vm_stop(RUN_STATE_PAUSED) != 0) {
        m_state = State::Failed;
        m_status = "Could not pause the Xbox while searching for a Kernel RPC safe point.";
        return;
    }

    XemuCheatX86Registers sample = {};
    if (!xemu_cheat_get_x86_registers(&sample)) {
        m_state = State::Failed;
        m_status = "Could not sample the Xbox CPU while searching for a Kernel RPC safe point.";
        vm_start();
        return;
    }
    ++m_safe_point_attempts;
    if (IsHddFrontendOperation()) {
        ++m_hdd_operation_safe_samples;
    }
    m_last_sample_eip = sample.eip;

    // First bias the RPC toward the running title rather than interrupt/DPC or
    // unrelated kernel code. The stub still checks KeGetCurrentIrql itself and
    // will skip NtQueryFullAttributesFile unless this exact point is IRQL 0.
    if (!IsTitleExecutionPoint(sample.eip) ||
        (sample.eflags & (1u << 9)) == 0) {
        vm_start();
        return;
    }

    std::string preflight_error;
    if (!ValidateCapturedContext(sample, preflight_error)) {
        vm_start();
        return;
    }

    m_saved = sample;
    m_was_running = true;
    if (m_fs_test_mode == FsTestMode::KernelDeleteRecursiveFolder) {
        BeginKernelDeleteAttempt();
    } else if (m_fs_test_mode == FsTestMode::KernelImportFolder) {
        BeginKernelImportAttempt();
    } else if (m_fs_test_mode == FsTestMode::KernelRelocate) {
        BeginKernelRelocateAttempt();
    } else {
        BeginReadOnlyFsAttempt();
    }
}

void GuestKernelRpcManager::HandleIrqlCompletion(
    const XemuCheatX86Registers &after)
{
    uint32_t raw_result = 0xffffffffu;
    if (!xemu_guest_rpc_arena_read(m_result_offset, &raw_result, sizeof(raw_result))) {
        Finish(false, "Kernel RPC reached completion but its result could not be read.", false);
        return;
    }
    m_irql = raw_result & 0xffu;

    std::string difference;
    const bool state_matches = PreservedStateMatches(m_saved, after, difference);
    char text[256];
    if (state_matches) {
        std::snprintf(text, sizeof(text),
                      "PASS: KeGetCurrentIrql returned %u (%s). General/segment/control registers and EFLAGS matched the pre-call snapshot.",
                      m_irql, IrqlName(m_irql));
        Finish(true, text, true);
    } else {
        Finish(false,
               "Kernel RPC reached completion but preserved CPU state did not match: " + difference,
               false);
    }
}

void GuestKernelRpcManager::CompletePathDiagnostic(
    const XemuKernelRpc::FileNetworkOpenResult &parsed)
{
    if (m_path_diagnostic_index >= m_path_diagnostics.size()) {
        m_state = State::Failed;
        m_status = "Namespace diagnostic result index became invalid.";
        return;
    }

    PathDiagnostic &item = m_path_diagnostics[m_path_diagnostic_index];
    item.ntstatus = m_query_status;
    item.file_size = parsed.end_of_file;
    item.attributes = parsed.attributes;
    item.safe_samples = m_safe_point_attempts;
    item.query_ran = m_query_ran != 0;
    item.completed = true;

    std::string cleanup_error;
    if (!CleanupAttemptForRetry(cleanup_error)) {
        m_state = State::Failed;
        m_status = "Namespace diagnostic query completed, but cleanup failed. " + cleanup_error;
        return;
    }

    const size_t next = m_path_diagnostic_index + 1u;
    if (next >= m_path_diagnostics.size()) {
        size_t success_count = 0;
        const PathDiagnostic *first_success = nullptr;
        for (const PathDiagnostic &result : m_path_diagnostics) {
            if (result.completed && (int32_t)result.ntstatus >= 0) {
                ++success_count;
                if (first_success == nullptr) {
                    first_success = &result;
                }
            }
        }
        char text[512];
        if (first_success != nullptr) {
            std::snprintf(text, sizeof(text),
                          "PASS: read-only Xbox namespace diagnostics completed: %zu/%zu path form(s) resolved. First success: %s -> %s. No HDD write was performed.",
                          success_count, m_path_diagnostics.size(),
                          first_success->label.c_str(), first_success->path.c_str());
        } else {
            std::snprintf(text, sizeof(text),
                          "Read-only Xbox namespace diagnostics completed, but none of the %zu path forms resolved successfully. No HDD write was performed.",
                          m_path_diagnostics.size());
        }
        m_state = success_count != 0 ? State::Passed : State::Failed;
        m_status = text;
        return;
    }
    ActivatePathDiagnostic(next);
}


void GuestKernelRpcManager::HandleReadOnlyFsCompletion(
    const XemuCheatX86Registers &after)
{
    uint8_t results[12] = {};
    std::array<uint8_t, kFsFileInformationBytes> file_info = {};
    if (!xemu_guest_rpc_arena_read(kFsIrqlOffset, results, sizeof(results)) ||
        !xemu_guest_rpc_arena_read(kFsFileInformationOffset, file_info.data(),
                                   file_info.size())) {
        Finish(false, "Read-only filesystem RPC reached completion but its result data could not be read.", false);
        return;
    }

    m_irql = XemuKernelRpc::Le32(results + 0) & 0xffu;
    m_query_status = XemuKernelRpc::Le32(results + 4);
    m_query_ran = XemuKernelRpc::Le32(results + 8);

    std::string difference;
    if (!PreservedFilesystemStateMatches(m_saved, after, difference)) {
        Finish(false,
               "Read-only filesystem RPC preserved-state check failed: " + difference,
               false);
        return;
    }

    if (m_irql != 0 || m_query_ran == 0) {
        std::string cleanup_error;
        if (!CleanupAttemptForRetry(cleanup_error)) {
            m_state = State::Failed;
            m_status = "Safe-point probe returned " + std::to_string(m_irql) +
                       " (" + IrqlName(m_irql) + "). " + cleanup_error;
            return;
        }
        m_state = State::WaitingSafePoint;
        m_next_safe_point_sample_ms = SDL_GetTicks() + kSafePointSampleIntervalMs;
        char text[256];
        std::snprintf(text, sizeof(text),
                      "Sample %u reached title code but IRQL was %u (%s); filesystem call was skipped. Waiting for PASSIVE_LEVEL...",
                      m_safe_point_attempts, m_irql, IrqlName(m_irql));
        m_status = text;
        return;
    }

    XemuKernelRpc::FileNetworkOpenResult parsed;
    if (!XemuKernelRpc::ParseFileNetworkOpenInformation(
            file_info.data(), file_info.size(), parsed)) {
        Finish(false, "NtQueryFullAttributesFile returned, but FILE_NETWORK_OPEN_INFORMATION could not be decoded.", true);
        return;
    }
    m_kernel_file_size = parsed.end_of_file;
    m_kernel_attributes = parsed.attributes;

    if (m_fs_test_mode == FsTestMode::NamespaceDiagnostics) {
        CompletePathDiagnostic(parsed);
        return;
    }

    const bool nt_success = (int32_t)m_query_status >= 0;
    const bool size_match = m_kernel_file_size == m_expected_file_size;
    const bool attrs_match = (m_kernel_attributes & 0xffu) == m_expected_attributes;
    char text[512];
    if (nt_success && size_match && attrs_match) {
        std::snprintf(text, sizeof(text),
                      "PASS: PASSIVE_LEVEL safe point found. NtQueryFullAttributesFile returned %08X and matched the direct FATX snapshot: size %llu byte(s), attributes %08X. CPU state matched the pre-call snapshot.",
                      m_query_status, (unsigned long long)m_kernel_file_size,
                      m_kernel_attributes);
        Finish(true, text, true);
    } else {
        std::snprintf(text, sizeof(text),
                      "Read-only filesystem RPC completed at PASSIVE_LEVEL, but comparison failed: NTSTATUS %08X, kernel size %llu vs FATX %llu, kernel attrs %08X vs FATX %02X. No HDD write was performed.",
                      m_query_status, (unsigned long long)m_kernel_file_size,
                      (unsigned long long)m_expected_file_size,
                      m_kernel_attributes, m_expected_attributes);
        Finish(false, text, true);
    }
}

void GuestKernelRpcManager::Tick()
{
    if (m_state == State::WaitingSafePoint) {
        TickWaitingSafePoint();
        return;
    }
    if (m_state != State::Running && m_state != State::RunningFsQuery) {
        return;
    }

    if (runstate_is_running()) {
        if (SDL_GetTicks() - m_started_ms > 3000u) {
            vm_stop(RUN_STATE_PAUSED);
            if (m_state == State::RunningFsQuery) {
                // A filesystem call may legitimately block or be inside FATX
                // when the timeout expires. Never unwind its stack or unmap
                // the RPC return address behind the kernel. Fail closed and
                // require a reset instead of guessing that restoration is safe.
                m_state = State::Failed;
                const bool delete_mode =
                    m_fs_test_mode == FsTestMode::KernelDeleteRecursiveFolder;
                const bool import_mode =
                    m_fs_test_mode == FsTestMode::KernelImportFolder;
                const bool relocate_mode =
                    m_fs_test_mode == FsTestMode::KernelRelocate;
                m_status = delete_mode
                    ? "Kernel Delete RPC exceeded 3 seconds before completion. Xbox left paused with the RPC mapping owned; reset before retrying."
                    : import_mode
                        ? "Kernel Import RPC exceeded 3 seconds before completion. Xbox left paused with the RPC mapping owned; reset before retrying."
                        : relocate_mode
                            ? "Kernel Rename/Move RPC exceeded 3 seconds before completion. Xbox left paused with the RPC mapping owned; reset before retrying."
                            : "Read-only filesystem RPC exceeded 3 seconds before completion. Xbox left paused with the RPC mapping owned; reset before retrying.";
                return;
            }
        } else {
            return;
        }
    }

    XemuCheatX86Registers after = {};
    if (!xemu_cheat_get_x86_registers(&after)) {
        Finish(false, "Kernel RPC stopped, but the CPU state could not be read.", false);
        return;
    }

    if (after.eip != m_completion_address && after.pc != m_completion_address) {
        char text[320];
        if (m_state == State::RunningFsQuery) {
            std::snprintf(text, sizeof(text),
                          "%s stopped unexpectedly at %08X instead of completion %08X. Xbox left paused with the RPC mapping owned; reset before retrying.",
                          m_fs_test_mode == FsTestMode::KernelDeleteRecursiveFolder
                              ? "Kernel Delete RPC"
                              : m_fs_test_mode == FsTestMode::KernelImportFolder
                                  ? "Kernel Import RPC"
                                  : m_fs_test_mode == FsTestMode::KernelRelocate
                                      ? "Kernel Rename/Move RPC" : "Read-only filesystem RPC",
                          after.eip, m_completion_address);
            m_state = State::Failed;
            m_status = text;
            return;
        }
        std::snprintf(text, sizeof(text),
                      "Kernel RPC stopped unexpectedly at %08X instead of completion %08X; no filesystem call was made.",
                      after.eip, m_completion_address);
        Finish(false, text, false);
        return;
    }

    if (m_state == State::RunningFsQuery) {
        if (m_fs_test_mode == FsTestMode::KernelDeleteRecursiveFolder) {
            HandleKernelDeleteCompletion(after);
        } else if (m_fs_test_mode == FsTestMode::KernelImportFolder) {
            HandleKernelImportCompletion(after);
        } else if (m_fs_test_mode == FsTestMode::KernelRelocate) {
            HandleKernelRelocateCompletion(after);
        } else {
            HandleReadOnlyFsCompletion(after);
        }
    } else {
        HandleIrqlCompletion(after);
    }
}

