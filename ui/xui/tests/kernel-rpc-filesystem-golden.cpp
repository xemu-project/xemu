// v2.87 current regression ownership.
#include "kernel-rpc-filesystem.hh"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static void Expect(bool condition, const char *message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

static XemuFatxHdd::Entry Directory(const std::string &name,
                                    std::vector<XemuFatxHdd::Entry> children = {})
{
    XemuFatxHdd::Entry entry;
    entry.name = name;
    entry.directory = true;
    entry.attributes = 0x10;
    entry.children = std::move(children);
    return entry;
}

static XemuFatxHdd::Entry File(const std::string &name, uint32_t size)
{
    XemuFatxHdd::Entry entry;
    entry.name = name;
    entry.directory = false;
    entry.file_size = size;
    entry.attributes = 0;
    return entry;
}

static XemuFatxHdd::Snapshot MakeSnapshot(bool include_delete_tree)
{
    XemuFatxHdd::Snapshot snapshot;
    snapshot.hdd_available = true;

    XemuFatxHdd::Partition e;
    e.letter = 'E';
    e.available = true;

    std::vector<XemuFatxHdd::Entry> title_children;
    if (include_delete_tree) {
        title_children.push_back(Directory("DeleteMe", {
            File("root.bin", 11),
            Directory("A", {
                File("a.bin", 22),
                Directory("B", {File("b.bin", 33)}),
            }),
        }));
    }
    XemuFatxHdd::Entry title = Directory("4541009E", std::move(title_children));
    e.entries.push_back(Directory("UDATA", {std::move(title)}));
    e.entries.push_back(Directory("TDATA", {Directory("4541009E")}));
    snapshot.partitions.push_back(std::move(e));
    return snapshot;
}

