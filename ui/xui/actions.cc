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
#include "misc.hh"
#include "xemu-hud.h"

void ActionEjectDisc(void)
{
    xemu_settings_set_string(&g_config.sys.files.dvd_path, "");
    xemu_eject_disc();
}

void ActionLoadDisc(void)
{
    const char *iso_file_filters = ".iso Files\0*.iso\0All Files\0*.*\0";
    const char *new_disc_path =
        PausedFileOpen(NOC_FILE_DIALOG_OPEN, iso_file_filters,
                       g_config.sys.files.dvd_path, NULL);
    if (new_disc_path == NULL) {
        /* Cancelled */
        return;
    }
    xemu_settings_set_string(&g_config.sys.files.dvd_path, new_disc_path);
    xemu_load_disc(new_disc_path);
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