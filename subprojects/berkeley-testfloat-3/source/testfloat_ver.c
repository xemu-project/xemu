
/*============================================================================

This C source file is part of TestFloat, Release 3e, a package of programs for
testing the correctness of floating-point arithmetic complying with the IEEE
Standard for Floating-Point, by John R. Hauser.

Copyright 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018 The Regents of the
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
#include "functions.h"
#include "verCases.h"
#include "writeCase.h"
#include "verLoops.h"

static void catchSIGINT( int signalCode )
{

    if ( verCases_stop ) exit( EXIT_FAILURE );
    verCases_stop = true;

}

int main( int argc, char *argv[] )
{
    bool exact;
    int functionCode, roundingCode, tininessCode;
    const char *argPtr;
    long i;
    int functionAttribs;
    uint_fast8_t roundingMode;
#ifdef FLOAT16
    float16_t (*trueFunction_abz_f16)( float16_t, float16_t );
    bool (*trueFunction_ab_f16_z_bool)( float16_t, float16_t );
#endif
    float32_t (*trueFunction_abz_f32)( float32_t, float32_t );
    bool (*trueFunction_ab_f32_z_bool)( float32_t, float32_t );
#ifdef FLOAT64
    float64_t (*trueFunction_abz_f64)( float64_t, float64_t );
    bool (*trueFunction_ab_f64_z_bool)( float64_t, float64_t );
#endif
#ifdef EXTFLOAT80
    void
     (*trueFunction_abz_extF80)(
         const extFloat80_t *, const extFloat80_t *, extFloat80_t * );
    bool
     (*trueFunction_ab_extF80_z_bool)(
         const extFloat80_t *, const extFloat80_t * );
#endif
#ifdef FLOAT128
    void
     (*trueFunction_abz_f128)(
         const float128_t *, const float128_t *, float128_t * );
    bool
     (*trueFunction_ab_f128_z_bool)( const float128_t *, const float128_t * );
#endif

    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    fail_programName = "testfloat_ver";
    if ( argc <= 1 ) goto writeHelpMessage;
    softfloat_detectTininess = softfloat_tininess_afterRounding;
#ifdef EXTFLOAT80
    extF80_roundingPrecision = 80;
#endif
    roundingMode = softfloat_round_near_even;
    exact = false;
    verCases_maxErrorCount = 20;
    verLoops_trueFlagsPtr = &softfloat_exceptionFlags;
    functionCode = 0;
    roundingCode = ROUND_NEAR_EVEN;
    tininessCode = TININESS_AFTER_ROUNDING;
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
"testfloat_ver [<option>...] <function>\n"
"  <option>:  (* is default)\n"
"    -help            --Write this message and exit.\n"
"    -errors <num>    --Stop after <num> errors.\n"
" *  -errors 20\n"
"    -checkNaNs       --Check for specific NaN results.\n"
"    -checkInvInts    --Check for specific invalid integer results.\n"
"    -checkAll        --Same as both '-checkNaNs' and '-checkInvInts'.\n"
#ifdef EXTFLOAT80
"    -precision32     --For extF80, rounding precision is 32 bits.\n"
"    -precision64     --For extF80, rounding precision is 64 bits.\n"
" *  -precision80     --For extF80, rounding precision is 80 bits.\n"
#endif
" *  -rnear_even      --Round to nearest/even.\n"
"    -rminMag         --Round to minimum magnitude (toward zero).\n"
"    -rmin            --Round to minimum (down).\n"
"    -rmax            --Round to maximum (up).\n"
"    -rnear_maxMag    --Round to nearest/maximum magnitude (nearest/away).\n"
#ifdef FLOAT_ROUND_ODD
"    -rodd            --Round to odd (jamming).  (For rounding to an integer\n"
"                         value, rounds to minimum magnitude instead.)\n"
#endif
"    -tininessbefore  --Detect underflow tininess before rounding.\n"
" *  -tininessafter   --Detect underflow tininess after rounding.\n"
" *  -notexact        --Rounding to integer is not exact (no inexact\n"
"                         exceptions).\n"
"    -exact           --Rounding to integer is exact (raising inexact\n"
"                         exceptions).\n"
"  <function>:\n"
"    <int>_to_<float>     <float>_add      <float>_eq\n"
"    <float>_to_<int>     <float>_sub      <float>_le\n"
"    <float>_to_<float>   <float>_mul      <float>_lt\n"
"    <float>_roundToInt   <float>_mulAdd   <float>_eq_signaling\n"
"                         <float>_div      <float>_le_quiet\n"
"                         <float>_rem      <float>_lt_quiet\n"
"                         <float>_sqrt\n"
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
        } else if ( ! strcmp( argPtr, "errors" ) ) {
            if ( argc < 2 ) goto optionError;
            i = strtol( argv[1], (char **) &argPtr, 10 );
            if ( *argPtr ) goto optionError;
            verCases_maxErrorCount = i;
            --argc;
            ++argv;
        } else if (
            ! strcmp( argPtr, "checkNaNs" ) || ! strcmp( argPtr, "checknans" )
        ) {
            verCases_checkNaNs = true;
        } else if (
               ! strcmp( argPtr, "checkInvInts" )
            || ! strcmp( argPtr, "checkinvints" )
        ) {
            verCases_checkInvInts = true;
        } else if (
            ! strcmp( argPtr, "checkAll" ) || ! strcmp( argPtr, "checkall" )
        ) {
            verCases_checkNaNs = true;
            verCases_checkInvInts = true;
#ifdef EXTFLOAT80
        } else if ( ! strcmp( argPtr, "precision32" ) ) {
            extF80_roundingPrecision = 32;
        } else if ( ! strcmp( argPtr, "precision64" ) ) {
            extF80_roundingPrecision = 64;
        } else if ( ! strcmp( argPtr, "precision80" ) ) {
            extF80_roundingPrecision = 80;
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
            exact = false;
        } else if ( ! strcmp( argPtr, "exact" ) ) {
            exact = true;
        } else {
            functionCode = 1;
            while ( strcmp( argPtr, functionInfos[functionCode].namePtr ) ) {
                ++functionCode;
                if ( functionCode == NUM_FUNCTIONS ) goto invalidArg;
            }
            functionAttribs = functionInfos[functionCode].attribs;
            if (
                (functionAttribs & FUNC_ARG_EXACT)
                    && ! (functionAttribs & FUNC_ARG_ROUNDINGMODE)
            ) {
                goto invalidArg;
            }
        }
    }
    if ( ! functionCode ) fail( "Function argument required" );
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    verCases_functionNamePtr = functionInfos[functionCode].namePtr;
#ifdef EXTFLOAT80
    verCases_roundingPrecision =
        functionAttribs & FUNC_EFF_ROUNDINGPRECISION ? extF80_roundingPrecision
            : 0;
#endif
    verCases_roundingCode =
        functionAttribs & (FUNC_ARG_ROUNDINGMODE | FUNC_EFF_ROUNDINGMODE)
            ? roundingCode
            : 0;
#ifdef EXTFLOAT80
    verCases_tininessCode =
        (functionAttribs & FUNC_EFF_TININESSMODE)
            || ((functionAttribs & FUNC_EFF_TININESSMODE_REDUCEDPREC)
                    && extF80_roundingPrecision
                    && (extF80_roundingPrecision < 80))
            ? tininessCode
            : 0;
#else
    verCases_tininessCode =
        functionAttribs & FUNC_EFF_TININESSMODE ? tininessCode : 0;
#endif
    verCases_usesExact = ((functionAttribs & FUNC_ARG_EXACT) != 0);
    verCases_exact = exact;
    roundingMode = roundingModes[roundingCode];
    softfloat_roundingMode = roundingMode;
    softfloat_detectTininess = tininessModes[tininessCode];
    signal( SIGINT, catchSIGINT );
    signal( SIGTERM, catchSIGINT );
    fputs( "Testing ", stderr );
    verCases_writeFunctionName( stderr );
    fputs( ".\n", stderr );
    switch ( functionCode ) {
        /*--------------------------------------------------------------------
        *--------------------------------------------------------------------*/
