#pragma once

#include "binary-utils.hh"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <string>
#include <vector>

namespace XemuKernelRpc {

constexpr uint32_t kXboxKernelBase = 0x80010000u;
constexpr uint32_t kKeGetCurrentIrqlOrdinal = 103u;
constexpr uint32_t kNtCloseOrdinal = 187u;
constexpr uint32_t kNtCreateFileOrdinal = 190u;
constexpr uint32_t kNtFlushBuffersFileOrdinal = 198u;
constexpr uint32_t kNtOpenFileOrdinal = 202u;
constexpr uint32_t kNtQueryFullAttributesFileOrdinal = 210u;
constexpr uint32_t kNtSetInformationFileOrdinal = 226u;
constexpr uint32_t kNtWriteFileOrdinal = 236u;
constexpr uint32_t kObDosDevicesDirectory = 0xfffffffdu;
constexpr uint32_t kObjCaseInsensitive = 0x00000040u;

constexpr uint32_t kDeleteAccess = 0x00010000u;
constexpr uint32_t kSynchronizeAccess = 0x00100000u;
constexpr uint32_t kFileListDirectoryAccess = 0x00000001u;
constexpr uint32_t kFileWriteDataAccess = 0x00000002u;
constexpr uint32_t kFileShareReadWriteDelete = 0x00000007u;
constexpr uint32_t kFileDirectoryFile = 0x00000001u;
constexpr uint32_t kFileSynchronousIoNonAlert = 0x00000020u;
constexpr uint32_t kFileNonDirectoryFile = 0x00000040u;
constexpr uint32_t kFileOpenForBackupIntent = 0x00004000u;
constexpr uint32_t kFileAttributeDirectory = 0x00000010u;
constexpr uint32_t kFileAttributeNormal = 0x00000080u;
constexpr uint32_t kFileOpen = 0x00000001u;
constexpr uint32_t kFileCreate = 0x00000002u;
constexpr uint32_t kFileRenameInformation = 10u;
constexpr uint32_t kFileDispositionInformation = 13u;

using MemoryRead = std::function<bool(uint32_t, void *, size_t)>;

inline uint16_t Le16(const uint8_t *p)
{
    return XemuDebugBinaryUtils::read_le16(p);
}

inline uint32_t Le32(const uint8_t *p)
{
    return XemuDebugBinaryUtils::read_le32(p);
}

inline void StoreLe16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value & 0xffu);
    p[1] = (uint8_t)(value >> 8);
}

inline uint64_t Le64(const uint8_t *p)
{
    return (uint64_t)Le32(p) | ((uint64_t)Le32(p + 4) << 32);
}

inline void StoreLe32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value & 0xffu);
    p[1] = (uint8_t)((value >> 8) & 0xffu);
    p[2] = (uint8_t)((value >> 16) & 0xffu);
    p[3] = (uint8_t)((value >> 24) & 0xffu);
}

inline bool AddRva(uint32_t image_base, uint32_t image_size, uint32_t rva,
                   uint32_t bytes, uint32_t &address)
{
    if (rva >= image_size || bytes > image_size - rva) {
        return false;
    }
    const uint64_t full = (uint64_t)image_base + rva;
    if (full > std::numeric_limits<uint32_t>::max()) {
        return false;
    }
    address = (uint32_t)full;
    return true;
}

/* Resolve one live xboxkrnl.exe export by ordinal from its PE export table.
 * The Xbox kernel image base is fixed, but no exported function address is
 * hard-coded. This intentionally rejects PE forwarders and malformed ranges. */
