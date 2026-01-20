/*
 * NVIDIA Driver Settings Management
 *
 * Copyright (c) 2025 Matt Borgerson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include <windows.h>
#include <stdbool.h>
#include <stdio.h>
#include "nvapi.h"
#include "nvapi_defs.h"

#define LOG(fmt, ...) fprintf(stderr, "nvapi: " fmt "\n", ##__VA_ARGS__)

static HMODULE g_hnvapi;
static NvAPI_QueryInterface_t NvAPI_QueryInterface;
#define DECL_NVAPI_FUNC(name, id) static name##_t name;
NVAPI_FUNCS_X(DECL_NVAPI_FUNC)

static bool init_nvapi_func(const char *name, unsigned int interface_id,
                            void **ptr)
{
    *ptr = NvAPI_QueryInterface(interface_id);
    if (*ptr == NULL) {
        LOG("Failed to resolve %s", name);
        return false;
    }
    return true;
}

bool nvapi_init(void)
{
#ifdef _WIN64
    g_hnvapi = LoadLibraryA("nvapi64.dll");
#else
    g_hnvapi = LoadLibraryA("nvapi.dll");
#endif
    if (g_hnvapi == NULL) {
        return false;
    }

    NvAPI_QueryInterface = (NvAPI_QueryInterface_t)GetProcAddress(
        g_hnvapi, "nvapi_QueryInterface");
    if (NvAPI_QueryInterface == NULL) {
        LOG("GetProcAddress failed for NvAPI_QueryInterface");
        goto error;
    }

#define INIT_NVAPI_FUNC(name, id)                   \
    if (!init_nvapi_func(#name, id, (void *)&name)) \
        goto error;
    NVAPI_FUNCS_X(INIT_NVAPI_FUNC)

    if (NvAPI_Initialize()) {
        LOG("NvAPI_Initialize failed");
        goto error;
    }

    return true;

error:
    FreeLibrary(g_hnvapi);
    g_hnvapi = NULL;
    return false;
}

void nvapi_finalize(void)
{
    if (g_hnvapi) {
        NvAPI_Unload();
        g_hnvapi = NULL;
    }
}

bool nvapi_setup_profile(NvApiProfileOpts opts)
{
    if (g_hnvapi == NULL) {
        return false;
    }

    bool result = false;
    NvDRSSessionHandle session;
    if (NvAPI_DRS_CreateSession(&session)) {
        LOG("NvAPI_DRS_CreateSession failed");
        return false;
    }

    if (NvAPI_DRS_LoadSettings(session)) {
        LOG("NvAPI_DRS_LoadSettings failed");
        goto cleanup;
    }

    NvDRSProfileHandle profile = NULL;
    if (NvAPI_DRS_FindProfileByName(session, (NvU16 *)opts.profile_name,
                                    &profile)) {
        NVDRS_PROFILE profile_info = {
            .version = NVDRS_PROFILE_VER,
            .isPredefined = 0,
        };
        wcsncpy(profile_info.profileName, opts.profile_name,
                sizeof(profile_info.profileName) / sizeof(wchar_t));
        if (NvAPI_DRS_CreateProfile(session, &profile_info, &profile)) {
            LOG("NvAPI_DRS_CreateProfile failed");
            goto cleanup;
        }
        LOG("Created new profile");
    }

    NVDRS_APPLICATION_V4 app = { .version = NVDRS_APPLICATION_VER_V4 };
    if (NvAPI_DRS_GetApplicationInfo(session, profile,
                                     (NvU16 *)opts.executable_name, &app)) {
        app.isPredefined = 0;
        app.launcher[0] = 0;
        app.fileInFolder[0] = 0;
        wcsncpy(app.appName, opts.executable_name,
                sizeof(app.appName) / sizeof(wchar_t));
        if (NvAPI_DRS_CreateApplication(session, profile, &app)) {
            LOG("NvAPI_DRS_CreateApplication failed");
            goto cleanup;
        }
        LOG("Added application to profile");
    }

    NVDRS_SETTING setting = {
        .version = NVDRS_SETTING_VER,
        .settingId = OGL_THREAD_CONTROL_ID,
        .settingType = NVDRS_DWORD_TYPE,
        .u32CurrentValue = opts.threaded_optimization ?
                               OGL_THREAD_CONTROL_ENABLE :
                               OGL_THREAD_CONTROL_DISABLE,
    };
    if (NvAPI_DRS_SetSetting(session, profile, &setting)) {
        LOG("NvAPI_DRS_SetSetting for settingId %x failed",
            setting.settingId);
        goto cleanup;
    }

    if (NvAPI_DRS_SaveSettings(session)) {
        LOG("NvAPI_DRS_SaveSettings failed");
        goto cleanup;
    }
    LOG("Saved settings");
    result = true;

cleanup:
    NvAPI_DRS_DestroySession(session);
    return result;
}
