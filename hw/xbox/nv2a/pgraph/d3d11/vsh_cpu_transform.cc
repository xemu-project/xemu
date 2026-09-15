/* CPU-authoritative NV2A programmable VSH transform. */

#include "vsh_cpu_transform.h"

#include "nv2a_vsh_emulator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

namespace xemu {
namespace d3d11_vsh {
namespace {

void SetError(char *error, size_t error_size, const char *message)
{
    if (error != nullptr && error_size != 0) {
        std::snprintf(error, error_size, "%s", message);
    }
}

bool CheckInput(const Nv2aVshInput &input, char *error, size_t error_size)
{
    if (input.type < NV2ART_NONE || input.type > NV2ART_ADDRESS) {
        SetError(error, error_size, "invalid VSH input register type");
        return false;
    }
    switch (input.type) {
    case NV2ART_NONE:
        return true;
    case NV2ART_TEMPORARY:
        if (input.index > kMaxTemps) {
            SetError(error, error_size,
                     "VSH temporary input index out of range");
            return false;
        }
        break;
    case NV2ART_INPUT:
        if (input.index >= NV2A_VERTEXSHADER_ATTRIBUTES) {
            SetError(error, error_size, "VSH vertex input index out of range");
            return false;
        }
        break;
    case NV2ART_CONTEXT:
        if (input.index >= kMaxConstants) {
            SetError(error, error_size,
                     "VSH constant input index out of range");
            return false;
        }
        break;
    case NV2ART_OUTPUT:
        SetError(error, error_size, "VSH output is not a readable input type");
        return false;
    case NV2ART_ADDRESS:
        SetError(error, error_size, "A0 is not a readable VSH input");
        return false;
    }
    for (unsigned component : input.swizzle) {
        if (component > NV2ASW_W) {
            SetError(error, error_size,
                     "VSH input swizzle component out of range");
            return false;
        }
    }
    return true;
}

bool CheckOutput(const Nv2aVshOutput &output, char *error, size_t error_size)
{
    if (output.type < NV2ART_NONE || output.type > NV2ART_ADDRESS ||
        output.writemask < 0 || output.writemask > NV2AWM_XYZW) {
        SetError(error, error_size, "invalid VSH output register");
        return false;
    }
    switch (output.type) {
    case NV2ART_NONE:
        return true;
    case NV2ART_TEMPORARY:
        if (output.index >= kMaxTemps) {
            SetError(error, error_size,
                     "VSH temporary output index out of range");
            return false;
        }
        break;
    case NV2ART_INPUT:
        SetError(error, error_size, "VSH cannot write vertex inputs");
        return false;
    case NV2ART_OUTPUT:
        /* o1/o2 are reserved in the CPU authority and are never written by
         * the NV2A execution model; reject them instead of silently exposing
         * hidden registers through the public wrapper. */
        if (output.index >= kMaxOutputs || output.index == 1 ||
            output.index == 2) {
            SetError(error, error_size, "VSH output index out of range");
            return false;
        }
        break;
    case NV2ART_CONTEXT:
        if (output.index >= kMaxConstants) {
            SetError(error, error_size,
                     "VSH context output index out of range");
            return false;
        }
        break;
    case NV2ART_ADDRESS:
        if (output.index != 0 || output.writemask != 0) {
            SetError(error, error_size, "invalid VSH A0 output");
            return false;
        }
        break;
    }
    return true;
}

unsigned RequiredInputs(Nv2aVshOpcode opcode, bool ilu)
{
    if (opcode == NV2AOP_NOP) {
        return 0;
    }
    if (ilu) {
        return 1;
    }
    if (opcode == NV2AOP_MOV || opcode == NV2AOP_ARL) {
        return 1;
    }
    if (opcode == NV2AOP_MAD) {
        return 3;
    }
    return 2;
}

bool CheckOperation(const Nv2aVshOperation &operation, bool ilu, char *error,
                    size_t error_size)
{
    const bool valid_opcode = operation.opcode >= NV2AOP_NOP &&
                              (ilu ? (operation.opcode == NV2AOP_NOP ||
                                      operation.opcode == NV2AOP_MOV ||
                                      (operation.opcode >= NV2AOP_RCP &&
                                       operation.opcode <= NV2AOP_LIT)) :
                                     operation.opcode <= NV2AOP_ARL);
    if (!valid_opcode) {
        SetError(error, error_size, "invalid VSH opcode");
        return false;
    }
    const unsigned input_count = RequiredInputs(operation.opcode, ilu);
    for (unsigned i = 0; i < 3; ++i) {
        const Nv2aVshInput &input = operation.inputs[i];
        if (!CheckInput(input, error, error_size) ||
            (i < input_count) != (input.type != NV2ART_NONE)) {
            SetError(error, error_size, "invalid VSH operation input arity");
            return false;
        }
    }
    if (operation.opcode == NV2AOP_NOP) {
        if (operation.outputs[0].type != NV2ART_NONE ||
            operation.outputs[1].type != NV2ART_NONE) {
            SetError(error, error_size, "NOP has VSH outputs");
            return false;
        }
        return true;
    }
    for (unsigned i = 0; i < 2; ++i) {
        const Nv2aVshOutput &output = operation.outputs[i];
        if (!CheckOutput(output, error, error_size) ||
            (i == 1 && operation.outputs[0].type == NV2ART_NONE &&
             output.type != NV2ART_NONE) ||
            (output.type != NV2ART_NONE && output.writemask == 0 &&
             !(operation.opcode == NV2AOP_ARL && !ilu && i == 0 &&
               output.type == NV2ART_ADDRESS))) {
            SetError(error, error_size, "invalid VSH operation output shape");
            return false;
        }
    }
    if (!ilu && operation.opcode == NV2AOP_ARL &&
        (operation.outputs[0].type != NV2ART_ADDRESS ||
         operation.outputs[1].type != NV2ART_NONE ||
         operation.inputs[0].type == NV2ART_NONE ||
         operation.inputs[1].type != NV2ART_NONE ||
         operation.inputs[2].type != NV2ART_NONE)) {
        SetError(error, error_size, "invalid VSH ARL shape");
        return false;
    }
    if ((ilu || operation.opcode != NV2AOP_ARL) &&
        (operation.outputs[0].type == NV2ART_ADDRESS ||
         operation.outputs[1].type == NV2ART_ADDRESS)) {
        SetError(error, error_size, "non-ARL operation writes A0");
        return false;
    }
    return true;
}

bool CheckProgram(const Nv2aVshProgram *program, uint32_t instruction_count,
                  char *error, size_t error_size)
{
    if (program == nullptr || program->steps == nullptr ||
        instruction_count == 0 || instruction_count > kMaxInstructions) {
        SetError(error, error_size, "invalid VSH program size or pointer");
        return false;
    }
    bool saw_final = false;
    for (uint32_t i = 0; i < instruction_count; ++i) {
        const Nv2aVshStep &step = program->steps[i];
        if (saw_final || !CheckOperation(step.mac, false, error, error_size) ||
            !CheckOperation(step.ilu, true, error, error_size)) {
            if (!saw_final) {
                return false;
            }
            SetError(error, error_size, "VSH instruction follows FINAL");
            return false;
        }
        if (step.is_final) {
            saw_final = true;
            if (i + 1 != instruction_count) {
                SetError(error, error_size,
                         "FINAL must terminate the VSH program");
                return false;
            }
        }
    }
    if (!saw_final) {
        SetError(error, error_size, "VSH program has no FINAL instruction");
        return false;
    }
    return true;
}

void NormalizePairedOutputs(Nv2aVshStep *step)
{
    if (step->mac.opcode == NV2AOP_NOP || step->ilu.opcode == NV2AOP_NOP) {
        return;
    }
    for (Nv2aVshOutput &output : step->mac.outputs) {
        if (output.type == NV2ART_TEMPORARY && output.index == 1) {
            output = {};
        }
    }
    for (Nv2aVshOutput &output : step->ilu.outputs) {
        if (output.type == NV2ART_TEMPORARY) {
            output.index = 1;
        }
    }
}

void NormalizeFogOutputs(Nv2aVshStep *step)
{
    Nv2aVshOutput *outputs[] = { step->mac.outputs, step->ilu.outputs };
    for (Nv2aVshOutput *pair : outputs) {
        for (unsigned i = 0; i < 2; ++i) {
            Nv2aVshOutput &output = pair[i];
            if (output.type == NV2ART_OUTPUT &&
                output.index == NV2AOR_FOG_COORD && output.writemask != 0) {
                /* GLSL's programmable path exposes oFog as a scalar: every
                 * non-empty destination mask writes result.x to oFog.x. */
                output.writemask = NV2AWM_X;
            }
        }
    }
}

bool RelativeContextOob(const Nv2aVshInput &input,
                        const Nv2aVshExecutionState &state)
{
    if (input.type != NV2ART_CONTEXT || !input.is_relative) {
        return false;
    }
    const float address = state.address_reg[0];
    if (!std::isfinite(address) ||
        address < static_cast<float>(std::numeric_limits<int32_t>::min()) ||
        address > static_cast<float>(std::numeric_limits<int32_t>::max())) {
        return true;
    }
    const int64_t offset = static_cast<int64_t>(input.index) +
                           static_cast<int64_t>(static_cast<int32_t>(address));
    return offset < 0 || offset >= kMaxConstants;
}

void MakeSafeInput(Nv2aVshInput *input, const Nv2aVshExecutionState &state)
{
    if (RelativeContextOob(*input, state)) {
        /* The emulator's normal context fetch is intentionally a direct array
         * access.  Redirect malformed guest-relative fetches to the extra
         * zero lane owned by this wrapper instead of allowing an OOB read. */
        input->index = kMaxConstants;
        input->is_relative = false;
    }
}

void MakeSafeStep(Nv2aVshStep *step, const Nv2aVshExecutionState &state)
{
    for (Nv2aVshInput &input : step->mac.inputs) {
        MakeSafeInput(&input, state);
    }
    for (Nv2aVshInput &input : step->ilu.inputs) {
        MakeSafeInput(&input, state);
    }
    NormalizePairedOutputs(step);
    NormalizeFogOutputs(step);
    /* The decoder represents ARL's synthetic A0 destination with mask zero;
     * the CPU emulator's generic masked write would consequently suppress it.
     * Make the synthetic destination writable for this wrapper only. */
    if (step->mac.opcode == NV2AOP_ARL &&
        step->mac.outputs[0].type == NV2ART_ADDRESS &&
        step->mac.outputs[0].writemask == 0) {
        step->mac.outputs[0].writemask = NV2AWM_XYZW;
    }
}

float BitsFloat(uint32_t bits)
{
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void PostprocessPosition(float position[4], const VshPostprocess &postprocess)
{
    position[0] = std::trunc(position[0] * 16.0f) / 16.0f;
    position[1] = std::trunc(position[1] * 16.0f) / 16.0f;
    const uint32_t w_bits = [&position]() {
        uint32_t bits = 0;
        std::memcpy(&bits, &position[3], sizeof(bits));
        return bits;
    }();
    float w = position[3];
    if (w > 0.0f || w_bits == 0) {
        w = (std::max)(BitsFloat(UINT32_C(0x1f800000)),
                       (std::min)(w, BitsFloat(UINT32_C(0x5f800000))));
    } else {
        w = (std::max)(BitsFloat(UINT32_C(0xdf800000)),
                       (std::min)(w, BitsFloat(UINT32_C(0x9f800000))));
    }
    const float ndc_x = (2.0f * position[0] - postprocess.surface_size[0]) /
                        postprocess.surface_size[0];
    const float ndc_y = (2.0f * position[1] - postprocess.surface_size[1]) /
                        postprocess.surface_size[1];
    position[0] = ndc_x * w;
    position[1] = -ndc_y * w;
    position[2] = position[2] / postprocess.clip_range[1] * w;
    position[3] = w;
}

bool ValidPostprocess(const VshPostprocess &postprocess)
{
    return std::isfinite(postprocess.surface_size[0]) &&
           std::isfinite(postprocess.surface_size[1]) &&
           postprocess.surface_size[0] > 0.0f &&
           postprocess.surface_size[1] > 0.0f &&
           std::isfinite(postprocess.clip_range[1]) &&
           postprocess.clip_range[1] != 0.0f;
}

bool ValidRawTokenOpcodes(const uint32_t *token)
{
    const uint32_t mac_opcode = (token[1] >> 21) & 0xf;
    const uint32_t ilu_opcode = (token[1] >> 25) & 0x7;
    /* The shared C parser indexes the MAC table directly, so reject its two
     * reserved four-bit encodings before entering the parser.  All eight ILU
     * three-bit encodings are defined by the token format. */
    return mac_opcode <= 13 && ilu_opcode <= 7;
}

float ClampColorComponent(float value)
{
    if (std::isnan(value)) {
        value = 1.0f;
    }
    return (std::max)(0.0f, (std::min)(value, 1.0f));
}

void ClampColor(float color[4])
{
    for (unsigned component = 0; component < 4; ++component) {
        color[component] = ClampColorComponent(color[component]);
    }
}

void ExpandOutput(const Nv2aVshCPUFullExecutionState &state,
                  const VshPostprocess &postprocess, VshCpuVertexOutput *output)
{
    std::memcpy(output->oPos, state.output_regs + NV2AOR_POS * 4,
                sizeof(output->oPos));
    PostprocessPosition(output->oPos, postprocess);
    std::memcpy(output->oD0, state.output_regs + NV2AOR_DIFFUSE * 4,
                sizeof(output->oD0));
    ClampColor(output->oD0);
    std::memcpy(output->oD1, state.output_regs + NV2AOR_SPECULAR * 4,
                sizeof(output->oD1));
    std::memcpy(output->oFog, state.output_regs + NV2AOR_FOG_COORD * 4,
                sizeof(output->oFog));
    std::memcpy(output->oPts, state.output_regs + NV2AOR_POINT_SIZE * 4,
                sizeof(output->oPts));
    std::memcpy(output->oB0, state.output_regs + NV2AOR_BACK_DIFFUSE * 4,
                sizeof(output->oB0));
    ClampColor(output->oB0);
    std::memcpy(output->oB1, state.output_regs + NV2AOR_BACK_SPECULAR * 4,
                sizeof(output->oB1));
    std::memcpy(output->oT0, state.output_regs + NV2AOR_TEX0 * 4,
                sizeof(output->oT0));
    std::memcpy(output->oT1, state.output_regs + NV2AOR_TEX1 * 4,
                sizeof(output->oT1));
    std::memcpy(output->oT2, state.output_regs + NV2AOR_TEX2 * 4,
                sizeof(output->oT2));
    std::memcpy(output->oT3, state.output_regs + NV2AOR_TEX3 * 4,
                sizeof(output->oT3));
    std::memcpy(output->position, output->oPos, sizeof(output->position));
    std::memcpy(output->color, output->oD0, sizeof(output->color));
}

} // namespace

VshPostprocess DefaultVshCpuPostprocess()
{
    return { { 640.0f, 480.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f } };
}

VshCpuTransformStatus
TransformProgram(const Nv2aVshProgram *program, uint32_t instruction_count,
                 const float (*inputs)[NV2A_VERTEXSHADER_ATTRIBUTES][4],
                 uint32_t vertex_count, const VshCpuTransformOptions &options,
                 VshCpuVertexOutput *outputs, char *error, size_t error_size)
{
    if (!CheckProgram(program, instruction_count, error, error_size) ||
        inputs == nullptr || outputs == nullptr || vertex_count == 0 ||
        options.constants == nullptr) {
        if (inputs == nullptr || outputs == nullptr || vertex_count == 0 ||
            options.constants == nullptr) {
            SetError(error, error_size, "invalid VSH transform input");
        }
        return VshCpuTransformStatus::Invalid;
    }
    VshPostprocess postprocess = options.postprocess;
    if (!ValidPostprocess(postprocess)) {
        SetError(error, error_size, "invalid VSH postprocess range");
        return VshCpuTransformStatus::Invalid;
    }

    float context[kMaxConstants + 1][4] = {};
    std::memcpy(context, options.constants, sizeof(float) * kMaxConstants * 4);
    for (uint32_t vertex = 0; vertex < vertex_count; ++vertex) {
        Nv2aVshCPUFullExecutionState full = {};
        Nv2aVshExecutionState state =
            nv2a_vsh_emu_initialize_full_execution_state(&full);
        state.context_regs = &context[0][0];
        /* The existing programmable GLSL path is the output-default
         * authority: every output starts as (0, 0, 0, 1).  The generic HLSL
         * interpreter is not authoritative for this CPU fallback. */
        for (unsigned output = 0; output < kMaxOutputs; ++output) {
            full.output_regs[output * 4 + 3] = 1.0f;
        }
        std::memcpy(full.input_regs, inputs[vertex], sizeof(full.input_regs));
        if (!options.serialize_context_writes && vertex != 0) {
            std::memcpy(context, options.constants,
                        sizeof(float) * kMaxConstants * 4);
        }
        std::memset(context[kMaxConstants], 0, sizeof(context[kMaxConstants]));
        for (uint32_t instruction = 0; instruction < instruction_count;
             ++instruction) {
            Nv2aVshStep step = program->steps[instruction];
            MakeSafeStep(&step, state);
            nv2a_vsh_emu_apply(&state, &step);
        }
        std::memcpy(full.context_regs, context,
                    sizeof(float) * kMaxConstants * 4);
        ExpandOutput(full, postprocess, &outputs[vertex]);
    }
    return VshCpuTransformStatus::Ready;
}

VshCpuTransformStatus
TransformTokens(const uint32_t *tokens, uint32_t token_count,
                const float (*inputs)[NV2A_VERTEXSHADER_ATTRIBUTES][4],
                uint32_t vertex_count, const VshCpuTransformOptions &options,
                VshCpuVertexOutput *outputs, char *error, size_t error_size)
{
    if (tokens == nullptr || token_count == 0 ||
        token_count > kMaxInstructions) {
        SetError(error, error_size, "VSH token count must be 1..136");
        return VshCpuTransformStatus::Invalid;
    }
    for (uint32_t instruction = 0; instruction < token_count; ++instruction) {
        if (!ValidRawTokenOpcodes(tokens + instruction * 4)) {
            SetError(error, error_size, "invalid raw VSH opcode fields");
            return VshCpuTransformStatus::Invalid;
        }
    }
    Nv2aVshProgram program = {};
    const Nv2aVshParseResult result =
        nv2a_vsh_parse_program(&program, tokens, token_count);
    if (result != NV2AVPR_SUCCESS) {
        SetError(error, error_size, "VSH token decode failed");
        return VshCpuTransformStatus::Invalid;
    }
    const VshCpuTransformStatus status =
        TransformProgram(&program, token_count, inputs, vertex_count, options,
                         outputs, error, error_size);
    nv2a_vsh_program_destroy(&program);
    return status;
}

VshCpuTransformStatus TransformPlan(const uint32_t *tokens,
                                    uint32_t token_count,
                                    const D3D11VertexPlan &plan,
                                    const VshCpuTransformOptions &options,
                                    std::vector<VshCpuVertexOutput> *outputs,
                                    char *error, size_t error_size)
{
    if (outputs == nullptr || plan.vsh_inputs.size() != plan.vertices.size() ||
        plan.vsh_inputs.empty() ||
        plan.vertices.size() > std::numeric_limits<uint32_t>::max() ||
        plan.vsh_inputs.size() > std::numeric_limits<uint32_t>::max() ||
        plan.indices.size() > std::numeric_limits<uint32_t>::max() ||
        plan.indices.empty() || plan.indices.size() % 3 != 0) {
        SetError(error, error_size, "VSH plan has no complete input stream");
        return VshCpuTransformStatus::Invalid;
    }
    for (uint32_t index : plan.indices) {
        if (index >= plan.vertices.size()) {
            SetError(error, error_size, "VSH plan index is out of range");
            return VshCpuTransformStatus::Invalid;
        }
    }
    outputs->assign(plan.vsh_inputs.size(), {});
    const VshCpuTransformStatus status = TransformTokens(
        tokens, token_count,
        reinterpret_cast<const float (*)[NV2A_VERTEXSHADER_ATTRIBUTES][4]>(
            plan.vsh_inputs.data()),
        static_cast<uint32_t>(plan.vsh_inputs.size()), options, outputs->data(),
        error, error_size);
    if (status != VshCpuTransformStatus::Ready) {
        outputs->clear();
    }
    return status;
}

} // namespace d3d11_vsh
} // namespace xemu
