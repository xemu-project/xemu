// v2.87 current regression ownership.
#include "kernel-rpc-utils.hh"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

using namespace XemuKernelRpc;

void fail(const std::string &message)
{
    std::cerr << "FAIL: " << message << "\n";
    std::exit(1);
}

void put16(std::vector<uint8_t> &image, size_t offset, uint16_t value)
{
    if (offset + 2 > image.size()) fail("put16 outside image");
    image[offset + 0] = static_cast<uint8_t>(value);
    image[offset + 1] = static_cast<uint8_t>(value >> 8);
}

void put32(std::vector<uint8_t> &image, size_t offset, uint32_t value)
{
    if (offset + 4 > image.size()) fail("put32 outside image");
    StoreLe32(image.data() + offset, value);
}

std::vector<uint8_t> make_kernel_image(uint32_t function_rva = 0x2000u)
{
    std::vector<uint8_t> image(0x5000, 0);
    put16(image, 0x00, 0x5a4d);       // MZ
    put32(image, 0x3c, 0x80);         // e_lfanew
    put32(image, 0x80, 0x00004550);   // PE\0\0
    put16(image, 0x80 + 20, 0x00e0);  // SizeOfOptionalHeader

    const size_t opt = 0x80 + 24;
    put16(image, opt + 0, 0x010b);     // PE32
    put32(image, opt + 56, static_cast<uint32_t>(image.size()));
    put32(image, opt + 92, 16);        // NumberOfRvaAndSizes
    put32(image, opt + 96, 0x0400);    // Export directory RVA
    put32(image, opt + 100, 0x0100);   // Export directory size

    const size_t exp = 0x0400;
    put32(image, exp + 16, 1);         // ordinal base
    put32(image, exp + 20, 256);       // NumberOfFunctions
    put32(image, exp + 28, 0x0600);    // AddressOfFunctions
    put32(image, 0x0600 + (kKeGetCurrentIrqlOrdinal - 1u) * 4u,
          function_rva);
    put32(image, 0x0600 + (kNtCloseOrdinal - 1u) * 4u, 0x2200u);
    put32(image, 0x0600 + (kNtCreateFileOrdinal - 1u) * 4u, 0x2500u);
    put32(image, 0x0600 + (kNtFlushBuffersFileOrdinal - 1u) * 4u, 0x2600u);
    put32(image, 0x0600 + (kNtOpenFileOrdinal - 1u) * 4u, 0x2300u);
    put32(image, 0x0600 + (kNtQueryFullAttributesFileOrdinal - 1u) * 4u,
          0x2100u);
    put32(image, 0x0600 + (kNtSetInformationFileOrdinal - 1u) * 4u, 0x2400u);
    put32(image, 0x0600 + (kNtWriteFileOrdinal - 1u) * 4u, 0x2700u);
    if (function_rva < image.size()) {
        image[function_rva] = 0x90;    // readable function byte
    }
    image[0x2100u] = 0x90;             // NtQueryFullAttributesFile
    image[0x2200u] = 0x90;             // NtClose
    image[0x2300u] = 0x90;             // NtOpenFile
    image[0x2400u] = 0x90;             // NtSetInformationFile
    image[0x2500u] = 0x90;             // NtCreateFile
    image[0x2600u] = 0x90;             // NtFlushBuffersFile
    image[0x2700u] = 0x90;             // NtWriteFile
    return image;
}

MemoryRead reader_for(const std::vector<uint8_t> &image)
{
    return [&image](uint32_t address, void *buffer, size_t size) {
        if (address < kXboxKernelBase) return false;
        const uint64_t offset = static_cast<uint64_t>(address) - kXboxKernelBase;
        if (offset > image.size() || size > image.size() - static_cast<size_t>(offset)) {
            return false;
        }
        std::memcpy(buffer, image.data() + static_cast<size_t>(offset), size);
        return true;
    };
}

