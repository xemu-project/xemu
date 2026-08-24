// v2.87 current regression ownership.
#include "xbe-labels.hh"
#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <vector>

static bool same(const XemuXbeLabels::Label &a, const XemuXbeLabels::Label &b) {
    return a.virtual_address == b.virtual_address && a.type == b.type &&
           a.name == b.name && a.source == b.source &&
           a.confidence == b.confidence &&
           a.section_name == b.section_name &&
           a.section_offset == b.section_offset &&
           a.has_section_location == b.has_section_location;
}
int main() {
    std::mt19937 rng(0x182);
    for (int round = 0; round < 1000; ++round) {
        XemuXbeLabels::Database old_db, new_db;
        for (int source_batch = 0; source_batch < 6; ++source_batch) {
            std::vector<XemuXbeLabels::Label> batch;
            const int n = 1 + (rng() % 200);
            batch.reserve(n);
            for (int i = 0; i < n; ++i) {
                XemuXbeLabels::Label l;
                l.virtual_address = 0x10000u + (rng() % 0x20000u);
                l.type = static_cast<XemuXbeLabels::Type>(rng() % 9);
                l.source = static_cast<XemuXbeLabels::Source>(rng() % 5);
                l.confidence = static_cast<XemuXbeLabels::Confidence>(rng() % 4);
                l.name = "sym_" + std::to_string(rng() % 400);
                l.section_name = ".text";
                l.section_offset = l.virtual_address - 0x10000u;
                l.has_section_location = true;
                batch.push_back(std::move(l));
                if ((rng() & 7u) == 0 && !batch.empty()) batch.push_back(batch.back());
            }
            XemuXbeLabels::Merge(old_db, batch);
            XemuXbeLabels::Append(new_db, batch);
        }
        XemuXbeLabels::SortAndUnique(new_db);
        if (old_db.labels.size() != new_db.labels.size()) return 2;
        for (size_t i = 0; i < old_db.labels.size(); ++i) {
            if (!same(old_db.labels[i], new_db.labels[i])) return 3;
        }
    }
    std::cout << "PASS: batched label append/sort is identical to repeated Merge\n";
    return 0;
}
