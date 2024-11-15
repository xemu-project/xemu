#include <boost/test/unit_test.hpp>

#include "nv2a_vsh_disassembler.h"

BOOST_AUTO_TEST_SUITE(basic_disassembler_suite)

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

static void check_opcode(const Nv2aVshOperation &expected,
                         const Nv2aVshOperation &actual) {
  for (int i = 0; i < 2; ++i) {
    BOOST_TEST_INFO_SCOPE("Output " << i);
    BOOST_TEST(expected.outputs[i].type == actual.outputs[i].type);
    if (expected.outputs[i].type == NV2ART_NONE) {
      continue;
    }
    BOOST_TEST(expected.outputs[i].index == actual.outputs[i].index);
    BOOST_TEST(expected.outputs[i].writemask == actual.outputs[i].writemask);
  }

  for (int i = 0; i < 3; ++i) {
    BOOST_TEST_INFO_SCOPE("Input " << i);
    BOOST_TEST(expected.inputs[i].type == actual.inputs[i].type);
    if (expected.inputs[i].type == NV2ART_NONE) {
      continue;
    }

    BOOST_TEST(expected.inputs[i].index == actual.inputs[i].index);
    BOOST_TEST(expected.inputs[i].is_negated == actual.inputs[i].is_negated);
    if (expected.inputs[i].type == NV2ART_CONTEXT) {
      BOOST_TEST(expected.inputs[i].is_relative ==
                 actual.inputs[i].is_relative);
    }
    BOOST_TEST(expected.inputs[i].swizzle[0] == actual.inputs[i].swizzle[0]);
    BOOST_TEST(expected.inputs[i].swizzle[1] == actual.inputs[i].swizzle[1]);
    BOOST_TEST(expected.inputs[i].swizzle[2] == actual.inputs[i].swizzle[2]);
    BOOST_TEST(expected.inputs[i].swizzle[3] == actual.inputs[i].swizzle[3]);
  }
}

static void check_result(const Nv2aVshStep &expected,
                         const Nv2aVshStep &actual) {
  BOOST_TEST(expected.mac.opcode == actual.mac.opcode);
  if (expected.mac.opcode != NV2AOP_NOP) {
    BOOST_TEST_INFO("MAC");
    check_opcode(expected.mac, actual.mac);
  }

  BOOST_TEST(expected.ilu.opcode == actual.ilu.opcode);
  if (expected.ilu.opcode != NV2AOP_NOP) {
    BOOST_TEST_INFO("ILU");
    check_opcode(expected.ilu, actual.ilu);
  }

  BOOST_TEST(expected.is_final == actual.is_final);
}

BOOST_AUTO_TEST_CASE(step_mac_mov) {
  // MOV oT2.xyzw, v11
  static constexpr uint32_t kTest[][4] = {
      {0x00000000, 0x0020161B, 0x0836106C, 0x2070F858},
  };

  Nv2aVshStep expected;
  clear_step(&expected);

  expected.mac.opcode = NV2AOP_MOV;
  expected.mac.outputs[0].type = NV2ART_OUTPUT;
  expected.mac.outputs[0].index = 11;
  expected.mac.outputs[0].writemask = NV2AWM_XYZW;

  expected.mac.inputs[0].type = NV2ART_INPUT;
  expected.mac.inputs[0].index = 11;

  Nv2aVshStep actual;
  auto result = nv2a_vsh_parse_step(&actual, kTest[0]);

  BOOST_TEST(result == NV2AVPR_SUCCESS);
  check_result(expected, actual);
}

BOOST_AUTO_TEST_CASE(step_mac_mov_final) {
  // MOV oT2.xyzw, v11
  static constexpr uint32_t kTest[][4] = {
      {0x00000000, 0x0020161B, 0x0836106C, 0x2070F859},
  };

  Nv2aVshStep expected;
  clear_step(&expected);
  expected.is_final = true;
  expected.mac.opcode = NV2AOP_MOV;
  expected.mac.outputs[0].type = NV2ART_OUTPUT;
  expected.mac.outputs[0].index = 11;
  expected.mac.outputs[0].writemask = NV2AWM_XYZW;

  expected.mac.inputs[0].type = NV2ART_INPUT;
  expected.mac.inputs[0].index = 11;

  Nv2aVshStep actual;
  auto result = nv2a_vsh_parse_step(&actual, kTest[0]);

  BOOST_TEST(result == NV2AVPR_SUCCESS);
  check_result(expected, actual);
}