void test_resolver()
{
    std::string error;
    uint32_t address = 0;
    auto image = make_kernel_image();
    if (!ResolveKernelOrdinal(reader_for(image), kKeGetCurrentIrqlOrdinal,
                              address, error)) {
        fail("valid kernel ordinal did not resolve: " + error);
    }
    if (address != kXboxKernelBase + 0x2000u) {
        fail("kernel ordinal resolved to wrong address");
    }
    if (!ResolveKernelOrdinal(reader_for(image), kNtQueryFullAttributesFileOrdinal,
                              address, error) ||
        address != kXboxKernelBase + 0x2100u) {
        fail("NtQueryFullAttributesFile ordinal did not resolve correctly");
    }
    if (!ResolveKernelOrdinal(reader_for(image), kNtOpenFileOrdinal,
                              address, error) ||
        address != kXboxKernelBase + 0x2300u) {
        fail("NtOpenFile ordinal did not resolve correctly");
    }
    if (!ResolveKernelOrdinal(reader_for(image), kNtSetInformationFileOrdinal,
                              address, error) ||
        address != kXboxKernelBase + 0x2400u) {
        fail("NtSetInformationFile ordinal did not resolve correctly");
    }
    if (!ResolveKernelOrdinal(reader_for(image), kNtCloseOrdinal,
                              address, error) ||
        address != kXboxKernelBase + 0x2200u) {
        fail("NtClose ordinal did not resolve correctly");
    }
    if (!ResolveKernelOrdinal(reader_for(image), kNtCreateFileOrdinal,
                              address, error) ||
        address != kXboxKernelBase + 0x2500u) {
        fail("NtCreateFile ordinal did not resolve correctly");
    }
    if (!ResolveKernelOrdinal(reader_for(image), kNtFlushBuffersFileOrdinal,
                              address, error) ||
        address != kXboxKernelBase + 0x2600u) {
        fail("NtFlushBuffersFile ordinal did not resolve correctly");
    }
    if (!ResolveKernelOrdinal(reader_for(image), kNtWriteFileOrdinal,
                              address, error) ||
        address != kXboxKernelBase + 0x2700u) {
        fail("NtWriteFile ordinal did not resolve correctly");
    }

    auto bad_mz = image;
    bad_mz[0] = 0;
    if (ResolveKernelOrdinal(reader_for(bad_mz), kKeGetCurrentIrqlOrdinal,
                             address, error)) {
        fail("resolver accepted invalid DOS header");
    }

    if (ResolveKernelOrdinal(reader_for(image), 5000u, address, error)) {
        fail("resolver accepted ordinal outside export table");
    }

    auto forwarder = make_kernel_image(0x0450u);
    if (ResolveKernelOrdinal(reader_for(forwarder), kKeGetCurrentIrqlOrdinal,
                             address, error)) {
        fail("resolver accepted forwarded export");
    }
}

void test_irql_stub()
{
    constexpr uint32_t stub_address = 0x68400000u;
    constexpr uint32_t export_address = 0x80012000u;
    constexpr uint32_t result_address = 0x68401000u;
    IrqlStub stub;
    std::string error;
    if (!BuildIrqlStub(stub_address, export_address, result_address, stub, error)) {
        fail("IRQL stub build failed: " + error);
    }
    if (stub.bytes.size() != 16u || stub.completion_address != stub_address + 14u) {
        fail("IRQL stub size/completion address mismatch");
    }
    const uint8_t fixed[] = {
        0x9c, 0x60, 0xe8,
        0, 0, 0, 0,
        0xa3,
        0, 0, 0, 0,
        0x61, 0x9d, 0xeb, 0xfe,
    };
    for (size_t i = 0; i < sizeof(fixed); ++i) {
        if ((i >= 3 && i <= 6) || (i >= 8 && i <= 11)) continue;
        if (stub.bytes[i] != fixed[i]) fail("IRQL stub opcode mismatch");
    }
    const int32_t rel = static_cast<int32_t>(Le32(stub.bytes.data() + 3));
    const uint32_t call_target = stub_address + 7u + static_cast<uint32_t>(rel);
    if (call_target != export_address) fail("IRQL stub CALL target mismatch");
    if (Le32(stub.bytes.data() + 8) != result_address) {
        fail("IRQL stub result address mismatch");
    }
}


