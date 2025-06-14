
/*============================================================================

This C source file is part of TestFloat, Release 3e, a package of programs for
testing the correctness of floating-point arithmetic complying with the IEEE
Standard for Floating-Point, by John R. Hauser.

Copyright 2011, 2012, 2013, 2014, 2015, 2016, 2017 The Regents of the
University of California.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions, and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions, and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

 3. Neither the name of the University nor the names of its contributors may
    be used to endorse or promote products derived from this software without
    specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS", AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, ARE
DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=============================================================================*/

#include <stdbool.h>
#include "platform.h"
#include "functions.h"

#define RNEVEN ROUND_NEAR_EVEN
#define RMINM  ROUND_MINMAG
#define RMIN   ROUND_MIN
#define RMAX   ROUND_MAX
#define RNMAXM ROUND_NEAR_MAXMAG

const struct standardFunctionInfo standardFunctionInfos[] = {
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
#ifdef FLOAT16
    { "ui32_to_f16",    UI32_TO_F16,    0, 0 },
#endif
    { "ui32_to_f32",    UI32_TO_F32,    0, 0 },
#ifdef FLOAT64
    { "ui32_to_f64",    UI32_TO_F64,    0, 0 },
#endif
#ifdef EXTFLOAT80
    { "ui32_to_extF80", UI32_TO_EXTF80, 0, 0 },
#endif
#ifdef FLOAT128
    { "ui32_to_f128",   UI32_TO_F128,   0, 0 },
#endif
#ifdef FLOAT16
    { "ui64_to_f16",    UI64_TO_F16,    0, 0 },
#endif
    { "ui64_to_f32",    UI64_TO_F32,    0, 0 },
#ifdef FLOAT64
    { "ui64_to_f64",    UI64_TO_F64,    0, 0 },
#endif
#ifdef EXTFLOAT80
    { "ui64_to_extF80", UI64_TO_EXTF80, 0, 0 },
#endif
#ifdef FLOAT128
    { "ui64_to_f128",   UI64_TO_F128,   0, 0 },
#endif
#ifdef FLOAT16
    { "i32_to_f16",     I32_TO_F16,     0, 0 },
#endif
    { "i32_to_f32",     I32_TO_F32,     0, 0 },
#ifdef FLOAT64
    { "i32_to_f64",     I32_TO_F64,     0, 0 },
#endif
#ifdef EXTFLOAT80
    { "i32_to_extF80",  I32_TO_EXTF80,  0, 0 },
#endif
#ifdef FLOAT128
    { "i32_to_f128",    I32_TO_F128,    0, 0 },
#endif
#ifdef FLOAT16
    { "i64_to_f16",     I64_TO_F16,     0, 0 },
#endif
    { "i64_to_f32",     I64_TO_F32,     0, 0 },
#ifdef FLOAT64
    { "i64_to_f64",     I64_TO_F64,     0, 0 },
#endif
#ifdef EXTFLOAT80
    { "i64_to_extF80",  I64_TO_EXTF80,  0, 0 },
#endif
#ifdef FLOAT128
    { "i64_to_f128",    I64_TO_F128,    0, 0 },
#endif
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
#ifdef FLOAT16
    { "f16_to_ui32_r_near_even",         F16_TO_UI32,         RNEVEN, false },
    { "f16_to_ui32_r_minMag",            F16_TO_UI32,         RMINM,  false },
    { "f16_to_ui32_r_min",               F16_TO_UI32,         RMIN,   false },
    { "f16_to_ui32_r_max",               F16_TO_UI32,         RMAX,   false },
    { "f16_to_ui32_r_near_maxMag",       F16_TO_UI32,         RNMAXM, false },
    { "f16_to_ui64_r_near_even",         F16_TO_UI64,         RNEVEN, false },
    { "f16_to_ui64_r_minMag",            F16_TO_UI64,         RMINM,  false },
    { "f16_to_ui64_r_min",               F16_TO_UI64,         RMIN,   false },
    { "f16_to_ui64_r_max",               F16_TO_UI64,         RMAX,   false },
    { "f16_to_ui64_r_near_maxMag",       F16_TO_UI64,         RNMAXM, false },
    { "f16_to_i32_r_near_even",          F16_TO_I32,          RNEVEN, false },
    { "f16_to_i32_r_minMag",             F16_TO_I32,          RMINM,  false },
    { "f16_to_i32_r_min",                F16_TO_I32,          RMIN,   false },
    { "f16_to_i32_r_max",                F16_TO_I32,          RMAX,   false },
    { "f16_to_i32_r_near_maxMag",        F16_TO_I32,          RNMAXM, false },
    { "f16_to_i64_r_near_even",          F16_TO_I64,          RNEVEN, false },
    { "f16_to_i64_r_minMag",             F16_TO_I64,          RMINM,  false },
    { "f16_to_i64_r_min",                F16_TO_I64,          RMIN,   false },
    { "f16_to_i64_r_max",                F16_TO_I64,          RMAX,   false },
    { "f16_to_i64_r_near_maxMag",        F16_TO_I64,          RNMAXM, false },
    { "f16_to_ui32_rx_near_even",        F16_TO_UI32,         RNEVEN, true  },
    { "f16_to_ui32_rx_minMag",           F16_TO_UI32,         RMINM,  true  },
    { "f16_to_ui32_rx_min",              F16_TO_UI32,         RMIN,   true  },
    { "f16_to_ui32_rx_max",              F16_TO_UI32,         RMAX,   true  },
    { "f16_to_ui32_rx_near_maxMag",      F16_TO_UI32,         RNMAXM, true  },
    { "f16_to_ui64_rx_near_even",        F16_TO_UI64,         RNEVEN, true  },
    { "f16_to_ui64_rx_minMag",           F16_TO_UI64,         RMINM,  true  },
    { "f16_to_ui64_rx_min",              F16_TO_UI64,         RMIN,   true  },
    { "f16_to_ui64_rx_max",              F16_TO_UI64,         RMAX,   true  },
    { "f16_to_ui64_rx_near_maxMag",      F16_TO_UI64,         RNMAXM, true  },
    { "f16_to_i32_rx_near_even",         F16_TO_I32,          RNEVEN, true  },
    { "f16_to_i32_rx_minMag",            F16_TO_I32,          RMINM,  true  },
    { "f16_to_i32_rx_min",               F16_TO_I32,          RMIN,   true  },
    { "f16_to_i32_rx_max",               F16_TO_I32,          RMAX,   true  },
    { "f16_to_i32_rx_near_maxMag",       F16_TO_I32,          RNMAXM, true  },
    { "f16_to_i64_rx_near_even",         F16_TO_I64,          RNEVEN, true  },
    { "f16_to_i64_rx_minMag",            F16_TO_I64,          RMINM,  true  },
    { "f16_to_i64_rx_min",               F16_TO_I64,          RMIN,   true  },
    { "f16_to_i64_rx_max",               F16_TO_I64,          RMAX,   true  },
    { "f16_to_i64_rx_near_maxMag",       F16_TO_I64,          RNMAXM, true  },
    { "f16_to_f32",                      F16_TO_F32,          0,      0     },
#ifdef FLOAT64
    { "f16_to_f64",                      F16_TO_F64,          0,      0     },
#endif
#ifdef EXTFLOAT80
    { "f16_to_extF80",                   F16_TO_EXTF80,       0,      0     },
#endif
#ifdef FLOAT128
    { "f16_to_f128",                     F16_TO_F128,         0,      0     },
#endif
    { "f16_roundToInt_r_near_even",      F16_ROUNDTOINT,      RNEVEN, false },
    { "f16_roundToInt_r_minMag",         F16_ROUNDTOINT,      RMINM,  false },
    { "f16_roundToInt_r_min",            F16_ROUNDTOINT,      RMIN,   false },
    { "f16_roundToInt_r_max",            F16_ROUNDTOINT,      RMAX,   false },
    { "f16_roundToInt_r_near_maxMag",    F16_ROUNDTOINT,      RNMAXM, false },
    { "f16_roundToInt_x",                F16_ROUNDTOINT,      0,      true  },
    { "f16_add",                         F16_ADD,             0,      0     },
    { "f16_sub",                         F16_SUB,             0,      0     },
    { "f16_mul",                         F16_MUL,             0,      0     },
    { "f16_mulAdd",                      F16_MULADD,          0,      0     },
    { "f16_div",                         F16_DIV,             0,      0     },
    { "f16_rem",                         F16_REM,             0,      0     },
    { "f16_sqrt",                        F16_SQRT,            0,      0     },
    { "f16_eq",                          F16_EQ,              0,      0     },
    { "f16_le",                          F16_LE,              0,      0     },
    { "f16_lt",                          F16_LT,              0,      0     },
    { "f16_eq_signaling",                F16_EQ_SIGNALING,    0,      0     },
    { "f16_le_quiet",                    F16_LE_QUIET,        0,      0     },
    { "f16_lt_quiet",                    F16_LT_QUIET,        0,      0     },
#endif
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    { "f32_to_ui32_r_near_even",         F32_TO_UI32,         RNEVEN, false },
    { "f32_to_ui32_r_minMag",            F32_TO_UI32,         RMINM,  false },
    { "f32_to_ui32_r_min",               F32_TO_UI32,         RMIN,   false },
    { "f32_to_ui32_r_max",               F32_TO_UI32,         RMAX,   false },
    { "f32_to_ui32_r_near_maxMag",       F32_TO_UI32,         RNMAXM, false },
    { "f32_to_ui64_r_near_even",         F32_TO_UI64,         RNEVEN, false },
    { "f32_to_ui64_r_minMag",            F32_TO_UI64,         RMINM,  false },
    { "f32_to_ui64_r_min",               F32_TO_UI64,         RMIN,   false },
    { "f32_to_ui64_r_max",               F32_TO_UI64,         RMAX,   false },
    { "f32_to_ui64_r_near_maxMag",       F32_TO_UI64,         RNMAXM, false },
    { "f32_to_i32_r_near_even",          F32_TO_I32,          RNEVEN, false },
    { "f32_to_i32_r_minMag",             F32_TO_I32,          RMINM,  false },
    { "f32_to_i32_r_min",                F32_TO_I32,          RMIN,   false },
    { "f32_to_i32_r_max",                F32_TO_I32,          RMAX,   false },
    { "f32_to_i32_r_near_maxMag",        F32_TO_I32,          RNMAXM, false },
    { "f32_to_i64_r_near_even",          F32_TO_I64,          RNEVEN, false },
    { "f32_to_i64_r_minMag",             F32_TO_I64,          RMINM,  false },
    { "f32_to_i64_r_min",                F32_TO_I64,          RMIN,   false },
    { "f32_to_i64_r_max",                F32_TO_I64,          RMAX,   false },
    { "f32_to_i64_r_near_maxMag",        F32_TO_I64,          RNMAXM, false },
    { "f32_to_ui32_rx_near_even",        F32_TO_UI32,         RNEVEN, true  },
    { "f32_to_ui32_rx_minMag",           F32_TO_UI32,         RMINM,  true  },
    { "f32_to_ui32_rx_min",              F32_TO_UI32,         RMIN,   true  },
    { "f32_to_ui32_rx_max",              F32_TO_UI32,         RMAX,   true  },
    { "f32_to_ui32_rx_near_maxMag",      F32_TO_UI32,         RNMAXM, true  },
    { "f32_to_ui64_rx_near_even",        F32_TO_UI64,         RNEVEN, true  },
    { "f32_to_ui64_rx_minMag",           F32_TO_UI64,         RMINM,  true  },
    { "f32_to_ui64_rx_min",              F32_TO_UI64,         RMIN,   true  },
    { "f32_to_ui64_rx_max",              F32_TO_UI64,         RMAX,   true  },
    { "f32_to_ui64_rx_near_maxMag",      F32_TO_UI64,         RNMAXM, true  },
    { "f32_to_i32_rx_near_even",         F32_TO_I32,          RNEVEN, true  },
    { "f32_to_i32_rx_minMag",            F32_TO_I32,          RMINM,  true  },
    { "f32_to_i32_rx_min",               F32_TO_I32,          RMIN,   true  },
    { "f32_to_i32_rx_max",               F32_TO_I32,          RMAX,   true  },
    { "f32_to_i32_rx_near_maxMag",       F32_TO_I32,          RNMAXM, true  },
    { "f32_to_i64_rx_near_even",         F32_TO_I64,          RNEVEN, true  },
    { "f32_to_i64_rx_minMag",            F32_TO_I64,          RMINM,  true  },
    { "f32_to_i64_rx_min",               F32_TO_I64,          RMIN,   true  },
    { "f32_to_i64_rx_max",               F32_TO_I64,          RMAX,   true  },
    { "f32_to_i64_rx_near_maxMag",       F32_TO_I64,          RNMAXM, true  },
#ifdef FLOAT16
    { "f32_to_f16",                      F32_TO_F16,          0,      0     },
#endif
#ifdef FLOAT64
    { "f32_to_f64",                      F32_TO_F64,          0,      0     },
#endif
#ifdef EXTFLOAT80
    { "f32_to_extF80",                   F32_TO_EXTF80,       0,      0     },
#endif
#ifdef FLOAT128
    { "f32_to_f128",                     F32_TO_F128,         0,      0     },
#endif
    { "f32_roundToInt_r_near_even",      F32_ROUNDTOINT,      RNEVEN, false },
    { "f32_roundToInt_r_minMag",         F32_ROUNDTOINT,      RMINM,  false },
    { "f32_roundToInt_r_min",            F32_ROUNDTOINT,      RMIN,   false },
    { "f32_roundToInt_r_max",            F32_ROUNDTOINT,      RMAX,   false },
    { "f32_roundToInt_r_near_maxMag",    F32_ROUNDTOINT,      RNMAXM, false },
    { "f32_roundToInt_x",                F32_ROUNDTOINT,      0,      true  },
    { "f32_add",                         F32_ADD,             0,      0     },
    { "f32_sub",                         F32_SUB,             0,      0     },
    { "f32_mul",                         F32_MUL,             0,      0     },
    { "f32_mulAdd",                      F32_MULADD,          0,      0     },
    { "f32_div",                         F32_DIV,             0,      0     },
    { "f32_rem",                         F32_REM,             0,      0     },
    { "f32_sqrt",                        F32_SQRT,            0,      0     },
    { "f32_eq",                          F32_EQ,              0,      0     },
    { "f32_le",                          F32_LE,              0,      0     },
    { "f32_lt",                          F32_LT,              0,      0     },
    { "f32_eq_signaling",                F32_EQ_SIGNALING,    0,      0     },
    { "f32_le_quiet",                    F32_LE_QUIET,        0,      0     },
    { "f32_lt_quiet",                    F32_LT_QUIET,        0,      0     },
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
#ifdef FLOAT64
    { "f64_to_ui32_r_near_even",         F64_TO_UI32,         RNEVEN, false },
    { "f64_to_ui32_r_minMag",            F64_TO_UI32,         RMINM,  false },
    { "f64_to_ui32_r_min",               F64_TO_UI32,         RMIN,   false },
    { "f64_to_ui32_r_max",               F64_TO_UI32,         RMAX,   false },
    { "f64_to_ui32_r_near_maxMag",       F64_TO_UI32,         RNMAXM, false },
    { "f64_to_ui64_r_near_even",         F64_TO_UI64,         RNEVEN, false },
    { "f64_to_ui64_r_minMag",            F64_TO_UI64,         RMINM,  false },
    { "f64_to_ui64_r_min",               F64_TO_UI64,         RMIN,   false },
    { "f64_to_ui64_r_max",               F64_TO_UI64,         RMAX,   false },
    { "f64_to_ui64_r_near_maxMag",       F64_TO_UI64,         RNMAXM, false },
    { "f64_to_i32_r_near_even",          F64_TO_I32,          RNEVEN, false },
    { "f64_to_i32_r_minMag",             F64_TO_I32,          RMINM,  false },
    { "f64_to_i32_r_min",                F64_TO_I32,          RMIN,   false },
    { "f64_to_i32_r_max",                F64_TO_I32,          RMAX,   false },
    { "f64_to_i32_r_near_maxMag",        F64_TO_I32,          RNMAXM, false },
    { "f64_to_i64_r_near_even",          F64_TO_I64,          RNEVEN, false },
    { "f64_to_i64_r_minMag",             F64_TO_I64,          RMINM,  false },
    { "f64_to_i64_r_min",                F64_TO_I64,          RMIN,   false },
    { "f64_to_i64_r_max",                F64_TO_I64,          RMAX,   false },
    { "f64_to_i64_r_near_maxMag",        F64_TO_I64,          RNMAXM, false },
    { "f64_to_ui32_rx_near_even",        F64_TO_UI32,         RNEVEN, true  },
    { "f64_to_ui32_rx_minMag",           F64_TO_UI32,         RMINM,  true  },
    { "f64_to_ui32_rx_min",              F64_TO_UI32,         RMIN,   true  },
    { "f64_to_ui32_rx_max",              F64_TO_UI32,         RMAX,   true  },
    { "f64_to_ui32_rx_near_maxMag",      F64_TO_UI32,         RNMAXM, true  },
    { "f64_to_ui64_rx_near_even",        F64_TO_UI64,         RNEVEN, true  },
    { "f64_to_ui64_rx_minMag",           F64_TO_UI64,         RMINM,  true  },
    { "f64_to_ui64_rx_min",              F64_TO_UI64,         RMIN,   true  },
    { "f64_to_ui64_rx_max",              F64_TO_UI64,         RMAX,   true  },
    { "f64_to_ui64_rx_near_maxMag",      F64_TO_UI64,         RNMAXM, true  },
    { "f64_to_i32_rx_near_even",         F64_TO_I32,          RNEVEN, true  },
    { "f64_to_i32_rx_minMag",            F64_TO_I32,          RMINM,  true  },
    { "f64_to_i32_rx_min",               F64_TO_I32,          RMIN,   true  },
    { "f64_to_i32_rx_max",               F64_TO_I32,          RMAX,   true  },
    { "f64_to_i32_rx_near_maxMag",       F64_TO_I32,          RNMAXM, true  },
    { "f64_to_i64_rx_near_even",         F64_TO_I64,          RNEVEN, true  },
    { "f64_to_i64_rx_minMag",            F64_TO_I64,          RMINM,  true  },
    { "f64_to_i64_rx_min",               F64_TO_I64,          RMIN,   true  },
    { "f64_to_i64_rx_max",               F64_TO_I64,          RMAX,   true  },
    { "f64_to_i64_rx_near_maxMag",       F64_TO_I64,          RNMAXM, true  },
#ifdef FLOAT16
    { "f64_to_f16",                      F64_TO_F16,          0,      0     },
#endif
    { "f64_to_f32",                      F64_TO_F32,          0,      0     },
#ifdef EXTFLOAT80
    { "f64_to_extF80",                   F64_TO_EXTF80,       0,      0     },
#endif
#ifdef FLOAT128
    { "f64_to_f128",                     F64_TO_F128,         0,      0     },
#endif
    { "f64_roundToInt_r_near_even",      F64_ROUNDTOINT,      RNEVEN, false },
    { "f64_roundToInt_r_minMag",         F64_ROUNDTOINT,      RMINM,  false },
    { "f64_roundToInt_r_min",            F64_ROUNDTOINT,      RMIN,   false },
    { "f64_roundToInt_r_max",            F64_ROUNDTOINT,      RMAX,   false },
    { "f64_roundToInt_r_near_maxMag",    F64_ROUNDTOINT,      RNMAXM, false },
    { "f64_roundToInt_x",                F64_ROUNDTOINT,      0,      true  },
    { "f64_add",                         F64_ADD,             0,      0     },
    { "f64_sub",                         F64_SUB,             0,      0     },
    { "f64_mul",                         F64_MUL,             0,      0     },
    { "f64_mulAdd",                      F64_MULADD,          0,      0     },
    { "f64_div",                         F64_DIV,             0,      0     },
    { "f64_rem",                         F64_REM,             0,      0     },
    { "f64_sqrt",                        F64_SQRT,            0,      0     },
    { "f64_eq",                          F64_EQ,              0,      0     },
    { "f64_le",                          F64_LE,              0,      0     },
    { "f64_lt",                          F64_LT,              0,      0     },
    { "f64_eq_signaling",                F64_EQ_SIGNALING,    0,      0     },
    { "f64_le_quiet",                    F64_LE_QUIET,        0,      0     },
    { "f64_lt_quiet",                    F64_LT_QUIET,        0,      0     },
#endif
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
#ifdef EXTFLOAT80
    { "extF80_to_ui32_r_near_even",      EXTF80_TO_UI32,      RNEVEN, false },
    { "extF80_to_ui32_r_minMag",         EXTF80_TO_UI32,      RMINM,  false },
    { "extF80_to_ui32_r_min",            EXTF80_TO_UI32,      RMIN,   false },
    { "extF80_to_ui32_r_max",            EXTF80_TO_UI32,      RMAX,   false },
    { "extF80_to_ui32_r_near_maxMag",    EXTF80_TO_UI32,      RNMAXM, false },
    { "extF80_to_ui64_r_near_even",      EXTF80_TO_UI64,      RNEVEN, false },
    { "extF80_to_ui64_r_minMag",         EXTF80_TO_UI64,      RMINM,  false },
    { "extF80_to_ui64_r_min",            EXTF80_TO_UI64,      RMIN,   false },
    { "extF80_to_ui64_r_max",            EXTF80_TO_UI64,      RMAX,   false },
    { "extF80_to_ui64_r_near_maxMag",    EXTF80_TO_UI64,      RNMAXM, false },
    { "extF80_to_i32_r_near_even",       EXTF80_TO_I32,       RNEVEN, false },
    { "extF80_to_i32_r_minMag",          EXTF80_TO_I32,       RMINM,  false },
    { "extF80_to_i32_r_min",             EXTF80_TO_I32,       RMIN,   false },
    { "extF80_to_i32_r_max",             EXTF80_TO_I32,       RMAX,   false },
    { "extF80_to_i32_r_near_maxMag",     EXTF80_TO_I32,       RNMAXM, false },
    { "extF80_to_i64_r_near_even",       EXTF80_TO_I64,       RNEVEN, false },
    { "extF80_to_i64_r_minMag",          EXTF80_TO_I64,       RMINM,  false },
    { "extF80_to_i64_r_min",             EXTF80_TO_I64,       RMIN,   false },
    { "extF80_to_i64_r_max",             EXTF80_TO_I64,       RMAX,   false },
    { "extF80_to_i64_r_near_maxMag",     EXTF80_TO_I64,       RNMAXM, false },
    { "extF80_to_ui32_rx_near_even",     EXTF80_TO_UI32,      RNEVEN, true  },
    { "extF80_to_ui32_rx_minMag",        EXTF80_TO_UI32,      RMINM,  true  },
    { "extF80_to_ui32_rx_min",           EXTF80_TO_UI32,      RMIN,   true  },
    { "extF80_to_ui32_rx_max",           EXTF80_TO_UI32,      RMAX,   true  },
    { "extF80_to_ui32_rx_near_maxMag",   EXTF80_TO_UI32,      RNMAXM, true  },
    { "extF80_to_ui64_rx_near_even",     EXTF80_TO_UI64,      RNEVEN, true  },
    { "extF80_to_ui64_rx_minMag",        EXTF80_TO_UI64,      RMINM,  true  },
    { "extF80_to_ui64_rx_min",           EXTF80_TO_UI64,      RMIN,   true  },
    { "extF80_to_ui64_rx_max",           EXTF80_TO_UI64,      RMAX,   true  },
    { "extF80_to_ui64_rx_near_maxMag",   EXTF80_TO_UI64,      RNMAXM, true  },
    { "extF80_to_i32_rx_near_even",      EXTF80_TO_I32,       RNEVEN, true  },
    { "extF80_to_i32_rx_minMag",         EXTF80_TO_I32,       RMINM,  true  },
    { "extF80_to_i32_rx_min",            EXTF80_TO_I32,       RMIN,   true  },
    { "extF80_to_i32_rx_max",            EXTF80_TO_I32,       RMAX,   true  },
    { "extF80_to_i32_rx_near_maxMag",    EXTF80_TO_I32,       RNMAXM, true  },
    { "extF80_to_i64_rx_near_even",      EXTF80_TO_I64,       RNEVEN, true  },
    { "extF80_to_i64_rx_minMag",         EXTF80_TO_I64,       RMINM,  true  },
    { "extF80_to_i64_rx_min",            EXTF80_TO_I64,       RMIN,   true  },
    { "extF80_to_i64_rx_max",            EXTF80_TO_I64,       RMAX,   true  },
    { "extF80_to_i64_rx_near_maxMag",    EXTF80_TO_I64,       RNMAXM, true  },
#ifdef FLOAT16
    { "extF80_to_f16",                   EXTF80_TO_F16,       0,      0     },
#endif
    { "extF80_to_f32",                   EXTF80_TO_F32,       0,      0     },
#ifdef FLOAT64
    { "extF80_to_f64",                   EXTF80_TO_F64,       0,      0     },
#endif
#ifdef FLOAT128
    { "extF80_to_f128",                  EXTF80_TO_F128,      0,      0     },
#endif
    { "extF80_roundToInt_r_near_even",   EXTF80_ROUNDTOINT,   RNEVEN, false },
    { "extF80_roundToInt_r_minMag",      EXTF80_ROUNDTOINT,   RMINM,  false },
    { "extF80_roundToInt_r_min",         EXTF80_ROUNDTOINT,   RMIN,   false },
    { "extF80_roundToInt_r_max",         EXTF80_ROUNDTOINT,   RMAX,   false },
    { "extF80_roundToInt_r_near_maxMag", EXTF80_ROUNDTOINT,   RNMAXM, false },
    { "extF80_roundToInt_x",             EXTF80_ROUNDTOINT,   0,      true  },
    { "extF80_add",                      EXTF80_ADD,          0,      0     },
    { "extF80_sub",                      EXTF80_SUB,          0,      0     },
    { "extF80_mul",                      EXTF80_MUL,          0,      0     },
    { "extF80_div",                      EXTF80_DIV,          0,      0     },
    { "extF80_rem",                      EXTF80_REM,          0,      0     },
    { "extF80_sqrt",                     EXTF80_SQRT,         0,      0     },
    { "extF80_eq",                       EXTF80_EQ,           0,      0     },
    { "extF80_le",                       EXTF80_LE,           0,      0     },
    { "extF80_lt",                       EXTF80_LT,           0,      0     },
    { "extF80_eq_signaling",             EXTF80_EQ_SIGNALING, 0,      0     },
    { "extF80_le_quiet",                 EXTF80_LE_QUIET,     0,      0     },
    { "extF80_lt_quiet",                 EXTF80_LT_QUIET,     0,      0     },
#endif
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
#ifdef FLOAT128
    { "f128_to_ui32_r_near_even",        F128_TO_UI32,        RNEVEN, false },
    { "f128_to_ui32_r_minMag",           F128_TO_UI32,        RMINM,  false },
    { "f128_to_ui32_r_min",              F128_TO_UI32,        RMIN,   false },
    { "f128_to_ui32_r_max",              F128_TO_UI32,        RMAX,   false },
    { "f128_to_ui32_r_near_maxMag",      F128_TO_UI32,        RNMAXM, false },
    { "f128_to_ui64_r_near_even",        F128_TO_UI64,        RNEVEN, false },
    { "f128_to_ui64_r_minMag",           F128_TO_UI64,        RMINM,  false },
    { "f128_to_ui64_r_min",              F128_TO_UI64,        RMIN,   false },
    { "f128_to_ui64_r_max",              F128_TO_UI64,        RMAX,   false },
    { "f128_to_ui64_r_near_maxMag",      F128_TO_UI64,        RNMAXM, false },
    { "f128_to_i32_r_near_even",         F128_TO_I32,         RNEVEN, false },
    { "f128_to_i32_r_minMag",            F128_TO_I32,         RMINM,  false },
    { "f128_to_i32_r_min",               F128_TO_I32,         RMIN,   false },
    { "f128_to_i32_r_max",               F128_TO_I32,         RMAX,   false },
    { "f128_to_i32_r_near_maxMag",       F128_TO_I32,         RNMAXM, false },
    { "f128_to_i64_r_near_even",         F128_TO_I64,         RNEVEN, false },
    { "f128_to_i64_r_minMag",            F128_TO_I64,         RMINM,  false },
    { "f128_to_i64_r_min",               F128_TO_I64,         RMIN,   false },
    { "f128_to_i64_r_max",               F128_TO_I64,         RMAX,   false },
    { "f128_to_i64_r_near_maxMag",       F128_TO_I64,         RNMAXM, false },
    { "f128_to_ui32_rx_near_even",       F128_TO_UI32,        RNEVEN, true  },
    { "f128_to_ui32_rx_minMag",          F128_TO_UI32,        RMINM,  true  },
    { "f128_to_ui32_rx_min",             F128_TO_UI32,        RMIN,   true  },
    { "f128_to_ui32_rx_max",             F128_TO_UI32,        RMAX,   true  },
    { "f128_to_ui32_rx_near_maxMag",     F128_TO_UI32,        RNMAXM, true  },
    { "f128_to_ui64_rx_near_even",       F128_TO_UI64,        RNEVEN, true  },
    { "f128_to_ui64_rx_minMag",          F128_TO_UI64,        RMINM,  true  },
    { "f128_to_ui64_rx_min",             F128_TO_UI64,        RMIN,   true  },
    { "f128_to_ui64_rx_max",             F128_TO_UI64,        RMAX,   true  },
    { "f128_to_ui64_rx_near_maxMag",     F128_TO_UI64,        RNMAXM, true  },
    { "f128_to_i32_rx_near_even",        F128_TO_I32,         RNEVEN, true  },
    { "f128_to_i32_rx_minMag",           F128_TO_I32,         RMINM,  true  },
    { "f128_to_i32_rx_min",              F128_TO_I32,         RMIN,   true  },
    { "f128_to_i32_rx_max",              F128_TO_I32,         RMAX,   true  },
    { "f128_to_i32_rx_near_maxMag",      F128_TO_I32,         RNMAXM, true  },
    { "f128_to_i64_rx_near_even",        F128_TO_I64,         RNEVEN, true  },
    { "f128_to_i64_rx_minMag",           F128_TO_I64,         RMINM,  true  },
    { "f128_to_i64_rx_min",              F128_TO_I64,         RMIN,   true  },
    { "f128_to_i64_rx_max",              F128_TO_I64,         RMAX,   true  },
    { "f128_to_i64_rx_near_maxMag",      F128_TO_I64,         RNMAXM, true  },
#ifdef FLOAT16
    { "f128_to_f16",                     F128_TO_F16,         0,      0     },
#endif
    { "f128_to_f32",                     F128_TO_F32,         0,      0     },
#ifdef FLOAT64
    { "f128_to_f64",                     F128_TO_F64,         0,      0     },
#endif
#ifdef EXTFLOAT80
    { "f128_to_extF80",                  F128_TO_EXTF80,      0,      0     },
#endif
    { "f128_roundToInt_r_near_even",     F128_ROUNDTOINT,     RNEVEN, false },
    { "f128_roundToInt_r_minMag",        F128_ROUNDTOINT,     RMINM,  false },
    { "f128_roundToInt_r_min",           F128_ROUNDTOINT,     RMIN,   false },
    { "f128_roundToInt_r_max",           F128_ROUNDTOINT,     RMAX,   false },
    { "f128_roundToInt_r_near_maxMag",   F128_ROUNDTOINT,     RNMAXM, false },
    { "f128_roundToInt_x",               F128_ROUNDTOINT,     0,      true  },
    { "f128_add",                        F128_ADD,            0,      0     },
    { "f128_sub",                        F128_SUB,            0,      0     },
    { "f128_mul",                        F128_MUL,            0,      0     },
    { "f128_mulAdd",                     F128_MULADD,         0,      0     },
    { "f128_div",                        F128_DIV,            0,      0     },
    { "f128_rem",                        F128_REM,            0,      0     },
    { "f128_sqrt",                       F128_SQRT,           0,      0     },
    { "f128_eq",                         F128_EQ,             0,      0     },
    { "f128_le",                         F128_LE,             0,      0     },
    { "f128_lt",                         F128_LT,             0,      0     },
    { "f128_eq_signaling",               F128_EQ_SIGNALING,   0,      0     },
    { "f128_le_quiet",                   F128_LE_QUIET,       0,      0     },
    { "f128_lt_quiet",                   F128_LT_QUIET,       0,      0     },
#endif
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    { 0, 0, 0, 0 }
};

