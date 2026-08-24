//
// xemu RAW Cheat Engine - local XDK symbol/signature importer
//
// This parser consumes Microsoft COFF .lib/.obj files only from a user's local
// Labels/XDK workspace.  It stores symbol names plus normalized fingerprints in
// Labels/Cache; it never persists the original executable bytes.
//

#include "xdk-labels.hh"
#include "binary-utils.hh"
#include "label-symbol-utils.hh"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

namespace XemuXdkLabels {
namespace {

using XemuDebugBinaryUtils::read_le16;
using XemuDebugBinaryUtils::read_le32;
using XemuDebugBinaryUtils::range_inside;
using XemuLabelSymbolUtils::lower_ascii;
using XemuLabelSymbolUtils::upper_ascii;

namespace fs = std::filesystem;

constexpr uint16_t kI386Machine = 0x014cu;
constexpr uint32_t kCoffCodeSection = 0x00000020u;
constexpr uint8_t kStorageExternal = 2;
constexpr uint8_t kStorageStatic = 3;
constexpr uint16_t kDerivedFunction = 0x0020u;
constexpr size_t kAnchorSize = 8;
constexpr uint32_t kMinExactFunctionSize = 16;
constexpr uint32_t kMaxFunctionSize = 64u * 1024u;
constexpr uint32_t kMaxSignatures = 200000u;
constexpr char kCacheMagic[8] = {'X','D','K','I','D','X','1','\0'};
constexpr uint32_t kCacheVersion = 1;

struct RelocMask {
    uint32_t offset = 0;
    uint8_t width = 0;

