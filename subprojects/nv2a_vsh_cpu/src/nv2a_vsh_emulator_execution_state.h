#ifndef NV2A_VSH_CPU_SRC_NV2A_VSH_EMU_EXECUTION_STATE_H_
#define NV2A_VSH_CPU_SRC_NV2A_VSH_EMU_EXECUTION_STATE_H_

#include <stdbool.h>
#include <stdint.h>

#include "nv2a_vsh_cpu.h"
#include "nv2a_vsh_disassembler.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum Nv2aVshOutputRegisterName_ {
  NV2AOR_POS = 0,
  NV2AOR_DIFFUSE = 3,
  NV2AOR_SPECULAR = 4,
  NV2AOR_FOG_COORD = 5,
  NV2AOR_POINT_SIZE = 6,
  NV2AOR_BACK_DIFFUSE = 7,
  NV2AOR_BACK_SPECULAR = 8,
  NV2AOR_TEX0 = 9,
  NV2AOR_TEX1 = 10,
  NV2AOR_TEX2 = 11,
  NV2AOR_TEX3 = 12,
} Nv2aVshOutputRegisterName;

// Models the full execution context of the nv2a. Each entry is a 4-component
// float.
typedef struct Nv2aVshExecutionState_ {
  // v0-v15
  float *input_regs;
  // o0 - o12, 1 and 2 will never be written to.
  float *output_regs;
  // r0-r11
  float *temp_regs;
  // c0-c191
  float *context_regs;
  // a0
  float *address_reg;

  // Optional array of 192 bools that will be set when writing to entries in
  // context_regs.
  bool *context_dirty;
} Nv2aVshExecutionState;

// Stores the entire execution state for full software-based nv2a vertex shader
// emulation.
typedef struct Nv2aVshCPUFullExecutionState_ {
  float input_regs[16 * 4];
  float output_regs[13 * 4];
  float temp_regs[12 * 4];
  float context_regs[192 * 4];
  float address_reg[4];
} Nv2aVshCPUFullExecutionState;

// Models a partial execution context where the context registers are held
// externally. Intended for use in vertex state shaders that just write to the
// context registers.
typedef struct Nv2aVshCPUXVSSExecutionState_ {
  // Only v0 is used.
  float input_regs[1 * 4];

  // No output registers are used.
  float *output_regs;

  float temp_regs[11 * 4];

  // Context regs should be initialized to a flat array of 192 registers.
  float *context_regs;

  float address_reg[4];
} Nv2aVshCPUXVSSExecutionState;

// Initializes the given Nv2aVshCPUFullExecutionState and returns an
// Nv2aVshExecutionState appropriate for use with nv2a_vsh_cpu_* functions.
Nv2aVshExecutionState nv2a_vsh_emu_initialize_full_execution_state(
    Nv2aVshCPUFullExecutionState *state);

// Initializes the given Nv2aVshCPUXVSSExecutionState and returns an
// Nv2aVshExecutionState appropriate for use with nv2a_vsh_cpu_* functions.
Nv2aVshExecutionState nv2a_vsh_emu_initialize_xss_execution_state(
    Nv2aVshCPUXVSSExecutionState *state, float *context_regs);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // NV2A_VSH_CPU_SRC_NV2A_VSH_EMU_EXECUTION_STATE_H_
