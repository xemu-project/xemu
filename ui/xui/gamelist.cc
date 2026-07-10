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
#include "gamelist.hh"
#include "common.hh"
#include "misc.hh"
#include "actions.hh"
#include "main-menu.hh"
#include "font-manager.hh"
#include "snapshot-manager.hh"
#include "viewport-manager.hh"

#include "../xemu-gameinfo.h"

#include <glib/gstdio.h> /* g_fopen/g_unlink/g_stat/GStatBuf */
#include <algorithm>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <map>
#include <numeric>

GameListWindow game_list_window;

// xemu accent green, matching the global UI style.
#define ACCENT_COL IM_COL32(61, 153, 0, 255)
#define STAR_COL IM_COL32(255, 200, 40, 255)

// On-disk metadata cache entry header (followed by raw RGBA icon pixels).
#define GAMELIST_CACHE_MAGIC 0x334C4758 // "XGL3"

GameListWindow::GameListWindow()
    : is_open(false), m_icon_size(140.0f), m_row_size(40.0f), m_sort_mode(0),
      m_view_mode(0),
      m_hold_vm(false), m_focus_first(false), m_was_open(false),
      m_last_tick_ms(0), m_last_save_ms(0), m_scan_abort(false), m_scan_gen(0)
{
    m_search[0] = '\0';
}

GameListWindow::~GameListWindow()
{
    StopScan();
    // GL context is already gone at static teardown; only flush play time.
    if (!m_current_key.empty()) {
        SavePlaytime();
    }
}

void GameListWindow::HoldVMAtBoot()
{
    m_hold_vm = true;
    m_focus_first = true;
}

// Let the machine run again after the launcher held it paused at boot.
void GameListWindow::ReleaseVM()
{
    if (!m_hold_vm) {
        return;
    }
    m_hold_vm = false;
    if (!runstate_is_running()) {
        vm_start();
    }
}

// ---------------------------------------------------------------------------
// Paths & small helpers
// ---------------------------------------------------------------------------

static std::string GamelibDir()
{
    return std::string(xemu_settings_get_base_path()) + "gamelib";
}

static std::string CoversDir()
{
    return std::string(xemu_settings_get_base_path()) + "covers";
}

static void EnsureDirs()
{
    static bool done = false;
    if (done) {
        return;
    }
    done = true;
    qemu_mkdir(GamelibDir().c_str());
    qemu_mkdir(CoversDir().c_str());
}

static uint64_t Fnv1a64(const char *s)
{
    uint64_t h = 1469598103934665603ULL;
    for (; *s; s++) {
        h ^= (uint8_t)*s;
        h *= 1099511628211ULL;
    }
    return h;
}

static std::string CachePathFor(const std::string &iso_path)
{
    char name[32];
    snprintf(name, sizeof(name), "%016llx.bin",
             (unsigned long long)Fnv1a64(iso_path.c_str()));
    return GamelibDir() + "/" + name;
}

static bool StatFile(const char *path, uint64_t *size, uint64_t *mtime)
{
    GStatBuf st;
    if (g_stat(path, &st) != 0) {
        return false;
    }
    *size = (uint64_t)st.st_size;
    *mtime = (uint64_t)st.st_mtime;
    return true;
}

// Cheap change detector for <base>/covers/: number of entries combined with
// the newest mtime. Lets the library pick up added/replaced custom covers on
// reopen without requiring a manual rescan.
static uint64_t CoversFingerprint()
{
    uint64_t count = 0, newest = 0;
    GDir *dir = g_dir_open(CoversDir().c_str(), 0, NULL);
    if (!dir) {
        return 0;
    }
    const char *name;
    while ((name = g_dir_read_name(dir)) != NULL) {
        std::string full = CoversDir() + "/" + name;
        uint64_t sz, mt;
        if (StatFile(full.c_str(), &sz, &mt)) {
            count++;
            newest = std::max(newest, mt);
        }
    }
    g_dir_close(dir);
    return (count << 32) ^ newest;
}


static GLuint UploadRGBA(const uint8_t *rgba, int w, int h)
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, rgba);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

static std::string ToLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

// Key used for recents and favorites: the file name including its extension,
// so that different versions of the same game (e.g. an .xiso and a .chd that
// share the title id and even the file stem) keep independent favorites.
static std::string RecentKey(const std::string &iso_path)
{
    return std::filesystem::u8path(iso_path).filename().u8string();
}

// Key used for play time: the title id, so all versions of a game accumulate
// into one shared counter. Falls back to the file name when the disc has no
// title id (or was launched before the scan finished).
static std::string PlaytimeKey(const std::string &title_id,
                               const std::string &iso_path)
{
    return title_id.empty() ? RecentKey(iso_path) : title_id;
}

// ---------------------------------------------------------------------------
// Metadata cache (worker thread only)
// ---------------------------------------------------------------------------

#pragma pack(push, 1)
struct CacheHeader {
    uint32_t magic;
    uint64_t iso_size;
    uint64_t iso_mtime;
    char     title[128];
    char     title_id[16];
    uint32_t region;
    uint32_t rating;
    int32_t  icon_w;
    int32_t  icon_h;
};
#pragma pack(pop)

static bool CacheLoad(const std::string &iso_path, std::string *title,
                      std::string *title_id, uint32_t *rating,
                      uint32_t *region, uint8_t **rgba, int *w, int *h)
{
    uint64_t size, mtime;
    if (!StatFile(iso_path.c_str(), &size, &mtime)) {
        return false;
    }

    FILE *f = g_fopen(CachePathFor(iso_path).c_str(), "rb");
    if (!f) {
        return false;
    }

    CacheHeader hdr;
    bool ok = fread(&hdr, sizeof(hdr), 1, f) == 1 &&
              hdr.magic == GAMELIST_CACHE_MAGIC && hdr.iso_size == size &&
              hdr.iso_mtime == mtime && hdr.icon_w >= 0 && hdr.icon_w <= 1024 &&
              hdr.icon_h >= 0 && hdr.icon_h <= 1024;
    uint8_t *pix = NULL;
    if (ok && hdr.icon_w > 0 && hdr.icon_h > 0) {
        size_t n = (size_t)hdr.icon_w * hdr.icon_h * 4;
        pix = (uint8_t *)g_malloc(n);
        ok = fread(pix, 1, n, f) == n;
    }
    fclose(f);

    if (!ok) {
        g_free(pix);
        return false;
    }

    hdr.title[sizeof(hdr.title) - 1] = '\0';
    hdr.title_id[sizeof(hdr.title_id) - 1] = '\0';
    *title = hdr.title;
    *title_id = hdr.title_id;
    *rating = hdr.rating;
    *region = hdr.region;
    *rgba = pix;
    *w = hdr.icon_w;
    *h = hdr.icon_h;
    return true;
}