inline bool ResolveKernelOrdinal(const MemoryRead &read, uint32_t ordinal,
                                 uint32_t &address, std::string &error,
                                 uint32_t image_base = kXboxKernelBase)
{
    error.clear();
    address = 0;
    uint8_t dos[0x40] = {};
    if (!read(image_base, dos, sizeof(dos)) || Le16(dos) != 0x5a4du) {
        error = "Xbox kernel DOS/PE header was not readable at 0x80010000.";
        return false;
    }

    const uint32_t nt_rva = Le32(dos + 0x3c);
    if (nt_rva < 0x40u || nt_rva > 0x10000u) {
        error = "Xbox kernel PE header offset is outside the safety limit.";
        return false;
    }
    uint32_t nt_address = 0;
    if ((uint64_t)image_base + nt_rva > UINT32_MAX) {
        error = "Xbox kernel PE header address overflowed.";
        return false;
    }
    nt_address = image_base + nt_rva;

    uint8_t nt_fixed[24] = {};
    if (!read(nt_address, nt_fixed, sizeof(nt_fixed)) ||
        Le32(nt_fixed) != 0x00004550u) {
        error = "Xbox kernel PE signature was not found.";
        return false;
    }
    const uint16_t optional_size = Le16(nt_fixed + 20);
    if (optional_size < 104u || optional_size > 0x400u) {
        error = "Xbox kernel PE optional header size is invalid.";
        return false;
    }
    std::vector<uint8_t> optional(optional_size);
    if (!read(nt_address + 24u, optional.data(), optional.size()) ||
        Le16(optional.data()) != 0x010bu) {
        error = "Xbox kernel is not a readable PE32 image.";
        return false;
    }

    const uint32_t image_size = Le32(optional.data() + 56);
    const uint32_t directory_count = Le32(optional.data() + 92);
    if (image_size < 0x1000u || image_size > 0x04000000u || directory_count < 1u) {
        error = "Xbox kernel PE image/export directory metadata is invalid.";
        return false;
    }
    const uint32_t export_rva = Le32(optional.data() + 96);
    const uint32_t export_size = Le32(optional.data() + 100);
    uint32_t export_address = 0;
    if (export_size < 40u ||
        !AddRva(image_base, image_size, export_rva, 40u, export_address)) {
        error = "Xbox kernel PE export directory is outside the image.";
        return false;
    }

    uint8_t exp[40] = {};
    if (!read(export_address, exp, sizeof(exp))) {
        error = "Xbox kernel PE export directory could not be read.";
        return false;
    }
    const uint32_t ordinal_base = Le32(exp + 16);
    const uint32_t function_count = Le32(exp + 20);
    const uint32_t functions_rva = Le32(exp + 28);
    if (function_count == 0 || function_count > 4096u || ordinal < ordinal_base) {
        error = "Requested Xbox kernel ordinal is not present.";
        return false;
    }
    const uint32_t index = ordinal - ordinal_base;
    if (index >= function_count) {
        error = "Requested Xbox kernel ordinal is outside the export table.";
        return false;
    }

    uint32_t eat_address = 0;
    if (!AddRva(image_base, image_size, functions_rva,
                function_count * 4u, eat_address)) {
        error = "Xbox kernel export address table is outside the image.";
        return false;
    }
    uint8_t entry[4] = {};
    if (!read(eat_address + index * 4u, entry, sizeof(entry))) {
        error = "Xbox kernel export address entry could not be read.";
        return false;
    }
    const uint32_t function_rva = Le32(entry);
    uint32_t function_address = 0;
    if (function_rva == 0 ||
        !AddRva(image_base, image_size, function_rva, 1u, function_address)) {
        error = "Xbox kernel export resolved outside the kernel image.";
        return false;
    }
    if (function_rva >= export_rva &&
        function_rva < export_rva + export_size) {
        error = "Forwarded Xbox kernel exports are not accepted by the RPC test.";
        return false;
    }

    uint8_t first_byte = 0;
    if (!read(function_address, &first_byte, 1)) {
        error = "Resolved Xbox kernel export is not readable executable memory.";
        return false;
    }
    address = function_address;
    return true;
}

struct IrqlStub {
    std::vector<uint8_t> bytes;
    uint32_t completion_address = 0;
};

/* Build a deliberately tiny register-preserving RPC stub:
 *   pushfd / pushad / call export / mov [result],eax / popad / popfd / jmp $
 * The execute breakpoint is installed on the final jmp before it executes. */
inline bool BuildIrqlStub(uint32_t stub_address, uint32_t export_address,
                          uint32_t result_address, IrqlStub &stub,
                          std::string &error)
{
    error.clear();
    stub = {};
    constexpr uint32_t kCallNextOffset = 7u;
    const int64_t displacement = (int64_t)export_address -
                                 ((int64_t)stub_address + kCallNextOffset);
    if (displacement < INT32_MIN || displacement > INT32_MAX) {
        error = "Kernel export is outside x86 rel32 CALL range.";
        return false;
    }

    stub.bytes = {
        0x9c,                         // pushfd
        0x60,                         // pushad
        0xe8, 0, 0, 0, 0,            // call rel32
        0xa3, 0, 0, 0, 0,            // mov [result_address], eax
        0x61,                         // popad
        0x9d,                         // popfd
        0xeb, 0xfe,                   // completion: jmp $
    };
    StoreLe32(stub.bytes.data() + 3, (uint32_t)(int32_t)displacement);
    StoreLe32(stub.bytes.data() + 8, result_address);
    stub.completion_address = stub_address + 14u;
    return true;
}


