/* Renderer-neutral tests for the CPU-authoritative NV2A VSH transform. */

#include "hw/xbox/nv2a/pgraph/d3d11/vsh_cpu_transform.h"

#include "nv2a_vsh_emulator_execution_state.h"

#include <glib.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace {

using xemu::D3D11CanonicalVshInputs;
using xemu::d3d11_vsh::DefaultVshCpuPostprocess;
using xemu::d3d11_vsh::TransformPlan;
using xemu::d3d11_vsh::TransformProgram;
using xemu::d3d11_vsh::TransformTokens;
using xemu::d3d11_vsh::VshCpuTransformOptions;
using xemu::d3d11_vsh::VshCpuTransformStatus;
using xemu::d3d11_vsh::VshCpuVertexOutput;

Nv2aVshInput Input(Nv2aVshRegisterType type, uint32_t index)
{
    Nv2aVshInput input = {};
    input.type = type;
    input.index = index;
    input.swizzle[0] = NV2ASW_X;
    input.swizzle[1] = NV2ASW_Y;
    input.swizzle[2] = NV2ASW_Z;
    input.swizzle[3] = NV2ASW_W;
    return input;
}

Nv2aVshOutput Output(Nv2aVshRegisterType type, uint32_t index,
                     Nv2aVshWritemask mask)
{
    Nv2aVshOutput output = {};
    output.type = type;
    output.index = index;
    output.writemask = mask;
    return output;
}

Nv2aVshStep EmptyStep()
{
    Nv2aVshStep step = {};
    for (Nv2aVshInput &input : step.mac.inputs) {
        input = Input(NV2ART_NONE, 0);
    }
    for (Nv2aVshInput &input : step.ilu.inputs) {
        input = Input(NV2ART_NONE, 0);
    }
    return step;
}

VshCpuTransformOptions Options(float constants[192][4])
{
    VshCpuTransformOptions options = {};
    options.constants = constants;
    options.postprocess = DefaultVshCpuPostprocess();
    return options;
}

bool Ready(const Nv2aVshProgram &program, uint32_t count,
           const float inputs[1][16][4], float constants[192][4],
           VshCpuVertexOutput *output)
{
    char error[128] = {};
    return TransformProgram(&program, count, inputs, 1, Options(constants),
                            output, error,
                            sizeof(error)) == VshCpuTransformStatus::Ready;
}

void MakeMac(Nv2aVshStep *step, Nv2aVshOpcode opcode,
             Nv2aVshRegisterType output_type = NV2ART_OUTPUT,
             uint32_t output_index = NV2AOR_DIFFUSE,
             Nv2aVshWritemask mask = NV2AWM_XYZW)
{
    *step = EmptyStep();
    step->mac.opcode = opcode;
    step->mac.inputs[0] = Input(NV2ART_INPUT, 0);
    if (opcode != NV2AOP_MOV && opcode != NV2AOP_ARL) {
        step->mac.inputs[1] = Input(NV2ART_INPUT, 1);
    }
    if (opcode == NV2AOP_MAD) {
        step->mac.inputs[2] = Input(NV2ART_INPUT, 2);
    }
    step->mac.outputs[0] = Output(output_type, output_index, mask);
}

void MakeIlu(Nv2aVshStep *step, Nv2aVshOpcode opcode,
             uint32_t output_index = NV2AOR_DIFFUSE,
             Nv2aVshWritemask mask = NV2AWM_XYZW)
{
    *step = EmptyStep();
    step->ilu.opcode = opcode;
    step->ilu.inputs[0] = Input(NV2ART_INPUT, 0);
    step->ilu.outputs[0] = Output(NV2ART_OUTPUT, output_index, mask);
}

void Finish(Nv2aVshStep *step)
{
    step->is_final = true;
}

void InitInputs(float inputs[1][16][4])
{
    std::memset(inputs, 0, sizeof(float) * 16 * 4);
    for (unsigned i = 0; i < 16; ++i) {
        for (unsigned component = 0; component < 4; ++component) {
            inputs[0][i][component] = static_cast<float>(i + component + 1);
        }
    }
}

