/*
 * Geforce NV2A PGRAPH Renderdoc Helpers
 *
 * Copyright (c) 2024 Matt Borgerson
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"

#include <stdint.h>
#include <stdbool.h>

#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include "thirdparty/renderdoc_app.h"

#include "hw/xbox/nv2a/debug.h"

#ifdef _WIN32
#include <libloaderapi.h>
#else
#include <dlfcn.h>
#endif

static RENDERDOC_API_1_6_0 *rdoc_api = NULL;

int renderdoc_capture_frames = 0;
bool renderdoc_trace_frames = false;

void nv2a_dbg_renderdoc_init(void)
{
    if (rdoc_api) {
        return;
    }

#ifdef _WIN32
    HMODULE renderdoc = GetModuleHandleA("renderdoc.dll");
    if (!renderdoc) {
        fprintf(stderr, "Error: Failed to open renderdoc library: 0x%lx\n",
                GetLastError());
        return;
    }
    pRENDERDOC_GetAPI RENDERDOC_GetAPI =
        (pRENDERDOC_GetAPI)GetProcAddress(renderdoc, "RENDERDOC_GetAPI");
#else // _WIN32
#ifdef __APPLE__
    void *renderdoc = dlopen("librenderdoc.dylib", RTLD_LAZY);
#else
    void *renderdoc = dlopen("librenderdoc.so", RTLD_LAZY);
#endif
    if (!renderdoc) {
        fprintf(stderr, "Error: Failed to open renderdoc library: %s\n",
                dlerror());
        return;
    }
    pRENDERDOC_GetAPI RENDERDOC_GetAPI =
        (pRENDERDOC_GetAPI)dlsym(renderdoc, "RENDERDOC_GetAPI");
#endif // _WIN32

    if (!RENDERDOC_GetAPI) {
        fprintf(stderr, "Error: Could not get RENDERDOC_GetAPI address\n");
        return;
    }

    int ret =
        RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void **)&rdoc_api);
    if (ret != 1) {
        fprintf(stderr, "Error: Failed to retrieve RenderDoc API.\n");
    }
}

void *nv2a_dbg_renderdoc_get_api(void)
{
    return (void*)rdoc_api;
}

bool nv2a_dbg_renderdoc_available(void)
{
    return rdoc_api != NULL;
}

void nv2a_dbg_renderdoc_capture_frames(int num_frames, bool trace)
{
    renderdoc_capture_frames += num_frames;
    renderdoc_trace_frames = trace;
}
