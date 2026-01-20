
/*============================================================================

This C header file is part of TestFloat, Release 3e, a package of programs for
testing the correctness of floating-point arithmetic complying with the IEEE
Standard for Floating-Point, by John R. Hauser.

Copyright 2011, 2012, 2013, 2014, 2015, 2017 The Regents of the University of
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
#include <stdio.h>
#include "softfloat.h"

extern bool testLoops_forever;

extern uint_fast8_t (*testLoops_trueFlagsFunction)( void );
extern uint_fast8_t (*testLoops_subjFlagsFunction)( void );

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/
#ifdef FLOAT16
void test_a_ui32_z_f16( float16_t ( uint32_t ), float16_t ( uint32_t ) );
#endif
void test_a_ui32_z_f32( float32_t ( uint32_t ), float32_t ( uint32_t ) );
#ifdef FLOAT64
void test_a_ui32_z_f64( float64_t ( uint32_t ), float64_t ( uint32_t ) );
#endif
#ifdef EXTFLOAT80
void
 test_a_ui32_z_extF80(
     void ( uint32_t, extFloat80_t * ), void ( uint32_t, extFloat80_t * ) );
#endif
#ifdef FLOAT128
void
 test_a_ui32_z_f128(
     void ( uint32_t, float128_t * ), void ( uint32_t, float128_t * )
 );
#endif
#ifdef FLOAT16
void test_a_ui64_z_f16( float16_t ( uint64_t ), float16_t ( uint64_t ) );
#endif
void test_a_ui64_z_f32( float32_t ( uint64_t ), float32_t ( uint64_t ) );
#ifdef FLOAT64
void test_a_ui64_z_f64( float64_t ( uint64_t ), float64_t ( uint64_t ) );
#endif
#ifdef EXTFLOAT80
void
 test_a_ui64_z_extF80(
     void ( uint64_t, extFloat80_t * ), void ( uint64_t, extFloat80_t * ) );
#endif
#ifdef FLOAT128
void
 test_a_ui64_z_f128(
     void ( uint64_t, float128_t * ), void ( uint64_t, float128_t * ) );
#endif
#ifdef FLOAT16
void test_a_i32_z_f16( float16_t ( int32_t ), float16_t ( int32_t ) );
#endif
void test_a_i32_z_f32( float32_t ( int32_t ), float32_t ( int32_t ) );
#ifdef FLOAT64
void test_a_i32_z_f64( float64_t ( int32_t ), float64_t ( int32_t ) );
#endif
#ifdef EXTFLOAT80
void
 test_a_i32_z_extF80(
     void ( int32_t, extFloat80_t * ), void ( int32_t, extFloat80_t * ) );
#endif
#ifdef FLOAT128
void
 test_a_i32_z_f128(
     void ( int32_t, float128_t * ), void ( int32_t, float128_t * ) );
#endif
#ifdef FLOAT16
void test_a_i64_z_f16( float16_t ( int64_t ), float16_t ( int64_t ) );
#endif
void test_a_i64_z_f32( float32_t ( int64_t ), float32_t ( int64_t ) );
#ifdef FLOAT64
void test_a_i64_z_f64( float64_t ( int64_t ), float64_t ( int64_t ) );
#endif
#ifdef EXTFLOAT80
void
 test_a_i64_z_extF80(
     void ( int64_t, extFloat80_t * ), void ( int64_t, extFloat80_t * ) );
#endif
#ifdef FLOAT128
void
 test_a_i64_z_f128(
     void ( int64_t, float128_t * ), void ( int64_t, float128_t * ) );
#endif

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/
#ifdef FLOAT16
void
 test_a_f16_z_ui32_rx(
     uint_fast32_t ( float16_t, uint_fast8_t, bool ),
     uint_fast32_t ( float16_t, uint_fast8_t, bool ),
     uint_fast8_t,
     bool
 );
void
 test_a_f16_z_ui64_rx(
     uint_fast64_t ( float16_t, uint_fast8_t, bool ),
     uint_fast64_t ( float16_t, uint_fast8_t, bool ),
     uint_fast8_t,
     bool
 );
void
 test_a_f16_z_i32_rx(
     int_fast32_t ( float16_t, uint_fast8_t, bool ),
     int_fast32_t ( float16_t, uint_fast8_t, bool ),
     uint_fast8_t,
     bool
 );
void
 test_a_f16_z_i64_rx(
     int_fast64_t ( float16_t, uint_fast8_t, bool ),
     int_fast64_t ( float16_t, uint_fast8_t, bool ),
     uint_fast8_t,
     bool
 );
void
 test_a_f16_z_ui32_x(
     uint_fast32_t ( float16_t, bool ), uint_fast32_t ( float16_t, bool ), bool
 );
void
 test_a_f16_z_ui64_x(
     uint_fast64_t ( float16_t, bool ), uint_fast64_t ( float16_t, bool ), bool
 );
void
 test_a_f16_z_i32_x(
     int_fast32_t ( float16_t, bool ), int_fast32_t ( float16_t, bool ), bool
 );
void
 test_a_f16_z_i64_x(
     int_fast64_t ( float16_t, bool ), int_fast64_t ( float16_t, bool ), bool
 );
void test_a_f16_z_f32( float32_t ( float16_t ), float32_t ( float16_t ) );
#ifdef FLOAT64
void test_a_f16_z_f64( float64_t ( float16_t ), float64_t ( float16_t ) );
#endif
#ifdef EXTFLOAT80
void
 test_a_f16_z_extF80(
     void ( float16_t, extFloat80_t * ), void ( float16_t, extFloat80_t * ) );
#endif
#ifdef FLOAT128
void
 test_a_f16_z_f128(
     void ( float16_t, float128_t * ), void ( float16_t, float128_t * ) );
#endif
void test_az_f16( float16_t ( float16_t ), float16_t ( float16_t ) );
void
 test_az_f16_rx(
     float16_t ( float16_t, uint_fast8_t, bool ),
     float16_t ( float16_t, uint_fast8_t, bool ),
     uint_fast8_t,
     bool
 );
void
 test_abz_f16(
     float16_t ( float16_t, float16_t ), float16_t ( float16_t, float16_t ) );
void
 test_abcz_f16(
     float16_t ( float16_t, float16_t, float16_t ),
     float16_t ( float16_t, float16_t, float16_t )
 );
void
 test_ab_f16_z_bool(
     bool ( float16_t, float16_t ), bool ( float16_t, float16_t ) );
#endif

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/
void
 test_a_f32_z_ui32_rx(
     uint_fast32_t ( float32_t, uint_fast8_t, bool ),
     uint_fast32_t ( float32_t, uint_fast8_t, bool ),
     uint_fast8_t,
     bool
 );
void
 test_a_f32_z_ui64_rx(
     uint_fast64_t ( float32_t, uint_fast8_t, bool ),
     uint_fast64_t ( float32_t, uint_fast8_t, bool ),
     uint_fast8_t,
     bool
 );
void
 test_a_f32_z_i32_rx(
     int_fast32_t ( float32_t, uint_fast8_t, bool ),
     int_fast32_t ( float32_t, uint_fast8_t, bool ),
     uint_fast8_t,
     bool
 );
void
 test_a_f32_z_i64_rx(
     int_fast64_t ( float32_t, uint_fast8_t, bool ),
     int_fast64_t ( float32_t, uint_fast8_t, bool ),
     uint_fast8_t,
     bool
 );
void
 test_a_f32_z_ui32_x(
     uint_fast32_t ( float32_t, bool ), uint_fast32_t ( float32_t, bool ), bool
 );
void
 test_a_f32_z_ui64_x(
     uint_fast64_t ( float32_t, bool ), uint_fast64_t ( float32_t, bool ), bool
 );
void
 test_a_f32_z_i32_x(
     int_fast32_t ( float32_t, bool ), int_fast32_t ( float32_t, bool ), bool
 );
void
 test_a_f32_z_i64_x(
     int_fast64_t ( float32_t, bool ), int_fast64_t ( float32_t, bool ), bool
 );
#ifdef FLOAT16
void test_a_f32_z_f16( float16_t ( float32_t ), float16_t ( float32_t ) );
#endif
#ifdef FLOAT64
void test_a_f32_z_f64( float64_t ( float32_t ), float64_t ( float32_t ) );
#endif
#ifdef EXTFLOAT80
void
 test_a_f32_z_extF80(
     void ( float32_t, extFloat80_t * ), void ( float32_t, extFloat80_t * ) );
#endif
#ifdef FLOAT128
void
 test_a_f32_z_f128(
     void ( float32_t, float128_t * ), void ( float32_t, float128_t * ) );
#endif
void test_az_f32( float32_t ( float32_t ), float32_t ( float32_t ) );
void
 test_az_f32_rx(
     float32_t ( float32_t, uint_fast8_t, bool ),
     float32_t ( float32_t, uint_fast8_t, bool ),
     uint_fast8_t,
     bool
 );
void
 test_abz_f32(
     float32_t ( float32_t, float32_t ), float32_t ( float32_t, float32_t ) );
void
 test_abcz_f32(
     float32_t ( float32_t, float32_t, float32_t ),
     float32_t ( float32_t, float32_t, float32_t )
 );
void
 test_ab_f32_z_bool(
     bool ( float32_t, float32_t ), bool ( float32_t, float32_t ) );

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/
#ifdef FLOAT64
void
 test_a_f64_z_ui32_rx(
     uint_fast32_t ( float64_t, uint_fast8_t, bool ),
     uint_fast32_t ( float64_t, uint_fast8_t, bool ),
     uint_fast8_t,
     bool
 );
void
 test_a_f64_z_ui64_rx(
     uint_fast64_t ( float64_t, uint_fast8_t, bool ),
     uint_fast64_t ( float64_t, uint_fast8_t, bool ),
     uint_fast8_t,
     bool
 );
void
 test_a_f64_z_i32_rx(
     int_fast32_t ( float64_t, uint_fast8_t, bool ),
     int_fast32_t ( float64_t, uint_fast8_t, bool ),
     uint_fast8_t,
     bool
 );
void
 test_a_f64_z_i64_rx(
     int_fast64_t ( float64_t, uint_fast8_t, bool ),
     int_fast64_t ( float64_t, uint_fast8_t, bool ),
     uint_fast8_t,
     bool
 );
void
 test_a_f64_z_ui32_x(
     uint_fast32_t ( float64_t, bool ), uint_fast32_t ( float64_t, bool ), bool
 );
void
 test_a_f64_z_ui64_x(
     uint_fast64_t ( float64_t, bool ), uint_fast64_t ( float64_t, bool ), bool
 );
void
 test_a_f64_z_i32_x(
     int_fast32_t ( float64_t, bool ), int_fast32_t ( float64_t, bool ), bool
 );
void
 test_a_f64_z_i64_x(
     int_fast64_t ( float64_t, bool ), int_fast64_t ( float64_t, bool ), bool
 );
#ifdef FLOAT16
void test_a_f64_z_f16( float16_t ( float64_t ), float16_t ( float64_t ) );
#endif
void test_a_f64_z_f32( float32_t ( float64_t ), float32_t ( float64_t ) );
#ifdef EXTFLOAT80
void
 test_a_f64_z_extF80(
     void ( float64_t, extFloat80_t * ), void ( float64_t, extFloat80_t * ) );
#endif
#ifdef FLOAT128
void
 test_a_f64_z_f128(
     void ( float64_t, float128_t * ), void ( float64_t, float128_t * ) );
#endif
void test_az_f64( float64_t ( float64_t ), float64_t ( float64_t ) );
void
 test_az_f64_rx(
     float64_t ( float64_t, uint_fast8_t, bool ),
     float64_t ( float64_t, uint_fast8_t, bool ),
     uint_fast8_t,
     bool
 );
void
 test_abz_f64(
     float64_t ( float64_t, float64_t ), float64_t ( float64_t, float64_t ) );
void
 test_abcz_f64(
     float64_t ( float64_t, float64_t, float64_t ),
     float64_t ( float64_t, float64_t, float64_t )
 );
void
 test_ab_f64_z_bool(
     bool ( float64_t, float64_t ), bool ( float64_t, float64_t ) );
#endif

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/
#ifdef EXTFLOAT80
void
 test_a_extF80_z_ui32_rx(
     uint_fast32_t ( const extFloat80_t *, uint_fast8_t, bool ),
     uint_fast32_t ( const extFloat80_t *, uint_fast8_t, bool ),
     uint_fast8_t,
     bool
 );
void
 test_a_extF80_z_ui64_rx(
     uint_fast64_t ( const extFloat80_t *, uint_fast8_t, bool ),
     uint_fast64_t ( const extFloat80_t *, uint_fast8_t, bool ),
     uint_fast8_t,
     bool
 );
void
 test_a_extF80_z_i32_rx(
     int_fast32_t ( const extFloat80_t *, uint_fast8_t, bool ),
     int_fast32_t ( const extFloat80_t *, uint_fast8_t, bool ),
     uint_fast8_t,
     bool
 );
void
 test_a_extF80_z_i64_rx(
     int_fast64_t ( const extFloat80_t *, uint_fast8_t, bool ),
     int_fast64_t ( const extFloat80_t *, uint_fast8_t, bool ),
     uint_fast8_t,
     bool
 );
void
 test_a_extF80_z_ui32_x(
     uint_fast32_t ( const extFloat80_t *, bool ),
     uint_fast32_t ( const extFloat80_t *, bool ),
     bool
 );
void
 test_a_extF80_z_ui64_x(
     uint_fast64_t ( const extFloat80_t *, bool ),
     uint_fast64_t ( const extFloat80_t *, bool ),
     bool
 );
void
 test_a_extF80_z_i32_x(
     int_fast32_t ( const extFloat80_t *, bool ),
     int_fast32_t ( const extFloat80_t *, bool ),
     bool
 );
void
 test_a_extF80_z_i64_x(
     int_fast64_t ( const extFloat80_t *, bool ),
     int_fast64_t ( const extFloat80_t *, bool ),
     bool
 );
#ifdef FLOAT16
void
 test_a_extF80_z_f16(
     float16_t ( const extFloat80_t * ), float16_t ( const extFloat80_t * ) );
#endif
void
 test_a_extF80_z_f32(
     float32_t ( const extFloat80_t * ), float32_t ( const extFloat80_t * ) );
#ifdef FLOAT64
void
 test_a_extF80_z_f64(
     float64_t ( const extFloat80_t * ), float64_t ( const extFloat80_t * ) );
#endif
#ifdef FLOAT128
void
 test_a_extF80_z_f128(
     void ( const extFloat80_t *, float128_t * ),
     void ( const extFloat80_t *, float128_t * )
 );
#endif
void
 test_az_extF80(
     void ( const extFloat80_t *, extFloat80_t * ),
     void ( const extFloat80_t *, extFloat80_t * )
 );
void
 test_az_extF80_rx(
     void ( const extFloat80_t *, uint_fast8_t, bool, extFloat80_t * ),
     void ( const extFloat80_t *, uint_fast8_t, bool, extFloat80_t * ),
     uint_fast8_t,
     bool
 );
void
 test_abz_extF80(
     void ( const extFloat80_t *, const extFloat80_t *, extFloat80_t * ),
     void ( const extFloat80_t *, const extFloat80_t *, extFloat80_t * )
 );
void
 test_ab_extF80_z_bool(
     bool ( const extFloat80_t *, const extFloat80_t * ),
     bool ( const extFloat80_t *, const extFloat80_t * )
 );
#endif

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/
#ifdef FLOAT128
void
 test_a_f128_z_ui32_rx(
     uint_fast32_t ( const float128_t *, uint_fast8_t, bool ),
     uint_fast32_t ( const float128_t *, uint_fast8_t, bool ),
     uint_fast8_t,
     bool
 );
void
 test_a_f128_z_ui64_rx(
     uint_fast64_t ( const float128_t *, uint_fast8_t, bool ),
     uint_fast64_t ( const float128_t *, uint_fast8_t, bool ),
     uint_fast8_t,
     bool
 );
void
 test_a_f128_z_i32_rx(
     int_fast32_t ( const float128_t *, uint_fast8_t, bool ),
     int_fast32_t ( const float128_t *, uint_fast8_t, bool ),
     uint_fast8_t,
     bool
 );
void
 test_a_f128_z_i64_rx(
     int_fast64_t ( const float128_t *, uint_fast8_t, bool ),
     int_fast64_t ( const float128_t *, uint_fast8_t, bool ),
     uint_fast8_t,
     bool
 );
void
 test_a_f128_z_ui32_x(
     uint_fast32_t ( const float128_t *, bool ),
     uint_fast32_t ( const float128_t *, bool ),
     bool
 );
void
 test_a_f128_z_ui64_x(
     uint_fast64_t ( const float128_t *, bool ),
     uint_fast64_t ( const float128_t *, bool ),
     bool
 );
void
 test_a_f128_z_i32_x(
     int_fast32_t ( const float128_t *, bool ),
     int_fast32_t ( const float128_t *, bool ),
     bool
 );
void
 test_a_f128_z_i64_x(
     int_fast64_t ( const float128_t *, bool ),
     int_fast64_t ( const float128_t *, bool ),
     bool
 );
#ifdef FLOAT16
void
 test_a_f128_z_f16(
     float16_t ( const float128_t * ), float16_t ( const float128_t * ) );
#endif
void
 test_a_f128_z_f32(
     float32_t ( const float128_t * ), float32_t ( const float128_t * ) );
#ifdef FLOAT64
void
 test_a_f128_z_f64(
     float64_t ( const float128_t * ), float64_t ( const float128_t * ) );
#endif
#ifdef EXTFLOAT80
void
 test_a_f128_z_extF80(
     void ( const float128_t *, extFloat80_t * ),
     void ( const float128_t *, extFloat80_t * )
 );
#endif
void
 test_az_f128(
     void ( const float128_t *, float128_t * ),
     void ( const float128_t *, float128_t * )
 );
void
 test_az_f128_rx(
     void ( const float128_t *, uint_fast8_t, bool, float128_t * ),
     void ( const float128_t *, uint_fast8_t, bool, float128_t * ),
     uint_fast8_t,
     bool
 );
void
 test_abz_f128(
     void ( const float128_t *, const float128_t *, float128_t * ),
     void ( const float128_t *, const float128_t *, float128_t * )
 );
void
 test_abcz_f128(
     void
      ( const float128_t *,
        const float128_t *,
        const float128_t *,
        float128_t *
      ),
     void
      ( const float128_t *,
        const float128_t *,
        const float128_t *,
        float128_t *
      )
 );
void
 test_ab_f128_z_bool(
     bool ( const float128_t *, const float128_t * ),
     bool ( const float128_t *, const float128_t * )
 );
#endif

