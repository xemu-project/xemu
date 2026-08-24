//
// xemu Current Game Manager
//
// Copyright (C) 2026 xemu contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#include "current-game.hh"
#include "binary-utils.hh"
#include "../misc.hh"

#include "xemu-xbe.h"
#include "disc-block-io.h"
#include "cheat-engine-memory.h"
#include "xdvdfs-export-service.hh"

#include <glib.h>
#include <glib/gstdio.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <vector>
#include <cstring>

CurrentGameManager current_game_manager;

namespace {
using XemuDebugBinaryUtils::range_inside;

constexpr uint64_t kRefreshIntervalMs = 500;
constexpr size_t kHashChunkSize = 1024 * 1024;

static bool title_name_from_xbe(const struct xbe *xbe, std::string &result)
{
    result.clear();
    if (!xbe || !xbe->cert) {
        return false;
    }

    glong items_written = 0;
    gchar *name = g_utf16_to_utf8(
        reinterpret_cast<const gunichar2 *>(xbe->cert->m_title_name),
        40, nullptr, &items_written, nullptr);
    if (!name) {
        return false;
    }

    result.assign(name);
    g_free(name);
    return true;
}

static std::string sha256_headers(const struct xbe *xbe)
{
    if (!xbe || !xbe->headers || xbe->headers_len == 0) {
        return {};
    }

    gchar *sum = g_compute_checksum_for_data(
        G_CHECKSUM_SHA256,
        reinterpret_cast<const guchar *>(xbe->headers),
        xbe->headers_len);
    if (!sum) {
        return {};
    }

    std::string result(sum);
    g_free(sum);
    return result;
}

static bool read_disc_file(XemuDiscBlockHandle blk, uint64_t image_size,
                           uint64_t offset, uint64_t size,
                           std::vector<uint8_t> &buffer, std::string &error)
{
    constexpr uint64_t kMaxXbeLabelBytes = 128ull * 1024ull * 1024ull;
    error.clear();
    buffer.clear();
    if (blk == nullptr || !range_inside(offset, size, image_size)) {
        error = "default.xbe points outside the mounted disc image.";
        return false;
    }
    if (size == 0 || size > kMaxXbeLabelBytes) {
        error = "default.xbe is too large for the label scanner safety limit.";
        return false;
    }
    buffer.resize((size_t)size);
    uint64_t position = 0;
    while (position < size) {
        const size_t amount = (size_t)std::min<uint64_t>(
            kHashChunkSize, size - position);
        if (!xemu_disc_block_pread(blk, offset + position,
                                   buffer.data() + position, amount)) {
            buffer.clear();
            error = "Unable to read default.xbe for label analysis.";
            return false;
        }
        position += amount;
    }
    return true;
}

static std::string sha256_bytes(const std::vector<uint8_t> &bytes,
                                std::string &error)
{
    error.clear();
    if (bytes.empty()) {
        error = "default.xbe is empty; cannot calculate SHA-256.";
        return {};
    }
    gchar *sum = g_compute_checksum_for_data(
        G_CHECKSUM_SHA256, bytes.data(), bytes.size());
    if (sum == nullptr) {
        error = "Unable to calculate default.xbe SHA-256.";
        return {};
    }
    std::string result(sum);
    g_free(sum);
    return result;
}

template <typename Output>
static bool read_local_file(const std::string &path, uint64_t max_bytes,
                            Output &out, std::string &error)
{
    out.clear();
    error.clear();
    GStatBuf st = {};
    if (g_stat(path.c_str(), &st) != 0 || st.st_size < 0) {
        error = "Could not stat file: " + path;
        return false;
    }
    const uint64_t size = (uint64_t)st.st_size;
    if (size > max_bytes) {
        error = "File exceeds import size limit.";
        return false;
    }
    FILE *fp = g_fopen(path.c_str(), "rb");
    if (fp == nullptr) {
        error = "Could not open file: " + path;
        return false;
    }
    out.resize((size_t)size);
    const bool ok = size == 0 ||
        std::fread(out.data(), 1, out.size(), fp) == out.size();
    const bool close_ok = std::fclose(fp) == 0;
    if (!ok || !close_ok) {
        out.clear();
        error = "Could not read complete file: " + path;
        return false;
    }
    return true;
}

static bool read_local_binary_file(const std::string &path, uint64_t max_bytes,
                                   std::vector<uint8_t> &out,
                                   std::string &error)
{
    return read_local_file(path, max_bytes, out, error);
}

static bool read_local_text_file(const std::string &path, uint64_t max_bytes,
                                 std::string &out, std::string &error)
{
    return read_local_file(path, max_bytes, out, error);
}

}

