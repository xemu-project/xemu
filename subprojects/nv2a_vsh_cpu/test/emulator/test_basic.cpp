#include <boost/test/unit_test.hpp>
#include <iterator>

#include "nv2a_vsh_disassembler.h"
#include "nv2a_vsh_emulator.h"

#define CHECK_REGISTER(bank, index, actual)   \
  do {                                        \
    float* expected = bank + index * 4;       \
    BOOST_TEST((expected)[0] == (actual)[0]); \
    BOOST_TEST((expected)[1] == (actual)[1]); \
    BOOST_TEST((expected)[2] == (actual)[2]); \
    BOOST_TEST((expected)[3] == (actual)[3]); \
  } while (0)

static void clear_step(Nv2aVshStep* out) {
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

// Sets an input that was reset by clear_step, keeping its .xyzw swizzle.
static void set_input(Nv2aVshInput* in, Nv2aVshRegisterType type,
                      uint32_t index) {
  in->type = type;
  in->index = index;
}

static void set_output(Nv2aVshOutput* out, Nv2aVshRegisterType type,
                       uint32_t index,
                       Nv2aVshWritemask writemask = NV2AWM_XYZW) {
  out->type = type;
  out->index = index;
  out->writemask = writemask;
}

static void set_register(float* bank, uint32_t index, float x, float y, float z,
                         float w) {
  float* reg = bank + index * 4;
  reg[0] = x;
  reg[1] = y;
  reg[2] = z;
  reg[3] = w;
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
  Nv2aVshStep* step = &steps[0];
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

BOOST_AUTO_TEST_CASE(arl_relative_context_read) {
  Nv2aVshCPUFullExecutionState full_state;
  Nv2aVshExecutionState state =
      nv2a_vsh_emu_initialize_full_execution_state(&full_state);
  for (uint32_t i = 0; i < 192 * 4; ++i) {
    full_state.context_regs[i] = static_cast<float>(i);
  }
  full_state.temp_regs[0] = 3.0f;

  // ARL A0, R0.x
  static constexpr uint32_t kARL[] = {0x00000000, 0x01A00000, 0x0436106C,
                                      0x20700FF8};
  Nv2aVshStep step;
  BOOST_TEST(nv2a_vsh_parse_step(&step, kARL) == NV2AVPR_SUCCESS);
  nv2a_vsh_emu_apply(&state, &step);
  BOOST_TEST(state.address_reg[0] == 3.0f);

  // MOV oFog.xyzw, c[A0+2]
  clear_step(&step);
  step.mac.opcode = NV2AOP_MOV;
  step.mac.outputs[0].type = NV2ART_OUTPUT;
  step.mac.outputs[0].index = NV2AOR_FOG_COORD;
  step.mac.outputs[0].writemask = NV2AWM_XYZW;
  step.mac.inputs[0].type = NV2ART_CONTEXT;
  step.mac.inputs[0].index = 2;
  step.mac.inputs[0].is_relative = true;
  nv2a_vsh_emu_apply(&state, &step);

  CHECK_REGISTER(state.output_regs, NV2AOR_FOG_COORD,
                 &full_state.context_regs[5 * 4]);
}

BOOST_AUTO_TEST_CASE(paired_mac_ilu_reads_before_writes) {
  Nv2aVshCPUFullExecutionState full_state;
  Nv2aVshExecutionState state =
      nv2a_vsh_emu_initialize_full_execution_state(&full_state);
  memset(full_state.context_regs, 0, sizeof(full_state.context_regs));
  float* v0 = &full_state.input_regs[0 * 4];
  v0[0] = 2.0f;
  v0[1] = 3.0f;
  v0[2] = 4.0f;
  v0[3] = 5.0f;
  float* v1 = &full_state.input_regs[1 * 4];
  v1[0] = 0.5f;
  v1[1] = 2.0f;
  v1[2] = 0.25f;
  v1[3] = -1.0f;
  float* r0 = &full_state.temp_regs[0 * 4];
  r0[0] = 4.0f;
  r0[1] = 0.0f;
  r0[2] = 0.0f;
  r0[3] = 0.0f;

  // MUL R0.xyzw, v0, v1 + RCP c3.xyzw, R0.x
  Nv2aVshStep steps[1];
  Nv2aVshStep* step = &steps[0];
  clear_step(step);
  step->mac.opcode = NV2AOP_MUL;
  step->mac.outputs[0].type = NV2ART_TEMPORARY;
  step->mac.outputs[0].index = 0;
  step->mac.outputs[0].writemask = NV2AWM_XYZW;
  step->mac.inputs[0].type = NV2ART_INPUT;
  step->mac.inputs[0].index = 0;
  step->mac.inputs[1].type = NV2ART_INPUT;
  step->mac.inputs[1].index = 1;

  step->ilu.opcode = NV2AOP_RCP;
  step->ilu.outputs[0].type = NV2ART_CONTEXT;
  step->ilu.outputs[0].index = 3;
  step->ilu.outputs[0].writemask = NV2AWM_XYZW;
  step->ilu.inputs[0].type = NV2ART_TEMPORARY;
  step->ilu.inputs[0].index = 0;
  step->ilu.inputs[0].swizzle[1] = NV2ASW_X;
  step->ilu.inputs[0].swizzle[2] = NV2ASW_X;
  step->ilu.inputs[0].swizzle[3] = NV2ASW_X;
  step->is_final = true;

  Nv2aVshProgram program;
  program.steps = steps;

  bool context_dirty[192] = {false};
  nv2a_vsh_emu_execute_track_context_writes(&state, &program, context_dirty);

  float expected_r0[] = {1.0f, 6.0f, 1.0f, -5.0f};
  CHECK_REGISTER(state.temp_regs, 0, expected_r0);

  // The ILU must see R0 as it was before the MAC wrote to it (1/4, not 1/1).
  float expected_c3[] = {0.25f, 0.25f, 0.25f, 0.25f};
  CHECK_REGISTER(state.context_regs, 3, expected_c3);

  for (uint32_t i = 0; i < 192; ++i) {
    BOOST_TEST_INFO(i);
    BOOST_TEST(context_dirty[i] == (i == 3));
  }
}

BOOST_AUTO_TEST_CASE(paired_mac_r1_write_is_suppressed) {
  Nv2aVshCPUFullExecutionState full_state = {};
  Nv2aVshExecutionState state =
      nv2a_vsh_emu_initialize_full_execution_state(&full_state);
  set_register(full_state.input_regs, 0, 0.75f, 0.5f, 2.0f, 3.0f);
  set_register(full_state.temp_regs, 1, 0.25f, 0.0f, 0.0f, 0.0f);

  // MOV R1.x, v0 + MOV R1.y, v0 (final)
  static constexpr uint32_t kProgram[][4] = {
      {0x00000000, 0x0220001B, 0x0836006C, 0x28140001},
  };

  Nv2aVshProgram program;
  BOOST_TEST(nv2a_vsh_parse_program(&program, kProgram[0],
                                    std::size(kProgram)) == NV2AVPR_SUCCESS);
  nv2a_vsh_emu_execute(&state, &program);
  nv2a_vsh_program_destroy(&program);

  float expected_r1[] = {0.25f, 0.5f, 0.0f, 0.0f};
  CHECK_REGISTER(state.temp_regs, 1, expected_r1);
}

BOOST_AUTO_TEST_CASE(paired_ilu_temp_write_redirected_to_r1) {
  Nv2aVshCPUFullExecutionState full_state = {};
  Nv2aVshExecutionState state =
      nv2a_vsh_emu_initialize_full_execution_state(&full_state);
  set_register(full_state.input_regs, 0, 0.75f, 0.5f, 2.0f, 3.0f);
  set_register(full_state.temp_regs, 1, 0.25f, 0.0f, 0.0f, 0.0f);

  // MOV R2.x, v0 + MOV R2.y, v0 (final)
  static constexpr uint32_t kProgram[][4] = {
      {0x00000000, 0x0220001B, 0x0836006C, 0x28240001},
  };

  Nv2aVshProgram program;
  BOOST_TEST(nv2a_vsh_parse_program(&program, kProgram[0],
                                    std::size(kProgram)) == NV2AVPR_SUCCESS);
  nv2a_vsh_emu_execute(&state, &program);
  nv2a_vsh_program_destroy(&program);

  float expected_r1[] = {0.25f, 0.5f, 0.0f, 0.0f};
  CHECK_REGISTER(state.temp_regs, 1, expected_r1);
  float expected_r2[] = {0.75f, 0.0f, 0.0f, 0.0f};
  CHECK_REGISTER(state.temp_regs, 2, expected_r2);
}

BOOST_AUTO_TEST_CASE(execute_stops_at_final) {
  Nv2aVshCPUFullExecutionState full_state = {};
  Nv2aVshExecutionState state =
      nv2a_vsh_emu_initialize_full_execution_state(&full_state);
  set_register(full_state.input_regs, 0, 1.0f, 2.0f, 3.0f, 4.0f);

  Nv2aVshStep steps[3];
  // MOV R1, v0
  clear_step(&steps[0]);
  steps[0].mac.opcode = NV2AOP_MOV;
  set_output(&steps[0].mac.outputs[0], NV2ART_TEMPORARY, 1);
  set_input(&steps[0].mac.inputs[0], NV2ART_INPUT, 0);

  // MOV oPos, R1 (final)
  clear_step(&steps[1]);
  steps[1].mac.opcode = NV2AOP_MOV;
  set_output(&steps[1].mac.outputs[0], NV2ART_OUTPUT, NV2AOR_POS);
  set_input(&steps[1].mac.inputs[0], NV2ART_TEMPORARY, 1);
  steps[1].is_final = true;

  // MOV oFog, v0 - must not execute since it follows the final step.
  clear_step(&steps[2]);
  steps[2].mac.opcode = NV2AOP_MOV;
  set_output(&steps[2].mac.outputs[0], NV2ART_OUTPUT, NV2AOR_FOG_COORD);
  set_input(&steps[2].mac.inputs[0], NV2ART_INPUT, 0);

  Nv2aVshProgram program;
  program.steps = steps;
  nv2a_vsh_emu_execute(&state, &program);

  CHECK_REGISTER(state.temp_regs, 1, &full_state.input_regs[0]);
  CHECK_REGISTER(state.output_regs, NV2AOR_POS, &full_state.input_regs[0]);
  float zero[] = {0.0f, 0.0f, 0.0f, 0.0f};
  CHECK_REGISTER(state.output_regs, NV2AOR_FOG_COORD, zero);
}

BOOST_AUTO_TEST_CASE(track_context_writes_multi_step) {
  Nv2aVshCPUFullExecutionState full_state = {};
  Nv2aVshExecutionState state =
      nv2a_vsh_emu_initialize_full_execution_state(&full_state);
  set_register(full_state.input_regs, 0, 1.0f, 2.0f, 3.0f, 4.0f);
  set_register(full_state.input_regs, 1, -1.0f, -2.0f, -3.0f, -4.0f);

  Nv2aVshStep steps[3];
  // MOV c2, v0
  clear_step(&steps[0]);
  steps[0].mac.opcode = NV2AOP_MOV;
  set_output(&steps[0].mac.outputs[0], NV2ART_CONTEXT, 2);
  set_input(&steps[0].mac.inputs[0], NV2ART_INPUT, 0);

  // MOV c7, v1 (final)
  clear_step(&steps[1]);
  steps[1].mac.opcode = NV2AOP_MOV;
  set_output(&steps[1].mac.outputs[0], NV2ART_CONTEXT, 7);
  set_input(&steps[1].mac.inputs[0], NV2ART_INPUT, 1);
  steps[1].is_final = true;

  // MOV c9, v0 - must not execute since it follows the final step.
  clear_step(&steps[2]);
  steps[2].mac.opcode = NV2AOP_MOV;
  set_output(&steps[2].mac.outputs[0], NV2ART_CONTEXT, 9);
  set_input(&steps[2].mac.inputs[0], NV2ART_INPUT, 0);

  Nv2aVshProgram program;
  program.steps = steps;
  bool context_dirty[192] = {false};
  nv2a_vsh_emu_execute_track_context_writes(&state, &program, context_dirty);

  CHECK_REGISTER(state.context_regs, 2, &full_state.input_regs[0]);
  CHECK_REGISTER(state.context_regs, 7, &full_state.input_regs[4]);
  float zero[] = {0.0f, 0.0f, 0.0f, 0.0f};
  CHECK_REGISTER(state.context_regs, 9, zero);
  for (uint32_t i = 0; i < 192; ++i) {
    BOOST_TEST_INFO(i);
    BOOST_TEST(context_dirty[i] == (i == 2 || i == 7));
  }
}

BOOST_AUTO_TEST_CASE(ilu_only_step) {
  Nv2aVshCPUFullExecutionState full_state = {};
  Nv2aVshExecutionState state =
      nv2a_vsh_emu_initialize_full_execution_state(&full_state);
  set_register(full_state.input_regs, 0, 4.0f, 0.0f, 0.0f, 0.0f);

  // RCP R0, v0.x
  Nv2aVshStep step;
  clear_step(&step);
  step.ilu.opcode = NV2AOP_RCP;
  set_output(&step.ilu.outputs[0], NV2ART_TEMPORARY, 0);
  set_input(&step.ilu.inputs[0], NV2ART_INPUT, 0);
  nv2a_vsh_emu_apply(&state, &step);

  float expected[] = {0.25f, 0.25f, 0.25f, 0.25f};
  CHECK_REGISTER(state.temp_regs, 0, expected);
}

BOOST_AUTO_TEST_CASE(mac_second_output_marks_context_dirty) {
  Nv2aVshCPUFullExecutionState full_state = {};
  Nv2aVshExecutionState state =
      nv2a_vsh_emu_initialize_full_execution_state(&full_state);
  set_register(full_state.input_regs, 0, 1.0f, 2.0f, 3.0f, 4.0f);

  // MOV R2, v0 + MOV c10, v0
  Nv2aVshStep steps[1];
  clear_step(&steps[0]);
  steps[0].mac.opcode = NV2AOP_MOV;
  set_output(&steps[0].mac.outputs[0], NV2ART_TEMPORARY, 2);
  set_output(&steps[0].mac.outputs[1], NV2ART_CONTEXT, 10);
  set_input(&steps[0].mac.inputs[0], NV2ART_INPUT, 0);
  steps[0].is_final = true;

  Nv2aVshProgram program;
  program.steps = steps;
  bool context_dirty[192] = {false};
  nv2a_vsh_emu_execute_track_context_writes(&state, &program, context_dirty);

  CHECK_REGISTER(state.temp_regs, 2, &full_state.input_regs[0]);
  CHECK_REGISTER(state.context_regs, 10, &full_state.input_regs[0]);
  for (uint32_t i = 0; i < 192; ++i) {
    BOOST_TEST_INFO(i);
    BOOST_TEST(context_dirty[i] == (i == 10));
  }
}

BOOST_AUTO_TEST_CASE(ilu_second_output_marks_context_dirty) {
  Nv2aVshCPUFullExecutionState full_state = {};
  Nv2aVshExecutionState state =
      nv2a_vsh_emu_initialize_full_execution_state(&full_state);
  set_register(full_state.input_regs, 0, 4.0f, 0.0f, 0.0f, 0.0f);

  // RCP R3, v0.x + RCP c20, v0.x
  Nv2aVshStep steps[1];
  clear_step(&steps[0]);
  steps[0].ilu.opcode = NV2AOP_RCP;
  set_output(&steps[0].ilu.outputs[0], NV2ART_TEMPORARY, 3);
  set_output(&steps[0].ilu.outputs[1], NV2ART_CONTEXT, 20);
  set_input(&steps[0].ilu.inputs[0], NV2ART_INPUT, 0);
  steps[0].is_final = true;

  Nv2aVshProgram program;
  program.steps = steps;
  bool context_dirty[192] = {false};
  nv2a_vsh_emu_execute_track_context_writes(&state, &program, context_dirty);

  float expected[] = {0.25f, 0.25f, 0.25f, 0.25f};
  CHECK_REGISTER(state.temp_regs, 3, expected);
  CHECK_REGISTER(state.context_regs, 20, expected);
  for (uint32_t i = 0; i < 192; ++i) {
    BOOST_TEST_INFO(i);
    BOOST_TEST(context_dirty[i] == (i == 20));
  }
}

BOOST_AUTO_TEST_CASE(r12_reads_output_position) {
  Nv2aVshCPUFullExecutionState full_state = {};
  Nv2aVshExecutionState state =
      nv2a_vsh_emu_initialize_full_execution_state(&full_state);
  set_register(full_state.output_regs, NV2AOR_POS, 1.0f, 2.0f, 3.0f, 4.0f);

  // MOV oD0, R12
  Nv2aVshStep step;
  clear_step(&step);
  step.mac.opcode = NV2AOP_MOV;
  set_output(&step.mac.outputs[0], NV2ART_OUTPUT, NV2AOR_DIFFUSE);
  set_input(&step.mac.inputs[0], NV2ART_TEMPORARY, 12);
  nv2a_vsh_emu_apply(&state, &step);

  CHECK_REGISTER(state.output_regs, NV2AOR_DIFFUSE,
                 &full_state.output_regs[NV2AOR_POS * 4]);
}

BOOST_AUTO_TEST_CASE(xss_state_writes_external_context) {
  Nv2aVshCPUXVSSExecutionState xss_state = {};
  float context_regs[192 * 4] = {0.0f};
  Nv2aVshExecutionState state =
      nv2a_vsh_emu_initialize_xss_execution_state(&xss_state, context_regs);
  BOOST_TEST(state.context_regs == context_regs);
  set_register(xss_state.input_regs, 0, 1.0f, 2.0f, 3.0f, 4.0f);

  // MOV c5, v0
  Nv2aVshStep step;
  clear_step(&step);
  step.mac.opcode = NV2AOP_MOV;
  set_output(&step.mac.outputs[0], NV2ART_CONTEXT, 5);
  set_input(&step.mac.inputs[0], NV2ART_INPUT, 0);
  nv2a_vsh_emu_apply(&state, &step);

  CHECK_REGISTER(context_regs, 5, xss_state.input_regs);
}

BOOST_AUTO_TEST_CASE(fog_write_uses_most_significant_masked_component) {
  struct TestCase {
    Nv2aVshWritemask writemask;
    float expected[4];
  };
  static constexpr TestCase kTests[] = {
      {NV2AWM_XYZW, {1.0f, 2.0f, 3.0f, 4.0f}},
      {NV2AWM_XZ, {1.0f, -1.0f, 3.0f, -1.0f}},
      {NV2AWM_YZW, {2.0f, 2.0f, 3.0f, 4.0f}},
      {NV2AWM_Y, {2.0f, 2.0f, -1.0f, -1.0f}},
      {NV2AWM_ZW, {3.0f, -1.0f, 3.0f, 4.0f}},
      {NV2AWM_Z, {3.0f, -1.0f, 3.0f, -1.0f}},
      {NV2AWM_W, {4.0f, -1.0f, -1.0f, 4.0f}},
  };

  for (const auto& test : kTests) {
    BOOST_TEST_INFO(test.writemask);
    Nv2aVshCPUFullExecutionState full_state = {};
    Nv2aVshExecutionState state =
        nv2a_vsh_emu_initialize_full_execution_state(&full_state);
    set_register(full_state.input_regs, 0, 1.0f, 2.0f, 3.0f, 4.0f);
    set_register(full_state.output_regs, NV2AOR_FOG_COORD, -1.0f, -1.0f, -1.0f,
                 -1.0f);

    // MOV oFog.<mask>, v0
    Nv2aVshStep step;
    clear_step(&step);
    step.mac.opcode = NV2AOP_MOV;
    set_output(&step.mac.outputs[0], NV2ART_OUTPUT, NV2AOR_FOG_COORD,
               test.writemask);
    set_input(&step.mac.inputs[0], NV2ART_INPUT, 0);
    nv2a_vsh_emu_apply(&state, &step);

    CHECK_REGISTER(state.output_regs, NV2AOR_FOG_COORD, test.expected);
  }
}

BOOST_AUTO_TEST_CASE(
    fog_write_from_ilu_uses_most_significant_masked_component) {
  Nv2aVshCPUFullExecutionState full_state = {};
  Nv2aVshExecutionState state =
      nv2a_vsh_emu_initialize_full_execution_state(&full_state);
  set_register(full_state.input_regs, 0, 1.0f, 2.0f, 4.0f, 8.0f);

  // RCP oFog.w, v0.z
  Nv2aVshStep step;
  clear_step(&step);
  step.ilu.opcode = NV2AOP_RCP;
  set_output(&step.ilu.outputs[0], NV2ART_OUTPUT, NV2AOR_FOG_COORD, NV2AWM_W);
  set_input(&step.ilu.inputs[0], NV2ART_INPUT, 0);
  step.ilu.inputs[0].swizzle[0] = NV2ASW_Z;
  step.ilu.inputs[0].swizzle[1] = NV2ASW_Z;
  step.ilu.inputs[0].swizzle[2] = NV2ASW_Z;
  step.ilu.inputs[0].swizzle[3] = NV2ASW_Z;
  nv2a_vsh_emu_apply(&state, &step);

  float expected[] = {0.25f, 0.0f, 0.0f, 0.25f};
  CHECK_REGISTER(state.output_regs, NV2AOR_FOG_COORD, expected);
}

// Mirrors the A0 probe shaders from nxdk_low_level_nv2a_tests vsh_tests:
//   ARL A0, v9.x
//   MOV oD0, c[A0 + static_offset]
// with c[i] = (i / 255, 1, 0.5, 1). Hardware returns (0, 0, 0, 0) for any
// read whose A0 + static_offset falls outside [0, 191].
static void check_a0_relative_reads(const float* a0_values, size_t count,
                                    uint32_t static_offset) {
  for (size_t i = 0; i < count; ++i) {
    const float a0 = a0_values[i];
    BOOST_TEST_CONTEXT("A0 = " << a0 << " + " << static_offset) {
      Nv2aVshCPUFullExecutionState full_state = {};
      Nv2aVshExecutionState state =
          nv2a_vsh_emu_initialize_full_execution_state(&full_state);
      for (uint32_t c = 0; c < 192; ++c) {
        set_register(full_state.context_regs, c, static_cast<float>(c) / 255.0f,
                     1.0f, 0.5f, 1.0f);
      }
      set_register(full_state.input_regs, 9, a0, 0.0f, 0.0f, 1.0f);
      set_register(full_state.output_regs, NV2AOR_DIFFUSE, -1.0f, -1.0f, -1.0f,
                   -1.0f);

      Nv2aVshStep step;
      clear_step(&step);
      step.mac.opcode = NV2AOP_ARL;
      set_output(&step.mac.outputs[0], NV2ART_ADDRESS, 0, NV2AWM_X);
      set_input(&step.mac.inputs[0], NV2ART_INPUT, 9);
      nv2a_vsh_emu_apply(&state, &step);

      clear_step(&step);
      step.mac.opcode = NV2AOP_MOV;
      set_output(&step.mac.outputs[0], NV2ART_OUTPUT, NV2AOR_DIFFUSE);
      set_input(&step.mac.inputs[0], NV2ART_CONTEXT, static_offset);
      step.mac.inputs[0].is_relative = true;
      nv2a_vsh_emu_apply(&state, &step);

      const int64_t effective = static_cast<int64_t>(a0) + static_offset;
      float expected[4] = {0.0f, 0.0f, 0.0f, 0.0f};
      if (effective >= 0 && effective < 192) {
        set_register(expected, 0, static_cast<float>(effective) / 255.0f, 1.0f,
                     0.5f, 1.0f);
      }
      CHECK_REGISTER(state.output_regs, NV2AOR_DIFFUSE, expected);
    }
  }
}

BOOST_AUTO_TEST_CASE(a0_relative_read_before_constant_file_returns_zero) {
  static constexpr float kA0Values[] = {
      0,    -1,   -2,   -96,  -191, -192,  -193,
      -256, -257, -384, -512, -513, -4096, -65536,
  };
  check_a0_relative_reads(kA0Values, std::size(kA0Values), 0);
}

BOOST_AUTO_TEST_CASE(a0_relative_read_beyond_constant_file_returns_zero) {
  static constexpr float kA0Values[] = {
      0,   1,   95,  96,   190,  191,  192,   193,   200,
      255, 256, 257, 383,  384,  447,  448,   511,   512,
      513, 767, 768, 1000, 4095, 4096, 65535, 65536, 1000000,
  };
  check_a0_relative_reads(kA0Values, std::size(kA0Values), 0);
}

BOOST_AUTO_TEST_CASE(
    a0_relative_read_with_offset_beyond_constant_file_returns_zero) {
  static constexpr float kA0Values[] = {
      -191, -96, -1, 0, 1, 2, 9, 65, 256, 4096, 65536,
  };
  check_a0_relative_reads(kA0Values, std::size(kA0Values), 191);
}

// Mirrors vsh_log.vsh from nxdk_low_level_nv2a_tests:
//   LOGP R0, v9.x
//   ADD oD0, R0.xxxx, c[0].zzzz
// Hardware produces a negative exponent for inputs in (0, 1).
BOOST_AUTO_TEST_CASE(log_negative_exponent) {
  struct TestCase {
    float input;
    float expected_exponent;
  };
  static constexpr TestCase kTests[] = {
      {0.75f, -1.0f},   {0.5f, -1.0f}, {0.25f, -2.0f},
      {0.001f, -10.0f}, {1.0f, 0.0f},  {2.0f, 1.0f},
  };

  for (const auto& test : kTests) {
    BOOST_TEST_CONTEXT("LOG(" << test.input << ")") {
      Nv2aVshCPUFullExecutionState full_state = {};
      Nv2aVshExecutionState state =
          nv2a_vsh_emu_initialize_full_execution_state(&full_state);
      set_register(full_state.context_regs, 0, 0.0f, 1.0f, 128.0f / 255.0f,
                   1.0f);
      set_register(full_state.input_regs, 9, test.input, 0.0f, 0.0f, 1.0f);

      Nv2aVshStep step;
      clear_step(&step);
      step.ilu.opcode = NV2AOP_LOG;
      set_output(&step.ilu.outputs[0], NV2ART_TEMPORARY, 0);
      set_input(&step.ilu.inputs[0], NV2ART_INPUT, 9);
      nv2a_vsh_emu_apply(&state, &step);

      BOOST_TEST(state.temp_regs[0] == test.expected_exponent);

      clear_step(&step);
      step.mac.opcode = NV2AOP_ADD;
      set_output(&step.mac.outputs[0], NV2ART_OUTPUT, NV2AOR_DIFFUSE);
      set_input(&step.mac.inputs[0], NV2ART_TEMPORARY, 0);
      memset(step.mac.inputs[0].swizzle, NV2ASW_X, 4);
      set_input(&step.mac.inputs[1], NV2ART_CONTEXT, 0);
      memset(step.mac.inputs[1].swizzle, NV2ASW_Z, 4);
      nv2a_vsh_emu_apply(&state, &step);

      const float expected_sum = test.expected_exponent + 128.0f / 255.0f;
      float expected[] = {expected_sum, expected_sum, expected_sum,
                          expected_sum};
      CHECK_REGISTER(state.output_regs, NV2AOR_DIFFUSE, expected);
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()
