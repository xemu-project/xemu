/*
 * Geforce NV2A PGRAPH Vulkan Renderer
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

#include "renderer.h"
#include "debug.h"

#ifndef _WIN32
#include <dlfcn.h>
#endif

#ifdef CONFIG_RENDERDOC
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include "thirdparty/renderdoc_app.h"
#endif

int nv2a_vk_dgroup_indent = 0;

void pgraph_vk_debug_init(void)
{
#ifdef CONFIG_RENDERDOC
    nv2a_dbg_renderdoc_init();
#endif
}

void pgraph_vk_debug_frame_terminator(void)
{
#ifdef CONFIG_RENDERDOC
    if (nv2a_dbg_renderdoc_available()) {
        RENDERDOC_API_1_6_0 *rdoc_api = nv2a_dbg_renderdoc_get_api();

        PGRAPHVkState *r = g_nv2a->pgraph.vk_renderer_state;
        if (rdoc_api->IsTargetControlConnected()) {
            if (rdoc_api->IsFrameCapturing()) {
                rdoc_api->EndFrameCapture(RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE(r->instance), 0);
            }
            if (renderdoc_capture_frames > 0) {
                rdoc_api->StartFrameCapture(RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE(r->instance), 0);
                --renderdoc_capture_frames;
            }
        }
    }
#endif
}

void pgraph_vk_insert_debug_marker(PGRAPHVkState *r, VkCommandBuffer cmd,
                                   const char *name, float color[4])
{
    if (r->debug_utils_extension_enabled) {
        VkDebugUtilsLabelEXT label_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
            .pLabelName = name,
        };
        memcpy(label_info.color, color, 4 * sizeof(float));
        vkCmdInsertDebugUtilsLabelEXT(cmd, &label_info);
    }
}
