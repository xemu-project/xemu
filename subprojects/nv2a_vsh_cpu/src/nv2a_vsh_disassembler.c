#include "nv2a_vsh_disassembler.h"

#include <stdlib.h>

#define NV2A_MAX_TRANSFORM_PROGRAM_LENGTH 136

#define EXTRACT(token, index, start, size) \
  (token[(index)] >> start) & ~(0xFFFFFFFF << size)

typedef enum Nv2aVshInputType_ {
  NV2AIT_NONE = 0,
  NV2AIT_TEMP = 1,
  NV2AIT_INPUT = 2,
  NV2AIT_CONTEXT = 3,
} Nv2aVshInputType;

static const Nv2aVshRegisterType kInputTypeToGeneric[] = {
    NV2ART_NONE,
    NV2ART_TEMPORARY,
    NV2ART_INPUT,
    NV2ART_CONTEXT,
};

typedef enum Nv2aVshILUOpcode_ {
  NV2AILU_NOP = 0,
  NV2AILU_MOV = 1,
  NV2AILU_RCP = 2,
  NV2AILU_RCC = 3,
  NV2AILU_RSQ = 4,
  NV2AILU_EXP = 5,
  NV2AILU_LOG = 6,
  NV2AILU_LIT = 7
} Nv2aVshILUOpcode;

static const Nv2aVshOpcode kILUOpcodeToGeneric[] = {
    NV2AOP_NOP, NV2AOP_MOV, NV2AOP_RCP, NV2AOP_RCC,
    NV2AOP_RSQ, NV2AOP_EXP, NV2AOP_LOG, NV2AOP_LIT};

typedef enum Nv2aVshMACOpcode_ {
  NV2AMAC_NOP = 0,
  NV2AMAC_MOV = 1,
  NV2AMAC_MUL = 2,
  NV2AMAC_ADD = 3,
  NV2AMAC_MAD = 4,
  NV2AMAC_DP3 = 5,
  NV2AMAC_DPH = 6,
  NV2AMAC_DP4 = 7,
  NV2AMAC_DST = 8,
  NV2AMAC_MIN = 9,
  NV2AMAC_MAX = 10,
  NV2AMAC_SLT = 11,
  NV2AMAC_SGE = 12,
  NV2AMAC_ARL = 13
} Nv2aVshMACOpcode;

static const Nv2aVshOpcode kMACOpcodeToGeneric[] = {
    NV2AOP_NOP, NV2AOP_MOV, NV2AOP_MUL, NV2AOP_ADD, NV2AOP_MAD,
    NV2AOP_DP3, NV2AOP_DPH, NV2AOP_DP4, NV2AOP_DST, NV2AOP_MIN,
    NV2AOP_MAX, NV2AOP_SLT, NV2AOP_SGE, NV2AOP_ARL,
};

void nv2a_vsh_program_destroy(Nv2aVshProgram *program) {
  if (program && program->steps) {
    free(program->steps);
    program->steps = NULL;
  }
}

static inline uint32_t parse_a_swizzle_w(const uint32_t *token) {
  return EXTRACT(token, 1, 0, 2);
}

static inline uint32_t parse_a_swizzle_z(const uint32_t *token) {
  return EXTRACT(token, 1, 2, 2);
}

static inline uint32_t parse_a_swizzle_y(const uint32_t *token) {
  return EXTRACT(token, 1, 4, 2);
}

static inline uint32_t parse_a_swizzle_x(const uint32_t *token) {
  return EXTRACT(token, 1, 6, 2);
}

static inline bool parse_a_negate(const uint32_t *token) {
  return EXTRACT(token, 1, 8, 1);
}

static inline uint32_t parse_input_reg(const uint32_t *token) {
  return EXTRACT(token, 1, 9, 4);
}

static inline uint32_t parse_context_reg(const uint32_t *token) {
  return EXTRACT(token, 1, 13, 8);
}

static inline Nv2aVshMACOpcode parse_mac_opcode(const uint32_t *token) {
  return EXTRACT(token, 1, 21, 4);
}