static void CacheStore(const std::string &iso_path, const XemuGameInfo *gi)
{
    uint64_t size, mtime;
    if (!StatFile(iso_path.c_str(), &size, &mtime)) {
        return;
    }

    CacheHeader hdr = {};
    hdr.magic = GAMELIST_CACHE_MAGIC;
    hdr.iso_size = size;
    hdr.iso_mtime = mtime;
    g_strlcpy(hdr.title, gi->title, sizeof(hdr.title));
    g_strlcpy(hdr.title_id, gi->title_id_str, sizeof(hdr.title_id));
    hdr.region = gi->region;
    hdr.rating = gi->rating;
    hdr.icon_w = gi->icon_rgba ? gi->icon_width : 0;
    hdr.icon_h = gi->icon_rgba ? gi->icon_height : 0;

    FILE *f = g_fopen(CachePathFor(iso_path).c_str(), "wb");
    if (!f) {
        return;
    }
    bool ok = fwrite(&hdr, sizeof(hdr), 1, f) == 1;
    if (ok && gi->icon_rgba) {
        size_t n = (size_t)gi->icon_width * gi->icon_height * 4;
        ok = fwrite(gi->icon_rgba, 1, n, f) == n;
    }
    fclose(f);
    if (!ok) {
        g_unlink(CachePathFor(iso_path).c_str());
    }
}

// Custom cover art from <base>/covers/, tried in this order:
//   1. <game file name>.png|jpg|jpeg (disc image stem, e.g. "Halo.png") --
//      per-file, so different versions of a game can carry distinct covers
//   2. <TitleID>.png|jpg|jpeg   (e.g. "EA-0090.png") -- shared default
// Falls back to the embedded XBE icon when neither exists.
// Worker thread only (pure file IO + decode).
static void CustomCoverLoad(const std::string &iso_path,
                            const std::string &title_id, uint8_t **rgba,
                            int *w, int *h)
{
    std::vector<std::string> candidates;
    static const char *exts[] = { ".png", ".jpg", ".jpeg" };

    std::string stem = std::filesystem::u8path(iso_path).stem().u8string();
    for (const char *ext : exts) {
        candidates.push_back(CoversDir() + "/" + stem + ext);
    }
    if (!title_id.empty()) {
        for (const char *ext : exts) {
            candidates.push_back(CoversDir() + "/" + title_id + ext);
        }
    }

    for (const auto &c : candidates) {
        if (!g_file_test(c.c_str(), G_FILE_TEST_IS_REGULAR)) {
            continue;
        }
        int cw = 0, ch = 0, n = 0;
        FILE *cf = g_fopen(c.c_str(), "rb");
        if (!cf) {
            continue;
        }
        unsigned char *data = stbi_load_from_file(cf, &cw, &ch, &n, 4);
        fclose(cf);
        if (!data) {
            continue;
        }
        // Guard against covers beyond common GL texture limits (and the
        // memory they would pin); fall back to the embedded XBE icon.
        if (cw <= 0 || ch <= 0 || cw > 4096 || ch > 4096) {
            stbi_image_free(data);
            continue;
        }
        g_free(*rgba);
        size_t bytes = (size_t)cw * ch * 4;
        *rgba = (uint8_t *)g_malloc(bytes);
        memcpy(*rgba, data, bytes);
        stbi_image_free(data);
        *w = cw;
        *h = ch;
        return;
    }
}

// ---------------------------------------------------------------------------
// Background scanning
// ---------------------------------------------------------------------------

void GameListWindow::ScanWorker(
    std::vector<std::pair<size_t, std::string>> work, uint64_t gen)
{
    for (const auto &[index, path] : work) {
        if (m_scan_abort.load()) {
            break;
        }

        ScanResult r = {};
        r.index = index;
        r.gen = gen;
        r.rgba = NULL;
        r.w = r.h = 0;

        if (CacheLoad(path, &r.title, &r.title_id, &r.rating, &r.region,
                      &r.rgba, &r.w, &r.h)) {
            r.ok = true;
        } else {
            XemuGameInfo gi;
            if (xemu_gameinfo_extract(path.c_str(), &gi)) {
                r.ok = true;
                r.title = gi.title;
                r.title_id = gi.title_id_str;
                r.rating = gi.rating;
                r.region = gi.region;
                if (gi.icon_rgba) {
                    size_t n = (size_t)gi.icon_width * gi.icon_height * 4;
                    r.rgba = (uint8_t *)g_malloc(n);
                    memcpy(r.rgba, gi.icon_rgba, n);
                    r.w = gi.icon_width;
                    r.h = gi.icon_height;
                }
                CacheStore(path, &gi);
            }
            xemu_gameinfo_free(&gi);
        }

        CustomCoverLoad(path, r.title_id, &r.rgba, &r.w, &r.h);

        {
            std::lock_guard<std::mutex> lock(m_scan_mutex);
            m_scan_results.push_back(std::move(r));
        }
    }
}

void GameListWindow::StartScan()
{
    std::vector<std::pair<size_t, std::string>> work;
    for (size_t i = 0; i < m_entries.size(); i++) {
        if (!m_entries[i].scanned) {
            work.emplace_back(i, m_entries[i].path);
        }
    }
    if (work.empty()) {
        return;
    }
    m_scan_abort = false;
    m_scan_thread = std::thread(&GameListWindow::ScanWorker, this,
                                std::move(work), m_scan_gen);
}

void GameListWindow::StopScan()
{
    m_scan_abort = true;
    if (m_scan_thread.joinable()) {
        m_scan_thread.join();
    }
    m_scan_abort = false;

    std::lock_guard<std::mutex> lock(m_scan_mutex);
    for (auto &r : m_scan_results) {
        g_free(r.rgba);
    }
    m_scan_results.clear();
}

// Apply finished scan results on the UI thread (GL texture upload).
void GameListWindow::ApplyScanResults()
{
    std::vector<ScanResult> results;
    {
        std::lock_guard<std::mutex> lock(m_scan_mutex);
        results.swap(m_scan_results);
    }

    for (auto &r : results) {
        if (r.gen != m_scan_gen || r.index >= m_entries.size()) {
            g_free(r.rgba);
            continue;
        }
        Entry &e = m_entries[r.index];
        e.scanned = true;
        if (r.ok) {
            e.title = r.title;
            e.title_id = r.title_id;
            e.rating = r.rating;
            e.region = r.region;
        }
        if (r.rgba) {
            if (e.tex) {
                glDeleteTextures(1, &e.tex);
            }
            e.tex = UploadRGBA(r.rgba, r.w, r.h);
            e.tex_w = r.w;
            e.tex_h = r.h;
        }
        g_free(r.rgba);

        RecomputeDerived(e);
        ApplyEntryMeta(e);
    }
    RefreshSnapshotFlags();
}

// ---------------------------------------------------------------------------
// Recently played
// ---------------------------------------------------------------------------

static std::string RecentFilePath()
{
    return GamelibDir() + "/recent.txt";
}

void GameListWindow::LoadRecent()
{
    m_recent.clear();
    gchar *contents = NULL;
    if (!g_file_get_contents(RecentFilePath().c_str(), &contents, NULL,
                             NULL)) {
        return;
    }
    for (char *line = strtok(contents, "\n"); line;
         line = strtok(NULL, "\n")) {
        char *sep = strchr(line, ' ');
        if (!sep) {
            continue;
        }
        *sep = '\0';
        uint64_t ts = g_ascii_strtoull(line, NULL, 10);
        if (ts && sep[1]) {
            m_recent[sep + 1] = ts;
        }
    }
    g_free(contents);
}