struct ReadOnlyFileQueryStub {
    std::vector<uint8_t> bytes;
    uint32_t completion_address = 0;
};

/* Xbox kernel read-only filesystem proof. The IRQL check and filesystem call
 * deliberately live in the same stub so there is no scheduling race between
 * proving PASSIVE_LEVEL and invoking NtQueryFullAttributesFile:
 *
 *   pushfd / pushad
 *   call KeGetCurrentIrql
 *   mov [irql],eax
 *   test eax,eax
 *   jnz completion_restore
 *   push file_info
 *   push object_attributes
 *   call NtQueryFullAttributesFile
 *   mov [ntstatus],eax
 *   mov dword ptr [query_ran],1
 * completion_restore:
 *   popad / popfd / jmp $
 */
inline bool BuildReadOnlyFileQueryStub(
    uint32_t stub_address, uint32_t irql_export_address,
    uint32_t query_export_address, uint32_t irql_result_address,
    uint32_t ntstatus_address, uint32_t query_ran_address,
    uint32_t object_attributes_address, uint32_t file_information_address,
    ReadOnlyFileQueryStub &stub, std::string &error)
{
    error.clear();
    stub = {};
    auto emit8 = [&](uint8_t v) { stub.bytes.push_back(v); };
    auto emit32 = [&](uint32_t v) {
        const size_t o = stub.bytes.size();
        stub.bytes.resize(o + 4);
        StoreLe32(stub.bytes.data() + o, v);
    };
    auto emit_rel32_call = [&](uint32_t target) -> bool {
        const uint32_t call_address = stub_address + (uint32_t)stub.bytes.size();
        const int64_t displacement = (int64_t)target - ((int64_t)call_address + 5);
        if (displacement < INT32_MIN || displacement > INT32_MAX) {
            return false;
        }
        emit8(0xe8);
        emit32((uint32_t)(int32_t)displacement);
        return true;
    };

    emit8(0x9c); // pushfd
    emit8(0x60); // pushad
    if (!emit_rel32_call(irql_export_address)) {
        error = "KeGetCurrentIrql is outside x86 rel32 CALL range.";
        return false;
    }
    emit8(0xa3); emit32(irql_result_address); // mov [irql],eax
    emit8(0x85); emit8(0xc0);                 // test eax,eax
    emit8(0x0f); emit8(0x85);                 // jnz rel32
    const size_t skip_disp_offset = stub.bytes.size();
    emit32(0);

    emit8(0x68); emit32(file_information_address);      // push file info
    emit8(0x68); emit32(object_attributes_address);     // push object attrs
    if (!emit_rel32_call(query_export_address)) {
        error = "NtQueryFullAttributesFile is outside x86 rel32 CALL range.";
        return false;
    }
    emit8(0xa3); emit32(ntstatus_address);              // mov [status],eax
    emit8(0xc7); emit8(0x05); emit32(query_ran_address); emit32(1u);

    const uint32_t restore_address = stub_address + (uint32_t)stub.bytes.size();
    const uint32_t branch_next = stub_address + (uint32_t)skip_disp_offset + 4u;
    const int64_t skip_disp = (int64_t)restore_address - (int64_t)branch_next;
    if (skip_disp < INT32_MIN || skip_disp > INT32_MAX) {
        error = "Kernel RPC safe-point branch is outside x86 rel32 range.";
        return false;
    }
    StoreLe32(stub.bytes.data() + skip_disp_offset, (uint32_t)(int32_t)skip_disp);

    emit8(0x61); // popad
    emit8(0x9d); // popfd
    stub.completion_address = stub_address + (uint32_t)stub.bytes.size();
    emit8(0xeb); emit8(0xfe); // completion: jmp $
    return true;
}


struct KernelDeleteFileStub {
    std::vector<uint8_t> bytes;
    uint32_t completion_address = 0;
};

