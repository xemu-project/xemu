//
// xemu RAW Cheat Engine
//
// Copyright (C) 2026 xemu contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#include "cheat-engine.hh"
#include "guest-pause-guard.hh"
#include "current-game.hh"
#include "../font-manager.hh"
#include "../misc.hh"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <sstream>

#include <glib.h>
#include <glib/gstdio.h>

#include "cheat-engine-memory.h"
#include "system/runstate.h"

CheatEngineWindow cheat_engine_window;

static const std::string kPreEntryPrefix = ":PREENTRY:";

bool CheatEngineWindow::ConsumePreEntryPrefix(std::string &spec)
{
    if (Upper(spec).rfind(kPreEntryPrefix, 0) != 0) {
        return false;
    }
    spec = Trim(spec.substr(kPreEntryPrefix.size()));
    return true;
}

static const char *kInitialSource = R"CHEATS(; xemu RAW Cheat Engine / CMP-style code file
; Current-game files are loaded from the "Cheats" folder beside xemu.exe.
; The filename can be readable (for example EA009E-<hash>.txt), but the
; ^1/^2 header is authoritative when matching a running game.
;
; Accepted GameID forms are equivalent:
;   ^2 = GameID: EA009E
;   ^2 = GameID: 4541009E
;
; Groups use CMP-style nesting:
;   !Group Name:
;       +Cheat Name{Optional description}
;       %Credits: Author>Another Author
;       $XXXXXXXX YYYYYYYY
;   !!
;
; The cheat list starts Disabled. Press the Disabled/Enabled button to enable
; live selection; after that, clicking a cheat activates/deactivates it immediately.
;
; Prefix a patch as +:PREENTRY:Cheat Name (preferred), or put :PREENTRY:
; immediately before a +Cheat block / directly after its +Cheat line before RAW
; lines. PREENTRY blocks live on Patch and apply once on the next startup/reset.
;
; Supported RAW memory types: 0/1/2/3/4/5/6/7, custom stackable 9,
; custom A raw-byte fill/write, D/E conditions, and F0 assembly/F1 raw external x86 code caves. A leading '$' on RAW lines is accepted.
;
; Default address space is VIRTUAL RAM. Ordinary codes use the literal guest
; virtual address encoded in the RAW line. Type 9 is only needed to select an
; explicit base/context such as physical RAM or pointer-based addressing.

^1 = Hash: 0000000000000000
^2 = GameID: EA009E
^3 = NAME: Example Game

!Player Codes:
+Example - Disabled{This is only a format example.}
%Credits: Skiller
$20000120 000003E7
$10000130 00000064
$00000140 000000FF
!!
)CHEATS";

CheatEngineWindow::CheatEngineWindow()
    : m_source(kInitialSource)
{
    ParseSource(false);
}

std::string CheatEngineWindow::Trim(const std::string &s)
{
    size_t first = 0;
    while (first < s.size() && std::isspace((unsigned char)s[first])) {
        ++first;
    }
    size_t last = s.size();
    while (last > first && std::isspace((unsigned char)s[last - 1])) {
        --last;
    }
    return s.substr(first, last - first);
}

bool CheatEngineWindow::ParseCodeLine(const std::string &input, RawCode &out,
                                      int source_line)
{
    std::string line = Trim(input);
    if (line.empty()) {
        return false;
    }

    if (line[0] == '$') {
        line.erase(0, 1);
        line = Trim(line);
    }

    // Remove trailing C/C++ or shell-style comments. Semicolon comments are
    // accepted only when they start the line so hex parsing stays unambiguous.
    size_t comment = line.find("//");
    if (comment != std::string::npos) {
        line.erase(comment);
    }
    comment = line.find('#');
    if (comment != std::string::npos) {
        line.erase(comment);
    }
    line = Trim(line);

    unsigned long long command = 0;
    unsigned long long value = 0;
    char extra = 0;
    int fields = std::sscanf(line.c_str(), "%8llx %8llx %c", &command, &value,
                             &extra);
    if (fields != 2 || command > 0xFFFFFFFFull || value > 0xFFFFFFFFull) {
        return false;
    }

    out.command = (uint32_t)command;
    out.value = (uint32_t)value;
    out.source_line = source_line;
    out.source = Trim(input);
    return true;
}

std::string CheatEngineWindow::Upper(const std::string &s)
{
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return (char)std::toupper(c);
    });
    return out;
}

std::string CheatEngineWindow::NormalizeTypeFLine(const std::string &line)
{
    std::string normalized = Trim(line);
    if (!normalized.empty() && normalized[0] == '$') {
        normalized = Trim(normalized.substr(1));
    }
    return normalized;
}

std::string CheatEngineWindow::TypeFDirective(const std::string &line)
{
    std::string directive = line;
    size_t cut = directive.size();
    size_t comment = directive.find(';');
    if (comment != std::string::npos) {
        cut = std::min(cut, comment);
    }
    comment = directive.find("//");
    if (comment != std::string::npos) {
        cut = std::min(cut, comment);
    }
    comment = directive.find('#');
    if (comment != std::string::npos) {
        cut = std::min(cut, comment);
    }
    directive.erase(cut);
    return Trim(directive);
}