void test_read_only_file_query_stub()
{
    constexpr uint32_t stub_address = 0x68400000u;
    constexpr uint32_t irql_export = 0x800145a4u;
    constexpr uint32_t query_export = 0x80022000u;
    constexpr uint32_t irql_result = 0x68401000u;
    constexpr uint32_t ntstatus = 0x68401004u;
    constexpr uint32_t query_ran = 0x68401008u;
    constexpr uint32_t object_attributes = 0x68401120u;
    constexpr uint32_t file_information = 0x68401140u;

    ReadOnlyFileQueryStub stub;
    std::string error;
    if (!BuildReadOnlyFileQueryStub(stub_address, irql_export, query_export,
                                    irql_result, ntstatus, query_ran,
                                    object_attributes, file_information,
                                    stub, error)) {
        fail("read-only file-query stub build failed: " + error);
    }
    if (stub.bytes.size() < 40u || stub.completion_address + 2u !=
                                     stub_address + stub.bytes.size()) {
        fail("read-only file-query stub completion mismatch");
    }
    if (stub.bytes[0] != 0x9c || stub.bytes[1] != 0x60 ||
        stub.bytes[stub.bytes.size() - 4] != 0x61 ||
        stub.bytes[stub.bytes.size() - 3] != 0x9d ||
        stub.bytes[stub.bytes.size() - 2] != 0xeb ||
        stub.bytes[stub.bytes.size() - 1] != 0xfe) {
        fail("read-only file-query register-preservation opcodes mismatch");
    }
    if (Le32(stub.bytes.data() + 8) != irql_result) {
        fail("read-only file-query IRQL result address mismatch");
    }

    // First CALL begins at +2 and returns at +7.
    const int32_t irql_rel = static_cast<int32_t>(Le32(stub.bytes.data() + 3));
    if (stub_address + 7u + static_cast<uint32_t>(irql_rel) != irql_export) {
        fail("read-only file-query IRQL CALL target mismatch");
    }

    const auto query_call = std::find(stub.bytes.begin() + 20, stub.bytes.end(), 0xe8);
    if (query_call == stub.bytes.end()) {
        fail("read-only file-query NtQueryFullAttributesFile CALL missing");
    }
    const size_t query_call_offset = static_cast<size_t>(query_call - stub.bytes.begin());
    const int32_t query_rel = static_cast<int32_t>(Le32(stub.bytes.data() + query_call_offset + 1));
    const uint32_t query_target = stub_address + static_cast<uint32_t>(query_call_offset) +
                                  5u + static_cast<uint32_t>(query_rel);
    if (query_target != query_export) {
        fail("read-only file-query NtQueryFullAttributesFile CALL target mismatch");
    }

    bool found_jnz = false;
    for (size_t i = 0; i + 1 < stub.bytes.size(); ++i) {
        if (stub.bytes[i] == 0x0f && stub.bytes[i + 1] == 0x85) {
            found_jnz = true;
            break;
        }
    }
    if (!found_jnz) {
        fail("read-only file-query PASSIVE_LEVEL skip branch missing");
    }
}

void test_kernel_delete_file_stub()
{
    constexpr uint32_t stub_address = 0x68400000u;
    constexpr uint32_t irql_export = 0x800145a4u;
    constexpr uint32_t open_export = 0x80023000u;
    constexpr uint32_t setinfo_export = 0x80024000u;
    constexpr uint32_t close_export = 0x80022000u;
    constexpr uint32_t irql_result = 0x68401000u;
    constexpr uint32_t open_status = 0x68401004u;
    constexpr uint32_t set_status = 0x68401008u;
    constexpr uint32_t close_status = 0x6840100cu;
    constexpr uint32_t operation_ran = 0x68401010u;
    constexpr uint32_t file_handle = 0x68401014u;
    constexpr uint32_t iosb = 0x68401020u;
    constexpr uint32_t object_attributes = 0x68401120u;
    constexpr uint32_t disposition = 0x68401030u;

    KernelDeleteFileStub stub;
    std::string error;
    if (!BuildKernelDeleteFileStub(
            stub_address, irql_export, open_export, setinfo_export, close_export,
            irql_result, open_status, set_status, close_status, operation_ran,
            file_handle, iosb, object_attributes, disposition, stub, error)) {
        fail("kernel delete-file stub build failed: " + error);
    }
    if (stub.bytes.size() < 100u ||
        stub.completion_address + 2u != stub_address + stub.bytes.size()) {
        fail("kernel delete-file stub completion mismatch");
    }
    if (stub.bytes[0] != 0x9c || stub.bytes[1] != 0x60 ||
        stub.bytes[stub.bytes.size() - 4] != 0x61 ||
        stub.bytes[stub.bytes.size() - 3] != 0x9d ||
        stub.bytes[stub.bytes.size() - 2] != 0xeb ||
        stub.bytes[stub.bytes.size() - 1] != 0xfe) {
        fail("kernel delete-file register-preservation opcodes mismatch");
    }

    std::vector<uint32_t> calls;
    for (size_t i = 0; i + 4 < stub.bytes.size(); ++i) {
        if (stub.bytes[i] != 0xe8) continue;
        const int32_t rel = static_cast<int32_t>(Le32(stub.bytes.data() + i + 1));
        calls.push_back(stub_address + static_cast<uint32_t>(i) + 5u +
                        static_cast<uint32_t>(rel));
        i += 4;
    }
    const std::vector<uint32_t> expected = {
        irql_export, open_export, setinfo_export, close_export,
    };
    if (calls != expected) {
        fail("kernel delete-file call sequence mismatch");
    }

    bool found_irql_jnz = false;
    bool found_open_js = false;
    bool found_handle_push = false;
    for (size_t i = 0; i + 1 < stub.bytes.size(); ++i) {
        if (stub.bytes[i] == 0x0f && stub.bytes[i + 1] == 0x85) {
            found_irql_jnz = true;
        }
        if (stub.bytes[i] == 0x0f && stub.bytes[i + 1] == 0x88) {
            found_open_js = true;
        }
        if (i + 5 < stub.bytes.size() && stub.bytes[i] == 0xff &&
            stub.bytes[i + 1] == 0x35 &&
            Le32(stub.bytes.data() + i + 2) == file_handle) {
            found_handle_push = true;
        }
    }
    if (!found_irql_jnz || !found_open_js || !found_handle_push) {
        fail("kernel delete-file safety/control opcodes missing");
    }
    if ((kDeleteAccess | kSynchronizeAccess) != 0x00110000u ||
        kFileShareReadWriteDelete != 7u ||
        (kFileNonDirectoryFile | kFileSynchronousIoNonAlert |
         kFileOpenForBackupIntent) != 0x00004060u ||
        kFileDispositionInformation != 13u) {
        fail("kernel delete-file Xbox constants mismatch");
    }
}


