/* Packed PSH-to-D3D11 interpreter cbuffer layout tests. */

#include "qemu/osdep.h"

#include "hw/xbox/nv2a/pgraph/d3d11/psh_interpreter.h"

static void test_input_unpack(void)
{
    PshReferenceProgram program = {};
    PshReferenceInputs inputs = {};
    xemu::d3d11_psh::InterpreterConstants constants = {};
    char error[128];
    program.combiner_control = 1;
    program.rgb_inputs[0] = 0x04250c2d;
    program.alpha_inputs[0] = 0x04351c3d;
    program.rgb_outputs[0] = 0x00000cba;
    program.alpha_outputs[0] = 0x00000cba;
    g_assert_true(xemu::d3d11_psh::BuildConstants(program, inputs, &constants,
                                                  error, sizeof(error)));
    g_assert_cmpuint(constants.rgb_inputs[0][0], ==, 0x04);
    g_assert_cmpuint(constants.rgb_inputs[0][1], ==, 0x25);
    g_assert_cmpuint(constants.rgb_inputs[0][2], ==, 0x0c);
    g_assert_cmpuint(constants.rgb_inputs[0][3], ==, 0x2d);
    g_assert_cmpuint(constants.alpha_inputs[0][0], ==, 0x04);
    g_assert_cmpuint(constants.alpha_inputs[0][1], ==, 0x35);
    g_assert_cmpuint(constants.alpha_inputs[0][2], ==, 0x1c);
    g_assert_cmpuint(constants.alpha_inputs[0][3], ==, 0x3d);
    g_assert_cmpuint(constants.rgb_outputs[0][0], ==, 0x00000cba);
    g_assert_cmpuint(constants.alpha_outputs[0][0], ==, 0x00000cba);

    program.rgb_inputs[0] = static_cast<uint32_t>(PS_REGISTER_EF_PROD) << 24;
    g_assert_false(xemu::d3d11_psh::BuildConstants(program, inputs, &constants,
                                                   error, sizeof(error)));
    program = {};
    program.final_inputs_1 = static_cast<uint32_t>(PS_REGISTER_EF_PROD) << 24;
    g_assert_false(xemu::d3d11_psh::BuildConstants(program, inputs, &constants,
                                                   error, sizeof(error)));
    program = {};
    program.final_inputs_0 =
        (static_cast<uint32_t>(PS_REGISTER_V1) |
         static_cast<uint32_t>(PS_INPUTMAPPING_EXPAND_NORMAL))
        << 24;
    g_assert_false(xemu::d3d11_psh::BuildConstants(program, inputs, &constants,
                                                   error, sizeof(error)));
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/pgraph/psh-interpreter/input-unpack", test_input_unpack);
    return g_test_run();
}
