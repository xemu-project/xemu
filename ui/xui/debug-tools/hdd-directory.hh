//
// xemu Xbox HDD Directory Viewer
//
// Copyright (C) 2026 xemu contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
#pragma once

#include "fatx-hdd.hh"
#include "kernel-rpc-filesystem.hh"

#include <cstdint>
#include <string>
#include <vector>

class HddDirectoryWindow
{
public:
    bool is_open = false;

    void Draw(bool detached = false);
    void DrawCurrentGameHdd(uint32_t title_id);
    void Refresh();

    const std::string &Status() const { return m_status; }

private:
    struct HddTarget {
        char partition = '?';
        std::vector<std::string> path;
        bool directory = false;
    };

    HddTarget m_delete_target;
    bool m_delete_pending = false;
    bool m_delete_popup_open_requested = false;
    std::string m_delete_display;
    std::vector<XemuKernelFs::DeleteEntry> m_delete_confirm_plan;

    bool m_import_pending = false;
    bool m_import_popup_open_requested = false;
    XemuKernelFs::ImportPlan m_import_confirm_plan;

    bool m_new_folder_pending = false;
    bool m_new_folder_popup_open_requested = false;
    HddTarget m_new_folder_destination;
    char m_new_folder_name[43] = {};

    bool m_rename_pending = false;
    bool m_rename_popup_open_requested = false;
    HddTarget m_rename_target;
    char m_rename_name[43] = {};

    enum class TransferSelectionMode { None, Copy, Move };
    TransferSelectionMode m_transfer_selection_mode = TransferSelectionMode::None;
    HddTarget m_transfer_source;
    std::string m_transfer_source_display;
    bool m_move_confirm_pending = false;
    bool m_move_popup_open_requested = false;
    XemuKernelFs::RelocatePlan m_move_confirm_plan;

    XemuFatxHdd::Snapshot m_snapshot;
    bool m_has_snapshot = false;
    uint64_t m_snapshot_change_generation = 0;
    bool m_kernel_status_visible = false;
    std::string m_status;
    std::string m_operation_status;
    char m_name_filter[128] = {};

    void RefreshIfStale();
    void DrawEntries(const XemuFatxHdd::Partition &partition,
                     const std::vector<XemuFatxHdd::Entry> &entries,
                     std::vector<std::string> &path,
                     bool current_game_view = false);
    void DrawCurrentGameArea(const XemuFatxHdd::Partition &partition,
                             const XemuFatxHdd::Entry *area,
                             const std::string &title_id,
                             const char *area_name,
                             const char *description);
    void DrawExportContext(const XemuFatxHdd::Partition &partition,
                           const XemuFatxHdd::Entry &entry,
                           const std::vector<std::string> &path,
                           bool save_folder,
                           bool current_game_view);
    void DrawImportMenuItems(const HddTarget &destination);
    void DrawRootImportButton(const XemuFatxHdd::Partition &partition,
                              const std::vector<std::string> &destination,
                              const char *popup_id);
    void RequestImport(const HddTarget &destination, bool folder,
                       const std::string &required_root_name = {});
    void DrawImportConfirmation();
    void RequestNewFolder(const HddTarget &destination);
    void DrawNewFolderPopup();
    void RequestRename(const HddTarget &target);
    void DrawRenamePopup();
    void SelectMoveSource(const HddTarget &target);
    void RequestMoveHere(const HddTarget &destination);
    void ClearTransferSelection();
    void DrawMoveConfirmation();
    void SelectCopySource(const HddTarget &target);
    void RequestCopyHere(const HddTarget &destination);

    void RequestDelete(const HddTarget &target,
                       const std::string &display_name);
    void DrawDeleteConfirmation();
    static bool DeleteAllowed(const HddTarget &target);

    void RequestExport(const HddTarget &target);

    void DrawKernelStatus();
    static std::string FormatByteSize(uint64_t bytes);
    static std::string FormatAttributes(uint8_t attributes);
};

extern HddDirectoryWindow hdd_directory_window;
