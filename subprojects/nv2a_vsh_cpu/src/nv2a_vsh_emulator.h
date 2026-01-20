#ifndef NV2A_VSH_CPU_SRC_NV2A_VSH_EMULATOR_H_
#define NV2A_VSH_CPU_SRC_NV2A_VSH_EMULATOR_H_

#include "nv2a_vsh_emulator_execution_state.h"

#ifdef __cplusplus
extern "C" {
#endif

// Emulates the given program by applying each step to the given state.
void nv2a_vsh_emu_execute(Nv2aVshExecutionState *state,
                          const Nv2aVshProgram *program);

// context_dirty is an array of 192 bools that will be set when writing to
// entries in context_regs.
void nv2a_vsh_emu_execute_track_context_writes(Nv2aVshExecutionState *state,
                                               const Nv2aVshProgram *program,
                                               bool *context_dirty);

// Emulates the given step by applying it to the given state.
void nv2a_vsh_emu_apply(Nv2aVshExecutionState *state, const Nv2aVshStep *step);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // NV2A_VSH_CPU_SRC_NV2A_VSH_EMULATOR_H_
