//
// xemu User Interface - Game library / launcher grid
//
// Copyright (C) 2024 xemu contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#pragma once

#include <epoxy/gl.h>
#include <atomic>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

struct XemuGameInfo;

class GameListWindow
{
public:
    bool is_open;

    GameListWindow();
    ~GameListWindow();
    void Draw();

    // Keep the VM paused until the user picks a game, boots without a disc,
    // or dismisses the launcher. Called at startup when the library opens
    // automatically.
    void HoldVMAtBoot();

private:
    struct Entry {
        std::string path;        // full path to the disc image
        std::string filename;    // stem, used as fallback label
        std::string title;       // title from XBE cert ("" if none)
        std::string title_id;    // formatted title id, e.g. "SC-0250"
        uint32_t    rating;      // raw XBE game-ratings field (0 = unknown)
        uint32_t    region;      // raw XBE region bitfield (0 = unknown)
        GLuint      tex;         // cover texture (0 if none)
        int         tex_w, tex_h;
        bool        scanned;     // metadata available (from scan thread)
        bool        favorite;
        uint64_t    last_played; // unix time of last launch (0 = never)
        uint64_t    play_ms;     // accumulated play time in milliseconds

        // Derived, cached to avoid rebuilding them every frame in Draw().
        std::string region_label; // e.g. "USA", "PAL" (from region)
        std::string search_key;   // lowercased filename+title+id+region
        bool        has_snapshot; // any saved state matches this entry
    };

    // Result of scanning one disc image on the worker thread. The GL texture
    // upload happens on the UI thread when the result is applied.
    struct ScanResult {
        size_t      index;
        uint64_t    gen;
        bool        ok;
        std::string title;
        std::string title_id;
        uint32_t    rating;
        uint32_t    region;
        uint8_t    *rgba; // owned; g_free after upload
        int         w, h;
    };

    std::vector<Entry> m_entries;
    std::string        m_scanned_dir;
    uint64_t           m_covers_fingerprint = 0;
    char               m_search[128];
    float              m_icon_size;
    float              m_row_size;  // list view thumbnail/row height
    int                m_sort_mode; // 0 = name, 1 = recently played
    int                m_view_mode; // 0 = grid, 1 = list
    bool               m_hold_vm;
    bool               m_focus_first;
    bool               m_was_open;  // edge detection for (re)open refresh
    bool               m_popup_was_open = false; // popup state last frame

    std::map<std::string, uint64_t> m_recent;
    std::set<std::string>           m_favorites;

    // Play time tracking for the game launched from the library
    std::map<std::string, uint64_t> m_playtime; // key -> milliseconds
    std::string m_current_key;  // recent-key of the running game ("" = none)
    std::string m_current_path; // disc path it was launched with
    uint32_t    m_last_tick_ms;
    uint32_t    m_last_save_ms;

    // Background metadata scanning
    std::thread             m_scan_thread;
    std::mutex              m_scan_mutex;
    std::vector<ScanResult> m_scan_results;
    std::atomic<bool>       m_scan_abort;
    uint64_t                m_scan_gen;

    void Rescan();
    void ClearTextures();
    void ReleaseVM();
    void ApplyEntryMeta(Entry &e);
    void RecomputeDerived(Entry &e);
    void RefreshSnapshotFlags();
    void RefreshEntryMeta();

    void StartScan();
    void StopScan();
    void ApplyScanResults();
    void ScanWorker(std::vector<std::pair<size_t, std::string>> work,
                    uint64_t gen);

    void LoadRecent();
    void RecordPlayed(const Entry &e);

    void LoadPlaytime();
    void SavePlaytime();
    void StartPlaySession(const Entry &e);
    void TickPlaytime();

    void LoadFavorites();
    void SaveFavorites();
    void ToggleFavorite(const Entry &e);
};

extern GameListWindow game_list_window;