bool CurrentGameManager::SameIdentity(const GameInfo &a, const GameInfo &b)
{
    if (a.valid != b.valid) {
        return false;
    }
    if (!a.valid) {
        return true;
    }
    return a.title_id == b.title_id &&
           a.header_sha256 == b.header_sha256 &&
           a.version == b.version &&
           a.disc_number == b.disc_number;
}

std::string CurrentGameManager::FormatTitleId(uint32_t title_id)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%08X", title_id);
    return buf;
}

std::string CurrentGameManager::FormatDatabaseGameId(uint32_t title_id)
{
    const unsigned char a = (unsigned char)((title_id >> 24) & 0xFFu);
    const unsigned char b = (unsigned char)((title_id >> 16) & 0xFFu);
    if (a >= 0x21 && a <= 0x7E && b >= 0x21 && b <= 0x7E) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%c%c%04X", a, b,
                      title_id & 0xFFFFu);
        return buf;
    }
    return FormatTitleId(title_id);
}

std::string CurrentGameManager::FormatRegion(uint32_t region)
{
    std::string out;
    auto append = [&](const char *name) {
        if (!out.empty()) {
            out += " + ";
        }
        out += name;
    };

    // Xbox XBE certificate region flags.
    if (region & 0x00000001u) append("North America");
    if (region & 0x00000002u) append("Japan");
    if (region & 0x00000004u) append("Rest of World");
    if (region & 0x80000000u) append("Manufacturing");

    if (out.empty()) {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "0x%08X", region);
        out = buf;
    }
    return out;
}

std::string CurrentGameManager::FormatByteSize(uint64_t bytes)
{
    char buf[64];
    if (bytes < 1024) {
        std::snprintf(buf, sizeof(buf), "%llu B",
                      (unsigned long long)bytes);
    } else if (bytes < 1024ull * 1024ull) {
        std::snprintf(buf, sizeof(buf), "%.2f KiB", bytes / 1024.0);
    } else if (bytes < 1024ull * 1024ull * 1024ull) {
        std::snprintf(buf, sizeof(buf), "%.2f MiB",
                      bytes / (1024.0 * 1024.0));
    } else {
        std::snprintf(buf, sizeof(buf), "%.2f GiB",
                      bytes / (1024.0 * 1024.0 * 1024.0));
    }
    return buf;
}

XemuLabelPacks::Identity CurrentGameManager::CurrentLabelIdentity() const
{
    XemuLabelPacks::Identity identity;
    identity.title_id = m_info.title_id;
    identity.game_id = FormatDatabaseGameId(m_info.title_id);
    identity.name = m_info.title_name;
    identity.header_sha256 = m_info.header_sha256;
    identity.xbe_sha256 = m_info.disc_xbe_sha256;
    return identity;
}

std::string CurrentGameManager::LabelRootDirectory() const
{
    char executable_dir[4096] = {};
    if (!xemu_cheat_get_executable_dir(executable_dir,
                                       sizeof(executable_dir))) {
        return "Labels";
    }
    gchar *path = g_build_filename(executable_dir, "Labels", nullptr);
    std::string result = path ? path : "Labels";
    g_free(path);
    return result;
}

std::string CurrentGameManager::LabelPackDirectory() const
{
    const std::string root = LabelRootDirectory();
    gchar *path = g_build_filename(root.c_str(), "Packs", nullptr);
    std::string result = path ? path : root + "/Packs";
    g_free(path);
    return result;
}

std::string CurrentGameManager::LabelXdkDirectory() const
{
    const std::string root = LabelRootDirectory();
    gchar *path = g_build_filename(root.c_str(), "XDK", nullptr);
    std::string result = path ? path : root + "/XDK";
    g_free(path);
    return result;
}