void test_kernel_create_write_stub()
{
    constexpr uint32_t stub_address = 0x68400000u;
    constexpr uint32_t irql_export = 0x800145a4u;
    constexpr uint32_t create_export = 0x80025000u;
    constexpr uint32_t write_export = 0x80027000u;
    constexpr uint32_t flush_export = 0x80026000u;
    constexpr uint32_t close_export = 0x80022000u;
    constexpr uint32_t irql_result = 0x68401000u;
    constexpr uint32_t create_status = 0x68401004u;
    constexpr uint32_t write_status = 0x68401008u;
    constexpr uint32_t flush_status = 0x6840100cu;
    constexpr uint32_t close_status = 0x68401010u;
    constexpr uint32_t operation_ran = 0x68401014u;
    constexpr uint32_t write_information = 0x68401018u;
    constexpr uint32_t file_handle = 0x68401020u;
    constexpr uint32_t iosb = 0x68401030u;
    constexpr uint32_t object_attributes = 0x68401120u;
    constexpr uint32_t byte_offset = 0x68401040u;
    constexpr uint32_t data = 0x68402000u;

    auto call_targets = [&](const KernelCreateWriteStub &stub) {
        std::vector<uint32_t> calls;
        for (size_t i = 0; i + 4u < stub.bytes.size(); ++i) {
            if (stub.bytes[i] != 0xe8) continue;
            const int32_t rel = static_cast<int32_t>(Le32(stub.bytes.data() + i + 1u));
            calls.push_back(stub_address + static_cast<uint32_t>(i) + 5u +
                            static_cast<uint32_t>(rel));
            i += 4u;
        }
        return calls;
    };

    KernelCreateWriteStub file_stub;
    std::string error;
    if (!BuildKernelCreateWriteStub(
            stub_address, irql_export, create_export, write_export, flush_export,
            close_export, irql_result, create_status, write_status, flush_status,
            close_status, operation_ran, write_information, file_handle, iosb,
            object_attributes, byte_offset, data, 0x1234u, kFileCreate, false,
            file_stub, error)) {
        fail("kernel create/write file stub build failed: " + error);
    }
    const std::vector<uint32_t> expected_file = {
        irql_export, create_export, write_export, flush_export, close_export,
    };
    if (call_targets(file_stub) != expected_file) {
        fail("kernel create/write file call sequence mismatch");
    }
    if (file_stub.completion_address + 2u != stub_address + file_stub.bytes.size() ||
        file_stub.bytes.front() != 0x9c || file_stub.bytes[1] != 0x60 ||
        file_stub.bytes[file_stub.bytes.size() - 4u] != 0x61 ||
        file_stub.bytes[file_stub.bytes.size() - 3u] != 0x9d ||
        file_stub.bytes[file_stub.bytes.size() - 2u] != 0xeb ||
        file_stub.bytes.back() != 0xfe) {
        fail("kernel create/write file preservation/completion shape mismatch");
    }

    KernelCreateWriteStub dir_stub;
    if (!BuildKernelCreateWriteStub(
            stub_address, irql_export, create_export, write_export, flush_export,
            close_export, irql_result, create_status, write_status, flush_status,
            close_status, operation_ran, write_information, file_handle, iosb,
            object_attributes, byte_offset, data, 0u, kFileCreate, true,
            dir_stub, error)) {
        fail("kernel create directory stub build failed: " + error);
    }
    const std::vector<uint32_t> expected_dir = {
        irql_export, create_export, close_export,
    };
    if (call_targets(dir_stub) != expected_dir) {
        fail("kernel create directory call sequence mismatch");
    }

    KernelCreateWriteStub empty_stub;
    if (!BuildKernelCreateWriteStub(
            stub_address, irql_export, create_export, write_export, flush_export,
            close_export, irql_result, create_status, write_status, flush_status,
            close_status, operation_ran, write_information, file_handle, iosb,
            object_attributes, byte_offset, data, 0u, kFileCreate, false,
            empty_stub, error)) {
        fail("kernel create empty-file stub build failed: " + error);
    }
    const std::vector<uint32_t> expected_empty = {
        irql_export, create_export, flush_export, close_export,
    };
    if (call_targets(empty_stub) != expected_empty) {
        fail("kernel empty-file create/flush/close sequence mismatch");
    }

    KernelCreateWriteStub bad_stub;
    if (BuildKernelCreateWriteStub(
            stub_address, irql_export, create_export, write_export, flush_export,
            close_export, irql_result, create_status, write_status, flush_status,
            close_status, operation_ran, write_information, file_handle, iosb,
            object_attributes, byte_offset, data, 1u, kFileCreate, true,
            bad_stub, error)) {
        fail("kernel import stub accepted directory data payload");
    }
    if (BuildKernelCreateWriteStub(
            stub_address, irql_export, create_export, write_export, flush_export,
            close_export, irql_result, create_status, write_status, flush_status,
            close_status, operation_ran, write_information, file_handle, iosb,
            object_attributes, byte_offset, data, 0u, kFileOpen + 99u, false,
            bad_stub, error)) {
        fail("kernel import stub accepted unsafe create disposition");
    }

    if ((kFileWriteDataAccess | kSynchronizeAccess) != 0x00100002u ||
        (kFileListDirectoryAccess | kSynchronizeAccess) != 0x00100001u ||
        kFileCreate != 2u || kFileOpen != 1u ||
        kFileAttributeDirectory != 0x10u || kFileAttributeNormal != 0x80u) {
        fail("kernel import Xbox constants mismatch");
    }
}

