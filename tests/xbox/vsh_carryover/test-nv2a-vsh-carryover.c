/*
 * Unit tests for Xbox NV2A vertex shader fog register carryover
 */

#include "qemu/osdep.h"
#include <glib.h>
#include <math.h>
#include "hw/xbox/nv2a/nv2a_int.h"
#include "hw/xbox/nv2a/nv2a_regs.h"
#include "nv2a_vsh_emulator_execution_state.h"
#include "mock-pgraph.h"

#define EPSILON 0.0001f

static void assert_float_near(float actual, float expected)
{
    g_assert_true(fabsf(actual - expected) <= EPSILON);
}

static void make_mov_output(uint32_t token[4], uint32_t out_idx,
                            uint32_t writemask, uint32_t in_v_idx, bool final)
{
    token[0] = 0;
    token[1] = (1 << 21) | ((in_v_idx & 0xF) << 9) | 0x1B;
    token[2] = (2 << 26);
    token[3] = (final ? 1 : 0) | ((out_idx & 0xFF) << 3) | (1 << 11) |
               ((writemask & 0xF) << 12);
}

static void setup_pgraph_test_state(PGRAPHState *pg)
{
    memset(pg, 0, sizeof(*pg));
    pgraph_vsh_carryover_reset(pg);

    /* Configure programmable vertex shader mode at program slot 0 */
    uint32_t d = pgraph_reg_r(pg, NV_PGRAPH_CSV0_D);
    SET_MASK(d, NV_PGRAPH_CSV0_D_MODE, 2);
    pgraph_reg_w(pg, NV_PGRAPH_CSV0_D, d);

    uint32_t c = pgraph_reg_r(pg, NV_PGRAPH_CSV0_C);
    SET_MASK(c, NV_PGRAPH_CSV0_C_CHEOPS_PROGRAM_START, 0);
    pgraph_reg_w(pg, NV_PGRAPH_CSV0_C, c);
}

static void test_vsh_carryover_reset_defaults(void)
{
    PGRAPHState pg;
    memset(&pg, 0x5A, sizeof(pg));
    memset(pg.vsh_carry_cached_programs, 0,
           sizeof(pg.vsh_carry_cached_programs));

    pgraph_vsh_carryover_reset(&pg);

    assert_float_near(pg.vsh_carry_fog[0], 0.0f);
    assert_float_near(pg.vsh_carry_fog[1], 0.0f);
    assert_float_near(pg.vsh_carry_fog[2], 0.0f);
    assert_float_near(pg.vsh_carry_fog[3], 1.0f);

    for (int i = 0; i < ARRAY_SIZE(pg.vsh_carry_cached_programs); ++i) {
        g_assert_null(pg.vsh_carry_cached_programs[i]);
    }
    g_assert_false(pg.vsh_carry_cache_dirty);
}

static void test_vsh_carryover_ff_no_op(void)
{
    PGRAPHState pg;
    setup_pgraph_test_state(&pg);

    /* Set mode to fixed function (mode = 0) */
    uint32_t d = pgraph_reg_r(&pg, NV_PGRAPH_CSV0_D);
    SET_MASK(d, NV_PGRAPH_CSV0_D_MODE, 0);
    pgraph_reg_w(&pg, NV_PGRAPH_CSV0_D, d);

    /* Put some values in v0 */
    pg.vertex_attributes[0].inline_value[0] = 0.5f;

    /* Put a program in program_data[0] that writes oFog */
    make_mov_output(pg.program_data[0], NV2AOR_FOG_COORD, 0xF, 0, true);

    pgraph_vsh_carryover_update(&pg);

    /* Fog must remain at default (0, 0, 0, 1) */
    assert_float_near(pg.vsh_carry_fog[0], 0.0f);
    assert_float_near(pg.vsh_carry_fog[1], 0.0f);
    assert_float_near(pg.vsh_carry_fog[2], 0.0f);
    assert_float_near(pg.vsh_carry_fog[3], 1.0f);
    for (int i = 0; i < ARRAY_SIZE(pg.vsh_carry_cached_programs); ++i) {
        g_assert_null(pg.vsh_carry_cached_programs[i]);
    }
}

/*
 * Validates the core scenario of issue #1852:
 * Draw 1 writes oFog and oPos.
 * Draw 2 writes only oPos.
 * oFog must carry over across the draw calls and retain the Draw 1 value.
 */