#ifdef FLOAT16
     case UI32_TO_F16:
        ver_a_ui32_z_f16( ui32_to_f16 );
        break;
#endif
     case UI32_TO_F32:
        ver_a_ui32_z_f32( ui32_to_f32 );
        break;
#ifdef FLOAT64
     case UI32_TO_F64:
        ver_a_ui32_z_f64( ui32_to_f64 );
        break;
#endif
#ifdef EXTFLOAT80
     case UI32_TO_EXTF80:
        ver_a_ui32_z_extF80( ui32_to_extF80M );
        break;
#endif
#ifdef FLOAT128
     case UI32_TO_F128:
        ver_a_ui32_z_f128( ui32_to_f128M );
        break;
#endif
#ifdef FLOAT16
     case UI64_TO_F16:
        ver_a_ui64_z_f16( ui64_to_f16 );
        break;
#endif
     case UI64_TO_F32:
        ver_a_ui64_z_f32( ui64_to_f32 );
        break;
#ifdef FLOAT64
     case UI64_TO_F64:
        ver_a_ui64_z_f64( ui64_to_f64 );
        break;
#endif
#ifdef EXTFLOAT80
     case UI64_TO_EXTF80:
        ver_a_ui64_z_extF80( ui64_to_extF80M );
        break;
#endif
#ifdef FLOAT128
     case UI64_TO_F128:
        ver_a_ui64_z_f128( ui64_to_f128M );
        break;
