// v2.87 current regression ownership.
#include "kernel-rpc-filesystem.hh"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

static XemuFatxHdd::Entry File(const char *name, uint32_t size, uint64_t off,
                               uint32_t cluster, uint16_t date, uint16_t time,
                               uint8_t attrs = 0)
{
    XemuFatxHdd::Entry e;
    e.name = name;
    e.file_size = size;
    e.directory_entry_offset = off;
    e.first_cluster = cluster;
    e.modified_date = date;
    e.modified_time = time;
    e.attributes = attrs;
    return e;
}

static XemuFatxHdd::Entry Dir(const char *name, uint64_t off, uint32_t cluster,
                              std::vector<XemuFatxHdd::Entry> children = {})
{
    XemuFatxHdd::Entry e;
    e.name = name;
    e.directory = true;
    e.attributes = 0x10;
    e.directory_entry_offset = off;
    e.first_cluster = cluster;
    e.children = std::move(children);
    return e;
}

int main()
{
    XemuFatxHdd::Snapshot snapshot;
    snapshot.hdd_available = true;
    XemuFatxHdd::Partition e;
    e.letter = 'E'; e.available = true;
    e.entries.push_back(Dir("SRC", 0x1000, 2, {
        File("large.bin", XemuKernelFs::kImportChunkBytes + 1u,
             0x1100, 3, 0x5D10, 0x8123, 0x01),
        Dir("sub", 0x1200, 4, {File("tiny.bin", 7, 0x1300, 5, 0x5D11, 0x8124)})
    }));
    e.entries.push_back(Dir("DEST", 0x1400, 6));
    XemuFatxHdd::Partition f;
    f.letter = 'F'; f.available = true;
    f.entries.push_back(Dir("OUT", 0x2000, 2));
    snapshot.partitions.push_back(e);
    snapshot.partitions.push_back(f);

    XemuKernelFs::ImportPlan copy;
    std::string error;
    assert(XemuKernelFs::BuildFatxCopyPlan(snapshot, 'E', {"SRC"}, true,
                                           'F', {"OUT"}, false, copy, error));
    assert(error.empty());
    assert(copy.source_from_fatx && !copy.delete_source_after_copy);
    assert(copy.file_count == 2 && copy.directory_count == 2);
    // root dir + large.bin(2 chunks) + sub dir + tiny.bin(1 chunk)
    assert(copy.total_operations == 5);
    assert(copy.entries.size() == 4);
    const auto &large = copy.entries[1];
    assert(!large.directory);
    assert(large.source_directory_entry_offset == 0x1100);
    assert(large.source_first_cluster == 3);
    assert(large.source_modified_date == 0x5D10);
    assert(large.source_modified_time == 0x8123);
    assert(large.source_attributes == 0x01);

    assert(XemuKernelFs::ExpectedCommittedFileSize(0, XemuKernelFs::kImportChunkBytes)
           == XemuKernelFs::kImportChunkBytes);
    assert(XemuKernelFs::ExpectedCommittedFileSize(XemuKernelFs::kImportChunkBytes, 1)
           == XemuKernelFs::kImportChunkBytes + 1u);

    XemuKernelFs::ImportPlan changed = copy;
    changed.entries[1].source_modified_time ^= 1;
    assert(!XemuKernelFs::SameImportPlan(copy, changed));

    XemuKernelFs::ImportPlan move;
    assert(XemuKernelFs::BuildFatxCopyPlan(snapshot, 'E', {"SRC"}, true,
                                           'F', {"OUT"}, true, move, error));
    assert(move.delete_source_after_copy);
    assert(!move.source_delete_plan.empty());
    const auto summary = XemuKernelFs::SummarizeDeletePlan(move.source_delete_plan);
    assert(summary.file_count == 2 && summary.directory_count == 2);

    // Same-volume descendant copy/move must fail closed.
    XemuKernelFs::ImportPlan invalid;
    assert(!XemuKernelFs::BuildFatxCopyPlan(snapshot, 'E', {"SRC"}, true,
                                            'E', {"SRC", "sub"}, false,
                                            invalid, error));

    std::cout << "PASS: filesystem transfer behavior contract golden\n";
    return 0;
}