void test_file_query_objects()
{
    constexpr uint32_t path_address = 0x68401200u;
    constexpr uint32_t ansi_address = 0x68401100u;
    std::array<uint8_t, 8> ansi = {};
    std::array<uint8_t, 12> attrs = {};
    std::string error;
    const std::string path = "E:\\UDATA\\4541009E\\TitleMeta.xbx";
    if (!BuildFileQueryObjects(path, path_address, ansi_address, ansi, attrs, error)) {
        fail("file-query object build failed: " + error);
    }
    if (Le16(ansi.data()) != path.size() ||
        Le16(ansi.data() + 2) != path.size() + 1u ||
        Le32(ansi.data() + 4) != path_address) {
        fail("ANSI_STRING layout mismatch");
    }
    if (Le32(attrs.data()) != kObDosDevicesDirectory ||
        Le32(attrs.data() + 4) != ansi_address ||
        Le32(attrs.data() + 8) != kObjCaseInsensitive) {
        fail("OBJECT_ATTRIBUTES layout mismatch");
    }

    std::array<uint8_t, 8> native_ansi = {};
    std::array<uint8_t, 12> native_attrs = {};
    const std::string native_path = "\\Device\\Harddisk0\\Partition1\\UDATA";
    if (!BuildFileQueryObjects(native_path, path_address, ansi_address,
                               native_ansi, native_attrs, error, 0u)) {
        fail("native file-query object build failed: " + error);
    }
    if (Le32(native_attrs.data()) != 0u ||
        Le32(native_attrs.data() + 4) != ansi_address ||
        Le32(native_attrs.data() + 8) != kObjCaseInsensitive) {
        fail("fully-qualified OBJECT_ATTRIBUTES root mismatch");
    }

    std::array<uint8_t, 64> info = {};
    StoreLe32(info.data() + 40, 0x89abcdefu);
    StoreLe32(info.data() + 44, 0x01234567u);
    StoreLe32(info.data() + 48, 0x20u);
    FileNetworkOpenResult parsed;
    if (!ParseFileNetworkOpenInformation(info.data(), info.size(), parsed) ||
        parsed.end_of_file != 0x0123456789abcdefull ||
        parsed.attributes != 0x20u) {
        fail("FILE_NETWORK_OPEN_INFORMATION decode mismatch");
    }
}