void test_mac_opcodes(void)
{
    float inputs[1][16][4] = {};
    float constants[192][4] = {};
    InitInputs(inputs);
    for (unsigned opcode = NV2AOP_MOV; opcode <= NV2AOP_SGE; ++opcode) {
        Nv2aVshStep step = EmptyStep();
        MakeMac(&step, static_cast<Nv2aVshOpcode>(opcode));
        Finish(&step);
        Nv2aVshProgram program = { &step };
        VshCpuVertexOutput output = {};
        g_assert_true(Ready(program, 1, inputs, constants, &output));
    }
}

void test_ilu_opcodes_and_exceptional_values(void)
{
    float inputs[1][16][4] = {};
    float constants[192][4] = {};
    InitInputs(inputs);
    inputs[0][0][0] = 0.0f;
    const Nv2aVshOpcode ilu_opcodes[] = {
        NV2AOP_MOV, NV2AOP_RCP, NV2AOP_RCC, NV2AOP_RSQ,
        NV2AOP_EXP, NV2AOP_LOG, NV2AOP_LIT,
    };
    for (Nv2aVshOpcode opcode : ilu_opcodes) {
        Nv2aVshStep step = EmptyStep();
        MakeIlu(&step, opcode);
        Finish(&step);
        Nv2aVshProgram program = { &step };
        VshCpuVertexOutput output = {};
        g_assert_true(Ready(program, 1, inputs, constants, &output));
    }
}

void test_swizzle_negate_mask_and_all_inputs(void)
{
    float inputs[1][16][4] = {};
    float constants[192][4] = {};
    InitInputs(inputs);
    Nv2aVshStep step = EmptyStep();
    MakeMac(&step, NV2AOP_MOV, NV2ART_OUTPUT, NV2AOR_DIFFUSE, NV2AWM_XY);
    step.mac.inputs[0] = Input(NV2ART_INPUT, 0);
    step.mac.inputs[0].swizzle[0] = NV2ASW_Y;
    step.mac.inputs[0].swizzle[1] = NV2ASW_X;
    step.mac.inputs[0].swizzle[2] = NV2ASW_W;
    step.mac.inputs[0].swizzle[3] = NV2ASW_Z;
    step.mac.inputs[0].is_negated = true;
    Finish(&step);
    Nv2aVshProgram program = { &step };
    VshCpuVertexOutput output = {};
    g_assert_true(Ready(program, 1, inputs, constants, &output));
    g_assert_cmpfloat(output.oD0[0], ==, 0.0f);
    g_assert_cmpfloat(output.oD0[1], ==, 0.0f);
    g_assert_cmpfloat(output.oD0[2], ==, 0.0f);
    g_assert_cmpfloat(output.oD0[3], ==, 1.0f);

    for (unsigned input_index = 0; input_index < 16; ++input_index) {
        inputs[0][input_index][0] = static_cast<float>(input_index + 1) / 16.0f;
        step = EmptyStep();
        MakeMac(&step, NV2AOP_MOV);
        step.mac.inputs[0] = Input(NV2ART_INPUT, input_index);
        Finish(&step);
        program.steps = &step;
        output = {};
        g_assert_true(Ready(program, 1, inputs, constants, &output));
        g_assert_cmpfloat(output.oD0[0], ==,
                          static_cast<float>(input_index + 1) / 16.0f);
    }
}

void test_paired_r1_and_partial_masks(void)
{
    float inputs[1][16][4] = {};
    float constants[192][4] = {};
    InitInputs(inputs);
    inputs[0][1][0] = 2.0f;
    Nv2aVshStep steps[2] = {};
    MakeMac(&steps[0], NV2AOP_MOV, NV2ART_TEMPORARY, 1, NV2AWM_X);
    steps[0].ilu.opcode = NV2AOP_RCP;
    steps[0].ilu.inputs[0] = Input(NV2ART_INPUT, 1);
    steps[0].ilu.outputs[0] = Output(NV2ART_TEMPORARY, 2, NV2AWM_Y);
    MakeMac(&steps[1], NV2AOP_MOV);
    steps[1].mac.inputs[0] = Input(NV2ART_TEMPORARY, 1);
    Finish(&steps[1]);
    Nv2aVshProgram program = { steps };
    VshCpuVertexOutput output = {};
    g_assert_true(Ready(program, 2, inputs, constants, &output));
    g_assert_cmpfloat(output.oD0[0], ==, 0.0f);
    g_assert_cmpfloat(output.oD0[1], ==, 0.5f);
}

