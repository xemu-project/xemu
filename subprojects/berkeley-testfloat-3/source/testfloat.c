
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
#include "subjfloat_config.h"
#include "subjfloat.h"
#include "functions.h"
#include "genCases.h"
#include "verCases.h"
#include "testLoops.h"

static void catchSIGINT( int signalCode )
{

    if ( verCases_stop ) exit( EXIT_FAILURE );
    verCases_stop = true;

}

static void (*subjFunctionPtr)();

#ifdef FLOAT16
typedef float16_t funcType_a_ui32_z_f16( uint32_t );
#endif
typedef float32_t funcType_a_ui32_z_f32( uint32_t );
#ifdef FLOAT64
typedef float64_t funcType_a_ui32_z_f64( uint32_t );
#endif
#ifdef EXTFLOAT80
typedef void funcType_a_ui32_z_extF80( uint32_t, extFloat80_t * );
#endif
#ifdef FLOAT128
typedef void funcType_a_ui32_z_f128( uint32_t, float128_t * );
#endif
#ifdef FLOAT16
typedef float16_t funcType_a_ui64_z_f16( uint64_t );
#endif
typedef float32_t funcType_a_ui64_z_f32( uint64_t );
#ifdef FLOAT64
typedef float64_t funcType_a_ui64_z_f64( uint64_t );
#endif
#ifdef EXTFLOAT80
typedef void funcType_a_ui64_z_extF80( uint64_t, extFloat80_t * );
#endif
#ifdef FLOAT128
typedef void funcType_a_ui64_z_f128( uint64_t, float128_t * );
#endif
#ifdef FLOAT16
typedef float16_t funcType_a_i32_z_f16( int32_t );
#endif
typedef float32_t funcType_a_i32_z_f32( int32_t );
#ifdef FLOAT64
typedef float64_t funcType_a_i32_z_f64( int32_t );
#endif
#ifdef EXTFLOAT80
typedef void funcType_a_i32_z_extF80( int32_t, extFloat80_t * );
#endif
#ifdef FLOAT128
typedef void funcType_a_i32_z_f128( int32_t, float128_t * );
#endif
#ifdef FLOAT16
typedef float16_t funcType_a_i64_z_f16( int64_t );
#endif
typedef float32_t funcType_a_i64_z_f32( int64_t );
#ifdef FLOAT64
typedef float64_t funcType_a_i64_z_f64( int64_t );
#endif
#ifdef EXTFLOAT80
typedef void funcType_a_i64_z_extF80( int64_t, extFloat80_t * );
#endif
#ifdef FLOAT128
typedef void funcType_a_i64_z_f128( int64_t, float128_t * );
#endif

#ifdef FLOAT16
typedef uint_fast32_t funcType_a_f16_z_ui32( float16_t );
typedef uint_fast64_t funcType_a_f16_z_ui64( float16_t );
typedef int_fast32_t funcType_a_f16_z_i32( float16_t );
typedef int_fast64_t funcType_a_f16_z_i64( float16_t );
typedef float32_t funcType_a_f16_z_f32( float16_t );
#ifdef FLOAT64
typedef float64_t funcType_a_f16_z_f64( float16_t );
#endif
#ifdef EXTFLOAT80
typedef void funcType_a_f16_z_extF80( float16_t, extFloat80_t * );
#endif
#ifdef FLOAT128
typedef void funcType_a_f16_z_f128( float16_t, float128_t * );
#endif
typedef float16_t funcType_az_f16( float16_t );
typedef float16_t funcType_abz_f16( float16_t, float16_t );
typedef float16_t funcType_abcz_f16( float16_t, float16_t, float16_t );
typedef bool funcType_ab_f16_z_bool( float16_t, float16_t );
#endif

typedef uint_fast32_t funcType_a_f32_z_ui32( float32_t );
typedef uint_fast64_t funcType_a_f32_z_ui64( float32_t );
typedef int_fast32_t funcType_a_f32_z_i32( float32_t );
typedef int_fast64_t funcType_a_f32_z_i64( float32_t );
#ifdef FLOAT16
typedef float16_t funcType_a_f32_z_f16( float32_t );
#endif
#ifdef FLOAT64
typedef float64_t funcType_a_f32_z_f64( float32_t );
#endif
#ifdef EXTFLOAT80
typedef void funcType_a_f32_z_extF80( float32_t, extFloat80_t * );
#endif
#ifdef FLOAT128
typedef void funcType_a_f32_z_f128( float32_t, float128_t * );
#endif
typedef float32_t funcType_az_f32( float32_t );
typedef float32_t funcType_abz_f32( float32_t, float32_t );
typedef float32_t funcType_abcz_f32( float32_t, float32_t, float32_t );
typedef bool funcType_ab_f32_z_bool( float32_t, float32_t );

#ifdef FLOAT64
typedef uint_fast32_t funcType_a_f64_z_ui32( float64_t );
typedef uint_fast64_t funcType_a_f64_z_ui64( float64_t );
typedef int_fast32_t funcType_a_f64_z_i32( float64_t );
typedef int_fast64_t funcType_a_f64_z_i64( float64_t );
#ifdef FLOAT16
typedef float16_t funcType_a_f64_z_f16( float64_t );
#endif
typedef float32_t funcType_a_f64_z_f32( float64_t );
#ifdef EXTFLOAT80
typedef void funcType_a_f64_z_extF80( float64_t, extFloat80_t * );
#endif
#ifdef FLOAT128
typedef void funcType_a_f64_z_f128( float64_t, float128_t * );
#endif
typedef float64_t funcType_az_f64( float64_t );
typedef float64_t funcType_abz_f64( float64_t, float64_t );
typedef float64_t funcType_abcz_f64( float64_t, float64_t, float64_t );
typedef bool funcType_ab_f64_z_bool( float64_t, float64_t );
#endif

#ifdef EXTFLOAT80
typedef uint_fast32_t funcType_a_extF80_z_ui32( const extFloat80_t * );
typedef uint_fast64_t funcType_a_extF80_z_ui64( const extFloat80_t * );
typedef int_fast32_t funcType_a_extF80_z_i32( const extFloat80_t * );
typedef int_fast64_t funcType_a_extF80_z_i64( const extFloat80_t * );
#ifdef FLOAT16
typedef float16_t funcType_a_extF80_z_f16( const extFloat80_t * );
#endif
typedef float32_t funcType_a_extF80_z_f32( const extFloat80_t * );
#ifdef FLOAT64
typedef float64_t funcType_a_extF80_z_f64( const extFloat80_t * );
#endif
#ifdef FLOAT128
typedef void funcType_a_extF80_z_f128( const extFloat80_t *, float128_t * );
#endif
typedef void funcType_az_extF80( const extFloat80_t *, extFloat80_t * );
typedef
 void
  funcType_abz_extF80(
      const extFloat80_t *, const extFloat80_t *, extFloat80_t * );
typedef
 bool funcType_ab_extF80_z_bool( const extFloat80_t *, const extFloat80_t * );
#endif