#endif
#ifdef FLOAT16
     case I32_TO_F16:
        ver_a_i32_z_f16( i32_to_f16 );
        break;
#endif
     case I32_TO_F32:
        ver_a_i32_z_f32( i32_to_f32 );
        break;
#ifdef FLOAT64
     case I32_TO_F64:
        ver_a_i32_z_f64( i32_to_f64 );
        break;
#endif
#ifdef EXTFLOAT80
     case I32_TO_EXTF80:
        ver_a_i32_z_extF80( i32_to_extF80M );
        break;
#endif
#ifdef FLOAT128
     case I32_TO_F128:
        ver_a_i32_z_f128( i32_to_f128M );
        break;
#endif
#ifdef FLOAT16
     case I64_TO_F16:
        ver_a_i64_z_f16( i64_to_f16 );
        break;
#endif
     case I64_TO_F32:
        ver_a_i64_z_f32( i64_to_f32 );
        break;
#ifdef FLOAT64
     case I64_TO_F64:
        ver_a_i64_z_f64( i64_to_f64 );
        break;
#endif
#ifdef EXTFLOAT80
     case I64_TO_EXTF80:
        ver_a_i64_z_extF80( i64_to_extF80M );
        break;
#endif
#ifdef FLOAT128
     case I64_TO_F128:
        ver_a_i64_z_f128( i64_to_f128M );
        break;
#endif
        /*--------------------------------------------------------------------
        *--------------------------------------------------------------------*/
#ifdef FLOAT16
     case F16_TO_UI32:
        ver_a_f16_z_ui32_rx( f16_to_ui32, roundingMode, exact );
        break;
     case F16_TO_UI64:
        ver_a_f16_z_ui64_rx( f16_to_ui64, roundingMode, exact );
        break;
     case F16_TO_I32:
        ver_a_f16_z_i32_rx( f16_to_i32, roundingMode, exact );
        break;
     case F16_TO_I64:
        ver_a_f16_z_i64_rx( f16_to_i64, roundingMode, exact );
        break;
     case F16_TO_F32:
        ver_a_f16_z_f32( f16_to_f32 );
        break;
#ifdef FLOAT64
     case F16_TO_F64:
        ver_a_f16_z_f64( f16_to_f64 );
        break;
#endif
#ifdef EXTFLOAT80
     case F16_TO_EXTF80:
        ver_a_f16_z_extF80( f16_to_extF80M );
        break;
#endif
#ifdef FLOAT128
     case F16_TO_F128:
        ver_a_f16_z_f128( f16_to_f128M );
        break;
#endif
     case F16_ROUNDTOINT:
        ver_az_f16_rx( f16_roundToInt, roundingMode, exact );
        break;
     case F16_ADD:
        trueFunction_abz_f16 = f16_add;
        goto ver_abz_f16;
     case F16_SUB:
        trueFunction_abz_f16 = f16_sub;
        goto ver_abz_f16;
     case F16_MUL:
        trueFunction_abz_f16 = f16_mul;
        goto ver_abz_f16;
     case F16_DIV:
        trueFunction_abz_f16 = f16_div;
        goto ver_abz_f16;
     case F16_REM:
        trueFunction_abz_f16 = f16_rem;
     ver_abz_f16:
        ver_abz_f16( trueFunction_abz_f16 );
        break;
     case F16_MULADD:
        ver_abcz_f16( f16_mulAdd );
        break;
     case F16_SQRT:
        ver_az_f16( f16_sqrt );
        break;
     case F16_EQ:
        trueFunction_ab_f16_z_bool = f16_eq;
        goto ver_ab_f16_z_bool;
     case F16_LE:
        trueFunction_ab_f16_z_bool = f16_le;
        goto ver_ab_f16_z_bool;
     case F16_LT:
        trueFunction_ab_f16_z_bool = f16_lt;
        goto ver_ab_f16_z_bool;
     case F16_EQ_SIGNALING:
        trueFunction_ab_f16_z_bool = f16_eq_signaling;
        goto ver_ab_f16_z_bool;
     case F16_LE_QUIET:
        trueFunction_ab_f16_z_bool = f16_le_quiet;
        goto ver_ab_f16_z_bool;
     case F16_LT_QUIET:
        trueFunction_ab_f16_z_bool = f16_lt_quiet;
     ver_ab_f16_z_bool:
        ver_ab_f16_z_bool( trueFunction_ab_f16_z_bool );
        break;
