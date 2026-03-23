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
    WCHAR product_name[1024];
    WCHAR display_version[1024];
    WCHAR build_number[1024];
    DWORD update_build_revision = 0;
    DWORD product_size = sizeof(product_name);
    DWORD display_size = sizeof(display_version);
    DWORD build_size = sizeof(build_number);
    DWORD ubr_size = sizeof(update_build_revision);

    if (RegGetValueW(HKEY_LOCAL_MACHINE,
                     L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                     L"ProductName", RRF_RT_REG_SZ, (LPVOID)NULL, &product_name,
                     &product_size) != ERROR_SUCCESS) {
        return "Windows";
    }

    bool have_display_version =
        RegGetValueW(HKEY_LOCAL_MACHINE,
                     L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                     L"DisplayVersion", RRF_RT_REG_SZ, NULL,
                     &display_version, &display_size) == ERROR_SUCCESS;

    bool have_build_number =
        RegGetValueW(HKEY_LOCAL_MACHINE,
                     L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                     L"CurrentBuildNumber", RRF_RT_REG_SZ, NULL,
                     &build_number, &build_size) == ERROR_SUCCESS;

    bool have_ubr =
        RegGetValueW(HKEY_LOCAL_MACHINE,
                     L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                     L"UBR", RRF_RT_REG_DWORD, NULL,
                     &update_build_revision, &ubr_size) == ERROR_SUCCESS;

    if (have_display_version && have_build_number && have_ubr) {
        return g_strdup_printf("%ls %ls (build %ls.%lu)",
                               product_name, display_version, build_number,
                               (unsigned long)update_build_revision);
    }

    if (have_display_version) {
        return g_strdup_printf("%ls %ls", product_name, display_version);
    }

    if (have_build_number) {
        return g_strdup_printf("%ls (build %ls)", product_name, build_number);
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

typedef HRESULT(WINAPI *SetCurrentProcessExplicitAppUserModelIDFn)(PCWSTR);

void xemu_windows_set_app_user_model_id(void)
{
    HMODULE shell32 = LoadLibraryW(L"shell32.dll");
    SetCurrentProcessExplicitAppUserModelIDFn set_app_user_model_id;

    if (shell32 == NULL) {
        return;
    }

    set_app_user_model_id = (SetCurrentProcessExplicitAppUserModelIDFn)
        GetProcAddress(shell32, "SetCurrentProcessExplicitAppUserModelID");
    if (set_app_user_model_id != NULL) {
        set_app_user_model_id(L"OpenMidway.OpenMidway");
    }

    FreeLibrary(shell32);
}

typedef HRESULT(WINAPI *DwmSetWindowAttributeFn)(HWND hwnd, DWORD dwAttribute,
                                                 LPCVOID pvAttribute,
                                                 DWORD cbAttribute);

void xemu_windows_apply_window_style(void *native_window)
{
    enum {
        DWMWA_USE_IMMERSIVE_DARK_MODE = 20,
        DWMWA_WINDOW_CORNER_PREFERENCE = 33,
    };
    enum {
        DWMWCP_DEFAULT = 0,
        DWMWCP_DONOTROUND = 1,
        DWMWCP_ROUND = 2,
        DWMWCP_ROUNDSMALL = 3,
    };

    HWND hwnd = (HWND)native_window;
    HMODULE dwmapi;
    DwmSetWindowAttributeFn set_window_attribute;
    BOOL enable_dark_mode = TRUE;
    DWORD corner_preference = DWMWCP_ROUND;

    if (hwnd == NULL) {
        return;
    }

    dwmapi = LoadLibraryW(L"dwmapi.dll");
    if (dwmapi == NULL) {
        return;
    }

    set_window_attribute = (DwmSetWindowAttributeFn)GetProcAddress(
        dwmapi, "DwmSetWindowAttribute");
    if (set_window_attribute != NULL) {
        set_window_attribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE,
                             &enable_dark_mode, sizeof(enable_dark_mode));
        set_window_attribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE,
                             &corner_preference, sizeof(corner_preference));
    }

    FreeLibrary(dwmapi);
}
