/* Texture-free NV2A PSH/register-combiner reference tests. */

#include "qemu/osdep.h"

#include "hw/xbox/nv2a/pgraph/psh_reference.h"

static uint32_t inputs(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    return a << 24 | b << 16 | c << 8 | d;
}

static void test_passthru_and_immutability(void)
{
    PshReferenceProgram program = {};
    PshReferenceInputs values = {};
    PshReferenceInputs before;
    PshReferenceResult result = {};
    char error[128];

    program.combiner_control = 0;
    program.shader_stage_program = PS_TEXTUREMODES_PASSTHRU;
    values.v0 = (PshReferenceVec4){ 0.2f, 0.3f, 0.4f, 0.5f };
    memcpy(&before, &values, sizeof(before));
    g_assert_true(psh_reference_evaluate(&program, &values, &result, error,
                                         sizeof(error)));
    g_assert_cmpfloat(result.color.x, ==, values.v0.x);
    g_assert_cmpfloat(result.color.w, ==, values.v0.w);
    g_assert_cmpmem(&values, sizeof(values), &before, sizeof(before));

    memset(&program, 0, sizeof(program));
    program.combiner_control = 1;
    program.shader_stage_program = PS_TEXTUREMODES_NONE;
    program.rgb_inputs[0] = inputs(PS_REGISTER_T0, PS_REGISTER_ONE,
                                   PS_REGISTER_ZERO, PS_REGISTER_ZERO);
    program.rgb_outputs[0] = PS_REGISTER_R0 << 4;
    values.t[0] = (PshReferenceVec4){ 0.8f, 0.7f, 0.6f, 0.5f };
    g_assert_true(psh_reference_evaluate(&program, &values, &result, error,
                                         sizeof(error)));
    g_assert_cmpfloat(result.r0.x, ==, 0.0f);
    g_assert_cmpfloat(result.r0.w, ==, 1.0f);
    program.shader_stage_program = PS_TEXTUREMODES_PASSTHRU;
    g_assert_true(psh_reference_evaluate(&program, &values, &result, error,
                                         sizeof(error)));
    g_assert_cmpfloat(result.r0.x, ==, values.t[0].x);
    g_assert_cmpfloat(result.r0.w, ==, values.t[0].w);
}

static void test_dot_mux_and_final(void)
{
    PshReferenceProgram program = {};
    PshReferenceInputs values = {};
    PshReferenceResult result = {};
    char error[128];

    program.combiner_control = 1;
    program.shader_stage_program = PS_TEXTUREMODES_NONE;
    program.rgb_inputs[0] = inputs(PS_REGISTER_V0, PS_REGISTER_V1,
                                   PS_REGISTER_ONE, PS_REGISTER_ONE);
    program.alpha_inputs[0] =
        inputs((uint32_t)PS_REGISTER_V0 | (uint32_t)PS_CHANNEL_ALPHA,
               PS_REGISTER_ONE, PS_REGISTER_ONE, PS_REGISTER_ONE);
    program.rgb_outputs[0] =
        (PS_REGISTER_R0 << 4) |
        ((PS_COMBINEROUTPUT_AB_DOT_PRODUCT | PS_COMBINEROUTPUT_AB_BLUE_TO_ALPHA)
         << 12);
    program.alpha_outputs[0] = PS_REGISTER_R0 << 4;
    values.v0 = (PshReferenceVec4){ 1, 0, 0, 0.25f };
    values.v1 = (PshReferenceVec4){ 1, 0, 0, 0.75f };
    g_assert_true(psh_reference_evaluate(&program, &values, &result, error,
                                         sizeof(error)));
    g_assert_cmpfloat(result.r0.x, ==, 1.0f);
    g_assert_cmpfloat(result.r0.y, ==, 1.0f);
    g_assert_cmpfloat(result.r0.z, ==, 1.0f);
    g_assert_cmpfloat(result.r0.w, ==, 0.25f);

    memset(&program, 0, sizeof(program));
    program.final_inputs_0 = inputs(PS_REGISTER_V1, PS_REGISTER_C0,
                                    PS_REGISTER_ZERO, PS_REGISTER_ZERO);
    program.final_inputs_1 =
        ((uint32_t)PS_REGISTER_V1 | (uint32_t)PS_CHANNEL_ALPHA) << 8;
    values.v1 = (PshReferenceVec4){ .25f, .25f, .25f, .75f };
    values.c0[8] = (PshReferenceVec4){ .8f, .8f, .8f, 1.0f };
    g_assert_true(psh_reference_evaluate(&program, &values, &result, error,
                                         sizeof(error)));
    g_assert_cmpfloat_with_epsilon(result.color.x, .2f, 0.00001f);
    g_assert_cmpfloat(result.color.w, ==, .75f);

    memset(&program, 0, sizeof(program));
    program.final_inputs_0 = inputs(PS_REGISTER_EF_PROD, PS_REGISTER_ONE,
                                    PS_REGISTER_ZERO, PS_REGISTER_ZERO);
    program.final_inputs_1 =
        inputs(PS_REGISTER_V0, PS_REGISTER_C0,
               (uint32_t)PS_REGISTER_V1 | (uint32_t)PS_CHANNEL_ALPHA, 0);
    values.v0 = (PshReferenceVec4){ .4f, .4f, .4f, .2f };
    values.c0[8] = (PshReferenceVec4){ .5f, .5f, .5f, 1.0f };
    values.v1.w = .7f;
    g_assert_true(psh_reference_evaluate(&program, &values, &result, error,
                                         sizeof(error)));
    /* E*F, not E*E: .4*.5 = .2. */
    g_assert_cmpfloat_with_epsilon(result.color.x, .2f, 0.00001f);
}