bool CheatEngineWindow::ParseDeadcodeCount(const std::string &text,
                                           uint8_t &out)
{
    if (text.size() != 8) {
        return false;
    }
    uint32_t value = 0;
    for (char c : text) {
        unsigned char u = (unsigned char)c;
        int nibble = -1;
        if (u >= '0' && u <= '9') {
            nibble = u - '0';
        } else {
            u = (unsigned char)std::toupper(u);
            if (u >= 'A' && u <= 'F') {
                nibble = u - 'A' + 10;
            }
        }
        if (nibble < 0) {
            return false;
        }
        value = (value << 4) | (uint32_t)nibble;
    }
    if (value < 1u || value > 8u) {
        return false;
    }
    out = (uint8_t)value;
    return true;
}

bool CheatEngineWindow::ParseGameId(const std::string &text,
                                    uint32_t &title_id)
{
    std::string value = Upper(Trim(text));
    if (value.size() == 8) {
        char *end = nullptr;
        errno = 0;
        unsigned long parsed = std::strtoul(value.c_str(), &end, 16);
        if (errno == 0 && end && *end == '\0' && parsed <= 0xFFFFFFFFul) {
            title_id = (uint32_t)parsed;
            return true;
        }
        return false;
    }

    if (value.size() == 6 &&
        std::isxdigit((unsigned char)value[2]) &&
        std::isxdigit((unsigned char)value[3]) &&
        std::isxdigit((unsigned char)value[4]) &&
        std::isxdigit((unsigned char)value[5])) {
        char *end = nullptr;
        unsigned long game = std::strtoul(value.c_str() + 2, &end, 16);
        if (!end || *end != '\0' || game > 0xFFFFul) {
            return false;
        }
        title_id = ((uint32_t)(uint8_t)value[0] << 24) |
                   ((uint32_t)(uint8_t)value[1] << 16) |
                   (uint32_t)game;
        return true;
    }
    return false;
}

bool CheatEngineWindow::ParseHeader(const std::string &source, FileHeader &out)
{
    out = FileHeader{};
    std::istringstream stream(source);
    std::string line;
    int inspected = 0;
    while (std::getline(stream, line) && inspected < 64) {
        std::string t = Trim(line);
        if (t.empty() || t[0] == ';') {
            continue;
        }
        ++inspected;
        if (t.empty() || t[0] != '^') {
            // Header fields are expected at the head, but allow comments and
            // formatting around them. Once actual cheat/group data begins the
            // header scan is complete.
            if (t[0] == '+' || t[0] == '!' || t[0] == '$' ||
                std::isxdigit((unsigned char)t[0])) {
                break;
            }
            continue;
        }

        size_t eq = t.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string lhs = Upper(Trim(t.substr(0, eq)));
        std::string rhs = Trim(t.substr(eq + 1));
        size_t colon = rhs.find(':');
        std::string key = colon == std::string::npos
                              ? std::string()
                              : Upper(Trim(rhs.substr(0, colon)));
        std::string value = colon == std::string::npos
                                ? rhs
                                : Trim(rhs.substr(colon + 1));

        if (lhs == "^1" && (key.empty() || key == "HASH")) {
            out.hash = Upper(value);
        } else if (lhs == "^2" &&
                   (key.empty() || key == "GAMEID" || key == "GAME ID")) {
            out.game_id = Upper(value);
            out.title_id_valid = ParseGameId(value, out.title_id);
        } else if (lhs == "^3" && (key.empty() || key == "NAME")) {
            out.name = value;
        }
    }
    return !out.hash.empty() || !out.game_id.empty() || !out.name.empty();
}

bool CheatEngineWindow::HashMatches(const std::string &file_hash,
                                    const std::string &current_hash)
{
    std::string a = Upper(Trim(file_hash));
    std::string b = Upper(Trim(current_hash));
    if (a.rfind("0X", 0) == 0) {
        a.erase(0, 2);
    }
    if (a.empty() || b.empty() || a.size() > b.size()) {
        return false;
    }
    for (char c : a) {
        if (!std::isxdigit((unsigned char)c)) {
            return false;
        }
    }
    return b.compare(0, a.size(), a) == 0;
}

std::string CheatEngineWindow::CheatDirectory() const
{
    char executable_dir[4096] = {};
    if (!xemu_cheat_get_executable_dir(executable_dir, sizeof(executable_dir))) {
        // Do not fall back to the xemu settings/AppData path. A relative
        // Cheats path is only an emergency fallback if executable discovery
        // is unavailable during an unusual startup/error state.
        return "Cheats";
    }

    gchar *path = g_build_filename(executable_dir, "Cheats", nullptr);
    std::string result = path ? path : "Cheats";
    g_free(path);
    return result;
}

