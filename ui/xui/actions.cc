//
// xemu User Interface
//
// Copyright (C) 2020-2022 Matt Borgerson
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
#include "common.hh"
#include "actions.hh"
#include "misc.hh"
#include "xemu-hud.h"
#include "../xemu-snapshots.h"
#include "../xemu-notifications.h"
#include "snapshot-manager.hh"

#define MAX_RECENT_DISCS 11

void ActionEjectDisc(void)
{
    Error *err = NULL;
    xemu_eject_disc(&err);
    if (err) {
        xemu_queue_error_message(error_get_pretty(err));
        error_free(err);
    }
}

void ActionLoadDisc(void)
{
    const char *iso_file_filters =
        "Disc Image Files (*.iso, *.xiso)\0*.iso;*.xiso\0All Files\0*.*\0";
    const char *new_disc_path =
        PausedFileOpen(NOC_FILE_DIALOG_OPEN, iso_file_filters,
                       g_config.sys.files.dvd_path, NULL);
    if (new_disc_path == NULL) {
        /* Cancelled */
        return;
    }

    ActionLoadDiscFile(new_disc_path);
}

void ActionLoadDiscFile(const char *file_path)
{
    Error *err = NULL;
    xemu_load_disc(file_path, &err);

    if (err) {
        xemu_queue_error_message(error_get_pretty(err));
        error_free(err);
    } else {
        if (!g_config.general.recent.discs) {
            g_config.general.recent.discs = g_new0(const char *, 1);
            g_config.general.recent.discs_count = 0;
        }

        // If current game is already in history,
        // move other game entries down,
        // then move the current game to the most recent slot.
        for (unsigned i = 0; i < g_config.general.recent.discs_count; i++) {
            if (g_strcmp0(g_config.general.recent.discs[i], file_path) == 0) {
                const char *current_path = g_config.general.recent.discs[i];
                for (unsigned int j = i; j > 0; j--) {
                    g_config.general.recent.discs[j] =
                        g_config.general.recent.discs[j - 1];
                }
                g_config.general.recent.discs[0] = current_path;
                return;
            }
        }
        // Free a slot for our entry.
        if (g_config.general.recent.discs_count >= MAX_RECENT_DISCS) {
            for (unsigned i = MAX_RECENT_DISCS;
                 i < g_config.general.recent.discs_count; i++) {
                g_free((void *)g_config.general.recent.discs[i]);
            }

            g_free((void *)g_config.general.recent.discs[MAX_RECENT_DISCS - 1]);
            g_config.general.recent.discs_count = MAX_RECENT_DISCS;
        } else {
            // Allocate new space.
            const char **new_discs =
                g_renew(const char *, g_config.general.recent.discs,
                        g_config.general.recent.discs_count + 1);
            g_config.general.recent.discs = new_discs;
            g_config.general.recent.discs_count++;
        }

        for (unsigned i = g_config.general.recent.discs_count - 1; i > 0; i--) {
            g_config.general.recent.discs[i] =
                g_config.general.recent.discs[i - 1];
        }
        g_config.general.recent.discs[0] = g_strdup(file_path);
    }
}

void ActionTogglePause(void)
{
    if (runstate_is_running()) {
        vm_stop(RUN_STATE_PAUSED);
    } else {
        vm_start();
    }
}

void ActionReset(void)
{
    qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
}

void ActionShutdown(void)
{
    qemu_system_shutdown_request(SHUTDOWN_CAUSE_HOST_UI);
}

void ActionScreenshot(void)
{
    g_screenshot_pending = true;
}

void ActionActivateBoundSnapshot(int slot, bool save)
{
    assert(slot < 4 && slot >= 0);
    const char *snapshot_name = *(g_snapshot_shortcut_index_key_map[slot]);
    if (!snapshot_name || !(snapshot_name[0])) {
        char *msg = g_strdup_printf("F%d is not bound to a snapshot", slot + 5);
        xemu_queue_notification(msg);
        g_free(msg);
        return;
    }

    Error *err = NULL;
    if (save) {
        xemu_snapshots_save(snapshot_name, &err);
    } else {
        ActionLoadSnapshotChecked(snapshot_name);
    }

    if (err) {
        xemu_queue_error_message(error_get_pretty(err));
        error_free(err);
    }
}

void ActionLoadSnapshotChecked(const char *name)
{
    g_snapshot_mgr.LoadSnapshotChecked(name);
}

void ActionClearDiscRecent(void)
{
    for (unsigned i = 0; i < g_config.general.recent.discs_count; i++) {
        g_free((void *)g_config.general.recent.discs[i]);
    }
    g_free(g_config.general.recent.discs);
    g_config.general.recent.discs = NULL;
    g_config.general.recent.discs_count = 0;
}
