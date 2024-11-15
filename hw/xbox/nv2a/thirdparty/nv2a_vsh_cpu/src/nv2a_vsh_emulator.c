#include "nv2a_vsh_emulator.h"

#include <assert.h>
#include <string.h>

// clang format off
static Nv2aVshCpuFunc kDispatchTable[] = {
    NULL,
    nv2a_vsh_cpu_mov,
    nv2a_vsh_cpu_mul,
    nv2a_vsh_cpu_add,
    nv2a_vsh_cpu_mad,
    nv2a_vsh_cpu_dp3,
    nv2a_vsh_cpu_dph,
    nv2a_vsh_cpu_dp4,
    nv2a_vsh_cpu_dst,
    nv2a_vsh_cpu_min,
    nv2a_vsh_cpu_max,
    nv2a_vsh_cpu_slt,
    nv2a_vsh_cpu_sge,
    nv2a_vsh_cpu_arl,
    nv2a_vsh_cpu_rcp,
    nv2a_vsh_cpu_rcc,
    nv2a_vsh_cpu_rsq,
    nv2a_vsh_cpu_exp,
    nv2a_vsh_cpu_log,
    nv2a_vsh_cpu_lit,
};
// clang format on

static inline void set_register(float *out, const float *in,
                                const uint8_t *swizzle, bool negate) {
  float mult = negate ? -1.0f : 1.0f;
  out[0] = mult * in[swizzle[0]];
  out[1] = mult * in[swizzle[1]];
  out[2] = mult * in[swizzle[2]];
  out[3] = mult * in[swizzle[3]];
}

static inline void fetch_value(float *out, const Nv2aVshExecutionState *state,
                               const Nv2aVshInput *input) {
  const float *in;

  switch (input->type) {
    default:
      assert(!"Invalid input type");

    case NV2ART_TEMPORARY:
      if (input->index == 12) {
        in = state->output_regs;
      } else {
        in = &state->temp_regs[input->index * 4];
      }
      break;

    case NV2ART_INPUT:
      in = &state->input_regs[input->index * 4];
      break;

    case NV2ART_CONTEXT: {
      uint32_t offset = input->index;
      if (input->is_relative) {
        offset += (int)state->address_reg[0];
      }
      in = &state->context_regs[offset * 4];
    } break;
  }

  set_register(out, in, input->swizzle, input->is_negated);
}

static inline void apply_operation(Nv2aVshExecutionState *state,
                                   const Nv2aVshOperation *op,
                                   const float *inputs) {
  if (op->opcode == NV2AOP_NOP) {
    return;
  }

  float result[4];
  kDispatchTable[op->opcode](result, inputs);

  const Nv2aVshOutput *out = op->outputs;
  for (uint32_t i = 0; i < 2; ++i, ++out) {
    float *outreg;
    switch (out->type) {
      case NV2ART_INPUT:
        assert(!"Attempt to write to input register.");

      case NV2ART_NONE:
        continue;

      case NV2ART_OUTPUT:
        assert(out->index < 13 && "Invalid result register target.");
        outreg = &state->output_regs[out->index * 4];
        break;

      case NV2ART_TEMPORARY:
        assert(out->index < 12 && "Invalid temp register target.");
        outreg = &state->temp_regs[out->index * 4];
        break;

      case NV2ART_CONTEXT:
        assert(out->index < 192 && "Invalid context register target.");
        outreg = &state->context_regs[out->index * 4];
        break;

      case NV2ART_ADDRESS:
        outreg = state->address_reg;
        break;
    }

    if (out->writemask & NV2AWM_X) {
      outreg[0] = result[0];
    }
    if (out->writemask & NV2AWM_Y) {
      outreg[1] = result[1];
    }
    if (out->writemask & NV2AWM_Z) {
      outreg[2] = result[2];
    }
    if (out->writemask & NV2AWM_W) {
      outreg[3] = result[3];
    }
  }
}

static inline void prepare_inputs(float *mac_inputs, float *ilu_input,
                                  Nv2aVshExecutionState *state,
                                  const Nv2aVshStep *step) {
  // Copy the inputs for both operations first to prevent introducing order
  // dependent behavior.
  if (step->mac.opcode) {
    for (uint32_t i = 0; i < 3; ++i) {
      if (step->mac.inputs[i].type == NV2ART_NONE) {
        break;
      }
      fetch_value(&mac_inputs[i * 4], state, &step->mac.inputs[i]);
    }
  }
  if (step->ilu.opcode) {
    fetch_value(ilu_input, state, &step->ilu.inputs[0]);
  }
}

static inline void apply(Nv2aVshExecutionState *state,
                         const Nv2aVshStep *step) {
  float mac_inputs[3 * 4];
  float ilu_input[4];
  prepare_inputs(mac_inputs, ilu_input, state, step);

  if (step->mac.opcode) {
    apply_operation(state, &step->mac, mac_inputs);
  }
  if (step->ilu.opcode) {
    apply_operation(state, &step->ilu, ilu_input);
  }
}

static inline void apply_track_context_writes(Nv2aVshExecutionState *state,
                                              const Nv2aVshStep *step,
                                              bool *context_dirty) {
  float mac_inputs[3 * 4];
  float ilu_input[4];
  prepare_inputs(mac_inputs, ilu_input, state, step);

  if (step->mac.opcode) {
    apply_operation(state, &step->mac, mac_inputs);
    if (step->mac.outputs[0].type == NV2ART_CONTEXT) {
      context_dirty[step->mac.outputs[0].index] = true;
    } else if (step->mac.outputs[1].type == NV2ART_CONTEXT) {
      context_dirty[step->mac.outputs[1].index] = true;
    }
  }
  if (step->ilu.opcode) {
    apply_operation(state, &step->ilu, ilu_input);
    if (step->ilu.outputs[0].type == NV2ART_CONTEXT) {
      context_dirty[step->ilu.outputs[0].index] = true;
    } else if (step->ilu.outputs[1].type == NV2ART_CONTEXT) {
      context_dirty[step->ilu.outputs[1].index] = true;
    }
  }
}

void nv2a_vsh_emu_execute(Nv2aVshExecutionState *state,
                          const Nv2aVshProgram *program) {
  assert(state);
  assert(program && program->steps);

  Nv2aVshStep *step = program->steps;
  while (true) {
    apply(state, step);
    if (step->is_final) {
      break;
    }
    ++step;
  }
}

void nv2a_vsh_emu_execute_track_context_writes(Nv2aVshExecutionState *state,
                                               const Nv2aVshProgram *program,
                                               bool *context_dirty) {
  assert(state);
  assert(program && program->steps);
  assert(context_dirty);

  Nv2aVshStep *step = program->steps;
  while (true) {
    apply_track_context_writes(state, step, context_dirty);
    if (step->is_final) {
      break;
    }
    ++step;
  }
}

void nv2a_vsh_emu_apply(Nv2aVshExecutionState *state, const Nv2aVshStep *step) {
  apply(state, step);
}