/* Kernel-managed single-file delete matching nxdk's DeleteFileA sequence:
 *
 *   pushfd / pushad
 *   call KeGetCurrentIrql
 *   mov [irql],eax
 *   test eax,eax
 *   jnz completion_restore
 *   mov [operation_ran],1
 *   NtOpenFile(DELETE|SYNCHRONIZE, share R/W/Delete,
 *              FILE_NON_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT|
 *              FILE_OPEN_FOR_BACKUP_INTENT)
 *   if open failed -> completion_restore
 *   NtSetInformationFile(FileDispositionInformation, DeleteFile=TRUE)
 *   NtClose(handle)
 * completion_restore:
 *   popad / popfd / jmp $
 *
 * The Xbox kernel owns all FATX mutation/cache semantics. The stub never
 * falls back to raw FATX writes when a kernel operation fails.
 */
inline bool BuildKernelDeleteEntryStub(
    uint32_t stub_address, uint32_t irql_export_address,
    uint32_t open_export_address, uint32_t setinfo_export_address,
    uint32_t close_export_address, uint32_t irql_result_address,
    uint32_t open_status_address, uint32_t setinfo_status_address,
    uint32_t close_status_address, uint32_t operation_ran_address,
    uint32_t file_handle_address, uint32_t io_status_block_address,
    uint32_t object_attributes_address, uint32_t disposition_address,
    bool directory, KernelDeleteFileStub &stub, std::string &error)
{
    error.clear();
    stub = {};
    auto emit8 = [&](uint8_t v) { stub.bytes.push_back(v); };
    auto emit32 = [&](uint32_t v) {
        const size_t o = stub.bytes.size();
        stub.bytes.resize(o + 4);
        StoreLe32(stub.bytes.data() + o, v);
    };
    auto emit_rel32_call = [&](uint32_t target) -> bool {
        const uint32_t call_address = stub_address + (uint32_t)stub.bytes.size();
        const int64_t displacement = (int64_t)target - ((int64_t)call_address + 5);
        if (displacement < INT32_MIN || displacement > INT32_MAX) {
            return false;
        }
        emit8(0xe8);
        emit32((uint32_t)(int32_t)displacement);
        return true;
    };
    auto emit_rel32_branch_placeholder = [&](uint8_t condition) -> size_t {
        emit8(0x0f);
        emit8(condition);
        const size_t offset = stub.bytes.size();
        emit32(0);
        return offset;
    };
    auto patch_rel32 = [&](size_t displacement_offset,
                           uint32_t target_address) -> bool {
        const uint32_t next = stub_address + (uint32_t)displacement_offset + 4u;
        const int64_t displacement = (int64_t)target_address - (int64_t)next;
        if (displacement < INT32_MIN || displacement > INT32_MAX) {
            return false;
        }
        StoreLe32(stub.bytes.data() + displacement_offset,
                  (uint32_t)(int32_t)displacement);
        return true;
    };

    emit8(0x9c); // pushfd
    emit8(0x60); // pushad
    if (!emit_rel32_call(irql_export_address)) {
        error = "KeGetCurrentIrql is outside x86 rel32 CALL range.";
        return false;
    }
    emit8(0xa3); emit32(irql_result_address); // mov [irql],eax
    emit8(0x85); emit8(0xc0);                 // test eax,eax
    const size_t irql_skip = emit_rel32_branch_placeholder(0x85); // jnz restore

    emit8(0xc7); emit8(0x05); emit32(operation_ran_address); emit32(1u);

    const uint32_t desired_access = kDeleteAccess | kSynchronizeAccess;
    const uint32_t open_options = (directory ? kFileDirectoryFile :
                                              kFileNonDirectoryFile) |
                                  kFileSynchronousIoNonAlert |
                                  kFileOpenForBackupIntent;
    emit8(0x68); emit32(open_options);                    // OpenOptions
    emit8(0x68); emit32(kFileShareReadWriteDelete);       // ShareAccess
    emit8(0x68); emit32(io_status_block_address);         // IoStatusBlock
    emit8(0x68); emit32(object_attributes_address);       // ObjectAttributes
    emit8(0x68); emit32(desired_access);                  // DesiredAccess
    emit8(0x68); emit32(file_handle_address);             // FileHandle*
    if (!emit_rel32_call(open_export_address)) {
        error = "NtOpenFile is outside x86 rel32 CALL range.";
        return false;
    }
    emit8(0xa3); emit32(open_status_address);             // mov [open_status],eax
    emit8(0x85); emit8(0xc0);                             // test eax,eax
    const size_t open_failed = emit_rel32_branch_placeholder(0x88); // js restore

    emit8(0x68); emit32(kFileDispositionInformation);     // information class
    emit8(0x68); emit32(1u);                              // sizeof disposition
    emit8(0x68); emit32(disposition_address);             // disposition
    emit8(0x68); emit32(io_status_block_address);         // IoStatusBlock
    emit8(0xff); emit8(0x35); emit32(file_handle_address);// push [handle]
    if (!emit_rel32_call(setinfo_export_address)) {
        error = "NtSetInformationFile is outside x86 rel32 CALL range.";
        return false;
    }
    emit8(0xa3); emit32(setinfo_status_address);          // mov [set_status],eax

    emit8(0xff); emit8(0x35); emit32(file_handle_address);// push [handle]
    if (!emit_rel32_call(close_export_address)) {
        error = "NtClose is outside x86 rel32 CALL range.";
        return false;
    }
    emit8(0xa3); emit32(close_status_address);            // mov [close_status],eax

    const uint32_t restore_address = stub_address + (uint32_t)stub.bytes.size();
    if (!patch_rel32(irql_skip, restore_address) ||
        !patch_rel32(open_failed, restore_address)) {
        error = "Kernel delete RPC branch is outside x86 rel32 range.";
        return false;
    }
    emit8(0x61); // popad
    emit8(0x9d); // popfd
    stub.completion_address = stub_address + (uint32_t)stub.bytes.size();
    emit8(0xeb); emit8(0xfe); // completion: jmp $
    return true;
}

