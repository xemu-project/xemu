
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

#include <stdbool.h>
#include <stdint.h>
#include "softfloat.h"

extern void (*const subjfloat_functions[])();

void subjfloat_setRoundingMode( uint_fast8_t );
void subjfloat_setExtF80RoundingPrecision( uint_fast8_t );
uint_fast8_t subjfloat_clearExceptionFlags( void );

/*----------------------------------------------------------------------------
| Subject function declarations.  (Many of these functions may not exist.)
| WARNING:
| This file should not normally be modified.  Use "subjfloat_config.h" to
| specify which of these functions actually exist.
*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/
#ifdef FLOAT16
float16_t subj_ui32_to_f16( uint32_t );
#endif
float32_t subj_ui32_to_f32( uint32_t );
#ifdef FLOAT64
float64_t subj_ui32_to_f64( uint32_t );
#endif
#ifdef EXTFLOAT80
void subj_ui32_to_extF80M( uint32_t, extFloat80_t * );
#endif
#ifdef FLOAT128
void subj_ui32_to_f128M( uint32_t, float128_t * );
#endif
#ifdef FLOAT16
float16_t subj_ui64_to_f16( uint64_t );
#endif
float32_t subj_ui64_to_f32( uint64_t );
#ifdef FLOAT64
float64_t subj_ui64_to_f64( uint64_t );
#endif
#ifdef EXTFLOAT80
void subj_ui64_to_extF80M( uint64_t, extFloat80_t * );
#endif
#ifdef FLOAT128
void subj_ui64_to_f128M( uint64_t, float128_t * );
#endif
#ifdef FLOAT16
float16_t subj_i32_to_f16( int32_t );
#endif
float32_t subj_i32_to_f32( int32_t );
#ifdef FLOAT64
float64_t subj_i32_to_f64( int32_t );
#endif
#ifdef EXTFLOAT80
void subj_i32_to_extF80M( int32_t, extFloat80_t * );
#endif
#ifdef FLOAT128
void subj_i32_to_f128M( int32_t, float128_t * );
#endif
#ifdef FLOAT16
float16_t subj_i64_to_f16( int64_t );
#endif
float32_t subj_i64_to_f32( int64_t );
#ifdef FLOAT64
float64_t subj_i64_to_f64( int64_t );
#endif
#ifdef EXTFLOAT80
void subj_i64_to_extF80M( int64_t, extFloat80_t * );
#endif
#ifdef FLOAT128
void subj_i64_to_f128M( int64_t, float128_t * );
#endif

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/
#ifdef FLOAT16

uint_fast32_t subj_f16_to_ui32_r_near_even( float16_t );
uint_fast32_t subj_f16_to_ui32_r_minMag( float16_t );
uint_fast32_t subj_f16_to_ui32_r_min( float16_t );
uint_fast32_t subj_f16_to_ui32_r_max( float16_t );
uint_fast32_t subj_f16_to_ui32_r_near_maxMag( float16_t );
uint_fast64_t subj_f16_to_ui64_r_near_even( float16_t );
uint_fast64_t subj_f16_to_ui64_r_minMag( float16_t );
uint_fast64_t subj_f16_to_ui64_r_min( float16_t );
uint_fast64_t subj_f16_to_ui64_r_max( float16_t );
uint_fast64_t subj_f16_to_ui64_r_near_maxMag( float16_t );
int_fast32_t subj_f16_to_i32_r_near_even( float16_t );
int_fast32_t subj_f16_to_i32_r_minMag( float16_t );
int_fast32_t subj_f16_to_i32_r_min( float16_t );
int_fast32_t subj_f16_to_i32_r_max( float16_t );
int_fast32_t subj_f16_to_i32_r_near_maxMag( float16_t );
int_fast64_t subj_f16_to_i64_r_near_even( float16_t );
int_fast64_t subj_f16_to_i64_r_minMag( float16_t );
int_fast64_t subj_f16_to_i64_r_min( float16_t );
int_fast64_t subj_f16_to_i64_r_max( float16_t );
int_fast64_t subj_f16_to_i64_r_near_maxMag( float16_t );

uint_fast32_t subj_f16_to_ui32_rx_near_even( float16_t );
uint_fast32_t subj_f16_to_ui32_rx_minMag( float16_t );
uint_fast32_t subj_f16_to_ui32_rx_min( float16_t );
uint_fast32_t subj_f16_to_ui32_rx_max( float16_t );
uint_fast32_t subj_f16_to_ui32_rx_near_maxMag( float16_t );
uint_fast64_t subj_f16_to_ui64_rx_near_even( float16_t );
uint_fast64_t subj_f16_to_ui64_rx_minMag( float16_t );
uint_fast64_t subj_f16_to_ui64_rx_min( float16_t );
uint_fast64_t subj_f16_to_ui64_rx_max( float16_t );
uint_fast64_t subj_f16_to_ui64_rx_near_maxMag( float16_t );
int_fast32_t subj_f16_to_i32_rx_near_even( float16_t );
int_fast32_t subj_f16_to_i32_rx_minMag( float16_t );
int_fast32_t subj_f16_to_i32_rx_min( float16_t );
int_fast32_t subj_f16_to_i32_rx_max( float16_t );
int_fast32_t subj_f16_to_i32_rx_near_maxMag( float16_t );
int_fast64_t subj_f16_to_i64_rx_near_even( float16_t );
int_fast64_t subj_f16_to_i64_rx_minMag( float16_t );
int_fast64_t subj_f16_to_i64_rx_min( float16_t );
int_fast64_t subj_f16_to_i64_rx_max( float16_t );
int_fast64_t subj_f16_to_i64_rx_near_maxMag( float16_t );

float32_t subj_f16_to_f32( float16_t );
#ifdef FLOAT64
float64_t subj_f16_to_f64( float16_t );
#endif
#ifdef EXTFLOAT80
void subj_f16_to_extF80M( float16_t, extFloat80_t * );
#endif
#ifdef FLOAT128
void subj_f16_to_f128M( float16_t, float128_t * );
#endif

float16_t subj_f16_roundToInt_r_near_even( float16_t );
float16_t subj_f16_roundToInt_r_minMag( float16_t );
float16_t subj_f16_roundToInt_r_min( float16_t );
float16_t subj_f16_roundToInt_r_max( float16_t );
float16_t subj_f16_roundToInt_r_near_maxMag( float16_t );
float16_t subj_f16_roundToInt_x( float16_t );
float16_t subj_f16_add( float16_t, float16_t );
float16_t subj_f16_sub( float16_t, float16_t );
float16_t subj_f16_mul( float16_t, float16_t );
float16_t subj_f16_mulAdd( float16_t, float16_t, float16_t );
float16_t subj_f16_div( float16_t, float16_t );
float16_t subj_f16_rem( float16_t, float16_t );
float16_t subj_f16_sqrt( float16_t );
bool subj_f16_eq( float16_t, float16_t );
bool subj_f16_le( float16_t, float16_t );
bool subj_f16_lt( float16_t, float16_t );
bool subj_f16_eq_signaling( float16_t, float16_t );
bool subj_f16_le_quiet( float16_t, float16_t );
bool subj_f16_lt_quiet( float16_t, float16_t );

#endif

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/
uint_fast32_t subj_f32_to_ui32_r_near_even( float32_t );
uint_fast32_t subj_f32_to_ui32_r_minMag( float32_t );
uint_fast32_t subj_f32_to_ui32_r_min( float32_t );
uint_fast32_t subj_f32_to_ui32_r_max( float32_t );
uint_fast32_t subj_f32_to_ui32_r_near_maxMag( float32_t );
uint_fast64_t subj_f32_to_ui64_r_near_even( float32_t );
uint_fast64_t subj_f32_to_ui64_r_minMag( float32_t );
uint_fast64_t subj_f32_to_ui64_r_min( float32_t );
uint_fast64_t subj_f32_to_ui64_r_max( float32_t );
uint_fast64_t subj_f32_to_ui64_r_near_maxMag( float32_t );
int_fast32_t subj_f32_to_i32_r_near_even( float32_t );
int_fast32_t subj_f32_to_i32_r_minMag( float32_t );
int_fast32_t subj_f32_to_i32_r_min( float32_t );
int_fast32_t subj_f32_to_i32_r_max( float32_t );
int_fast32_t subj_f32_to_i32_r_near_maxMag( float32_t );
int_fast64_t subj_f32_to_i64_r_near_even( float32_t );
int_fast64_t subj_f32_to_i64_r_minMag( float32_t );
int_fast64_t subj_f32_to_i64_r_min( float32_t );
int_fast64_t subj_f32_to_i64_r_max( float32_t );
int_fast64_t subj_f32_to_i64_r_near_maxMag( float32_t );

uint_fast32_t subj_f32_to_ui32_rx_near_even( float32_t );
uint_fast32_t subj_f32_to_ui32_rx_minMag( float32_t );
uint_fast32_t subj_f32_to_ui32_rx_min( float32_t );
uint_fast32_t subj_f32_to_ui32_rx_max( float32_t );
uint_fast32_t subj_f32_to_ui32_rx_near_maxMag( float32_t );
uint_fast64_t subj_f32_to_ui64_rx_near_even( float32_t );
uint_fast64_t subj_f32_to_ui64_rx_minMag( float32_t );
uint_fast64_t subj_f32_to_ui64_rx_min( float32_t );
uint_fast64_t subj_f32_to_ui64_rx_max( float32_t );
uint_fast64_t subj_f32_to_ui64_rx_near_maxMag( float32_t );
int_fast32_t subj_f32_to_i32_rx_near_even( float32_t );
int_fast32_t subj_f32_to_i32_rx_minMag( float32_t );
int_fast32_t subj_f32_to_i32_rx_min( float32_t );
int_fast32_t subj_f32_to_i32_rx_max( float32_t );
int_fast32_t subj_f32_to_i32_rx_near_maxMag( float32_t );
int_fast64_t subj_f32_to_i64_rx_near_even( float32_t );
int_fast64_t subj_f32_to_i64_rx_minMag( float32_t );
int_fast64_t subj_f32_to_i64_rx_min( float32_t );
int_fast64_t subj_f32_to_i64_rx_max( float32_t );
int_fast64_t subj_f32_to_i64_rx_near_maxMag( float32_t );

#ifdef FLOAT16
float16_t subj_f32_to_f16( float32_t );
#endif
#ifdef FLOAT64
float64_t subj_f32_to_f64( float32_t );
#endif
#ifdef EXTFLOAT80
void subj_f32_to_extF80M( float32_t, extFloat80_t * );
#endif
#ifdef FLOAT128
void subj_f32_to_f128M( float32_t, float128_t * );
#endif

float32_t subj_f32_roundToInt_r_near_even( float32_t );
float32_t subj_f32_roundToInt_r_minMag( float32_t );
float32_t subj_f32_roundToInt_r_min( float32_t );
float32_t subj_f32_roundToInt_r_max( float32_t );
float32_t subj_f32_roundToInt_r_near_maxMag( float32_t );
float32_t subj_f32_roundToInt_x( float32_t );
float32_t subj_f32_add( float32_t, float32_t );
float32_t subj_f32_sub( float32_t, float32_t );
float32_t subj_f32_mul( float32_t, float32_t );
float32_t subj_f32_mulAdd( float32_t, float32_t, float32_t );
float32_t subj_f32_div( float32_t, float32_t );
float32_t subj_f32_rem( float32_t, float32_t );
float32_t subj_f32_sqrt( float32_t );
bool subj_f32_eq( float32_t, float32_t );
bool subj_f32_le( float32_t, float32_t );
bool subj_f32_lt( float32_t, float32_t );
bool subj_f32_eq_signaling( float32_t, float32_t );
bool subj_f32_le_quiet( float32_t, float32_t );
bool subj_f32_lt_quiet( float32_t, float32_t );

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/
#ifdef FLOAT64

uint_fast32_t subj_f64_to_ui32_r_near_even( float64_t );
uint_fast32_t subj_f64_to_ui32_r_minMag( float64_t );
uint_fast32_t subj_f64_to_ui32_r_min( float64_t );
uint_fast32_t subj_f64_to_ui32_r_max( float64_t );
uint_fast32_t subj_f64_to_ui32_r_near_maxMag( float64_t );
uint_fast64_t subj_f64_to_ui64_r_near_even( float64_t );
uint_fast64_t subj_f64_to_ui64_r_minMag( float64_t );
uint_fast64_t subj_f64_to_ui64_r_min( float64_t );
uint_fast64_t subj_f64_to_ui64_r_max( float64_t );
uint_fast64_t subj_f64_to_ui64_r_near_maxMag( float64_t );
int_fast32_t subj_f64_to_i32_r_near_even( float64_t );
int_fast32_t subj_f64_to_i32_r_minMag( float64_t );
int_fast32_t subj_f64_to_i32_r_min( float64_t );
int_fast32_t subj_f64_to_i32_r_max( float64_t );
int_fast32_t subj_f64_to_i32_r_near_maxMag( float64_t );
int_fast64_t subj_f64_to_i64_r_near_even( float64_t );
int_fast64_t subj_f64_to_i64_r_minMag( float64_t );
int_fast64_t subj_f64_to_i64_r_min( float64_t );
int_fast64_t subj_f64_to_i64_r_max( float64_t );
int_fast64_t subj_f64_to_i64_r_near_maxMag( float64_t );

uint_fast32_t subj_f64_to_ui32_rx_near_even( float64_t );
uint_fast32_t subj_f64_to_ui32_rx_minMag( float64_t );
uint_fast32_t subj_f64_to_ui32_rx_min( float64_t );
uint_fast32_t subj_f64_to_ui32_rx_max( float64_t );
uint_fast32_t subj_f64_to_ui32_rx_near_maxMag( float64_t );
uint_fast64_t subj_f64_to_ui64_rx_near_even( float64_t );
uint_fast64_t subj_f64_to_ui64_rx_minMag( float64_t );
uint_fast64_t subj_f64_to_ui64_rx_min( float64_t );
uint_fast64_t subj_f64_to_ui64_rx_max( float64_t );
uint_fast64_t subj_f64_to_ui64_rx_near_maxMag( float64_t );
int_fast32_t subj_f64_to_i32_rx_near_even( float64_t );
int_fast32_t subj_f64_to_i32_rx_minMag( float64_t );
int_fast32_t subj_f64_to_i32_rx_min( float64_t );
int_fast32_t subj_f64_to_i32_rx_max( float64_t );
int_fast32_t subj_f64_to_i32_rx_near_maxMag( float64_t );
int_fast64_t subj_f64_to_i64_rx_near_even( float64_t );
int_fast64_t subj_f64_to_i64_rx_minMag( float64_t );
int_fast64_t subj_f64_to_i64_rx_min( float64_t );
int_fast64_t subj_f64_to_i64_rx_max( float64_t );
int_fast64_t subj_f64_to_i64_rx_near_maxMag( float64_t );

#ifdef FLOAT16
float16_t subj_f64_to_f16( float64_t );
#endif
float32_t subj_f64_to_f32( float64_t );
#ifdef EXTFLOAT80
void subj_f64_to_extF80M( float64_t, extFloat80_t * );
#endif
#ifdef FLOAT128
void subj_f64_to_f128M( float64_t, float128_t * );
#endif

float64_t subj_f64_roundToInt_r_near_even( float64_t );
float64_t subj_f64_roundToInt_r_minMag( float64_t );
float64_t subj_f64_roundToInt_r_min( float64_t );
float64_t subj_f64_roundToInt_r_max( float64_t );
float64_t subj_f64_roundToInt_r_near_maxMag( float64_t );
float64_t subj_f64_roundToInt_x( float64_t );
float64_t subj_f64_add( float64_t, float64_t );
float64_t subj_f64_sub( float64_t, float64_t );
float64_t subj_f64_mul( float64_t, float64_t );
float64_t subj_f64_mulAdd( float64_t, float64_t, float64_t );
float64_t subj_f64_div( float64_t, float64_t );
float64_t subj_f64_rem( float64_t, float64_t );
float64_t subj_f64_sqrt( float64_t );
bool subj_f64_eq( float64_t, float64_t );
bool subj_f64_le( float64_t, float64_t );
bool subj_f64_lt( float64_t, float64_t );
bool subj_f64_eq_signaling( float64_t, float64_t );
bool subj_f64_le_quiet( float64_t, float64_t );
bool subj_f64_lt_quiet( float64_t, float64_t );

#endif

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/
#ifdef EXTFLOAT80

uint_fast32_t subj_extF80M_to_ui32_r_near_even( const extFloat80_t * );
uint_fast32_t subj_extF80M_to_ui32_r_minMag( const extFloat80_t * );
uint_fast32_t subj_extF80M_to_ui32_r_min( const extFloat80_t * );
uint_fast32_t subj_extF80M_to_ui32_r_max( const extFloat80_t * );
uint_fast32_t subj_extF80M_to_ui32_r_near_maxMag( const extFloat80_t * );
uint_fast64_t subj_extF80M_to_ui64_r_near_even( const extFloat80_t * );
uint_fast64_t subj_extF80M_to_ui64_r_minMag( const extFloat80_t * );
uint_fast64_t subj_extF80M_to_ui64_r_min( const extFloat80_t * );
uint_fast64_t subj_extF80M_to_ui64_r_max( const extFloat80_t * );
uint_fast64_t subj_extF80M_to_ui64_r_near_maxMag( const extFloat80_t * );
int_fast32_t subj_extF80M_to_i32_r_near_even( const extFloat80_t * );
int_fast32_t subj_extF80M_to_i32_r_minMag( const extFloat80_t * );
int_fast32_t subj_extF80M_to_i32_r_min( const extFloat80_t * );
int_fast32_t subj_extF80M_to_i32_r_max( const extFloat80_t * );
int_fast32_t subj_extF80M_to_i32_r_near_maxMag( const extFloat80_t * );
int_fast64_t subj_extF80M_to_i64_r_near_even( const extFloat80_t * );
int_fast64_t subj_extF80M_to_i64_r_minMag( const extFloat80_t * );
int_fast64_t subj_extF80M_to_i64_r_min( const extFloat80_t * );
int_fast64_t subj_extF80M_to_i64_r_max( const extFloat80_t * );
int_fast64_t subj_extF80M_to_i64_r_near_maxMag( const extFloat80_t * );

uint_fast32_t subj_extF80M_to_ui32_rx_near_even( const extFloat80_t * );
uint_fast32_t subj_extF80M_to_ui32_rx_minMag( const extFloat80_t * );
uint_fast32_t subj_extF80M_to_ui32_rx_min( const extFloat80_t * );
uint_fast32_t subj_extF80M_to_ui32_rx_max( const extFloat80_t * );
uint_fast32_t subj_extF80M_to_ui32_rx_near_maxMag( const extFloat80_t * );
uint_fast64_t subj_extF80M_to_ui64_rx_near_even( const extFloat80_t * );
uint_fast64_t subj_extF80M_to_ui64_rx_minMag( const extFloat80_t * );
uint_fast64_t subj_extF80M_to_ui64_rx_min( const extFloat80_t * );
uint_fast64_t subj_extF80M_to_ui64_rx_max( const extFloat80_t * );
uint_fast64_t subj_extF80M_to_ui64_rx_near_maxMag( const extFloat80_t * );
int_fast32_t subj_extF80M_to_i32_rx_near_even( const extFloat80_t * );
int_fast32_t subj_extF80M_to_i32_rx_minMag( const extFloat80_t * );
int_fast32_t subj_extF80M_to_i32_rx_min( const extFloat80_t * );
int_fast32_t subj_extF80M_to_i32_rx_max( const extFloat80_t * );
int_fast32_t subj_extF80M_to_i32_rx_near_maxMag( const extFloat80_t * );
int_fast64_t subj_extF80M_to_i64_rx_near_even( const extFloat80_t * );
int_fast64_t subj_extF80M_to_i64_rx_minMag( const extFloat80_t * );
int_fast64_t subj_extF80M_to_i64_rx_min( const extFloat80_t * );
int_fast64_t subj_extF80M_to_i64_rx_max( const extFloat80_t * );
int_fast64_t subj_extF80M_to_i64_rx_near_maxMag( const extFloat80_t * );

#ifdef FLOAT16
float16_t subj_extF80M_to_f16( const extFloat80_t * );
#endif
float32_t subj_extF80M_to_f32( const extFloat80_t * );
#ifdef FLOAT64
float64_t subj_extF80M_to_f64( const extFloat80_t * );
#endif
#ifdef EXTFLOAT80
void subj_extF80M_to_f128M( const extFloat80_t *, float128_t * );
#endif

void
 subj_extF80M_roundToInt_r_near_even( const extFloat80_t *, extFloat80_t * );
void subj_extF80M_roundToInt_r_minMag( const extFloat80_t *, extFloat80_t * );
void subj_extF80M_roundToInt_r_min( const extFloat80_t *, extFloat80_t * );
void subj_extF80M_roundToInt_r_max( const extFloat80_t *, extFloat80_t * );
void
 subj_extF80M_roundToInt_r_near_maxMag( const extFloat80_t *, extFloat80_t * );
void subj_extF80M_roundToInt_x( const extFloat80_t *, extFloat80_t * );
void
 subj_extF80M_add(
     const extFloat80_t *, const extFloat80_t *, extFloat80_t * );
void
 subj_extF80M_sub(
     const extFloat80_t *, const extFloat80_t *, extFloat80_t * );
void
 subj_extF80M_mul(
     const extFloat80_t *, const extFloat80_t *, extFloat80_t * );
void
 subj_extF80M_div(
     const extFloat80_t *, const extFloat80_t *, extFloat80_t * );
void
 subj_extF80M_rem(
     const extFloat80_t *, const extFloat80_t *, extFloat80_t * );
void subj_extF80M_sqrt( const extFloat80_t *, extFloat80_t * );
bool subj_extF80M_eq( const extFloat80_t *, const extFloat80_t * );
bool subj_extF80M_le( const extFloat80_t *, const extFloat80_t * );
bool subj_extF80M_lt( const extFloat80_t *, const extFloat80_t * );
bool subj_extF80M_eq_signaling( const extFloat80_t *, const extFloat80_t * );
bool subj_extF80M_le_quiet( const extFloat80_t *, const extFloat80_t * );
bool subj_extF80M_lt_quiet( const extFloat80_t *, const extFloat80_t * );

#endif

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/
#ifdef FLOAT128

uint_fast32_t subj_f128M_to_ui32_r_near_even( const float128_t * );
uint_fast32_t subj_f128M_to_ui32_r_minMag( const float128_t * );
uint_fast32_t subj_f128M_to_ui32_r_min( const float128_t * );
uint_fast32_t subj_f128M_to_ui32_r_max( const float128_t * );
uint_fast32_t subj_f128M_to_ui32_r_near_maxMag( extFloat80_t * );
uint_fast64_t subj_f128M_to_ui64_r_near_even( const float128_t * );
uint_fast64_t subj_f128M_to_ui64_r_minMag( const float128_t * );
uint_fast64_t subj_f128M_to_ui64_r_min( const float128_t * );
uint_fast64_t subj_f128M_to_ui64_r_max( const float128_t * );
uint_fast64_t subj_f128M_to_ui64_r_near_maxMag( extFloat80_t * );
int_fast32_t subj_f128M_to_i32_r_near_even( const float128_t * );
int_fast32_t subj_f128M_to_i32_r_minMag( const float128_t * );
int_fast32_t subj_f128M_to_i32_r_min( const float128_t * );
int_fast32_t subj_f128M_to_i32_r_max( const float128_t * );
int_fast32_t subj_f128M_to_i32_r_near_maxMag( extFloat80_t * );
int_fast64_t subj_f128M_to_i64_r_near_even( const float128_t * );
int_fast64_t subj_f128M_to_i64_r_minMag( const float128_t * );
int_fast64_t subj_f128M_to_i64_r_min( const float128_t * );
int_fast64_t subj_f128M_to_i64_r_max( const float128_t * );
int_fast64_t subj_f128M_to_i64_r_near_maxMag( extFloat80_t * );

uint_fast32_t subj_f128M_to_ui32_rx_near_even( const float128_t * );
uint_fast32_t subj_f128M_to_ui32_rx_minMag( const float128_t * );
uint_fast32_t subj_f128M_to_ui32_rx_min( const float128_t * );
uint_fast32_t subj_f128M_to_ui32_rx_max( const float128_t * );
uint_fast32_t subj_f128M_to_ui32_rx_near_maxMag( extFloat80_t * );
uint_fast64_t subj_f128M_to_ui64_rx_near_even( const float128_t * );
uint_fast64_t subj_f128M_to_ui64_rx_minMag( const float128_t * );
uint_fast64_t subj_f128M_to_ui64_rx_min( const float128_t * );
uint_fast64_t subj_f128M_to_ui64_rx_max( const float128_t * );
uint_fast64_t subj_f128M_to_ui64_rx_near_maxMag( extFloat80_t * );
int_fast32_t subj_f128M_to_i32_rx_near_even( const float128_t * );
int_fast32_t subj_f128M_to_i32_rx_minMag( const float128_t * );
int_fast32_t subj_f128M_to_i32_rx_min( const float128_t * );
int_fast32_t subj_f128M_to_i32_rx_max( const float128_t * );
int_fast32_t subj_f128M_to_i32_rx_near_maxMag( extFloat80_t * );
int_fast64_t subj_f128M_to_i64_rx_near_even( const float128_t * );
int_fast64_t subj_f128M_to_i64_rx_minMag( const float128_t * );
int_fast64_t subj_f128M_to_i64_rx_min( const float128_t * );
int_fast64_t subj_f128M_to_i64_rx_max( const float128_t * );
int_fast64_t subj_f128M_to_i64_rx_near_maxMag( extFloat80_t * );

#ifdef FLOAT16
float16_t subj_f128M_to_f16( const float128_t * );
#endif
float32_t subj_f128M_to_f32( const float128_t * );
#ifdef FLOAT64
float64_t subj_f128M_to_f64( const float128_t * );
#endif
#ifdef FLOAT128
void subj_f128M_to_extF80M( const float128_t *, extFloat80_t * );
#endif

void subj_f128M_roundToInt_r_near_even( const float128_t, float128_t * );
void subj_f128M_roundToInt_r_minMag( const float128_t, float128_t * );
void subj_f128M_roundToInt_r_min( const float128_t, float128_t * );
void subj_f128M_roundToInt_r_max( const float128_t, float128_t * );
void subj_f128M_roundToInt_r_near_maxMag( const float128_t, float128_t * );
void subj_f128M_roundToInt_x( const float128_t, float128_t * );
void subj_f128M_add( const float128_t *, const float128_t *, float128_t * );
void subj_f128M_sub( const float128_t *, const float128_t *, float128_t * );
void subj_f128M_mul( const float128_t *, const float128_t *, float128_t * );
void
 subj_f128M_mulAdd(
     const float128_t *, const float128_t *, const float128_t *, float128_t *
 );
void subj_f128M_div( const float128_t *, const float128_t *, float128_t * );
void subj_f128M_rem( const float128_t *, const float128_t *, float128_t * );
void subj_f128M_sqrt( const float128_t *, float128_t * );
bool subj_f128M_eq( const float128_t *, const float128_t * );
bool subj_f128M_le( const float128_t *, const float128_t * );
bool subj_f128M_lt( const float128_t *, const float128_t * );
bool subj_f128M_eq_signaling( const float128_t *, const float128_t * );
bool subj_f128M_le_quiet( const float128_t *, const float128_t * );
bool subj_f128M_lt_quiet( const float128_t *, const float128_t * );

#endif

