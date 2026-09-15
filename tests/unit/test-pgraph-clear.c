/* Renderer-neutral NV2A CLEAR_SURFACE helper tests. */

#include "qemu/osdep.h"

#include "hw/xbox/nv2a/nv2a_regs.h"
#include "hw/xbox/nv2a/pgraph/clear.h"

static void test_rectangles(void)
{
    PGRAPHClearRect input = { 2, 1, 99, 99 };
    PGRAPHClearRect output = {};
    g_assert_true(pgraph_clear_rect_clamp(&input, 8, 6, &output));
    g_assert_cmpuint(output.left, ==, 2);
    g_assert_cmpuint(output.top, ==, 1);
    g_assert_cmpuint(output.right, ==, 7);
    g_assert_cmpuint(output.bottom, ==, 5);
    g_assert_false(pgraph_clear_rect_is_full(&output, 8, 6));

    input = (PGRAPHClearRect){ 0, 0, 7, 5 };
    g_assert_true(pgraph_clear_rect_clamp(&input, 8, 6, &output));
    g_assert_true(pgraph_clear_rect_is_full(&output, 8, 6));
    input = (PGRAPHClearRect){ 8, 0, 8, 0 };
    g_assert_false(pgraph_clear_rect_clamp(&input, 8, 6, &output));
    input = (PGRAPHClearRect){ 3, 2, 1, 2 };
    g_assert_false(pgraph_clear_rect_clamp(&input, 8, 6, &output));
}

static void test_color_storage(void)
{
    g_assert_cmpint(pgraph_clear_classify_color_storage(
                        NV097_SET_SURFACE_FORMAT_COLOR_LE_X8R8G8B8_Z8R8G8B8),
                    ==, PGRAPH_CLEAR_COLOR_STORAGE_BGR8A8);
    g_assert_cmpint(pgraph_clear_classify_color_storage(
                        NV097_SET_SURFACE_FORMAT_COLOR_LE_X8R8G8B8_O8R8G8B8),
                    ==, PGRAPH_CLEAR_COLOR_STORAGE_BGR8A8);
    g_assert_cmpint(pgraph_clear_classify_color_storage(
                        NV097_SET_SURFACE_FORMAT_COLOR_LE_A8R8G8B8),
                    ==, PGRAPH_CLEAR_COLOR_STORAGE_BGR8A8);
    g_assert_cmpint(pgraph_clear_classify_color_storage(
                        NV097_SET_SURFACE_FORMAT_COLOR_LE_R5G6B5),
                    ==, PGRAPH_CLEAR_COLOR_STORAGE_UNSUPPORTED);
}

static void test_fixed_depth_stencil(void)
{
    uint32_t depth = 0;
    uint8_t stencil = 0;
    g_assert_true(pgraph_clear_extract_fixed_depth_stencil(
        NV097_SET_SURFACE_FORMAT_ZETA_Z16, 0, 0x12345678, &depth, &stencil));
    g_assert_cmpuint(depth, ==, 0x5678);
    g_assert_cmpuint(stencil, ==, 0);
    g_assert_true(pgraph_clear_extract_fixed_depth_stencil(
        NV097_SET_SURFACE_FORMAT_ZETA_Z24S8, 0, 0xabcdef42, &depth, &stencil));
    g_assert_cmpuint(depth, ==, 0xabcdef);
    g_assert_cmpuint(stencil, ==, 0x42);
    g_assert_false(pgraph_clear_extract_fixed_depth_stencil(
        NV097_SET_SURFACE_FORMAT_ZETA_Z24S8, 1, 0, &depth, &stencil));
    g_assert_false(
        pgraph_clear_extract_fixed_depth_stencil(0, 0, 0, &depth, &stencil));
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/pgraph/clear/rectangles", test_rectangles);
    g_test_add_func("/pgraph/clear/color-storage", test_color_storage);
    g_test_add_func("/pgraph/clear/fixed-depth-stencil",
                    test_fixed_depth_stencil);
    return g_test_run();
}
