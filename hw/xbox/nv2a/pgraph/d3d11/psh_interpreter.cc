// Packed NV2A PSH state to the offline interpreter cbuffer.

#include "psh_interpreter.h"
#include "psh_interpreter_shaders.h"

#include <cstring>

namespace xemu {
namespace d3d11_psh {

bool BuildConstants(const PshReferenceProgram &program,
                    const PshReferenceInputs &inputs,
                    InterpreterConstants *constants, char *error,
                    size_t error_size)
{
    if (constants == nullptr) {
        if (error != nullptr && error_size != 0) {
            const char message[] = "constants is null";
            const size_t count = (sizeof(message) - 1 < error_size - 1) ?
                                     sizeof(message) - 1 :
                                     error_size - 1;
            std::memcpy(error, message, count);
            error[count] = '\0';
        }
        return false;
    }
    if (!psh_reference_validate(&program, error, error_size)) {
        return false;
    }

    std::memset(constants, 0, sizeof(*constants));
    constants->program[0] = program.combiner_control;
    constants->program[1] = program.shader_stage_program;
    constants->program[2] = program.other_stage_input;
    constants->program[3] = program.final_inputs_0;
    constants->program[4] = program.final_inputs_1;
    for (unsigned i = 0; i < 8; ++i) {
        /* HLSL run_stage consumes A/B/C/D as x/y/z/w.  PGRAPH packs those
         * bytes in descending order, exactly as parse_inputs does. */
        const uint32_t rgb = program.rgb_inputs[i];
        const uint32_t alpha = program.alpha_inputs[i];
        constants->rgb_inputs[i][0] = (rgb >> 24) & 0xff;
        constants->rgb_inputs[i][1] = (rgb >> 16) & 0xff;
        constants->rgb_inputs[i][2] = (rgb >> 8) & 0xff;
        constants->rgb_inputs[i][3] = rgb & 0xff;
        constants->alpha_inputs[i][0] = (alpha >> 24) & 0xff;
        constants->alpha_inputs[i][1] = (alpha >> 16) & 0xff;
        constants->alpha_inputs[i][2] = (alpha >> 8) & 0xff;
        constants->alpha_inputs[i][3] = alpha & 0xff;
        constants->rgb_outputs[i][0] = program.rgb_outputs[i];
        constants->alpha_outputs[i][0] = program.alpha_outputs[i];
    }
    std::memcpy(constants->v0, &inputs.v0, sizeof(constants->v0));
    std::memcpy(constants->v1, &inputs.v1, sizeof(constants->v1));
    std::memcpy(constants->t, inputs.t, sizeof(constants->t));
    std::memcpy(constants->fog, &inputs.fog, sizeof(constants->fog));
    std::memcpy(constants->c0, inputs.c0, sizeof(constants->c0));
    std::memcpy(constants->c1, inputs.c1, sizeof(constants->c1));
    return true;
}

ShaderBytecode GetShaderBytecode()
{
    return { kVertexShader, kVertexShaderSize, kPixelShader, kPixelShaderSize };
}

} // namespace d3d11_psh
} // namespace xemu
