/* CPU-authoritative NV2A programmable VSH transform. */

#ifndef XEMU_NV2A_PGRAPH_D3D11_VSH_CPU_TRANSFORM_H
#define XEMU_NV2A_PGRAPH_D3D11_VSH_CPU_TRANSFORM_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "nv2a_vsh_disassembler.h"

#include "vertex-adapter.h"
#include "vsh_common.h"

namespace xemu {
namespace d3d11_vsh {

enum class VshCpuTransformStatus : uint8_t {
    Ready,
    Invalid,
};

/* Expanded values match the programmable NV2A outputs used by the D3D11
 * fallback.  `position` and `color` retain the legacy draw-plan spelling. */
struct VshCpuVertexOutput {
    float position[3] = {};
    float color[4] = {};
    float oPos[4] = {};
    float oD0[4] = {};
    float oD1[4] = {};
    float oFog[4] = {};
    float oPts[4] = {};
    float oB0[4] = {};
    float oB1[4] = {};
    float oT0[4] = {};
    float oT1[4] = {};
    float oT2[4] = {};
    float oT3[4] = {};
};

struct VshCpuTransformOptions {
    const float (*constants)[4] = nullptr;
    VshPostprocess postprocess = {};
    /* Context outputs are stateful NV2A vertex-shader operations.  When set,
     * writes from vertex N are visible to vertex N+1 in the supplied order. */
    bool serialize_context_writes = true;
};

VshPostprocess DefaultVshCpuPostprocess();

VshCpuTransformStatus
TransformProgram(const Nv2aVshProgram *program, uint32_t instruction_count,
                 const float (*inputs)[NV2A_VERTEXSHADER_ATTRIBUTES][4],
                 uint32_t vertex_count, const VshCpuTransformOptions &options,
                 VshCpuVertexOutput *outputs, char *error, size_t error_size);

VshCpuTransformStatus
TransformTokens(const uint32_t *tokens, uint32_t token_count,
                const float (*inputs)[NV2A_VERTEXSHADER_ATTRIBUTES][4],
                uint32_t vertex_count, const VshCpuTransformOptions &options,
                VshCpuVertexOutput *outputs, char *error, size_t error_size);

VshCpuTransformStatus TransformPlan(const uint32_t *tokens,
                                    uint32_t token_count,
                                    const D3D11VertexPlan &plan,
                                    const VshCpuTransformOptions &options,
                                    std::vector<VshCpuVertexOutput> *outputs,
                                    char *error, size_t error_size);

} // namespace d3d11_vsh
} // namespace xemu

#endif // XEMU_NV2A_PGRAPH_D3D11_VSH_CPU_TRANSFORM_H