static inline Nv2aVshILUOpcode parse_ilu_opcode(const uint32_t *token) {
  return EXTRACT(token, 1, 25, 3);
}

static inline uint32_t parse_c_swizzle_w(const uint32_t *token) {
  return EXTRACT(token, 2, 2, 2);
}

static inline uint32_t parse_c_swizzle_z(const uint32_t *token) {
  return EXTRACT(token, 2, 4, 2);
}

static inline uint32_t parse_c_swizzle_y(const uint32_t *token) {
  return EXTRACT(token, 2, 6, 2);
}

static inline uint32_t parse_c_swizzle_x(const uint32_t *token) {
  return EXTRACT(token, 2, 8, 2);
}

static inline bool parse_c_negate(const uint32_t *token) {
  return EXTRACT(token, 2, 10, 1);
}

static inline Nv2aVshInputType parse_b_type(const uint32_t *token) {
  return EXTRACT(token, 2, 11, 2);
}

static inline uint32_t parse_b_temp_reg(const uint32_t *token) {
  return EXTRACT(token, 2, 13, 4);
}

static inline uint32_t parse_b_swizzle_w(const uint32_t *token) {
  return EXTRACT(token, 2, 17, 2);
}

static inline uint32_t parse_b_swizzle_z(const uint32_t *token) {
  return EXTRACT(token, 2, 19, 2);
}

static inline uint32_t parse_b_swizzle_y(const uint32_t *token) {
  return EXTRACT(token, 2, 21, 2);
}

static inline uint32_t parse_b_swizzle_x(const uint32_t *token) {
  return EXTRACT(token, 2, 23, 2);
}

static inline bool parse_b_negate(const uint32_t *token) {
  return EXTRACT(token, 2, 25, 1);
}

static inline Nv2aVshInputType parse_a_type(const uint32_t *token) {
  return EXTRACT(token, 2, 26, 2);
}

static inline uint32_t parse_a_temp_reg(const uint32_t *token) {
  return EXTRACT(token, 2, 28, 4);
}

static inline bool parse_final(const uint32_t *token) {
  return EXTRACT(token, 3, 0, 1);
}

static inline bool parse_a0(const uint32_t *token) {
  return EXTRACT(token, 3, 1, 1);
}

static inline bool parse_output_is_ilu(const uint32_t *token) {
  return EXTRACT(token, 3, 2, 1);
}

static inline uint32_t parse_output_index(const uint32_t *token) {
  return EXTRACT(token, 3, 3, 8);
}

static inline bool parse_out_is_output(const uint32_t *token) {
  return EXTRACT(token, 3, 11, 1);
}

static inline uint32_t parse_output_writemask(const uint32_t *token) {
  return EXTRACT(token, 3, 12, 4);
}

static inline uint32_t parse_temp_writemask_ilu(const uint32_t *token) {
  return EXTRACT(token, 3, 16, 4);
}

static inline uint32_t parse_out_temp_reg(const uint32_t *token) {
  return EXTRACT(token, 3, 20, 4);
}

static inline uint32_t parse_temp_writemask_mac(const uint32_t *token) {
  return EXTRACT(token, 3, 24, 4);
}

static inline Nv2aVshInputType parse_c_type(const uint32_t *token) {
  return EXTRACT(token, 3, 28, 2);
}

static inline uint32_t parse_c_temp_reg(const uint32_t *token) {
  uint32_t low = EXTRACT(token, 3, 30, 2);
  uint32_t high = EXTRACT(token, 2, 0, 2);
  return ((high & 0x03) << 2) + (low & 0x03);
}

static inline void process_input(Nv2aVshInput *out, const uint32_t *token,
                                 bool negate, uint32_t temp_reg, uint32_t x,
                                 uint32_t y, uint32_t z, uint32_t w) {
  switch (out->type) {
    case NV2ART_TEMPORARY:
      out->index = temp_reg;
      break;

    case NV2ART_INPUT:
      out->index = parse_input_reg(token);
      break;

    case NV2ART_CONTEXT:
      out->index = parse_context_reg(token);
      out->is_relative = parse_a0(token);
      break;

    default:
      return;
  }

  out->is_negated = negate;
  out->swizzle[0] = x & 0xFF;
  out->swizzle[1] = y & 0xFF;
  out->swizzle[2] = z & 0xFF;
  out->swizzle[3] = w & 0xFF;
}