void GameListWindow::RecordPlayed(const Entry &e)
{
    m_recent[RecentKey(e.path)] = (uint64_t)time(NULL);

    std::string out;
    for (const auto &[key, ts] : m_recent) {
        out += std::to_string(ts) + " " + key + "\n";
    }
    g_file_set_contents(RecentFilePath().c_str(), out.c_str(), out.size(),
                        NULL);
}

// ---------------------------------------------------------------------------
// Play time tracking
// ---------------------------------------------------------------------------

static std::string PlaytimeFilePath()
{
    return GamelibDir() + "/playtime.txt";
}

void GameListWindow::LoadPlaytime()
{
    m_playtime.clear();
    gchar *contents = NULL;
    if (!g_file_get_contents(PlaytimeFilePath().c_str(), &contents, NULL,
                             NULL)) {
        return;
    }
    for (char *line = strtok(contents, "\n"); line;
         line = strtok(NULL, "\n")) {
        char *sep = strchr(line, ' ');
        if (!sep) {
            continue;
        }
        *sep = '\0';
        uint64_t secs = g_ascii_strtoull(line, NULL, 10);
        if (secs && sep[1]) {
            m_playtime[sep + 1] = secs * 1000;
        }
    }
    g_free(contents);
}

void GameListWindow::SavePlaytime()
{
    std::string out;
    for (const auto &[key, ms] : m_playtime) {
        if (ms < 1000) {
            continue;
        }
        out += std::to_string(ms / 1000) + " " + key + "\n";
    }
    g_file_set_contents(PlaytimeFilePath().c_str(), out.c_str(), out.size(),
                        NULL);
}

void GameListWindow::StartPlaySession(const Entry &e)
{
    m_current_key = PlaytimeKey(e.title_id, e.path);
    m_current_path = e.path;
    m_last_tick_ms = SDL_GetTicks();
    m_last_save_ms = m_last_tick_ms;
}

// Called every frame (even while the launcher is closed): accumulate play
// time for the game launched from the library while the machine is running.
void GameListWindow::TickPlaytime()
{
    uint32_t now = SDL_GetTicks();
    uint32_t delta = now - m_last_tick_ms;
    m_last_tick_ms = now;

    if (m_current_key.empty()) {
        return;
    }

    // Stop counting when the disc changed under us (menu load, eject).
    const char *dvd = g_config.sys.files.dvd_path;
    if (!dvd || m_current_path != dvd) {
        SavePlaytime();
        m_current_key.clear();
        return;
    }

    if (runstate_is_running() && delta < 10000) {
        m_playtime[m_current_key] += delta;
        if (now - m_last_save_ms > 60000) {
            SavePlaytime();
            m_last_save_ms = now;
        }
    }
}

// "34 min played" / "12.5 h played" / "48 h played", "" when < 1 min.
static std::string FormatPlaytime(uint64_t ms)
{
    uint64_t mins = ms / 60000;
    if (mins < 1) {
        return "";
    }
    char buf[48];
    if (mins < 60) {
        snprintf(buf, sizeof(buf), "%llu min played",
                 (unsigned long long)mins);
    } else if (mins < 600) {
        snprintf(buf, sizeof(buf), "%.1f h played", mins / 60.0);
    } else {
        snprintf(buf, sizeof(buf), "%llu h played",
                 (unsigned long long)(mins / 60));
    }
    return buf;
}

// ESRB decode of the XBE certificate game-ratings field, per the parental
// control table on https://xboxdevwiki.net/EEPROM (M/T/E also verified
// against retail discs). 0 means Rating Pending / unrated -- the norm on
// non-US discs, whose PEGI/USK ratings were print-only -- and returns ""
// so the UI can show a placeholder.
static const char *EsrbRatingLabel(uint32_t rating)
{
    switch (rating) {
    case 1: return "Adults Only";
    case 2: return "Mature";
    case 3: return "Teen";
    case 4: return "Everyone";
    case 5: return "Kids to Adults";
    case 6: return "Early Childhood";
    default: return "";
    }
}

// Compact label for the XBE region bitfield (1 = North America, 2 = Japan,
// 4 = rest of world; 0x80000000 = manufacturing). Combinations are joined
// with "/", the all-regions mask reads "ALL".
static std::string RegionLabel(uint32_t region)
{
    region &= 0x7; // ignore the manufacturing bit
    if (region == 0x7) {
        return "ALL";
    }
    std::string s;
    if (region & 0x1) s += "USA";
    if (region & 0x2) s += s.empty() ? "JAP" : "/JAP";
    if (region & 0x4) s += s.empty() ? "PAL" : "/PAL";
    return s;
}

// ---------------------------------------------------------------------------
// Favorites
// ---------------------------------------------------------------------------

static std::string FavoritesFilePath()
{
    return GamelibDir() + "/favorites.txt";
}

void GameListWindow::LoadFavorites()
{
    m_favorites.clear();
    gchar *contents = NULL;
    if (!g_file_get_contents(FavoritesFilePath().c_str(), &contents, NULL,
                             NULL)) {
        return;
    }
    for (char *line = strtok(contents, "\n"); line;
         line = strtok(NULL, "\n")) {
        if (line[0]) {
            m_favorites.insert(line);
        }
    }
    g_free(contents);
}

void GameListWindow::SaveFavorites()
{
    std::string out;
    for (const auto &key : m_favorites) {
        out += key + "\n";
    }
    g_file_set_contents(FavoritesFilePath().c_str(), out.c_str(), out.size(),
                        NULL);
}

void GameListWindow::ToggleFavorite(const Entry &e)
{
    const std::string key = RecentKey(e.path);
    if (m_favorites.count(key)) {
        m_favorites.erase(key);
    } else {
        m_favorites.insert(key);
    }
    SaveFavorites();

    // Keys are per file now, but refresh all entries anyway to keep the
    // flags consistent after external favorites-file edits.
    for (auto &other : m_entries) {
        other.favorite =
            m_favorites.count(RecentKey(other.path)) > 0;
    }
}

// Derive one entry's metadata (last played, play time, favorite) from the
// maps. Reads fall back to the legacy title-id key so recents/play time
// recorded by older builds (which keyed by title id) still show up; new
// writes always use the per-file key.
void GameListWindow::ApplyEntryMeta(Entry &e)
{
    const std::string key = RecentKey(e.path);

    auto it = m_recent.find(key);
    if (it == m_recent.end() && !e.title_id.empty()) {
        it = m_recent.find(e.title_id);
    }
    e.last_played = (it != m_recent.end()) ? it->second : 0;

    /* Play time is shared across all versions of a title. Also pick up
     * per-file entries written by intermediate builds. */
    auto pt = m_playtime.find(PlaytimeKey(e.title_id, e.path));
    if (pt == m_playtime.end()) {
        pt = m_playtime.find(RecentKey(e.path));
    }
    e.play_ms = (pt != m_playtime.end()) ? pt->second : 0;

    e.favorite = m_favorites.count(key) > 0;
}

// Does this snapshot belong to a disc at `path` / with XBE title `title`?
// Shared by the tile badge check and the resume menu so the two never drift.
static bool SnapshotMatches(const XemuSnapshotData &xd, const std::string &path,
                            const std::string &title)
{
    return (xd.disc_path && path == xd.disc_path) ||
           (!title.empty() && xd.xbe_title_name && title == xd.xbe_title_name);
}