BOOST_AUTO_TEST_CASE(step_mac_mad) {
  // MAD oPos.xyz, R12, R1.x, c[59]
  static constexpr uint32_t kTest[][4] = {
      {0x00000000, 0x0087601B, 0xC400286C, 0x3070E801},
  };

  Nv2aVshStep expected;
  clear_step(&expected);

  expected.is_final = true;
  expected.mac.opcode = NV2AOP_MAD;
  expected.mac.outputs[0].type = NV2ART_OUTPUT;
  expected.mac.outputs[0].index = 0;
  expected.mac.outputs[0].writemask = NV2AWM_XYZ;

  expected.mac.inputs[0].type = NV2ART_TEMPORARY;
  expected.mac.inputs[0].index = 12;

  expected.mac.inputs[1].type = NV2ART_TEMPORARY;
  expected.mac.inputs[1].index = 1;
  expected.mac.inputs[1].swizzle[1] = NV2ASW_X;
  expected.mac.inputs[1].swizzle[2] = NV2ASW_X;
  expected.mac.inputs[1].swizzle[3] = NV2ASW_X;

  expected.mac.inputs[2].type = NV2ART_CONTEXT;
  expected.mac.inputs[2].index = 59;

  Nv2aVshStep actual;
  auto result = nv2a_vsh_parse_step(&actual, kTest[0]);

  BOOST_TEST(result == NV2AVPR_SUCCESS);
  check_result(expected, actual);
}

BOOST_AUTO_TEST_CASE(step_mac_dp4) {
  // DP4 oPos.z, v0, c[100]
  static constexpr uint32_t kTest[][4] = {
      {0x00000000, 0x00EC801B, 0x0836186C, 0x20702800},
  };

  Nv2aVshStep expected;
  clear_step(&expected);

  expected.mac.opcode = NV2AOP_DP4;
  expected.mac.outputs[0].type = NV2ART_OUTPUT;
  expected.mac.outputs[0].index = 0;
  expected.mac.outputs[0].writemask = NV2AWM_Z;

  expected.mac.inputs[0].type = NV2ART_INPUT;
  expected.mac.inputs[0].index = 0;

  expected.mac.inputs[1].type = NV2ART_CONTEXT;
  expected.mac.inputs[1].index = 100;

  Nv2aVshStep actual;
  auto result = nv2a_vsh_parse_step(&actual, kTest[0]);

  BOOST_TEST(result == NV2AVPR_SUCCESS);
  check_result(expected, actual);
}

BOOST_AUTO_TEST_CASE(step_mac_mad_ambiguous) {
  // MAD R0.z, R0.z, c[117].z, -c[117].w
  static constexpr uint32_t kTest[][4] = {
      {0x00000000, 0x008EA0AA, 0x05541FFC, 0x32000FF8},
      {0x00000000, 0x008EA0AA, 0x0554BFFD, 0x72000000},
  };

  Nv2aVshStep expected;
  clear_step(&expected);

  expected.mac.opcode = NV2AOP_MAD;
  expected.mac.outputs[0].type = NV2ART_TEMPORARY;
  expected.mac.outputs[0].index = 0;
  expected.mac.outputs[0].writemask = NV2AWM_Z;

  expected.mac.inputs[0].type = NV2ART_TEMPORARY;
  expected.mac.inputs[0].index = 0;
  expected.mac.inputs[0].swizzle[0] = NV2ASW_Z;
  expected.mac.inputs[0].swizzle[1] = NV2ASW_Z;
  expected.mac.inputs[0].swizzle[2] = NV2ASW_Z;
  expected.mac.inputs[0].swizzle[3] = NV2ASW_Z;

  expected.mac.inputs[1].type = NV2ART_CONTEXT;
  expected.mac.inputs[1].index = 117;
  expected.mac.inputs[1].swizzle[0] = NV2ASW_Z;
  expected.mac.inputs[1].swizzle[1] = NV2ASW_Z;
  expected.mac.inputs[1].swizzle[2] = NV2ASW_Z;
  expected.mac.inputs[1].swizzle[3] = NV2ASW_Z;

  expected.mac.inputs[2].type = NV2ART_CONTEXT;
  expected.mac.inputs[2].index = 117;
  expected.mac.inputs[2].is_negated = true;
  expected.mac.inputs[2].swizzle[0] = NV2ASW_W;
  expected.mac.inputs[2].swizzle[1] = NV2ASW_W;
  expected.mac.inputs[2].swizzle[2] = NV2ASW_W;
  expected.mac.inputs[2].swizzle[3] = NV2ASW_W;

  Nv2aVshStep actual;
  auto result = nv2a_vsh_parse_step(&actual, kTest[0]);

  BOOST_TEST(result == NV2AVPR_SUCCESS);
  check_result(expected, actual);

  result = nv2a_vsh_parse_step(&actual, kTest[1]);
  BOOST_TEST(result == NV2AVPR_SUCCESS);
  check_result(expected, actual);
}