std::string CurrentGameManager::LabelPdbDirectory() const
{
    const std::string root = LabelRootDirectory();
    gchar *path = g_build_filename(root.c_str(), "PDB", nullptr);
    std::string result = path ? path : root + "/PDB";
    g_free(path);
    return result;
}

std::string CurrentGameManager::LabelCacheDirectory() const
{
    const std::string root = LabelRootDirectory();
    gchar *path = g_build_filename(root.c_str(), "Cache", nullptr);
    std::string result = path ? path : root + "/Cache";
    g_free(path);
    return result;
}

std::string CurrentGameManager::SuggestedCurrentLabelPackPath() const
{
    if (!m_info.valid) {
        return {};
    }
    std::string hash = m_info.header_sha256.size() >= 16
                           ? m_info.header_sha256.substr(0, 16)
                           : m_info.header_sha256;
    std::transform(hash.begin(), hash.end(), hash.begin(),
                   [](unsigned char ch) { return (char)std::toupper(ch); });
    std::string id = FormatDatabaseGameId(m_info.title_id);
    std::transform(id.begin(), id.end(), id.begin(),
                   [](unsigned char ch) { return (char)std::toupper(ch); });
    const std::string filename = id + "-" + hash + ".xlabel";
    const std::string dir = LabelPackDirectory();
    gchar *path = g_build_filename(dir.c_str(), filename.c_str(), nullptr);
    std::string result = path ? path : filename;
    g_free(path);
    return result;
}

bool CurrentGameManager::EnsureLabelDirectories(std::string &error) const
{
    error.clear();
    const std::string root = LabelRootDirectory();
    const char *children[] = {"XDK", "PDB", "Packs", "Cache"};
    if (g_mkdir_with_parents(root.c_str(), 0755) != 0) {
        error = "Could not create Labels directory: " + root;
        return false;
    }
    for (const char *child : children) {
        gchar *path = g_build_filename(root.c_str(), child, nullptr);
        if (path == nullptr || g_mkdir_with_parents(path, 0755) != 0) {
            error = std::string("Could not create Labels/") + child +
                    " directory.";
            g_free(path);
            return false;
        }
        g_free(path);
    }
    return true;
}

bool CurrentGameManager::MergeLabelPackText(const std::string &path,
                                            const std::string &text,
                                            bool report_mismatch,
                                            bool defer_sort)
{
    if (std::find(m_loaded_label_packs.begin(), m_loaded_label_packs.end(),
                  path) != m_loaded_label_packs.end()) {
        if (report_mismatch) {
            m_label_status = "Label pack is already loaded: " + path;
        }
        return true;
    }

    XemuLabelPacks::Pack pack;
    std::string error;
    if (!XemuLabelPacks::Parse(text, pack, error)) {
        if (report_mismatch) {
            m_label_status = "Could not load .xlabel: " + error;
        }
        return false;
    }

    std::string reason;
    if (!XemuLabelPacks::Matches(pack.header, CurrentLabelIdentity(), reason)) {
        if (report_mismatch) {
            m_label_status = "Label pack does not match current game: " + reason;
        }
        return false;
    }

    std::vector<XemuXbeLabels::Label> resolved;
    size_t unresolved = 0;
    if (!XemuLabelPacks::Resolve(pack, m_auto_labels, resolved,
                                 unresolved, error)) {
        if (report_mismatch) {
            m_label_status = "Could not resolve .xlabel: " + error;
        }
        return false;
    }
    if (defer_sort) {
        XemuXbeLabels::Append(m_labels, resolved);
    } else {
        XemuXbeLabels::Merge(m_labels, resolved);
        ++m_label_generation;
    }
    m_loaded_label_packs.push_back(path);

    char status[384];
    std::snprintf(status, sizeof(status),
                  "Loaded %zu label(s) from %zu .xlabel pack(s)%s.",
                  resolved.size(), m_loaded_label_packs.size(),
                  unresolved ? " (some entries unresolved)" : "");
    m_label_status = status;
    return true;
}