inline bool BuildKernelDeleteFileStub(
    uint32_t stub_address, uint32_t irql_export_address,
    uint32_t open_export_address, uint32_t setinfo_export_address,
    uint32_t close_export_address, uint32_t irql_result_address,
    uint32_t open_status_address, uint32_t setinfo_status_address,
    uint32_t close_status_address, uint32_t operation_ran_address,
    uint32_t file_handle_address, uint32_t io_status_block_address,
    uint32_t object_attributes_address, uint32_t disposition_address,
    KernelDeleteFileStub &stub, std::string &error)
{
    return BuildKernelDeleteEntryStub(
        stub_address, irql_export_address, open_export_address,
        setinfo_export_address, close_export_address, irql_result_address,
        open_status_address, setinfo_status_address, close_status_address,
        operation_ran_address, file_handle_address, io_status_block_address,
        object_attributes_address, disposition_address, false, stub, error);
}

inline bool BuildKernelDeleteDirectoryStub(
    uint32_t stub_address, uint32_t irql_export_address,
    uint32_t open_export_address, uint32_t setinfo_export_address,
    uint32_t close_export_address, uint32_t irql_result_address,
    uint32_t open_status_address, uint32_t setinfo_status_address,
    uint32_t close_status_address, uint32_t operation_ran_address,
    uint32_t file_handle_address, uint32_t io_status_block_address,
    uint32_t object_attributes_address, uint32_t disposition_address,
    KernelDeleteFileStub &stub, std::string &error)
{
    return BuildKernelDeleteEntryStub(
        stub_address, irql_export_address, open_export_address,
        setinfo_export_address, close_export_address, irql_result_address,
        open_status_address, setinfo_status_address, close_status_address,
        operation_ran_address, file_handle_address, io_status_block_address,
        object_attributes_address, disposition_address, true, stub, error);
}


struct KernelRenameStub {
    std::vector<uint8_t> bytes;
    uint32_t completion_address = 0;
};

/* Kernel-managed FATX rename/move primitive. FILE_RENAME_INFORMATION on Xbox
 * is a 16-byte structure: BOOLEAN ReplaceIfExists, HANDLE RootDirectory,
 * OBJECT_STRING FileName. Destination replacement is always disabled.
 */