#ifdef FLOAT128
typedef uint_fast32_t funcType_a_f128_z_ui32( const float128_t * );
typedef uint_fast64_t funcType_a_f128_z_ui64( const float128_t * );
typedef int_fast32_t funcType_a_f128_z_i32( const float128_t * );
typedef int_fast64_t funcType_a_f128_z_i64( const float128_t * );
#ifdef FLOAT16
typedef float16_t funcType_a_f128_z_f16( const float128_t * );
#endif
typedef float32_t funcType_a_f128_z_f32( const float128_t * );
#ifdef FLOAT64
typedef float64_t funcType_a_f128_z_f64( const float128_t * );
#endif
#ifdef EXTFLOAT80
typedef void funcType_a_f128_z_extF80( const float128_t *, extFloat80_t * );
#endif
typedef void funcType_az_f128( const float128_t *, float128_t * );
typedef
 void
  funcType_abz_f128( const float128_t *, const float128_t *, float128_t * );
typedef
 void
  funcType_abcz_f128(
      const float128_t *, const float128_t *, const float128_t *, float128_t *
  );
typedef bool funcType_ab_f128_z_bool( const float128_t *, const float128_t * );
#endif

#ifdef FLOAT16

static
uint_fast32_t
 subjFunction_a_f16_z_ui32_rx(
     float16_t a, uint_fast8_t roundingMode, bool exact )
{

    return ((funcType_a_f16_z_ui32 *) subjFunctionPtr)( a );

}

static
uint_fast64_t
 subjFunction_a_f16_z_ui64_rx(
     float16_t a, uint_fast8_t roundingMode, bool exact )
{

    return ((funcType_a_f16_z_ui64 *) subjFunctionPtr)( a );

}

static
int_fast32_t
 subjFunction_a_f16_z_i32_rx(
     float16_t a, uint_fast8_t roundingMode, bool exact )
{

    return ((funcType_a_f16_z_i32 *) subjFunctionPtr)( a );

}

static
int_fast64_t
 subjFunction_a_f16_z_i64_rx(
     float16_t a, uint_fast8_t roundingMode, bool exact )
{

    return ((funcType_a_f16_z_i64 *) subjFunctionPtr)( a );

}

static
float16_t
 subjFunction_az_f16_rx( float16_t a, uint_fast8_t roundingMode, bool exact )
{

    return ((funcType_az_f16 *) subjFunctionPtr)( a );

}

#endif

static
uint_fast32_t
 subjFunction_a_f32_z_ui32_rx(
     float32_t a, uint_fast8_t roundingMode, bool exact )
{

    return ((funcType_a_f32_z_ui32 *) subjFunctionPtr)( a );

}

static
uint_fast64_t
 subjFunction_a_f32_z_ui64_rx(
     float32_t a, uint_fast8_t roundingMode, bool exact )
{

    return ((funcType_a_f32_z_ui64 *) subjFunctionPtr)( a );

}

static
int_fast32_t
 subjFunction_a_f32_z_i32_rx(
     float32_t a, uint_fast8_t roundingMode, bool exact )
{

    return ((funcType_a_f32_z_i32 *) subjFunctionPtr)( a );

}

static
int_fast64_t
 subjFunction_a_f32_z_i64_rx(
     float32_t a, uint_fast8_t roundingMode, bool exact )
{

    return ((funcType_a_f32_z_i64 *) subjFunctionPtr)( a );

}

static
float32_t
 subjFunction_az_f32_rx( float32_t a, uint_fast8_t roundingMode, bool exact )
{

    return ((funcType_az_f32 *) subjFunctionPtr)( a );

}

#ifdef FLOAT64

static
uint_fast32_t
 subjFunction_a_f64_z_ui32_rx(
     float64_t a, uint_fast8_t roundingMode, bool exact )
{

    return ((funcType_a_f64_z_ui32 *) subjFunctionPtr)( a );

}

static
uint_fast64_t
 subjFunction_a_f64_z_ui64_rx(
     float64_t a, uint_fast8_t roundingMode, bool exact )
{

    return ((funcType_a_f64_z_ui64 *) subjFunctionPtr)( a );

}

static
int_fast32_t
 subjFunction_a_f64_z_i32_rx(
     float64_t a, uint_fast8_t roundingMode, bool exact )
{

    return ((funcType_a_f64_z_i32 *) subjFunctionPtr)( a );

}

static
int_fast64_t
 subjFunction_a_f64_z_i64_rx(
     float64_t a, uint_fast8_t roundingMode, bool exact )
{

    return ((funcType_a_f64_z_i64 *) subjFunctionPtr)( a );

}

static
float64_t
 subjFunction_az_f64_rx( float64_t a, uint_fast8_t roundingMode, bool exact )
{

    return ((funcType_az_f64 *) subjFunctionPtr)( a );

}

#endif

#ifdef EXTFLOAT80

static
uint_fast32_t
 subjFunction_a_extF80_z_ui32_rx(
     const extFloat80_t *aPtr, uint_fast8_t roundingMode, bool exact )
{

    return ((funcType_a_extF80_z_ui32 *) subjFunctionPtr)( aPtr );

}

static
uint_fast64_t
 subjFunction_a_extF80_z_ui64_rx(
     const extFloat80_t *aPtr, uint_fast8_t roundingMode, bool exact )
{

    return ((funcType_a_extF80_z_ui64 *) subjFunctionPtr)( aPtr );

}

static
int_fast32_t
 subjFunction_a_extF80_z_i32_rx(
     const extFloat80_t *aPtr, uint_fast8_t roundingMode, bool exact )
{

    return ((funcType_a_extF80_z_i32 *) subjFunctionPtr)( aPtr );

}

static
int_fast64_t
 subjFunction_a_extF80_z_i64_rx(
     const extFloat80_t *aPtr, uint_fast8_t roundingMode, bool exact )
{

    return ((funcType_a_extF80_z_i64 *) subjFunctionPtr)( aPtr );

}

static
void
 subjFunction_az_extF80_rx(
     const extFloat80_t *aPtr,
     uint_fast8_t roundingMode,
     bool exact,
     extFloat80_t *zPtr
 )
{

    return ((funcType_az_extF80 *) subjFunctionPtr)( aPtr, zPtr );

}

#endif

#ifdef FLOAT128

static
uint_fast32_t
 subjFunction_a_f128_z_ui32_rx(
     const float128_t *aPtr, uint_fast8_t roundingMode, bool exact )
{

    return ((funcType_a_f128_z_ui32 *) subjFunctionPtr)( aPtr );

}

static
uint_fast64_t
 subjFunction_a_f128_z_ui64_rx(
     const float128_t *aPtr, uint_fast8_t roundingMode, bool exact )
{

    return ((funcType_a_f128_z_ui64 *) subjFunctionPtr)( aPtr );

}

static
int_fast32_t
 subjFunction_a_f128_z_i32_rx(
     const float128_t *aPtr, uint_fast8_t roundingMode, bool exact )
{

    return ((funcType_a_f128_z_i32 *) subjFunctionPtr)( aPtr );

}

