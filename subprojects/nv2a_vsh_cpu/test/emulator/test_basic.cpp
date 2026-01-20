#include <boost/test/unit_test.hpp>

#include "nv2a_vsh_emulator.h"

#define CHECK_REGISTER(bank, index, actual)   \
  do {                                        \
    float *expected = bank + index * 4;       \
    BOOST_TEST((expected)[0] == (actual)[0]); \
    BOOST_TEST((expected)[1] == (actual)[1]); \
    BOOST_TEST((expected)[2] == (actual)[2]); \
    BOOST_TEST((expected)[3] == (actual)[3]); \
  } while (0)

static void clear_step(Nv2aVshStep *out) {
  out->is_final = false;
  memset(out->mac.outputs, 0, sizeof(out->mac.outputs));
  memset(out->mac.inputs, 0, sizeof(out->mac.inputs));
  memset(out->ilu.outputs, 0, sizeof(out->ilu.outputs));
  memset(out->ilu.inputs, 0, sizeof(out->ilu.inputs));

  out->mac.opcode = NV2AOP_NOP;
  out->mac.inputs[0].type = NV2ART_NONE;
  out->mac.inputs[0].swizzle[0] = NV2ASW_X;
  out->mac.inputs[0].swizzle[1] = NV2ASW_Y;
  out->mac.inputs[0].swizzle[2] = NV2ASW_Z;
  out->mac.inputs[0].swizzle[3] = NV2ASW_W;
  out->mac.inputs[1].type = NV2ART_NONE;
  out->mac.inputs[1].swizzle[0] = NV2ASW_X;
  out->mac.inputs[1].swizzle[1] = NV2ASW_Y;
  out->mac.inputs[1].swizzle[2] = NV2ASW_Z;
  out->mac.inputs[1].swizzle[3] = NV2ASW_W;
  out->mac.inputs[2].type = NV2ART_NONE;
  out->mac.inputs[2].swizzle[0] = NV2ASW_X;
  out->mac.inputs[2].swizzle[1] = NV2ASW_Y;
  out->mac.inputs[2].swizzle[2] = NV2ASW_Z;
  out->mac.inputs[2].swizzle[3] = NV2ASW_W;
  out->mac.outputs[0].type = NV2ART_NONE;
  out->mac.outputs[1].type = NV2ART_NONE;

  out->ilu.opcode = NV2AOP_NOP;
  out->ilu.inputs[0].type = NV2ART_NONE;
  out->ilu.inputs[0].swizzle[0] = NV2ASW_X;
  out->ilu.inputs[0].swizzle[1] = NV2ASW_Y;
  out->ilu.inputs[0].swizzle[2] = NV2ASW_Z;
  out->ilu.inputs[0].swizzle[3] = NV2ASW_W;
  out->ilu.inputs[1].type = NV2ART_NONE;
  out->ilu.inputs[1].swizzle[0] = NV2ASW_X;
  out->ilu.inputs[1].swizzle[1] = NV2ASW_Y;
  out->ilu.inputs[1].swizzle[2] = NV2ASW_Z;
  out->ilu.inputs[1].swizzle[3] = NV2ASW_W;
  out->ilu.inputs[2].type = NV2ART_NONE;
  out->ilu.inputs[2].swizzle[0] = NV2ASW_X;
  out->ilu.inputs[2].swizzle[1] = NV2ASW_Y;
  out->ilu.inputs[2].swizzle[2] = NV2ASW_Z;
  out->ilu.inputs[2].swizzle[3] = NV2ASW_W;
  out->ilu.outputs[0].type = NV2ART_NONE;
  out->ilu.outputs[1].type = NV2ART_NONE;
}

BOOST_AUTO_TEST_SUITE(basic_operation_suite)

BOOST_AUTO_TEST_CASE(step_trivial) {
  Nv2aVshCPUFullExecutionState full_state;
  Nv2aVshExecutionState state =
      nv2a_vsh_emu_initialize_full_execution_state(&full_state);
  uint32_t reg = 11 * 4;
  full_state.input_regs[reg + 0] = 123.0f;
  full_state.input_regs[reg + 1] = -456.0f;
  full_state.input_regs[reg + 2] = 0.789f;
  full_state.input_regs[reg + 3] = 32.64f;

  // MOV oT2.xyzw, v11
  Nv2aVshStep step;
  clear_step(&step);
  step.mac.opcode = NV2AOP_MOV;
  step.mac.outputs[0].type = NV2ART_OUTPUT;
  step.mac.outputs[0].index = 11;
  step.mac.outputs[0].writemask = NV2AWM_XYZW;
  step.mac.inputs[0].type = NV2ART_INPUT;
  step.mac.inputs[0].index = 11;

  nv2a_vsh_emu_apply(&state, &step);

  CHECK_REGISTER(state.output_regs, NV2AOR_TEX2, &full_state.input_regs[reg]);
}

BOOST_AUTO_TEST_CASE(program_context_tracked) {
  Nv2aVshCPUFullExecutionState full_state;
  Nv2aVshExecutionState state =
      nv2a_vsh_emu_initialize_full_execution_state(&full_state);
  uint32_t reg = 11 * 4;
  full_state.input_regs[reg + 0] = 123.0f;
  full_state.input_regs[reg + 1] = -456.0f;
  full_state.input_regs[reg + 2] = 0.789f;
  full_state.input_regs[reg + 3] = 32.64f;

  // MOV c1.xyzw, v11
  Nv2aVshStep steps[1];
  Nv2aVshStep *step = &steps[0];
  clear_step(step);
  step->mac.opcode = NV2AOP_MOV;
  step->mac.outputs[0].type = NV2ART_CONTEXT;
  step->mac.outputs[0].index = 1;
  step->mac.outputs[0].writemask = NV2AWM_XYZW;
  step->mac.inputs[0].type = NV2ART_INPUT;
  step->mac.inputs[0].index = 11;
  step->is_final = true;

  Nv2aVshProgram program;
  program.steps = steps;

  bool context_dirty[192] = {false};
  nv2a_vsh_emu_execute_track_context_writes(&state, &program, context_dirty);

  CHECK_REGISTER(state.context_regs, 1, &full_state.input_regs[reg]);
  BOOST_TEST(!context_dirty[0]);
  BOOST_TEST(context_dirty[1]);
  for (uint32_t i = 2; i < 192; ++i) {
    BOOST_TEST_INFO(i);
    BOOST_TEST(!context_dirty[i]);
  }
}

BOOST_AUTO_TEST_SUITE_END()