static void test_rejects_unsupported_programs(void)
{
    PshReferenceProgram program = {};
    char error[128];

    program.shader_stage_program = PS_TEXTUREMODES_PROJECT2D;
    g_assert_false(psh_reference_validate(&program, error, sizeof(error)));
    g_assert_nonnull(strstr(error, "texture stage"));
    memset(&program, 0, sizeof(program));
    program.combiner_control = 9;
    g_assert_false(psh_reference_validate(&program, error, sizeof(error)));
    memset(&program, 0, sizeof(program));
    program.shader_stage_program = PS_TEXTUREMODES_PASSTHRU << 15;
    g_assert_true(psh_reference_validate(&program, error, sizeof(error)));
    program.shader_stage_program |= 1u << 20;
    g_assert_false(psh_reference_validate(&program, error, sizeof(error)));
    memset(&program, 0, sizeof(program));
    program.final_inputs_1 = (uint32_t)PS_REGISTER_EF_PROD << 24;
    g_assert_false(psh_reference_validate(&program, error, sizeof(error)));
    memset(&program, 0, sizeof(program));
    program.rgb_inputs[0] = (uint32_t)PS_REGISTER_V1R0_SUM << 24;
    program.combiner_control = 1;
    g_assert_false(psh_reference_validate(&program, error, sizeof(error)));
}

static void test_output_flags(void)
{
    static const unsigned mappings[] = { 0x00, 0x08, 0x10, 0x18, 0x20, 0x30 };
    PshReferenceProgram program = { .combiner_control = 1 };
    char error[128];
    for (unsigned mapping = 0; mapping < G_N_ELEMENTS(mappings); ++mapping) {
        for (unsigned operation = 0; operation < 8; ++operation) {
            for (unsigned blue = 0; blue < 4; ++blue) {
                const unsigned flags = mappings[mapping] | operation |
                                       ((blue & 1) ? 0x40 : 0) |
                                       ((blue & 2) ? 0x80 : 0);
                program.rgb_outputs[0] = (PS_REGISTER_R1 << 4) |
                                         (PS_REGISTER_R0 << 8) | (flags << 12);
                g_assert_true(
                    psh_reference_validate(&program, error, sizeof(error)));
            }
        }
    }
    program.rgb_outputs[0] = (PS_REGISTER_R0 << 4) | (0x100u << 12);
    g_assert_false(psh_reference_validate(&program, error, sizeof(error)));
    program.alpha_outputs[0] = (PS_REGISTER_R0 << 4) | (0x40u << 12);
    g_assert_false(psh_reference_validate(&program, error, sizeof(error)));
}

static void test_unique_constants_and_mux_msb(void)
{
    PshReferenceProgram program = {};
    PshReferenceInputs values = {};
    PshReferenceResult result = {};
    char error[128];
    const uint32_t unique =
        (PS_COMBINERCOUNT_UNIQUE_C0 | PS_COMBINERCOUNT_UNIQUE_C1) << 8;
    program.combiner_control = 2 | unique;
    program.shader_stage_program =
        PS_TEXTUREMODES_PASSTHRU | (PS_TEXTUREMODES_PASSTHRU << 5);
    program.rgb_inputs[0] = inputs(PS_REGISTER_C0, PS_REGISTER_C1,
                                   PS_REGISTER_ZERO, PS_REGISTER_ZERO);
    program.rgb_inputs[1] = program.rgb_inputs[0];
    program.rgb_outputs[0] = PS_REGISTER_R0 << 4;
    program.rgb_outputs[1] = PS_REGISTER_R0 << 4;
    values.c0[0] = (PshReferenceVec4){ .2f, .2f, .2f, 1 };
    values.c1[0] = (PshReferenceVec4){ .3f, .3f, .3f, 1 };
    values.c0[1] = (PshReferenceVec4){ .4f, .4f, .4f, 1 };
    values.c1[1] = (PshReferenceVec4){ .5f, .5f, .5f, 1 };
    g_assert_true(psh_reference_evaluate(&program, &values, &result, error,
                                         sizeof(error)));
    g_assert_cmpfloat_with_epsilon(result.r0.x, .2f, 0.00001f);

    memset(&program, 0, sizeof(program));
    program.combiner_control = 1 | (PS_COMBINERCOUNT_MUX_MSB << 8);
    program.shader_stage_program = PS_TEXTUREMODES_PASSTHRU;
    program.rgb_inputs[0] = inputs(PS_REGISTER_V0, PS_REGISTER_ONE,
                                   PS_REGISTER_V1, PS_REGISTER_ONE);
    program.rgb_outputs[0] =
        (PS_REGISTER_R0 << 8) | (PS_COMBINEROUTPUT_AB_CD_MUX << 12);
    values.v0 = (PshReferenceVec4){ .1f, .1f, .1f, .1f };
    values.v1 = (PshReferenceVec4){ .8f, .7f, .6f, .75f };
    values.t[0].w = .75f;
    g_assert_true(psh_reference_evaluate(&program, &values, &result, error,
                                         sizeof(error)));
    g_assert_cmpfloat_with_epsilon(result.r0.x, .8f, 0.00001f);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/pgraph/psh-reference/passthru-immutable",
                    test_passthru_and_immutability);
    g_test_add_func("/pgraph/psh-reference/dot-final", test_dot_mux_and_final);
    g_test_add_func("/pgraph/psh-reference/reject",
                    test_rejects_unsupported_programs);
    g_test_add_func("/pgraph/psh-reference/output-flags", test_output_flags);
    g_test_add_func("/pgraph/psh-reference/unique-mux",
                    test_unique_constants_and_mux_msb);
    return g_test_run();
}
