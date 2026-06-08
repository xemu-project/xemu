#include <boost/test/unit_test.hpp>
#include <cmath>

#include "nv2a_vsh_cpu.h"

BOOST_AUTO_TEST_SUITE(basic_operation_suite)

BOOST_AUTO_TEST_CASE(mov) {
  float inputs[] = {0.0f, -1000.0f, 1000.0f, 64.123456f};
  float out[4];
  nv2a_vsh_cpu_mov(out, inputs);

  BOOST_TEST(out[0] == inputs[0]);
  BOOST_TEST(out[1] == inputs[1]);
  BOOST_TEST(out[2] == inputs[2]);
  BOOST_TEST(out[3] == inputs[3]);
}

BOOST_AUTO_TEST_CASE(arl_trivial) {
  float inputs[] = {10.0f, -1000.0f, 1000.0f, 64.123456f};

  float out[4];
  nv2a_vsh_cpu_arl(out, inputs);

  BOOST_TEST(out[0] == inputs[0]);
  BOOST_TEST(out[1] == inputs[0]);
  BOOST_TEST(out[2] == inputs[0]);
  BOOST_TEST(out[3] == inputs[0]);
}

BOOST_AUTO_TEST_CASE(arl_truncate) {
  float inputs[] = {10.12345f, -1000.0f, 1000.0f, 64.123456f};

  float out[4];
  nv2a_vsh_cpu_arl(out, inputs);

  BOOST_TEST(out[0] == 10.0f);
  BOOST_TEST(out[1] == 10.0f);
  BOOST_TEST(out[2] == 10.0f);
  BOOST_TEST(out[3] == 10.0f);
}

BOOST_AUTO_TEST_CASE(arl_biased) {
  float inputs[] = {9.9999999f, -1000.0f, 1000.0f, 64.123456f};

  float out[4];
  nv2a_vsh_cpu_arl(out, inputs);

  BOOST_TEST(out[0] == 10.0f);
  BOOST_TEST(out[1] == 10.0f);
  BOOST_TEST(out[2] == 10.0f);
  BOOST_TEST(out[3] == 10.0f);
}

BOOST_AUTO_TEST_CASE(add_trivial) {
  float inputs[] = {1.0f, 2.0f, 4.0f, 64.0f,
                    10.0f, -10.0f, 100.0f, -100.0f};

  float out[4];
  nv2a_vsh_cpu_add(out, inputs);

  BOOST_TEST(out[0] == 11.0f);
  BOOST_TEST(out[1] == -8.0f);
  BOOST_TEST(out[2] == 104.0f);
  BOOST_TEST(out[3] == -36.0f);
}

//BOOST_AUTO_TEST_CASE(dp3_trivial) {
//
//  float inputs[][8] = {
//      {0.123457f, -0.000423457f, -8.901235e+25f, -323457.0f, -6.243211e+15f,
//          -8.901235e+25f, 0.000423457f, -6.243211e+15f},
//      {-8.901235e+25f, 6.432100e-15f, 5.864211e+16f, 1.844675e+19f, 1.844675e+19f, -6.432100e-15f, 1.234568e+20f, -0.123457f}
//  };
//  float results[][4] = {
//      {-7.036874418e14f,-7.036874418e14f,-7.036874418e14f,-7.036874418e14f},
//      {-3.330426e+38f, -3.330426e+38f, -3.330426e+38f, -3.330426e+38f},
//  };
//
//  for (uint32_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
//    float *in = inputs[i];
//    float *expected = results[i];
//    BOOST_TEST_INFO(i);
//    float out[4];
//    nv2a_vsh_cpu_dp3(out, in);
//
//    BOOST_TEST(out[0] == expected[0]);
//    BOOST_TEST(out[1] == expected[1]);
//    BOOST_TEST(out[2] == expected[2]);
//    BOOST_TEST(out[3] == expected[3]);
//
//  }
//}

// Returned values are very close to correct and could be checked with a more
// permissive float equality.
//BOOST_AUTO_TEST_CASE(log_trivial) {
//  // 0xDB5056B0
//  float inputs[][4] = {
//      {-5.864211e16f, 0.0f, 0.0f, 0.0f},
//      {0.0f, 0.0f, 0.0f, 0.0f},
//      {-0.0f, 0.0f, 0.0f, 0.0f},
//      {INFINITY, 0.0f, 0.0f, 0.0f},
//      {-INFINITY, 0.0f, 0.0f, 0.0f},
//  };
//  float results[][4] = {
//      {55.0f, 1.62765f, 55.7028f, 1.0f},
//      {-INFINITY, 1.0f, -INFINITY, 1.0f},
//      {-INFINITY, 1.0f, -INFINITY, 1.0f},
//      {INFINITY, 1.0f, INFINITY, 1.0f},
//      {INFINITY, 1.0f, INFINITY, 1.0f},
//  };
//
//  for (uint32_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
//    float *in = inputs[i];
//    float *expected = results[i];
//    BOOST_TEST_INFO(i);
//    float out[4];
//    nv2a_vsh_cpu_log(out, in);
//
//    BOOST_TEST(out[0] == expected[0]);
//    BOOST_TEST(out[1] == expected[1]);
//    BOOST_TEST(out[2] == expected[2]);
//    BOOST_TEST(out[3] == expected[3]);
//
//  }
//}

BOOST_AUTO_TEST_SUITE_END()
