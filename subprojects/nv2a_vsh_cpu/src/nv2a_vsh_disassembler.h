// * Based on https://github.com/abaire/nv2a_vsh_asm which is
// * based on
// https://github.com/XboxDev/nxdk/blob/c4b69e7a82452c21aa2c62701fd3836755950f58/tools/vp20compiler/prog_instruction.c#L1
// * Mesa 3-D graphics library
// * Version:  7.3
// *
// * Copyright (C) 1999-2008  Brian Paul   All Rights Reserved.
// * Copyright (C) 1999-2009  VMware, Inc.  All Rights Reserved.
// *
// * Permission is hereby granted, free of charge, to any person obtaining a
// * copy of this software and associated documentation files (the "Software"),
// * to deal in the Software without restriction, including without limitation
// * the rights to use, copy, modify, merge, publish, distribute, sublicense,
// * and/or sell copies of the Software, and to permit persons to whom the
// * Software is furnished to do so, subject to the following conditions:
// *
// * The above copyright notice and this permission notice shall be included
// * in all copies or substantial portions of the Software.
// *
// * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
// * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
// * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef NV2A_VSH_CPU_SRC_NV2A_VSH_DISASSEMBLER_H_
#define NV2A_VSH_CPU_SRC_NV2A_VSH_DISASSEMBLER_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum Nv2aVshOpcode_ {
  NV2AOP_NOP = 0,
  NV2AOP_MOV,
  NV2AOP_MUL,
  NV2AOP_ADD,
  NV2AOP_MAD,
  NV2AOP_DP3,
  NV2AOP_DPH,
  NV2AOP_DP4,
  NV2AOP_DST,
  NV2AOP_MIN,
  NV2AOP_MAX,
  NV2AOP_SLT,
  NV2AOP_SGE,
  NV2AOP_ARL,
  NV2AOP_RCP,
  NV2AOP_RCC,
  NV2AOP_RSQ,
  NV2AOP_EXP,
  NV2AOP_LOG,
  NV2AOP_LIT
} Nv2aVshOpcode;

typedef enum Nv2aVshSwizzle_ {
  NV2ASW_X = 0,
  NV2ASW_Y,
  NV2ASW_Z,
  NV2ASW_W,
} Nv2aVshSwizzle;

typedef enum Nv2aVshWritemask_ {
  NV2AWM_W = 1,
  NV2AWM_Z,
  NV2AWM_ZW,
  NV2AWM_Y,
  NV2AWM_YW,
  NV2AWM_YZ,
  NV2AWM_YZW,
  NV2AWM_X,
  NV2AWM_XW,
  NV2AWM_XZ,
  NV2AWM_XZW,
  NV2AWM_XY,
  NV2AWM_XYW,
  NV2AWM_XYZ,
  NV2AWM_XYZW,
} Nv2aVshWritemask;

typedef enum Nv2aVshRegisterType_ {
  NV2ART_NONE = 0,  // This input/output slot is unused.
  NV2ART_TEMPORARY,
  NV2ART_INPUT,
  NV2ART_OUTPUT,
  NV2ART_CONTEXT,
  NV2ART_ADDRESS,  // A0
} Nv2aVshRegisterType;

typedef struct Nv2aVshOutput_ {
  Nv2aVshRegisterType type;
  uint32_t index;
  Nv2aVshWritemask writemask;
} Nv2aVshOutput;

typedef struct Nv2aVshInput_ {
  Nv2aVshRegisterType type;
  uint32_t index;
  uint8_t swizzle[4];
  bool is_negated;
  bool is_relative;
} Nv2aVshInput;

// Represents a single operation.
typedef struct Nv2aVshOperation_ {
  Nv2aVshOpcode opcode;
  Nv2aVshOutput outputs[2];
  Nv2aVshInput inputs[3];
} Nv2aVshOperation;

typedef struct Nv2aVshStep_ {
  Nv2aVshOperation mac;
  Nv2aVshOperation ilu;
  bool is_final;
} Nv2aVshStep;

typedef struct Nv2aVshProgram_ {
  Nv2aVshStep *steps;
} Nv2aVshProgram;

typedef enum Nv2aVshParseResult_ {
  NV2AVPR_SUCCESS = 0,
  NV2AVPR_BAD_OUTPUT,
  NV2AVPR_BAD_PROGRAM,
  NV2AVPR_BAD_PROGRAM_SIZE,
  NV2AVPR_ARL_CONFLICT,
  NV2AVPR_BAD_MAC_OPCODE,
  NV2AVPR_BAD_ILU_OPCODE,
} Nv2aVshParseResult;

void nv2a_vsh_program_destroy(Nv2aVshProgram *program);

// Disassemble the given token (which must be 4 uint32_t's) into an Nv2aVshStep.
Nv2aVshParseResult nv2a_vsh_parse_step(Nv2aVshStep *out, const uint32_t *token);

// Disassemble the given array of nv2a transform opcodes into an
// Nv2aVshProgram representation.
//
// out - Nv2aVshProgram which will be updated to contain the parsed
//       steps.
// program - Flat array of integers containing the nv2a transform opcodes to be
//           processed.
// program_slots - Number of slots in `program` (each slot is 4 integers).
//
// Note: On success, the caller is responsible for calling
//       `nv2a_vsh_program_destroy` to clean up the allocated program.
Nv2aVshParseResult nv2a_vsh_parse_program(Nv2aVshProgram *out,
                                          const uint32_t *program,
                                          uint32_t program_slots);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // NV2A_VSH_CPU_SRC_NV2A_VSH_DISASSEMBLER_H_
