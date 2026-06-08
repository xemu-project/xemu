
/*============================================================================

This C header file is part of TestFloat, Release 3e, a package of programs for
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

#include <stdint.h>

/*----------------------------------------------------------------------------
| Warning:  This list must match the contents of "functionInfos.c".
*----------------------------------------------------------------------------*/
enum {
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
#ifdef FLOAT16
    UI32_TO_F16 = 1,
    UI32_TO_F32,
#else
    UI32_TO_F32 = 1,
#endif
#ifdef FLOAT64
    UI32_TO_F64,
#endif
#ifdef EXTFLOAT80
    UI32_TO_EXTF80,
#endif
#ifdef FLOAT128
    UI32_TO_F128,
#endif
#ifdef FLOAT16
    UI64_TO_F16,
#endif
    UI64_TO_F32,
#ifdef FLOAT64
    UI64_TO_F64,
#endif
#ifdef EXTFLOAT80
    UI64_TO_EXTF80,
#endif
#ifdef FLOAT128
    UI64_TO_F128,
#endif
#ifdef FLOAT16
    I32_TO_F16,
#endif
    I32_TO_F32,
#ifdef FLOAT64
    I32_TO_F64,
#endif
#ifdef EXTFLOAT80
    I32_TO_EXTF80,
#endif
#ifdef FLOAT128
    I32_TO_F128,
#endif
#ifdef FLOAT16
    I64_TO_F16,
#endif
    I64_TO_F32,
#ifdef FLOAT64
    I64_TO_F64,
#endif
#ifdef EXTFLOAT80
    I64_TO_EXTF80,
#endif
#ifdef FLOAT128
    I64_TO_F128,
#endif
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
#ifdef FLOAT16
    F16_TO_UI32,
    F16_TO_UI64,
    F16_TO_I32,
    F16_TO_I64,
    F16_TO_UI32_R_MINMAG,
    F16_TO_UI64_R_MINMAG,
    F16_TO_I32_R_MINMAG,
    F16_TO_I64_R_MINMAG,
    F16_TO_F32,
#ifdef FLOAT64
    F16_TO_F64,
#endif
#ifdef EXTFLOAT80
    F16_TO_EXTF80,
#endif
#ifdef FLOAT128
    F16_TO_F128,
#endif
    F16_ROUNDTOINT,
    F16_ADD,
    F16_SUB,
    F16_MUL,
    F16_MULADD,
    F16_DIV,
    F16_REM,
    F16_SQRT,
    F16_EQ,
    F16_LE,
    F16_LT,
    F16_EQ_SIGNALING,
    F16_LE_QUIET,
    F16_LT_QUIET,
#endif
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    F32_TO_UI32,
    F32_TO_UI64,
    F32_TO_I32,
    F32_TO_I64,
    F32_TO_UI32_R_MINMAG,
    F32_TO_UI64_R_MINMAG,
    F32_TO_I32_R_MINMAG,
    F32_TO_I64_R_MINMAG,
#ifdef FLOAT16
    F32_TO_F16,
#endif
#ifdef FLOAT64
    F32_TO_F64,
#endif
#ifdef EXTFLOAT80
    F32_TO_EXTF80,
#endif
#ifdef FLOAT128
    F32_TO_F128,
#endif
    F32_ROUNDTOINT,
    F32_ADD,
    F32_SUB,
    F32_MUL,
    F32_MULADD,
    F32_DIV,
    F32_REM,
    F32_SQRT,
    F32_EQ,
    F32_LE,
    F32_LT,
    F32_EQ_SIGNALING,
    F32_LE_QUIET,
    F32_LT_QUIET,
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
#ifdef FLOAT64
    F64_TO_UI32,
    F64_TO_UI64,
    F64_TO_I32,
    F64_TO_I64,
    F64_TO_UI32_R_MINMAG,
    F64_TO_UI64_R_MINMAG,
    F64_TO_I32_R_MINMAG,
    F64_TO_I64_R_MINMAG,
#ifdef FLOAT16
    F64_TO_F16,
#endif
    F64_TO_F32,
#ifdef EXTFLOAT80
    F64_TO_EXTF80,
#endif
#ifdef FLOAT128
    F64_TO_F128,
#endif
    F64_ROUNDTOINT,
    F64_ADD,
    F64_SUB,
    F64_MUL,
    F64_MULADD,
    F64_DIV,
    F64_REM,
    F64_SQRT,
    F64_EQ,
    F64_LE,
    F64_LT,
    F64_EQ_SIGNALING,
    F64_LE_QUIET,
    F64_LT_QUIET,
#endif
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
#ifdef EXTFLOAT80
    EXTF80_TO_UI32,
    EXTF80_TO_UI64,
    EXTF80_TO_I32,
    EXTF80_TO_I64,
    EXTF80_TO_UI32_R_MINMAG,
    EXTF80_TO_UI64_R_MINMAG,
    EXTF80_TO_I32_R_MINMAG,
    EXTF80_TO_I64_R_MINMAG,
#ifdef FLOAT16
    EXTF80_TO_F16,
#endif
    EXTF80_TO_F32,
#ifdef FLOAT64
    EXTF80_TO_F64,
#endif
#ifdef FLOAT128
    EXTF80_TO_F128,
#endif
    EXTF80_ROUNDTOINT,
    EXTF80_ADD,
    EXTF80_SUB,
    EXTF80_MUL,
    EXTF80_DIV,
    EXTF80_REM,
    EXTF80_SQRT,
    EXTF80_EQ,
    EXTF80_LE,
    EXTF80_LT,
    EXTF80_EQ_SIGNALING,
    EXTF80_LE_QUIET,
    EXTF80_LT_QUIET,
#endif
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
#ifdef FLOAT128
    F128_TO_UI32,
    F128_TO_UI64,
    F128_TO_I32,
    F128_TO_I64,
    F128_TO_UI32_R_MINMAG,
    F128_TO_UI64_R_MINMAG,
    F128_TO_I32_R_MINMAG,
    F128_TO_I64_R_MINMAG,
#ifdef FLOAT16
    F128_TO_F16,
#endif
    F128_TO_F32,
#ifdef FLOAT64
    F128_TO_F64,
#endif
#ifdef EXTFLOAT80
    F128_TO_EXTF80,
#endif
    F128_ROUNDTOINT,
    F128_ADD,
    F128_SUB,
    F128_MUL,
    F128_MULADD,
    F128_DIV,
    F128_REM,
    F128_SQRT,
    F128_EQ,
    F128_LE,
    F128_LT,
    F128_EQ_SIGNALING,
    F128_LE_QUIET,
    F128_LT_QUIET,
#endif
    NUM_FUNCTIONS
};

