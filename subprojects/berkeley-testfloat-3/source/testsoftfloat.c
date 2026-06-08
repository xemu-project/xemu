
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
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include "platform.h"
#include "fail.h"
#include "softfloat.h"
#include "slowfloat.h"
#include "functions.h"
#include "genCases.h"
#include "verCases.h"
#include "writeCase.h"
#include "testLoops.h"

static void catchSIGINT( int signalCode )
{

    if ( verCases_stop ) exit( EXIT_FAILURE );
    verCases_stop = true;

}

static uint_fast8_t softfloat_clearExceptionFlags( void )
{
    uint_fast8_t prevFlags;

    prevFlags = softfloat_exceptionFlags;
    softfloat_exceptionFlags = 0;
    return prevFlags;

}

static
void
 testFunctionInstance(
     int functionCode, uint_fast8_t roundingMode, bool exact )
{
#ifdef FLOAT16
    float16_t (*trueFunction_abz_f16)( float16_t, float16_t );
    float16_t (*subjFunction_abz_f16)( float16_t, float16_t );
    bool (*trueFunction_ab_f16_z_bool)( float16_t, float16_t );
    bool (*subjFunction_ab_f16_z_bool)( float16_t, float16_t );
#endif
    float32_t (*trueFunction_abz_f32)( float32_t, float32_t );
    float32_t (*subjFunction_abz_f32)( float32_t, float32_t );
    bool (*trueFunction_ab_f32_z_bool)( float32_t, float32_t );
    bool (*subjFunction_ab_f32_z_bool)( float32_t, float32_t );
#ifdef FLOAT64
    float64_t (*trueFunction_abz_f64)( float64_t, float64_t );
    float64_t (*subjFunction_abz_f64)( float64_t, float64_t );
    bool (*trueFunction_ab_f64_z_bool)( float64_t, float64_t );
    bool (*subjFunction_ab_f64_z_bool)( float64_t, float64_t );
#endif
#ifdef EXTFLOAT80
    void (*trueFunction_abz_extF80M)(
        const extFloat80_t *, const extFloat80_t *, extFloat80_t * );
    void (*subjFunction_abz_extF80M)(
        const extFloat80_t *, const extFloat80_t *, extFloat80_t * );
    bool (*trueFunction_ab_extF80M_z_bool)(
        const extFloat80_t *, const extFloat80_t * );
    bool (*subjFunction_ab_extF80M_z_bool)(
        const extFloat80_t *, const extFloat80_t * );
#endif
#ifdef FLOAT128
    void (*trueFunction_abz_f128M)(
        const float128_t *, const float128_t *, float128_t * );
    void (*subjFunction_abz_f128M)(
        const float128_t *, const float128_t *, float128_t * );
    bool (*trueFunction_ab_f128M_z_bool)(
        const float128_t *, const float128_t * );
    bool (*subjFunction_ab_f128M_z_bool)(
        const float128_t *, const float128_t * );
#endif

    fputs( "Testing ", stderr );
    verCases_writeFunctionName( stderr );
    fputs( ".\n", stderr );
    switch ( functionCode ) {
        /*--------------------------------------------------------------------
        *--------------------------------------------------------------------*/
#ifdef FLOAT16
     case UI32_TO_F16:
        test_a_ui32_z_f16( slow_ui32_to_f16, ui32_to_f16 );
        break;
#endif
     case UI32_TO_F32:
        test_a_ui32_z_f32( slow_ui32_to_f32, ui32_to_f32 );
        break;
#ifdef FLOAT64
     case UI32_TO_F64:
        test_a_ui32_z_f64( slow_ui32_to_f64, ui32_to_f64 );
        break;
#endif
#ifdef EXTFLOAT80
     case UI32_TO_EXTF80:
        test_a_ui32_z_extF80( slow_ui32_to_extF80M, ui32_to_extF80M );
        break;
#endif
#ifdef FLOAT128
     case UI32_TO_F128:
        test_a_ui32_z_f128( slow_ui32_to_f128M, ui32_to_f128M );
        break;
#endif
#ifdef FLOAT16
     case UI64_TO_F16:
        test_a_ui64_z_f16( slow_ui64_to_f16, ui64_to_f16 );
        break;
#endif
     case UI64_TO_F32:
        test_a_ui64_z_f32( slow_ui64_to_f32, ui64_to_f32 );
        break;
#ifdef FLOAT64
     case UI64_TO_F64:
        test_a_ui64_z_f64( slow_ui64_to_f64, ui64_to_f64 );
        break;
#endif
#ifdef EXTFLOAT80
     case UI64_TO_EXTF80:
        test_a_ui64_z_extF80( slow_ui64_to_extF80M, ui64_to_extF80M );
        break;
#endif
#ifdef FLOAT128
     case UI64_TO_F128:
        test_a_ui64_z_f128( slow_ui64_to_f128M, ui64_to_f128M );
        break;
#endif
#ifdef FLOAT16
     case I32_TO_F16:
        test_a_i32_z_f16( slow_i32_to_f16, i32_to_f16 );
        break;
#endif
     case I32_TO_F32:
        test_a_i32_z_f32( slow_i32_to_f32, i32_to_f32 );
        break;
#ifdef FLOAT64
     case I32_TO_F64:
        test_a_i32_z_f64( slow_i32_to_f64, i32_to_f64 );
        break;
#endif
#ifdef EXTFLOAT80
     case I32_TO_EXTF80:
        test_a_i32_z_extF80( slow_i32_to_extF80M, i32_to_extF80M );
        break;
#endif
#ifdef FLOAT128
     case I32_TO_F128:
        test_a_i32_z_f128( slow_i32_to_f128M, i32_to_f128M );
        break;
#endif
#ifdef FLOAT16
     case I64_TO_F16:
        test_a_i64_z_f16( slow_i64_to_f16, i64_to_f16 );
        break;
#endif
     case I64_TO_F32:
        test_a_i64_z_f32( slow_i64_to_f32, i64_to_f32 );
        break;
#ifdef FLOAT64
     case I64_TO_F64:
        test_a_i64_z_f64( slow_i64_to_f64, i64_to_f64 );
        break;
#endif
#ifdef EXTFLOAT80
     case I64_TO_EXTF80:
        test_a_i64_z_extF80( slow_i64_to_extF80M, i64_to_extF80M );
        break;
#endif
#ifdef FLOAT128
     case I64_TO_F128:
        test_a_i64_z_f128( slow_i64_to_f128M, i64_to_f128M );
        break;
#endif
        /*--------------------------------------------------------------------
        *--------------------------------------------------------------------*/
#ifdef FLOAT16
     case F16_TO_UI32:
        test_a_f16_z_ui32_rx(
            slow_f16_to_ui32, f16_to_ui32, roundingMode, exact );
        break;
     case F16_TO_UI64:
        test_a_f16_z_ui64_rx(
            slow_f16_to_ui64, f16_to_ui64, roundingMode, exact );
        break;
     case F16_TO_I32:
        test_a_f16_z_i32_rx(
            slow_f16_to_i32, f16_to_i32, roundingMode, exact );
        break;
     case F16_TO_I64:
        test_a_f16_z_i64_rx(
            slow_f16_to_i64, f16_to_i64, roundingMode, exact );
        break;
     case F16_TO_UI32_R_MINMAG:
        test_a_f16_z_ui32_x(
            slow_f16_to_ui32_r_minMag, f16_to_ui32_r_minMag, exact );
        break;
     case F16_TO_UI64_R_MINMAG:
        test_a_f16_z_ui64_x(
            slow_f16_to_ui64_r_minMag, f16_to_ui64_r_minMag, exact );
        break;
     case F16_TO_I32_R_MINMAG:
        test_a_f16_z_i32_x(
            slow_f16_to_i32_r_minMag, f16_to_i32_r_minMag, exact );
        break;
     case F16_TO_I64_R_MINMAG:
        test_a_f16_z_i64_x(
            slow_f16_to_i64_r_minMag, f16_to_i64_r_minMag, exact );
        break;
     case F16_TO_F32:
        test_a_f16_z_f32( slow_f16_to_f32, f16_to_f32 );
        break;
#ifdef FLOAT64
     case F16_TO_F64:
        test_a_f16_z_f64( slow_f16_to_f64, f16_to_f64 );
        break;
#endif
#ifdef EXTFLOAT80
     case F16_TO_EXTF80:
        test_a_f16_z_extF80( slow_f16_to_extF80M, f16_to_extF80M );
        break;
#endif
#ifdef FLOAT128
     case F16_TO_F128:
        test_a_f16_z_f128( slow_f16_to_f128M, f16_to_f128M );
        break;
#endif
     case F16_ROUNDTOINT:
        test_az_f16_rx(
            slow_f16_roundToInt, f16_roundToInt, roundingMode, exact );
        break;
     case F16_ADD:
        trueFunction_abz_f16 = slow_f16_add;
        subjFunction_abz_f16 = f16_add;
        goto test_abz_f16;
     case F16_SUB:
        trueFunction_abz_f16 = slow_f16_sub;
        subjFunction_abz_f16 = f16_sub;
        goto test_abz_f16;
     case F16_MUL:
        trueFunction_abz_f16 = slow_f16_mul;
        subjFunction_abz_f16 = f16_mul;
        goto test_abz_f16;
     case F16_DIV:
        trueFunction_abz_f16 = slow_f16_div;
        subjFunction_abz_f16 = f16_div;
        goto test_abz_f16;
     case F16_REM:
        trueFunction_abz_f16 = slow_f16_rem;
        subjFunction_abz_f16 = f16_rem;
     test_abz_f16:
        test_abz_f16( trueFunction_abz_f16, subjFunction_abz_f16 );
        break;
     case F16_MULADD:
        test_abcz_f16( slow_f16_mulAdd, f16_mulAdd );
        break;
     case F16_SQRT:
        test_az_f16( slow_f16_sqrt, f16_sqrt );
        break;
     case F16_EQ:
        trueFunction_ab_f16_z_bool = slow_f16_eq;
        subjFunction_ab_f16_z_bool = f16_eq;
        goto test_ab_f16_z_bool;
     case F16_LE:
        trueFunction_ab_f16_z_bool = slow_f16_le;
        subjFunction_ab_f16_z_bool = f16_le;
        goto test_ab_f16_z_bool;
     case F16_LT:
        trueFunction_ab_f16_z_bool = slow_f16_lt;
        subjFunction_ab_f16_z_bool = f16_lt;
        goto test_ab_f16_z_bool;
     case F16_EQ_SIGNALING:
        trueFunction_ab_f16_z_bool = slow_f16_eq_signaling;
        subjFunction_ab_f16_z_bool = f16_eq_signaling;
        goto test_ab_f16_z_bool;
     case F16_LE_QUIET:
        trueFunction_ab_f16_z_bool = slow_f16_le_quiet;
        subjFunction_ab_f16_z_bool = f16_le_quiet;
        goto test_ab_f16_z_bool;
     case F16_LT_QUIET:
        trueFunction_ab_f16_z_bool = slow_f16_lt_quiet;
        subjFunction_ab_f16_z_bool = f16_lt_quiet;
     test_ab_f16_z_bool:
        test_ab_f16_z_bool(
            trueFunction_ab_f16_z_bool, subjFunction_ab_f16_z_bool );
        break;
#endif
        /*--------------------------------------------------------------------
        *--------------------------------------------------------------------*/
     case F32_TO_UI32:
        test_a_f32_z_ui32_rx(
            slow_f32_to_ui32, f32_to_ui32, roundingMode, exact );
        break;
     case F32_TO_UI64:
        test_a_f32_z_ui64_rx(
            slow_f32_to_ui64, f32_to_ui64, roundingMode, exact );
        break;
     case F32_TO_I32:
        test_a_f32_z_i32_rx(
            slow_f32_to_i32, f32_to_i32, roundingMode, exact );
        break;
     case F32_TO_I64:
        test_a_f32_z_i64_rx(
            slow_f32_to_i64, f32_to_i64, roundingMode, exact );
        break;
     case F32_TO_UI32_R_MINMAG:
        test_a_f32_z_ui32_x(
            slow_f32_to_ui32_r_minMag, f32_to_ui32_r_minMag, exact );
        break;
     case F32_TO_UI64_R_MINMAG:
        test_a_f32_z_ui64_x(
            slow_f32_to_ui64_r_minMag, f32_to_ui64_r_minMag, exact );
        break;
     case F32_TO_I32_R_MINMAG:
        test_a_f32_z_i32_x(
            slow_f32_to_i32_r_minMag, f32_to_i32_r_minMag, exact );
        break;
     case F32_TO_I64_R_MINMAG:
        test_a_f32_z_i64_x(
            slow_f32_to_i64_r_minMag, f32_to_i64_r_minMag, exact );
        break;
#ifdef FLOAT16
     case F32_TO_F16:
        test_a_f32_z_f16( slow_f32_to_f16, f32_to_f16 );
        break;
#endif
#ifdef FLOAT64
     case F32_TO_F64:
        test_a_f32_z_f64( slow_f32_to_f64, f32_to_f64 );
        break;
#endif
#ifdef EXTFLOAT80
     case F32_TO_EXTF80:
        test_a_f32_z_extF80( slow_f32_to_extF80M, f32_to_extF80M );
        break;
#endif
#ifdef FLOAT128
     case F32_TO_F128:
        test_a_f32_z_f128( slow_f32_to_f128M, f32_to_f128M );
        break;
#endif
     case F32_ROUNDTOINT:
        test_az_f32_rx(
            slow_f32_roundToInt, f32_roundToInt, roundingMode, exact );
        break;
     case F32_ADD:
        trueFunction_abz_f32 = slow_f32_add;
        subjFunction_abz_f32 = f32_add;
        goto test_abz_f32;
     case F32_SUB:
        trueFunction_abz_f32 = slow_f32_sub;
        subjFunction_abz_f32 = f32_sub;
        goto test_abz_f32;
     case F32_MUL:
        trueFunction_abz_f32 = slow_f32_mul;
        subjFunction_abz_f32 = f32_mul;
        goto test_abz_f32;
     case F32_DIV:
        trueFunction_abz_f32 = slow_f32_div;
        subjFunction_abz_f32 = f32_div;
        goto test_abz_f32;
     case F32_REM:
        trueFunction_abz_f32 = slow_f32_rem;
        subjFunction_abz_f32 = f32_rem;
     test_abz_f32:
        test_abz_f32( trueFunction_abz_f32, subjFunction_abz_f32 );
        break;
     case F32_MULADD:
        test_abcz_f32( slow_f32_mulAdd, f32_mulAdd );
        break;
     case F32_SQRT:
        test_az_f32( slow_f32_sqrt, f32_sqrt );
        break;
     case F32_EQ:
        trueFunction_ab_f32_z_bool = slow_f32_eq;
        subjFunction_ab_f32_z_bool = f32_eq;
        goto test_ab_f32_z_bool;
     case F32_LE:
        trueFunction_ab_f32_z_bool = slow_f32_le;
        subjFunction_ab_f32_z_bool = f32_le;
        goto test_ab_f32_z_bool;
     case F32_LT:
        trueFunction_ab_f32_z_bool = slow_f32_lt;
        subjFunction_ab_f32_z_bool = f32_lt;
        goto test_ab_f32_z_bool;
     case F32_EQ_SIGNALING:
        trueFunction_ab_f32_z_bool = slow_f32_eq_signaling;
        subjFunction_ab_f32_z_bool = f32_eq_signaling;
        goto test_ab_f32_z_bool;
     case F32_LE_QUIET:
        trueFunction_ab_f32_z_bool = slow_f32_le_quiet;
        subjFunction_ab_f32_z_bool = f32_le_quiet;
        goto test_ab_f32_z_bool;
     case F32_LT_QUIET:
        trueFunction_ab_f32_z_bool = slow_f32_lt_quiet;
        subjFunction_ab_f32_z_bool = f32_lt_quiet;
     test_ab_f32_z_bool:
        test_ab_f32_z_bool(
            trueFunction_ab_f32_z_bool, subjFunction_ab_f32_z_bool );
        break;
        /*--------------------------------------------------------------------
        *--------------------------------------------------------------------*/
#ifdef FLOAT64
     case F64_TO_UI32:
        test_a_f64_z_ui32_rx(
            slow_f64_to_ui32, f64_to_ui32, roundingMode, exact );
        break;
     case F64_TO_UI64:
        test_a_f64_z_ui64_rx(
            slow_f64_to_ui64, f64_to_ui64, roundingMode, exact );
        break;
     case F64_TO_I32:
        test_a_f64_z_i32_rx(
            slow_f64_to_i32, f64_to_i32, roundingMode, exact );
        break;
     case F64_TO_I64:
        test_a_f64_z_i64_rx(
            slow_f64_to_i64, f64_to_i64, roundingMode, exact );
        break;
     case F64_TO_UI32_R_MINMAG:
        test_a_f64_z_ui32_x(
            slow_f64_to_ui32_r_minMag, f64_to_ui32_r_minMag, exact );
        break;
     case F64_TO_UI64_R_MINMAG:
        test_a_f64_z_ui64_x(
            slow_f64_to_ui64_r_minMag, f64_to_ui64_r_minMag, exact );
        break;
     case F64_TO_I32_R_MINMAG:
        test_a_f64_z_i32_x(
            slow_f64_to_i32_r_minMag, f64_to_i32_r_minMag, exact );
        break;
     case F64_TO_I64_R_MINMAG:
        test_a_f64_z_i64_x(
            slow_f64_to_i64_r_minMag, f64_to_i64_r_minMag, exact );
        break;
#ifdef FLOAT16
     case F64_TO_F16:
        test_a_f64_z_f16( slow_f64_to_f16, f64_to_f16 );
        break;
#endif
     case F64_TO_F32:
        test_a_f64_z_f32( slow_f64_to_f32, f64_to_f32 );
        break;
#ifdef EXTFLOAT80
     case F64_TO_EXTF80:
        test_a_f64_z_extF80( slow_f64_to_extF80M, f64_to_extF80M );
        break;
#endif
#ifdef FLOAT128
     case F64_TO_F128:
        test_a_f64_z_f128( slow_f64_to_f128M, f64_to_f128M );
        break;
#endif
     case F64_ROUNDTOINT:
        test_az_f64_rx(
            slow_f64_roundToInt, f64_roundToInt, roundingMode, exact );
        break;
     case F64_ADD:
        trueFunction_abz_f64 = slow_f64_add;
        subjFunction_abz_f64 = f64_add;
        goto test_abz_f64;
     case F64_SUB:
        trueFunction_abz_f64 = slow_f64_sub;
        subjFunction_abz_f64 = f64_sub;
        goto test_abz_f64;
     case F64_MUL:
        trueFunction_abz_f64 = slow_f64_mul;
        subjFunction_abz_f64 = f64_mul;
        goto test_abz_f64;
     case F64_DIV:
        trueFunction_abz_f64 = slow_f64_div;
        subjFunction_abz_f64 = f64_div;
        goto test_abz_f64;
     case F64_REM:
        trueFunction_abz_f64 = slow_f64_rem;
        subjFunction_abz_f64 = f64_rem;
     test_abz_f64:
        test_abz_f64( trueFunction_abz_f64, subjFunction_abz_f64 );
        break;
     case F64_MULADD:
        test_abcz_f64( slow_f64_mulAdd, f64_mulAdd );
        break;
     case F64_SQRT:
        test_az_f64( slow_f64_sqrt, f64_sqrt );
        break;
     case F64_EQ:
        trueFunction_ab_f64_z_bool = slow_f64_eq;
        subjFunction_ab_f64_z_bool = f64_eq;
        goto test_ab_f64_z_bool;
     case F64_LE:
        trueFunction_ab_f64_z_bool = slow_f64_le;
        subjFunction_ab_f64_z_bool = f64_le;
        goto test_ab_f64_z_bool;
     case F64_LT:
        trueFunction_ab_f64_z_bool = slow_f64_lt;
        subjFunction_ab_f64_z_bool = f64_lt;
        goto test_ab_f64_z_bool;
     case F64_EQ_SIGNALING:
        trueFunction_ab_f64_z_bool = slow_f64_eq_signaling;
        subjFunction_ab_f64_z_bool = f64_eq_signaling;
        goto test_ab_f64_z_bool;
     case F64_LE_QUIET:
        trueFunction_ab_f64_z_bool = slow_f64_le_quiet;
        subjFunction_ab_f64_z_bool = f64_le_quiet;
        goto test_ab_f64_z_bool;
     case F64_LT_QUIET:
        trueFunction_ab_f64_z_bool = slow_f64_lt_quiet;
        subjFunction_ab_f64_z_bool = f64_lt_quiet;
     test_ab_f64_z_bool:
        test_ab_f64_z_bool(
            trueFunction_ab_f64_z_bool, subjFunction_ab_f64_z_bool );
        break;
#endif
        /*--------------------------------------------------------------------
        *--------------------------------------------------------------------*/
#ifdef EXTFLOAT80
     case EXTF80_TO_UI32:
        test_a_extF80_z_ui32_rx(
            slow_extF80M_to_ui32, extF80M_to_ui32, roundingMode, exact );
        break;
     case EXTF80_TO_UI64:
        test_a_extF80_z_ui64_rx(
            slow_extF80M_to_ui64, extF80M_to_ui64, roundingMode, exact );
        break;
     case EXTF80_TO_I32:
        test_a_extF80_z_i32_rx(
            slow_extF80M_to_i32, extF80M_to_i32, roundingMode, exact );
        break;
     case EXTF80_TO_I64:
        test_a_extF80_z_i64_rx(
            slow_extF80M_to_i64, extF80M_to_i64, roundingMode, exact );
        break;
     case EXTF80_TO_UI32_R_MINMAG:
        test_a_extF80_z_ui32_x(
            slow_extF80M_to_ui32_r_minMag, extF80M_to_ui32_r_minMag, exact );
        break;
     case EXTF80_TO_UI64_R_MINMAG:
        test_a_extF80_z_ui64_x(
            slow_extF80M_to_ui64_r_minMag, extF80M_to_ui64_r_minMag, exact );
        break;
     case EXTF80_TO_I32_R_MINMAG:
        test_a_extF80_z_i32_x(
            slow_extF80M_to_i32_r_minMag, extF80M_to_i32_r_minMag, exact );
        break;
     case EXTF80_TO_I64_R_MINMAG:
        test_a_extF80_z_i64_x(
            slow_extF80M_to_i64_r_minMag, extF80M_to_i64_r_minMag, exact );
        break;
#ifdef FLOAT16
     case EXTF80_TO_F16:
        test_a_extF80_z_f16( slow_extF80M_to_f16, extF80M_to_f16 );
        break;
#endif
     case EXTF80_TO_F32:
        test_a_extF80_z_f32( slow_extF80M_to_f32, extF80M_to_f32 );
        break;
#ifdef FLOAT64
     case EXTF80_TO_F64:
        test_a_extF80_z_f64( slow_extF80M_to_f64, extF80M_to_f64 );
        break;
#endif
#ifdef FLOAT128
     case EXTF80_TO_F128:
        test_a_extF80_z_f128( slow_extF80M_to_f128M, extF80M_to_f128M );
        break;
#endif
     case EXTF80_ROUNDTOINT:
        test_az_extF80_rx(
            slow_extF80M_roundToInt, extF80M_roundToInt, roundingMode, exact );
        break;
     case EXTF80_ADD:
        trueFunction_abz_extF80M = slow_extF80M_add;
        subjFunction_abz_extF80M = extF80M_add;
        goto test_abz_extF80;
     case EXTF80_SUB:
        trueFunction_abz_extF80M = slow_extF80M_sub;
        subjFunction_abz_extF80M = extF80M_sub;
        goto test_abz_extF80;
     case EXTF80_MUL:
        trueFunction_abz_extF80M = slow_extF80M_mul;
        subjFunction_abz_extF80M = extF80M_mul;
        goto test_abz_extF80;
     case EXTF80_DIV:
        trueFunction_abz_extF80M = slow_extF80M_div;
        subjFunction_abz_extF80M = extF80M_div;
        goto test_abz_extF80;
     case EXTF80_REM:
        trueFunction_abz_extF80M = slow_extF80M_rem;
        subjFunction_abz_extF80M = extF80M_rem;
     test_abz_extF80:
        test_abz_extF80( trueFunction_abz_extF80M, subjFunction_abz_extF80M );
        break;
     case EXTF80_SQRT:
        test_az_extF80( slow_extF80M_sqrt, extF80M_sqrt );
        break;
     case EXTF80_EQ:
        trueFunction_ab_extF80M_z_bool = slow_extF80M_eq;
        subjFunction_ab_extF80M_z_bool = extF80M_eq;
        goto test_ab_extF80_z_bool;
     case EXTF80_LE:
        trueFunction_ab_extF80M_z_bool = slow_extF80M_le;
        subjFunction_ab_extF80M_z_bool = extF80M_le;
        goto test_ab_extF80_z_bool;
     case EXTF80_LT:
        trueFunction_ab_extF80M_z_bool = slow_extF80M_lt;
        subjFunction_ab_extF80M_z_bool = extF80M_lt;
        goto test_ab_extF80_z_bool;
     case EXTF80_EQ_SIGNALING:
        trueFunction_ab_extF80M_z_bool = slow_extF80M_eq_signaling;
        subjFunction_ab_extF80M_z_bool = extF80M_eq_signaling;
        goto test_ab_extF80_z_bool;
     case EXTF80_LE_QUIET:
        trueFunction_ab_extF80M_z_bool = slow_extF80M_le_quiet;
        subjFunction_ab_extF80M_z_bool = extF80M_le_quiet;
        goto test_ab_extF80_z_bool;
     case EXTF80_LT_QUIET:
        trueFunction_ab_extF80M_z_bool = slow_extF80M_lt_quiet;
        subjFunction_ab_extF80M_z_bool = extF80M_lt_quiet;
     test_ab_extF80_z_bool:
        test_ab_extF80_z_bool(
            trueFunction_ab_extF80M_z_bool, subjFunction_ab_extF80M_z_bool );
        break;
#endif
        /*--------------------------------------------------------------------
        *--------------------------------------------------------------------*/
#ifdef FLOAT128
     case F128_TO_UI32:
        test_a_f128_z_ui32_rx(
            slow_f128M_to_ui32, f128M_to_ui32, roundingMode, exact );
        break;
     case F128_TO_UI64:
        test_a_f128_z_ui64_rx(
            slow_f128M_to_ui64, f128M_to_ui64, roundingMode, exact );
        break;
     case F128_TO_I32:
        test_a_f128_z_i32_rx(
            slow_f128M_to_i32, f128M_to_i32, roundingMode, exact );
        break;
     case F128_TO_I64:
        test_a_f128_z_i64_rx(
            slow_f128M_to_i64, f128M_to_i64, roundingMode, exact );
        break;
     case F128_TO_UI32_R_MINMAG:
        test_a_f128_z_ui32_x(
            slow_f128M_to_ui32_r_minMag, f128M_to_ui32_r_minMag, exact );
        break;
     case F128_TO_UI64_R_MINMAG:
        test_a_f128_z_ui64_x(
            slow_f128M_to_ui64_r_minMag, f128M_to_ui64_r_minMag, exact );
        break;
     case F128_TO_I32_R_MINMAG:
        test_a_f128_z_i32_x(
            slow_f128M_to_i32_r_minMag, f128M_to_i32_r_minMag, exact );
        break;
     case F128_TO_I64_R_MINMAG:
        test_a_f128_z_i64_x(
            slow_f128M_to_i64_r_minMag, f128M_to_i64_r_minMag, exact );
        break;
#ifdef FLOAT16
     case F128_TO_F16:
        test_a_f128_z_f16( slow_f128M_to_f16, f128M_to_f16 );
        break;
#endif
     case F128_TO_F32:
        test_a_f128_z_f32( slow_f128M_to_f32, f128M_to_f32 );
        break;
#ifdef FLOAT64
     case F128_TO_F64:
        test_a_f128_z_f64( slow_f128M_to_f64, f128M_to_f64 );
        break;
#endif
#ifdef EXTFLOAT80
     case F128_TO_EXTF80:
        test_a_f128_z_extF80( slow_f128M_to_extF80M, f128M_to_extF80M );
        break;
#endif
     case F128_ROUNDTOINT:
        test_az_f128_rx(
            slow_f128M_roundToInt, f128M_roundToInt, roundingMode, exact );
        break;
     case F128_ADD:
        trueFunction_abz_f128M = slow_f128M_add;
        subjFunction_abz_f128M = f128M_add;
        goto test_abz_f128;
     case F128_SUB:
        trueFunction_abz_f128M = slow_f128M_sub;
        subjFunction_abz_f128M = f128M_sub;
        goto test_abz_f128;
     case F128_MUL:
        trueFunction_abz_f128M = slow_f128M_mul;
        subjFunction_abz_f128M = f128M_mul;
        goto test_abz_f128;
     case F128_DIV:
        trueFunction_abz_f128M = slow_f128M_div;
        subjFunction_abz_f128M = f128M_div;
        goto test_abz_f128;
     case F128_REM:
        trueFunction_abz_f128M = slow_f128M_rem;
        subjFunction_abz_f128M = f128M_rem;
     test_abz_f128:
        test_abz_f128( trueFunction_abz_f128M, subjFunction_abz_f128M );
        break;
     case F128_MULADD:
        test_abcz_f128( slow_f128M_mulAdd, f128M_mulAdd );
        break;
     case F128_SQRT:
        test_az_f128( slow_f128M_sqrt, f128M_sqrt );
        break;
     case F128_EQ:
        trueFunction_ab_f128M_z_bool = slow_f128M_eq;
        subjFunction_ab_f128M_z_bool = f128M_eq;
        goto test_ab_f128_z_bool;
     case F128_LE:
        trueFunction_ab_f128M_z_bool = slow_f128M_le;
        subjFunction_ab_f128M_z_bool = f128M_le;
        goto test_ab_f128_z_bool;
     case F128_LT:
        trueFunction_ab_f128M_z_bool = slow_f128M_lt;
        subjFunction_ab_f128M_z_bool = f128M_lt;
        goto test_ab_f128_z_bool;
     case F128_EQ_SIGNALING:
        trueFunction_ab_f128M_z_bool = slow_f128M_eq_signaling;
        subjFunction_ab_f128M_z_bool = f128M_eq_signaling;
        goto test_ab_f128_z_bool;
     case F128_LE_QUIET:
        trueFunction_ab_f128M_z_bool = slow_f128M_le_quiet;
        subjFunction_ab_f128M_z_bool = f128M_le_quiet;
        goto test_ab_f128_z_bool;
     case F128_LT_QUIET:
        trueFunction_ab_f128M_z_bool = slow_f128M_lt_quiet;
        subjFunction_ab_f128M_z_bool = f128M_lt_quiet;
     test_ab_f128_z_bool:
        test_ab_f128_z_bool(
            trueFunction_ab_f128M_z_bool, subjFunction_ab_f128M_z_bool );
        break;
#endif
    }
    if ( (verCases_errorStop && verCases_anyErrors) || verCases_stop ) {
        verCases_exitWithStatus();
    }

}

