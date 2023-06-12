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
    const char *buffer = NULL;
    HKEY keyhandle;
    WCHAR current_version[1024], current_build[1024], product_name[1024];
    WCHAR version_size = 1024, build_size = 1024, product_size = 1024;

    // If one of the registry open fails, return Windows as a fallback.
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0,
                      KEY_QUERY_VALUE, &keyhandle) != ERROR_SUCCESS)
        return NULL;

    if (RegGetValueW(HKEY_LOCAL_MACHINE,
                     L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                     L"ProductName", RRF_RT_REG_SZ, (LPVOID)NULL, &product_name,
                     (LPDWORD)&product_size) != ERROR_SUCCESS)
        return NULL;

    if (RegGetValueW(HKEY_LOCAL_MACHINE,
                     L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                     L"CurrentVersion", RRF_RT_REG_SZ, (LPVOID)NULL,
                     &current_version, (LPDWORD)&version_size) != ERROR_SUCCESS)
        return NULL;

    // If version number is 6.3/10.0 (8.1/10/11), get the build number from the
    // DisplayVersion Registry. Reference:
    // https://en.wikipedia.org/wiki/Windows_NT
    if ((wcscmp(current_version, L"10.0") == 0) ||
        (wcscmp(current_version, L"6.3") == 0)) {
        if (RegGetValueW(HKEY_LOCAL_MACHINE,
                         L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                         L"DisplayVersion", RRF_RT_REG_SZ, (LPVOID)NULL,
                         &current_build,
                         (LPDWORD)&build_size) != ERROR_SUCCESS) {
            // If it's lower (Win 8 and below until Win XP) get the build descriptor
            // from CSDVersion.
            if (RegGetValueW(HKEY_LOCAL_MACHINE,
                             L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                             L"CSDVersion", RRF_RT_REG_SZ, (LPVOID)NULL,
                             &current_build,
                             (LPDWORD)&build_size) != ERROR_SUCCESS)
                return NULL;
        }
    }
    RegCloseKey(keyhandle);
    return g_strdup_printf("%ls %ls", product_name, current_build);
}

const char *xemu_get_os_info(void)
{
    static const char *buffer = NULL;

    if (buffer == NULL) {
        buffer = get_windows_build_info();

        if (buffer == NULL) {
            buffer = "Windows";
        }
    }
    return buffer;
}


void xemu_open_web_browser(const char *url)
{
    ShellExecute(0, "open", url, 0, 0, SW_SHOW);
}