bool CheatEngineWindow::OpenPathExternally(const std::string &path)
{
    GError *error = nullptr;
    gchar *absolute = g_canonicalize_filename(path.c_str(), nullptr);
    gchar *uri = absolute ? g_filename_to_uri(absolute, nullptr, &error) : nullptr;
    if (!uri) {
        m_file_status = "Unable to open path: ";
        m_file_status += error ? error->message : path;
        if (error) {
            g_error_free(error);
        }
        g_free(absolute);
        return false;
    }

    const bool ok = SDL_OpenURL(uri);
    if (!ok) {
        m_file_status = "Unable to open path: ";
        m_file_status += SDL_GetError();
    }
    g_free(uri);
    g_free(absolute);
    return ok;
}

std::string CheatEngineWindow::SuggestedCurrentCheatPath() const
{
    const auto &game = current_game_manager.Get();
    if (!game.valid) {
        return {};
    }

    const std::string id =
        Upper(CurrentGameManager::FormatDatabaseGameId(game.title_id));
    const std::string hash = game.header_sha256.size() >= 16
                                 ? Upper(game.header_sha256.substr(0, 16))
                                 : Upper(game.header_sha256);
    const std::string filename = id + "-" + hash + ".txt";
    const std::string dir = CheatDirectory();
    gchar *path = g_build_filename(dir.c_str(), filename.c_str(), nullptr);
    std::string result = path ? path : filename;
    g_free(path);
    return result;
}

bool CheatEngineWindow::OpenCheatDirectory()
{
    const std::string dir = CheatDirectory();
    if (g_mkdir_with_parents(dir.c_str(), 0755) != 0) {
        m_file_status = "Could not create/open cheat folder: " + dir;
        return false;
    }
    return OpenPathExternally(dir);
}

bool CheatEngineWindow::ReloadCurrentCheatFile()
{
    if (!m_loaded_path.empty() && g_file_test(m_loaded_path.c_str(), G_FILE_TEST_IS_REGULAR)) {
        DisableAllCheats(true);
        return LoadSourceFile(m_loaded_path);
    }

    m_seen_game_generation = current_game_manager.Generation();
    DisableAllCheats(true);
    return LoadMatchingCurrentGameFile(true);
}

bool CheatEngineWindow::EditOrCreateCurrentCheatFile()
{
    std::string path = m_loaded_path;
    if (path.empty()) {
        path = SuggestedCurrentCheatPath();
    }
    if (path.empty()) {
        m_file_status = "No running XBE detected; cannot create a current-game cheat file.";
        return false;
    }

    const std::string dir = CheatDirectory();
    if (g_mkdir_with_parents(dir.c_str(), 0755) != 0) {
        m_file_status = "Could not create/open cheat folder: " + dir;
        return false;
    }

    if (!g_file_test(path.c_str(), G_FILE_TEST_IS_REGULAR)) {
        const auto &game = current_game_manager.Get();
        if (!game.valid) {
            m_file_status = "No running XBE detected; cannot create a current-game cheat file.";
            return false;
        }

        std::ostringstream source;
        source << "^1 = Hash: " << game.header_sha256 << "\n";
        source << "^2 = GameID: "
               << CurrentGameManager::FormatDatabaseGameId(game.title_id) << "\n";
        source << "^3 = NAME: "
               << (game.title_name.empty() ? "Unknown Game" : game.title_name)
               << "\n\n";
        source << "; Add CMP-style groups and +Cheat blocks below.\n";

        GError *error = nullptr;
        if (!g_file_set_contents(path.c_str(), source.str().c_str(), -1, &error)) {
            m_file_status = "Unable to create cheat file: ";
            m_file_status += error ? error->message : path;
            if (error) {
                g_error_free(error);
            }
            return false;
        }
        LoadSourceFile(path);
    }

    return OpenPathExternally(path);
}

bool CheatEngineWindow::LoadSourceFile(const std::string &path)
{
    gchar *contents = nullptr;
    gsize length = 0;
    GError *error = nullptr;
    if (!g_file_get_contents(path.c_str(), &contents, &length, &error)) {
        m_file_status = "Unable to read cheat file: ";
        m_file_status += error ? error->message : path;
        if (error) {
            g_error_free(error);
        }
        return false;
    }

    m_source.assign(contents, length);
    g_free(contents);
    m_loaded_path = path;
    ParseSource(false);
    m_file_status = "Loaded: " + path;
    return true;
}

