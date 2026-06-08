#include "nv2a_vsh_cpu.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define _X 0
#define _Y 1
#define _Z 2
#define _W 3
#define COMP(array, index, component) (array[(index) * 4 + component])

void nv2a_vsh_cpu_mov(float *out, const float *inputs) {
  memcpy(out, inputs, sizeof(float) * 4);
}

void nv2a_vsh_cpu_arl(float *out, const float *inputs) {
  float val = floorf(COMP(inputs, 0, _X) + 0.001f);
  out[0] = val;
  out[1] = val;
  out[2] = val;
  out[3] = val;
}

// nv2a does not allow multiplication of non-inf numbers to result in inf.
static inline float fix_inf_mult(float a, float b) {
  float output = a * b;
  if (!isinf(output) || isinf(a) || isinf(b)) {
    return output;
  }

  uint32_t fixed = (*(uint32_t*)&output & 0xFF000000) + 0x7FFFFF;
  return *(float*)&fixed;
}

void nv2a_vsh_cpu_mul(float *out, const float *inputs) {
  out[0] = fix_inf_mult(COMP(inputs, 0, _X), COMP(inputs, 1, _X));
  out[1] = fix_inf_mult(COMP(inputs, 0, _Y), COMP(inputs, 1, _Y));
  out[2] = fix_inf_mult(COMP(inputs, 0, _Z), COMP(inputs, 1, _Z));
  out[3] = fix_inf_mult(COMP(inputs, 0, _W), COMP(inputs, 1, _W));
}

void nv2a_vsh_cpu_add(float *out, const float *inputs) {
  out[0] = COMP(inputs, 0, _X) + COMP(inputs, 1, _X);
  out[1] = COMP(inputs, 0, _Y) + COMP(inputs, 1, _Y);
  out[2] = COMP(inputs, 0, _Z) + COMP(inputs, 1, _Z);
  out[3] = COMP(inputs, 0, _W) + COMP(inputs, 1, _W);
}

void nv2a_vsh_cpu_mad(float *out, const float *inputs) {
  out[0] = fix_inf_mult(COMP(inputs, 0, _X), COMP(inputs, 1, _X)) + COMP(inputs, 2, _X);
  out[1] = fix_inf_mult(COMP(inputs, 0, _Y), COMP(inputs, 1, _Y)) + COMP(inputs, 2, _Y);
  out[2] = fix_inf_mult(COMP(inputs, 0, _Z), COMP(inputs, 1, _Z)) + COMP(inputs, 2, _Z);
  out[3] = fix_inf_mult(COMP(inputs, 0, _W), COMP(inputs, 1, _W)) + COMP(inputs, 2, _W);
}

static inline float fix_inf(float in) {
  if (!isinf(in)) {
    return in;
  }

  uint32_t fixed = (*(uint32_t*)&in & 0xFF000000) + 0x7FFFFF;
  return *(float*)&fixed;
}

void nv2a_vsh_cpu_dp3(float *out, const float *inputs) {
  float result = fix_inf_mult(COMP(inputs, 0, _X), COMP(inputs, 1, _X)) +
                 fix_inf_mult(COMP(inputs, 0, _Y), COMP(inputs, 1, _Y)) +
                 fix_inf_mult(COMP(inputs, 0, _Z), COMP(inputs, 1, _Z));
  result = fix_inf(result);
  out[0] = result;
  out[1] = result;
  out[2] = result;
  out[3] = result;
}

void nv2a_vsh_cpu_dph(float *out, const float *inputs) {
  float result = fix_inf_mult(COMP(inputs, 0, _X), COMP(inputs, 1, _X)) +
                 fix_inf_mult(COMP(inputs, 0, _Y), COMP(inputs, 1, _Y)) +
                 fix_inf_mult(COMP(inputs, 0, _Z), COMP(inputs, 1, _Z)) + COMP(inputs, 1, _W);
  result = fix_inf(result);
  out[0] = result;
  out[1] = result;
  out[2] = result;
  out[3] = result;
}