static Nv2aVshParseResult parse_inputs(Nv2aVshInput *inputs,
                                       const uint32_t *token) {
  inputs[0].type = kInputTypeToGeneric[parse_a_type(token)];
  if (inputs[0].type) {
    process_input(&inputs[0], token, parse_a_negate(token),
                  parse_a_temp_reg(token), parse_a_swizzle_x(token),
                  parse_a_swizzle_y(token), parse_a_swizzle_z(token),
                  parse_a_swizzle_w(token));
  }

  inputs[1].type = kInputTypeToGeneric[parse_b_type(token)];
  if (inputs[1].type) {
    process_input(&inputs[1], token, parse_b_negate(token),
                  parse_b_temp_reg(token), parse_b_swizzle_x(token),
                  parse_b_swizzle_y(token), parse_b_swizzle_z(token),
                  parse_b_swizzle_w(token));
  }

  inputs[2].type = kInputTypeToGeneric[parse_c_type(token)];
  if (inputs[2].type) {
    process_input(&inputs[2], token, parse_c_negate(token),
                  parse_c_temp_reg(token), parse_c_swizzle_x(token),
                  parse_c_swizzle_y(token), parse_c_swizzle_z(token),
                  parse_c_swizzle_w(token));
  }

  return NV2AVPR_SUCCESS;
}

static Nv2aVshParseResult parse_outputs(Nv2aVshStep *out,
                                        const uint32_t *token) {
  out->mac.outputs[0].type = NV2ART_NONE;
  out->mac.outputs[1].type = NV2ART_NONE;
  out->ilu.outputs[0].type = NV2ART_NONE;
  out->ilu.outputs[1].type = NV2ART_NONE;

  uint32_t out_temp_register = parse_out_temp_reg(token);
  uint32_t temp_writemask_mac = parse_temp_writemask_mac(token);
  uint32_t temp_writemask_ilu = parse_temp_writemask_ilu(token);

  if (temp_writemask_mac) {
    out->mac.outputs[0].type = NV2ART_TEMPORARY;
    out->mac.outputs[0].index = out_temp_register;
    out->mac.outputs[0].writemask = temp_writemask_mac;
  }

  if (temp_writemask_ilu) {
    out->ilu.outputs[0].type = NV2ART_TEMPORARY;
    if (out->mac.opcode != NV2AOP_NOP) {
      // Paired ILU instructions that write to temporary registers may only
      // write to R1.
      out->ilu.outputs[0].index = 1;
    } else {
      out->ilu.outputs[0].index = out_temp_register;
    }
    out->ilu.outputs[0].writemask = temp_writemask_ilu;
  }

  uint32_t output_writemask = parse_output_writemask(token);
  if (output_writemask) {
    Nv2aVshOutput *output = NULL;
    if (parse_output_is_ilu(token)) {
      output = &out->ilu.outputs[0];
      if (output->type != NV2ART_NONE) {
        ++output;
      }
    } else {
      output = &out->mac.outputs[0];
      if (output->type != NV2ART_NONE) {
        ++output;
      }
    }

    output->type = parse_out_is_output(token) ? NV2ART_OUTPUT : NV2ART_CONTEXT;
    output->index = parse_output_index(token);
    output->writemask = output_writemask;
  }

  if (out->mac.opcode == NV2AOP_ARL) {
    if (out->mac.outputs[0].type != NV2ART_NONE) {
      return NV2AVPR_ARL_CONFLICT;
    }
    out->mac.outputs[0].type = NV2ART_ADDRESS;
    out->mac.outputs[0].index = 0;
    out->mac.outputs[0].writemask = 0;
  }

  return NV2AVPR_SUCCESS;
}