enum { EXACT_FALSE = 1, EXACT_TRUE };

static
void
 testFunction(
     int functionCode,
     uint_fast8_t roundingPrecisionIn,
     int roundingCodeIn,
     int tininessCodeIn,
     int exactCodeIn
 )
{
    int functionAttribs;
    uint_fast8_t roundingPrecision;
    int roundingCode;
    uint_fast8_t roundingMode;
    int exactCode;
    bool exact;
    int tininessCode;
    uint_fast8_t tininessMode;

    functionAttribs = functionInfos[functionCode].attribs;
    verCases_functionNamePtr = functionInfos[functionCode].namePtr;
    roundingPrecision = 32;
    for (;;) {
        if ( functionAttribs & FUNC_EFF_ROUNDINGPRECISION ) {
            if ( roundingPrecisionIn ) roundingPrecision = roundingPrecisionIn;
        } else {
            roundingPrecision = 0;
        }
#ifdef EXTFLOAT80
        verCases_roundingPrecision = roundingPrecision;
        if ( roundingPrecision ) {
            slow_extF80_roundingPrecision = roundingPrecision;
            extF80_roundingPrecision = roundingPrecision;
        }
#endif
        for (
            roundingCode = 1; roundingCode < NUM_ROUNDINGMODES; ++roundingCode
        ) {
            if (
                functionAttribs
                    & (FUNC_ARG_ROUNDINGMODE | FUNC_EFF_ROUNDINGMODE)
            ) {
                if ( roundingCodeIn ) roundingCode = roundingCodeIn;
            } else {
                roundingCode = 0;
            }
            verCases_roundingCode = roundingCode;
            if ( roundingCode ) {
                roundingMode = roundingModes[roundingCode];
                if ( functionAttribs & FUNC_EFF_ROUNDINGMODE ) {
                    slowfloat_roundingMode = roundingMode;
                    softfloat_roundingMode = roundingMode;
                }
            }
            for (
                exactCode = EXACT_FALSE; exactCode <= EXACT_TRUE; ++exactCode
            ) {
                if ( functionAttribs & FUNC_ARG_EXACT ) {
                    if ( exactCodeIn ) exactCode = exactCodeIn;
                } else {
                    exactCode = 0;
                }
                exact = (exactCode == EXACT_TRUE);
                verCases_usesExact = (exactCode != 0);
                verCases_exact = exact;
                for (
                    tininessCode = 1;
                    tininessCode < NUM_TININESSMODES;
                    ++tininessCode
                ) {
                    if (
                        (functionAttribs & FUNC_EFF_TININESSMODE)
                            || ((functionAttribs
                                     & FUNC_EFF_TININESSMODE_REDUCEDPREC)
                                    && roundingPrecision
                                    && (roundingPrecision < 80))
                    ) {
                        if ( tininessCodeIn ) tininessCode = tininessCodeIn;
                    } else {
                        tininessCode = 0;
                    }
                    verCases_tininessCode = tininessCode;
                    if ( tininessCode ) {
                        tininessMode = tininessModes[tininessCode];
                        slowfloat_detectTininess = tininessMode;
                        softfloat_detectTininess = tininessMode;
                    }
                    testFunctionInstance( functionCode, roundingMode, exact );
                    if ( tininessCodeIn || ! tininessCode ) break;
                }
                if ( exactCodeIn || ! exactCode ) break;
            }
            if ( roundingCodeIn || ! roundingCode ) break;
        }
        if ( roundingPrecisionIn || ! roundingPrecision ) break;
        if ( roundingPrecision == 80 ) {
            break;
        } else if ( roundingPrecision == 64 ) {
            roundingPrecision = 80;
        } else if ( roundingPrecision == 32 ) {
            roundingPrecision = 64;
        }
    }

}