// Recompute the immutable derived fields (region label, search key) from an
// entry's scanned metadata. Cheap, but done once per scan instead of every
// frame in Draw().
void GameListWindow::RecomputeDerived(Entry &e)
{
    e.region_label = RegionLabel(e.region);
    e.search_key = ToLower(e.filename) + " " + ToLower(e.title) + " " +
                   ToLower(e.title_id) + " " + ToLower(e.region_label);
}

// Refresh the cached has_snapshot flag on every entry. Snapshots only change
// when g_snapshot_mgr.Refresh() runs, so this is called right after that
// rather than scanning the snapshot list per entry per frame.
void GameListWindow::RefreshSnapshotFlags()
{
    for (auto &e : m_entries) {
        e.has_snapshot = false;
        for (int s = 0; s < g_snapshot_mgr.m_snapshots_len; s++) {
            if (SnapshotMatches(g_snapshot_mgr.m_extra_data[s], e.path,
                                e.title)) {
                e.has_snapshot = true;
                break;
            }
        }
    }
}

// Re-derive the per-entry metadata (last played, play time, favorite) from
// the maps. Called when the library is (re)opened so a session played since
// the last scan shows up in "Sort: Recent" and the play-time column without
// requiring a full rescan.
void GameListWindow::RefreshEntryMeta()
{
    for (auto &e : m_entries) {
        RecomputeDerived(e);
        ApplyEntryMeta(e);
    }
}

// ---------------------------------------------------------------------------
// Scanning (directory walk on the UI thread; metadata on the worker)
// ---------------------------------------------------------------------------

void GameListWindow::ClearTextures()
{
    for (auto &e : m_entries) {
        if (e.tex) {
            glDeleteTextures(1, &e.tex);
            e.tex = 0;
        }
    }
}

void GameListWindow::Rescan()
{
    StopScan();
    m_scan_gen++;

    EnsureDirs();
    LoadRecent();
    LoadPlaytime();
    LoadFavorites();
    ClearTextures();
    m_entries.clear();

    const char *dir = g_config.general.games_dir;
    m_scanned_dir = dir ? dir : "";
    if (m_scanned_dir.empty()) {
        return;
    }

    std::error_code ec;
    std::filesystem::path directory = std::filesystem::u8path(m_scanned_dir);
    if (!std::filesystem::is_directory(directory, ec)) {
        return;
    }

    // Recursive: picks up images in the folder and any of its subfolders.
    for (const auto &file : std::filesystem::recursive_directory_iterator(
             directory,
             std::filesystem::directory_options::skip_permission_denied, ec)) {
        const auto &p = file.path();
        if (!std::filesystem::is_regular_file(p, ec)) {
            continue;
        }
        std::string ext = ToLower(p.extension().u8string());
        bool is_chd = ext == ".chd" && xemu_gameinfo_chd_supported();
        if (ext != ".iso" && ext != ".xiso" && !is_chd) {
            continue;
        }
        Entry e = {};
        e.path = p.u8string();
        e.filename = p.stem().u8string();
        m_entries.push_back(std::move(e));
    }

    std::sort(m_entries.begin(), m_entries.end(),
              [](const Entry &a, const Entry &b) {
                  return ToLower(a.filename) < ToLower(b.filename);
              });

    m_covers_fingerprint = CoversFingerprint();
    StartScan();
}

// ---------------------------------------------------------------------------
// UI
// ---------------------------------------------------------------------------

static void ChooseGamesFolder()
{
    const char *current = g_config.general.games_dir;
    ShowOpenFolderDialog(current && current[0] ? current : NULL,
                         [](const char *path) {
                             xemu_settings_set_string(
                                 &g_config.general.games_dir, path);
                         });
}

static void BootWithoutDisc()
{
    if (g_config.sys.files.dvd_path && g_config.sys.files.dvd_path[0]) {
        ActionEjectDisc();
    }
}