#endif
        /*--------------------------------------------------------------------
        *--------------------------------------------------------------------*/
     case F32_TO_UI32:
        ver_a_f32_z_ui32_rx( f32_to_ui32, roundingMode, exact );
        break;
     case F32_TO_UI64:
        ver_a_f32_z_ui64_rx( f32_to_ui64, roundingMode, exact );
        break;
     case F32_TO_I32:
        ver_a_f32_z_i32_rx( f32_to_i32, roundingMode, exact );
        break;
     case F32_TO_I64:
        ver_a_f32_z_i64_rx( f32_to_i64, roundingMode, exact );
        break;
#ifdef FLOAT16
     case F32_TO_F16:
        ver_a_f32_z_f16( f32_to_f16 );
        break;
#endif
#ifdef FLOAT64
     case F32_TO_F64:
        ver_a_f32_z_f64( f32_to_f64 );
        break;
#endif
#ifdef EXTFLOAT80
     case F32_TO_EXTF80:
        ver_a_f32_z_extF80( f32_to_extF80M );
        break;
#endif
#ifdef FLOAT128
     case F32_TO_F128:
        ver_a_f32_z_f128( f32_to_f128M );
        break;
#endif
     case F32_ROUNDTOINT:
        ver_az_f32_rx( f32_roundToInt, roundingMode, exact );
        break;
     case F32_ADD:
        trueFunction_abz_f32 = f32_add;
        goto ver_abz_f32;
     case F32_SUB:
        trueFunction_abz_f32 = f32_sub;
        goto ver_abz_f32;
     case F32_MUL:
        trueFunction_abz_f32 = f32_mul;
        goto ver_abz_f32;
     case F32_DIV:
        trueFunction_abz_f32 = f32_div;
        goto ver_abz_f32;
     case F32_REM:
        trueFunction_abz_f32 = f32_rem;
     ver_abz_f32:
        ver_abz_f32( trueFunction_abz_f32 );
        break;
     case F32_MULADD:
        ver_abcz_f32( f32_mulAdd );
        break;
     case F32_SQRT:
        ver_az_f32( f32_sqrt );
        break;
     case F32_EQ:
        trueFunction_ab_f32_z_bool = f32_eq;
        goto ver_ab_f32_z_bool;
     case F32_LE:
        trueFunction_ab_f32_z_bool = f32_le;
        goto ver_ab_f32_z_bool;
     case F32_LT:
        trueFunction_ab_f32_z_bool = f32_lt;
        goto ver_ab_f32_z_bool;
     case F32_EQ_SIGNALING:
        trueFunction_ab_f32_z_bool = f32_eq_signaling;
        goto ver_ab_f32_z_bool;
     case F32_LE_QUIET:
        trueFunction_ab_f32_z_bool = f32_le_quiet;
        goto ver_ab_f32_z_bool;
     case F32_LT_QUIET:
        trueFunction_ab_f32_z_bool = f32_lt_quiet;
     ver_ab_f32_z_bool:
        ver_ab_f32_z_bool( trueFunction_ab_f32_z_bool );
        break;
        /*--------------------------------------------------------------------
        *--------------------------------------------------------------------*/
#ifdef FLOAT64
     case F64_TO_UI32:
        ver_a_f64_z_ui32_rx( f64_to_ui32, roundingMode, exact );
        break;
     case F64_TO_UI64:
        ver_a_f64_z_ui64_rx( f64_to_ui64, roundingMode, exact );
        break;
     case F64_TO_I32:
        ver_a_f64_z_i32_rx( f64_to_i32, roundingMode, exact );
        break;
     case F64_TO_I64:
        ver_a_f64_z_i64_rx( f64_to_i64, roundingMode, exact );
        break;
#ifdef FLOAT16
     case F64_TO_F16:
        ver_a_f64_z_f16( f64_to_f16 );
        break;
