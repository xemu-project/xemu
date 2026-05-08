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
#include "trace/control.h"

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
            bool capturing = rdoc_api->IsFrameCapturing();
            if (capturing && renderdoc_capture_frames == 0) {
                rdoc_api->EndFrameCapture(RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE(r->instance), 0);
                if (renderdoc_trace_frames) {
                    trace_enable_events("-nv2a_pgraph_*");
                    renderdoc_trace_frames = false;
                }
            }
            if (renderdoc_capture_frames > 0) {
                if (!capturing) {
                    if (renderdoc_trace_frames) {
                        trace_enable_events("nv2a_pgraph_*");
                    }
                    rdoc_api->StartFrameCapture(RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE(r->instance), 0);
                }
                --renderdoc_capture_frames;
            }
        }
    }
#endif
}

void pgraph_vk_insert_debug_marker(PGRAPHVkState *r, VkCommandBuffer cmd,
                                   float color[4], const char *format, ...)
{
    if (!r->debug_utils_extension_enabled) {
        return;
    }

    char *buf = NULL;

    va_list args;
    va_start(args, format);
    int err = vasprintf(&buf, format, args);
    assert(err >= 0);
    va_end(args);

    VkDebugUtilsLabelEXT label_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        .pLabelName = buf,
    };
    memcpy(label_info.color, color, 4 * sizeof(float));
    vkCmdInsertDebugUtilsLabelEXT(cmd, &label_info);
    free(buf);
}

void pgraph_vk_begin_debug_marker(PGRAPHVkState *r, VkCommandBuffer cmd,
                                  float color[4], const char *format, ...)
{
    if (!r->debug_utils_extension_enabled) {
        return;
    }

    char *buf = NULL;

    va_list args;
    va_start(args, format);
    int err = vasprintf(&buf, format, args);
    assert(err >= 0);
    va_end(args);

    VkDebugUtilsLabelEXT label_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        .pLabelName = buf,
    };
    memcpy(label_info.color, color, 4 * sizeof(float));
    vkCmdBeginDebugUtilsLabelEXT(cmd, &label_info);
    free(buf);

    r->debug_depth += 1;
    assert(r->debug_depth < 10 && "Missing pgraph_vk_debug_marker_end?");
}

void pgraph_vk_end_debug_marker(PGRAPHVkState *r, VkCommandBuffer cmd)
{
    if (!r->debug_utils_extension_enabled) {
        return;
    }

    vkCmdEndDebugUtilsLabelEXT(cmd);
    assert(r->debug_depth > 0);
    r->debug_depth -= 1;
}