// A single game tile: rounded cover (or placeholder) aspect-fitted into a
// square, green ring on hover or gamepad/keyboard focus, gold star badge for
// favorites, centered caption (name + sub line) below. Returns true when
// clicked/activated.
static bool DrawGameTile(const char *id, GLuint tex, int tex_w, int tex_h,
                         const std::string &label, const std::string &sub,
                         const std::string &region, bool favorite,
                         bool has_snapshot, int num_versions, float cell,
                         float rounding)
{
    ImDrawList *dl = ImGui::GetWindowDrawList();
    float line_h = ImGui::GetTextLineHeight();
    float caption_h = line_h * 2.9f;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(id, ImVec2(cell, cell + caption_h),
                                          ImGuiButtonFlags_EnableNav);
    bool hovered = ImGui::IsItemHovered();
    bool focused = ImGui::IsItemFocused();

    ImVec2 c0 = pos;
    ImVec2 c1 = ImVec2(pos.x + cell, pos.y + cell);

    // Cover, aspect-fitted and centered in the square cell
    if (tex) {
        dl->AddRectFilled(c0, c1, IM_COL32(16, 16, 16, 255), rounding);
        ImVec2 i0 = c0, i1 = c1;
        if (tex_w > 0 && tex_h > 0 && tex_w != tex_h) {
            float ar = (float)tex_w / (float)tex_h;
            if (ar > 1.0f) {
                float ih = cell / ar;
                i0.y = pos.y + (cell - ih) * 0.5f;
                i1.y = i0.y + ih;
            } else {
                float iw = cell * ar;
                i0.x = pos.x + (cell - iw) * 0.5f;
                i1.x = i0.x + iw;
            }
        }
        dl->AddImageRounded((ImTextureID)(intptr_t)tex, i0, i1, ImVec2(0, 0),
                            ImVec2(1, 1), IM_COL32_WHITE, rounding);
    } else {
        dl->AddRectFilled(c0, c1, IM_COL32(38, 38, 38, 255), rounding);
        ImVec2 ts = ImGui::CalcTextSize(label.c_str(), NULL, false,
                                        cell * 0.85f);
        ImVec2 tp = ImVec2(pos.x + (cell - ts.x) * 0.5f,
                           pos.y + (cell - ts.y) * 0.5f);
        dl->AddText(NULL, 0.0f, tp, IM_COL32(190, 190, 190, 255),
                    label.c_str(), NULL, cell * 0.85f);
    }

    // Region badge in the top-left corner of the cover (favorite star sits
    // top-right): small dark pill with the region tag.
    if (!region.empty()) {
        ImFont *font = ImGui::GetFont();
        float fs = ImGui::GetFontSize() * 0.72f;
        ImVec2 ts = font->CalcTextSizeA(fs, 10000.0f, 0.0f, region.c_str());
        float pad = 4.0f * g_viewport_mgr.m_scale;
        float margin = 5.0f * g_viewport_mgr.m_scale;
        ImVec2 b0 = ImVec2(pos.x + margin, pos.y + margin);
        ImVec2 b1 = ImVec2(b0.x + ts.x + pad * 2, b0.y + ts.y + pad);
        dl->AddRectFilled(b0, b1, IM_COL32(0, 0, 0, 180),
                          rounding * 0.5f);
        dl->AddText(font, fs, ImVec2(b0.x + pad, b0.y + pad * 0.5f),
                    IM_COL32(220, 220, 220, 255), region.c_str());
    }

    // Multiple files share this title: version count in the bottom-left
    // corner (pick a specific version from the right-click menu).
    if (num_versions > 1) {
        char vbuf[16];
        snprintf(vbuf, sizeof(vbuf), "%d\xC3\x97", num_versions); // "2x"
        ImFont *font = ImGui::GetFont();
        float fs = ImGui::GetFontSize() * 0.72f;
        ImVec2 ts = font->CalcTextSizeA(fs, 10000.0f, 0.0f, vbuf);
        float pad = 4.0f * g_viewport_mgr.m_scale;
        float margin = 5.0f * g_viewport_mgr.m_scale;
        ImVec2 b0 = ImVec2(pos.x + margin, c1.y - ts.y - pad - margin);
        ImVec2 b1 = ImVec2(b0.x + ts.x + pad * 2, b0.y + ts.y + pad);
        dl->AddRectFilled(b0, b1, IM_COL32(0, 0, 0, 180), rounding * 0.5f);
        dl->AddText(font, fs, ImVec2(b0.x + pad, b0.y + pad * 0.5f),
                    IM_COL32(220, 220, 220, 255), vbuf);
    }

    // Snapshot badge in the bottom-right corner of the cover: this game has
    // at least one saved state to resume (right-click menu).
    if (has_snapshot) {
        ImVec2 ss = ImGui::CalcTextSize(ICON_FA_CLOCK_ROTATE_LEFT);
        ImVec2 sp = ImVec2(c1.x - ss.x - 6.0f * g_viewport_mgr.m_scale,
                           c1.y - ss.y - 5.0f * g_viewport_mgr.m_scale);
        dl->AddText(ImVec2(sp.x + 1, sp.y + 1), IM_COL32(0, 0, 0, 200),
                    ICON_FA_CLOCK_ROTATE_LEFT);
        dl->AddText(sp, IM_COL32(120, 220, 60, 255),
                    ICON_FA_CLOCK_ROTATE_LEFT);
    }

    // Gold star badge for favorites (with a soft shadow for contrast)
    if (favorite) {
        ImVec2 ss = ImGui::CalcTextSize(ICON_FA_STAR);
        ImVec2 sp = ImVec2(c1.x - ss.x - 6.0f * g_viewport_mgr.m_scale,
                           pos.y + 5.0f * g_viewport_mgr.m_scale);
        dl->AddText(ImVec2(sp.x + 1, sp.y + 1), IM_COL32(0, 0, 0, 200),
                    ICON_FA_STAR);
        dl->AddText(sp, STAR_COL, ICON_FA_STAR);
    }

    // Hover / focus treatment: brighten + green ring
    if (hovered || focused) {
        dl->AddRectFilled(c0, c1, IM_COL32(255, 255, 255, 18), rounding);
        dl->AddRect(c0, c1, ACCENT_COL, rounding, 0,
                    2.0f * g_viewport_mgr.m_scale);
    } else {
        dl->AddRect(c0, c1, IM_COL32(255, 255, 255, 14), rounding);
    }

    // Caption: name centered under the cover (ellipsis if too long), the
    // sub line in a dimmer tone below.
    ImVec2 ts = ImGui::CalcTextSize(label.c_str());
    float ty = c1.y + line_h * 0.35f;
    ImU32 cap_col = (hovered || focused) ? IM_COL32(255, 255, 255, 255) :
                                           IM_COL32(210, 210, 210, 255);
    if (ts.x <= cell) {
        dl->AddText(ImVec2(pos.x + (cell - ts.x) * 0.5f, ty), cap_col,
                    label.c_str());
    } else {
        ImGui::RenderTextEllipsis(dl, ImVec2(pos.x, ty),
                                  ImVec2(pos.x + cell, ty + ts.y), pos.x + cell,
                                  pos.x + cell, label.c_str(), NULL, &ts);
    }

    if (!sub.empty()) {
        ImVec2 ss = ImGui::CalcTextSize(sub.c_str());
        float sy = ty + line_h * 1.15f;
        if (ss.x <= cell) {
            dl->AddText(ImVec2(pos.x + (cell - ss.x) * 0.5f, sy),
                        IM_COL32(140, 140, 140, 255), sub.c_str());
        } else {
            // Too wide for the cell: clip with an ellipsis. Segment order
            // (id, rating, play time) keeps the rating visible when trimmed.
            dl->PushClipRect(ImVec2(pos.x, sy),
                             ImVec2(pos.x + cell, sy + ss.y), true);
            ImGui::RenderTextEllipsis(dl, ImVec2(pos.x, sy),
                                      ImVec2(pos.x + cell, sy + ss.y),
                                      pos.x + cell, pos.x + cell, sub.c_str(),
                                      NULL, &ss);
            dl->PopClipRect();
        }
    }

    return clicked;
}

static void SectionHeader(const char *text)
{
    ImGui::PushFont(g_font_mgr.m_menu_font_small);
    ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.75f, 1.0f), "%s", text);
    ImGui::PopFont();
    ImGui::Dummy(g_viewport_mgr.Scale(ImVec2(0, 4)));
}

