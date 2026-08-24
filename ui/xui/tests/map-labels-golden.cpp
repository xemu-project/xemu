// v2.87 current regression ownership.
// Microsoft linker MAP importer golden tests.
#include "map-labels.hh"

#include <cstdio>
#include <string>
#include <vector>

namespace {

static int fail(const char *what)
{
    std::fprintf(stderr, "FAIL: %s\n", what);
    return 1;
}

static const XemuXbeLabels::Label *find_label(
    const std::vector<XemuXbeLabels::Label> &labels, uint32_t address,
    const char *name)
{
    for (const auto &label : labels) {
        if (label.virtual_address == address && label.name == name) {
            return &label;
        }
    }
    return nullptr;
}

} // namespace

int main()
{
    XemuXbeLabels::Database db;
    db.sections.push_back({".text", 0x00011000u, 0x200, 0x200,
                           0x1000, 0x4});
    db.sections.push_back({".data", 0x00012000u, 0x100, 0x80,
                           0x2000, 0});

    const std::string map =
        " Example Game\n"
        "\n"
        " Timestamp is 12345678 (test)\n"
        "\n"
        " Preferred load address is 00400000\n"
        "\n"
        " Start         Length     Name                   Class\n"
        " 0001:00000000 00000200H .text                   CODE\n"
        " 0002:00000000 00000100H .data                   DATA\n"
        "\n"
        " Address         Publics by Value              Rva+Base     Lib:Object\n"
        "\n"
        " 0001:00000010       ?Pause@cBackgroundLoader@@UAEXXZ 00401010 f   BackgroundLoader.obj\n"
        " 0001:00000030       ??0cBackgroundLoader@@QAE@H@Z 00401030 f   BackgroundLoader.obj\n"
        " 0002:00000004       _gGameState              00402004     Game.obj\n"
        "\n"
        " Static symbols\n"
        "\n"
        " 0001:00000050       _helper@4                00401050 f   Game.obj\n"
        " 0001:00000060       $L1234                   00401060 f   Game.obj\n";

    std::vector<XemuXbeLabels::Label> labels;
    XemuMapLabels::Status status;
    std::string error;
    if (!XemuMapLabels::ParseAndResolve(map, db, 0x12345678u,
                                        labels, status, error)) {
        std::fprintf(stderr, "FAIL: exact MAP rejected: %s\n", error.c_str());
        return 1;
    }
    if (!status.parsed || !status.timestamp_match || !status.layout_match ||
        status.mapped_segments != 2 || status.parsed_symbols != 5) {
        return fail("exact MAP status");
    }

    const auto *pause = find_label(labels, 0x00011010u,
                                   "cBackgroundLoader::Pause");
    if (pause == nullptr || pause->type != XemuXbeLabels::Type::Function ||
        pause->source != XemuXbeLabels::Source::Map ||
        pause->confidence != XemuXbeLabels::Confidence::Exact ||
        !pause->has_section_location || pause->section_name != ".text" ||
        pause->section_offset != 0x10) {
        return fail("C++ MAP function label");
    }

    if (find_label(labels, 0x00011030u,
                   "cBackgroundLoader::cBackgroundLoader") == nullptr) {
        return fail("constructor demangle");
    }
    const auto *data = find_label(labels, 0x00012004u, "gGameState");
    if (data == nullptr || data->type != XemuXbeLabels::Type::Symbol) {
        return fail("MAP data symbol");
    }
    if (find_label(labels, 0x00011050u, "helper") == nullptr) {
        return fail("stdcall cleanup");
    }
    if (find_label(labels, 0x00011060u, "$L1234") != nullptr) {
        return fail("compiler-local filtering");
    }

    std::vector<XemuXbeLabels::Label> rejected;
    XemuMapLabels::Status mismatch;
    error.clear();
    if (XemuMapLabels::ParseAndResolve(map, db, 0x87654321u,
                                       rejected, mismatch, error)) {
        return fail("timestamp mismatch accepted");
    }
    if (!mismatch.parsed || mismatch.timestamp_match || !rejected.empty() ||
        error.find("timestamp mismatch") == std::string::npos) {
        return fail("timestamp mismatch diagnostics");
    }

    XemuXbeLabels::Database small = db;
    small.sections[0].virtual_size = 0x100;
    small.sections[0].raw_size = 0x100;
    XemuMapLabels::Status layout;
    error.clear();
    if (XemuMapLabels::ParseAndResolve(map, small, 0x12345678u,
                                       rejected, layout, error)) {
        return fail("layout mismatch accepted");
    }
    if (layout.layout_match ||
        error.find("segment layout") == std::string::npos) {
        return fail("layout mismatch diagnostics");
    }

    std::puts("PASS: Microsoft linker MAP importer golden cases");
    return 0;
}
