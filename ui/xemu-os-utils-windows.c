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

const char *xemu_get_os_info(void)
{ 
	HKEY keyHandle;
    WCHAR windows_build[1024], windows_name[1024], version_number[1024];
    static const char *buffer = NULL; 
    DWORD string = 1024, string1 = 1024, string2 = 1024, string3 = 1024;

    if (buffer == NULL) {
        //Get Current Kernel version number.
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows NT\\CurrentVersion", 0,
                          KEY_QUERY_VALUE, &keyHandle) == ERROR_SUCCESS) {
            RegQueryValueExW(keyHandle, L"CurrentVersion", NULL, NULL, (LPBYTE)version_number, &string);
            
            //if version number is 6.3 (win 8.1/10/11), get the build number from the DisplayVersion Registry.
            if (wcscmp(version_number, L"6.3") == 0) {
                RegQueryValueExW(keyHandle, L"DisplayVersion", NULL, NULL, (LPBYTE)windows_build, &string1);

                //If it's lower than 6.3 (win 8 and below until XP) get the build descriptor from CSDBuildNumber.
                } else { 
                    RegQueryValueExW(keyHandle, L"CSDBuildNumber", NULL, NULL, (LPBYTE)windows_build, &string2);
                }

            RegQueryValueExW(keyHandle, L"ProductName", NULL, NULL, (LPBYTE)windows_name, &string3); 
            RegCloseKey(keyHandle);
        } else {
            const char *error = "Couldn't access system information!";
            return error;
        }
    }
    buffer = g_strdup_printf("%ls %ls", windows_name, windows_build);
    return buffer;
}

void xemu_open_web_browser(const char *url)
{
	ShellExecute(0, "open", url, 0, 0 , SW_SHOW);
}