bool CheatEngineWindow::LoadMatchingCurrentGameFile(bool force_reload)
{
    const auto &game = current_game_manager.Get();
    if (!game.valid) {
        DeactivateAllFHooks();
        for (auto &block : m_blocks) {
            block.enabled = false;
        }
        m_switches.clear();
        m_loaded_path.clear();
        m_blocks.clear();
        m_groups.clear();
        m_groups.push_back(CheatGroup{"", {}, {}});
        m_file_status = "No running XBE detected; no code file loaded.";
        return false;
    }

    const std::string dir = CheatDirectory();
    if (g_mkdir_with_parents(dir.c_str(), 0755) != 0) {
        m_file_status = "Could not create/open cheat folder: " + dir;
        return false;
    }

    const std::string short_id =
        Upper(CurrentGameManager::FormatDatabaseGameId(game.title_id));
    const std::string full_id = Upper(CurrentGameManager::FormatTitleId(game.title_id));

    /* Matching only needs the CMP header. Avoid loading every candidate's full
     * cheat body just to inspect the first few authoritative fields. */
    auto read_header = [&](const std::string &path, FileHeader &header) -> bool {
        constexpr size_t kMaxHeaderBytes = 64u * 1024u;
        FILE *input = g_fopen(path.c_str(), "rb");
        if (!input) {
            return false;
        }

        std::string prefix(kMaxHeaderBytes, '\0');
        const size_t bytes_read =
            std::fread(prefix.data(), 1, prefix.size(), input);
        const bool read_failed = std::ferror(input) != 0;
        std::fclose(input);
        if (read_failed) {
            return false;
        }
        prefix.resize(bytes_read);
        return ParseHeader(prefix, header);
    };

    auto header_matches = [&](const FileHeader &header) {
        return header.title_id_valid && header.title_id == game.title_id &&
               HashMatches(header.hash, game.header_sha256);
    };

    /* Same-title reset/reload fast path: the already-loaded file is the most
     * likely match. Validate its authoritative header before doing a directory
     * scan; a stale file from another title simply falls through. */
    if (force_reload && !m_loaded_path.empty() &&
        g_file_test(m_loaded_path.c_str(), G_FILE_TEST_IS_REGULAR)) {
        FileHeader loaded_header;
        if (read_header(m_loaded_path, loaded_header) &&
            header_matches(loaded_header)) {
            return LoadSourceFile(m_loaded_path);
        }
    }

    GError *dir_error = nullptr;
    GDir *handle = g_dir_open(dir.c_str(), 0, &dir_error);
    if (!handle) {
        m_file_status = "Could not open cheat folder: ";
        m_file_status += dir_error ? dir_error->message : dir;
        if (dir_error) {
            g_error_free(dir_error);
        }
        return false;
    }

    std::vector<std::string> preferred_names;
    std::vector<std::string> fallback_names;
    const gchar *name = nullptr;
    while ((name = g_dir_read_name(handle)) != nullptr) {
        std::string filename(name);
        if (filename.size() < 4 ||
            g_ascii_strcasecmp(filename.c_str() + filename.size() - 4,
                               ".txt") != 0) {
            continue;
        }
        const std::string upper_name = Upper(filename);
        if (upper_name.rfind(short_id + "-", 0) == 0 ||
            upper_name.rfind(full_id + "-", 0) == 0) {
            preferred_names.push_back(std::move(filename));
        } else {
            fallback_names.push_back(std::move(filename));
        }
    }
    g_dir_close(handle);

    std::string best_path;
    std::string best_filename;
    size_t best_score = 0;
    size_t matching_count = 0;
    auto filename_before = [](const std::string &a, const std::string &b) {
        const int folded = g_ascii_strcasecmp(a.c_str(), b.c_str());
        return folded < 0 || (folded == 0 && a < b);
    };
    auto consider_names = [&](const std::vector<std::string> &names) {
        for (const std::string &filename : names) {
            gchar *full = g_build_filename(dir.c_str(), filename.c_str(), nullptr);
            if (!full) {
                continue;
            }
            std::string path(full);
            g_free(full);

            FileHeader header;
            if (!read_header(path, header) || !header_matches(header)) {
                continue;
            }

            ++matching_count;
            size_t score = header.hash.size();
            const std::string upper_name = Upper(filename);
            if (upper_name.rfind(short_id + "-", 0) == 0) {
                score += 256;
            } else if (upper_name.rfind(full_id + "-", 0) == 0) {
                score += 128;
            }
            if (best_path.empty() || score > best_score ||
                (score == best_score && filename_before(filename, best_filename))) {
                best_path = std::move(path);
                best_filename = filename;
                best_score = score;
            }
        }
    };
    consider_names(preferred_names);
    consider_names(fallback_names);

    if (best_path.empty()) {
        DeactivateAllFHooks();
        for (auto &block : m_blocks) {
            block.enabled = false;
        }
        m_switches.clear();
        m_loaded_path.clear();
        m_blocks.clear();
        m_groups.clear();
        m_groups.push_back(CheatGroup{"", {}, {}});

        const std::string hash = game.header_sha256.size() >= 16
                                     ? game.header_sha256.substr(0, 16)
                                     : game.header_sha256;
        m_file_status = "No matching code file. Suggested filename: " +
                        short_id + "-" + Upper(hash) + ".txt";
        return false;
    }

    if (!force_reload && best_path == m_loaded_path) {
        if (matching_count > 1) {
            m_file_status = "Multiple matching code files (" +
                            std::to_string(matching_count) + "); using: " +
                            best_path;
        }
        return true;
    }
    const bool loaded = LoadSourceFile(best_path);
    if (loaded && matching_count > 1) {
        m_file_status = "Multiple matching code files (" +
                        std::to_string(matching_count) + "); loaded: " +
                        best_path;
    }
    return loaded;
}

