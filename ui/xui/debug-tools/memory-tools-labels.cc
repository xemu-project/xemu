//
// xemu Memory Viewer / Search / x86 Debugger
//
// Copyright (C) 2026 xemu contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#include "memory-tools.hh"
#include "memory-tools-internal.hh"
#include "current-game.hh"
#include "cheat-engine-memory.h"
#include "cheat-engine.hh"
#include "../font-manager.hh"
#include "../misc.hh"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <utility>

#include <glib.h>
#include <glib/gstdio.h>

using namespace xemu_memory_tools_internal;

void MemoryToolsWindow::DumpLabels()
{
    const auto &database = current_game_manager.Labels();
    if (database.labels.empty()) {
        m_label_status = "No XBE labels are available for the current game.";
        m_debug_status = m_label_status;
        return;
    }

    XemuDebugGuestPauseGuard guest_pause;

    const std::string directory = DumpDirectory();
    if (directory.empty()) {
        m_label_status = "Could not determine the xemu label dump directory.";
        m_debug_status = m_label_status;
        return;
    }
    if (g_mkdir_with_parents(directory.c_str(), 0755) != 0) {
        m_label_status = "Could not create label dump directory: " + directory;
        m_debug_status = m_label_status;
        return;
    }

    const std::string filename = DumpStem() + "-LABELS.txt";
    gchar *path_c = g_build_filename(directory.c_str(), filename.c_str(), nullptr);
    const std::string path = path_c ? path_c : filename;
    g_free(path_c);

    FILE *fp = g_fopen(path.c_str(), "wb");
    if (fp == nullptr) {
        m_label_status = "Could not create label dump file: " + path;
        m_debug_status = m_label_status;
        return;
    }

    const auto &game = current_game_manager.Get();
    std::fprintf(fp, "Xemu XBE Labels\n");
    std::fprintf(fp, "Title: %s\n",
                 game.title_name.empty() ? "<unknown>" : game.title_name.c_str());
    std::fprintf(fp, "Title ID: %s\n",
                 CurrentGameManager::FormatTitleId(game.title_id).c_str());
    std::fprintf(fp, "Header SHA-256: %s\n", game.header_sha256.c_str());
    std::fprintf(fp, "default.xbe SHA-256: %s\n\n",
                 game.disc_xbe_sha256.c_str());
    std::fprintf(fp, "VIRTUAL    PHYSICAL   TYPE       SOURCE   CONFIDENCE  LOCATION                 LABEL\n");
    std::fprintf(fp, "-------------------------------------------------------------------------------------------------------------\n");

    const bool can_translate = xemu_cheat_prepare_virtual_map() != 0;
    size_t mapped = 0;
    for (const XemuXbeLabels::Label &label : database.labels) {
        uint64_t physical = 0;
        const bool physical_valid = can_translate &&
            xemu_cheat_virtual_to_physical(label.virtual_address, &physical) != 0;
        if (physical_valid) {
            ++mapped;
            const std::string location = label.has_section_location
                ? label.section_name + "+" + [&]() {
                      char off[9];
                      std::snprintf(off, sizeof(off), "%08X", label.section_offset);
                      return std::string(off);
                  }()
                : std::string("@VA");
            std::fprintf(fp, "%08X   %08llX   %-10s %-8s %-11s %-24s %s\n",
                         label.virtual_address,
                         (unsigned long long)physical,
                         XemuXbeLabels::TypeName(label.type),
                         XemuXbeLabels::SourceName(label.source),
                         XemuXbeLabels::ConfidenceName(label.confidence),
                         location.c_str(), label.name.c_str());
        } else {
            const std::string location = label.has_section_location
                ? label.section_name + "+" + [&]() {
                      char off[9];
                      std::snprintf(off, sizeof(off), "%08X", label.section_offset);
                      return std::string(off);
                  }()
                : std::string("@VA");
            std::fprintf(fp, "%08X   UNMAPPED   %-10s %-8s %-11s %-24s %s\n",
                         label.virtual_address,
                         XemuXbeLabels::TypeName(label.type),
                         XemuXbeLabels::SourceName(label.source),
                         XemuXbeLabels::ConfidenceName(label.confidence),
                         location.c_str(), label.name.c_str());
        }
    }
    const bool write_ok = std::fclose(fp) == 0;

    /* Preserve the historical resume boundary: file creation/translation is
     * transactional, while status-string/UI bookkeeping happens after resume. */
    guest_pause.Resume();

    char status[640];
    if (write_ok) {
        std::snprintf(status, sizeof(status),
                      "Dumped %zu XBE labels (%zu currently mapped) to: %s",
                      database.labels.size(), mapped, path.c_str());
    } else {
        std::snprintf(status, sizeof(status),
                      "Label dump write failed while closing: %s", path.c_str());
    }
    m_label_status = status;
    m_debug_status = m_label_status;
}

// Labels rendering/UI is owned by memory-tools-labels-ui.cc.