BOOST_AUTO_TEST_CASE(step_mac_arl) {
  // ARL A0, R0.x
  static constexpr uint32_t kTest[][4] = {
      {0x00000000, 0x01A00000, 0x0436106C, 0x20700FF8},
  };

  Nv2aVshStep expected;
  clear_step(&expected);

  expected.mac.opcode = NV2AOP_ARL;
  expected.mac.outputs[0].type = NV2ART_ADDRESS;
  expected.mac.outputs[0].index = 0;

  expected.mac.inputs[0].type = NV2ART_TEMPORARY;
  expected.mac.inputs[0].index = 0;
  expected.mac.inputs[0].swizzle[0] = NV2ASW_X;
  expected.mac.inputs[0].swizzle[1] = NV2ASW_X;
  expected.mac.inputs[0].swizzle[2] = NV2ASW_X;
  expected.mac.inputs[0].swizzle[3] = NV2ASW_X;

  Nv2aVshStep actual;
  auto result = nv2a_vsh_parse_step(&actual, kTest[0]);

  BOOST_TEST(result == NV2AVPR_SUCCESS);
  check_result(expected, actual);
}

BOOST_AUTO_TEST_CASE(step_address_relative) {
  // ADD R0.xy, c[A0+121].zw, -c[A0+121].xy
  static constexpr uint32_t kTest[][4] = {
      {0x00000000, 0x006F20BF, 0x9C001456, 0x7C000002},
  };

  Nv2aVshStep expected;
  clear_step(&expected);

  expected.mac.opcode = NV2AOP_ADD;
  expected.mac.outputs[0].type = NV2ART_TEMPORARY;
  expected.mac.outputs[0].index = 0;
  expected.mac.outputs[0].writemask = NV2AWM_XY;

  expected.mac.inputs[0].type = NV2ART_CONTEXT;
  expected.mac.inputs[0].index = 121;
  expected.mac.inputs[0].is_relative = true;
  expected.mac.inputs[0].swizzle[0] = NV2ASW_Z;
  expected.mac.inputs[0].swizzle[1] = NV2ASW_W;
  expected.mac.inputs[0].swizzle[2] = NV2ASW_W;
  expected.mac.inputs[0].swizzle[3] = NV2ASW_W;

  expected.mac.inputs[1].type = NV2ART_CONTEXT;
  expected.mac.inputs[1].index = 121;
  expected.mac.inputs[1].is_negated = true;
  expected.mac.inputs[1].is_relative = true;
  expected.mac.inputs[1].swizzle[0] = NV2ASW_X;
  expected.mac.inputs[1].swizzle[1] = NV2ASW_Y;
  expected.mac.inputs[1].swizzle[2] = NV2ASW_Y;
  expected.mac.inputs[1].swizzle[3] = NV2ASW_Y;

  Nv2aVshStep actual;
  auto result = nv2a_vsh_parse_step(&actual, kTest[0]);

  BOOST_TEST(result == NV2AVPR_SUCCESS);
  check_result(expected, actual);
}