enum {
    ROUND_NEAR_EVEN = 1,
    ROUND_MINMAG,
    ROUND_MIN,
    ROUND_MAX,
    ROUND_NEAR_MAXMAG,
#ifdef FLOAT_ROUND_ODD
    ROUND_ODD,
#endif
    NUM_ROUNDINGMODES
};
enum {
    TININESS_BEFORE_ROUNDING = 1,
    TININESS_AFTER_ROUNDING,
    NUM_TININESSMODES
};

extern const uint_fast8_t roundingModes[NUM_ROUNDINGMODES];
extern const uint_fast8_t tininessModes[NUM_TININESSMODES];

enum {
    FUNC_ARG_UNARY                    = 0x01,
    FUNC_ARG_BINARY                   = 0x02,
    FUNC_ARG_ROUNDINGMODE             = 0x04,
    FUNC_ARG_EXACT                    = 0x08,
    FUNC_EFF_ROUNDINGPRECISION        = 0x10,
    FUNC_EFF_ROUNDINGMODE             = 0x20,
    FUNC_EFF_TININESSMODE             = 0x40,
    FUNC_EFF_TININESSMODE_REDUCEDPREC = 0x80
};
struct functionInfo {
    const char *namePtr;
    unsigned char attribs;
};
extern const struct functionInfo functionInfos[NUM_FUNCTIONS];

struct standardFunctionInfo {
    const char *namePtr;
    unsigned char functionCode;
    char roundingCode, exact;
};
extern const struct standardFunctionInfo standardFunctionInfos[];