static
int_fast64_t
 subjFunction_a_f128_z_i64_rx(
     const float128_t *aPtr, uint_fast8_t roundingMode, bool exact )
{

    return ((funcType_a_f128_z_i64 *) subjFunctionPtr)( aPtr );

}

static
void
 subjFunction_az_f128_rx(
     const float128_t *aPtr,
     uint_fast8_t roundingMode,
     bool exact,
     float128_t *zPtr
 )
{

    return ((funcType_az_f128 *) subjFunctionPtr)( aPtr, zPtr );

}

#endif

static
void
 testFunctionInstance(
     int functionCode, uint_fast8_t roundingMode, bool exact )
{
#ifdef FLOAT16
    funcType_abz_f16 *trueFunction_abz_f16;
    funcType_ab_f16_z_bool *trueFunction_ab_f16_z_bool;
#endif
    funcType_abz_f32 *trueFunction_abz_f32;
    funcType_ab_f32_z_bool *trueFunction_ab_f32_z_bool;
#ifdef FLOAT64
    funcType_abz_f64 *trueFunction_abz_f64;
    funcType_ab_f64_z_bool *trueFunction_ab_f64_z_bool;
#endif
#ifdef EXTFLOAT80
    funcType_abz_extF80 *trueFunction_abz_extF80;
    funcType_ab_extF80_z_bool *trueFunction_ab_extF80_z_bool;
#endif
#ifdef FLOAT128
    funcType_abz_f128 *trueFunction_abz_f128;
    funcType_ab_f128_z_bool *trueFunction_ab_f128_z_bool;
#endif

    fputs( "Testing ", stderr );
    verCases_writeFunctionName( stderr );
    fputs( ".\n", stderr );
    switch ( functionCode ) {
        /*--------------------------------------------------------------------
        *--------------------------------------------------------------------*/
#ifdef FLOAT16
#ifdef SUBJ_UI32_TO_F16
     case UI32_TO_F16:
        test_a_ui32_z_f16(
            ui32_to_f16, (funcType_a_ui32_z_f16 *) subjFunctionPtr );
        break;
#endif
#endif
#ifdef SUBJ_UI32_TO_F32
     case UI32_TO_F32:
        test_a_ui32_z_f32(
            ui32_to_f32, (funcType_a_ui32_z_f32 *) subjFunctionPtr );
        break;
#endif
#ifdef FLOAT64
#ifdef SUBJ_UI32_TO_F64
     case UI32_TO_F64:
        test_a_ui32_z_f64(
            ui32_to_f64, (funcType_a_ui32_z_f64 *) subjFunctionPtr );
        break;
#endif
#endif
#ifdef EXTFLOAT80
#ifdef SUBJ_UI32_TO_EXTF80
     case UI32_TO_EXTF80:
        test_a_ui32_z_extF80(
            ui32_to_extF80M, (funcType_a_ui32_z_extF80 *) subjFunctionPtr );
        break;
#endif
#endif
#ifdef FLOAT128
#ifdef SUBJ_UI32_TO_F128
     case UI32_TO_F128:
        test_a_ui32_z_f128(
            ui32_to_f128M, (funcType_a_ui32_z_f128 *) subjFunctionPtr );
        break;
#endif
#endif
#ifdef FLOAT16
#ifdef SUBJ_UI64_TO_F16
     case UI64_TO_F16:
        test_a_ui64_z_f16(
            ui64_to_f16, (funcType_a_ui64_z_f16 *) subjFunctionPtr );
        break;
#endif
#endif
#ifdef SUBJ_UI64_TO_F32
     case UI64_TO_F32:
        test_a_ui64_z_f32(
            ui64_to_f32, (funcType_a_ui64_z_f32 *) subjFunctionPtr );
        break;
#endif
#ifdef FLOAT64
#ifdef SUBJ_UI64_TO_F64
     case UI64_TO_F64:
        test_a_ui64_z_f64(
            ui64_to_f64, (funcType_a_ui64_z_f64 *) subjFunctionPtr );
        break;
#endif
#endif
#ifdef EXTFLOAT80
#ifdef SUBJ_UI64_TO_EXTF80
     case UI64_TO_EXTF80:
        test_a_ui64_z_extF80(
            ui64_to_extF80M, (funcType_a_ui64_z_extF80 *) subjFunctionPtr );
        break;
#endif
#endif
#ifdef FLOAT128
#ifdef SUBJ_UI64_TO_F128
     case UI64_TO_F128:
        test_a_ui64_z_f128(
            ui64_to_f128M, (funcType_a_ui64_z_f128 *) subjFunctionPtr );
        break;
#endif
#endif
#ifdef FLOAT16
#ifdef SUBJ_I32_TO_F16
     case I32_TO_F16:
        test_a_i32_z_f16(
            i32_to_f16, (funcType_a_i32_z_f16 *) subjFunctionPtr );
        break;
#endif
#endif
#ifdef SUBJ_I32_TO_F32
     case I32_TO_F32:
        test_a_i32_z_f32(
            i32_to_f32, (funcType_a_i32_z_f32 *) subjFunctionPtr );
        break;
#endif
#ifdef FLOAT64
#ifdef SUBJ_I32_TO_F64
     case I32_TO_F64:
        test_a_i32_z_f64(
            i32_to_f64, (funcType_a_i32_z_f64 *) subjFunctionPtr );
        break;
#endif
#endif
#ifdef EXTFLOAT80
#ifdef SUBJ_I32_TO_EXTF80
     case I32_TO_EXTF80:
        test_a_i32_z_extF80(
            i32_to_extF80M, (funcType_a_i32_z_extF80 *) subjFunctionPtr );
        break;
#endif
#endif
#ifdef FLOAT128
#ifdef SUBJ_I32_TO_F128
     case I32_TO_F128:
        test_a_i32_z_f128(
            i32_to_f128M, (funcType_a_i32_z_f128 *) subjFunctionPtr );
        break;
#endif
#endif
#ifdef FLOAT16
#ifdef SUBJ_I64_TO_F16
     case I64_TO_F16:
        test_a_i64_z_f16(
            i64_to_f16, (funcType_a_i64_z_f16 *) subjFunctionPtr );
        break;
#endif
#endif
#ifdef SUBJ_I64_TO_F32
     case I64_TO_F32:
        test_a_i64_z_f32(
            i64_to_f32, (funcType_a_i64_z_f32 *) subjFunctionPtr );
        break;
#endif
#ifdef FLOAT64
#ifdef SUBJ_I64_TO_F64
     case I64_TO_F64:
        test_a_i64_z_f64(
            i64_to_f64, (funcType_a_i64_z_f64 *) subjFunctionPtr );
        break;
#endif
#endif
#ifdef EXTFLOAT80
#ifdef SUBJ_I64_TO_EXTF80
     case I64_TO_EXTF80:
        test_a_i64_z_extF80(
            i64_to_extF80M, (funcType_a_i64_z_extF80 *) subjFunctionPtr );
        break;
#endif
#endif
#ifdef FLOAT128
#ifdef SUBJ_I64_TO_F128
     case I64_TO_F128:
        test_a_i64_z_f128(
            i64_to_f128M, (funcType_a_i64_z_f128 *) subjFunctionPtr );
        break;
#endif
#endif
        /*--------------------------------------------------------------------
        *--------------------------------------------------------------------*/
#ifdef FLOAT16
     case F16_TO_UI32:
        test_a_f16_z_ui32_rx(
            f16_to_ui32, subjFunction_a_f16_z_ui32_rx, roundingMode, exact );
        break;
     case F16_TO_UI64:
        test_a_f16_z_ui64_rx(
            f16_to_ui64, subjFunction_a_f16_z_ui64_rx, roundingMode, exact );
        break;
     case F16_TO_I32:
        test_a_f16_z_i32_rx(
            f16_to_i32, subjFunction_a_f16_z_i32_rx, roundingMode, exact );
        break;
     case F16_TO_I64:
        test_a_f16_z_i64_rx(
            f16_to_i64, subjFunction_a_f16_z_i64_rx, roundingMode, exact );
        break;
#ifdef SUBJ_F16_TO_F32
     case F16_TO_F32:
        test_a_f16_z_f32(
            f16_to_f32, (funcType_a_f16_z_f32 *) subjFunctionPtr );
        break;
#endif
#ifdef FLOAT64
#ifdef SUBJ_F16_TO_F64
     case F16_TO_F64:
        test_a_f16_z_f64(
            f16_to_f64, (funcType_a_f16_z_f64 *) subjFunctionPtr );
        break;
#endif
#endif
#ifdef EXTFLOAT80
#ifdef SUBJ_F16_TO_EXTF80
     case F16_TO_EXTF80:
        test_a_f16_z_extF80(
            f16_to_extF80M, (funcType_a_f16_z_extF80 *) subjFunctionPtr );
        break;
#endif
#endif
#ifdef FLOAT128
#ifdef SUBJ_F16_TO_F128
     case F16_TO_F128:
        test_a_f16_z_f128(
            f16_to_f128M, (funcType_a_f16_z_f128 *) subjFunctionPtr );
        break;
#endif
#endif
     case F16_ROUNDTOINT:
        test_az_f16_rx(
            f16_roundToInt, subjFunction_az_f16_rx, roundingMode, exact );
        break;
#ifdef SUBJ_F16_ADD
     case F16_ADD:
        trueFunction_abz_f16 = f16_add;
        goto test_abz_f16;
#endif
#ifdef SUBJ_F16_SUB
     case F16_SUB:
        trueFunction_abz_f16 = f16_sub;
        goto test_abz_f16;
#endif
#ifdef SUBJ_F16_MUL
     case F16_MUL:
        trueFunction_abz_f16 = f16_mul;
        goto test_abz_f16;
#endif
#ifdef SUBJ_F16_MULADD
     case F16_MULADD:
        test_abcz_f16( f16_mulAdd, (funcType_abcz_f16 *) subjFunctionPtr );
        break;
#endif
#ifdef SUBJ_F16_DIV
     case F16_DIV:
        trueFunction_abz_f16 = f16_div;
        goto test_abz_f16;
#endif
#ifdef SUBJ_F16_REM
     case F16_REM:
        trueFunction_abz_f16 = f16_rem;
        goto test_abz_f16;
#endif
     test_abz_f16:
        test_abz_f16(
            trueFunction_abz_f16, (funcType_abz_f16 *) subjFunctionPtr );
        break;
#ifdef SUBJ_F16_SQRT
     case F16_SQRT:
        test_az_f16( f16_sqrt, (funcType_az_f16 *) subjFunctionPtr );
        break;
#endif
#ifdef SUBJ_F16_EQ
     case F16_EQ:
        trueFunction_ab_f16_z_bool = f16_eq;
        goto test_ab_f16_z_bool;
#endif
#ifdef SUBJ_F16_LE
     case F16_LE:
        trueFunction_ab_f16_z_bool = f16_le;
        goto test_ab_f16_z_bool;
#endif
#ifdef SUBJ_F16_LT
     case F16_LT:
        trueFunction_ab_f16_z_bool = f16_lt;
        goto test_ab_f16_z_bool;
#endif
#ifdef SUBJ_F16_EQ_SIGNALING
     case F16_EQ_SIGNALING:
        trueFunction_ab_f16_z_bool = f16_eq_signaling;
        goto test_ab_f16_z_bool;
#endif
#ifdef SUBJ_F16_LE_QUIET
     case F16_LE_QUIET:
        trueFunction_ab_f16_z_bool = f16_le_quiet;
        goto test_ab_f16_z_bool;
#endif
#ifdef SUBJ_F16_LT_QUIET
     case F16_LT_QUIET:
        trueFunction_ab_f16_z_bool = f16_lt_quiet;
        goto test_ab_f16_z_bool;
#endif
     test_ab_f16_z_bool:
        test_ab_f16_z_bool(
            trueFunction_ab_f16_z_bool,
            (funcType_ab_f16_z_bool *) subjFunctionPtr
        );
        break;
#endif
        /*--------------------------------------------------------------------
        *--------------------------------------------------------------------*/
     case F32_TO_UI32:
        test_a_f32_z_ui32_rx(
            f32_to_ui32, subjFunction_a_f32_z_ui32_rx, roundingMode, exact );
        break;
     case F32_TO_UI64:
        test_a_f32_z_ui64_rx(
            f32_to_ui64, subjFunction_a_f32_z_ui64_rx, roundingMode, exact );
        break;
     case F32_TO_I32:
        test_a_f32_z_i32_rx(
            f32_to_i32, subjFunction_a_f32_z_i32_rx, roundingMode, exact );
        break;
     case F32_TO_I64:
        test_a_f32_z_i64_rx(
            f32_to_i64, subjFunction_a_f32_z_i64_rx, roundingMode, exact );
        break;
#ifdef FLOAT16
#ifdef SUBJ_F32_TO_F16
     case F32_TO_F16:
        test_a_f32_z_f16(
            f32_to_f16, (funcType_a_f32_z_f16 *) subjFunctionPtr );
        break;
#endif
#endif
#ifdef FLOAT64
#ifdef SUBJ_F32_TO_F64
     case F32_TO_F64:
        test_a_f32_z_f64(
            f32_to_f64, (funcType_a_f32_z_f64 *) subjFunctionPtr );
        break;
#endif
#endif
#ifdef EXTFLOAT80
#ifdef SUBJ_F32_TO_EXTF80
     case F32_TO_EXTF80:
        test_a_f32_z_extF80(
            f32_to_extF80M, (funcType_a_f32_z_extF80 *) subjFunctionPtr );
        break;
#endif
#endif
#ifdef FLOAT128
#ifdef SUBJ_F32_TO_F128
     case F32_TO_F128:
        test_a_f32_z_f128(
            f32_to_f128M, (funcType_a_f32_z_f128 *) subjFunctionPtr );
        break;
#endif
#endif
     case F32_ROUNDTOINT:
        test_az_f32_rx(
            f32_roundToInt, subjFunction_az_f32_rx, roundingMode, exact );
        break;
#ifdef SUBJ_F32_ADD
     case F32_ADD:
        trueFunction_abz_f32 = f32_add;
        goto test_abz_f32;
#endif
#ifdef SUBJ_F32_SUB
     case F32_SUB:
        trueFunction_abz_f32 = f32_sub;
        goto test_abz_f32;
#endif
#ifdef SUBJ_F32_MUL
     case F32_MUL:
        trueFunction_abz_f32 = f32_mul;
        goto test_abz_f32;
#endif
#ifdef SUBJ_F32_MULADD
     case F32_MULADD:
        test_abcz_f32( f32_mulAdd, (funcType_abcz_f32 *) subjFunctionPtr );
        break;
#endif
#ifdef SUBJ_F32_DIV
     case F32_DIV:
        trueFunction_abz_f32 = f32_div;
        goto test_abz_f32;
#endif
#ifdef SUBJ_F32_REM
     case F32_REM:
        trueFunction_abz_f32 = f32_rem;
        goto test_abz_f32;
#endif
     test_abz_f32:
        test_abz_f32(
            trueFunction_abz_f32, (funcType_abz_f32 *) subjFunctionPtr );
        break;
#ifdef SUBJ_F32_SQRT
     case F32_SQRT:
        test_az_f32( f32_sqrt, (funcType_az_f32 *) subjFunctionPtr );
        break;
#endif
#ifdef SUBJ_F32_EQ
     case F32_EQ:
        trueFunction_ab_f32_z_bool = f32_eq;
        goto test_ab_f32_z_bool;
#endif
#ifdef SUBJ_F32_LE
     case F32_LE:
        trueFunction_ab_f32_z_bool = f32_le;
        goto test_ab_f32_z_bool;
#endif
#ifdef SUBJ_F32_LT
     case F32_LT:
        trueFunction_ab_f32_z_bool = f32_lt;
        goto test_ab_f32_z_bool;
#endif
#ifdef SUBJ_F32_EQ_SIGNALING
     case F32_EQ_SIGNALING:
        trueFunction_ab_f32_z_bool = f32_eq_signaling;
        goto test_ab_f32_z_bool;
#endif
#ifdef SUBJ_F32_LE_QUIET
     case F32_LE_QUIET:
        trueFunction_ab_f32_z_bool = f32_le_quiet;
        goto test_ab_f32_z_bool;
#endif
#ifdef SUBJ_F32_LT_QUIET
     case F32_LT_QUIET:
        trueFunction_ab_f32_z_bool = f32_lt_quiet;
        goto test_ab_f32_z_bool;
#endif
     test_ab_f32_z_bool:
        test_ab_f32_z_bool(
            trueFunction_ab_f32_z_bool,
            (funcType_ab_f32_z_bool *) subjFunctionPtr
        );
        break;
        /*--------------------------------------------------------------------
        *--------------------------------------------------------------------*/
#ifdef FLOAT64
     case F64_TO_UI32:
        test_a_f64_z_ui32_rx(
            f64_to_ui32, subjFunction_a_f64_z_ui32_rx, roundingMode, exact );
        break;
     case F64_TO_UI64:
        test_a_f64_z_ui64_rx(
            f64_to_ui64, subjFunction_a_f64_z_ui64_rx, roundingMode, exact );
        break;
     case F64_TO_I32:
        test_a_f64_z_i32_rx(
            f64_to_i32, subjFunction_a_f64_z_i32_rx, roundingMode, exact );
        break;
     case F64_TO_I64:
        test_a_f64_z_i64_rx(
            f64_to_i64, subjFunction_a_f64_z_i64_rx, roundingMode, exact );
        break;
#ifdef FLOAT16
#ifdef SUBJ_F64_TO_F16
     case F64_TO_F16:
        test_a_f64_z_f16(
            f64_to_f16, (funcType_a_f64_z_f16 *) subjFunctionPtr );
        break;
#endif
#endif
#ifdef SUBJ_F64_TO_F32
     case F64_TO_F32:
        test_a_f64_z_f32(
            f64_to_f32, (funcType_a_f64_z_f32 *) subjFunctionPtr );
        break;
#endif
#ifdef EXTFLOAT80
#ifdef SUBJ_F64_TO_EXTF80
     case F64_TO_EXTF80:
        test_a_f64_z_extF80(
            f64_to_extF80M, (funcType_a_f64_z_extF80 *) subjFunctionPtr );
        break;
#endif
#endif
#ifdef FLOAT128
#ifdef SUBJ_F64_TO_F128
     case F64_TO_F128:
        test_a_f64_z_f128(
            f64_to_f128M, (funcType_a_f64_z_f128 *) subjFunctionPtr );
        break;
#endif
#endif
     case F64_ROUNDTOINT:
        test_az_f64_rx(
            f64_roundToInt, subjFunction_az_f64_rx, roundingMode, exact );
        break;
#ifdef SUBJ_F64_ADD
     case F64_ADD:
        trueFunction_abz_f64 = f64_add;
        goto test_abz_f64;
#endif
#ifdef SUBJ_F64_SUB
     case F64_SUB:
        trueFunction_abz_f64 = f64_sub;
        goto test_abz_f64;
#endif
#ifdef SUBJ_F64_MUL
     case F64_MUL:
        trueFunction_abz_f64 = f64_mul;
        goto test_abz_f64;
#endif
#ifdef SUBJ_F64_MULADD
     case F64_MULADD:
        test_abcz_f64( f64_mulAdd, (funcType_abcz_f64 *) subjFunctionPtr );
        break;
#endif
#ifdef SUBJ_F64_DIV
     case F64_DIV:
        trueFunction_abz_f64 = f64_div;
        goto test_abz_f64;
#endif
#ifdef SUBJ_F64_REM
     case F64_REM:
        trueFunction_abz_f64 = f64_rem;
        goto test_abz_f64;
#endif
     test_abz_f64:
        test_abz_f64(
            trueFunction_abz_f64, (funcType_abz_f64 *) subjFunctionPtr );
        break;
#ifdef SUBJ_F64_SQRT
     case F64_SQRT:
        test_az_f64( f64_sqrt, (funcType_az_f64 *) subjFunctionPtr );
        break;
#endif
#ifdef SUBJ_F64_EQ
     case F64_EQ:
        trueFunction_ab_f64_z_bool = f64_eq;
        goto test_ab_f64_z_bool;
#endif
#ifdef SUBJ_F64_LE
     case F64_LE:
        trueFunction_ab_f64_z_bool = f64_le;
        goto test_ab_f64_z_bool;
#endif
#ifdef SUBJ_F64_LT
     case F64_LT:
        trueFunction_ab_f64_z_bool = f64_lt;
        goto test_ab_f64_z_bool;
#endif
#ifdef SUBJ_F64_EQ_SIGNALING
     case F64_EQ_SIGNALING:
        trueFunction_ab_f64_z_bool = f64_eq_signaling;
        goto test_ab_f64_z_bool;
#endif
#ifdef SUBJ_F64_LE_QUIET
     case F64_LE_QUIET:
        trueFunction_ab_f64_z_bool = f64_le_quiet;
        goto test_ab_f64_z_bool;
#endif
#ifdef SUBJ_F64_LT_QUIET
     case F64_LT_QUIET:
        trueFunction_ab_f64_z_bool = f64_lt_quiet;
        goto test_ab_f64_z_bool;
#endif
     test_ab_f64_z_bool:
        test_ab_f64_z_bool(
            trueFunction_ab_f64_z_bool,
            (funcType_ab_f64_z_bool *) subjFunctionPtr
        );
        break;
#endif
        /*--------------------------------------------------------------------
        *--------------------------------------------------------------------*/
#ifdef EXTFLOAT80
     case EXTF80_TO_UI32:
        test_a_extF80_z_ui32_rx(
            extF80M_to_ui32,
            subjFunction_a_extF80_z_ui32_rx,
            roundingMode,
            exact
        );
        break;
     case EXTF80_TO_UI64:
        test_a_extF80_z_ui64_rx(
            extF80M_to_ui64,
            subjFunction_a_extF80_z_ui64_rx,
            roundingMode,
            exact
        );
        break;
     case EXTF80_TO_I32:
        test_a_extF80_z_i32_rx(
            extF80M_to_i32, subjFunction_a_extF80_z_i32_rx, roundingMode, exact
        );
        break;
     case EXTF80_TO_I64:
        test_a_extF80_z_i64_rx(
            extF80M_to_i64, subjFunction_a_extF80_z_i64_rx, roundingMode, exact
        );
        break;
#ifdef FLOAT16
#ifdef SUBJ_EXTF80_TO_F16
     case EXTF80_TO_F16:
        test_a_extF80_z_f16(
            extF80M_to_f16, (funcType_a_extF80_z_f16 *) subjFunctionPtr );
        break;
#endif
#endif
#ifdef SUBJ_EXTF80_TO_F32
     case EXTF80_TO_F32:
        test_a_extF80_z_f32(
            extF80M_to_f32, (funcType_a_extF80_z_f32 *) subjFunctionPtr );
        break;
#endif
#ifdef FLOAT64
#ifdef SUBJ_EXTF80_TO_F64
     case EXTF80_TO_F64:
        test_a_extF80_z_f64(
            extF80M_to_f64, (funcType_a_extF80_z_f64 *) subjFunctionPtr );
        break;
#endif
#endif
#ifdef FLOAT128
#ifdef SUBJ_EXTF80_TO_F128
     case EXTF80_TO_F128:
        test_a_extF80_z_f128(
            extF80M_to_f128M, (funcType_a_extF80_z_f128 *) subjFunctionPtr );
        break;
#endif
#endif
     case EXTF80_ROUNDTOINT:
        test_az_extF80_rx(
            extF80M_roundToInt, subjFunction_az_extF80_rx, roundingMode, exact
        );
        break;
#ifdef SUBJ_EXTF80_ADD
     case EXTF80_ADD:
        trueFunction_abz_extF80 = extF80M_add;
        goto test_abz_extF80;
#endif
#ifdef SUBJ_EXTF80_SUB
     case EXTF80_SUB:
        trueFunction_abz_extF80 = extF80M_sub;
        goto test_abz_extF80;
#endif
#ifdef SUBJ_EXTF80_MUL
     case EXTF80_MUL:
        trueFunction_abz_extF80 = extF80M_mul;
        goto test_abz_extF80;
#endif
#ifdef SUBJ_EXTF80_DIV
     case EXTF80_DIV:
        trueFunction_abz_extF80 = extF80M_div;
        goto test_abz_extF80;
#endif
#ifdef SUBJ_EXTF80_REM
     case EXTF80_REM:
        trueFunction_abz_extF80 = extF80M_rem;
        goto test_abz_extF80;
#endif
     test_abz_extF80:
        test_abz_extF80(
            trueFunction_abz_extF80, (funcType_abz_extF80 *) subjFunctionPtr );
        break;
#ifdef SUBJ_EXTF80_SQRT
     case EXTF80_SQRT:
        test_az_extF80( extF80M_sqrt, (funcType_az_extF80 *) subjFunctionPtr );
        break;
#endif
#ifdef SUBJ_EXTF80_EQ
     case EXTF80_EQ:
        trueFunction_ab_extF80_z_bool = extF80M_eq;
        goto test_ab_extF80_z_bool;
#endif
#ifdef SUBJ_EXTF80_LE
     case EXTF80_LE:
        trueFunction_ab_extF80_z_bool = extF80M_le;
        goto test_ab_extF80_z_bool;
#endif
#ifdef SUBJ_EXTF80_LT
     case EXTF80_LT:
        trueFunction_ab_extF80_z_bool = extF80M_lt;
        goto test_ab_extF80_z_bool;
#endif
#ifdef SUBJ_EXTF80_EQ_SIGNALING
     case EXTF80_EQ_SIGNALING:
        trueFunction_ab_extF80_z_bool = extF80M_eq_signaling;
        goto test_ab_extF80_z_bool;
#endif
#ifdef SUBJ_EXTF80_LE_QUIET
     case EXTF80_LE_QUIET:
        trueFunction_ab_extF80_z_bool = extF80M_le_quiet;
        goto test_ab_extF80_z_bool;
#endif
#ifdef SUBJ_EXTF80_LT_QUIET
     case EXTF80_LT_QUIET:
        trueFunction_ab_extF80_z_bool = extF80M_lt_quiet;
        goto test_ab_extF80_z_bool;
#endif
     test_ab_extF80_z_bool:
        test_ab_extF80_z_bool(
            trueFunction_ab_extF80_z_bool,
            (funcType_ab_extF80_z_bool *) subjFunctionPtr
        );
        break;
#endif
        /*--------------------------------------------------------------------
        *--------------------------------------------------------------------*/
#ifdef FLOAT128
     case F128_TO_UI32:
        test_a_f128_z_ui32_rx(
            f128M_to_ui32, subjFunction_a_f128_z_ui32_rx, roundingMode, exact
        );
        break;
     case F128_TO_UI64:
        test_a_f128_z_ui64_rx(
            f128M_to_ui64, subjFunction_a_f128_z_ui64_rx, roundingMode, exact
        );
        break;
     case F128_TO_I32:
        test_a_f128_z_i32_rx(
            f128M_to_i32, subjFunction_a_f128_z_i32_rx, roundingMode, exact );
        break;
     case F128_TO_I64:
        test_a_f128_z_i64_rx(
            f128M_to_i64, subjFunction_a_f128_z_i64_rx, roundingMode, exact );
        break;
#ifdef FLOAT16
#ifdef SUBJ_F128_TO_F16
     case F128_TO_F16:
        test_a_f128_z_f16(
            f128M_to_f16, (funcType_a_f128_z_f16 *) subjFunctionPtr );
        break;
#endif
#endif
#ifdef SUBJ_F128_TO_F32
     case F128_TO_F32:
        test_a_f128_z_f32(
            f128M_to_f32, (funcType_a_f128_z_f32 *) subjFunctionPtr );
        break;
#endif
#ifdef FLOAT64
#ifdef SUBJ_F128_TO_F64
     case F128_TO_F64:
        test_a_f128_z_f64(
            f128M_to_f64, (funcType_a_f128_z_f64 *) subjFunctionPtr );
        break;
#endif
#endif
#ifdef EXTFLOAT80
#ifdef SUBJ_F128_TO_EXTF80
     case F128_TO_EXTF80:
        test_a_f128_z_extF80(
            f128M_to_extF80M, (funcType_a_f128_z_extF80 *) subjFunctionPtr );
        break;
#endif
#endif
     case F128_ROUNDTOINT:
        test_az_f128_rx(
            f128M_roundToInt, subjFunction_az_f128_rx, roundingMode, exact );
        break;
#ifdef SUBJ_F128_ADD
     case F128_ADD:
        trueFunction_abz_f128 = f128M_add;
        goto test_abz_f128;
#endif
#ifdef SUBJ_F128_SUB
     case F128_SUB:
        trueFunction_abz_f128 = f128M_sub;
        goto test_abz_f128;
#endif
#ifdef SUBJ_F128_MUL
     case F128_MUL:
        trueFunction_abz_f128 = f128M_mul;
        goto test_abz_f128;
#endif
#ifdef SUBJ_F128_MULADD
     case F128_MULADD:
        test_abcz_f128( f128M_mulAdd, (funcType_abcz_f128 *) subjFunctionPtr );
        break;
#endif
#ifdef SUBJ_F128_DIV
     case F128_DIV:
        trueFunction_abz_f128 = f128M_div;
        goto test_abz_f128;
#endif
#ifdef SUBJ_F128_REM
     case F128_REM:
        trueFunction_abz_f128 = f128M_rem;
        goto test_abz_f128;
#endif
     test_abz_f128:
        test_abz_f128(
            trueFunction_abz_f128, (funcType_abz_f128 *) subjFunctionPtr );
        break;
#ifdef SUBJ_F128_SQRT
     case F128_SQRT:
        test_az_f128( f128M_sqrt, (funcType_az_f128 *) subjFunctionPtr );
        break;
#endif
#ifdef SUBJ_F128_EQ
     case F128_EQ:
        trueFunction_ab_f128_z_bool = f128M_eq;
        goto test_ab_f128_z_bool;
#endif
#ifdef SUBJ_F128_LE
     case F128_LE:
        trueFunction_ab_f128_z_bool = f128M_le;
        goto test_ab_f128_z_bool;
#endif
#ifdef SUBJ_F128_LT
     case F128_LT:
        trueFunction_ab_f128_z_bool = f128M_lt;
        goto test_ab_f128_z_bool;
#endif
#ifdef SUBJ_F128_EQ_SIGNALING
     case F128_EQ_SIGNALING:
        trueFunction_ab_f128_z_bool = f128M_eq_signaling;
        goto test_ab_f128_z_bool;
#endif
#ifdef SUBJ_F128_LE_QUIET
     case F128_LE_QUIET:
        trueFunction_ab_f128_z_bool = f128M_le_quiet;
        goto test_ab_f128_z_bool;
#endif
#ifdef SUBJ_F128_LT_QUIET
     case F128_LT_QUIET:
        trueFunction_ab_f128_z_bool = f128M_lt_quiet;
        goto test_ab_f128_z_bool;
#endif
     test_ab_f128_z_bool:
        test_ab_f128_z_bool(
            trueFunction_ab_f128_z_bool,
            (funcType_ab_f128_z_bool *) subjFunctionPtr
        );
        break;
#endif
    }
    if ( (verCases_errorStop && verCases_anyErrors) || verCases_stop ) {
        verCases_exitWithStatus();
    }

}