Nv2aVshParseResult nv2a_vsh_parse_step(Nv2aVshStep *out,
                                       const uint32_t *token) {
  out->mac.opcode = kMACOpcodeToGeneric[parse_mac_opcode(token)];
  out->ilu.opcode = kILUOpcodeToGeneric[parse_ilu_opcode(token)];

  out->mac.inputs[0].type = NV2ART_NONE;
  out->mac.inputs[1].type = NV2ART_NONE;
  out->mac.inputs[2].type = NV2ART_NONE;
  out->ilu.inputs[0].type = NV2ART_NONE;
  out->ilu.inputs[1].type = NV2ART_NONE;
  out->ilu.inputs[2].type = NV2ART_NONE;

  out->is_final = parse_final(token);

  if (!(out->mac.opcode || out->ilu.opcode)) {
    return NV2AVPR_SUCCESS;
  }

  Nv2aVshParseResult result = parse_outputs(out, token);
  if (result != NV2AVPR_SUCCESS) {
    return result;
  }

  Nv2aVshInput inputs[3];
  result = parse_inputs(inputs, token);
  if (result != NV2AVPR_SUCCESS) {
    return result;
  }

  out->mac.inputs[0] = inputs[0];
  switch (out->mac.opcode) {
    case NV2AOP_NOP:
      break;

    case NV2AOP_MOV:
    case NV2AOP_ARL:
      // These only use "a" which is already assigned.
      break;

    case NV2AOP_MUL:
    case NV2AOP_DP3:
    case NV2AOP_DP4:
    case NV2AOP_DPH:
    case NV2AOP_DST:
    case NV2AOP_MIN:
    case NV2AOP_MAX:
    case NV2AOP_SGE:
    case NV2AOP_SLT:
      out->mac.inputs[1] = inputs[1];
      break;

    case NV2AOP_MAD:
      out->mac.inputs[1] = inputs[1];
      out->mac.inputs[2] = inputs[2];
      break;

    case NV2AOP_ADD:
      out->mac.inputs[1] = inputs[2];
      break;

    default:
      return NV2AVPR_BAD_MAC_OPCODE;
  }

  switch (out->ilu.opcode) {
    default:
      return NV2AVPR_BAD_ILU_OPCODE;

    case NV2AOP_NOP:
      break;

    case NV2AOP_MOV:
    case NV2AOP_LIT:
      out->ilu.inputs[0] = inputs[2];
      break;

      // These commands operate on the "x" component only.
    case NV2AOP_RCP:
    case NV2AOP_RCC:
    case NV2AOP_RSQ:
    case NV2AOP_EXP:
    case NV2AOP_LOG:
      out->ilu.inputs[0] = inputs[2];
      out->ilu.inputs[0].swizzle[1] = out->ilu.inputs[0].swizzle[0];
      out->ilu.inputs[0].swizzle[2] = out->ilu.inputs[0].swizzle[0];
      out->ilu.inputs[0].swizzle[3] = out->ilu.inputs[0].swizzle[0];
      break;
  }

  return result;
}

Nv2aVshParseResult nv2a_vsh_parse_program(Nv2aVshProgram *out,
                                          const uint32_t *program,
                                          uint32_t program_slots) {
  if (!out) {
    return NV2AVPR_BAD_OUTPUT;
  }

  if (!program_slots || program_slots > NV2A_MAX_TRANSFORM_PROGRAM_LENGTH) {
    return NV2AVPR_BAD_PROGRAM_SIZE;
  }

  if (!program) {
    return NV2AVPR_BAD_PROGRAM;
  }

  out->steps = (Nv2aVshStep *)malloc(sizeof(Nv2aVshStep) * program_slots);

  Nv2aVshStep *step = out->steps;
  const uint32_t *opcodes = program;

  for (uint32_t i = 0; i < program_slots; ++i, ++step, opcodes += 4) {
    Nv2aVshParseResult result = nv2a_vsh_parse_step(step, opcodes);
    if (result != NV2AVPR_SUCCESS) {
      nv2a_vsh_program_destroy(out);
      return result;
    }
  }

  return NV2AVPR_SUCCESS;
}
