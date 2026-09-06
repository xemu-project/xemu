/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"

#include "hw/xbox/nv2a/pgraph/uniform-dirty.h"
#include "hw/xbox/nv2a/pgraph/vk/glsl.h"

static void test_changed_value_dirties_row(void)
{
    uint32_t rows[2][4] = { 0 };
    bool dirty_rows[2] = { false };

    pgraph_uniform_u32_row_update(rows, dirty_rows, 1, 2, 0x12345678);

    g_assert_cmphex(rows[1][2], ==, 0x12345678);
    g_assert_true(dirty_rows[1]);
    g_assert_false(dirty_rows[0]);
}

static void test_identical_value_stays_clean(void)
{
    uint32_t rows[2][4] = {
        { 0 },
        { 0, 0, 0x12345678, 0 },
    };
    bool dirty_rows[2] = { false };

    pgraph_uniform_u32_row_update(rows, dirty_rows, 1, 2, 0x12345678);

    g_assert_false(dirty_rows[1]);
}

static void test_identical_value_preserves_prior_dirty(void)
{
    uint32_t rows[2][4] = {
        { 0 },
        { 0, 0, 0x12345678, 0 },
    };
    bool dirty_rows[2] = { false, true };

    pgraph_uniform_u32_row_update(rows, dirty_rows, 1, 2, 0x12345678);

    g_assert_true(dirty_rows[1]);
}

static void test_post_load_invalidation_marks_all_rows_dirty(void)
{
    bool dirty_rows[4] = { false };

    pgraph_uniform_dirty_rows_invalidate(dirty_rows,
                                         G_N_ELEMENTS(dirty_rows));

    for (unsigned int row = 0; row < G_N_ELEMENTS(dirty_rows); row++) {
        g_assert_true(dirty_rows[row]);
    }
}

static ShaderUniformLayout make_test_layout(ShaderUniform *uniform,
                                            uint32_t allocation[][4],
                                            size_t row_count)
{
    *uniform = (ShaderUniform) {
        .name = "rows",
        .dim_v = 4,
        .dim_a = row_count,
        .align = 16,
        .stride = 16,
        .offset = 0,
    };
    return (ShaderUniformLayout) {
        .uniforms = uniform,
        .num_uniforms = 1,
        .total_size = row_count * sizeof(allocation[0]),
        .allocation = allocation,
    };
}

static void test_uniform_copy_reports_real_changes(void)
{
    uint32_t allocation[2][4] = { 0 };
    uint32_t values[2][4] = {
        { 1, 2, 3, 4 },
        { 5, 6, 7, 8 },
    };
    ShaderUniform uniform;
    ShaderUniformLayout layout =
        make_test_layout(&uniform, allocation, G_N_ELEMENTS(allocation));

    g_assert_true(uniform_copy(&layout, 1, values, sizeof(uint32_t), 8));
    g_assert_cmpmem(allocation, sizeof(allocation), values, sizeof(values));
    g_assert_false(uniform_copy(&layout, 1, values, sizeof(uint32_t), 8));
}

static void test_uniform_array_copy_is_row_scoped(void)
{
    uint32_t allocation[2][4] = {
        { 1, 2, 3, 4 },
        { 5, 6, 7, 8 },
    };
    uint32_t replacement[4] = { 9, 10, 11, 12 };
    const uint32_t first_row[4] = { 1, 2, 3, 4 };
    ShaderUniform uniform;
    ShaderUniformLayout layout =
        make_test_layout(&uniform, allocation, G_N_ELEMENTS(allocation));

    g_assert_true(uniform_copy_array_element(
        &layout, 1, 1, replacement, sizeof(uint32_t)));
    g_assert_cmpmem(allocation[0], sizeof(allocation[0]), first_row,
                    sizeof(first_row));
    g_assert_cmpmem(allocation[1], sizeof(allocation[1]), replacement,
                    sizeof(replacement));
    g_assert_false(uniform_copy_array_element(
        &layout, 1, 1, replacement, sizeof(uint32_t)));
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/xbox/pgraph/uniform-dirty/changed",
                    test_changed_value_dirties_row);
    g_test_add_func("/xbox/pgraph/uniform-dirty/identical-clean",
                    test_identical_value_stays_clean);
    g_test_add_func("/xbox/pgraph/uniform-dirty/identical-prior-dirty",
                    test_identical_value_preserves_prior_dirty);
    g_test_add_func("/xbox/pgraph/uniform-dirty/post-load-invalidate",
                    test_post_load_invalidation_marks_all_rows_dirty);
    g_test_add_func("/xbox/vulkan/uniform-copy/change-detection",
                    test_uniform_copy_reports_real_changes);
    g_test_add_func("/xbox/vulkan/uniform-copy/row-scope",
                    test_uniform_array_copy_is_row_scoped);

    return g_test_run();
}