bool CurrentGameManager::LoadLabelPackFile(const std::string &path)
{
    if (!m_info.valid || m_auto_labels.sections.empty()) {
        m_label_status = "A running XBE with parsed section information is required.";
        return false;
    }
    gchar *contents = nullptr;
    gsize length = 0;
    GError *error = nullptr;
    if (!g_file_get_contents(path.c_str(), &contents, &length, &error)) {
        m_label_status = "Could not read .xlabel file: ";
        m_label_status += error ? error->message : path;
        if (error) g_error_free(error);
        return false;
    }
    const std::string text(contents, length);
    g_free(contents);
    return MergeLabelPackText(path, text, true);
}

bool CurrentGameManager::ReloadLabelPacks()
{
    m_labels = m_auto_labels;
    XemuXbeLabels::Append(m_labels, m_xdk_labels);
    XemuXbeLabels::Append(m_labels, m_map_labels);
    XemuXbeLabels::Append(m_labels, m_pdb_labels);
    m_loaded_label_packs.clear();
    if (!m_info.valid || m_auto_labels.sections.empty()) {
        XemuXbeLabels::SortAndUnique(m_labels);
        ++m_label_generation;
        m_label_status = m_auto_labels.labels.empty()
                             ? "No XBE labels are available for the current game."
                             : "XBE section metadata is unavailable for label packs.";
        return false;
    }

    std::string dir_error;
    if (!EnsureLabelDirectories(dir_error)) {
        m_label_status = dir_error;
        return false;
    }

    const std::string dir = LabelPackDirectory();
    GError *open_error = nullptr;
    GDir *handle = g_dir_open(dir.c_str(), 0, &open_error);
    if (handle == nullptr) {
        m_label_status = "Could not open Labels/Packs: ";
        m_label_status += open_error ? open_error->message : dir;
        if (open_error) g_error_free(open_error);
        return false;
    }

    size_t imported = 0;
    const gchar *name = nullptr;
    while ((name = g_dir_read_name(handle)) != nullptr) {
        const size_t len = std::strlen(name);
        if (len < 7 || g_ascii_strcasecmp(name + len - 7, ".xlabel") != 0) {
            continue;
        }
        gchar *full = g_build_filename(dir.c_str(), name, nullptr);
        if (full == nullptr) {
            continue;
        }
        gchar *contents = nullptr;
        gsize length = 0;
        if (g_file_get_contents(full, &contents, &length, nullptr)) {
            const size_t before = m_labels.labels.size();
            if (MergeLabelPackText(full, std::string(contents, length), false, true)) {
                imported += m_labels.labels.size() - before;
            }
            g_free(contents);
        }
        g_free(full);
    }
    g_dir_close(handle);

    /* All active label sources are now present. Canonicalize once instead of
     * sorting the growing database after every XDK/MAP/PDB/pack append. */
    XemuXbeLabels::SortAndUnique(m_labels);
    ++m_label_generation;

    if (m_loaded_label_packs.empty()) {
        char status[160];
        std::snprintf(status, sizeof(status),
                      "%zu XBE + %zu XDK + %zu MAP + %zu PDB label(s) ready; no matching .xlabel pack found.",
                      m_auto_labels.labels.size(), m_xdk_labels.size(),
                      m_map_labels.size(), m_pdb_labels.size());
        m_label_status = status;
        return true;
    }

    char status[224];
    std::snprintf(status, sizeof(status),
                  "%zu XBE + %zu XDK + %zu MAP + %zu PDB + %zu imported label(s) from %zu matching .xlabel pack(s).",
                  m_auto_labels.labels.size(), m_xdk_labels.size(),
                  m_map_labels.size(), m_pdb_labels.size(), imported,
                  m_loaded_label_packs.size());
    m_label_status = status;
    return true;
}

bool CurrentGameManager::RefreshXdkLabels(bool rebuild_cache)
{
    m_xdk_labels.clear();
    m_xdk_status = {};
    if (!m_info.valid || m_auto_labels.sections.empty() ||
        m_disc_xbe_file.empty()) {
        m_xdk_status.message =
            "A mounted default.xbe with parsed section information is required.";
        return false;
    }

    std::string dir_error;
    if (!EnsureLabelDirectories(dir_error)) {
        m_xdk_status.message = dir_error;
        return false;
    }

    std::string error;
    const bool ok = XemuXdkLabels::Process(
        LabelXdkDirectory(), LabelCacheDirectory(), m_disc_xbe_file,
        m_auto_labels, rebuild_cache, m_xdk_labels, m_xdk_status, error);
    ReloadLabelPacks();
    if (!ok && !error.empty()) {
        m_label_status = "XDK: " + error;
    }
    return ok;
}

