
/*============================================================================

This C source file is part of TestFloat, Release 3e, a package of programs for
testing the correctness of floating-point arithmetic complying with the IEEE
Standard for Floating-Point, by John R. Hauser.

Copyright 2011, 2012, 2013, 2014, 2017 The Regents of the University of
California.  All rights reserved.

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
#include <fenv.h>
#include <math.h>
#include "platform.h"
#include "softfloat.h"
#include "subjfloat_config.h"
#include "subjfloat.h"

#pragma STDC FENV_ACCESS ON

void subjfloat_setRoundingMode( uint_fast8_t roundingMode )
{

    fesetround(
          (roundingMode == softfloat_round_near_even) ? FE_TONEAREST
        : (roundingMode == softfloat_round_minMag)    ? FE_TOWARDZERO
        : (roundingMode == softfloat_round_min)       ? FE_DOWNWARD
        : FE_UPWARD
    );

}

void subjfloat_setExtF80RoundingPrecision( uint_fast8_t roundingPrecision )
{

}

uint_fast8_t subjfloat_clearExceptionFlags( void )
{
    int subjExceptionFlags;
    uint_fast8_t exceptionFlags;

    subjExceptionFlags =
        fetestexcept(
            FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW | FE_INEXACT
        );
    feclearexcept(
        FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW | FE_INEXACT );
    exceptionFlags = 0;
    if ( subjExceptionFlags & FE_INVALID ) {
        exceptionFlags |= softfloat_flag_invalid;
    }
    if ( subjExceptionFlags & FE_DIVBYZERO ) {
        exceptionFlags |= softfloat_flag_infinite;
    }
    if ( subjExceptionFlags & FE_OVERFLOW ) {
        exceptionFlags |= softfloat_flag_overflow;
    }
    if ( subjExceptionFlags & FE_UNDERFLOW ) {
        exceptionFlags |= softfloat_flag_underflow;
    }
    if ( subjExceptionFlags & FE_INEXACT ) {
        exceptionFlags |= softfloat_flag_inexact;
    }
    return exceptionFlags;

}

union f32_f { float32_t f32; float f; };

float32_t subj_ui32_to_f32( uint32_t a )
{
    union f32_f uZ;

    uZ.f = a;
    return uZ.f32;

}

float32_t subj_ui64_to_f32( uint64_t a )
{
    union f32_f uZ;

    uZ.f = a;
    return uZ.f32;

}

float32_t subj_i32_to_f32( int32_t a )
{
    union f32_f uZ;

    uZ.f = a;
    return uZ.f32;

}

float32_t subj_i64_to_f32( int64_t a )
{
    union f32_f uZ;

    uZ.f = a;
    return uZ.f32;

}

uint_fast32_t subj_f32_to_ui32_rx_minMag( float32_t a )
{
    union f32_f uA;

    uA.f32 = a;
    return (uint32_t) uA.f;

}

uint_fast64_t subj_f32_to_ui64_rx_minMag( float32_t a )
{
    union f32_f uA;

    uA.f32 = a;
    return (uint64_t) uA.f;

}

int_fast32_t subj_f32_to_i32_rx_minMag( float32_t a )
{
    union f32_f uA;

    uA.f32 = a;
    return (int32_t) uA.f;

}

int_fast64_t subj_f32_to_i64_rx_minMag( float32_t a )
{
    union f32_f uA;

    uA.f32 = a;
    return (int64_t) uA.f;

}

float32_t subj_f32_add( float32_t a, float32_t b )
{
    union f32_f uA, uB, uZ;

    uA.f32 = a;
    uB.f32 = b;
    uZ.f = uA.f + uB.f;
    return uZ.f32;

}

float32_t subj_f32_sub( float32_t a, float32_t b )
{
    union f32_f uA, uB, uZ;

    uA.f32 = a;
    uB.f32 = b;
    uZ.f = uA.f - uB.f;
    return uZ.f32;

}

float32_t subj_f32_mul( float32_t a, float32_t b )
{
    union f32_f uA, uB, uZ;

    uA.f32 = a;
    uB.f32 = b;
    uZ.f = uA.f * uB.f;
    return uZ.f32;

}

#ifdef __STDC_VERSION__
#if 199901L <= __STDC_VERSION__

float32_t subj_f32_mulAdd( float32_t a, float32_t b, float32_t c )
{
    union f32_f uA, uB, uC, uZ;

    uA.f32 = a;
    uB.f32 = b;
    uC.f32 = c;
    uZ.f = fmaf( uA.f, uB.f, uC.f );
    return uZ.f32;

}

#endif
#endif

float32_t subj_f32_div( float32_t a, float32_t b )
{
    union f32_f uA, uB, uZ;

    uA.f32 = a;
    uB.f32 = b;
    uZ.f = uA.f / uB.f;
    return uZ.f32;

}

#ifdef __STDC_VERSION__
#if 199901L <= __STDC_VERSION__

float32_t subj_f32_sqrt( float32_t a )
{
    union f32_f uA, uZ;

    uA.f32 = a;
    uZ.f = sqrtf( uA.f );
    return uZ.f32;

}

#endif
#endif

bool subj_f32_eq( float32_t a, float32_t b )
{
    union f32_f uA, uB;

    uA.f32 = a;
    uB.f32 = b;
    return (uA.f == uB.f);

}

bool subj_f32_le( float32_t a, float32_t b )
{
    union f32_f uA, uB;

    uA.f32 = a;
    uB.f32 = b;
    return (uA.f <= uB.f);

}

bool subj_f32_lt( float32_t a, float32_t b )
{
    union f32_f uA, uB;

    uA.f32 = a;
    uB.f32 = b;
    return (uA.f < uB.f);

}

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

#ifdef FLOAT64

union f64_d { float64_t f64; double d; };

float64_t subj_ui32_to_f64( uint32_t a )
{
    union f64_d uZ;

    uZ.d = a;
    return uZ.f64;

}

float64_t subj_ui64_to_f64( uint64_t a )
{
    union f64_d uZ;

    uZ.d = a;
    return uZ.f64;

}

float64_t subj_i32_to_f64( int32_t a )
{
    union f64_d uZ;

    uZ.d = a;
    return uZ.f64;

}

float64_t subj_i64_to_f64( int64_t a )
{
    union f64_d uZ;

    uZ.d = a;
    return uZ.f64;

}

float64_t subj_f32_to_f64( float32_t a )
{
    union f32_f uA;
    union f64_d uZ;

    uA.f32 = a;
    uZ.d = uA.f;
    return uZ.f64;

}

uint_fast32_t subj_f64_to_ui32_rx_minMag( float64_t a )
{
    union f64_d uA;

    uA.f64 = a;
    return (uint32_t) uA.d;

}

uint_fast64_t subj_f64_to_ui64_rx_minMag( float64_t a )
{
    union f64_d uA;

    uA.f64 = a;
    return (uint64_t) uA.d;

}

int_fast32_t subj_f64_to_i32_rx_minMag( float64_t a )
{
    union f64_d uA;

    uA.f64 = a;
    return (int32_t) uA.d;

}

int_fast64_t subj_f64_to_i64_rx_minMag( float64_t a )
{
    union f64_d uA;

    uA.f64 = a;
    return (int64_t) uA.d;

}

float32_t subj_f64_to_f32( float64_t a )
{
    union f64_d uA;
    union f32_f uZ;

    uA.f64 = a;
    uZ.f = uA.d;
    return uZ.f32;

}

float64_t subj_f64_add( float64_t a, float64_t b )
{
    union f64_d uA, uB, uZ;

    uA.f64 = a;
    uB.f64 = b;
    uZ.d = uA.d + uB.d;
    return uZ.f64;

}

float64_t subj_f64_sub( float64_t a, float64_t b )
{
    union f64_d uA, uB, uZ;

    uA.f64 = a;
    uB.f64 = b;
    uZ.d = uA.d - uB.d;
    return uZ.f64;

}

float64_t subj_f64_mul( float64_t a, float64_t b )
{
    union f64_d uA, uB, uZ;

    uA.f64 = a;
    uB.f64 = b;
    uZ.d = uA.d * uB.d;
    return uZ.f64;

}

#ifdef __STDC_VERSION__
#if 199901L <= __STDC_VERSION__

float64_t subj_f64_mulAdd( float64_t a, float64_t b, float64_t c )
{
    union f64_d uA, uB, uC, uZ;

    uA.f64 = a;
    uB.f64 = b;
    uC.f64 = c;
    uZ.d = fma( uA.d, uB.d, uC.d );
    return uZ.f64;

}

#endif
#endif

float64_t subj_f64_div( float64_t a, float64_t b )
{
    union f64_d uA, uB, uZ;

    uA.f64 = a;
    uB.f64 = b;
    uZ.d = uA.d / uB.d;
    return uZ.f64;

}

float64_t subj_f64_sqrt( float64_t a )
{
    union f64_d uA, uZ;

    uA.f64 = a;
    uZ.d = sqrt( uA.d );
    return uZ.f64;

}

bool subj_f64_eq( float64_t a, float64_t b )
{
    union f64_d uA, uB;

    uA.f64 = a;
    uB.f64 = b;
    return (uA.d == uB.d);

}

bool subj_f64_le( float64_t a, float64_t b )
{
    union f64_d uA, uB;

    uA.f64 = a;
    uB.f64 = b;
    return (uA.d <= uB.d);

}

bool subj_f64_lt( float64_t a, float64_t b )
{
    union f64_d uA, uB;

    uA.f64 = a;
    uB.f64 = b;
    return (uA.d < uB.d);

}

#endif

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

#if defined EXTFLOAT80 && defined LONG_DOUBLE_IS_EXTFLOAT80

void subj_ui32_to_extF80M( uint32_t a, extFloat80_t *zPtr )
{

    *((long double *) zPtr) = a;

}

void subj_ui64_to_extF80M( uint64_t a, extFloat80_t *zPtr )
{

    *((long double *) zPtr) = a;

}

void subj_i32_to_extF80M( int32_t a, extFloat80_t *zPtr )
{

    *((long double *) zPtr) = a;

}

void subj_i64_to_extF80M( int64_t a, extFloat80_t *zPtr )
{

    *((long double *) zPtr) = a;

}

void subj_f32_to_extF80M( float32_t a, extFloat80_t *zPtr )
{
    union f32_f uA;

    uA.f32 = a;
    *((long double *) zPtr) = uA.f;

}

#ifdef FLOAT64

void subj_f64_to_extF80M( float64_t a, extFloat80_t *zPtr )
{
    union f64_d uA;

    uA.f64 = a;
    *((long double *) zPtr) = uA.d;

}

#endif

uint_fast32_t subj_extF80M_to_ui32_rx_minMag( const extFloat80_t *aPtr )
{

    return *((const long double *) aPtr);

}

uint_fast64_t subj_extF80M_to_ui64_rx_minMag( const extFloat80_t *aPtr )
{

    return *((const long double *) aPtr);

}

int_fast32_t subj_extF80M_to_i32_rx_minMag( const extFloat80_t *aPtr )
{

    return *((const long double *) aPtr);

}

int_fast64_t subj_extF80M_to_i64_rx_minMag( const extFloat80_t *aPtr )
{

    return *((const long double *) aPtr);

}

float32_t subj_extF80M_to_f32( const extFloat80_t *aPtr )
{
    union f32_f uZ;

    uZ.f = *((const long double *) aPtr);
    return uZ.f32;

}

#ifdef FLOAT64

float64_t subj_extF80M_to_f64( const extFloat80_t *aPtr )
{
    union f64_d uZ;

    uZ.d = *((const long double *) aPtr);
    return uZ.f64;

}

#endif

void
 subj_extF80M_add(
     const extFloat80_t *aPtr, const extFloat80_t *bPtr, extFloat80_t *zPtr )
{

    *((long double *) zPtr) =
        *((const long double *) aPtr) + *((const long double *) bPtr);

}

void
 subj_extF80M_sub(
     const extFloat80_t *aPtr, const extFloat80_t *bPtr, extFloat80_t *zPtr )
{

    *((long double *) zPtr) =
        *((const long double *) aPtr) - *((const long double *) bPtr);

}

void
 subj_extF80M_mul(
     const extFloat80_t *aPtr, const extFloat80_t *bPtr, extFloat80_t *zPtr )
{

    *((long double *) zPtr) =
        *((const long double *) aPtr) * *((const long double *) bPtr);

}

void
 subj_extF80M_div(
     const extFloat80_t *aPtr, const extFloat80_t *bPtr, extFloat80_t *zPtr )
{

    *((long double *) zPtr) =
        *((const long double *) aPtr) / *((const long double *) bPtr);

}

bool subj_extF80M_eq( const extFloat80_t *aPtr, const extFloat80_t *bPtr )
{

    return (*((const long double *) aPtr) == *((const long double *) bPtr));

}

bool subj_extF80M_le( const extFloat80_t *aPtr, const extFloat80_t *bPtr )
{

    return (*((const long double *) aPtr) <= *((const long double *) bPtr));

}

bool subj_extF80M_lt( const extFloat80_t *aPtr, const extFloat80_t *bPtr )
{

    return (*((const long double *) aPtr) < *((const long double *) bPtr));

}

#endif

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

#if defined FLOAT128 && defined LONG_DOUBLE_IS_FLOAT128

void subj_ui32_to_f128M( uint32_t a, float128_t *zPtr )
{

    *((long double *) zPtr) = a;

}

void subj_ui64_to_f128M( uint64_t a, float128_t *zPtr )
{

    *((long double *) zPtr) = a;

}

void subj_i32_to_f128M( int32_t a, float128_t *zPtr )
{

    *((long double *) zPtr) = a;

}

void subj_i64_to_f128M( int64_t a, float128_t *zPtr )
{

    *((long double *) zPtr) = a;

}

void subj_f32_to_f128M( float32_t a, float128_t *zPtr )
{
    union f32_f uA;

    uA.f32 = a;
    *((long double *) zPtr) = uA.f;

}

#ifdef FLOAT64

void subj_f64_to_f128M( float64_t a, float128_t *zPtr )
{
    union f64_d uA;

    uA.f64 = a;
    *((long double *) zPtr) = uA.d;

}

#endif

uint_fast32_t subj_f128M_to_ui32_rx_minMag( const float128_t *aPtr )
{

    return *((const long double *) aPtr);

}

uint_fast64_t subj_f128M_to_ui64_rx_minMag( const float128_t *aPtr )
{

    return *((const long double *) aPtr);

}

int_fast32_t subj_f128M_to_i32_rx_minMag( const float128_t *aPtr )
{

    return *((const long double *) aPtr);

}

int_fast64_t subj_f128M_to_i64_rx_minMag( const float128_t *aPtr )
{

    return *((const long double *) aPtr);

}

float32_t subj_f128M_to_f32( const float128_t *aPtr )
{
    union f32_f uZ;

    uZ.f = *((const long double *) aPtr);
    return uZ.f32;

}

#ifdef FLOAT64

float64_t subj_f128M_to_f64( const float128_t *aPtr )
{
    union f64_d uZ;

    uZ.d = *((const long double *) aPtr);
    return uZ.f64;

}

#endif

void
 subj_f128M_add(
     const float128_t *aPtr, const float128_t *bPtr, float128_t *zPtr )
{

    *((long double *) zPtr) =
        *((const long double *) aPtr) + *((const long double *) bPtr);

}

void
 subj_f128M_sub(
     const float128_t *aPtr, const float128_t *bPtr, float128_t *zPtr )
{

    *((long double *) zPtr) =
        *((const long double *) aPtr) - *((const long double *) bPtr);

}

void
 subj_f128M_mul(
     const float128_t *aPtr, const float128_t *bPtr, float128_t *zPtr )
{

    *((long double *) zPtr) =
        *((const long double *) aPtr) * *((const long double *) bPtr);

}

#ifdef __STDC_VERSION__
#if 199901L <= __STDC_VERSION__

void
 subj_f128M_mulAdd(
     const float128_t *aPtr,
     const float128_t *bPtr,
     const float128_t *cPtr,
     float128_t *zPtr
 )
{

    *((long double *) zPtr) =
        fmal(
            *((const long double *) aPtr),
            *((const long double *) bPtr),
            *((const long double *) cPtr)
        );

}

#endif
#endif

void
 subj_f128M_div(
     const float128_t *aPtr, const float128_t *bPtr, float128_t *zPtr )
{

    *((long double *) zPtr) =
        *((const long double *) aPtr) / *((const long double *) bPtr);

}

#ifdef __STDC_VERSION__
#if 199901L <= __STDC_VERSION__

void subj_f128M_sqrt( const float128_t *aPtr, float128_t *zPtr )
{

    *((long double *) zPtr) = sqrtl( *((const long double *) aPtr) );

}

#endif
#endif

bool subj_f128M_eq( const float128_t *aPtr, const float128_t *bPtr )
{

    return (*((const long double *) aPtr) == *((const long double *) bPtr));

}

bool subj_f128M_le( const float128_t *aPtr, const float128_t *bPtr )
{

    return (*((const long double *) aPtr) <= *((const long double *) bPtr));

}

bool subj_f128M_lt( const float128_t *aPtr, const float128_t *bPtr )
{

    return (*((const long double *) aPtr) < *((const long double *) bPtr));

}

#endif