void test_arl_bounds_and_serial_context(void)
{
    float inputs[2][16][4] = {};
    float constants[192][4] = {};
    InitInputs(reinterpret_cast<float (*)[16][4]>(inputs));
    inputs[0][2][0] = 2.0f;
    inputs[1][2][0] = 200.0f;
    constants[5][0] = 0.7f;
    Nv2aVshStep in_range[3] = {};
    in_range[0] = EmptyStep();
    in_range[0].mac.opcode = NV2AOP_ARL;
    in_range[0].mac.inputs[0] = Input(NV2ART_INPUT, 2);
    in_range[0].mac.outputs[0] = Output(NV2ART_ADDRESS, 0, (Nv2aVshWritemask)0);
    in_range[1] = EmptyStep();
    in_range[1].mac.opcode = NV2AOP_MOV;
    in_range[1].mac.inputs[0] = Input(NV2ART_CONTEXT, 3);
    in_range[1].mac.inputs[0].is_relative = true;
    in_range[1].mac.outputs[0] =
        Output(NV2ART_OUTPUT, NV2AOR_DIFFUSE, NV2AWM_XYZW);
    in_range[2] = EmptyStep();
    in_range[2].mac.opcode = NV2AOP_MOV;
    in_range[2].mac.inputs[0] = Input(NV2ART_INPUT, 0);
    Finish(&in_range[2]);
    Nv2aVshProgram program = { in_range };
    VshCpuTransformOptions options = Options(constants);
    VshCpuVertexOutput output[2] = {};
    char error[128] = {};
    g_assert_true(TransformProgram(&program, 3, inputs, 2, options, output,
                                   error, sizeof(error)) ==
                  VshCpuTransformStatus::Ready);
    g_assert_cmpfloat(output[0].oD0[0], ==, 0.7f);
    g_assert_cmpfloat(output[1].oD0[0], ==, 0.0f);

    float relative_input[1][16][4] = {};
    std::memcpy(relative_input, inputs, sizeof(relative_input));
    relative_input[0][2][0] = 0.0f;
    constants[3][0] = 0.13f;
    g_assert_true(TransformProgram(&program, 3, relative_input, 1, options,
                                   output, error, sizeof(error)) ==
                  VshCpuTransformStatus::Ready);
    g_assert_cmpfloat(output[0].oD0[0], ==, 0.13f);
    relative_input[0][2][0] = -2.0f;
    constants[1][0] = 0.11f;
    g_assert_true(TransformProgram(&program, 3, relative_input, 1, options,
                                   output, error, sizeof(error)) ==
                  VshCpuTransformStatus::Ready);
    g_assert_cmpfloat(output[0].oD0[0], ==, 0.11f);

    Nv2aVshStep context_steps[2] = {};
    MakeMac(&context_steps[0], NV2AOP_MOV, NV2ART_OUTPUT, NV2AOR_DIFFUSE);
    context_steps[0].mac.inputs[0] = Input(NV2ART_CONTEXT, 0);
    context_steps[1] = EmptyStep();
    context_steps[1].mac.opcode = NV2AOP_MOV;
    context_steps[1].mac.inputs[0] = Input(NV2ART_INPUT, 0);
    context_steps[1].mac.outputs[0] = Output(NV2ART_CONTEXT, 0, NV2AWM_XYZW);
    Finish(&context_steps[1]);
    program.steps = context_steps;
    constants[0][0] = 0.9f;
    g_assert_true(TransformProgram(&program, 2, inputs, 2, options, output,
                                   error, sizeof(error)) ==
                  VshCpuTransformStatus::Ready);
    g_assert_cmpfloat(output[0].oD0[0], ==, 0.9f);
    g_assert_cmpfloat(output[1].oD0[0], ==, 1.0f);
}