static uint_fast8_t clearExceptionFlags( void )
{
    uint_fast8_t prevFlags;

    prevFlags = slowfloat_exceptionFlags;
    slowfloat_exceptionFlags = 0;
    return prevFlags;
}

int main( int argc, char *argv[] )
{
    bool haveFunctionArg;
    int functionCode, numOperands;
    uint_fast8_t roundingPrecision;
    int roundingCode, tininessCode, exactCode;
    const char *argPtr;
    unsigned long ui;
    long i;
    int functionMatchAttrib;

    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    fail_programName = "testsoftfloat";
    if ( argc <= 1 ) goto writeHelpMessage;
    genCases_setLevel( 1 );
    verCases_maxErrorCount = 20;
    testLoops_trueFlagsFunction = clearExceptionFlags;
    testLoops_subjFlagsFunction = softfloat_clearExceptionFlags;
    haveFunctionArg = false;
    functionCode = 0;
    numOperands = 0;
    roundingPrecision = 0;
    roundingCode = 0;
    tininessCode = 0;
    exactCode = 0;
    for (;;) {
        --argc;
        if ( ! argc ) break;
        argPtr = *++argv;
        if ( ! argPtr ) break;
        if ( argPtr[0] == '-' ) ++argPtr;
        if (
            ! strcmp( argPtr, "help" ) || ! strcmp( argPtr, "-help" )
                || ! strcmp( argPtr, "h" )
        ) {
 writeHelpMessage:
            fputs(
"testsoftfloat [<option>...] <function>\n"
"  <option>:  (* is default)\n"
"    -help            --Write this message and exit.\n"
"    -seed <num>      --Set pseudo-random number generator seed to <num>.\n"
" *  -seed 1\n"
"    -level <num>     --Testing level <num> (1 or 2).\n"
" *  -level 1\n"
"    -errors <num>    --Stop each function test after <num> errors.\n"
" *  -errors 20\n"
"    -errorstop       --Exit after first function with any error.\n"
"    -forever         --Test one function repeatedly (implies '-level 2').\n"
#ifdef EXTFLOAT80
"    -precision32     --For extF80, test only 32-bit rounding precision.\n"
"    -precision64     --For extF80, test only 64-bit rounding precision.\n"
"    -precision80     --For extF80, test only 80-bit rounding precision.\n"
#endif
"    -rnear_even      --Test only rounding to nearest/even.\n"
"    -rminMag         --Test only rounding to minimum magnitude (toward zero).\n"
"    -rmin            --Test only rounding to minimum (down).\n"
"    -rmax            --Test only rounding to maximum (up).\n"
"    -rnear_maxMag    --Test only rounding to nearest/maximum magnitude\n"
"                         (nearest/away).\n"
#ifdef FLOAT_ROUND_ODD
"    -rodd            --Test only rounding to odd (jamming).  (For rounding to\n"
"                         an integer value, 'minMag' rounding is done instead.)\n"
#endif
"    -tininessbefore  --Test only underflow tininess detected before rounding.\n"
"    -tininessafter   --Test only underflow tininess detected after rounding.\n"
"    -notexact        --Test only non-exact rounding to integer (no inexact\n"
"                         exceptions).\n"
"    -exact           --Test only exact rounding to integer (raising inexact\n"
"                         exceptions).\n"
"  <function>:\n"
"    <int>_to_<float>            <float>_add      <float>_eq\n"
"    <float>_to_<int>            <float>_sub      <float>_le\n"
"    <float>_to_<int>_r_minMag   <float>_mul      <float>_lt\n"
"    <float>_to_<float>          <float>_mulAdd   <float>_eq_signaling\n"
"    <float>_roundToInt          <float>_div      <float>_le_quiet\n"
"                                <float>_rem      <float>_lt_quiet\n"
"                                <float>_sqrt\n"
"    -all1            --All unary functions.\n"
"    -all2            --All binary functions.\n"
"  <int>:\n"
"    ui32             --Unsigned 32-bit integer.\n"
"    ui64             --Unsigned 64-bit integer.\n"
"    i32              --Signed 32-bit integer.\n"
"    i64              --Signed 64-bit integer.\n"
"  <float>:\n"
#ifdef FLOAT16
"    f16              --Binary 16-bit floating-point (half-precision).\n"
#endif
"    f32              --Binary 32-bit floating-point (single-precision).\n"
#ifdef FLOAT64
"    f64              --Binary 64-bit floating-point (double-precision).\n"
#endif
#ifdef EXTFLOAT80
"    extF80           --Binary 80-bit extended floating-point.\n"
#endif
#ifdef FLOAT128
"    f128             --Binary 128-bit floating-point (quadruple-precision).\n"
#endif
                ,
                stdout
            );
            return EXIT_SUCCESS;
        } else if ( ! strcmp( argPtr, "seed" ) ) {
            if ( argc < 2 ) goto optionError;
            ui = strtoul( argv[1], (char **) &argPtr, 10 );
            if ( *argPtr ) goto optionError;
            srand( ui );
            --argc;
            ++argv;
        } else if ( ! strcmp( argPtr, "level" ) ) {
            if ( argc < 2 ) goto optionError;
            i = strtol( argv[1], (char **) &argPtr, 10 );
            if ( *argPtr ) goto optionError;
            genCases_setLevel( i );
            --argc;
            ++argv;
        } else if ( ! strcmp( argPtr, "level1" ) ) {
            genCases_setLevel( 1 );
        } else if ( ! strcmp( argPtr, "level2" ) ) {
            genCases_setLevel( 2 );
        } else if ( ! strcmp( argPtr, "errors" ) ) {
            if ( argc < 2 ) goto optionError;
            i = strtol( argv[1], (char **) &argPtr, 10 );
            if ( *argPtr ) goto optionError;
            verCases_maxErrorCount = i;
            --argc;
            ++argv;
        } else if ( ! strcmp( argPtr, "errorstop" ) ) {
            verCases_errorStop = true;
        } else if ( ! strcmp( argPtr, "forever" ) ) {
            genCases_setLevel( 2 );
            testLoops_forever = true;
#ifdef EXTFLOAT80
        } else if ( ! strcmp( argPtr, "precision32" ) ) {
            roundingPrecision = 32;
        } else if ( ! strcmp( argPtr, "precision64" ) ) {
            roundingPrecision = 64;
        } else if ( ! strcmp( argPtr, "precision80" ) ) {
            roundingPrecision = 80;
#endif
        } else if (
               ! strcmp( argPtr, "rnear_even" )
            || ! strcmp( argPtr, "rneareven" )
            || ! strcmp( argPtr, "rnearest_even" )
        ) {
            roundingCode = ROUND_NEAR_EVEN;
        } else if (
            ! strcmp( argPtr, "rminmag" ) || ! strcmp( argPtr, "rminMag" )
        ) {
            roundingCode = ROUND_MINMAG;
        } else if ( ! strcmp( argPtr, "rmin" ) ) {
            roundingCode = ROUND_MIN;
        } else if ( ! strcmp( argPtr, "rmax" ) ) {
            roundingCode = ROUND_MAX;
        } else if (
               ! strcmp( argPtr, "rnear_maxmag" )
            || ! strcmp( argPtr, "rnear_maxMag" )
            || ! strcmp( argPtr, "rnearmaxmag" )
            || ! strcmp( argPtr, "rnearest_maxmag" )
            || ! strcmp( argPtr, "rnearest_maxMag" )
        ) {
            roundingCode = ROUND_NEAR_MAXMAG;
#ifdef FLOAT_ROUND_ODD
        } else if ( ! strcmp( argPtr, "rodd" ) ) {
            roundingCode = ROUND_ODD;
#endif
        } else if ( ! strcmp( argPtr, "tininessbefore" ) ) {
            tininessCode = TININESS_BEFORE_ROUNDING;
        } else if ( ! strcmp( argPtr, "tininessafter" ) ) {
            tininessCode = TININESS_AFTER_ROUNDING;
        } else if ( ! strcmp( argPtr, "notexact" ) ) {
            exactCode = EXACT_FALSE;
        } else if ( ! strcmp( argPtr, "exact" ) ) {
            exactCode = EXACT_TRUE;
        } else if ( ! strcmp( argPtr, "all1" ) ) {
            haveFunctionArg = true;
            functionCode = 0;
            numOperands = 1;
        } else if ( ! strcmp( argPtr, "all2" ) ) {
            haveFunctionArg = true;
            functionCode = 0;
            numOperands = 2;
        } else {
            functionCode = 1;
            while ( strcmp( argPtr, functionInfos[functionCode].namePtr ) ) {
                ++functionCode;
                if ( functionCode == NUM_FUNCTIONS ) {
                    fail( "Invalid argument '%s'", *argv );
                }
            }
            haveFunctionArg = true;
        }
    }
    if ( ! haveFunctionArg ) fail( "Function argument required" );
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    signal( SIGINT, catchSIGINT );
    signal( SIGTERM, catchSIGINT );
    if ( functionCode ) {
        if ( testLoops_forever ) {
            if ( ! roundingPrecision ) roundingPrecision = 80;
            if ( ! roundingCode ) roundingCode = ROUND_NEAR_EVEN;
        }
        testFunction(
            functionCode,
            roundingPrecision,
            roundingCode,
            tininessCode,
            exactCode
        );
    } else {
        if ( testLoops_forever ) {
             fail( "Can test only one function with '-forever' option" );
        }
        functionMatchAttrib =
            (numOperands == 1) ? FUNC_ARG_UNARY : FUNC_ARG_BINARY;
        for (
            functionCode = 1; functionCode < NUM_FUNCTIONS; ++functionCode
        ) {
            if ( functionInfos[functionCode].attribs & functionMatchAttrib ) {
                testFunction(
                    functionCode,
                    roundingPrecision,
                    roundingCode,
                    tininessCode,
                    exactCode
                );
            }
        }
    }
    verCases_exitWithStatus();
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
 optionError:
    fail( "'%s' option requires numeric argument", *argv );

}

