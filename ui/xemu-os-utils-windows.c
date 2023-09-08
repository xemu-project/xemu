/*
 * OS-specific Helpers
 *
 * Copyright (C) 2020-2021 Matt Borgerson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "xemu-os-utils.h"
#include <windows.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <wchar.h>

static const char *get_windows_build_info(void)
{
    WCHAR current_build[1024], product_name[1024];
    WCHAR build_size = 1024, product_size = 1024;

    if (RegGetValueW(HKEY_LOCAL_MACHINE,
                     L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                     L"ProductName", RRF_RT_REG_SZ, (LPVOID)NULL, &product_name,
                     (LPDWORD)&product_size) != ERROR_SUCCESS) {
        return "Windows";
    }

    if ((RegGetValueW(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                      L"DisplayVersion", RRF_RT_REG_SZ, (LPVOID)NULL,
                      &current_build, (LPDWORD)&build_size) == ERROR_SUCCESS) ||
        (RegGetValueW(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                      L"CSDVersion", RRF_RT_REG_SZ, (LPVOID)NULL,
                      &current_build, (LPDWORD)&build_size) == ERROR_SUCCESS)) {
        return g_strdup_printf("%ls %ls", product_name, current_build);
    }

    return g_strdup_printf("%ls", product_name);
}

const char *xemu_get_os_info(void)
{
    static const char *buffer = NULL;

    if (buffer == NULL) {
        buffer = get_windows_build_info();
    }

    return buffer;
}


void xemu_open_web_browser(const char *url)
{
    ShellExecute(0, "open", url, 0, 0, SW_SHOW);
}