void test_limits_position_and_mapping(void)
{
    float inputs[1][16][4] = {};
    float constants[192][4] = {};
    InitInputs(inputs);
    Nv2aVshStep one = EmptyStep();
    MakeMac(&one, NV2AOP_MOV, NV2ART_OUTPUT, NV2AOR_POS);
    one.mac.inputs[0] = Input(NV2ART_INPUT, 0);
    Finish(&one);
    Nv2aVshProgram program = { &one };
    VshCpuTransformOptions options = Options(constants);
    options.postprocess.surface_size[0] = 10.0f;
    options.postprocess.surface_size[1] = 20.0f;
    options.postprocess.clip_range[1] = 2.0f;
    inputs[0][0][0] = 5.123f;
    inputs[0][0][1] = -2.123f;
    inputs[0][0][2] = 3.25f;
    inputs[0][0][3] = 2.0f;
    VshCpuVertexOutput output = {};
    char error[128] = {};
    g_assert_true(TransformProgram(&program, 1, inputs, 1, options, &output,
                                   error, sizeof(error)) ==
                  VshCpuTransformStatus::Ready);
    g_assert_cmpfloat(output.oPos[0], ==, 0.025f);
    g_assert_cmpfloat(output.oPos[1], ==, 2.4125f);
    g_assert_cmpfloat(output.oPos[2], ==, 3.25f);
    g_assert_cmpfloat(output.position[0], ==, output.oPos[0]);
    g_assert_cmpfloat(output.color[0], ==, output.oD0[0]);

    inputs[0][0][3] = -0.0f;
    g_assert_true(TransformProgram(&program, 1, inputs, 1, options, &output,
                                   error, sizeof(error)) ==
                  VshCpuTransformStatus::Ready);
    g_assert_true(std::signbit(output.oPos[3]) && output.oPos[3] < 0.0f);

    std::vector<Nv2aVshStep> long_program(136, EmptyStep());
    for (unsigned i = 0; i < 135; ++i) {
        long_program[i].is_final = false;
    }
    MakeMac(&long_program.back(), NV2AOP_MOV);
    Finish(&long_program.back());
    program.steps = long_program.data();
    g_assert_true(TransformProgram(&program, 136, inputs, 1, options, &output,
                                   error, sizeof(error)) ==
                  VshCpuTransformStatus::Ready);
    g_assert_false(TransformProgram(&program, 137, inputs, 1, options, &output,
                                    error, sizeof(error)) ==
                   VshCpuTransformStatus::Ready);
}

