/*
 * Texture-free NV2A register-combiner reference evaluator.
 *
 * This is deliberately renderer independent.  It is an offline/reference
 * implementation of the subset described by psh_regs.h; it is not a PGRAPH
 * renderer and is not registered with any renderer backend.
 */

#ifndef HW_XBOX_NV2A_PGRAPH_PSH_REFERENCE_H
#define HW_XBOX_NV2A_PGRAPH_PSH_REFERENCE_H

#include "hw/xbox/nv2a/pgraph/psh_regs.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct PshReferenceVec4 {
    float x, y, z, w;
} PshReferenceVec4;

/* Values interpolated into a texture-free pixel.  Constants are indexed by
 * combiner stage; slot 8 is the final-combiner constant slot. */
typedef struct PshReferenceInputs {
    PshReferenceVec4 v0, v1;
    PshReferenceVec4 t[4];
    /* Matches GLSL pFog: fog.rgb is the fog color and fog.w is the
     * interpolated fog factor, clamped to [0,1] by the evaluator. */
    PshReferenceVec4 fog;
    PshReferenceVec4 c0[9];
    PshReferenceVec4 c1[9];
} PshReferenceInputs;

/* Register encodings are the same values written to NV_PGRAPH_* registers.
 * The arrays above are intentionally raw so tests can exercise hardware
 * packing and validation without a renderer-specific intermediate format. */
typedef struct PshReferenceProgram {
    uint32_t combiner_control;
    uint32_t shader_stage_program;
    uint32_t other_stage_input;
    uint32_t final_inputs_0;
    uint32_t final_inputs_1;
    uint32_t rgb_inputs[8];
    uint32_t rgb_outputs[8];
    uint32_t alpha_inputs[8];
    uint32_t alpha_outputs[8];
} PshReferenceProgram;

typedef struct PshReferenceResult {
    PshReferenceVec4 color;
    PshReferenceVec4 r0;
    PshReferenceVec4 r1;
} PshReferenceResult;

#ifdef __cplusplus
extern "C" {
#endif

/* Returns false and writes a stable, human-readable reason into error when
 * any unsupported texture mode, register encoding, flag, or stage is found. */
bool psh_reference_validate(const PshReferenceProgram *program, char *error,
                            size_t error_size);

/* Evaluates a previously validated program.  The evaluator validates again
 * so callers cannot accidentally execute an unsupported encoding. */
bool psh_reference_evaluate(const PshReferenceProgram *program,
                            const PshReferenceInputs *inputs,
                            PshReferenceResult *result, char *error,
                            size_t error_size);

#ifdef __cplusplus
}
#endif

#endif /* HW_XBOX_NV2A_PGRAPH_PSH_REFERENCE_H */