inline bool BuildKernelRenameStub(
    uint32_t stub_address, uint32_t irql_export_address,
    uint32_t open_export_address, uint32_t setinfo_export_address,
    uint32_t close_export_address, uint32_t irql_result_address,
    uint32_t open_status_address, uint32_t setinfo_status_address,
    uint32_t close_status_address, uint32_t operation_ran_address,
    uint32_t file_handle_address, uint32_t io_status_block_address,
    uint32_t source_object_attributes_address,
    uint32_t rename_information_address, bool directory,
    KernelRenameStub &stub, std::string &error)
{
    error.clear();
    stub = {};
    auto emit8 = [&](uint8_t v) { stub.bytes.push_back(v); };
    auto emit32 = [&](uint32_t v) {
        const size_t o = stub.bytes.size();
        stub.bytes.resize(o + 4);
        StoreLe32(stub.bytes.data() + o, v);
    };
    auto emit_rel32_call = [&](uint32_t target) -> bool {
        const uint32_t call_address = stub_address + (uint32_t)stub.bytes.size();
        const int64_t displacement = (int64_t)target - ((int64_t)call_address + 5);
        if (displacement < INT32_MIN || displacement > INT32_MAX) return false;
        emit8(0xe8); emit32((uint32_t)(int32_t)displacement); return true;
    };
    auto branch = [&](uint8_t condition) -> size_t {
        emit8(0x0f); emit8(condition); const size_t o=stub.bytes.size(); emit32(0); return o;
    };
    auto patch = [&](size_t o, uint32_t target) -> bool {
        const uint32_t next=stub_address+(uint32_t)o+4u;
        const int64_t d=(int64_t)target-(int64_t)next;
        if (d<INT32_MIN || d>INT32_MAX) return false;
        StoreLe32(stub.bytes.data()+o,(uint32_t)(int32_t)d); return true;
    };

    emit8(0x9c); emit8(0x60);
    if (!emit_rel32_call(irql_export_address)) { error="KeGetCurrentIrql is outside x86 rel32 CALL range."; return false; }
    emit8(0xa3); emit32(irql_result_address);
    emit8(0x85); emit8(0xc0);
    const size_t irql_skip=branch(0x85);
    emit8(0xc7); emit8(0x05); emit32(operation_ran_address); emit32(1u);

    const uint32_t desired_access=kDeleteAccess|kSynchronizeAccess;
    const uint32_t open_options=(directory?kFileDirectoryFile:kFileNonDirectoryFile)|
        kFileSynchronousIoNonAlert|kFileOpenForBackupIntent;
    emit8(0x68); emit32(open_options);
    emit8(0x68); emit32(kFileShareReadWriteDelete);
    emit8(0x68); emit32(io_status_block_address);
    emit8(0x68); emit32(source_object_attributes_address);
    emit8(0x68); emit32(desired_access);
    emit8(0x68); emit32(file_handle_address);
    if (!emit_rel32_call(open_export_address)) { error="NtOpenFile is outside x86 rel32 CALL range."; return false; }
    emit8(0xa3); emit32(open_status_address);
    emit8(0x85); emit8(0xc0);
    const size_t open_failed=branch(0x88);

    emit8(0x68); emit32(kFileRenameInformation);
    emit8(0x68); emit32(16u);
    emit8(0x68); emit32(rename_information_address);
    emit8(0x68); emit32(io_status_block_address);
    emit8(0xff); emit8(0x35); emit32(file_handle_address);
    if (!emit_rel32_call(setinfo_export_address)) { error="NtSetInformationFile is outside x86 rel32 CALL range."; return false; }
    emit8(0xa3); emit32(setinfo_status_address);

    emit8(0xff); emit8(0x35); emit32(file_handle_address);
    if (!emit_rel32_call(close_export_address)) { error="NtClose is outside x86 rel32 CALL range."; return false; }
    emit8(0xa3); emit32(close_status_address);

    const uint32_t restore=stub_address+(uint32_t)stub.bytes.size();
    if (!patch(irql_skip,restore)||!patch(open_failed,restore)) { error="Kernel rename RPC branch is outside x86 rel32 range."; return false; }
    emit8(0x61); emit8(0x9d);
    stub.completion_address=stub_address+(uint32_t)stub.bytes.size();
    emit8(0xeb); emit8(0xfe);
    return true;
}

struct KernelCreateWriteStub {
    std::vector<uint8_t> bytes;
    uint32_t completion_address = 0;
};

/* Kernel-managed create/write primitive used by the v2.12 folder-import test.
 * One RPC owns one directory create or one file chunk. Every attempt is gated
 * by KeGetCurrentIrql == PASSIVE_LEVEL and closes the file handle before the
 * borrowed title thread is restored. Existing destinations are never opened by
 * the first operation: FILE_CREATE is used for a new directory/file and later
 * chunks reopen that already-created file with FILE_OPEN.
 */
