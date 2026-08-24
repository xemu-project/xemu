// v2.87 current regression ownership.
// Microsoft PDB importer golden tests.
#include "pdb-labels.hh"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int fail(const char *message)
{
    std::fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

static void put16(std::vector<uint8_t> &out, size_t off, uint16_t value)
{
    out[off + 0] = (uint8_t)value;
    out[off + 1] = (uint8_t)(value >> 8);
}

static void put32(std::vector<uint8_t> &out, size_t off, uint32_t value)
{
    out[off + 0] = (uint8_t)value;
    out[off + 1] = (uint8_t)(value >> 8);
    out[off + 2] = (uint8_t)(value >> 16);
    out[off + 3] = (uint8_t)(value >> 24);
}

static void append_pub(std::vector<uint8_t> &stream, uint32_t flags,
                       uint32_t offset, uint16_t segment,
                       const char *name)
{
    const size_t name_len = std::strlen(name) + 1;
    const uint16_t record_length = (uint16_t)(2 + 10 + name_len);
    const size_t start = stream.size();
    stream.resize(start + 2 + record_length);
    put16(stream, start, record_length);
    put16(stream, start + 2, 0x110e);
    put32(stream, start + 4, flags);
    put32(stream, start + 8, offset);
    put16(stream, start + 12, segment);
    std::memcpy(stream.data() + start + 14, name, name_len);
}

static std::vector<uint8_t> make_pdb(const uint8_t guid[16], uint32_t age,
                                     uint32_t text_size)
{
    constexpr uint32_t bs = 512;
    constexpr uint32_t blocks = 9;
    std::vector<uint8_t> file((size_t)bs * blocks, 0);
    static const uint8_t magic[32] = {
        'M','i','c','r','o','s','o','f','t',' ','C','/','C','+','+',' ','M','S','F',' ',
        '7','.','0','0','\r','\n',0x1a,'D','S',0,0,0
    };
    std::memcpy(file.data(), magic, sizeof(magic));
    put32(file, 32, bs);
    put32(file, 36, 1);
    put32(file, 40, blocks);

    std::vector<uint8_t> info(28, 0);
    put32(info, 0, 20000404);
    put32(info, 4, 0x12345678);
    put32(info, 8, age);
    std::memcpy(info.data() + 12, guid, 16);

    std::vector<uint8_t> dbi(64 + 22, 0);
    put32(dbi, 0, 0xffffffffu);
    put32(dbi, 4, 19990903);
    put32(dbi, 8, age);
    put16(dbi, 12, 0xffff);
    put16(dbi, 16, 0xffff);
    put16(dbi, 20, 4); // SymRecordStream
    put32(dbi, 48, 22); // OptionalDbgHeaderSize
    put16(dbi, 58, 0x14c);
    for (size_t i = 64; i < 86; i += 2) {
        put16(dbi, i, 0xffff);
    }
    put16(dbi, 64 + 5 * 2, 5); // original section headers

    std::vector<uint8_t> symbols;
    append_pub(symbols, 2, 0x20, 1, "?Test@cThing@@SAXXZ");
    append_pub(symbols, 0, 0x40, 1, "_g_Value");

    std::vector<uint8_t> sections(40, 0);
    std::memcpy(sections.data(), ".text", 5);
    put32(sections, 8, text_size);
    put32(sections, 12, 0x440);
    put32(sections, 16, text_size);
    put32(sections, 36, 0x60000020);

    const std::vector<std::vector<uint8_t>> streams = {
        {}, info, {}, dbi, symbols, sections
    };
    const uint32_t directory_bytes =
        4 + (uint32_t)streams.size() * 4 + 4 * 4;
    put32(file, 44, directory_bytes);
    put32(file, 48, 0);
    put32(file, 52, 8); // block map

    // Streams 1,3,4,5 live in blocks 3,4,5,6.
    std::memcpy(file.data() + 3 * bs, info.data(), info.size());
    std::memcpy(file.data() + 4 * bs, dbi.data(), dbi.size());
    std::memcpy(file.data() + 5 * bs, symbols.data(), symbols.size());
    std::memcpy(file.data() + 6 * bs, sections.data(), sections.size());

    std::vector<uint8_t> directory(directory_bytes, 0);
    put32(directory, 0, (uint32_t)streams.size());
    size_t pos = 4;
    for (const auto &stream : streams) {
        put32(directory, pos, (uint32_t)stream.size());
        pos += 4;
    }
    for (uint32_t block : {3u, 4u, 5u, 6u}) {
        put32(directory, pos, block);
        pos += 4;
    }
    std::memcpy(file.data() + 7 * bs, directory.data(), directory.size());
    put32(file, 8 * bs, 7); // directory block list
    return file;
}

static std::vector<uint8_t> make_xbe_rsds(const uint8_t guid[16], uint32_t age)
{
    std::vector<uint8_t> xbe(128, 0);
    const size_t off = 16;
    std::memcpy(xbe.data() + off, "RSDS", 4);
    std::memcpy(xbe.data() + off + 4, guid, 16);
    put32(xbe, off + 20, age);
    const char path[] = "C:\\build\\Test.pdb";
    std::memcpy(xbe.data() + off + 24, path, sizeof(path));
    return xbe;
}

int main()
{
    const uint8_t guid[16] = {
        0xca,0xf5,0x75,0xbb,0x73,0x2e,0x29,0x42,
        0x9f,0x5e,0x90,0x4a,0xc2,0x86,0xd4,0xc8
    };
    XemuXbeLabels::Database db;
    db.image_base = 0x00010000;
    XemuXbeLabels::SectionInfo text;
    text.name = ".text";
    text.virtual_address = 0x00011000;
    text.virtual_size = 0x200;
    text.raw_size = 0x200;
    db.sections.push_back(text);

    const auto pdb = make_pdb(guid, 5, 0x180);
    const auto xbe = make_xbe_rsds(guid, 5);
    std::vector<XemuXbeLabels::Label> labels;
    XemuPdbLabels::Status status;
    std::string error;
    if (!XemuPdbLabels::ParseAndResolve(pdb, xbe, db, labels, status, error)) {
        std::fprintf(stderr, "FAIL: exact PDB rejected: %s\n", error.c_str());
        return 1;
    }
    if (!status.guid_match || !status.age_match || !status.layout_match ||
        status.public_symbols != 2 || status.resolved_labels != 2) {
        return fail("exact PDB status");
    }
    const auto function = std::find_if(labels.begin(), labels.end(), [](const auto &l) {
        return l.name == "cThing::Test";
    });
    if (function == labels.end() || function->virtual_address != 0x00011020 ||
        function->type != XemuXbeLabels::Type::Function ||
        function->source != XemuXbeLabels::Source::Pdb ||
        function->confidence != XemuXbeLabels::Confidence::Exact) {
        return fail("PDB function label");
    }
    const auto data = std::find_if(labels.begin(), labels.end(), [](const auto &l) {
        return l.name == "g_Value";
    });
    if (data == labels.end() || data->virtual_address != 0x00011040 ||
        data->type != XemuXbeLabels::Type::Symbol) {
        return fail("PDB data label");
    }

    auto older_xbe = make_xbe_rsds(guid, 6);
    labels.clear();
    status = {};
    error.clear();
    if (XemuPdbLabels::ParseAndResolve(pdb, older_xbe, db, labels, status, error) ||
        !status.guid_match || status.age_match || !labels.empty()) {
        return fail("PDB age mismatch rejection");
    }

    auto wrong_guid = make_xbe_rsds(guid, 5);
    wrong_guid[20] ^= 0x80;
    labels.clear();
    status = {};
    error.clear();
    if (XemuPdbLabels::ParseAndResolve(pdb, wrong_guid, db, labels, status, error) ||
        status.guid_match || !labels.empty()) {
        return fail("PDB GUID mismatch rejection");
    }

    XemuXbeLabels::Database small = db;
    small.sections[0].virtual_size = 0x100;
    small.sections[0].raw_size = 0x100;
    labels.clear();
    status = {};
    error.clear();
    if (XemuPdbLabels::ParseAndResolve(pdb, xbe, small, labels, status, error) ||
        status.layout_match || !labels.empty()) {
        return fail("PDB section-layout mismatch rejection");
    }

    XemuPdbLabels::Identity identity;
    if (!XemuPdbLabels::ExtractXbeIdentity(xbe, identity) ||
        identity.age != 5 || identity.path != "C:\\build\\Test.pdb" ||
        XemuPdbLabels::FormatGuid(identity.guid) !=
            "BB75F5CA-2E73-4229-9F5E-904AC286D4C8") {
        return fail("XBE RSDS identity extraction");
    }

    std::puts("PASS: Microsoft PDB GUID/Age/public-symbol importer golden cases");
    return 0;
}