void test_fog_defaults_and_ir_shapes(void)
{
    float inputs[1][16][4] = {};
    float constants[192][4] = {};
    InitInputs(inputs);
    inputs[0][0][0] = 4.0f;
    VshCpuTransformOptions options = Options(constants);
    VshCpuVertexOutput output = {};
    char error[128] = {};

    Nv2aVshStep fog = EmptyStep();
    MakeMac(&fog, NV2AOP_MOV, NV2ART_OUTPUT, NV2AOR_FOG_COORD, NV2AWM_YZW);
    Finish(&fog);
    Nv2aVshProgram program = { &fog };
    g_assert_true(TransformProgram(&program, 1, inputs, 1, options, &output,
                                   error, sizeof(error)) ==
                  VshCpuTransformStatus::Ready);
    g_assert_cmpfloat(output.oFog[0], ==, 4.0f);
    g_assert_cmpfloat(output.oFog[1], ==, 0.0f);
    g_assert_cmpfloat(output.oFog[3], ==, 1.0f);

    fog.mac.outputs[0].writemask = NV2AWM_Z;
    g_assert_true(TransformProgram(&program, 1, inputs, 1, options, &output,
                                   error, sizeof(error)) ==
                  VshCpuTransformStatus::Ready);
    g_assert_cmpfloat(output.oFog[0], ==, 4.0f);

    MakeMac(&fog, NV2AOP_MOV, NV2ART_OUTPUT, NV2AOR_DIFFUSE, NV2AWM_X);
    Finish(&fog);
    g_assert_true(TransformProgram(&program, 1, inputs, 1, options, &output,
                                   error, sizeof(error)) ==
                  VshCpuTransformStatus::Ready);
    g_assert_cmpfloat(output.oD0[0], ==, 1.0f);
    g_assert_cmpfloat(output.oD0[1], ==, 0.0f);
    g_assert_cmpfloat(output.oD0[3], ==, 1.0f);

    inputs[0][0][0] = std::numeric_limits<float>::quiet_NaN();
    MakeMac(&fog, NV2AOP_MOV, NV2ART_OUTPUT, NV2AOR_BACK_DIFFUSE, NV2AWM_X);
    Finish(&fog);
    g_assert_true(TransformProgram(&program, 1, inputs, 1, options, &output,
                                   error, sizeof(error)) ==
                  VshCpuTransformStatus::Ready);
    g_assert_cmpfloat(output.oB0[0], ==, 1.0f);
    inputs[0][0][0] = -2.0f;
    g_assert_true(TransformProgram(&program, 1, inputs, 1, options, &output,
                                   error, sizeof(error)) ==
                  VshCpuTransformStatus::Ready);
    g_assert_cmpfloat(output.oB0[0], ==, 0.0f);

    fog = EmptyStep();
    Finish(&fog);
    g_assert_true(TransformProgram(&program, 1, inputs, 1, options, &output,
                                   error, sizeof(error)) ==
                  VshCpuTransformStatus::Ready);
    /* Programmable GLSL defaults are (0, 0, 0, 1), then position
     * postprocess maps that origin to (-1, 1). */
    g_assert_cmpfloat(output.oPos[0], ==, -1.0f);
    g_assert_cmpfloat(output.oPos[1], ==, 1.0f);
    g_assert_cmpfloat(output.oPos[3], ==, 1.0f);

    MakeMac(&fog, NV2AOP_MOV);
    fog.mac.inputs[0] = Input(NV2ART_NONE, 0);
    Finish(&fog);
    g_assert_true(TransformProgram(&program, 1, inputs, 1, options, &output,
                                   error, sizeof(error)) ==
                  VshCpuTransformStatus::Invalid);
    MakeMac(&fog, NV2AOP_ADD);
    fog.mac.inputs[1] = Input(NV2ART_NONE, 0);
    Finish(&fog);
    g_assert_true(TransformProgram(&program, 1, inputs, 1, options, &output,
                                   error, sizeof(error)) ==
                  VshCpuTransformStatus::Invalid);
    MakeIlu(&fog, NV2AOP_ADD);
    Finish(&fog);
    g_assert_true(TransformProgram(&program, 1, inputs, 1, options, &output,
                                   error, sizeof(error)) ==
                  VshCpuTransformStatus::Invalid);
    fog = EmptyStep();
    fog.mac.opcode = NV2AOP_MOV;
    fog.mac.inputs[0] = Input(NV2ART_INPUT, 0);
    fog.mac.outputs[1] = Output(NV2ART_OUTPUT, NV2AOR_DIFFUSE, NV2AWM_X);
    Finish(&fog);
    g_assert_true(TransformProgram(&program, 1, inputs, 1, options, &output,
                                   error, sizeof(error)) ==
                  VshCpuTransformStatus::Invalid);

    MakeIlu(&fog, NV2AOP_MOV);
    fog.ilu.outputs[0] = Output(NV2ART_ADDRESS, 0, NV2AWM_X);
    Finish(&fog);
    g_assert_true(TransformProgram(&program, 1, inputs, 1, options, &output,
                                   error, sizeof(error)) ==
                  VshCpuTransformStatus::Invalid);

    MakeMac(&fog, NV2AOP_MOV, NV2ART_OUTPUT, 1, NV2AWM_X);
    Finish(&fog);
    g_assert_true(TransformProgram(&program, 1, inputs, 1, options, &output,
                                   error, sizeof(error)) ==
                  VshCpuTransformStatus::Invalid);

    MakeMac(&fog, NV2AOP_MOV);
    fog.mac.opcode = static_cast<Nv2aVshOpcode>(-1);
    Finish(&fog);
    g_assert_true(TransformProgram(&program, 1, inputs, 1, options, &output,
                                   error, sizeof(error)) ==
                  VshCpuTransformStatus::Invalid);
    MakeIlu(&fog, static_cast<Nv2aVshOpcode>(-1));
    Finish(&fog);
    g_assert_true(TransformProgram(&program, 1, inputs, 1, options, &output,
                                   error, sizeof(error)) ==
                  VshCpuTransformStatus::Invalid);
    MakeMac(&fog, NV2AOP_MOV);
    fog.mac.inputs[0].type = static_cast<Nv2aVshRegisterType>(-1);
    Finish(&fog);
    g_assert_true(TransformProgram(&program, 1, inputs, 1, options, &output,
                                   error, sizeof(error)) ==
                  VshCpuTransformStatus::Invalid);
    MakeMac(&fog, NV2AOP_MOV);
    fog.mac.outputs[0].type = static_cast<Nv2aVshRegisterType>(-1);
    Finish(&fog);
    g_assert_true(TransformProgram(&program, 1, inputs, 1, options, &output,
                                   error, sizeof(error)) ==
                  VshCpuTransformStatus::Invalid);
}