bool CurrentGameManager::LoadMapFile(const std::string &path)
{
    if (!m_info.valid || m_auto_labels.sections.empty()) {
        m_label_status =
            "A running XBE with parsed section information is required.";
        return false;
    }

    std::string map_text;
    std::string read_error;
    if (!read_local_text_file(path, 64ull * 1024ull * 1024ull,
                              map_text, read_error)) {
        m_label_status = "Could not read MAP file: " + read_error;
        return false;
    }

    std::vector<XemuXbeLabels::Label> labels;
    XemuMapLabels::Status status;
    std::string error;
    const bool ok = XemuMapLabels::ParseAndResolve(
        map_text, m_auto_labels, m_info.pe_timestamp,
        labels, status, error);

    m_map_status = status;
    m_loaded_map_path = path;
    if (!ok) {
        m_map_labels.clear();
        ReloadLabelPacks();
        m_label_status = error.empty() ? status.message : error;
        return false;
    }

    m_map_labels = std::move(labels);
    ReloadLabelPacks();
    m_label_status = status.message;
    return true;
}

bool CurrentGameManager::LoadPdbFile(const std::string &path)
{
    if (!m_info.valid || m_auto_labels.sections.empty() ||
        m_disc_xbe_file.empty()) {
        m_label_status =
            "A mounted default.xbe with parsed section information is required.";
        return false;
    }

    std::vector<uint8_t> pdb_file;
    std::string read_error;
    if (!read_local_binary_file(path, 512ull * 1024ull * 1024ull,
                                pdb_file, read_error)) {
        m_label_status = "Could not read PDB file: " + read_error;
        return false;
    }

    std::vector<XemuXbeLabels::Label> labels;
    XemuPdbLabels::Status status;
    std::string error;
    const bool ok = XemuPdbLabels::ParseAndResolve(
        pdb_file, m_disc_xbe_file, m_auto_labels, labels, status, error);

    m_pdb_status = status;
    m_loaded_pdb_path = path;
    if (!ok) {
        m_pdb_labels.clear();
        ReloadLabelPacks();
        m_label_status = error.empty() ? status.message : error;
        return false;
    }

    m_pdb_labels = std::move(labels);
    ReloadLabelPacks();
    m_label_status = status.message;
    return true;
}

bool CurrentGameManager::SaveLabelPackFile(const std::string &path)
{
    std::string text;
    std::string error;
    if (!XemuLabelPacks::Serialize(CurrentLabelIdentity(), m_labels,
                                   text, error)) {
        m_label_status = error;
        return false;
    }

    gchar *parent = g_path_get_dirname(path.c_str());
    if (parent == nullptr || g_mkdir_with_parents(parent, 0755) != 0) {
        m_label_status = "Could not create .xlabel output directory.";
        g_free(parent);
        return false;
    }
    g_free(parent);

    GError *write_error = nullptr;
    if (!g_file_set_contents(path.c_str(), text.data(), text.size(),
                             &write_error)) {
        m_label_status = "Could not save .xlabel: ";
        m_label_status += write_error ? write_error->message : path;
        if (write_error) g_error_free(write_error);
        return false;
    }
    m_label_status = "Saved portable .xlabel pack: " + path;
    return true;
}