void nv2a_vsh_cpu_dp4(float *out, const float *inputs) {
  float result =
      fix_inf_mult(COMP(inputs, 0, _X), COMP(inputs, 1, _X)) +
      fix_inf_mult(COMP(inputs, 0, _Y), COMP(inputs, 1, _Y)) +
      fix_inf_mult(COMP(inputs, 0, _Z), COMP(inputs, 1, _Z)) +
      fix_inf_mult(COMP(inputs, 0, _W), COMP(inputs, 1, _W));
  result = fix_inf(result);
  out[0] = result;
  out[1] = result;
  out[2] = result;
  out[3] = result;
}

void nv2a_vsh_cpu_dst(float *out, const float *inputs) {
  out[0] = 1.0f;
  out[1] = fix_inf_mult(COMP(inputs, 0, _Y), COMP(inputs, 1, _Y));
  out[2] = COMP(inputs, 0, _Z);
  out[3] = COMP(inputs, 1, _W);
}

void nv2a_vsh_cpu_min(float *out, const float *inputs) {
  out[0] =
      COMP(inputs, 0, _X) < COMP(inputs, 1, _X) ? COMP(inputs, 0, _X) : COMP(inputs, 1, _X);
  out[1] =
      COMP(inputs, 0, _Y) < COMP(inputs, 1, _Y) ? COMP(inputs, 0, _Y) : COMP(inputs, 1, _Y);
  out[2] =
      COMP(inputs, 0, _Z) < COMP(inputs, 1, _Z) ? COMP(inputs, 0, _Z) : COMP(inputs, 1, _Z);
  out[3] =
      COMP(inputs, 0, _W) < COMP(inputs, 1, _W) ? COMP(inputs, 0, _W) : COMP(inputs, 1, _W);
}

void nv2a_vsh_cpu_max(float *out, const float *inputs) {
  out[0] =
      COMP(inputs, 0, _X) > COMP(inputs, 1, _X) ? COMP(inputs, 0, _X) : COMP(inputs, 1, _X);
  out[1] =
      COMP(inputs, 0, _Y) > COMP(inputs, 1, _Y) ? COMP(inputs, 0, _Y) : COMP(inputs, 1, _Y);
  out[2] =
      COMP(inputs, 0, _Z) > COMP(inputs, 1, _Z) ? COMP(inputs, 0, _Z) : COMP(inputs, 1, _Z);
  out[3] =
      COMP(inputs, 0, _W) > COMP(inputs, 1, _W) ? COMP(inputs, 0, _W) : COMP(inputs, 1, _W);
}

static inline float nv2a_less_than(float a, float b) {
  if (a < b) {
    return 1.0f;
  }

  // nv2a hardware treats -0 as < 0.
  uint32_t a_int = *(uint32_t*)&a;
  uint32_t b_int = *(uint32_t*)&b;
  if (a_int == 0x80000000 && !b_int) {
    return 1.0f;
  }

  return 0.0f;
}

void nv2a_vsh_cpu_slt(float *out, const float *inputs) {
  out[0] = nv2a_less_than(COMP(inputs, 0, _X), COMP(inputs, 1, _X));
  out[1] = nv2a_less_than(COMP(inputs, 0, _Y), COMP(inputs, 1, _Y));
  out[2] = nv2a_less_than(COMP(inputs, 0, _Z), COMP(inputs, 1, _Z));
  out[3] = nv2a_less_than(COMP(inputs, 0, _W), COMP(inputs, 1, _W));
}

void nv2a_vsh_cpu_sge(float *out, const float *inputs) {
  out[0] = -1.0f * nv2a_less_than(COMP(inputs, 0, _X), COMP(inputs, 1, _X)) + 1.0f;
  out[1] = -1.0f * nv2a_less_than(COMP(inputs, 0, _Y), COMP(inputs, 1, _Y)) + 1.0f;
  out[2] = -1.0f * nv2a_less_than(COMP(inputs, 0, _Z), COMP(inputs, 1, _Z)) + 1.0f;
  out[3] = -1.0f * nv2a_less_than(COMP(inputs, 0, _W), COMP(inputs, 1, _W)) + 1.0f;
}