void test_tokens_plan_and_final_validation(void)
{
    static const uint32_t token[4] = {
        0x00000000,
        0x0020161B,
        0x0836106C,
        0x2070F859,
    };
    D3D11CanonicalVshInputs input = {};
    input.values[0][3] = 1.0f;
    input.values[1][0] = 0.25f;
    input.values[1][1] = 0.5f;
    input.values[1][2] = 0.75f;
    input.values[1][3] = 1.0f;
    xemu::D3D11VertexPlan plan;
    plan.vertices.resize(1);
    plan.vsh_inputs.push_back(input);
    plan.indices = { 0, 0, 0 };
    float constants[192][4] = {};
    VshCpuTransformOptions options = Options(constants);
    std::vector<VshCpuVertexOutput> outputs;
    char error[128] = {};
    g_assert_true(TransformPlan(token, 1, plan, options, &outputs, error,
                                sizeof(error)) == VshCpuTransformStatus::Ready);
    g_assert_cmpuint(outputs.size(), ==, 1);

    plan.indices = { 1, 0, 0 };
    g_assert_true(TransformPlan(token, 1, plan, options, &outputs, error,
                                sizeof(error)) ==
                  VshCpuTransformStatus::Invalid);
    plan.indices = { 0, 0 };
    g_assert_true(TransformPlan(token, 1, plan, options, &outputs, error,
                                sizeof(error)) ==
                  VshCpuTransformStatus::Invalid);
    plan.indices = { 0, 0, 0 };
    g_assert_true(
        TransformPlan(token, 1, plan, options, nullptr, error, sizeof(error)) ==
        VshCpuTransformStatus::Invalid);
    plan.vsh_inputs.clear();
    g_assert_true(TransformPlan(token, 1, plan, options, &outputs, error,
                                sizeof(error)) ==
                  VshCpuTransformStatus::Invalid);
    plan.vsh_inputs.push_back(input);
    g_assert_true(TransformTokens(token, 0, &input.values, 1, options,
                                  outputs.data(), error, sizeof(error)) ==
                  VshCpuTransformStatus::Invalid);
    g_assert_true(TransformTokens(token, 137, &input.values, 1, options,
                                  outputs.data(), error, sizeof(error)) ==
                  VshCpuTransformStatus::Invalid);
    uint32_t malformed[4] = {};
    std::memcpy(malformed, token, sizeof(malformed));
    malformed[1] =
        (malformed[1] & ~(UINT32_C(0xf) << 21)) | (UINT32_C(14) << 21);
    g_assert_true(TransformTokens(malformed, 1, &input.values, 1, options,
                                  outputs.data(), error, sizeof(error)) ==
                  VshCpuTransformStatus::Invalid);
    malformed[1] =
        (malformed[1] & ~(UINT32_C(0xf) << 21)) | (UINT32_C(15) << 21);
    g_assert_true(TransformTokens(malformed, 1, &input.values, 1, options,
                                  outputs.data(), error, sizeof(error)) ==
                  VshCpuTransformStatus::Invalid);

    Nv2aVshStep invalid_steps[2] = {};
    MakeMac(&invalid_steps[0], NV2AOP_MOV);
    Finish(&invalid_steps[0]);
    MakeMac(&invalid_steps[1], NV2AOP_MOV);
    Finish(&invalid_steps[1]);
    Nv2aVshProgram invalid = { invalid_steps };
    VshCpuVertexOutput output = {};
    g_assert_false(Ready(invalid, 2, &input.values, constants, &output));
}

} // namespace

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, nullptr);
    g_test_add_func("/pgraph/vsh-cpu/mac-opcodes", test_mac_opcodes);
    g_test_add_func("/pgraph/vsh-cpu/ilu-opcodes",
                    test_ilu_opcodes_and_exceptional_values);
    g_test_add_func("/pgraph/vsh-cpu/inputs",
                    test_swizzle_negate_mask_and_all_inputs);
    g_test_add_func("/pgraph/vsh-cpu/paired-r1",
                    test_paired_r1_and_partial_masks);
    g_test_add_func("/pgraph/vsh-cpu/arl-context",
                    test_arl_bounds_and_serial_context);
    g_test_add_func("/pgraph/vsh-cpu/limits-position",
                    test_limits_position_and_mapping);
    g_test_add_func("/pgraph/vsh-cpu/fog-defaults-ir",
                    test_fog_defaults_and_ir_shapes);
    g_test_add_func("/pgraph/vsh-cpu/tokens-plan-final",
                    test_tokens_plan_and_final_validation);
    return g_test_run();
}
