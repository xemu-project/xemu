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

#define MAX_RECENT_DISCS 10

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
        if (file_path && file_path[0]) {
            if (!g_config.general.history.discs) {
                g_config.general.history.discs = g_new0(const char *, 1);
                g_config.general.history.discs_count = 0;
            }

            int i;
            for (i = 0; i < g_config.general.history.discs_count; i++) {
                if (g_strcmp0(g_config.general.history.discs[i], file_path) == 0) {
                    for (int j = i; j > 0; j--) {
                        g_config.general.history.discs[j] = g_config.general.history.discs[j-1];
                    }
                    g_config.general.history.discs[0] = g_strdup(file_path);
                    return;
                }
            }

            if (i == g_config.general.history.discs_count) {
                if (g_config.general.history.discs_count >= MAX_RECENT_DISCS) {
                    for (int j = MAX_RECENT_DISCS; j < g_config.general.history.discs_count; j++) {
                        g_free((void*)g_config.general.history.discs[j]);
                    }
                    g_config.general.history.discs_count = MAX_RECENT_DISCS;
                } else {
                    const char **new_discs = g_renew(const char *, g_config.general.history.discs, 
                                               g_config.general.history.discs_count + 1);
                    g_config.general.history.discs = new_discs;
                    g_config.general.history.discs_count++;
                }

                for (int i = g_config.general.history.discs_count - 1; i > 0; i--) {
                    g_config.general.history.discs[i] = g_config.general.history.discs[i-1];
                }
                g_config.general.history.discs[0] = g_strdup(file_path);
            }
        }
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

void ActionLoadDiscFromHistory(int index)
{
    g_assert(index >= 0 && index < g_config.general.history.discs_count);

    if (index < 0 || index >= g_config.general.history.discs_count) {
        return;
    }

    const char *file_path = g_config.general.history.discs[index];
    if (!file_path || !file_path[0]) {
        return;
    }

    if (qemu_access(file_path, F_OK) == -1) {
        g_free((void*)g_config.general.history.discs[index]);
        for (int j = index; j < g_config.general.history.discs_count - 1; j++) {
            g_config.general.history.discs[j] = g_config.general.history.discs[j + 1];
        }
        g_config.general.history.discs_count--;
        if (g_config.general.history.discs_count == 0) {
            g_free(g_config.general.history.discs);
            g_config.general.history.discs = NULL;
        }
        return;
    }

    ActionLoadDiscFile(file_path);
}

void ActionClearDiscHistory(void)
{
    for (int i = 0; i < g_config.general.history.discs_count; i++) {
        g_free((void*)g_config.general.history.discs[i]);
    }
    g_free(g_config.general.history.discs);
    g_config.general.history.discs = NULL;
    g_config.general.history.discs_count = 0;
}
