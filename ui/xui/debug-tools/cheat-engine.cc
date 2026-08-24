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

/* Keep the historical Type-F local name so the behavior-bearing hook methods
 * remain source-identical while sharing the generic Debug Tools pause owner. */
using TypeFGuestPauseGuard = XemuDebugGuestPauseGuard;

// UI/frontend methods are owned by cheat-engine-ui.cc.
// Per-frame coordinator; parser, F-hook lifecycle, and RAW execution live in
// dedicated Cheat Engine translation units.

void CheatEngineWindow::Tick()
{
    ObserveRequestedGameReset();
    MaybeAutoLoadCurrentGame();

    const bool cpu_available = xemu_cheat_cpu_available() != 0;
    if (!cpu_available) {
        return;
    }

    ApplySelectedPreEntryPatches();
    ReleaseRetiredFHooks();

    /* Deactivate normal cheat-owned F hooks with one map scan and one guest
     * pause window. The old path scanned the entire F-hook table once for
     * every cheat block on every 10 Hz tick. Debugger-owned CodeCave hooks
     * use kDebuggerFHookOwner and intentionally remain independent of the
     * Cheat Engine's global/live checkboxes. */
    const bool run_live_blocks = m_engine_enabled && m_live_cheats_enabled;
    m_f_deactivate_scratch.clear();
    for (const auto &entry : m_f_hooks) {
        const size_t owner = entry.second.owner_block;
        if (owner >= m_blocks.size()) {
            continue;
        }
        const CheatBlock &owner_block = m_blocks[owner];
        const bool should_deactivate = owner_block.preentry
                                           ? !owner_block.preentry_applied
                                           : (!run_live_blocks ||
                                              !owner_block.enabled);
        if (should_deactivate) {
            if (entry.second.installed || FHookHasTrackedEntries(entry.second)) {
                m_f_deactivate_scratch.emplace_back(owner, entry.first);
            }
        }
    }
    if (!m_f_deactivate_scratch.empty()) {
        /* Preserve the old block-by-block deactivation order while retaining
         * one hook-table scan. stable_sort keeps the unordered-map iteration
         * order unchanged for hooks that belong to the same block. */
        std::stable_sort(
            m_f_deactivate_scratch.begin(), m_f_deactivate_scratch.end(),
            [](const auto &a, const auto &b) { return a.first < b.first; });
        TypeFGuestPauseGuard guest_pause;
        for (const auto &pending : m_f_deactivate_scratch) {
            DeactivateFHook(pending.second);
        }
    }

    if (!run_live_blocks) {
        return;
    }

    m_last_runtime_message.clear();
    for (size_t i = 0; i < m_blocks.size(); ++i) {
        if (!m_blocks[i].preentry && m_blocks[i].enabled) {
            ExecuteBlock(i, m_blocks[i]);
        }
    }
}