static
void
 testFunction(
     const struct standardFunctionInfo *standardFunctionInfoPtr,
     uint_fast8_t roundingPrecisionIn,
     int roundingCodeIn
 )
{
    int functionCode, functionAttribs;
    bool standardFunctionHasFixedRounding;
    int roundingCode;
    bool exact;
    uint_fast8_t roundingPrecision, roundingMode;

    functionCode = standardFunctionInfoPtr->functionCode;
    functionAttribs = functionInfos[functionCode].attribs;
    standardFunctionHasFixedRounding = false;
    if ( functionAttribs & FUNC_ARG_ROUNDINGMODE ) {
        roundingCode = standardFunctionInfoPtr->roundingCode;
        if ( roundingCode ) {
            standardFunctionHasFixedRounding = true;
            roundingCodeIn = roundingCode;
        }
    }
    exact = standardFunctionInfoPtr->exact;
    verCases_functionNamePtr = standardFunctionInfoPtr->namePtr;
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
            extF80_roundingPrecision = roundingPrecision;
            subjfloat_setExtF80RoundingPrecision( roundingPrecision );
        }
#endif
        for (
            roundingCode = 1; roundingCode < NUM_ROUNDINGMODES; ++roundingCode
        ) {
#ifndef SUBJFLOAT_ROUND_NEAR_MAXMAG
            if ( roundingCode != ROUND_NEAR_MAXMAG ) {
#endif
#if defined FLOAT_ROUND_ODD && ! defined SUBJFLOAT_ROUND_NEAR_MAXMAG
            if ( roundingCode != ROUND_ODD ) {
#endif
                if (
                    functionAttribs
                        & (FUNC_ARG_ROUNDINGMODE | FUNC_EFF_ROUNDINGMODE)
                ) {
                    if ( roundingCodeIn ) roundingCode = roundingCodeIn;
                } else {
                    roundingCode = 0;
                }
                verCases_roundingCode =
                    standardFunctionHasFixedRounding ? 0 : roundingCode;
                if ( roundingCode ) {
                    roundingMode = roundingModes[roundingCode];
                    softfloat_roundingMode = roundingMode;
                    if ( ! standardFunctionHasFixedRounding ) {
                        subjfloat_setRoundingMode( roundingMode );
                    }
                }
                testFunctionInstance( functionCode, roundingMode, exact );
                if ( roundingCodeIn || ! roundingCode ) break;
#if defined FLOAT_ROUND_ODD && ! defined SUBJFLOAT_ROUND_NEAR_MAXMAG
            }
#endif
#ifndef SUBJFLOAT_ROUND_NEAR_MAXMAG
            }
#endif
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
    const struct standardFunctionInfo *standardFunctionInfoPtr;
    int numOperands;
    uint_fast8_t roundingPrecision;
    int roundingCode;
    const char *argPtr;
    void (*const *subjFunctionPtrPtr)();
    const char *functionNamePtr;
    unsigned long ui;
    long i;
    int functionMatchAttrib;

    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    fail_programName = "testfloat";
    if ( argc <= 1 ) goto writeHelpMessage;
    genCases_setLevel( 1 );
    verCases_maxErrorCount = 20;
    testLoops_trueFlagsFunction = clearExceptionFlags;
    testLoops_subjFlagsFunction = subjfloat_clearExceptionFlags;
    haveFunctionArg = false;
    standardFunctionInfoPtr = 0;
    numOperands = 0;
    roundingPrecision = 0;
    roundingCode = 0;
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
"testfloat [<option>...] <function>\n"
"  <option>:  (* is default)\n"
"    -help            --Write this message and exit.\n"
"    -list            --List all testable subject functions and exit.\n"
"    -seed <num>      --Set pseudo-random number generator seed to <num>.\n"
" *  -seed 1\n"
"    -level <num>     --Testing level <num> (1 or 2).\n"
" *  -level 1\n"
"    -errors <num>    --Stop each function test after <num> errors.\n"
" *  -errors 20\n"
"    -errorstop       --Exit after first function with any error.\n"
"    -forever         --Test one function repeatedly (implies '-level 2').\n"
"    -checkNaNs       --Check for specific NaN results.\n"
"    -checkInvInts    --Check for specific invalid integer results.\n"
"    -checkAll        --Same as both '-checkNaNs' and '-checkInvInts'.\n"
#ifdef EXTFLOAT80
"    -precision32     --For extF80, test only 32-bit rounding precision.\n"
"    -precision64     --For extF80, test only 64-bit rounding precision.\n"
"    -precision80     --For extF80, test only 80-bit rounding precision.\n"
#endif
"    -r<round>        --Test only specified rounding (if not inherent to\n"
"                         function).\n"
"    -tininessbefore  --Underflow tininess is detected before rounding.\n"
"    -tininessafter   --Underflow tininess is detected after rounding.\n"
"  <function>:\n"
"    <int>_to_<float>               <float>_add      <float>_eq\n"
"    <float>_to_<int>_r_<round>     <float>_sub      <float>_le\n"
"    <float>_to_<int>_rx_<round>    <float>_mul      <float>_lt\n"
"    <float>_to_<float>             <float>_mulAdd   <float>_eq_signaling\n"
"    <float>_roundToInt_r_<round>   <float>_div      <float>_le_quiet\n"
"    <float>_roundToInt_x           <float>_rem      <float>_lt_quiet\n"
"                                   <float>_sqrt\n"
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
"  <round>:\n"
"    near_even        --Round to nearest/even.\n"
"    minMag           --Round to minimum magnitude (toward zero).\n"
"    min              --Round to minimum (down).\n"
"    max              --Round to maximum (up).\n"
#ifdef SUBJFLOAT_ROUND_NEAR_MAXMAG
"    near_maxMag      --Round to nearest/maximum magnitude (nearest/away).\n"
#endif
#if defined FLOAT_ROUND_ODD && defined SUBJFLOAT_ROUND_ODD
"    odd              --Round to odd (jamming).  (Not allowed as an inherent\n"
"                         rounding mode.  For 'roundToInt_x', rounds to minimum\n"
"                         magnitude instead.)\n"
#endif
                ,
                stdout
            );
            return EXIT_SUCCESS;
        } else if ( ! strcmp( argPtr, "list" ) ) {
            standardFunctionInfoPtr = standardFunctionInfos;
            subjFunctionPtrPtr = subjfloat_functions;
            for (;;) {
                functionNamePtr = standardFunctionInfoPtr->namePtr;
                if ( ! functionNamePtr ) break;
                if ( *subjFunctionPtrPtr ) puts( functionNamePtr );
                ++standardFunctionInfoPtr;
                ++subjFunctionPtrPtr;
            }
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
#ifdef SUBJFLOAT_ROUND_NEAR_MAXMAG
            roundingCode = ROUND_NEAR_MAXMAG;
#else
            fail(
             "Rounding mode 'near_maxMag' is not supported or cannot be tested"
            );
#endif
#ifdef FLOAT_ROUND_ODD
        } else if ( ! strcmp( argPtr, "rodd" ) ) {
#ifdef SUBJFLOAT_ROUND_ODD
            roundingCode = ROUND_ODD;
#else
            fail( "Rounding mode 'odd' is not supported or cannot be tested" );
#endif
#endif
        } else if ( ! strcmp( argPtr, "tininessbefore" ) ) {
            softfloat_detectTininess = softfloat_tininess_beforeRounding;
        } else if ( ! strcmp( argPtr, "tininessafter" ) ) {
            softfloat_detectTininess = softfloat_tininess_afterRounding;
        } else if ( ! strcmp( argPtr, "all1" ) ) {
            haveFunctionArg = true;
            standardFunctionInfoPtr = 0;
            numOperands = 1;
        } else if ( ! strcmp( argPtr, "all2" ) ) {
            haveFunctionArg = true;
            standardFunctionInfoPtr = 0;
            numOperands = 2;
        } else {
            standardFunctionInfoPtr = standardFunctionInfos;
            for (;;) {
                functionNamePtr = standardFunctionInfoPtr->namePtr;
                if ( ! functionNamePtr ) {
                    fail( "Invalid argument '%s'", *argv );
                }
                if ( ! strcmp( argPtr, functionNamePtr ) ) break;
                ++standardFunctionInfoPtr;
            }
            subjFunctionPtr =
                subjfloat_functions
                    [standardFunctionInfoPtr - standardFunctionInfos];
            if ( ! subjFunctionPtr ) {
                fail(
                    "Function '%s' is not supported or cannot be tested",
                    argPtr
                );
            }
            haveFunctionArg = true;
        }
    }
    if ( ! haveFunctionArg ) fail( "Function argument required" );
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
    signal( SIGINT, catchSIGINT );
    signal( SIGTERM, catchSIGINT );
    if ( standardFunctionInfoPtr ) {
        if ( testLoops_forever ) {
            if ( ! roundingPrecision ) roundingPrecision = 80;
            if ( ! roundingCode ) roundingCode = ROUND_NEAR_EVEN;
        }
        testFunction(
            standardFunctionInfoPtr, roundingPrecision, roundingCode );
    } else {
        if ( testLoops_forever ) {
            fail( "Can test only one function with '-forever' option" );
        }
        functionMatchAttrib =
            (numOperands == 1) ? FUNC_ARG_UNARY : FUNC_ARG_BINARY;
        standardFunctionInfoPtr = standardFunctionInfos;
        subjFunctionPtrPtr = subjfloat_functions;
        while ( standardFunctionInfoPtr->namePtr ) {
            subjFunctionPtr = *subjFunctionPtrPtr;
            if (
                subjFunctionPtr
                    && (functionInfos[standardFunctionInfoPtr->functionCode]
                            .attribs
                            & functionMatchAttrib)
            ) {
                testFunction(
                    standardFunctionInfoPtr, roundingPrecision, roundingCode );
            }
            ++standardFunctionInfoPtr;
            ++subjFunctionPtrPtr;
        }
    }
    verCases_exitWithStatus();
    /*------------------------------------------------------------------------
    *------------------------------------------------------------------------*/
 optionError:
    fail( "'%s' option requires numeric argument", *argv );

}

