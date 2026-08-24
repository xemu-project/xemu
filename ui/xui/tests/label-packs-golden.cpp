// v2.87 current regression ownership.
// Portable .xlabel label-pack golden tests.
#include "label-packs.hh"

#include <cstdio>
#include <string>
#include <vector>

namespace {

static int fail(const char *what)
{
    std::fprintf(stderr, "FAIL: %s\n", what);
    return 1;
}

} // namespace

int main()
{
    using namespace XemuXbeLabels;

    Database source;
    source.image_base = 0x00010000u;
    source.image_size = 0x00040000u;
    source.sections.push_back({".text", 0x00011000u, 0x1000u, 0x1000u});
    source.sections.push_back({".rdata", 0x00013000u, 0x1000u, 0x1000u});

    Label manual;
    manual.virtual_address = 0x00011234u;
    manual.type = Type::Inferred;
    manual.name = "UnlockSystem_IsDLCUnlock";
    manual.source = Source::Manual;
    manual.confidence = Confidence::Manual;
    manual.section_name = ".text";
    manual.section_offset = 0x234u;
    manual.has_section_location = true;
    source.labels.push_back(manual);

    Label map_symbol;
    map_symbol.virtual_address = 0x00011300u;
    map_symbol.type = Type::Function;
    map_symbol.name = "cCheatSettings::Init";
    map_symbol.source = Source::Map;
    map_symbol.confidence = Confidence::Exact;
    map_symbol.section_name = ".text";
    map_symbol.section_offset = 0x300u;
    map_symbol.has_section_location = true;
    source.labels.push_back(map_symbol);

    Label outside;
    outside.virtual_address = 0x00010500u;
    outside.type = Type::Section;
    outside.name = "XBE_HeaderThing";
    outside.source = Source::Xbe;
    outside.confidence = Confidence::Exact;
    source.labels.push_back(outside);
    SortAndUnique(source);

    XemuLabelPacks::Identity identity;
    identity.title_id = 0x4541009Eu;
    identity.game_id = "EA009E";
    identity.name = "Example Game";
    identity.header_sha256 =
        "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF";
    identity.xbe_sha256 =
        "FEDCBA9876543210FEDCBA9876543210FEDCBA9876543210FEDCBA9876543210";

    std::string text;
    std::string error;
    if (!XemuLabelPacks::Serialize(identity, source, text, error)) {
        std::fprintf(stderr, "FAIL: serialize: %s\n", error.c_str());
        return 1;
    }
    if (text.find("PHYSICAL") != std::string::npos) {
        return fail("portable format must not persist physical addresses");
    }
    if (text.find(".text|00000234|00011234|INFERRED|MANUAL|MANUAL|") ==
        std::string::npos) {
        return fail("section-relative record");
    }
    if (text.find("@VA|00010500|00010500|SECTION|XBE|EXACT|") ==
        std::string::npos) {
        return fail("non-section virtual fallback record");
    }
    if (text.find(".text|00000300|00011300|FUNCTION|MAP|EXACT|cCheatSettings::Init") ==
        std::string::npos) {
        return fail("MAP provenance record");
    }

    XemuLabelPacks::Pack pack;
    if (!XemuLabelPacks::Parse(text, pack, error)) {
        std::fprintf(stderr, "FAIL: parse: %s\n", error.c_str());
        return 1;
    }
    std::string reason;
    if (!XemuLabelPacks::Matches(pack.header, identity, reason)) {
        std::fprintf(stderr, "FAIL: matching identity: %s\n", reason.c_str());
        return 1;
    }

    // Simulate a new runtime/XBE layout where .text moved.  The .xlabel
    // section offset must relocate the label; no physical address participates.
    Database relocated = source;
    relocated.labels.clear();
    relocated.sections[0].virtual_address = 0x00021000u;

    std::vector<Label> resolved;
    size_t unresolved = 0;
    if (!XemuLabelPacks::Resolve(pack, relocated, resolved, unresolved, error)) {
        std::fprintf(stderr, "FAIL: resolve: %s\n", error.c_str());
        return 1;
    }
    bool found_relocated = false;
    bool found_va = false;
    for (const Label &label : resolved) {
        if (label.name == "UnlockSystem_IsDLCUnlock" &&
            label.virtual_address == 0x00021234u &&
            label.source == Source::Manual) {
            found_relocated = true;
        }
        if (label.name == "XBE_HeaderThing" &&
            label.virtual_address == 0x00010500u) {
            found_va = true;
        }
    }
    if (!found_relocated) return fail("section-relative relocation");
    if (!found_va) return fail("@VA exact-revision fallback");
    if (unresolved != 0) return fail("unexpected unresolved entries");

    // Manual/PDB/XDK provenance has priority over auto-XBE labels at the same
    // address when the disassembler asks for one primary name.
    Label auto_label;
    auto_label.virtual_address = 0x00021234u;
    auto_label.type = Type::Inferred;
    auto_label.name = "~AutoGuess";
    auto_label.source = Source::Xbe;
    auto_label.confidence = Confidence::Inferred;
    relocated.labels.push_back(auto_label);
    Merge(relocated, resolved);
    const Label *primary = PrimaryAt(relocated, 0x00021234u);
    if (primary == nullptr || primary->name != "UnlockSystem_IsDLCUnlock") {
        return fail("manual provenance primary-label priority");
    }

    XemuLabelPacks::Identity wrong = identity;
    wrong.title_id ^= 1u;
    if (XemuLabelPacks::Matches(pack.header, wrong, reason)) {
        return fail("wrong GameID rejection");
    }

    std::puts("PASS: portable .xlabel parse/save/match/relocation golden cases");
    return 0;
}