void CheatEngineWindow::MaybeAutoLoadCurrentGame()
{
    const uint64_t generation = current_game_manager.Generation();
    if (generation == m_seen_game_generation) {
        return;
    }

    const bool was_game_valid = m_seen_game_valid;
    const auto &game = current_game_manager.Get();
    const bool game_valid = game.valid;
    const std::string game_identity = game_valid
        ? Upper(CurrentGameManager::FormatTitleId(game.title_id)) + ":" +
              Upper(game.header_sha256)
        : std::string();
    const bool game_identity_changed =
        game_valid && (!was_game_valid || game_identity != m_seen_game_identity);
    const bool guest_ownership_changed =
        m_force_forget_f_hooks_on_next_game_observation ||
        (was_game_valid &&
         (!game_valid || game_identity != m_seen_game_identity));

    /* Always observe Current Game identity, even while automatic code-file
     * loading is disabled. Otherwise turning Auto-load back on mid-game can
     * manufacture a false invalid->valid startup edge and run PREENTRY late.
     * Only a previously observed guest ending/changing (or an explicit reset)
     * invalidates hook/cave ownership. An invalid->valid edge can simply mean
     * XBE metadata became available after a debugger CodeCave was installed;
     * clearing ownership there would strand its JMP while zeroing the cave.
     * Never restore old Type-F bytes across a confirmed guest boundary: forget
     * ownership first, then clear only live-cheat execution/selection state.
     * PREENTRY selections are session metadata and intentionally survive game
     * changes. */
    if (guest_ownership_changed) {
        ForgetFHookOwnershipForNewGuest();
    }
    m_force_forget_f_hooks_on_next_game_observation = false;
    for (auto &block : m_blocks) {
        if (!block.preentry) {
            block.enabled = false;
            block.selected = false;
        }
    }
    m_switches.clear();
    m_last_runtime_message.clear();
    m_seen_game_generation = generation;
    m_seen_game_valid = game_valid;
    m_seen_game_identity = game_identity;
    if (!game_valid) {
        m_last_preentry_message.clear();
    }

    if (!m_auto_load_current_game) {
        return;
    }

    const bool loaded = LoadMatchingCurrentGameFile(true);

    /* PREENTRY patches are one-shot startup work. Checkbox changes only stage
     * the next startup. Never let the ordinary game-valid edge compete with an
     * explicit Reset that is already being tracked by the lifecycle state. */
    if (loaded && game_identity_changed &&
        m_preentry_lifecycle == PreEntryLifecycle::Idle) {
        m_preentry_lifecycle = PreEntryLifecycle::ApplyPending;
    }
}

void CheatEngineWindow::NotifyGameResetRequested()
{
    /* ActionReset calls this immediately before qemu_system_reset_request().
     * Do not require a transient no-XBE window: same-title resets can keep the
     * copied XBE metadata valid for the entire reset. */
    m_preentry_lifecycle = PreEntryLifecycle::ResetRequested;
}

void CheatEngineWindow::ObserveRequestedGameReset()
{
    if (m_preentry_lifecycle != PreEntryLifecycle::ResetRequested) {
        return;
    }

    /* qemu_system_reset_request() leaves a reset request pending until QEMU's
     * main loop consumes it and performs qemu_system_reset(). Because Notify is
     * called immediately before that request is made, seeing no pending reset
     * on a later Tick is the durable completion edge even when XBE metadata
     * never disappears. If Reset was converted to shutdown, never arm PREENTRY. */
    if (qemu_reset_requested_get() != SHUTDOWN_CAUSE_NONE) {
        return;
    }
    if (qemu_shutdown_requested_get() != SHUTDOWN_CAUSE_NONE) {
        m_preentry_lifecycle = PreEntryLifecycle::Idle;
        return;
    }

    m_preentry_lifecycle = PreEntryLifecycle::ApplyPending;
    for (auto &block : m_blocks) {
        if (block.preentry) {
            block.preentry_applied = false;
        }
    }

    /* Force the normal current-game reload path once after an explicit reset.
     * This invalidates stale Type-F/cave ownership from the pre-reset guest and
     * reloads the same-title file without relying on an XBE identity change. */
    m_force_forget_f_hooks_on_next_game_observation = true;
    current_game_manager.RefreshRunningXbe(true);
    m_seen_game_generation = UINT64_MAX;
}

std::string CheatEngineWindow::BlockIdentityKey(const CheatBlock &block) const
{
    return m_loaded_path + "\n" + block.group_path + "\n" + block.name +
           "\n" + std::to_string(block.identity_ordinal);
}

std::string CheatEngineWindow::PreEntrySelectionKey(
    const CheatBlock &block) const
{
    return BlockIdentityKey(block);
}

bool CheatEngineWindow::IsPreEntrySelected(const CheatBlock &block) const
{
    return m_selected_preentry_keys.find(PreEntrySelectionKey(block)) !=
           m_selected_preentry_keys.end();
}