static void WriteBytes(const fs::path &path, size_t count, uint8_t seed)
{
    std::ofstream out(path, std::ios::binary);
    Expect((bool)out, "could not create host golden file");
    std::vector<uint8_t> bytes(count);
    for (size_t i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<uint8_t>(seed + i);
    }
    out.write(reinterpret_cast<const char *>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    Expect((bool)out, "could not write host golden file");
}

struct TempTree {
    fs::path base;
    ~TempTree()
    {
        std::error_code ec;
        fs::remove_all(base, ec);
    }
};

int main()
{
    try {
        // Recursive delete planning remains leaf-first and includes the selected
        // directory as the final operation.
        XemuFatxHdd::Snapshot delete_snapshot = MakeSnapshot(true);
        std::vector<XemuKernelFs::DeleteEntry> delete_plan;
        std::string error;
        Expect(XemuKernelFs::BuildDeletePlan(
                   delete_snapshot, 'E', {"UDATA", "4541009E", "DeleteMe"},
                   true, delete_plan, error),
               error.c_str());
        Expect(delete_plan.size() == 6, "unexpected recursive delete plan size");
        const XemuKernelFs::DeletePlanSummary summary =
            XemuKernelFs::SummarizeDeletePlan(delete_plan);
        Expect(summary.file_count == 3, "recursive file count changed");
        Expect(summary.directory_count == 3, "recursive directory count changed");
        Expect(!delete_plan[0].directory && delete_plan[0].components.back() == "root.bin",
               "first root file ordering changed");
        Expect(!delete_plan[1].directory && delete_plan[1].components.back() == "a.bin",
               "nested file ordering changed");
        Expect(!delete_plan[2].directory && delete_plan[2].components.back() == "b.bin",
               "deep file ordering changed");
        Expect(delete_plan[3].directory && delete_plan[3].components.back() == "B",
               "deep directory must follow its children");
        Expect(delete_plan[4].directory && delete_plan[4].components.back() == "A",
               "parent directory must follow descendants");
        Expect(delete_plan[5].directory && delete_plan[5].components.back() == "DeleteMe",
               "selected root must be deleted last");
        Expect(delete_plan[5].fatx_path == "E:\\UDATA\\4541009E\\DeleteMe",
               "FATX path mapping changed");
        Expect(delete_plan[5].native_path ==
                   "\\Device\\Harddisk0\\Partition1\\UDATA\\4541009E\\DeleteMe",
               "native E: path mapping changed");
        Expect(XemuKernelFs::SameDeletePlan(delete_plan, delete_plan),
               "identical recursive plans must compare equal");
        auto changed_plan = delete_plan;
        changed_plan[0].directory = true;
        Expect(!XemuKernelFs::SameDeletePlan(delete_plan, changed_plan),
               "path/type changes must invalidate confirmation");
        // Build a real host tree to freeze parent-first import planning, exact
        // byte/operation totals, immutable host validation, and chunk reads.
        const auto nonce = std::chrono::high_resolution_clock::now()
                               .time_since_epoch().count();
        TempTree temp{fs::temp_directory_path() /
                      ("xemu-kernel-rpc-fs-golden-" + std::to_string(nonce))};
        const fs::path root = temp.base / "ImportRoot";
        fs::create_directories(root / "A");
        fs::create_directories(root / "B");
        WriteBytes(root / "A" / "tiny.bin", 3, 0x10);
        WriteBytes(root / "B" / "large.bin",
                   XemuKernelFs::kImportChunkBytes + 5u, 0x20);
        WriteBytes(root / "empty.bin", 0, 0);

        XemuFatxHdd::Snapshot import_snapshot = MakeSnapshot(false);
        XemuKernelFs::ImportPlan import_plan;
        error.clear();
        Expect(XemuKernelFs::BuildImportFolderPlanAtDestination(
                   root.u8string(), import_snapshot, 'E',
                   {"UDATA", "4541009E"}, import_plan, error),
               error.c_str());
        Expect(import_plan.root_name == "ImportRoot", "host root name changed");
        Expect(import_plan.directory_count == 3, "import directory count changed");
        Expect(import_plan.file_count == 3, "import file count changed");
        Expect(import_plan.total_bytes ==
                   static_cast<uint64_t>(XemuKernelFs::kImportChunkBytes) + 8u,
               "import byte total changed");
        Expect(import_plan.total_operations == 7, "import operation count changed");
        Expect(import_plan.entries.size() == 6, "import plan entry count changed");
        Expect(import_plan.entries.front().directory &&
                   import_plan.entries.front().components.back() == "ImportRoot",
               "import root must be created first");
        Expect(import_plan.entries[1].directory &&
                   import_plan.entries[1].components.back() == "A",
               "A directory parent-first ordering changed");
        Expect(!import_plan.entries[2].directory &&
                   import_plan.entries[2].components.back() == "tiny.bin",
               "A child file ordering changed");
        Expect(import_plan.entries[3].directory &&
                   import_plan.entries[3].components.back() == "B",
               "B directory parent-first ordering changed");

        for (const XemuKernelFs::ImportEntry &item : import_plan.entries) {
            error.clear();
            Expect(XemuKernelFs::ValidateImportHostEntryMetadata(item, error), error.c_str());
        }
        XemuKernelFs::ImportPlan fresh_import_plan;
        error.clear();
        Expect(XemuKernelFs::BuildImportFolderPlanAtDestination(
                   root.u8string(), import_snapshot, 'E',
                   {"UDATA", "4541009E"}, fresh_import_plan, error),
               error.c_str());
        Expect(XemuKernelFs::SameImportPlan(import_plan, fresh_import_plan),
               "fresh host import plan changed before mutation");

        const auto large_it = std::find_if(
            import_plan.entries.begin(), import_plan.entries.end(),
            [](const XemuKernelFs::ImportEntry &entry) {
                return !entry.directory && !entry.components.empty() &&
                       entry.components.back() == "large.bin";
            });
        Expect(large_it != import_plan.entries.end(), "large import file missing");
        std::vector<uint8_t> chunk;
        uint32_t chunk_bytes = 0;
        uint64_t expected_file_size = 0;
        error.clear();
        Expect(XemuKernelFs::LoadImportFileChunk(
                   *large_it, 0, chunk, chunk_bytes, expected_file_size, error),
               error.c_str());
        Expect(chunk_bytes == XemuKernelFs::kImportChunkBytes,
               "first import chunk size changed");
        Expect(expected_file_size == XemuKernelFs::kImportChunkBytes,
               "first import expected-size checkpoint changed");
        Expect(chunk.size() == XemuKernelFs::kImportChunkBytes,
               "first import chunk buffer size changed");
        Expect(chunk.front() == 0x20, "first import chunk data changed");

        error.clear();
        Expect(XemuKernelFs::LoadImportFileChunk(
                   *large_it, XemuKernelFs::kImportChunkBytes,
                   chunk, chunk_bytes, expected_file_size, error),
               error.c_str());
        Expect(chunk_bytes == 5, "final import chunk size changed");
        Expect(expected_file_size ==
                   static_cast<uint64_t>(XemuKernelFs::kImportChunkBytes) + 5u,
               "final import expected-size checkpoint changed");

        // v2.15 general HDD frontend: every exposed FATX drive maps to the
        // corresponding Xbox kernel device object. Import into a volume root is
        // valid, while deleting the volume root itself stays hard-blocked.
        const std::pair<char, int> mappings[] = {
            {'E', 1}, {'C', 2}, {'X', 3}, {'Y', 4},
            {'Z', 5}, {'F', 6}, {'G', 7},
        };
        for (const auto &mapping : mappings) {
            std::string native;
            error.clear();
            Expect(XemuKernelFs::IsKernelWritablePartition(mapping.first),
                   "expected mapped FATX partition");
            Expect(XemuKernelFs::NativePathForPartition(
                       mapping.first, {"Folder"}, native, error),
                   error.c_str());
            const std::string expected = "\\Device\\Harddisk0\\Partition" +
                                         std::to_string(mapping.second) +
                                         "\\Folder";
            Expect(native == expected, "general Xbox partition mapping changed");
        }
        Expect(!XemuKernelFs::IsKernelWritablePartition('Q'),
               "unknown FATX partition unexpectedly writable");

        XemuFatxHdd::Partition c;
        c.letter = 'C';
        c.available = true;
        c.entries.push_back(Directory("DeleteTree", {File("inside.bin", 9)}));
        c.entries.push_back(File("delete.bin", 4));
        import_snapshot.partitions.push_back(std::move(c));

        std::vector<XemuKernelFs::DeleteEntry> general_delete;
        error.clear();
        Expect(XemuKernelFs::BuildDeletePlan(
                   import_snapshot, 'C', {"DeleteTree"}, true,
                   general_delete, error),
               error.c_str());
        Expect(general_delete.size() == 2,
               "general recursive delete plan size changed");
        Expect(!general_delete[0].directory &&
                   general_delete[0].fatx_path == "C:\\DeleteTree\\inside.bin",
               "general recursive file path changed");
        Expect(general_delete[1].directory &&
                   general_delete[1].native_path ==
                       "\\Device\\Harddisk0\\Partition2\\DeleteTree",
               "general recursive directory path changed");
        general_delete.clear();
        error.clear();
        Expect(XemuKernelFs::BuildDeletePlan(
                   import_snapshot, 'C', {"delete.bin"}, false,
                   general_delete, error),
               error.c_str());
        Expect(general_delete.size() == 1 && !general_delete[0].directory,
               "general single-file delete plan changed");
        general_delete.clear();
        error.clear();
        Expect(!XemuKernelFs::BuildDeletePlan(
                   import_snapshot, 'C', {}, true, general_delete, error),
               "partition-root delete must remain blocked");

        XemuKernelFs::ImportPlan root_folder_plan;
        error.clear();
        Expect(XemuKernelFs::BuildImportFolderPlanAtDestination(
                   root.u8string(), import_snapshot, 'C', {},
                   root_folder_plan, error),
               error.c_str());
        Expect(root_folder_plan.partition == 'C' &&
                   root_folder_plan.destination_components.empty(),
               "root folder-import destination changed");
        Expect(root_folder_plan.entries.front().fatx_path == "C:\\ImportRoot",
               "C: root folder import FATX path changed");
        Expect(root_folder_plan.entries.front().native_path ==
                   "\\Device\\Harddisk0\\Partition2\\ImportRoot",
               "C: root folder import native path changed");

        const fs::path single = temp.base / "single.bin";
        WriteBytes(single, 19, 0x44);
        XemuKernelFs::ImportPlan root_file_plan;
        error.clear();
        Expect(XemuKernelFs::BuildImportFilePlanAtDestination(
                   single.u8string(), import_snapshot, 'C', {},
                   root_file_plan, error),
               error.c_str());
        Expect(!root_file_plan.source_is_directory &&
                   root_file_plan.file_count == 1 &&
                   root_file_plan.directory_count == 0,
               "root single-file import plan changed");
        Expect(root_file_plan.entries.size() == 1 &&
                   root_file_plan.entries[0].fatx_path == "C:\\single.bin",
               "C: root file import path changed");

        // Destination collision must still stop before any kernel mutation.
        XemuFatxHdd::Partition *e = XemuFatxHdd::FindPartition(import_snapshot, 'E');
        Expect(e != nullptr, "synthetic E partition missing");
        XemuFatxHdd::Entry *title = XemuFatxHdd::FindEntry(*e, {"UDATA", "4541009E"});
        Expect(title != nullptr, "synthetic Title-ID root missing");
        title->children.push_back(Directory("ImportRoot"));
        XemuKernelFs::ImportPlan collision_plan;
        error.clear();
        Expect(!XemuKernelFs::BuildImportFolderPlanAtDestination(
                   root.u8string(), import_snapshot, 'E',
                   {"UDATA", "4541009E"}, collision_plan, error),
               "existing destination must block import");

        // Planned host file size is immutable for the duration of an import.
        {
            std::ofstream out(root / "A" / "tiny.bin", std::ios::binary | std::ios::app);
            out.put('\x55');
        }
        const auto tiny_it = std::find_if(
            import_plan.entries.begin(), import_plan.entries.end(),
            [](const XemuKernelFs::ImportEntry &entry) {
                return !entry.directory && !entry.components.empty() &&
                       entry.components.back() == "tiny.bin";
            });
        Expect(tiny_it != import_plan.entries.end(), "tiny import file missing");
        error.clear();
        Expect(!XemuKernelFs::ValidateImportHostEntryMetadata(*tiny_it, error),
               "changed host file size must invalidate import entry");

        Expect(XemuKernelFs::EqualsNoCase("UDATA", "udata"),
               "case-insensitive FATX comparison changed");
        error.clear();
        Expect(!XemuKernelFs::IsSafeFatxImportComponent("bad:name", error),
               "forbidden FATX import character accepted");
        error.clear();
        Expect(!XemuKernelFs::IsSafeFatxImportComponent(std::string(43, 'A'), error),
               "overlong FATX import name accepted");

        std::cout << "PASS: Kernel RPC filesystem backend golden\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
