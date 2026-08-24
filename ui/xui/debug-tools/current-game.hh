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
#pragma once

#include "../common.hh"

#include <cstdint>
#include <string>

#include "xdvdfs-disc.hh"
#include "xbe-labels.hh"
#include "label-packs.hh"
#include "xdk-labels.hh"
#include "map-labels.hh"
#include "pdb-labels.hh"

class CurrentGameManager
{
public:
    struct GameInfo {
        bool valid = false;
        uint32_t title_id = 0;
        std::string title_name;
        uint32_t region = 0;
        uint32_t disc_number = 0;
        uint32_t version = 0;
        uint32_t xbe_base = 0;
        uint32_t xbe_image_size = 0;
        uint32_t pe_checksum = 0;
        uint32_t pe_timestamp = 0;
        std::string header_sha256;
        std::string disc_xbe_sha256;
        uint32_t disc_xbe_size = 0;
        uint32_t disc_xbe_start_sector = 0;
        uint64_t disc_xbe_offset = 0;
        std::string revision_key;
    };

    bool is_open = false;

    void Refresh(bool force = false);
    void RefreshRunningXbe(bool force = false);
    void Draw(bool detached = false);
    void DrawInlineSummary(const char *id) const;

    const GameInfo &Get() const { return m_info; }
    uint64_t Generation() const { return m_generation; }
    bool HasGame() const { return m_info.valid; }
    const XemuXbeLabels::Database &Labels() const { return m_labels; }
    uint64_t LabelGeneration() const { return m_label_generation; }
    const XemuXbeLabels::Label *PrimaryLabelAt(uint32_t virtual_address) const
    {
        return XemuXbeLabels::PrimaryAt(m_labels, virtual_address);
    }
    const std::string &LabelStatus() const { return m_label_status; }
    const XemuXdkLabels::Status &XdkStatus() const { return m_xdk_status; }
    const XemuMapLabels::Status &MapStatus() const { return m_map_status; }
    const XemuPdbLabels::Status &PdbStatus() const { return m_pdb_status; }

    std::string LabelRootDirectory() const;
    std::string LabelPackDirectory() const;
    std::string LabelXdkDirectory() const;
    std::string LabelPdbDirectory() const;
    std::string LabelCacheDirectory() const;
    std::string SuggestedCurrentLabelPackPath() const;
    bool EnsureLabelDirectories(std::string &error) const;
    bool ReloadLabelPacks();
    bool RefreshXdkLabels(bool rebuild_cache = false);
    bool LoadMapFile(const std::string &path);
    bool LoadPdbFile(const std::string &path);
    bool LoadLabelPackFile(const std::string &path);
    bool SaveLabelPackFile(const std::string &path);

    static std::string FormatTitleId(uint32_t title_id);
    static std::string FormatDatabaseGameId(uint32_t title_id);
    static std::string FormatRegion(uint32_t region);

private:
    GameInfo m_info;
    uint64_t m_generation = 0;
    uint64_t m_last_refresh_ms = 0;
    XemuXdvdfs::Disc m_disc;
    std::string m_disc_status;
    std::string m_disc_export_status;
    XemuXbeLabels::Database m_labels;
    uint64_t m_label_generation = 0;
    XemuXbeLabels::Database m_auto_labels;
    std::vector<XemuXbeLabels::Label> m_xdk_labels;
    XemuXdkLabels::Status m_xdk_status;
    std::vector<XemuXbeLabels::Label> m_map_labels;
    XemuMapLabels::Status m_map_status;
    std::string m_loaded_map_path;
    std::vector<XemuXbeLabels::Label> m_pdb_labels;
    XemuPdbLabels::Status m_pdb_status;
    std::string m_loaded_pdb_path;
    XemuPdbLabels::Identity m_xbe_pdb_identity;
    std::vector<uint8_t> m_disc_xbe_file;
    // Exact copy of the last loaded in-memory XBE header block. Refresh()
    // still rereads the complete block every poll so a revision change cannot
    // be hidden by a partial fingerprint; identical bytes let us skip the
    // repeated SHA/title/GameInfo rebuild work. Capacity is retained across
    // game changes to avoid adding another recurring allocation.
    std::vector<uint8_t> m_loaded_xbe_headers;
    bool m_loaded_xbe_derived_valid = false;
    std::vector<std::string> m_loaded_label_packs;
    std::string m_label_status;
    uintptr_t m_disc_backend_identity = 0;
    int64_t m_disc_image_size = 0;
    bool m_show_kernel_rpc_diagnostics = false;

    void RefreshInternal(bool force, bool refresh_disc);
    void RefreshDisc(bool force);
    void DrawGameInfoTab(bool detached);
    void DrawDiscContentsTab();
    void DrawDiscEntry(const XemuXdvdfs::Entry &entry,
                       std::vector<std::string> &path);
    void RequestDiscExport(const XemuXdvdfs::Entry &entry,
                           const std::vector<std::string> &path);
    static std::string FormatByteSize(uint64_t bytes);

    XemuLabelPacks::Identity CurrentLabelIdentity() const;
    bool MergeLabelPackText(const std::string &path, const std::string &text,
                            bool report_mismatch, bool defer_sort = false);

    static bool SameIdentity(const GameInfo &a, const GameInfo &b);
};

extern CurrentGameManager current_game_manager;