inline bool BuildKernelCreateWriteStub(
    uint32_t stub_address, uint32_t irql_export_address,
    uint32_t create_export_address, uint32_t write_export_address,
    uint32_t flush_export_address, uint32_t close_export_address,
    uint32_t irql_result_address, uint32_t create_status_address,
    uint32_t write_status_address, uint32_t flush_status_address,
    uint32_t close_status_address, uint32_t operation_ran_address,
    uint32_t write_information_address, uint32_t file_handle_address,
    uint32_t io_status_block_address, uint32_t object_attributes_address,
    uint32_t byte_offset_address, uint32_t data_address,
    uint32_t write_length, uint32_t create_disposition, bool directory,
    KernelCreateWriteStub &stub, std::string &error)
{
    error.clear();
    stub = {};
    if (create_disposition != kFileCreate && create_disposition != kFileOpen) {
        error = "Kernel import create disposition is not FILE_CREATE/FILE_OPEN.";
        return false;
    }
    if (directory && write_length != 0u) {
        error = "Kernel import directory operation cannot carry file data.";
        return false;
    }

    auto emit8 = [&](uint8_t v) { stub.bytes.push_back(v); };
    auto emit32 = [&](uint32_t v) {
        const size_t o = stub.bytes.size();
        stub.bytes.resize(o + 4);
        StoreLe32(stub.bytes.data() + o, v);
    };
    auto emit_rel32_call = [&](uint32_t target) -> bool {
        const uint32_t call_address = stub_address + (uint32_t)stub.bytes.size();
        const int64_t displacement = (int64_t)target - ((int64_t)call_address + 5);
        if (displacement < INT32_MIN || displacement > INT32_MAX) {
            return false;
        }
        emit8(0xe8);
        emit32((uint32_t)(int32_t)displacement);
        return true;
    };
    auto emit_rel32_branch_placeholder = [&](uint8_t condition) -> size_t {
        emit8(0x0f);
        emit8(condition);
        const size_t offset = stub.bytes.size();
        emit32(0);
        return offset;
    };
    auto patch_rel32 = [&](size_t displacement_offset,
                           uint32_t target_address) -> bool {
        const uint32_t next = stub_address + (uint32_t)displacement_offset + 4u;
        const int64_t displacement = (int64_t)target_address - (int64_t)next;
        if (displacement < INT32_MIN || displacement > INT32_MAX) {
            return false;
        }
        StoreLe32(stub.bytes.data() + displacement_offset,
                  (uint32_t)(int32_t)displacement);
        return true;
    };

    emit8(0x9c); // pushfd
    emit8(0x60); // pushad
    if (!emit_rel32_call(irql_export_address)) {
        error = "KeGetCurrentIrql is outside x86 rel32 CALL range.";
        return false;
    }
    emit8(0xa3); emit32(irql_result_address); // mov [irql],eax
    emit8(0x85); emit8(0xc0);                 // test eax,eax
    const size_t irql_skip = emit_rel32_branch_placeholder(0x85); // jnz restore

    emit8(0xc7); emit8(0x05); emit32(operation_ran_address); emit32(1u);

    const uint32_t desired_access = kSynchronizeAccess |
        (directory ? kFileListDirectoryAccess : kFileWriteDataAccess);
    const uint32_t file_attributes = directory ? kFileAttributeDirectory :
                                                 kFileAttributeNormal;
    const uint32_t create_options = (directory ? kFileDirectoryFile :
                                                 kFileNonDirectoryFile) |
                                    kFileSynchronousIoNonAlert |
                                    kFileOpenForBackupIntent;

    emit8(0x68); emit32(create_options);                 // CreateOptions
    emit8(0x68); emit32(create_disposition);             // CreateDisposition
    emit8(0x68); emit32(kFileShareReadWriteDelete);      // ShareAccess
    emit8(0x68); emit32(file_attributes);                // FileAttributes
    emit8(0x6a); emit8(0x00);                            // AllocationSize = NULL
    emit8(0x68); emit32(io_status_block_address);        // IoStatusBlock
    emit8(0x68); emit32(object_attributes_address);      // ObjectAttributes
    emit8(0x68); emit32(desired_access);                 // DesiredAccess
    emit8(0x68); emit32(file_handle_address);            // FileHandle*
    if (!emit_rel32_call(create_export_address)) {
        error = "NtCreateFile is outside x86 rel32 CALL range.";
        return false;
    }
    emit8(0xa3); emit32(create_status_address);          // mov [create_status],eax
    emit8(0x85); emit8(0xc0);                            // test eax,eax
    const size_t create_failed = emit_rel32_branch_placeholder(0x88); // js restore

    size_t write_failed = SIZE_MAX;
    if (!directory && write_length != 0u) {
        emit8(0x68); emit32(byte_offset_address);         // ByteOffset
        emit8(0x68); emit32(write_length);                // Length
        emit8(0x68); emit32(data_address);                // Buffer
        emit8(0x68); emit32(io_status_block_address);     // IoStatusBlock
        emit8(0x6a); emit8(0x00);                         // ApcContext
        emit8(0x6a); emit8(0x00);                         // ApcRoutine
        emit8(0x6a); emit8(0x00);                         // Event
        emit8(0xff); emit8(0x35); emit32(file_handle_address); // FileHandle
        if (!emit_rel32_call(write_export_address)) {
            error = "NtWriteFile is outside x86 rel32 CALL range.";
            return false;
        }
        emit8(0xa3); emit32(write_status_address);        // mov [write_status],eax
        emit8(0x85); emit8(0xc0);                         // test eax,eax
        write_failed = emit_rel32_branch_placeholder(0x88); // js close
        emit8(0xa1); emit32(io_status_block_address + 4u);// mov eax,[iosb.Information]
        emit8(0xa3); emit32(write_information_address);   // mov [write_info],eax
    }

    if (!directory) {
        emit8(0x68); emit32(io_status_block_address);     // IoStatusBlock
        emit8(0xff); emit8(0x35); emit32(file_handle_address); // FileHandle
        if (!emit_rel32_call(flush_export_address)) {
            error = "NtFlushBuffersFile is outside x86 rel32 CALL range.";
            return false;
        }
        emit8(0xa3); emit32(flush_status_address);        // mov [flush_status],eax
    }

    const uint32_t close_address = stub_address + (uint32_t)stub.bytes.size();
    emit8(0xff); emit8(0x35); emit32(file_handle_address);// FileHandle
    if (!emit_rel32_call(close_export_address)) {
        error = "NtClose is outside x86 rel32 CALL range.";
        return false;
    }
    emit8(0xa3); emit32(close_status_address);            // mov [close_status],eax

    const uint32_t restore_address = stub_address + (uint32_t)stub.bytes.size();
    if (!patch_rel32(irql_skip, restore_address) ||
        !patch_rel32(create_failed, restore_address) ||
        (write_failed != SIZE_MAX && !patch_rel32(write_failed, close_address))) {
        error = "Kernel import RPC branch is outside x86 rel32 range.";
        return false;
    }
    emit8(0x61); // popad
    emit8(0x9d); // popfd
    stub.completion_address = stub_address + (uint32_t)stub.bytes.size();
    emit8(0xeb); emit8(0xfe); // completion: jmp $
    return true;
}