void CurrentGameManager::RefreshDisc(bool force)
{
    XemuDiscBlockHandle blk = xemu_disc_block_by_name("ide0-cd1");
    const uintptr_t backend_identity = xemu_disc_block_identity(blk);
    const bool available = xemu_disc_block_is_available(blk);
    const int64_t image_size = available ? xemu_disc_block_get_length(blk) : -1;

    const bool media_changed = backend_identity != m_disc_backend_identity ||
                               image_size != m_disc_image_size;
    if (!force && !media_changed) {
        return;
    }

    m_disc_backend_identity = backend_identity;
    m_disc_image_size = image_size;
    m_disc = {};
    m_disc_status.clear();
    m_info.disc_xbe_sha256.clear();
    m_info.disc_xbe_size = 0;
    m_info.disc_xbe_start_sector = 0;
    m_info.disc_xbe_offset = 0;
    m_labels = {};
    ++m_label_generation;
    m_auto_labels = {};
    m_xdk_labels.clear();
    m_xdk_status = {};
    m_map_labels.clear();
    m_map_status = {};
    m_loaded_map_path.clear();
    m_pdb_labels.clear();
    m_pdb_status = {};
    m_loaded_pdb_path.clear();
    m_xbe_pdb_identity = {};
    m_disc_xbe_file.clear();
    m_loaded_label_packs.clear();
    m_label_status.clear();

    if (blk == nullptr || backend_identity == 0 || image_size <= 0 ||
        !available) {
        m_disc_status = "No DVD media is currently mounted.";
        return;
    }

    auto reader = [blk, image_size](uint64_t offset, void *buffer,
                                    size_t size) -> bool {
        if (!range_inside(offset, size, (uint64_t)image_size)) {
            return false;
        }
        return xemu_disc_block_pread(blk, offset, buffer, size);
    };

    if (!XemuXdvdfs::Parse(reader, (uint64_t)image_size, m_disc,
                           m_disc_status)) {
        return;
    }

    const XemuXdvdfs::Entry *default_xbe =
        XemuXdvdfs::FindRootFile(m_disc, "default.xbe");
    if (default_xbe == nullptr) {
        m_disc_status = "XDVDFS loaded, but default.xbe was not found in the disc root.";
        return;
    }

    m_info.disc_xbe_size = default_xbe->size;
    m_info.disc_xbe_start_sector = default_xbe->start_sector;
    m_info.disc_xbe_offset = default_xbe->disc_offset;

    std::vector<uint8_t> xbe_file;
    std::string label_read_error;
    if (read_disc_file(blk, (uint64_t)image_size, default_xbe->disc_offset,
                       default_xbe->size, xbe_file, label_read_error)) {
        std::string hash_error;
        m_info.disc_xbe_sha256 = sha256_bytes(xbe_file, hash_error);
        if (!hash_error.empty()) {
            m_disc_status = hash_error;
            return;
        }

        std::string label_error;
        if (XemuXbeLabels::Build(xbe_file, m_labels, label_error)) {
            m_auto_labels = m_labels;
            m_disc_xbe_file = std::move(xbe_file);
            XemuPdbLabels::ExtractXbeIdentity(m_disc_xbe_file,
                                              m_xbe_pdb_identity);
            std::string xdk_error;
            XemuXdkLabels::Process(LabelXdkDirectory(), LabelCacheDirectory(),
                                   m_disc_xbe_file, m_auto_labels, false,
                                   m_xdk_labels, m_xdk_status, xdk_error);
            ReloadLabelPacks();
        } else {
            m_label_status = label_error;
        }
    } else {
        m_label_status = label_read_error;
    }

    char status[160];
    std::snprintf(status, sizeof(status),
                  "XDVDFS loaded: %zu files, %zu folders.",
                  m_disc.file_count, m_disc.directory_count);
    m_disc_status = status;
}

void CurrentGameManager::Refresh(bool force)
{
    RefreshInternal(force, true);
}

void CurrentGameManager::RefreshRunningXbe(bool force)
{
    /* PREENTRY reset recovery only needs fresh loaded-XBE identity. Avoid
     * forcing XDVDFS/default.xbe hashing, label packs, XDK/MAP/PDB processing,
     * or other mounted-disc refresh work on every explicit game reset. */
    RefreshInternal(force, false);
}

