/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Vulkan dynamic blend-constant command cache.
 */

#ifndef HW_XBOX_NV2A_PGRAPH_VK_BLEND_CONSTANTS_CACHE_H
#define HW_XBOX_NV2A_PGRAPH_VK_BLEND_CONSTANTS_CACHE_H

typedef struct PGRAPHVkBlendConstantsCache {
    uint32_t command_guest_color;
    uint32_t packed_guest_color;
    float packed_color[4];
    bool command_valid;
    bool packed_valid;
} PGRAPHVkBlendConstantsCache;

static inline void pgraph_vk_blend_constants_cache_invalidate(
    PGRAPHVkBlendConstantsCache *cache)
{
    cache->command_valid = false;
}

/* Static state invalidates blend constants; dynamic-to-dynamic binds retain it. */
static inline void pgraph_vk_blend_constants_cache_pipeline_bound(
    PGRAPHVkBlendConstantsCache *cache, bool uses_dynamic_blend_constants)
{
    if (!uses_dynamic_blend_constants) {
        pgraph_vk_blend_constants_cache_invalidate(cache);
    }
}

/* Returns true exactly when the caller must emit vkCmdSetBlendConstants. */
static inline bool pgraph_vk_blend_constants_cache_update(
    PGRAPHVkBlendConstantsCache *cache, uint32_t guest_color,
    uint32_t relevant_component_mask)
{
    assert(relevant_component_mask != 0);

    if (cache->command_valid &&
        ((cache->command_guest_color ^ guest_color) &
         relevant_component_mask) == 0) {
        return false;
    }

    cache->command_guest_color = guest_color;
    cache->command_valid = true;
    return true;
}

/* Packed values survive command-state invalidation and pipeline changes. */
static inline bool pgraph_vk_blend_constants_cache_needs_pack(
    PGRAPHVkBlendConstantsCache *cache, uint32_t guest_color)
{
    if (cache->packed_valid && cache->packed_guest_color == guest_color) {
        return false;
    }

    cache->packed_guest_color = guest_color;
    cache->packed_valid = true;
    return true;
}

#endif