void nv2a_vsh_cpu_rcp(float *out, const float *inputs) {
  float in = COMP(inputs, 0, _X);
  float result;
  if (in == 1.0f) {
    result = 1.0f;
  } else if (in == 0.0f) {
    // nv2a preserves the sign.
    if (*(uint32_t*)&in & 0x80000000) {
      result = -INFINITY;
    } else {
      result = INFINITY;
    }
  } else {
    result = 1.0f / in;
  }
  out[0] = result;
  out[1] = result;
  out[2] = result;
  out[3] = result;
}

static const uint32_t kRCCMaxInt = 0x5F800000;
static const uint32_t kRCCMaxNegInt = 0xDF800000;

void nv2a_vsh_cpu_rcc(float *out, const float *inputs) {
  float result;
  float in = COMP(inputs, 0, _X);
  if (in == 1.0f) {
    result = 1.0f;
  } else {
    result = 1.0f / in;
    if (result > 0.0f) {
      if (result < 5.42101e-020f) {
        result = 5.42101e-020f;
      } else if (result > 1.884467e+019f) {
        result = *(float*)&kRCCMaxInt;
      }
    } else {
      if (result < -1.884467e+019f) {
        result = *(float*)&kRCCMaxNegInt;
      } else if (result > -5.42101e-020f) {
        result = -5.42101e-020f;
      }
    }
  }

  out[0] = result;
  out[1] = result;
  out[2] = result;
  out[3] = result;
}

void nv2a_vsh_cpu_rsq(float *out, const float *inputs) {
  float in = fabsf(inputs[0]);
  float result;
  if (in == 1.0f) {
    result = 1.0f;
  } else if (in == 0.0f) {
    result = INFINITY;
  } else {
    result = 1.0f / sqrtf(in);
  }

  out[0] = result;
  out[1] = result;
  out[2] = result;
  out[3] = result;
}

void nv2a_vsh_cpu_exp(float *out, const float *inputs) {
  float tmp = floorf(inputs[0]);
  out[0] = powf(2.0f, tmp);
  out[1] = inputs[0] - tmp;
  out[2] = powf(2.0f, inputs[0]);
  out[3] = 1.0f;
}

void nv2a_vsh_cpu_log(float *out, const float *inputs) {
  // TODO: Validate this on HW.
  float tmp = fabsf(inputs[0]);
  if (tmp == 0.0f) {
    out[0] = -INFINITY;
    out[1] = 1.0f;
    out[2] = -INFINITY;
  } else if (isinf(tmp)) {
    out[0] = INFINITY;
    out[1] = 1.0f;
    out[2] = INFINITY;
  } else {
    // frexpf returns values that do not match nv2a, so the exponent is
    // extracted manually.
    uint32_t tmp_int = *(uint32_t*)&tmp;
    uint32_t exponent = ((tmp_int >> 23) & 0xFF) - 127;
    uint32_t mantissa = (tmp_int & 0x7FFFFF) | 0x3F800000;

    out[0] = (float)exponent;
    out[1] = *(float*)&mantissa;
    out[2] = log2f(tmp);
  }

  out[3] = 1.0f;
}

void nv2a_vsh_cpu_lit(float *out, const float *inputs) {
  static const float kMax = 127.9961f;

  out[0] = 1.0f;
  out[1] = 0.0f;
  out[2] = 0.0f;
  out[3] = 1.0f;

  float power = inputs[3] < -kMax
                    ? -kMax
                    : (inputs[3] > kMax ? kMax : inputs[3]);
  if (inputs[0] > 0.0f) {
    out[1] = inputs[0];
    if (inputs[1] > 0.0f) {
      out[2] = powf(inputs[1], power);
    }
  }
}