static void test_vsh_carryover_fog_carryover(void)
{
    PGRAPHState pg;
    setup_pgraph_test_state(&pg);

    /* Draw 1: Program at slot 0 writes oFog = v0 and oPos = v1 */
    /* Instruction 0: MOV oFog.xyzw, v0 */
    make_mov_output(pg.program_data[0], NV2AOR_FOG_COORD, 0xF, 0, false);
    /* Instruction 1: MOV oPos.xyzw, v1 (final) */
    make_mov_output(pg.program_data[1], NV2AOR_POS, 0xF, 1, true);

    pg.vertex_attributes[0].inline_value[0] = 0.9999f;
    pg.vertex_attributes[0].inline_value[1] = 0.9999f;
    pg.vertex_attributes[0].inline_value[2] = 0.9999f;
    pg.vertex_attributes[0].inline_value[3] = 0.9999f;

    pg.vertex_attributes[1].inline_value[0] = 1.0f;
    pg.vertex_attributes[1].inline_value[1] = 2.0f;
    pg.vertex_attributes[1].inline_value[2] = 3.0f;
    pg.vertex_attributes[1].inline_value[3] = 4.0f;

    pgraph_vsh_carryover_update(&pg);

    /* Verify Draw 1 updated oFog */
    assert_float_near(pg.vsh_carry_fog[0], 0.9999f);
    g_assert_nonnull(pg.vsh_carry_cached_programs[0]);

    /* Draw 2: Program at slot 2 writes only oPos = v1, oFog is omitted */
    uint32_t c = pgraph_reg_r(&pg, NV_PGRAPH_CSV0_C);
    SET_MASK(c, NV_PGRAPH_CSV0_C_CHEOPS_PROGRAM_START, 2);
    pgraph_reg_w(&pg, NV_PGRAPH_CSV0_C, c);

    make_mov_output(pg.program_data[2], NV2AOR_POS, 0xF, 1, true);

    pg.vertex_attributes[1].inline_value[0] = 10.0f;
    pg.vertex_attributes[1].inline_value[1] = 20.0f;
    pg.vertex_attributes[1].inline_value[2] = 30.0f;
    pg.vertex_attributes[1].inline_value[3] = 40.0f;

    /* Invalidate v0 inputs so we are certain oFog is not re-read from v0 */
    pg.vertex_attributes[0].inline_value[0] = 0.0f;

    pgraph_vsh_carryover_update(&pg);

    /* Verify oFog retained the value from Draw 1! */
    assert_float_near(pg.vsh_carry_fog[0], 0.9999f);

    /* Both programs should remain cached */
    g_assert_nonnull(pg.vsh_carry_cached_programs[0]);
    g_assert_nonnull(pg.vsh_carry_cached_programs[2]);

    pgraph_vsh_carryover_invalidate_cache(&pg);
}

static void test_vsh_carryover_program_cache_invalidation(void)
{
    PGRAPHState pg;
    setup_pgraph_test_state(&pg);

    make_mov_output(pg.program_data[0], NV2AOR_POS, 0xF, 0, true);
    pgraph_vsh_carryover_update(&pg);

    g_assert_nonnull(pg.vsh_carry_cached_programs[0]);

    pgraph_vsh_carryover_invalidate_cache(&pg);

    for (int i = 0; i < ARRAY_SIZE(pg.vsh_carry_cached_programs); ++i) {
        g_assert_null(pg.vsh_carry_cached_programs[i]);
    }

    /* Verify lazy invalidation via vsh_carry_cache_dirty */
    make_mov_output(pg.program_data[0], NV2AOR_POS, 0xF, 0, true);
    pgraph_vsh_carryover_update(&pg);
    g_assert_nonnull(pg.vsh_carry_cached_programs[0]);

    pg.vsh_carry_cache_dirty = true;
    pgraph_vsh_carryover_update(&pg);
    g_assert_nonnull(pg.vsh_carry_cached_programs[0]);
    g_assert_false(pg.vsh_carry_cache_dirty);

    pgraph_vsh_carryover_invalidate_cache(&pg);
}

static void test_vsh_carryover_multi_program_cache(void)
{
    PGRAPHState pg;
    setup_pgraph_test_state(&pg);

    /* Program at slot 0 */
    make_mov_output(pg.program_data[0], NV2AOR_POS, 0xF, 0, true);

    /* Program at slot 10 */
    make_mov_output(pg.program_data[10], NV2AOR_POS, 0xF, 1, true);

    /* Run slot 0 */
    pgraph_vsh_carryover_update(&pg);
    g_assert_nonnull(pg.vsh_carry_cached_programs[0]);
    g_assert_null(pg.vsh_carry_cached_programs[10]);

    Nv2aVshProgram *cached_prog_0 = pg.vsh_carry_cached_programs[0];

    /* Switch to slot 10 via CHEOPS_PROGRAM_START without setting
     * vsh_carry_cache_dirty */
    uint32_t c = pgraph_reg_r(&pg, NV_PGRAPH_CSV0_C);
    SET_MASK(c, NV_PGRAPH_CSV0_C_CHEOPS_PROGRAM_START, 10);
    pgraph_reg_w(&pg, NV_PGRAPH_CSV0_C, c);

    pgraph_vsh_carryover_update(&pg);
    g_assert_nonnull(pg.vsh_carry_cached_programs[10]);
    /* Slot 0 must remain cached (pointer unchanged) */
    g_assert_true(pg.vsh_carry_cached_programs[0] == cached_prog_0);

    /* Switch back to slot 0 */
    SET_MASK(c, NV_PGRAPH_CSV0_C_CHEOPS_PROGRAM_START, 0);
    pgraph_reg_w(&pg, NV_PGRAPH_CSV0_C, c);

    pgraph_vsh_carryover_update(&pg);
    /* Both still cached and intact */
    g_assert_true(pg.vsh_carry_cached_programs[0] == cached_prog_0);
    g_assert_nonnull(pg.vsh_carry_cached_programs[10]);

    pgraph_vsh_carryover_invalidate_cache(&pg);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/xbox/vsh_carryover/reset_defaults",
                    test_vsh_carryover_reset_defaults);
    g_test_add_func("/xbox/vsh_carryover/ff_no_op",
                    test_vsh_carryover_ff_no_op);
    g_test_add_func("/xbox/vsh_carryover/fog_carryover",
                    test_vsh_carryover_fog_carryover);
    g_test_add_func("/xbox/vsh_carryover/program_cache_invalidation",
                    test_vsh_carryover_program_cache_invalidation);
    g_test_add_func("/xbox/vsh_carryover/multi_program_cache",
                    test_vsh_carryover_multi_program_cache);

    return g_test_run();
}