void GameListWindow::Draw()
{
    TickPlaytime();

    if (!is_open) {
        // Dismissed without a choice: let the machine boot to the dashboard.
        ReleaseVM();
        m_was_open = false;
        return;
    }

    // On (re)open, pick up whatever was played since the library was last
    // shown: recents, play time and favorites are re-derived from the maps.
    if (!m_was_open) {
        m_was_open = true;
        m_focus_first = true; // give gamepad/keyboard nav an anchor
        m_view_mode = g_config.general.game_library_view ? 1 : 0;
        m_icon_size = CLAMP(g_config.general.game_library_icon_size,
                            96.0f, 280.0f);
        m_row_size = CLAMP(g_config.general.game_library_row_size,
                           24.0f, 96.0f);
        g_snapshot_mgr.Refresh();
        RefreshSnapshotFlags();
        uint64_t fp = CoversFingerprint();
        if (fp != m_covers_fingerprint) {
            m_covers_fingerprint = fp;
            Rescan();
        }
        RefreshEntryMeta();
    }

    ApplyScanResults();

    // While the launcher decides what to boot, keep the machine paused.
    if (m_hold_vm && runstate_is_running()) {
        vm_stop(RUN_STATE_PAUSED);
    }

    // Full-screen overlay below the menu bar; fixed, no chrome.
    ImGuiViewport *vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(vp->WorkSize, ImGuiCond_Always);

    // Style overrides apply to this Begin() only; popped right after so
    // popups opened from within (e.g. the tile context menu) keep the
    // regular style.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        g_viewport_mgr.Scale(ImVec2(38, 24)));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.05f, 0.97f));

    bool visible = ImGui::Begin(
        "##GameLibrary", &is_open,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
    if (!visible) {
        ImGui::End();
        return;
    }

    // Escape / gamepad B dismisses the launcher (boots to dashboard when the
    // machine was held at startup). While a popup (context menu) is open, the
    // same press must only close that popup. ImGui's nav-cancel already
    // closes it during NewFrame -- before we run -- so the check must use
    // last frame's popup state, not the current one.
    bool popup_open = ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopup);
    bool popup_just_closed = m_popup_was_open && !popup_open;
    m_popup_was_open = popup_open;
    if (!popup_open && !popup_just_closed &&
        (ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
         ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false))) {
        ReleaseVM();
        is_open = false;
        ImGui::End();
        return;
    }

    // Select / gamepad Back toggles between grid and list view.
    if (!popup_open &&
        ImGui::IsKeyPressed(ImGuiKey_GamepadBack, false)) {
        m_view_mode = m_view_mode ? 0 : 1;
        g_config.general.game_library_view = m_view_mode;
        m_focus_first = true;
    }

    // Rescan when the configured games directory changes.
    const char *dir = g_config.general.games_dir;
    if (m_scanned_dir != (dir ? dir : "")) {
        Rescan();
    }

    // ---- Header: title left, actions right -------------------------------
    ImGui::PushFont(g_font_mgr.m_menu_font_medium);
    ImGui::TextColored(ImVec4(0.24f, 0.60f, 0.00f, 1.0f),
                       ICON_FA_COMPACT_DISC);
    ImGui::SameLine();
    ImGui::TextUnformatted("Game Library");
    ImVec2 title_max = ImGui::GetItemRectMax();
    ImGui::PopFont();

    ImGui::PushFont(g_font_mgr.m_menu_font_small);
    const char *btn_boot = "Boot without Disc";
    const char *btn_folder = "Folder";
    const char *btn_rescan = "Rescan";
    ImGuiStyle &style = ImGui::GetStyle();
    float w_boot = ImGui::CalcTextSize(btn_boot).x + style.FramePadding.x * 2;
    float w_folder =
        ImGui::CalcTextSize(btn_folder).x + style.FramePadding.x * 2;
    float w_rescan =
        ImGui::CalcTextSize(btn_rescan).x + style.FramePadding.x * 2;
    float right = ImGui::GetWindowContentRegionMax().x;

    ImGui::SameLine(right - w_boot - w_folder - w_rescan -
                    style.ItemSpacing.x * 2);
    if (ImGui::Button(btn_folder)) {
        ChooseGamesFolder();
    }
    ImGui::SameLine();
    if (ImGui::Button(btn_rescan)) {
        Rescan();
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.34f, 0.02f, 1.0f));
    bool boot_no_disc = ImGui::Button(btn_boot);
    ImGui::PopStyleColor();
    ImGui::PopFont();

    if (boot_no_disc) {
        BootWithoutDisc();
        ReleaseVM();
        is_open = false;
        ImGui::End();
        return;
    }

    // Thin accent rule under the header
    {
        ImDrawList *dl = ImGui::GetWindowDrawList();
        float y = title_max.y + g_viewport_mgr.Scale(ImVec2(0, 10)).y;
        float x0 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMin().x;
        float x1 = ImGui::GetWindowPos().x + right;
        dl->AddRectFilled(ImVec2(x0, y), ImVec2(x1, y + 2.0f),
                          IM_COL32(61, 153, 0, 160));
        ImGui::Dummy(g_viewport_mgr.Scale(ImVec2(0, 18)));
    }

    // ---- No folder configured yet ----------------------------------------
    if (m_scanned_dir.empty()) {
        ImGui::Dummy(g_viewport_mgr.Scale(ImVec2(0, 60)));
        const char *msg = "No games folder is configured.";
        float tw = ImGui::CalcTextSize(msg).x;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - tw) * 0.5f);
        ImGui::TextUnformatted(msg);
        const char *pick = "Choose Games Folder...";
        float bw = ImGui::CalcTextSize(pick).x + style.FramePadding.x * 2;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - bw) * 0.5f);
        if (ImGui::Button(pick)) {
            ChooseGamesFolder();
        }
        ImGui::End();
        return;
    }

    // ---- Search + sort + view + size row -----------------------------------
    ImGui::SetNextItemWidth(g_viewport_mgr.Scale(ImVec2(320, 0)).x);
    ImGui::InputTextWithHint("##search", "Search games...", m_search,
                             sizeof(m_search));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(g_viewport_mgr.Scale(ImVec2(190, 0)).x);
    ImGui::Combo("##sort", &m_sort_mode,
                 "Sort: Name\0Sort: Recent\0Sort: Play Time\0Sort: Rating\0");

    // View mode buttons
    ImGui::SameLine();
    bool grid_active = m_view_mode == 0;
    ImGui::PushStyleColor(ImGuiCol_Button,
                          grid_active ? ImVec4(0.24f, 0.60f, 0.00f, 1.0f) :
                                        style.Colors[ImGuiCol_Button]);
    if (ImGui::Button(ICON_FA_TABLE_CELLS)) {
        m_view_mode = 0;
        g_config.general.game_library_view = 0;
        m_focus_first = true;
    }
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button,
                          !grid_active ? ImVec4(0.24f, 0.60f, 0.00f, 1.0f) :
                                         style.Colors[ImGuiCol_Button]);
    if (ImGui::Button(ICON_FA_LIST)) {
        m_view_mode = 1;
        g_config.general.game_library_view = 1;
        m_focus_first = true;
    }
    ImGui::PopStyleColor();

    ImGui::SameLine(right - g_viewport_mgr.Scale(ImVec2(180, 0)).x);
    ImGui::SetNextItemWidth(g_viewport_mgr.Scale(ImVec2(180, 0)).x);
    if (m_view_mode == 0) {
        if (ImGui::SliderFloat("##size", &m_icon_size, 96.0f, 280.0f,
                               "Size %.0f")) {
            g_config.general.game_library_icon_size = m_icon_size;
        }
    } else {
        if (ImGui::SliderFloat("##rowsize", &m_row_size, 24.0f, 96.0f,
                               "Size %.0f")) {
            g_config.general.game_library_row_size = m_row_size;
        }
    }
    ImGui::Dummy(g_viewport_mgr.Scale(ImVec2(0, 10)));

    if (m_entries.empty()) {
        ImGui::Dummy(g_viewport_mgr.Scale(ImVec2(0, 40)));
        const char *msg =
            "No .iso / .xiso / .chd disc images found in the games folder or "
            "its subfolders.";
        float tw = ImGui::CalcTextSize(msg).x;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - tw) * 0.5f);
        ImGui::TextUnformatted(msg);
        ImGui::End();
        return;
    }

    std::string filter = ToLower(m_search);

    // Display order: name (= scan order), most recently played, most play
    // time, or best rating. Ties keep the name order (stable sort).
    std::vector<size_t> order(m_entries.size());
    std::iota(order.begin(), order.end(), 0);
    if (m_sort_mode == 1) {
        std::stable_sort(order.begin(), order.end(),
                         [this](size_t a, size_t b) {
                             return m_entries[a].last_played >
                                    m_entries[b].last_played;
                         });
    } else if (m_sort_mode == 2) {
        std::stable_sort(order.begin(), order.end(),
                         [this](size_t a, size_t b) {
                             return m_entries[a].play_ms >
                                    m_entries[b].play_ms;
                         });
    } else if (m_sort_mode == 3) {
        // Rating order: rated first (E=4 ... AO=1 are all "playable" tiers,
        // higher = milder), unrated (0) last.
        auto rank = [this](size_t i) {
            uint32_t r = m_entries[i].rating;
            return r == 0 ? UINT32_MAX : UINT32_MAX - 1 - r;
        };
        std::stable_sort(order.begin(), order.end(),
                         [&](size_t a, size_t b) { return rank(a) < rank(b); });
    }

    // Apply the search filter once, up front.
    std::vector<size_t> visible_idx;
    std::vector<size_t> favorite_idx;
    for (size_t idx : order) {
        Entry &e = m_entries[idx];
        if (!filter.empty() &&
            e.search_key.find(filter) == std::string::npos) {
            continue;
        }
        visible_idx.push_back(idx);
        if (e.favorite) {
            favorite_idx.push_back(idx);
        }
    }

    // Group versions of the same game (same non-empty title id) for the
    // grid: one tile per game, individual files reachable via the context
    // menu. Files without a title id stay singletons.
    std::vector<std::vector<size_t>> groups;
    {
        std::map<std::string, size_t> by_title;
        for (size_t idx : visible_idx) {
            const std::string &tid = m_entries[idx].title_id;
            if (tid.empty()) {
                groups.push_back({idx});
                continue;
            }
            auto it = by_title.find(tid);
            if (it == by_title.end()) {
                by_title[tid] = groups.size();
                groups.push_back({idx});
            } else {
                groups[it->second].push_back(idx);
            }
        }
    }

    const Entry *pending_launch = nullptr;
    const Entry *pending_snap_entry = nullptr;
    std::string pending_snap_name;

    // Context menu shared by both views; anchored to the last item. Works on
    // a version group: one or more files that share a title id.
    auto TileContextMenu = [&](const std::vector<size_t> &members) {
        if (!ImGui::BeginPopupContextItem("tilectx")) {
            return;
        }
        if (ImGui::IsWindowAppearing()) {
            g_snapshot_mgr.Refresh();
            RefreshSnapshotFlags();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) {
            ImGui::CloseCurrentPopup();
        }

        if (members.size() > 1) {
            // Multiple versions: explicit start + favorite per file.
            for (size_t m : members) {
                Entry &v = m_entries[m];
                ImGui::PushID((int)m);
                char buf[320];
                snprintf(buf, sizeof(buf), "Start '%s'",
                         RecentKey(v.path).c_str());
                if (ImGui::MenuItem(buf)) {
                    pending_launch = &v;
                }
                ImGui::PopID();
            }
            ImGui::Separator();
            for (size_t m : members) {
                Entry &v = m_entries[m];
                ImGui::PushID((int)(m + 0x10000));
                char buf[320];
                snprintf(buf, sizeof(buf), ICON_FA_STAR " %s",
                         RecentKey(v.path).c_str());
                if (ImGui::MenuItem(buf, NULL, v.favorite)) {
                    ToggleFavorite(v);
                }
                ImGui::PopID();
            }
        } else {
            Entry &e = m_entries[members[0]];
            if (ImGui::MenuItem(e.favorite ?
                                    ICON_FA_STAR " Remove from Favorites" :
                                    ICON_FA_STAR " Add to Favorites")) {
                ToggleFavorite(e);
            }
        }
        ImGui::Separator();

        bool any = false;
        for (int s = 0; s < g_snapshot_mgr.m_snapshots_len; s++) {
            const XemuSnapshotData &xd = g_snapshot_mgr.m_extra_data[s];
            Entry *snap_e = nullptr; // resume against the matching version
            for (size_t m : members) {
                Entry &v = m_entries[m];
                if (SnapshotMatches(xd, v.path, v.title)) {
                    snap_e = &v;
                    break;
                }
            }
            if (!snap_e) {
                continue;
            }
            any = true;
            const QEMUSnapshotInfo &si = g_snapshot_mgr.m_snapshots[s];
            char when[32] = "";
            time_t t = (time_t)si.date_sec;
            struct tm *tm = localtime(&t);
            if (tm) {
                strftime(when, sizeof(when), "%d.%m.%Y %H:%M", tm);
            }
            char item[384];
            snprintf(item, sizeof(item), "Resume '%s'  (%s)", si.name, when);
            if (ImGui::MenuItem(item)) {
                pending_snap_entry = snap_e;
                pending_snap_name = si.name;
            }
            // Preview: show the snapshot's screenshot while hovering or
            // when focused via controller navigation.
            if (xd.gl_thumbnail &&
                (ImGui::IsItemHovered() || ImGui::IsItemFocused())) {
                ImGui::BeginTooltip();
                ImVec2 sz = g_viewport_mgr.Scale(
                    ImVec2(2 * XEMU_SNAPSHOT_THUMBNAIL_WIDTH,
                           2 * XEMU_SNAPSHOT_THUMBNAIL_HEIGHT));
                ImGui::Image((ImTextureID)(intptr_t)xd.gl_thumbnail, sz);
                ImGui::EndTooltip();
            }
        }
        if (!any) {
            ImGui::MenuItem("No snapshots for this game", NULL, false, false);
        }
        ImGui::EndPopup();
    };

    ImGui::BeginChild("##content", ImVec2(0, 0), false);

    if (m_view_mode == 0) {
        // ---- Cover grid, horizontally centered, favorites row on top ------
        float cell = g_viewport_mgr.Scale(ImVec2(m_icon_size, 0)).x;
        float spacing = g_viewport_mgr.Scale(ImVec2(22, 0)).x;
        float avail = ImGui::GetContentRegionAvail().x;
        int cols = std::max(1, (int)((avail + spacing) / (cell + spacing)));
        float grid_w = cols * cell + (cols - 1) * spacing;
        float indent = std::max(0.0f, (avail - grid_w) * 0.5f);
        float rounding = 8.0f * g_viewport_mgr.m_scale;

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                            ImVec2(spacing, spacing));

        auto DrawGrid = [&](const std::vector<std::vector<size_t>> &grps) {
            int col = 0;
            for (const auto &members : grps) {
                // Representative: the most recently played version.
                size_t idx = members[0];
                for (size_t m : members) {
                    if (m_entries[m].last_played >
                        m_entries[idx].last_played) {
                        idx = m;
                    }
                }
                Entry &e = m_entries[idx];
                bool any_fav = false, any_snap = false;
                for (size_t m : members) {
                    any_fav = any_fav || m_entries[m].favorite;
                    any_snap = any_snap || m_entries[m].has_snapshot;
                }

                if (col == 0) {
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
                }
                if (m_focus_first) {
                    ImGui::SetKeyboardFocusHere();
                    m_focus_first = false;
                }

                ImGui::PushID((int)idx);
                const std::string &label =
                    e.title.empty() ? e.filename : e.title;
                std::string sub = e.title_id;
                std::string played = FormatPlaytime(e.play_ms);
                if (!played.empty()) {
                    if (!sub.empty()) {
                        sub += "  \xC2\xB7  ";
                    }
                    sub += played;
                }
                bool tile_clicked =
                    DrawGameTile("tile", e.tex, e.tex_w, e.tex_h, label, sub,
                                 e.region_label, any_fav, any_snap,
                                 (int)members.size(), cell, rounding);
                // Gamepad Y doubles as ImGui's nav "input" activation, which
                // would also trigger the button: Y means favorite, not start.
                if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp, false)) {
                    tile_clicked = false;
                }
                if (tile_clicked) {
                    pending_launch = &e;
                }
                // Gamepad: Y toggles favorite, X opens the context menu on
                // the focused tile (A activates = start, B closes above).
                if (ImGui::IsItemFocused()) {
                    if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp, false)) {
                        ToggleFavorite(e);
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceLeft, false)) {
                        ImGui::OpenPopup("tilectx");
                    }
                }
                TileContextMenu(members);
                ImGui::PopID();

                if (++col % cols != 0) {
                    ImGui::SameLine();
                } else {
                    col = 0;
                }
            }
            if (col != 0) {
                ImGui::NewLine();
            }
        };

        if (!favorite_idx.empty()) {
            SectionHeader(ICON_FA_STAR " Favorites");
            // Favorites are explicit per-file picks: no grouping here.
            std::vector<std::vector<size_t>> fav_groups;
            for (size_t idx : favorite_idx) {
                fav_groups.push_back({idx});
            }
            // Sections may show the same entry twice; scope the IDs.
            ImGui::PushID("fav");
            DrawGrid(fav_groups);
            ImGui::PopID();
            ImGui::Dummy(g_viewport_mgr.Scale(ImVec2(0, 14)));
            SectionHeader("All Games");
        }
        ImGui::PushID("all");
        DrawGrid(groups);
        ImGui::PopID();

        ImGui::PopStyleVar();
    } else {
        // ---- List view: favorites pinned on top ---------------------------
        std::vector<size_t> list_idx = visible_idx;
        std::stable_sort(list_idx.begin(), list_idx.end(),
                         [this](size_t a, size_t b) {
                             return m_entries[a].favorite >
                                    m_entries[b].favorite;
                         });

        float thumb = g_viewport_mgr.Scale(ImVec2(m_row_size, 0)).x;
        // Larger, condensed font for better readability in the table (it
        // also carries the icon glyphs used for the favorite star).
        ImGui::PushFont(g_font_mgr.m_menu_font_small);
        if (ImGui::BeginTable("##gamelist", 7,
                              ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_BordersInnerH |
                                  ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed,
                                    thumb + 8);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Title ID",
                                    ImGuiTableColumnFlags_WidthFixed,
                                    g_viewport_mgr.Scale(ImVec2(110, 0)).x);
            ImGui::TableSetupColumn("Region",
                                    ImGuiTableColumnFlags_WidthFixed,
                                    g_viewport_mgr.Scale(ImVec2(90, 0)).x);
            ImGui::TableSetupColumn("Rating",
                                    ImGuiTableColumnFlags_WidthFixed,
                                    g_viewport_mgr.Scale(ImVec2(160, 0)).x);
            ImGui::TableSetupColumn("Play Time",
                                    ImGuiTableColumnFlags_WidthFixed,
                                    g_viewport_mgr.Scale(ImVec2(150, 0)).x);
            ImGui::TableSetupColumn("Last Played",
                                    ImGuiTableColumnFlags_WidthFixed,
                                    g_viewport_mgr.Scale(ImVec2(120, 0)).x);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            for (size_t idx : list_idx) {
                Entry &e = m_entries[idx];
                ImGui::PushID((int)idx);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                if (e.tex) {
                    float ar = (e.tex_w > 0 && e.tex_h > 0) ?
                                   (float)e.tex_w / e.tex_h :
                                   1.0f;
                    ImVec2 sz = ar >= 1.0f ? ImVec2(thumb, thumb / ar) :
                                             ImVec2(thumb * ar, thumb);
                    ImGui::Image((ImTextureID)(intptr_t)e.tex, sz);
                } else {
                    ImGui::Dummy(ImVec2(thumb, thumb));
                }

                ImGui::TableSetColumnIndex(1);
                if (m_focus_first) {
                    ImGui::SetKeyboardFocusHere();
                    m_focus_first = false;
                }
                bool clicked = ImGui::Selectable(
                    "##row", false,
                    ImGuiSelectableFlags_SpanAllColumns,
                    ImVec2(0, thumb));
                // See grid: Y = favorite must not count as row activation.
                if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp, false)) {
                    clicked = false;
                }
                if (ImGui::IsItemFocused()) {
                    if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp, false)) {
                        ToggleFavorite(e);
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceLeft, false)) {
                        ImGui::OpenPopup("tilectx");
                    }
                }
                TileContextMenu({idx});
                if (clicked) {
                    pending_launch = &e;
                }
                ImGui::SameLine();
                if (e.favorite) {
                    ImGui::PushStyleColor(ImGuiCol_Text,
                                          ImVec4(1.0f, 0.78f, 0.16f, 1.0f));
                    ImGui::TextUnformatted(ICON_FA_STAR);
                    ImGui::PopStyleColor();
                    ImGui::SameLine();
                }
                ImGui::TextUnformatted(
                    e.title.empty() ? e.filename.c_str() : e.title.c_str());
                if (e.has_snapshot) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.47f, 0.86f, 0.24f, 1.0f),
                                       ICON_FA_CLOCK_ROTATE_LEFT);
                }

                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(e.title_id.c_str());

                ImGui::TableSetColumnIndex(3);
                const std::string &region_lbl = e.region_label;
                if (!region_lbl.empty()) {
                    ImGui::TextUnformatted(region_lbl.c_str());
                } else {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "-");
                }

                ImGui::TableSetColumnIndex(4);
                // Non-US discs (PAL/JP) leave the ESRB field unset; show a
                // dash so the column reads as intentional rather than blank.
                const char *rating_lbl = EsrbRatingLabel(e.rating);
                if (rating_lbl[0]) {
                    ImGui::TextUnformatted(rating_lbl);
                } else {
                    // Plain ASCII dash: the UI font has no em-dash glyph.
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "-");
                }

                ImGui::TableSetColumnIndex(5);
                ImGui::TextUnformatted(FormatPlaytime(e.play_ms).c_str());

                ImGui::TableSetColumnIndex(6);
                if (e.last_played) {
                    char when[32] = "";
                    time_t t = (time_t)e.last_played;
                    struct tm *tm = localtime(&t);
                    if (tm) {
                        strftime(when, sizeof(when), "%d.%m.%Y", tm);
                    }
                    ImGui::TextUnformatted(when);
                }

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::PopFont();
    }

    ImGui::EndChild();
    ImGui::End();

    if (pending_launch) {
        RecordPlayed(*pending_launch);
        StartPlaySession(*pending_launch);
        std::string path = pending_launch->path;
        ActionLoadDiscFile(path.c_str());
        ReleaseVM();
        is_open = false;
    } else if (pending_snap_entry) {
        // Insert the right disc first so the snapshot loads without the
        // disc-mismatch dialog, then restore.
        RecordPlayed(*pending_snap_entry);
        StartPlaySession(*pending_snap_entry);
        ActionLoadDiscFile(pending_snap_entry->path.c_str());
        ReleaseVM();
        g_snapshot_mgr.LoadSnapshotChecked(pending_snap_name.c_str());
        is_open = false;
    }
}