BOOST_AUTO_TEST_CASE(step_ilu_rcp) {
  // RCP oFog.xyzw, v0.w
  static constexpr uint32_t kTest[][4] = {
      {0x00000000, 0x0400001B, 0x083613FC, 0x2070F82C},
  };

  Nv2aVshStep expected;
  clear_step(&expected);

  expected.ilu.opcode = NV2AOP_RCP;
  expected.ilu.outputs[0].type = NV2ART_OUTPUT;
  expected.ilu.outputs[0].index = 5;
  expected.ilu.outputs[0].writemask = NV2AWM_XYZW;

  expected.ilu.inputs[0].type = NV2ART_INPUT;
  expected.ilu.inputs[0].index = 0;
  expected.ilu.inputs[0].swizzle[0] = NV2ASW_W;
  expected.ilu.inputs[0].swizzle[1] = NV2ASW_W;
  expected.ilu.inputs[0].swizzle[2] = NV2ASW_W;
  expected.ilu.inputs[0].swizzle[3] = NV2ASW_W;

  Nv2aVshStep actual;
  auto result = nv2a_vsh_parse_step(&actual, kTest[0]);

  BOOST_TEST(result == NV2AVPR_SUCCESS);
  check_result(expected, actual);
}

BOOST_AUTO_TEST_CASE(step_mac_mul) {
  // MUL oPos.xyz, R12.xyz, c[58].xyz
  static constexpr uint32_t kTest[][4] = {
      {0x00000000, 0x0047401A, 0xC434186C, 0x2070E800},
  };

  Nv2aVshStep expected;
  clear_step(&expected);

  expected.mac.opcode = NV2AOP_MUL;
  expected.mac.outputs[0].type = NV2ART_OUTPUT;
  expected.mac.outputs[0].index = 0;
  expected.mac.outputs[0].writemask = NV2AWM_XYZ;

  expected.mac.inputs[0].type = NV2ART_TEMPORARY;
  expected.mac.inputs[0].index = 12;
  expected.mac.inputs[0].swizzle[3] = NV2ASW_Z;

  expected.mac.inputs[1].type = NV2ART_CONTEXT;
  expected.mac.inputs[1].index = 58;
  expected.mac.inputs[1].swizzle[3] = NV2ASW_Z;

  Nv2aVshStep actual;
  auto result = nv2a_vsh_parse_step(&actual, kTest[0]);

  BOOST_TEST(result == NV2AVPR_SUCCESS);
  check_result(expected, actual);
}

BOOST_AUTO_TEST_CASE(step_paired_mul_mov) {
  // MUL R2.xyzw, R1, c[0] + MOV oD1.xyzw, v4
  static constexpr uint32_t kTest[][4] = {
      {0x00000000, 0x0240081B, 0x1436186C, 0x2F20F824},
  };

  Nv2aVshStep expected;
  clear_step(&expected);

  expected.mac.opcode = NV2AOP_MUL;
  expected.mac.outputs[0].type = NV2ART_TEMPORARY;
  expected.mac.outputs[0].index = 2;
  expected.mac.outputs[0].writemask = NV2AWM_XYZW;

  expected.mac.inputs[0].type = NV2ART_TEMPORARY;
  expected.mac.inputs[0].index = 1;

  expected.mac.inputs[1].type = NV2ART_CONTEXT;
  expected.mac.inputs[1].index = 0;

  expected.ilu.opcode = NV2AOP_MOV;
  expected.ilu.outputs[0].type = NV2ART_OUTPUT;
  expected.ilu.outputs[0].index = 4;
  expected.ilu.outputs[0].writemask = NV2AWM_XYZW;

  expected.ilu.inputs[0].type = NV2ART_INPUT;
  expected.ilu.inputs[0].index = 4;

  Nv2aVshStep actual;
  auto result = nv2a_vsh_parse_step(&actual, kTest[0]);

  BOOST_TEST(result == NV2AVPR_SUCCESS);
  check_result(expected, actual);
}