void CurrentGameManager::RefreshInternal(bool force, bool refresh_disc)
{
    const uint64_t now = SDL_GetTicks();
    if (!force && m_last_refresh_ms != 0 &&
        now - m_last_refresh_ms < kRefreshIntervalMs) {
        return;
    }
    m_last_refresh_ms = now;

    struct xbe *xbe = xemu_get_xbe_info();
    const bool have_xbe = xbe && xbe->header && xbe->cert &&
                          xbe->headers && xbe->headers_len != 0;

    if (!have_xbe) {
        /* No running XBE is already the stable state in the common frontend/
         * boot-menu case. Preserve disc-derived metadata in place and avoid
         * rebuilding/assigning an equivalent empty GameInfo every 500 ms. */
        m_loaded_xbe_headers.clear();
        m_loaded_xbe_derived_valid = false;
        if (!m_info.valid) {
            if (refresh_disc) {
                RefreshDisc(force);
            }
            return;
        }

        GameInfo next;
        next.disc_xbe_sha256 = m_info.disc_xbe_sha256;
        next.disc_xbe_size = m_info.disc_xbe_size;
        next.disc_xbe_start_sector = m_info.disc_xbe_start_sector;
        next.disc_xbe_offset = m_info.disc_xbe_offset;
        m_info = std::move(next);
        ++m_generation;
        if (refresh_disc) {
            RefreshDisc(true);
        }
        return;
    }

    /* The loaded-header SHA-256 is part of game/revision identity, so a cheap
     * field-only fingerprint is not sufficient. xemu_get_xbe_info() has
     * already reread the entire header block; compare every byte with our last
     * exact copy. Only an exact match may skip SHA-256, UTF-16 title conversion,
     * revision-key construction, and GameInfo string assignment. Force refresh
     * deliberately bypasses this fast path. */
    const bool headers_unchanged =
        !force && m_info.valid && m_loaded_xbe_derived_valid &&
        m_loaded_xbe_headers.size() == xbe->headers_len &&
        std::memcmp(m_loaded_xbe_headers.data(), xbe->headers,
                    xbe->headers_len) == 0;
    if (headers_unchanged) {
        if (refresh_disc) {
            RefreshDisc(false);
        }
        return;
    }

    m_loaded_xbe_headers.assign(xbe->headers,
                                xbe->headers + xbe->headers_len);

    GameInfo next;
    next.valid = true;
    next.title_id = xbe->cert->m_titleid;
    const bool title_name_valid = title_name_from_xbe(xbe, next.title_name);
    next.region = xbe->cert->m_game_region;
    next.disc_number = xbe->cert->m_disk_number;
    next.version = xbe->cert->m_version;
    next.xbe_base = xbe->header->m_base;
    next.xbe_image_size = xbe->header->m_sizeof_image;
    next.pe_checksum = xbe->header->m_pe_checksum;
    next.pe_timestamp = xbe->header->m_pe_timedate;
    next.header_sha256 = sha256_headers(xbe);
    m_loaded_xbe_derived_valid =
        title_name_valid && !next.header_sha256.empty();

    const std::string title = FormatTitleId(next.title_id);
    const std::string short_hash = next.header_sha256.size() >= 16
                                       ? next.header_sha256.substr(0, 16)
                                       : next.header_sha256;
    next.revision_key = title + "-" + short_hash;

    const bool game_changed = !SameIdentity(m_info, next);
    // Preserve disc-derived metadata until RefreshDisc decides whether the
    // mounted backend changed and needs to be rescanned.
    next.disc_xbe_sha256 = m_info.disc_xbe_sha256;
    next.disc_xbe_size = m_info.disc_xbe_size;
    next.disc_xbe_start_sector = m_info.disc_xbe_start_sector;
    next.disc_xbe_offset = m_info.disc_xbe_offset;
    m_info = std::move(next);

    if (game_changed) {
        ++m_generation;
    }

    if (refresh_disc) {
        RefreshDisc(force || game_changed);
    }
}

// Current Game rendering/UI methods are owned by current-game-ui.cc.

void CurrentGameManager::RequestDiscExport(
    const XemuXdvdfs::Entry &entry, const std::vector<std::string> &path)
{
    XemuXdvdfsExport::Target target;
    target.components = path;
    target.directory = entry.IsDirectory();
    target.size = entry.size;
    target.start_sector = entry.start_sector;

    ShowOpenFolderDialog(nullptr, [this, target](const char *destination) {
        if (!destination || destination[0] == '\0') {
            return;
        }
        XemuXdvdfsExport::Result result;
        std::string error;
        if (!XemuXdvdfsExport::ExportToHost(target, destination, result, error)) {
            m_disc_export_status = "Disc export failed: " + error;
            return;
        }
        m_disc_export_status = "Exported " + std::to_string(result.file_count) +
            " file(s), " + std::to_string(result.directory_count) +
            " folder(s), " + FormatByteSize(result.byte_count) + " to " +
            result.host_path;
    });
}