#endif
     case F64_TO_F32:
        ver_a_f64_z_f32( f64_to_f32 );
        break;
#ifdef EXTFLOAT80
     case F64_TO_EXTF80:
        ver_a_f64_z_extF80( f64_to_extF80M );
        break;
#endif
#ifdef FLOAT128
     case F64_TO_F128:
        ver_a_f64_z_f128( f64_to_f128M );
        break;
#endif
     case F64_ROUNDTOINT:
        ver_az_f64_rx( f64_roundToInt, roundingMode, exact );
        break;
     case F64_ADD:
        trueFunction_abz_f64 = f64_add;
        goto ver_abz_f64;
     case F64_SUB:
        trueFunction_abz_f64 = f64_sub;
        goto ver_abz_f64;
     case F64_MUL:
        trueFunction_abz_f64 = f64_mul;
        goto ver_abz_f64;
     case F64_DIV:
        trueFunction_abz_f64 = f64_div;
        goto ver_abz_f64;
     case F64_REM:
        trueFunction_abz_f64 = f64_rem;
     ver_abz_f64:
        ver_abz_f64( trueFunction_abz_f64 );
        break;
     case F64_MULADD:
        ver_abcz_f64( f64_mulAdd );
        break;
     case F64_SQRT:
        ver_az_f64( f64_sqrt );
        break;
     case F64_EQ:
        trueFunction_ab_f64_z_bool = f64_eq;
        goto ver_ab_f64_z_bool;
     case F64_LE:
        trueFunction_ab_f64_z_bool = f64_le;
        goto ver_ab_f64_z_bool;
     case F64_LT:
        trueFunction_ab_f64_z_bool = f64_lt;
        goto ver_ab_f64_z_bool;
     case F64_EQ_SIGNALING:
        trueFunction_ab_f64_z_bool = f64_eq_signaling;
        goto ver_ab_f64_z_bool;
     case F64_LE_QUIET:
        trueFunction_ab_f64_z_bool = f64_le_quiet;
        goto ver_ab_f64_z_bool;
     case F64_LT_QUIET:
        trueFunction_ab_f64_z_bool = f64_lt_quiet;
     ver_ab_f64_z_bool:
        ver_ab_f64_z_bool( trueFunction_ab_f64_z_bool );
        break;
#endif
        /*--------------------------------------------------------------------
        *--------------------------------------------------------------------*/
#ifdef EXTFLOAT80
     case EXTF80_TO_UI32:
        ver_a_extF80_z_ui32_rx( extF80M_to_ui32, roundingMode, exact );
        break;
     case EXTF80_TO_UI64:
        ver_a_extF80_z_ui64_rx( extF80M_to_ui64, roundingMode, exact );
        break;
     case EXTF80_TO_I32:
        ver_a_extF80_z_i32_rx( extF80M_to_i32, roundingMode, exact );
        break;
     case EXTF80_TO_I64:
        ver_a_extF80_z_i64_rx( extF80M_to_i64, roundingMode, exact );
        break;
#ifdef FLOAT16
     case EXTF80_TO_F16:
        ver_a_extF80_z_f16( extF80M_to_f16 );
        break;
#endif
     case EXTF80_TO_F32:
        ver_a_extF80_z_f32( extF80M_to_f32 );
        break;
#ifdef FLOAT64
     case EXTF80_TO_F64:
        ver_a_extF80_z_f64( extF80M_to_f64 );
        break;
#endif
#ifdef FLOAT128
     case EXTF80_TO_F128:
        ver_a_extF80_z_f128( extF80M_to_f128M );
        break;