BOOST_AUTO_TEST_CASE(step_paired_mov_rcp) {
  // MOV oD0.xyzw, v3 + RCP R1.w, R1.w
  static constexpr uint32_t kTest[][4] = {
      {0x00000000, 0x0420061B, 0x083613FC, 0x5011F818},
  };

  Nv2aVshStep expected;
  clear_step(&expected);

  expected.mac.opcode = NV2AOP_MOV;
  expected.mac.outputs[0].type = NV2ART_OUTPUT;
  expected.mac.outputs[0].index = 3;
  expected.mac.outputs[0].writemask = NV2AWM_XYZW;

  expected.mac.inputs[0].type = NV2ART_INPUT;
  expected.mac.inputs[0].index = 3;

  expected.ilu.opcode = NV2AOP_RCP;
  expected.ilu.outputs[0].type = NV2ART_TEMPORARY;
  expected.ilu.outputs[0].index = 1;
  expected.ilu.outputs[0].writemask = NV2AWM_W;

  expected.ilu.inputs[0].type = NV2ART_TEMPORARY;
  expected.ilu.inputs[0].index = 1;
  expected.ilu.inputs[0].swizzle[0] = NV2ASW_W;
  expected.ilu.inputs[0].swizzle[1] = NV2ASW_W;
  expected.ilu.inputs[0].swizzle[2] = NV2ASW_W;
  expected.ilu.inputs[0].swizzle[3] = NV2ASW_W;

  Nv2aVshStep actual;
  auto result = nv2a_vsh_parse_step(&actual, kTest[0]);

  BOOST_TEST(result == NV2AVPR_SUCCESS);
  check_result(expected, actual);
}

BOOST_AUTO_TEST_CASE(step_paired_dp4_rsq) {
  // DP4 oPos.x, R6, c[96] + RSQ R1.x, R2.x
  static constexpr uint32_t kTest[][4] = {
      {0x00000000, 0x08EC001B, 0x64361800, 0x90A88800},
  };

  Nv2aVshStep expected;
  clear_step(&expected);

  expected.mac.opcode = NV2AOP_DP4;
  expected.mac.outputs[0].type = NV2ART_OUTPUT;
  expected.mac.outputs[0].index = 0;
  expected.mac.outputs[0].writemask = NV2AWM_X;

  expected.mac.inputs[0].type = NV2ART_TEMPORARY;
  expected.mac.inputs[0].index = 6;
  expected.mac.inputs[1].type = NV2ART_CONTEXT;
  expected.mac.inputs[1].index = 96;

  expected.ilu.opcode = NV2AOP_RSQ;
  expected.ilu.outputs[0].type = NV2ART_TEMPORARY;
  expected.ilu.outputs[0].index = 1;
  expected.ilu.outputs[0].writemask = NV2AWM_X;

  expected.ilu.inputs[0].type = NV2ART_TEMPORARY;
  expected.ilu.inputs[0].index = 2;
  expected.ilu.inputs[0].swizzle[0] = NV2ASW_X;
  expected.ilu.inputs[0].swizzle[1] = NV2ASW_X;
  expected.ilu.inputs[0].swizzle[2] = NV2ASW_X;
  expected.ilu.inputs[0].swizzle[3] = NV2ASW_X;

  Nv2aVshStep actual;
  auto result = nv2a_vsh_parse_step(&actual, kTest[0]);

  BOOST_TEST(result == NV2AVPR_SUCCESS);
  check_result(expected, actual);
}

BOOST_AUTO_TEST_CASE(step_multi_output) {
  // DP4 oPos.z, R6, c[98] + DP4 R0.x, R6, c[98]
  static constexpr uint32_t kTest[][4] = {
      {0x00000000, 0x00EC401B, 0x64365800, 0x28002800},
  };

  Nv2aVshStep expected;
  clear_step(&expected);

  expected.mac.opcode = NV2AOP_DP4;
  expected.mac.outputs[0].type = NV2ART_TEMPORARY;
  expected.mac.outputs[0].index = 0;
  expected.mac.outputs[0].writemask = NV2AWM_X;
  expected.mac.outputs[1].type = NV2ART_OUTPUT;
  expected.mac.outputs[1].index = 0;
  expected.mac.outputs[1].writemask = NV2AWM_Z;

  expected.mac.inputs[0].type = NV2ART_TEMPORARY;
  expected.mac.inputs[0].index = 6;

  expected.mac.inputs[1].type = NV2ART_CONTEXT;
  expected.mac.inputs[1].index = 98;

  Nv2aVshStep actual;
  auto result = nv2a_vsh_parse_step(&actual, kTest[0]);

  BOOST_TEST(result == NV2AVPR_SUCCESS);
  check_result(expected, actual);
}