/* Build the Xbox 32-bit ANSI_STRING + OBJECT_ATTRIBUTES data used by
 * NtQueryFullAttributesFile. nxdk's GetFileAttributesA uses the same
 * OBJ_CASE_INSENSITIVE + ObDosDevicesDirectory() (-3) arrangement. */
inline bool BuildFileQueryObjects(const std::string &path,
                                  uint32_t path_address,
                                  uint32_t ansi_string_address,
                                  std::array<uint8_t, 8> &ansi_string,
                                  std::array<uint8_t, 12> &object_attributes,
                                  std::string &error,
                                  uint32_t root_directory = kObDosDevicesDirectory)
{
    error.clear();
    ansi_string.fill(0);
    object_attributes.fill(0);
    if (path.empty() || path.size() > 0xfffeu) {
        error = "Xbox file-query path length is invalid.";
        return false;
    }
    StoreLe16(ansi_string.data() + 0, (uint16_t)path.size());
    StoreLe16(ansi_string.data() + 2, (uint16_t)(path.size() + 1u));
    StoreLe32(ansi_string.data() + 4, path_address);

    StoreLe32(object_attributes.data() + 0, root_directory);
    StoreLe32(object_attributes.data() + 4, ansi_string_address);
    StoreLe32(object_attributes.data() + 8, kObjCaseInsensitive);
    return true;
}

struct FileNetworkOpenResult {
    uint64_t end_of_file = 0;
    uint32_t attributes = 0;
};

inline bool ParseFileNetworkOpenInformation(const uint8_t *data, size_t size,
                                            FileNetworkOpenResult &result)
{
    if (data == nullptr || size < 52u) {
        return false;
    }
    result.end_of_file = Le64(data + 40u);
    result.attributes = Le32(data + 48u);
    return true;
}

} // namespace XemuKernelRpc