    bool operator==(const RelocMask &other) const
    {
        return offset == other.offset && width == other.width;
    }
};

struct Signature {
    std::string library;
    std::string name;
    uint32_t size = 0;
    uint32_t anchor_offset = 0;
    uint64_t anchor_hash = 0;
    uint64_t hash_a = 0;
    uint64_t hash_b = 0;
    std::vector<RelocMask> relocs;
};

struct Index {
    uint16_t build = 0;
    std::vector<Signature> signatures;
};

struct CoffSection {
    std::string name;
    uint32_t raw_size = 0;
    uint32_t raw_offset = 0;
    uint32_t reloc_offset = 0;
    uint16_t reloc_count = 0;
    uint32_t characteristics = 0;
};

struct CoffSymbol {
    std::string name;
    uint32_t value = 0;
    int16_t section_number = 0;
    uint16_t type = 0;
    uint8_t storage_class = 0;
    std::vector<std::array<uint8_t, 18>> aux;
};

struct CandidateFunction {
    std::string name;
    uint32_t value = 0;
    uint32_t size = 0;
    size_t section_index = 0;
};

static uint64_t read_le64(const uint8_t *p)
{
    return (uint64_t)read_le32(p) | ((uint64_t)read_le32(p + 4) << 32);
}

static void write_le16(std::ostream &out, uint16_t value)
{
    const uint8_t b[2] = {(uint8_t)value, (uint8_t)(value >> 8)};
    out.write(reinterpret_cast<const char *>(b), sizeof(b));
}

static void write_le32(std::ostream &out, uint32_t value)
{
    const uint8_t b[4] = {
        (uint8_t)value, (uint8_t)(value >> 8),
        (uint8_t)(value >> 16), (uint8_t)(value >> 24)};
    out.write(reinterpret_cast<const char *>(b), sizeof(b));
}

static void write_le64(std::ostream &out, uint64_t value)
{
    write_le32(out, (uint32_t)value);
    write_le32(out, (uint32_t)(value >> 32));
}

static uint64_t fnv1a64(const uint8_t *data, size_t size, uint64_t seed)
{
    uint64_t hash = seed;
    for (size_t i = 0; i < size; ++i) {
        hash ^= data[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

static bool masked_byte(const Signature &sig, uint32_t position)
{
    for (const RelocMask &mask : sig.relocs) {
        if (position >= mask.offset &&
            position < mask.offset + mask.width) {
            return true;
        }
    }
    return false;
}

static std::pair<uint64_t, uint64_t>
normalized_hash(const uint8_t *data, const Signature &sig)
{
    uint64_t a = 1469598103934665603ull;
    uint64_t b = 1099511628211ull ^ 0x9E3779B97F4A7C15ull;
    for (uint32_t i = 0; i < sig.size; ++i) {
        const uint8_t byte = masked_byte(sig, i) ? 0 : data[i];
        a ^= byte;
        a *= 1099511628211ull;
        b ^= (uint64_t)(byte + 0x9Du);
        b *= 14029467366897019727ull;
        b ^= b >> 31;
    }
    return {a, b};
}

static std::pair<uint64_t, uint64_t>
normalized_hash(const std::vector<uint8_t> &bytes,
                const std::vector<RelocMask> &relocs)
{
    Signature sig;
    sig.size = (uint32_t)bytes.size();
    sig.relocs = relocs;
    return normalized_hash(bytes.data(), sig);
}

static std::string clean_symbol_name(std::string name)
{
    if (name.rfind("__imp_", 0) == 0) {
        return {};
    }
    if (!name.empty() && (name[0] == '_' || name[0] == '@')) {
        name.erase(name.begin());
    }
    const size_t at = name.rfind('@');
    if (at != std::string::npos && at + 1 < name.size()) {
        bool digits = true;
        for (size_t i = at + 1; i < name.size(); ++i) {
            digits &= std::isdigit((unsigned char)name[i]) != 0;
        }
        if (digits) {
            name.resize(at);
        }
    }
    return name;
}

static std::string library_filename(const std::string &xbe_name)
{
    const std::string name = upper_ascii(xbe_name);
    if (name == "XGRAPHC") return "xgraphics.lib";
    if (name == "XAPILIB") return "xapilib.lib";
    if (name == "XBOXKRNL") return "xboxkrnl.lib";
    if (name == "LIBCPMT") return "libcpmt.lib";
    if (name == "LIBCMT") return "libcmt.lib";
    if (name == "D3D8") return "d3d8.lib";
    if (name == "D3DX8") return "d3dx8.lib";
    if (name == "DSOUND") return "dsound.lib";
    if (name == "DMUSIC") return "dmusic.lib";
    if (name == "XACTENG") return "xacteng.lib";
    if (name == "XONLINE") return "xonline.lib";
    if (name == "XNET") return "xnet.lib";
    if (name == "XVOICE") return "xvoice.lib";
    return lower_ascii(xbe_name) + ".lib";
}

static uint16_t dominant_build(const XemuXbeLabels::Database &database)
{
    std::map<uint16_t, size_t> counts;
    for (const auto &lib : database.libraries) {
        if (lib.build != 0) {
            ++counts[lib.build];
        }
    }
    uint16_t best = 0;
    size_t best_count = 0;
    for (const auto &it : counts) {
        if (it.second > best_count) {
            best = it.first;
            best_count = it.second;
        }
    }
    return best;
}

static std::vector<std::string>
required_library_names(const XemuXbeLabels::Database &database,
                       uint16_t build)
{
    std::vector<std::string> out;
    for (const auto &lib : database.libraries) {
        if (lib.build != build) {
            continue;
        }
        const std::string filename = library_filename(lib.name);
        if (std::find(out.begin(), out.end(), filename) == out.end()) {
            out.push_back(filename);
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

static std::string xbe_name_for_filename(const XemuXbeLabels::Database &database,
                                         uint16_t build,
                                         const std::string &filename)
{
    for (const auto &lib : database.libraries) {
        if (lib.build == build &&
            lower_ascii(library_filename(lib.name)) == lower_ascii(filename)) {
            return upper_ascii(lib.name);
        }
    }
    return upper_ascii(filename);
}

static bool read_file(const fs::path &path, std::vector<uint8_t> &data)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    in.seekg(0, std::ios::end);
    const std::streamoff length = in.tellg();
    if (length <= 0 || length > (std::streamoff)(512ull * 1024ull * 1024ull)) {
        return false;
    }
    in.seekg(0, std::ios::beg);
    data.resize((size_t)length);
    return (bool)in.read(reinterpret_cast<char *>(data.data()), length);
}

static std::vector<std::pair<std::string, fs::path>>
find_libraries(const fs::path &xdk_root, uint16_t build,
               const XemuXbeLabels::Database &database,
               size_t &required_count, std::string &source_directory)
{
    const std::vector<std::string> required =
        required_library_names(database, build);
    required_count = required.size();
    std::set<std::string> wanted;
    for (const std::string &name : required) {
        wanted.insert(lower_ascii(name));
    }

    fs::path scan_root = xdk_root / std::to_string(build);
    std::error_code ec;
    if (!fs::is_directory(scan_root, ec)) {
        scan_root = xdk_root;
    }
    source_directory = scan_root.string();

    std::map<std::string, fs::path> found;
    if (!fs::is_directory(scan_root, ec)) {
        return {};
    }

    fs::recursive_directory_iterator it(
        scan_root, fs::directory_options::skip_permission_denied, ec);
    const fs::recursive_directory_iterator end;
    while (!ec && it != end) {
        if (it->is_regular_file(ec)) {
            const std::string filename = lower_ascii(it->path().filename().string());
            if (wanted.count(filename) != 0 && found.count(filename) == 0) {
                found.emplace(filename, it->path());
            }
        }
        it.increment(ec);
    }

    std::vector<std::pair<std::string, fs::path>> result;
    for (const auto &entry : found) {
        result.emplace_back(
            xbe_name_for_filename(database, build, entry.first), entry.second);
    }
    return result;
}

static std::string coff_string(const std::vector<uint8_t> &strings,
                               uint32_t offset)
{
    if (offset < 4 || offset >= strings.size()) {
        return {};
    }
    size_t end = offset;
    while (end < strings.size() && strings[end] != 0) {
        ++end;
    }
    return std::string(reinterpret_cast<const char *>(strings.data() + offset),
                       end - offset);
}

static bool parse_coff(const uint8_t *data, size_t size,
                       std::vector<CoffSection> &sections,
                       std::vector<CoffSymbol> &symbols)
{
    sections.clear();
    symbols.clear();
    if (size < 20 || read_le16(data) != kI386Machine) {
        return false;
    }
    const uint16_t section_count = read_le16(data + 2);
    const uint32_t symbol_offset = read_le32(data + 8);
    const uint32_t symbol_count = read_le32(data + 12);
    const uint16_t optional_size = read_le16(data + 16);
    if (section_count == 0 || section_count > 4096) {
        return false;
    }
    const uint64_t section_headers = 20ull + optional_size;
    if (!range_inside(section_headers, (uint64_t)section_count * 40, size)) {
        return false;
    }

    std::vector<uint8_t> string_table;
    if (symbol_offset != 0 && symbol_count != 0 &&
        range_inside(symbol_offset, (uint64_t)symbol_count * 18 + 4, size)) {
        const uint64_t strings_at =
            (uint64_t)symbol_offset + (uint64_t)symbol_count * 18;
        const uint32_t string_size = read_le32(data + strings_at);
        if (string_size >= 4 && range_inside(strings_at, string_size, size)) {
            string_table.assign(data + strings_at,
                                data + strings_at + string_size);
        }
    }

    sections.reserve(section_count);
    for (uint16_t i = 0; i < section_count; ++i) {
        const uint8_t *sh = data + section_headers + (uint64_t)i * 40;
        CoffSection section;
        char name_buffer[9] = {};
        std::memcpy(name_buffer, sh, 8);
        if (name_buffer[0] == '/' &&
            std::isdigit((unsigned char)name_buffer[1])) {
            section.name = coff_string(
                string_table, (uint32_t)std::strtoul(name_buffer + 1,
                                                     nullptr, 10));
        } else {
            section.name = name_buffer;
        }
        section.raw_size = read_le32(sh + 16);
        section.raw_offset = read_le32(sh + 20);
        section.reloc_offset = read_le32(sh + 24);
        section.reloc_count = read_le16(sh + 32);
        section.characteristics = read_le32(sh + 36);
        if (section.raw_size != 0 &&
            !range_inside(section.raw_offset, section.raw_size, size)) {
            return false;
        }
        if (section.reloc_count != 0 &&
            !range_inside(section.reloc_offset,
                          (uint64_t)section.reloc_count * 10, size)) {
            return false;
        }
        sections.push_back(std::move(section));
    }

    if (symbol_offset == 0 || symbol_count == 0 ||
        !range_inside(symbol_offset, (uint64_t)symbol_count * 18, size)) {
        return true;
    }

    uint32_t index = 0;
    while (index < symbol_count) {
        const uint8_t *entry = data + symbol_offset + (uint64_t)index * 18;
        CoffSymbol symbol;
        if (read_le32(entry) == 0) {
            symbol.name = coff_string(string_table, read_le32(entry + 4));
        } else {
            char name_buffer[9] = {};
            std::memcpy(name_buffer, entry, 8);
            symbol.name = name_buffer;
        }
        symbol.value = read_le32(entry + 8);
        symbol.section_number = (int16_t)read_le16(entry + 12);
        symbol.type = read_le16(entry + 14);
        symbol.storage_class = entry[16];
        const uint8_t aux_count = entry[17];
        if ((uint64_t)index + aux_count >= symbol_count) {
            return false;
        }
        for (uint8_t a = 0; a < aux_count; ++a) {
            std::array<uint8_t, 18> aux{};
            std::memcpy(aux.data(), entry + (uint64_t)(a + 1) * 18, 18);
            symbol.aux.push_back(aux);
        }
        symbols.push_back(std::move(symbol));
        index += 1u + aux_count;
    }
    return true;
}

static uint8_t relocation_width(uint16_t type)
{
    switch (type) {
    case 0x0000: return 0; // ABSOLUTE: no relocation applied
    case 0x0001: return 2; // DIR16
    case 0x0002: return 2; // REL16
    case 0x0006: return 4; // DIR32
    case 0x0007: return 4; // DIR32NB
    case 0x0009: return 2; // SEG12
    case 0x000A: return 2; // SECTION
    case 0x000B: return 4; // SECREL
    case 0x000C: return 4; // TOKEN
    case 0x000D: return 1; // SECREL7
    case 0x0014: return 4; // REL32
    default: return 0xFF;
    }
}

static bool build_signature(const uint8_t *object, size_t object_size,
                            const CoffSection &section,
                            const CandidateFunction &function,
                            const std::string &library,
                            Signature &signature)
{
    if (function.size < kMinExactFunctionSize || function.size > kMaxFunctionSize ||
        function.value > section.raw_size ||
        function.size > section.raw_size - function.value ||
        !range_inside((uint64_t)section.raw_offset + function.value,
                      function.size, object_size)) {
        return false;
    }

    signature = {};
    signature.library = library;
    signature.name = clean_symbol_name(function.name);
    signature.size = function.size;
    if (signature.name.empty()) {
        return false;
    }

    if (section.reloc_count != 0) {
        for (uint16_t i = 0; i < section.reloc_count; ++i) {
            const uint8_t *rel =
                object + section.reloc_offset + (uint64_t)i * 10;
            const uint32_t offset = read_le32(rel);
            if (offset < function.value ||
                offset >= function.value + function.size) {
                continue;
            }
            const uint8_t width = relocation_width(read_le16(rel + 8));
            if (width == 0) {
                continue;
            }
            if (width == 0xFF) {
                return false; // unknown relocation: cannot claim exact match
            }
            const uint32_t relative = offset - function.value;
            if (relative > function.size || width > function.size - relative) {
                return false;
            }
            signature.relocs.push_back({relative, width});
        }
    }
    std::sort(signature.relocs.begin(), signature.relocs.end(),
              [](const RelocMask &a, const RelocMask &b) {
                  if (a.offset != b.offset) return a.offset < b.offset;
                  return a.width < b.width;
              });
    signature.relocs.erase(
        std::unique(signature.relocs.begin(), signature.relocs.end()),
        signature.relocs.end());

    std::vector<uint8_t> masked(function.size, 0);
    for (const RelocMask &mask : signature.relocs) {
        for (uint32_t i = 0; i < mask.width; ++i) {
            masked[mask.offset + i] = 1;
        }
    }

    size_t best_start = 0;
    size_t best_length = 0;
    size_t run_start = 0;
    size_t run_length = 0;
    for (size_t i = 0; i <= masked.size(); ++i) {
        const bool blocked = i == masked.size() || masked[i] != 0;
        if (!blocked) {
            if (run_length == 0) run_start = i;
            ++run_length;
        } else {
            if (run_length >= kAnchorSize && run_length > best_length) {
                best_start = run_start;
                best_length = run_length;
            }
            run_length = 0;
        }
    }
    if (best_length < kAnchorSize) {
        return false;
    }
    signature.anchor_offset = (uint32_t)(
        best_start + (best_length - kAnchorSize) / 2);

    const uint8_t *bytes = object + section.raw_offset + function.value;
    signature.anchor_hash = fnv1a64(
        bytes + signature.anchor_offset, kAnchorSize,
        1469598103934665603ull);
    const std::vector<uint8_t> function_bytes(bytes, bytes + function.size);
    const auto hash = normalized_hash(function_bytes, signature.relocs);
    signature.hash_a = hash.first;
    signature.hash_b = hash.second;
    return true;
}

static void add_object_signatures(const uint8_t *object, size_t object_size,
                                  const std::string &library,
                                  std::vector<Signature> &out)
{
    std::vector<CoffSection> sections;
    std::vector<CoffSymbol> symbols;
    if (!parse_coff(object, object_size, sections, symbols)) {
        return;
    }

    std::map<size_t, std::vector<CandidateFunction>> by_section;
    for (const CoffSymbol &symbol : symbols) {
        if (symbol.section_number <= 0 ||
            (size_t)symbol.section_number > sections.size() ||
            (symbol.type & kDerivedFunction) == 0 ||
            (symbol.storage_class != kStorageExternal &&
             symbol.storage_class != kStorageStatic) ||
            symbol.name.empty() || symbol.name[0] == '$' ||
            symbol.name[0] == '.') {
            continue;
        }
        const size_t section_index = (size_t)symbol.section_number - 1;
        const CoffSection &section = sections[section_index];
        if ((section.characteristics & kCoffCodeSection) == 0) {
            continue;
        }
        CandidateFunction function;
        function.name = symbol.name;
        function.value = symbol.value;
        function.section_index = section_index;
        if (!symbol.aux.empty()) {
            const uint32_t total_size = read_le32(symbol.aux[0].data() + 4);
            if (total_size != 0 && symbol.value <= section.raw_size &&
                total_size <= section.raw_size - symbol.value) {
                function.size = total_size;
            }
        }
        by_section[section_index].push_back(std::move(function));
    }

    for (auto &entry : by_section) {
        const CoffSection &section = sections[entry.first];
        auto &functions = entry.second;
        std::sort(functions.begin(), functions.end(),
                  [](const CandidateFunction &a,
                     const CandidateFunction &b) {
                      if (a.value != b.value) return a.value < b.value;
                      return a.name < b.name;
                  });
        for (size_t i = 0; i < functions.size(); ++i) {
            CandidateFunction function = functions[i];
            if (function.size == 0) {
                uint32_t end = section.raw_size;
                for (size_t j = i + 1; j < functions.size(); ++j) {
                    if (functions[j].value > function.value) {
                        end = functions[j].value;
                        break;
                    }
                }
                if (end > function.value) {
                    function.size = end - function.value;
                }
            }
            Signature signature;
            if (build_signature(object, object_size, section, function,
                                library, signature)) {
                out.push_back(std::move(signature));
                if (out.size() >= kMaxSignatures) {
                    return;
                }
            }
        }
    }
}

static bool parse_archive(const std::vector<uint8_t> &archive,
                          const std::string &library,
                          std::vector<Signature> &out)
{
    static const char magic[] = "!<arch>\n";
    if (archive.size() < 8 ||
        std::memcmp(archive.data(), magic, 8) != 0) {
        return false;
    }

    std::vector<uint8_t> long_names;
    uint64_t offset = 8;
    while (range_inside(offset, 60, archive.size())) {
        const uint8_t *header = archive.data() + offset;
        if (header[58] != '`' || header[59] != '\n') {
            break;
        }
        char size_buffer[11] = {};
        std::memcpy(size_buffer, header + 48, 10);
        char *end = nullptr;
        const unsigned long member_size = std::strtoul(size_buffer, &end, 10);
        if (end == size_buffer ||
            !range_inside(offset + 60, member_size, archive.size())) {
            break;
        }
        const uint8_t *member = archive.data() + offset + 60;

        char raw_name[17] = {};
        std::memcpy(raw_name, header, 16);
        std::string name(raw_name);
        while (!name.empty() && name.back() == ' ') name.pop_back();
        if (name == "//") {
            long_names.assign(member, member + member_size);
        } else if (name != "/" && name != "/SYM64/") {
            // The actual member name is not needed to build fingerprints.
            add_object_signatures(member, member_size, library, out);
        }

        offset += 60 + member_size;
        if (member_size & 1u) ++offset;
        if (out.size() >= kMaxSignatures) break;
    }
    return true;
}

static std::string signature_key(const Signature &sig)
{
    std::ostringstream out;
    out << sig.size << ':' << std::hex << sig.hash_a << ':' << sig.hash_b;
    for (const RelocMask &mask : sig.relocs) {
        out << ':' << mask.offset << '/' << (unsigned)mask.width;
    }
    return out.str();
}

static void remove_ambiguous_signatures(std::vector<Signature> &signatures)
{
    std::map<std::string, std::vector<size_t>> groups;
    for (size_t i = 0; i < signatures.size(); ++i) {
        groups[signature_key(signatures[i])].push_back(i);
    }

    std::vector<Signature> clean;
    clean.reserve(signatures.size());
    for (const auto &group : groups) {
        std::set<std::string> names;
        for (size_t index : group.second) {
            names.insert(signatures[index].name);
        }
        if (names.size() != 1) {
            continue;
        }
        // Identical implementations with the same name can occur in multiple
        // libraries.  Keep one deterministic representative; the symbol name
        // remains exact even if library ownership is shared.
        size_t best = group.second.front();
        for (size_t index : group.second) {
            if (signatures[index].library < signatures[best].library) {
                best = index;
            }
        }
        clean.push_back(std::move(signatures[best]));
    }
    signatures = std::move(clean);
    std::sort(signatures.begin(), signatures.end(),
              [](const Signature &a, const Signature &b) {
                  if (a.library != b.library) return a.library < b.library;
                  if (a.name != b.name) return a.name < b.name;
                  if (a.size != b.size) return a.size < b.size;
                  if (a.hash_a != b.hash_a) return a.hash_a < b.hash_a;
                  return a.hash_b < b.hash_b;
              });
}

static bool build_index(const std::vector<std::pair<std::string, fs::path>> &libs,
                        uint16_t build, Index &index, std::string &error)
{
    index = {};
    index.build = build;
    error.clear();
    for (const auto &lib : libs) {
        std::vector<uint8_t> bytes;
        if (!read_file(lib.second, bytes)) {
            error = "Could not read XDK library: " + lib.second.string();
            return false;
        }
        parse_archive(bytes, lib.first, index.signatures);
        if (index.signatures.size() >= kMaxSignatures) {
            error = "XDK signature safety limit reached.";
            return false;
        }
    }
    remove_ambiguous_signatures(index.signatures);
    if (index.signatures.empty()) {
        error = "No usable x86 function signatures were found in the XDK libraries.";
        return false;
    }
    return true;
}

static bool write_string(std::ostream &out, const std::string &value)
{
    if (value.size() > std::numeric_limits<uint16_t>::max()) {
        return false;
    }
    write_le16(out, (uint16_t)value.size());
    out.write(value.data(), value.size());
    return (bool)out;
}

static bool read_string(std::istream &in, std::string &value)
{
    uint8_t length_bytes[2] = {};
    if (!in.read(reinterpret_cast<char *>(length_bytes), 2)) return false;
    const uint16_t length = read_le16(length_bytes);
    value.resize(length);
    return length == 0 || (bool)in.read(value.data(), length);
}

static bool save_cache(const fs::path &path, const Index &index,
                       std::string &error)
{
    error.clear();
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    if (ec) {
        error = "Could not create Labels/Cache directory.";
        return false;
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        error = "Could not create XDK cache: " + path.string();
        return false;
    }
    out.write(kCacheMagic, sizeof(kCacheMagic));
    write_le32(out, kCacheVersion);
    write_le16(out, index.build);
    write_le32(out, (uint32_t)index.signatures.size());
    for (const Signature &sig : index.signatures) {
        write_le32(out, sig.size);
        write_le32(out, sig.anchor_offset);
        write_le64(out, sig.anchor_hash);
        write_le64(out, sig.hash_a);
        write_le64(out, sig.hash_b);
        if (sig.relocs.size() > std::numeric_limits<uint16_t>::max()) {
            error = "XDK cache relocation count overflow.";
            return false;
        }
        write_le16(out, (uint16_t)sig.relocs.size());
        for (const RelocMask &mask : sig.relocs) {
            write_le32(out, mask.offset);
            out.put((char)mask.width);
        }
        if (!write_string(out, sig.library) || !write_string(out, sig.name)) {
            error = "XDK cache symbol name is too long.";
            return false;
        }
    }
    if (!out) {
        error = "Failed while writing XDK cache: " + path.string();
        return false;
    }
    return true;
}

static bool load_cache(const fs::path &path, uint16_t expected_build,
                       Index &index, std::string &error)
{
    index = {};
    error.clear();
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "XDK cache is not available.";
        return false;
    }
    uint8_t header[18] = {};
    if (!in.read(reinterpret_cast<char *>(header), sizeof(header)) ||
        std::memcmp(header, kCacheMagic, sizeof(kCacheMagic)) != 0) {
        error = "XDK cache has an invalid header.";
        return false;
    }
    const uint32_t version = read_le32(header + 8);
    const uint16_t build = read_le16(header + 12);
    const uint32_t count = read_le32(header + 14);
    if (version != kCacheVersion || build != expected_build ||
        count == 0 || count > kMaxSignatures) {
        error = "XDK cache version/build does not match the current title.";
        return false;
    }
    index.build = build;
    index.signatures.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        uint8_t fixed[34] = {};
        if (!in.read(reinterpret_cast<char *>(fixed), sizeof(fixed))) {
            error = "XDK cache is truncated.";
            return false;
        }
        Signature sig;
        sig.size = read_le32(fixed);
        sig.anchor_offset = read_le32(fixed + 4);
        sig.anchor_hash = read_le64(fixed + 8);
        sig.hash_a = read_le64(fixed + 16);
        sig.hash_b = read_le64(fixed + 24);
        const uint16_t reloc_count = read_le16(fixed + 32);
        if (sig.size < kMinExactFunctionSize || sig.size > kMaxFunctionSize ||
            sig.anchor_offset > sig.size - kAnchorSize) {
            error = "XDK cache contains an invalid function signature.";
            return false;
        }
        for (uint16_t r = 0; r < reloc_count; ++r) {
            uint8_t reloc[5] = {};
            if (!in.read(reinterpret_cast<char *>(reloc), sizeof(reloc))) {
                error = "XDK cache relocation table is truncated.";
                return false;
            }
            RelocMask mask{read_le32(reloc), reloc[4]};
            if (mask.width == 0 || mask.offset > sig.size ||
                mask.width > sig.size - mask.offset) {
                error = "XDK cache contains an invalid relocation mask.";
                return false;
            }
            sig.relocs.push_back(mask);
        }
        if (!read_string(in, sig.library) || !read_string(in, sig.name) ||
            sig.library.empty() || sig.name.empty()) {
            error = "XDK cache symbol table is truncated.";
            return false;
        }
        index.signatures.push_back(std::move(sig));
    }
    return true;
}

static bool library_allowed_in_section(const std::string &library,
                                       const std::string &section)
{
    const std::string lib = upper_ascii(library);
    const std::string sec = upper_ascii(section);
    if (lib == "D3D8") return sec == "D3D" || sec == ".TEXT";
    if (lib == "D3DX8") return sec == "D3DX" || sec == ".TEXT";
    if (lib == "XGRAPHC") return sec == "XGRPH" || sec == ".TEXT";
    if (lib == "DSOUND") return sec == "DSOUND" || sec == ".TEXT";
    if (lib == "XAPILIB") return sec == ".TEXT" || sec == "XPP";
    if (lib == "LIBCMT" || lib == "LIBCPMT") return sec == ".TEXT";
    return sec == ".TEXT";
}

static void attach_location(const XemuXbeLabels::Database &database,
                            XemuXbeLabels::Label &label)
{
    for (const auto &section : database.sections) {
        const uint64_t span =
            std::max<uint32_t>(section.virtual_size, section.raw_size);
        if ((uint64_t)label.virtual_address >= section.virtual_address &&
            (uint64_t)label.virtual_address <
                (uint64_t)section.virtual_address + span) {
            label.section_name = section.name;
            label.section_offset = label.virtual_address -
                                   section.virtual_address;
            label.has_section_location = true;
            return;
        }
    }
}

static bool match_index(const Index &index,
                        const std::vector<uint8_t> &xbe_file,
                        const XemuXbeLabels::Database &database,
                        std::vector<XemuXbeLabels::Label> &labels,
                        std::string &error)
{
    labels.clear();
    error.clear();
    if (index.signatures.empty()) {
        error = "XDK index contains no signatures.";
        return false;
    }

    std::unordered_multimap<uint64_t, size_t> anchors;
    anchors.reserve(index.signatures.size() * 2 + 1);
    for (size_t i = 0; i < index.signatures.size(); ++i) {
        anchors.emplace(index.signatures[i].anchor_hash, i);
    }

    std::vector<std::set<uint32_t>> matches(index.signatures.size());
    for (const auto &section : database.sections) {
        if (section.raw_size < kAnchorSize ||
            !range_inside(section.raw_address, section.raw_size,
                          xbe_file.size())) {
            continue;
        }
        const uint8_t *bytes = xbe_file.data() + section.raw_address;
        for (uint32_t pos = 0; pos + kAnchorSize <= section.raw_size; ++pos) {
            const uint64_t anchor = fnv1a64(
                bytes + pos, kAnchorSize, 1469598103934665603ull);
            const auto range = anchors.equal_range(anchor);
            for (auto it = range.first; it != range.second; ++it) {
                const Signature &sig = index.signatures[it->second];
                if (!library_allowed_in_section(sig.library, section.name) ||
                    pos < sig.anchor_offset) {
                    continue;
                }
                const uint32_t start = pos - sig.anchor_offset;
                if (start > section.raw_size ||
                    sig.size > section.raw_size - start) {
                    continue;
                }
                const auto hash = normalized_hash(bytes + start, sig);
                if (hash.first == sig.hash_a && hash.second == sig.hash_b) {
                    matches[it->second].insert(section.virtual_address + start);
                }
            }
        }
    }

    // Exact means both sides are unique: one symbol identity maps to exactly
    // one XBE address, and one XBE address maps to exactly one symbol name.
    std::map<uint32_t, std::set<std::string>> names_by_address;
    for (size_t i = 0; i < matches.size(); ++i) {
        if (matches[i].size() == 1) {
            names_by_address[*matches[i].begin()].insert(index.signatures[i].name);
        }
    }

    std::set<std::pair<uint32_t, std::string>> emitted;
    for (size_t i = 0; i < matches.size(); ++i) {
        if (matches[i].size() != 1) {
            continue;
        }
        const uint32_t address = *matches[i].begin();
        if (names_by_address[address].size() != 1) {
            continue;
        }
        const Signature &sig = index.signatures[i];
        if (!emitted.emplace(address, sig.name).second) {
            continue;
        }
        XemuXbeLabels::Label label;
        label.virtual_address = address;
        label.type = XemuXbeLabels::Type::Function;
        label.name = sig.library + "::" + sig.name;
        label.source = XemuXbeLabels::Source::Xdk;
        label.confidence = XemuXbeLabels::Confidence::Exact;
        attach_location(database, label);
        labels.push_back(std::move(label));
    }
    std::sort(labels.begin(), labels.end(),
              [](const auto &a, const auto &b) {
                  if (a.virtual_address != b.virtual_address) {
                      return a.virtual_address < b.virtual_address;
                  }
                  return a.name < b.name;
              });
    return true;
}

static fs::path cache_path_for(const fs::path &cache_root, uint16_t build)
{
    return cache_root / ("XDK-" + std::to_string(build) + ".xdkidx");
}

} // namespace

bool Process(const std::string &xdk_root, const std::string &cache_root,
             const std::vector<uint8_t> &xbe_file,
             const XemuXbeLabels::Database &xbe_database,
             bool rebuild_cache,
             std::vector<XemuXbeLabels::Label> &labels,
             Status &status, std::string &error)
{
    labels.clear();
    status = {};
    error.clear();

    status.build = dominant_build(xbe_database);
    if (status.build == 0) {
        status.message = "Current XBE does not expose a usable XDK library build.";
        return true;
    }

    const fs::path xdk_path(xdk_root);
    const fs::path cache_path = cache_path_for(fs::path(cache_root),
                                               status.build);
    status.cache_path = cache_path.string();
    std::error_code ec;
    status.cache_found = fs::is_regular_file(cache_path, ec);

    size_t required = 0;
    std::string source_directory;
    const auto libraries = find_libraries(xdk_path, status.build,
                                          xbe_database, required,
                                          source_directory);
    status.required_libraries = required;
    status.found_libraries = libraries.size();
    status.source_found = !libraries.empty();
    status.source_directory = source_directory;

    Index index;
    if (rebuild_cache) {
        if (libraries.empty()) {
            error = "No matching XDK " + std::to_string(status.build) +
                    " .lib files were found under Labels/XDK.";
            status.message = error;
            return false;
        }
        if (!build_index(libraries, status.build, index, error) ||
            !save_cache(cache_path, index, error)) {
            status.message = error;
            return false;
        }
        status.cache_found = true;
        status.cache_loaded = true;
        status.cache_rebuilt = true;
    } else if (status.cache_found) {
        if (!load_cache(cache_path, status.build, index, error)) {
            status.message = error + " Use BUILD / REFRESH XDK INDEX.";
            status.needs_build = true;
            return false;
        }
        status.cache_loaded = true;
    } else {
        status.needs_build = status.source_found;
        std::ostringstream msg;
        msg << "XDK " << status.build << ": " << status.found_libraries
            << "/" << status.required_libraries << " required libraries found";
        if (status.source_found) {
            msg << "; build the local fingerprint index to enable XDK labels.";
        } else {
            msg << "; no matching local XDK source was found.";
        }
        status.message = msg.str();
        return true;
    }

    status.signatures = index.signatures.size();
    if (!match_index(index, xbe_file, xbe_database, labels, error)) {
        status.message = error;
        return false;
    }
    status.exact_matches = labels.size();

    std::ostringstream msg;
    msg << "XDK " << status.build << ": " << status.exact_matches
        << " exact function label(s) from " << status.signatures
        << " local fingerprint(s); " << status.found_libraries << "/"
        << status.required_libraries << " matching libraries found";
    if (status.cache_rebuilt) msg << " (cache rebuilt).";
    else msg << " (cache loaded).";
    status.message = msg.str();
    return true;
}

} // namespace XemuXdkLabels