BOOST_AUTO_TEST_CASE(step_context_write) {
  // DPH c[15].xy, v4, c[10]
  static constexpr uint32_t kTest[][4] = {
      {0x00000000, 0x00C1481B, 0x0836186C, 0x2070C078},
  };

  Nv2aVshStep expected;
  clear_step(&expected);

  expected.mac.opcode = NV2AOP_DPH;
  expected.mac.outputs[0].type = NV2ART_CONTEXT;
  expected.mac.outputs[0].index = 15;
  expected.mac.outputs[0].writemask = NV2AWM_XY;

  expected.mac.inputs[0].type = NV2ART_INPUT;
  expected.mac.inputs[0].index = 4;

  expected.mac.inputs[1].type = NV2ART_CONTEXT;
  expected.mac.inputs[1].index = 10;

  Nv2aVshStep actual;
  auto result = nv2a_vsh_parse_step(&actual, kTest[0]);

  BOOST_TEST(result == NV2AVPR_SUCCESS);
  check_result(expected, actual);
}

BOOST_AUTO_TEST_CASE(step_paired_cinput_r2) {
  // DP4 R11.y, R5, c[113] + MOV oT2.xyz, R2
  static constexpr uint32_t kTest[][4] = {
      {0, 49160219, 1412831340, 2494621788},
  };

  Nv2aVshStep expected;
  clear_step(&expected);

  expected.mac.opcode = NV2AOP_DP4;
  expected.mac.outputs[0].type = NV2ART_TEMPORARY;
  expected.mac.outputs[0].index = 11;
  expected.mac.outputs[0].writemask = NV2AWM_Y;

  expected.mac.inputs[0].type = NV2ART_TEMPORARY;
  expected.mac.inputs[0].index = 5;

  expected.mac.inputs[1].type = NV2ART_CONTEXT;
  expected.mac.inputs[1].index = 113;

  expected.ilu.opcode = NV2AOP_MOV;
  expected.ilu.outputs[0].type = NV2ART_OUTPUT;
  expected.ilu.outputs[0].index = 11;
  expected.ilu.outputs[0].writemask = NV2AWM_XYZ;

  expected.ilu.inputs[0].type = NV2ART_TEMPORARY;
  expected.ilu.inputs[0].index = 2;

  Nv2aVshStep actual;
  auto result = nv2a_vsh_parse_step(&actual, kTest[0]);

  BOOST_TEST(result == NV2AVPR_SUCCESS);
  check_result(expected, actual);
}

BOOST_AUTO_TEST_CASE(step_ilu_rcp_r3_r2) {
  // rcp r3, r2
  static constexpr uint32_t kTest[][4] = {
      {0x00000000, 0x0400001b, 0x0836106c, 0x903f0ff8},
  };

  Nv2aVshStep expected;
  clear_step(&expected);

  expected.ilu.opcode = NV2AOP_RCP;
  expected.ilu.outputs[0].type = NV2ART_TEMPORARY;
  expected.ilu.outputs[0].index = 3;
  expected.ilu.outputs[0].writemask = NV2AWM_XYZW;

  expected.ilu.inputs[0].type = NV2ART_TEMPORARY;
  expected.ilu.inputs[0].index = 2;
  expected.ilu.inputs[0].swizzle[0] = NV2ASW_X;
  expected.ilu.inputs[0].swizzle[1] = NV2ASW_X;
  expected.ilu.inputs[0].swizzle[2] = NV2ASW_X;
  expected.ilu.inputs[0].swizzle[3] = NV2ASW_X;

  Nv2aVshStep actual;
  auto result = nv2a_vsh_parse_step(&actual, kTest[0]);

  BOOST_TEST(result == NV2AVPR_SUCCESS);
  check_result(expected, actual);
}

