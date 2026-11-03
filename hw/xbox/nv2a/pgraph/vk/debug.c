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

const char *pgraph_vk_result_to_string(VkResult result)
{
#define VK_RESULT_CASE(x) \
    case x:               \
        return #x

    switch (result) {
        VK_RESULT_CASE(VK_SUCCESS);
        VK_RESULT_CASE(VK_NOT_READY);
        VK_RESULT_CASE(VK_TIMEOUT);
        VK_RESULT_CASE(VK_EVENT_SET);
        VK_RESULT_CASE(VK_EVENT_RESET);
        VK_RESULT_CASE(VK_INCOMPLETE);
        VK_RESULT_CASE(VK_ERROR_OUT_OF_HOST_MEMORY);
        VK_RESULT_CASE(VK_ERROR_OUT_OF_DEVICE_MEMORY);
        VK_RESULT_CASE(VK_ERROR_INITIALIZATION_FAILED);
        VK_RESULT_CASE(VK_ERROR_DEVICE_LOST);
        VK_RESULT_CASE(VK_ERROR_MEMORY_MAP_FAILED);
        VK_RESULT_CASE(VK_ERROR_LAYER_NOT_PRESENT);
        VK_RESULT_CASE(VK_ERROR_EXTENSION_NOT_PRESENT);
        VK_RESULT_CASE(VK_ERROR_FEATURE_NOT_PRESENT);
        VK_RESULT_CASE(VK_ERROR_INCOMPATIBLE_DRIVER);
        VK_RESULT_CASE(VK_ERROR_TOO_MANY_OBJECTS);
        VK_RESULT_CASE(VK_ERROR_FORMAT_NOT_SUPPORTED);
        VK_RESULT_CASE(VK_ERROR_FRAGMENTED_POOL);
        VK_RESULT_CASE(VK_ERROR_UNKNOWN);
        VK_RESULT_CASE(VK_ERROR_OUT_OF_POOL_MEMORY);
        VK_RESULT_CASE(VK_ERROR_INVALID_EXTERNAL_HANDLE);
        VK_RESULT_CASE(VK_ERROR_FRAGMENTATION);
        VK_RESULT_CASE(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS);
        VK_RESULT_CASE(VK_PIPELINE_COMPILE_REQUIRED);
        VK_RESULT_CASE(VK_ERROR_SURFACE_LOST_KHR);
        VK_RESULT_CASE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
        VK_RESULT_CASE(VK_SUBOPTIMAL_KHR);
        VK_RESULT_CASE(VK_ERROR_OUT_OF_DATE_KHR);
        VK_RESULT_CASE(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
        VK_RESULT_CASE(VK_ERROR_VALIDATION_FAILED_EXT);
        VK_RESULT_CASE(VK_ERROR_INVALID_SHADER_NV);
        VK_RESULT_CASE(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
        VK_RESULT_CASE(VK_ERROR_NOT_PERMITTED_KHR);
        VK_RESULT_CASE(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT);
        VK_RESULT_CASE(VK_THREAD_IDLE_KHR);
        VK_RESULT_CASE(VK_THREAD_DONE_KHR);
        VK_RESULT_CASE(VK_OPERATION_DEFERRED_KHR);
        VK_RESULT_CASE(VK_OPERATION_NOT_DEFERRED_KHR);
    default:
        return "unknown VkResult";
    }

#undef VK_RESULT_CASE
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