void CheatEngineWindow::RememberPreEntrySelection(CheatBlock &block)
{
    if (!block.preentry) {
        return;
    }

    const std::string key = PreEntrySelectionKey(block);
    if (block.selected) {
        m_selected_preentry_keys.insert(key);
    } else {
        m_selected_preentry_keys.erase(key);
        block.preentry_error.clear();
    }
}

void CheatEngineWindow::ApplySelectedPreEntryPatches()
{
    if (m_preentry_lifecycle != PreEntryLifecycle::ApplyPending) {
        return;
    }

    /* CPU/XBE readiness is transient during startup, so keep the one-shot
     * pending instead of losing PREENTRY just because the first frame was too
     * early. The global Engine Enabled option remains the master safety gate,
     * but the Cheats-tab Enabled/Disabled button is intentionally irrelevant. */
    if (!current_game_manager.HasGame() || m_loaded_path.empty() ||
        !xemu_cheat_cpu_available()) {
        return;
    }
    if (!m_engine_enabled) {
        m_preentry_lifecycle = PreEntryLifecycle::Idle;
        return;
    }

    std::string saved_live_message = std::move(m_last_runtime_message);
    std::string first_error;
    for (size_t i = 0; i < m_blocks.size(); ++i) {
        CheatBlock &block = m_blocks[i];
        if (!block.preentry || !block.selected || block.preentry_applied) {
            continue;
        }

        block.preentry_error.clear();
        m_last_runtime_message.clear();
        ExecuteBlock(i, block);
        if (m_last_runtime_message.empty()) {
            block.preentry_applied = true;
        } else {
            block.preentry_error = m_last_runtime_message;
            if (first_error.empty()) {
                first_error = block.name + ": " + block.preentry_error;
            }
        }
    }
    m_last_preentry_message = std::move(first_error);
    m_last_runtime_message = std::move(saved_live_message);
    m_preentry_lifecycle = PreEntryLifecycle::Idle;
}

