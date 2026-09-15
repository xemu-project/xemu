// Offline universal texture-free NV2A PSH interpreter upload layout.

#ifndef XEMU_NV2A_PGRAPH_D3D11_PSH_INTERPRETER_H
#define XEMU_NV2A_PGRAPH_D3D11_PSH_INTERPRETER_H

#include <cstddef>
#include <cstdint>

#include "hw/xbox/nv2a/pgraph/psh_reference.h"

namespace xemu {
namespace d3d11_psh {

struct ShaderBytecode {
    const unsigned char *vertex_shader;
    size_t vertex_shader_size;
    const unsigned char *pixel_shader;
    size_t pixel_shader_size;
};

struct alignas(16) InterpreterConstants {
    /* program[0..3] = control, stage program, other-stage input, final 0;
     * program[4] = final 1; the remaining words are reserved padding. */
    uint32_t program[8];
    uint32_t rgb_inputs[8][4];
    uint32_t alpha_inputs[8][4];
    uint32_t rgb_outputs[8][4];
    uint32_t alpha_outputs[8][4];
    /* Reference inputs are retained for deterministic upload/parity fixtures;
     * the runtime VS supplies the equivalent interpolants through VSOutput. */
    float v0[4];
    float v1[4];
    float t[4][4];
    float fog[4];
    float c0[9][4];
    float c1[9][4];
};

static_assert(offsetof(InterpreterConstants, program) == 0);
static_assert(offsetof(InterpreterConstants, rgb_inputs) == 32);
static_assert(offsetof(InterpreterConstants, alpha_inputs) == 160);
static_assert(offsetof(InterpreterConstants, rgb_outputs) == 288);
static_assert(offsetof(InterpreterConstants, alpha_outputs) == 416);
static_assert(offsetof(InterpreterConstants, v0) == 544);
static_assert(offsetof(InterpreterConstants, v1) == 560);
static_assert(offsetof(InterpreterConstants, t) == 576);
static_assert(offsetof(InterpreterConstants, fog) == 640);
static_assert(offsetof(InterpreterConstants, c0) == 656);
static_assert(offsetof(InterpreterConstants, c1) == 800);
static_assert(sizeof(InterpreterConstants) == 944);

/* Validates raw PGRAPH encodings, then converts them to the exact cbuffer
 * layout consumed by psh_interpreter.hlsl.  No D3D runtime/compiler calls are
 * made here. */
bool BuildConstants(const PshReferenceProgram &program,
                    const PshReferenceInputs &inputs,
                    InterpreterConstants *constants, char *error,
                    size_t error_size);

/* Offline-generated SM5 bytecode.  Keeping this accessor in the production
 * helper makes every D3D11 consumer use the checked-in, hashed artifact. */
ShaderBytecode GetShaderBytecode();

} // namespace d3d11_psh
} // namespace xemu

#endif // XEMU_NV2A_PGRAPH_D3D11_PSH_INTERPRETER_H