BOOST_AUTO_TEST_CASE(step_ilu_rcp_r4_r3) {
  // rcp r4, r3
  static constexpr uint32_t kTest[][4] = {
      {0x00000000, 0x0400001b, 0x0836106c, 0xd04f0ff8},
  };

  Nv2aVshStep expected;
  clear_step(&expected);

  expected.ilu.opcode = NV2AOP_RCP;
  expected.ilu.outputs[0].type = NV2ART_TEMPORARY;
  expected.ilu.outputs[0].index = 4;
  expected.ilu.outputs[0].writemask = NV2AWM_XYZW;

  expected.ilu.inputs[0].type = NV2ART_TEMPORARY;
  expected.ilu.inputs[0].index = 3;
  expected.ilu.inputs[0].swizzle[0] = NV2ASW_X;
  expected.ilu.inputs[0].swizzle[1] = NV2ASW_X;
  expected.ilu.inputs[0].swizzle[2] = NV2ASW_X;
  expected.ilu.inputs[0].swizzle[3] = NV2ASW_X;

  Nv2aVshStep actual;
  auto result = nv2a_vsh_parse_step(&actual, kTest[0]);

  BOOST_TEST(result == NV2AVPR_SUCCESS);
  check_result(expected, actual);
}

BOOST_AUTO_TEST_CASE(step_ilu_rcp_r5_r11) {
  // rcp r5, r11
  static constexpr uint32_t kTest[][4] = {
      {0x00000000, 0x0400001b, 0x0836106e, 0xd05f0ff8},
  };

  Nv2aVshStep expected;
  clear_step(&expected);

  expected.ilu.opcode = NV2AOP_RCP;
  expected.ilu.outputs[0].type = NV2ART_TEMPORARY;
  expected.ilu.outputs[0].index = 5;
  expected.ilu.outputs[0].writemask = NV2AWM_XYZW;

  expected.ilu.inputs[0].type = NV2ART_TEMPORARY;
  expected.ilu.inputs[0].index = 11;
  expected.ilu.inputs[0].swizzle[0] = NV2ASW_X;
  expected.ilu.inputs[0].swizzle[1] = NV2ASW_X;
  expected.ilu.inputs[0].swizzle[2] = NV2ASW_X;
  expected.ilu.inputs[0].swizzle[3] = NV2ASW_X;

  Nv2aVshStep actual;
  auto result = nv2a_vsh_parse_step(&actual, kTest[0]);

  BOOST_TEST(result == NV2AVPR_SUCCESS);
  check_result(expected, actual);
}

BOOST_AUTO_TEST_CASE(step_ilu_rcp_r6_r12) {
  // rcp r6, r12
  static constexpr uint32_t kTest[][4] = {
      {0x00000000, 0x0400001b, 0x0836106f, 0x106f0ff8},
  };

  Nv2aVshStep expected;
  clear_step(&expected);

  expected.ilu.opcode = NV2AOP_RCP;
  expected.ilu.outputs[0].type = NV2ART_TEMPORARY;
  expected.ilu.outputs[0].index = 6;
  expected.ilu.outputs[0].writemask = NV2AWM_XYZW;

  expected.ilu.inputs[0].type = NV2ART_TEMPORARY;
  expected.ilu.inputs[0].index = 12;
  expected.ilu.inputs[0].swizzle[0] = NV2ASW_X;
  expected.ilu.inputs[0].swizzle[1] = NV2ASW_X;
  expected.ilu.inputs[0].swizzle[2] = NV2ASW_X;
  expected.ilu.inputs[0].swizzle[3] = NV2ASW_X;

  Nv2aVshStep actual;
  auto result = nv2a_vsh_parse_step(&actual, kTest[0]);

  BOOST_TEST(result == NV2AVPR_SUCCESS);
  check_result(expected, actual);
}

BOOST_AUTO_TEST_SUITE_END()
