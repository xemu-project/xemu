
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

void writeCase_a_ui32( uint_fast32_t, const char * );
void writeCase_a_ui64( uint_fast64_t, const char * );
#define writeCase_a_i32 writeCase_a_ui32
#define writeCase_a_i64 writeCase_a_ui64
#ifdef FLOAT16
void writeCase_a_f16( float16_t );
void writeCase_ab_f16( float16_t, float16_t );
void writeCase_abc_f16( float16_t, float16_t, float16_t );
#endif
void writeCase_a_f32( float32_t, const char * );
void writeCase_ab_f32( float32_t, float32_t );
void writeCase_abc_f32( float32_t, float32_t, float32_t );
#ifdef FLOAT64
void writeCase_a_f64( float64_t, const char * );
void writeCase_ab_f64( float64_t, float64_t, const char * );
void writeCase_abc_f64( float64_t, float64_t, float64_t );
#endif
#ifdef EXTFLOAT80
void writeCase_a_extF80M( const extFloat80_t *, const char * );
void
 writeCase_ab_extF80M(
     const extFloat80_t *, const extFloat80_t *, const char * );
#endif
#ifdef FLOAT128
void writeCase_a_f128M( const float128_t *, const char * );
void writeCase_ab_f128M( const float128_t *, const float128_t * );
void
 writeCase_abc_f128M(
     const float128_t *, const float128_t *, const float128_t * );
#endif

void writeCase_z_bool( bool, uint_fast8_t, bool, uint_fast8_t );
void
 writeCase_z_ui32( uint_fast32_t, uint_fast8_t, uint_fast32_t, uint_fast8_t );
void
 writeCase_z_ui64( uint_fast64_t, uint_fast8_t, uint_fast64_t, uint_fast8_t );
#define writeCase_z_i32 writeCase_z_ui32
#define writeCase_z_i64 writeCase_z_ui64
#ifdef FLOAT16
void writeCase_z_f16( float16_t, uint_fast8_t, float16_t, uint_fast8_t );
#endif
void writeCase_z_f32( float32_t, uint_fast8_t, float32_t, uint_fast8_t );
#ifdef FLOAT64
void writeCase_z_f64( float64_t, uint_fast8_t, float64_t, uint_fast8_t );
#endif
#ifdef EXTFLOAT80
void
 writeCase_z_extF80M(
     const extFloat80_t *, uint_fast8_t, const extFloat80_t *, uint_fast8_t );
#endif
#ifdef FLOAT128
void
 writeCase_z_f128M(
     const float128_t *, uint_fast8_t, const float128_t *, uint_fast8_t );
#endif