void test_kernel_delete_directory_stub()
{
    constexpr uint32_t stub_address = 0x68400000u;
    constexpr uint32_t irql_export = 0x800145a4u;
    constexpr uint32_t open_export = 0x80023000u;
    constexpr uint32_t setinfo_export = 0x80024000u;
    constexpr uint32_t close_export = 0x80022000u;
    constexpr uint32_t irql_result = 0x68401000u;
    constexpr uint32_t open_status = 0x68401004u;
    constexpr uint32_t set_status = 0x68401008u;
    constexpr uint32_t close_status = 0x6840100cu;
    constexpr uint32_t operation_ran = 0x68401010u;
    constexpr uint32_t file_handle = 0x68401014u;
    constexpr uint32_t iosb = 0x68401020u;
    constexpr uint32_t object_attributes = 0x68401120u;
    constexpr uint32_t disposition = 0x68401030u;

    KernelDeleteFileStub file_stub;
    KernelDeleteFileStub dir_stub;
    std::string error;
    if (!BuildKernelDeleteFileStub(
            stub_address, irql_export, open_export, setinfo_export, close_export,
            irql_result, open_status, set_status, close_status, operation_ran,
            file_handle, iosb, object_attributes, disposition, file_stub, error)) {
        fail("file delete stub build failed in directory comparison: " + error);
    }
    if (!BuildKernelDeleteDirectoryStub(
            stub_address, irql_export, open_export, setinfo_export, close_export,
            irql_result, open_status, set_status, close_status, operation_ran,
            file_handle, iosb, object_attributes, disposition, dir_stub, error)) {
        fail("directory delete stub build failed: " + error);
    }
    if (file_stub.bytes.size() != dir_stub.bytes.size() ||
        file_stub.completion_address != dir_stub.completion_address) {
        fail("file/directory delete stub shape mismatch");
    }

    const uint32_t file_options = kFileNonDirectoryFile |
                                  kFileSynchronousIoNonAlert |
                                  kFileOpenForBackupIntent;
    const uint32_t dir_options = kFileDirectoryFile |
                                 kFileSynchronousIoNonAlert |
                                 kFileOpenForBackupIntent;
    if (file_options != 0x00004060u || dir_options != 0x00004021u) {
        fail("file/directory open-option constants mismatch");
    }

    size_t differences = 0;
    for (size_t i = 0; i < file_stub.bytes.size(); ++i) {
        if (file_stub.bytes[i] != dir_stub.bytes[i]) {
            ++differences;
        }
    }
    if (differences != 1u) {
        fail("directory delete stub should differ only in the open-options immediate");
    }

    auto contains_le32 = [](const std::vector<uint8_t> &bytes, uint32_t value) {
        for (size_t i = 0; i + 4u <= bytes.size(); ++i) {
            if (Le32(bytes.data() + i) == value) return true;
        }
        return false;
    };
    if (!contains_le32(file_stub.bytes, file_options) ||
        !contains_le32(dir_stub.bytes, dir_options)) {
        fail("file/directory delete open-options immediate missing");
    }
}


} // namespace

int main()
{
    test_resolver();
    test_irql_stub();
    test_read_only_file_query_stub();
    test_kernel_delete_file_stub();
    test_kernel_delete_directory_stub();
    test_kernel_create_write_stub();
    test_file_query_objects();
    std::cout << "PASS: Xbox kernel ordinal resolver, read-only RPC, delete stubs, and create/write import stub\n";
    return 0;
}