void CheatEngineWindow::ParseSource(bool preserve_states)
{
    /* Keep deactivated Type-F hook metadata across a source reload so the
     * known hook/original-byte relationship remains associated with this XBE.
     * DeactivateAllFHooks() restores hooks and safely returns retired cave
     * blocks to the external arena whenever the CPU is no longer executing
     * them. Current-game changes still clear this table completely. */
    DeactivateAllFHooks();
    m_last_preentry_message.clear();
    std::unordered_map<std::string, bool> selected_by_identity;
    std::unordered_map<std::string, bool> enabled_by_identity;
    if (preserve_states) {
        selected_by_identity.reserve(m_blocks.size());
        enabled_by_identity.reserve(m_blocks.size());
        for (const auto &block : m_blocks) {
            /* PREENTRY selection has its own session persistence. Live-cheat
             * reload state uses the same duplicate-safe block identity. */
            if (block.preentry) {
                continue;
            }
            selected_by_identity[BlockIdentityKey(block)] = block.selected;
            enabled_by_identity[BlockIdentityKey(block)] = block.enabled;
        }
    }

    m_blocks.clear();
    m_groups.clear();
    m_groups.push_back(CheatGroup{"", {}, {}}); // invisible root
    m_parse_messages.clear();
    m_switches.clear();

    std::vector<int> group_stack{0};
    group_stack.reserve(8);
    CheatBlock *current = nullptr;
    bool next_block_preentry = false;
    std::istringstream stream(m_source);
    std::string line;
    int line_number = 0;
    std::unordered_map<std::string, uint32_t> occurrence_by_base;

    /* Type-F executable and post-DEADCODE data lines intentionally share
     * the same normalization/comment rules through the helpers above. */

    auto new_block = [&](const std::string &spec) -> CheatBlock * {
        CheatBlock block;
        std::string label = Trim(spec);
        size_t open = label.find('{');
        size_t close = label.rfind('}');
        if (open != std::string::npos && close != std::string::npos &&
            close > open) {
            block.name = Trim(label.substr(0, open));
            block.description = Trim(label.substr(open + 1, close - open - 1));
        } else {
            block.name = label;
        }
        if (block.name.empty()) {
            block.name = "RAW Codes";
        }
        block.preentry = next_block_preentry;
        next_block_preentry = false;
        block.group_index = group_stack.back();
        for (size_t depth = 1; depth < group_stack.size(); ++depth) {
            if (!block.group_path.empty()) {
                block.group_path += "/";
            }
            block.group_path += m_groups[(size_t)group_stack[depth]].name;
        }
        const std::string occurrence_base = block.group_path + "\n" + block.name;
        block.identity_ordinal = occurrence_by_base[occurrence_base]++;
        if (preserve_states) {
            const std::string identity = BlockIdentityKey(block);
            auto sit = selected_by_identity.find(identity);
            if (sit != selected_by_identity.end()) {
                block.selected = sit->second;
            }
            auto eit = enabled_by_identity.find(identity);
            if (eit != enabled_by_identity.end()) {
                block.enabled = eit->second;
            }
        }
        if (block.preentry) {
            block.enabled = false;
            block.selected = IsPreEntrySelected(block);
        }
        m_blocks.push_back(std::move(block));
        return &m_blocks.back();
    };

    while (std::getline(stream, line)) {
        ++line_number;
        std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == ';') {
            continue;
        }

        if (trimmed[0] == '^') {
            continue;
        }

        if (Upper(trimmed) == kPreEntryPrefix) {
            /* PREENTRY is deliberately block-scoped instead of a sticky file
             * mode: putting it before a +Cheat marks the next block, while
             * placing it immediately after +Cheat marks that still-empty
             * current block. A directive after RAW lines is malformed and is
             * ignored rather than silently reclassifying the following cheat. */
            if (current && current->codes.empty()) {
                current->preentry = true;
                current->enabled = false;
                current->selected = IsPreEntrySelected(*current);
            } else if (current) {
                m_parse_messages.emplace_back(
                    ":PREENTRY: on line " + std::to_string(line_number) +
                    " appears after RAW lines and was ignored; place it before "
                    "+Cheat or use +:PREENTRY:Name.");
            } else {
                next_block_preentry = true;
            }
            continue;
        }

        if (trimmed.rfind("!!", 0) == 0) {
            if (group_stack.size() > 1) {
                group_stack.pop_back();
            } else {
                m_parse_messages.emplace_back(
                    "Group close (!!) had no matching open group on line " +
                    std::to_string(line_number));
            }
            current = nullptr;
            continue;
        }

        if (trimmed[0] == '!') {
            std::string group_name = Trim(trimmed.substr(1));
            if (!group_name.empty() && group_name.back() == ':') {
                group_name.pop_back();
                group_name = Trim(group_name);
            }
            if (!group_name.empty()) {
                CheatGroup group;
                group.name = group_name;
                int index = (int)m_groups.size();
                m_groups.push_back(std::move(group));
                m_groups[(size_t)group_stack.back()].child_groups.push_back(index);
                group_stack.push_back(index);
            }
            current = nullptr;
            continue;
        }

        if (trimmed[0] == '+') {
            std::string spec = Trim(trimmed.substr(1));
            if (ConsumePreEntryPrefix(spec)) {
                next_block_preentry = true;
            }
            current = new_block(spec);
            continue;
        }

        if (trimmed[0] == '{') {
            if (current) {
                size_t close = trimmed.rfind('}');
                std::string desc = close != std::string::npos && close > 0
                                       ? trimmed.substr(1, close - 1)
                                       : trimmed.substr(1);
                if (!current->description.empty()) {
                    current->description += " ";
                }
                current->description += Trim(desc);
            }
            continue;
        }

        if (trimmed[0] == '%') {
            std::string upper = Upper(trimmed);
            const std::string prefix = "%CREDITS:";
            if (current && upper.rfind(prefix, 0) == 0) {
                current->credits = Trim(trimmed.substr(prefix.size()));
            }
            continue;
        }

        RawCode code;
        if (ParseCodeLine(line, code, line_number)) {
            if (!current) {
                current = new_block("RAW Codes");
            }

            /* Type-F source blocks are self-contained:
             *
             *   F0000000 AAAAAAAA   assembly source
             *   ...
             *   DEADCODE
             *
             *   F1000000 AAAAAAAA   aligned literal hex bytes
             *   XXXXXXXX YYYYYYYY
             *   ...
             *   DEADCODE 000000NN   NN=01..08 valid bytes in final line
             *
             * A leading '$' is optional on every Type-F body/directive line.
             * The body is captured as text instead of being fed through the
             * ordinary RAW-pair parser. DEADCODE is syntax, never literal
             * DE AD C0 DE bytes. */
            const uint32_t type = code.command >> 28;
            const uint32_t f_subtype = (code.command >> 24) & 0xFu;
            if (type == 0xF && (f_subtype == 0x0 || f_subtype == 0x1)) {
                bool terminated = false;
                std::string body_line;
                while (std::getline(stream, body_line)) {
                    ++line_number;
                    std::string normalized = NormalizeTypeFLine(body_line);
                    std::string directive = TypeFDirective(normalized);

                    std::istringstream directive_tokens(directive);
                    std::string directive_name;
                    directive_tokens >> directive_name;
                    if (Upper(directive_name) == "DEADCODE") {
                        terminated = true;
                        std::string arg;
                        std::string extra;
                        directive_tokens >> arg;
                        directive_tokens >> extra;

                        if (f_subtype == 0x0) {
                            if (!arg.empty() || !extra.empty()) {
                                m_parse_messages.emplace_back(
                                    "Type-F0 DEADCODE must not have an operand on line " +
                                    std::to_string(line_number) + ".");
                            }
                        } else {
                            if (!ParseDeadcodeCount(arg, code.f_final_valid_bytes) ||
                                !extra.empty()) {
                                code.f_final_valid_bytes = 0;
                                m_parse_messages.emplace_back(
                                    "Type-F1 must end with DEADCODE 000000NN where NN is 01-08 (line " +
                                    std::to_string(line_number) + ").");
                            }
                        }

                        /* F0 static DD data may be written after DEADCODE for
                         * readability. DEADCODE still marks the end of executable
                         * source; only label/DD declarations are absorbed here and
                         * the assembler physically places them after the generated
                         * return JMP. Rewind as soon as the next real cheat/source
                         * line is encountered so the outer parser sees it normally. */
                        if (f_subtype == 0x0) {
                            while (true) {
                                const std::streampos candidate_pos = stream.tellg();
                                const int before_candidate_line = line_number;
                                std::string data_line;
                                if (!std::getline(stream, data_line)) {
                                    break;
                                }
                                ++line_number;

                                std::string data_norm = NormalizeTypeFLine(data_line);
                                std::string data_directive =
                                    TypeFDirective(data_norm);

                                if (data_directive.empty()) {
                                    continue;
                                }

                                std::string after_label = data_directive;
                                const size_t data_colon = after_label.find(':');
                                bool label_only = false;
                                if (data_colon != std::string::npos) {
                                    std::string label_name =
                                        Trim(after_label.substr(0, data_colon));
                                    label_only = !label_name.empty() &&
                                        Trim(after_label.substr(data_colon + 1)).empty();
                                    after_label =
                                        Trim(after_label.substr(data_colon + 1));
                                }
                                std::istringstream data_tokens(after_label);
                                std::string data_name;
                                data_tokens >> data_name;
                                const bool is_dd = Upper(data_name) == "DD";

                                if (label_only || is_dd) {
                                    code.f_body.push_back(
                                        XemuCheatAsmLine{line_number, data_norm});
                                    continue;
                                }

                                if (candidate_pos != std::streampos(-1)) {
                                    stream.clear();
                                    stream.seekg(candidate_pos);
                                    line_number = before_candidate_line;
                                }
                                break;
                            }
                        }
                        break;
                    }

                    if (directive.empty()) {
                        continue;
                    }
                    code.f_body.push_back(XemuCheatAsmLine{line_number, normalized});
                }
                code.f_terminated = terminated;
                if (!terminated) {
                    m_parse_messages.emplace_back(
                        "Type-F block starting on line " +
                        std::to_string(code.source_line) +
                        " was not terminated with DEADCODE.");
                }
            }

            if ((code.command >> 28) == 0xFu &&
                (((code.command >> 24) & 0xFu) == 0x0u ||
                 ((code.command >> 24) & 0xFu) == 0x1u)) {
                PrecompileTypeF(code);
            }
            current->codes.push_back(std::move(code));
            continue;
        }

        bool looks_hex = !trimmed.empty() &&
                         std::isxdigit((unsigned char)trimmed[0]);
        if (looks_hex) {
            char msg[256];
            std::snprintf(msg, sizeof(msg),
                          "Line %d was not a valid RAW pair: %s",
                          line_number, trimmed.c_str());
            m_parse_messages.emplace_back(msg);
        }
    }

    if (group_stack.size() > 1) {
        m_parse_messages.emplace_back(
            "One or more groups were not closed with !! before end of file.");
    }

    // Remove empty blocks, then rebuild each group's cheat index list.
    std::vector<CheatBlock> kept;
    kept.reserve(m_blocks.size());
    for (auto &block : m_blocks) {
        if (!block.codes.empty()) {
            kept.push_back(std::move(block));
        }
    }
    m_blocks = std::move(kept);
    for (auto &group : m_groups) {
        group.cheats.clear();
    }
    for (size_t i = 0; i < m_blocks.size(); ++i) {
        int group = m_blocks[i].group_index;
        if (group < 0 || (size_t)group >= m_groups.size()) {
            group = 0;
            m_blocks[i].group_index = 0;
        }
        m_groups[(size_t)group].cheats.push_back(i);
    }

    /* Drop unreachable PREENTRY selection identities for the file just parsed.
     * Keep selections belonging to other games/files so switching titles does
     * not forget the user's staged startup patches. */
    if (!m_loaded_path.empty()) {
        std::unordered_set<std::string> valid_preentry_keys;
        for (const CheatBlock &block : m_blocks) {
            if (block.preentry) {
                valid_preentry_keys.insert(PreEntrySelectionKey(block));
            }
        }
        const std::string file_prefix = m_loaded_path + "\n";
        for (auto it = m_selected_preentry_keys.begin();
             it != m_selected_preentry_keys.end();) {
            const bool belongs_to_loaded_file =
                it->compare(0, file_prefix.size(), file_prefix) == 0;
            if (belongs_to_loaded_file &&
                valid_preentry_keys.find(*it) == valid_preentry_keys.end()) {
                it = m_selected_preentry_keys.erase(it);
            } else {
                ++it;
            }
        }
    }

    if (m_blocks.empty()) {
        m_parse_messages.emplace_back("No RAW code lines were found.");
    }
    InvalidateFTempBankCache();
}

