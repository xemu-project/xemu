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

#ifndef HW_XBOX_NV2A_PGRAPH_VK_DEBUG_H
#define HW_XBOX_NV2A_PGRAPH_VK_DEBUG_H

#define DEBUG_VK 0

extern int nv2a_vk_dgroup_indent;

#define NV2A_VK_XDPRINTF(x, fmt, ...)                                  \
    do {                                                               \
        if (x) {                                                       \
            fprintf(stderr, "%*s" fmt "\n", nv2a_vk_dgroup_indent, "", \
                    ##__VA_ARGS__);                                    \
        }                                                              \
    } while (0)

#define NV2A_VK_DPRINTF(fmt, ...) NV2A_VK_XDPRINTF(DEBUG_VK, fmt, ##__VA_ARGS__)

#define NV2A_VK_DGROUP_BEGIN(fmt, ...)                  \
    do {                                                \
        NV2A_VK_XDPRINTF(DEBUG_VK, fmt, ##__VA_ARGS__); \
        nv2a_vk_dgroup_indent++;                        \
    } while (0)

#define NV2A_VK_DGROUP_END(...)                        \
    do {                                               \
        nv2a_vk_dgroup_indent--;                       \
        NV2A_ASSERT_FATAL(nv2a_vk_dgroup_indent >= 0); \
    } while (0)

#define VK_CHECK(x)                                                          \
    do {                                                                     \
        VkResult vk_result = (x);                                            \
        if (vk_result != VK_SUCCESS) {                                       \
            fprintf(stderr, "vk_result = %d\n", vk_result);                  \
            nv2a_log_fatal_error(                                            \
                "vk check failed: vk_result = %d\nat %s:%d\n", vk_result,    \
                __FILE__, __LINE__);                                         \
            assert((x) &&                                                    \
                   "A fatal error occurred. Check the xemu fatal error log " \
                   "in your home directory for details.");                   \
            exit(127);                                                       \
        }                                                                    \
    } while (0)

void pgraph_vk_debug_frame_terminator(void);

#endif
