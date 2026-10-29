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
#include <filesystem>

static const unsigned MAX_RECENT_DISCS = 10;

static void config_stringlist_init(const char ***list_ptr, unsigned *list_count)
{
    *list_count = 0;
    *list_ptr = g_new0(const char *, 1);
}

static void config_stringlist_free(const char **list, unsigned list_count)
{
    for (unsigned i = 0; i < list_count; i++) {
        g_free((void *)list[i]);
    }
    g_free(list);
}

static void config_stringlist_push(const char ***list_ptr, unsigned *list_count,
                                   const char *new_entry)
{
    const char **list = *list_ptr;
    const unsigned cur_count = *list_count;

    // If target file is already in history, move to top
    for (unsigned i = 0; i < cur_count; i++) {
        if (g_strcmp0(list[i], new_entry) == 0) {
            const char *current_path = list[i];
            for (unsigned int j = i; j > 0; j--) {
                list[j] = list[j - 1];
            }
            list[0] = current_path;
            return;
        }
    }

    // Free a slot for our entry
    unsigned new_count;
    if (cur_count >= MAX_RECENT_DISCS) {
        for (unsigned i = MAX_RECENT_DISCS; i < cur_count; i++) {
            g_free((void *)list[i]);
        }

        g_free((void *)list[MAX_RECENT_DISCS - 1]);
        new_count = MAX_RECENT_DISCS;
    } else {
        // Allocate new space
        new_count = cur_count + 1;
        const char **new_discs = g_renew(const char *, list, new_count);
        list = new_discs;
        *list_ptr = new_discs;
    }

    *list_count = new_count;

    for (unsigned i = new_count - 1; i > 0; i--) {
        list[i] = list[i - 1];
    }
    list[0] = g_strdup(new_entry);
}

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
    static const SDL_DialogFileFilter filters[] = {
        { "Disc Image Files (*.iso, *.xiso)", "iso;xiso" }, { "All Files", "*" }
    };
    const char *default_path = g_config.sys.files.dvd_path;
    if (!default_path || !default_path[0]) {
        default_path = g_config.general.games_dir;
    }
    ShowOpenFileDialog(filters, 2, default_path,
                       [](const char *path) { ActionLoadDiscFile(path); });
}

void ActionLoadDiscFile(const char *file_path)
{
    Error *err = NULL;
    xemu_load_disc(file_path, &err);

    if (err) {
        xemu_queue_error_message(error_get_pretty(err));
        error_free(err);
    } else {
        const char *games_dir = g_config.general.games_dir;
        if (!games_dir || !games_dir[0]) {
            std::string dir =
                std::filesystem::path(file_path).parent_path().string();
            xemu_settings_set_string(&g_config.general.games_dir, dir.c_str());
        }
    }

    if (!g_config.general.recent.discs) {
        config_stringlist_init(&g_config.general.recent.discs,
                               &g_config.general.recent.discs_count);
    }

    config_stringlist_push(&g_config.general.recent.discs,
                           &g_config.general.recent.discs_count, file_path);
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
    config_stringlist_free(g_config.general.recent.discs,
                           g_config.general.recent.discs_count);
    g_config.general.recent.discs = NULL;
    g_config.general.recent.discs_count = 0;
}