#endif
     case EXTF80_ROUNDTOINT:
        ver_az_extF80_rx( extF80M_roundToInt, roundingMode, exact );
        break;
     case EXTF80_ADD:
        trueFunction_abz_extF80 = extF80M_add;
        goto ver_abz_extF80;
     case EXTF80_SUB:
        trueFunction_abz_extF80 = extF80M_sub;
        goto ver_abz_extF80;
     case EXTF80_MUL:
        trueFunction_abz_extF80 = extF80M_mul;
        goto ver_abz_extF80;
     case EXTF80_DIV:
        trueFunction_abz_extF80 = extF80M_div;
        goto ver_abz_extF80;
     case EXTF80_REM:
        trueFunction_abz_extF80 = extF80M_rem;
     ver_abz_extF80:
        ver_abz_extF80( trueFunction_abz_extF80 );
        break;
     case EXTF80_SQRT:
        ver_az_extF80( extF80M_sqrt );
        break;
     case EXTF80_EQ:
        trueFunction_ab_extF80_z_bool = extF80M_eq;
        goto ver_ab_extF80_z_bool;
     case EXTF80_LE:
        trueFunction_ab_extF80_z_bool = extF80M_le;
        goto ver_ab_extF80_z_bool;
     case EXTF80_LT:
        trueFunction_ab_extF80_z_bool = extF80M_lt;
        goto ver_ab_extF80_z_bool;
     case EXTF80_EQ_SIGNALING:
        trueFunction_ab_extF80_z_bool = extF80M_eq_signaling;
        goto ver_ab_extF80_z_bool;
     case EXTF80_LE_QUIET:
        trueFunction_ab_extF80_z_bool = extF80M_le_quiet;
        goto ver_ab_extF80_z_bool;
     case EXTF80_LT_QUIET:
        trueFunction_ab_extF80_z_bool = extF80M_lt_quiet;
     ver_ab_extF80_z_bool:
        ver_ab_extF80_z_bool( trueFunction_ab_extF80_z_bool );
        break;
#endif
        /*--------------------------------------------------------------------
        *--------------------------------------------------------------------*/
#ifdef FLOAT128
     case F128_TO_UI32:
        ver_a_f128_z_ui32_rx( f128M_to_ui32, roundingMode, exact );
        break;
     case F128_TO_UI64:
        ver_a_f128_z_ui64_rx( f128M_to_ui64, roundingMode, exact );
        break;
     case F128_TO_I32:
        ver_a_f128_z_i32_rx( f128M_to_i32, roundingMode, exact );
        break;
     case F128_TO_I64:
        ver_a_f128_z_i64_rx( f128M_to_i64, roundingMode, exact );
        break;
#ifdef FLOAT16
     case F128_TO_F16:
        ver_a_f128_z_f16( f128M_to_f16 );
        break;
#endif
     case F128_TO_F32:
        ver_a_f128_z_f32( f128M_to_f32 );
        break;
#ifdef FLOAT64
     case F128_TO_F64:
        ver_a_f128_z_f64( f128M_to_f64 );
        break;
#endif
#ifdef EXTFLOAT80
     case F128_TO_EXTF80:
        ver_a_f128_z_extF80( f128M_to_extF80M );
        break;
#endif
     case F128_ROUNDTOINT:
        ver_az_f128_rx( f128M_roundToInt, roundingMode, exact );
        break;
     case F128_ADD:
        trueFunction_abz_f128 = f128M_add;
        goto ver_abz_f128;
     case F128_SUB:
        trueFunction_abz_f128 = f128M_sub;
        goto ver_abz_f128;
     case F128_MUL:
        trueFunction_abz_f128 = f128M_mul;
        goto ver_abz_f128;
     case F128_DIV:
        trueFunction_abz_f128 = f128M_div;
        goto ver_abz_f128;
     case F128_REM:
        trueFunction_abz_f128 = f128M_rem;
     ver_abz_f128:
        ver_abz_f128( trueFunction_abz_f128 );
        break;
     case F128_MULADD:
        ver_abcz_f128( f128M_mulAdd );
        break;
     case F128_SQRT:
        ver_az_f128( f128M_sqrt );
        break;
     case F128_EQ:
        trueFunction_ab_f128_z_bool = f128M_eq;
        goto ver_ab_f128_z_bool;
     case F128_LE:
        trueFunction_ab_f128_z_bool = f128M_le;
        goto ver_ab_f128_z_bool;
     case F128_LT:
        trueFunction_ab_f128_z_bool = f128M_lt;
        goto ver_ab_f128_z_bool;
     case F128_EQ_SIGNALING:
        trueFunction_ab_f128_z_bool = f128M_eq_signaling;
        goto ver_ab_f128_z_bool;
     case F128_LE_QUIET:
        trueFunction_ab_f128_z_bool = f128M_le_quiet;
        goto ver_ab_f128_z_bool;
     case F128_LT_QUIET:
        trueFunction_ab_f128_z_bool = f128M_lt_quiet;
     ver_ab_f128_z_bool:
        ver_ab_f128_z_bool( trueFunction_ab_f128_z_bool );
        break;
#endif
    }
    verCases_exitWithStatus();
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
 optionError:
    fail( "'%s' option requires numeric argument", *argv );
 invalidArg:
    fail( "Invalid argument '%s'", *argv );

}

