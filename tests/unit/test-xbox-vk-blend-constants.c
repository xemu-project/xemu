/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "hw/xbox/nv2a/pgraph/vk/blend-constants-cache.h"

static void test_first_value_emits(void)
{
    PGRAPHVkBlendConstantsCache cache = {0};

    g_assert_true(pgraph_vk_blend_constants_cache_update(&cache, 0,
                                                         UINT32_MAX));
    g_assert_true(cache.command_valid);
    g_assert_cmphex(cache.command_guest_color, ==, 0);
}

static void test_identical_value_skips(void)
{
    PGRAPHVkBlendConstantsCache cache = {0};

    g_assert_true(pgraph_vk_blend_constants_cache_update(
        &cache, 0x12345678, UINT32_MAX));
    g_assert_false(pgraph_vk_blend_constants_cache_update(
        &cache, 0x12345678, UINT32_MAX));
    g_assert_true(pgraph_vk_blend_constants_cache_update(
        &cache, 0x12345679, UINT32_MAX));
}

static void test_invalidation_forces_emit(void)
{
    PGRAPHVkBlendConstantsCache cache = {0};

    g_assert_true(pgraph_vk_blend_constants_cache_update(
        &cache, 0x89abcdef, UINT32_MAX));
    pgraph_vk_blend_constants_cache_invalidate(&cache);
    g_assert_false(cache.command_valid);
    g_assert_true(pgraph_vk_blend_constants_cache_update(
        &cache, 0x89abcdef, UINT32_MAX));
}

static void test_dynamic_dynamic_transition(void)
{
    PGRAPHVkBlendConstantsCache cache = {0};

    pgraph_vk_blend_constants_cache_pipeline_bound(&cache, true);
    g_assert_true(pgraph_vk_blend_constants_cache_update(
        &cache, 0x10203040, UINT32_MAX));
    pgraph_vk_blend_constants_cache_pipeline_bound(&cache, true);
    g_assert_false(pgraph_vk_blend_constants_cache_update(
        &cache, 0x10203040, UINT32_MAX));
}

static void test_dynamic_static_dynamic_transition(void)
{
    PGRAPHVkBlendConstantsCache cache = {0};

    /* Bind a static-state pipeline, then return to a dynamic-state pipeline. */
    pgraph_vk_blend_constants_cache_pipeline_bound(&cache, true);
    g_assert_true(pgraph_vk_blend_constants_cache_update(
        &cache, 0x10203040, UINT32_MAX));
    pgraph_vk_blend_constants_cache_pipeline_bound(&cache, false);
    pgraph_vk_blend_constants_cache_pipeline_bound(&cache, true);
    g_assert_true(pgraph_vk_blend_constants_cache_update(
        &cache, 0x10203040, UINT32_MAX));
}

static void test_dynamic_clear_dynamic_transition(void)
{
    PGRAPHVkBlendConstantsCache cache = {0};

    pgraph_vk_blend_constants_cache_pipeline_bound(&cache, true);
    g_assert_true(pgraph_vk_blend_constants_cache_update(
        &cache, 0x50607080, UINT32_MAX));

    /* Full/depth clear binds a pipeline without setting blend constants. */
    pgraph_vk_blend_constants_cache_pipeline_bound(&cache, false);
    pgraph_vk_blend_constants_cache_pipeline_bound(&cache, true);
    g_assert_true(pgraph_vk_blend_constants_cache_update(
        &cache, 0x50607080, UINT32_MAX));
}

static void test_alpha_only_ignores_rgb(void)
{
    PGRAPHVkBlendConstantsCache cache = {0};

    g_assert_true(pgraph_vk_blend_constants_cache_update(
        &cache, 0x12345678, 0xff000000));
    g_assert_false(pgraph_vk_blend_constants_cache_update(
        &cache, 0x12abcdef, 0xff000000));
    g_assert_true(pgraph_vk_blend_constants_cache_update(
        &cache, 0x13abcdef, 0xff000000));
}

static void test_expanded_component_mask_forces_emit(void)
{
    PGRAPHVkBlendConstantsCache cache = {0};

    g_assert_true(pgraph_vk_blend_constants_cache_update(
        &cache, 0x12345678, 0xff000000));
    g_assert_false(pgraph_vk_blend_constants_cache_update(
        &cache, 0x12abcdef, 0xff000000));
    g_assert_true(pgraph_vk_blend_constants_cache_update(
        &cache, 0x12abcdef, UINT32_MAX));
}

static void test_packed_value_survives_command_invalidation(void)
{
    PGRAPHVkBlendConstantsCache cache = {0};

    g_assert_true(pgraph_vk_blend_constants_cache_needs_pack(
        &cache, 0x12345678));
    g_assert_false(pgraph_vk_blend_constants_cache_needs_pack(
        &cache, 0x12345678));
    pgraph_vk_blend_constants_cache_invalidate(&cache);
    g_assert_false(pgraph_vk_blend_constants_cache_needs_pack(
        &cache, 0x12345678));
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/xbox/vulkan/blend-constants/first-value",
                    test_first_value_emits);
    g_test_add_func("/xbox/vulkan/blend-constants/identical-value",
                    test_identical_value_skips);
    g_test_add_func("/xbox/vulkan/blend-constants/invalidation",
                    test_invalidation_forces_emit);
    g_test_add_func("/xbox/vulkan/blend-constants/dynamic-dynamic",
                    test_dynamic_dynamic_transition);
    g_test_add_func("/xbox/vulkan/blend-constants/dynamic-static-dynamic",
                    test_dynamic_static_dynamic_transition);
    g_test_add_func("/xbox/vulkan/blend-constants/dynamic-clear-dynamic",
                    test_dynamic_clear_dynamic_transition);
    g_test_add_func("/xbox/vulkan/blend-constants/alpha-only",
                    test_alpha_only_ignores_rgb);
    g_test_add_func("/xbox/vulkan/blend-constants/expanded-mask",
                    test_expanded_component_mask_forces_emit);
    g_test_add_func("/xbox/vulkan/blend-constants/packed-value",
                    test_packed_value_survives_command_invalidation);

    return g_test_run();
}
