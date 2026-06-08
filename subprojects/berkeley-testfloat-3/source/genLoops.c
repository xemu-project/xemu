
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
#include <stdio.h>
#include <signal.h>
#include "platform.h"
#include "uint128.h"
#include "fail.h"
#include "softfloat.h"
#include "genCases.h"
#include "writeHex.h"
#include "genLoops.h"

volatile sig_atomic_t genLoops_stop = false;

bool genLoops_forever;
bool genLoops_givenCount;
uint_fast64_t genLoops_count;
uint_fast8_t *genLoops_trueFlagsPtr;

#ifdef FLOAT16
union ui16_f16 { uint16_t ui; float16_t f; };
#endif
union ui32_f32 { uint32_t ui; float32_t f; };
#ifdef FLOAT64
union ui64_f64 { uint64_t ui; float64_t f; };
#endif

static void checkEnoughCases( void )
{

    if ( genLoops_givenCount && (genLoops_count < genCases_total) ) {
        if ( 2000000000 <= genCases_total ) {
            fail(
                "Too few cases; minimum is %lu%09lu",
                (unsigned long) (genCases_total / 1000000000),
                (unsigned long) (genCases_total % 1000000000)
            );
        } else {
            fail(
                "Too few cases; minimum is %lu", (unsigned long) genCases_total
            );
        }
    }

}

static void writeGenOutput_flags( uint_fast8_t flags )
{
    uint_fast8_t commonFlags;

    commonFlags = 0;
    if ( flags & softfloat_flag_invalid   ) commonFlags |= 0x10;
    if ( flags & softfloat_flag_infinite  ) commonFlags |= 0x08;
    if ( flags & softfloat_flag_overflow  ) commonFlags |= 0x04;
    if ( flags & softfloat_flag_underflow ) commonFlags |= 0x02;
    if ( flags & softfloat_flag_inexact   ) commonFlags |= 0x01;
    writeHex_ui8( commonFlags, '\n' );

}

static bool writeGenOutputs_bool( bool z, uint_fast8_t flags )
{

    writeHex_bool( z, ' ' );
    writeGenOutput_flags( flags );
    if ( genLoops_givenCount ) {
        --genLoops_count;
        if ( ! genLoops_count ) return true;
    }
    return false;

}

#ifdef FLOAT16

static bool writeGenOutputs_ui16( uint_fast16_t z, uint_fast8_t flags )
{

    writeHex_ui16( z, ' ' );
    writeGenOutput_flags( flags );
    if ( genLoops_givenCount ) {
        --genLoops_count;
        if ( ! genLoops_count ) return true;
    }
    return false;

}

#endif

static bool writeGenOutputs_ui32( uint_fast32_t z, uint_fast8_t flags )
{

    writeHex_ui32( z, ' ' );
    writeGenOutput_flags( flags );
    if ( genLoops_givenCount ) {
        --genLoops_count;
        if ( ! genLoops_count ) return true;
    }
    return false;

}

static bool writeGenOutputs_ui64( uint_fast64_t z, uint_fast8_t flags )
{

    writeHex_ui64( z, ' ' );
    writeGenOutput_flags( flags );
    if ( genLoops_givenCount ) {
        --genLoops_count;
        if ( ! genLoops_count ) return true;
    }
    return false;

}

#ifdef EXTFLOAT80

static void writeHex_uiExtF80M( const extFloat80_t *aPtr, char sepChar )
{
    const struct extFloat80M *aSPtr;

    aSPtr = (const struct extFloat80M *) aPtr;
    writeHex_ui16( aSPtr->signExp, 0 );
    writeHex_ui64( aSPtr->signif, sepChar );

}

static
bool writeGenOutputs_extF80M( const extFloat80_t *aPtr, uint_fast8_t flags )
{

    writeHex_uiExtF80M( aPtr, ' ' );
    writeGenOutput_flags( flags );
    if ( genLoops_givenCount ) {
        --genLoops_count;
        if ( ! genLoops_count ) return true;
    }
    return false;

}

#endif

#ifdef FLOAT128

static void writeHex_uiF128M( const float128_t *aPtr, char sepChar )
{
    const struct uint128 *uiAPtr;

    uiAPtr = (const struct uint128 *) aPtr;
    writeHex_ui64( uiAPtr->v64, 0 );
    writeHex_ui64( uiAPtr->v0, sepChar );

}

static bool writeGenOutputs_f128M( const float128_t *aPtr, uint_fast8_t flags )
{

    writeHex_uiF128M( aPtr, ' ' );
    writeGenOutput_flags( flags );
    if ( genLoops_givenCount ) {
        --genLoops_count;
        if ( ! genLoops_count ) return true;
    }
    return false;

}

#endif

void gen_a_ui32( void )
{

    genCases_ui32_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_ui32_a_next();
        writeHex_ui32( genCases_ui32_a, '\n' );
        if ( genLoops_givenCount ) {
            --genLoops_count;
            if ( ! genLoops_count ) break;
        }
    }

}

void gen_a_ui64( void )
{

    genCases_ui64_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_ui64_a_next();
        writeHex_ui64( genCases_ui64_a, '\n' );
        if ( genLoops_givenCount ) {
            --genLoops_count;
            if ( ! genLoops_count ) break;
        }
    }

}

void gen_a_i32( void )
{

    genCases_i32_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_i32_a_next();
        writeHex_ui32( genCases_i32_a, '\n' );
        if ( genLoops_givenCount ) {
            --genLoops_count;
            if ( ! genLoops_count ) break;
        }
    }

}

void gen_a_i64( void )
{

    genCases_i64_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_i64_a_next();
        writeHex_ui64( genCases_i64_a, '\n' );
        if ( genLoops_givenCount ) {
            --genLoops_count;
            if ( ! genLoops_count ) break;
        }
    }

}

#ifdef FLOAT16

void gen_a_f16( void )
{
    union ui16_f16 uA;

    genCases_f16_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f16_a_next();
        uA.f = genCases_f16_a;
        writeHex_ui16( uA.ui, '\n' );
        if ( genLoops_givenCount ) {
            --genLoops_count;
            if ( ! genLoops_count ) break;
        }
    }

}

void gen_ab_f16( void )
{
    union ui16_f16 u;

    genCases_f16_ab_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f16_ab_next();
        u.f = genCases_f16_a;
        writeHex_ui16( u.ui, ' ' );
        u.f = genCases_f16_b;
        writeHex_ui16( u.ui, '\n' );
        if ( genLoops_givenCount ) {
            --genLoops_count;
            if ( ! genLoops_count ) break;
        }
    }

}

void gen_abc_f16( void )
{
    union ui16_f16 u;

    genCases_f16_abc_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f16_abc_next();
        u.f = genCases_f16_a;
        writeHex_ui16( u.ui, ' ' );
        u.f = genCases_f16_b;
        writeHex_ui16( u.ui, ' ' );
        u.f = genCases_f16_c;
        writeHex_ui16( u.ui, '\n' );
        if ( genLoops_givenCount ) {
            --genLoops_count;
            if ( ! genLoops_count ) break;
        }
    }

}

#endif

void gen_a_f32( void )
{
    union ui32_f32 uA;

    genCases_f32_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f32_a_next();
        uA.f = genCases_f32_a;
        writeHex_ui32( uA.ui, '\n' );
        if ( genLoops_givenCount ) {
            --genLoops_count;
            if ( ! genLoops_count ) break;
        }
    }

}

void gen_ab_f32( void )
{
    union ui32_f32 u;

    genCases_f32_ab_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f32_ab_next();
        u.f = genCases_f32_a;
        writeHex_ui32( u.ui, ' ' );
        u.f = genCases_f32_b;
        writeHex_ui32( u.ui, '\n' );
        if ( genLoops_givenCount ) {
            --genLoops_count;
            if ( ! genLoops_count ) break;
        }
    }

}

void gen_abc_f32( void )
{
    union ui32_f32 u;

    genCases_f32_abc_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f32_abc_next();
        u.f = genCases_f32_a;
        writeHex_ui32( u.ui, ' ' );
        u.f = genCases_f32_b;
        writeHex_ui32( u.ui, ' ' );
        u.f = genCases_f32_c;
        writeHex_ui32( u.ui, '\n' );
        if ( genLoops_givenCount ) {
            --genLoops_count;
            if ( ! genLoops_count ) break;
        }
    }

}

#ifdef FLOAT64

void gen_a_f64( void )
{
    union ui64_f64 uA;

    genCases_f64_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f64_a_next();
        uA.f = genCases_f64_a;
        writeHex_ui64( uA.ui, '\n' );
        if ( genLoops_givenCount ) {
            --genLoops_count;
            if ( ! genLoops_count ) break;
        }
    }

}

void gen_ab_f64( void )
{
    union ui64_f64 u;

    genCases_f64_ab_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f64_ab_next();
        u.f = genCases_f64_a;
        writeHex_ui64( u.ui, ' ' );
        u.f = genCases_f64_b;
        writeHex_ui64( u.ui, '\n' );
        if ( genLoops_givenCount ) {
            --genLoops_count;
            if ( ! genLoops_count ) break;
        }
    }

}

void gen_abc_f64( void )
{
    union ui64_f64 u;

    genCases_f64_abc_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f64_abc_next();
        u.f = genCases_f64_a;
        writeHex_ui64( u.ui, ' ' );
        u.f = genCases_f64_b;
        writeHex_ui64( u.ui, ' ' );
        u.f = genCases_f64_c;
        writeHex_ui64( u.ui, '\n' );
        if ( genLoops_givenCount ) {
            --genLoops_count;
            if ( ! genLoops_count ) break;
        }
    }

}

#endif

#ifdef EXTFLOAT80

void gen_a_extF80( void )
{

    genCases_extF80_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_extF80_a_next();
        writeHex_uiExtF80M( &genCases_extF80_a, '\n' );
        if ( genLoops_givenCount ) {
            --genLoops_count;
            if ( ! genLoops_count ) break;
        }
    }

}

void gen_ab_extF80( void )
{

    genCases_extF80_ab_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_extF80_ab_next();
        writeHex_uiExtF80M( &genCases_extF80_a, ' ' );
        writeHex_uiExtF80M( &genCases_extF80_b, '\n' );
        if ( genLoops_givenCount ) {
            --genLoops_count;
            if ( ! genLoops_count ) break;
        }
    }

}

void gen_abc_extF80( void )
{

    genCases_extF80_abc_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_extF80_abc_next();
        writeHex_uiExtF80M( &genCases_extF80_a, ' ' );
        writeHex_uiExtF80M( &genCases_extF80_b, ' ' );
        writeHex_uiExtF80M( &genCases_extF80_c, '\n' );
        if ( genLoops_givenCount ) {
            --genLoops_count;
            if ( ! genLoops_count ) break;
        }
    }

}

#endif

#ifdef FLOAT128

void gen_a_f128( void )
{

    genCases_f128_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f128_a_next();
        writeHex_uiF128M( &genCases_f128_a, '\n' );
        if ( genLoops_givenCount ) {
            --genLoops_count;
            if ( ! genLoops_count ) break;
        }
    }

}

void gen_ab_f128( void )
{

    genCases_f128_ab_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f128_ab_next();
        writeHex_uiF128M( &genCases_f128_a, ' ' );
        writeHex_uiF128M( &genCases_f128_b, '\n' );
        if ( genLoops_givenCount ) {
            --genLoops_count;
            if ( ! genLoops_count ) break;
        }
    }

}

void gen_abc_f128( void )
{

    genCases_f128_abc_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f128_abc_next();
        writeHex_uiF128M( &genCases_f128_a, ' ' );
        writeHex_uiF128M( &genCases_f128_b, ' ' );
        writeHex_uiF128M( &genCases_f128_c, '\n' );
        if ( genLoops_givenCount ) {
            --genLoops_count;
            if ( ! genLoops_count ) break;
        }
    }

}

#endif

#ifdef FLOAT16

void gen_a_ui32_z_f16( float16_t trueFunction( uint32_t ) )
{
    union ui16_f16 uTrueZ;
    uint_fast8_t trueFlags;

    genCases_ui32_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_ui32_a_next();
        writeHex_ui32( genCases_ui32_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        uTrueZ.f = trueFunction( genCases_ui32_a );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui16( uTrueZ.ui, trueFlags ) ) break;
    }

}

#endif

void gen_a_ui32_z_f32( float32_t trueFunction( uint32_t ) )
{
    union ui32_f32 uTrueZ;
    uint_fast8_t trueFlags;

    genCases_ui32_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_ui32_a_next();
        writeHex_ui32( genCases_ui32_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        uTrueZ.f = trueFunction( genCases_ui32_a );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( uTrueZ.ui, trueFlags ) ) break;
    }

}

#ifdef FLOAT64

void gen_a_ui32_z_f64( float64_t trueFunction( uint32_t ) )
{
    union ui64_f64 uTrueZ;
    uint_fast8_t trueFlags;

    genCases_ui32_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_ui32_a_next();
        writeHex_ui32( genCases_ui32_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        uTrueZ.f = trueFunction( genCases_ui32_a );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( uTrueZ.ui, trueFlags ) ) break;
    }

}

#endif

#ifdef EXTFLOAT80

void gen_a_ui32_z_extF80( void trueFunction( uint32_t, extFloat80_t * ) )
{
    extFloat80_t trueZ;
    uint_fast8_t trueFlags;

    genCases_ui32_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_ui32_a_next();
        writeHex_ui32( genCases_ui32_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueFunction( genCases_ui32_a, &trueZ );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_extF80M( &trueZ, trueFlags ) ) break;
    }

}

#endif

#ifdef FLOAT128

void gen_a_ui32_z_f128( void trueFunction( uint32_t, float128_t * ) )
{
    float128_t trueZ;
    uint_fast8_t trueFlags;

    genCases_ui32_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_ui32_a_next();
        writeHex_ui32( genCases_ui32_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueFunction( genCases_ui32_a, &trueZ );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_f128M( &trueZ, trueFlags ) ) break;
    }

}

#endif

#ifdef FLOAT16

void gen_a_ui64_z_f16( float16_t trueFunction( uint64_t ) )
{
    union ui16_f16 uTrueZ;
    uint_fast8_t trueFlags;

    genCases_ui64_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_ui64_a_next();
        writeHex_ui64( genCases_ui64_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        uTrueZ.f = trueFunction( genCases_ui64_a );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui16( uTrueZ.ui, trueFlags ) ) break;
    }

}

#endif

void gen_a_ui64_z_f32( float32_t trueFunction( uint64_t ) )
{
    union ui32_f32 uTrueZ;
    uint_fast8_t trueFlags;

    genCases_ui64_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_ui64_a_next();
        writeHex_ui64( genCases_ui64_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        uTrueZ.f = trueFunction( genCases_ui64_a );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( uTrueZ.ui, trueFlags ) ) break;
    }

}

#ifdef FLOAT64

void gen_a_ui64_z_f64( float64_t trueFunction( uint64_t ) )
{
    union ui64_f64 uTrueZ;
    uint_fast8_t trueFlags;

    genCases_ui64_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_ui64_a_next();
        writeHex_ui64( genCases_ui64_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        uTrueZ.f = trueFunction( genCases_ui64_a );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( uTrueZ.ui, trueFlags ) ) break;
    }

}

#endif

#ifdef EXTFLOAT80

void gen_a_ui64_z_extF80( void trueFunction( uint64_t, extFloat80_t * ) )
{
    extFloat80_t trueZ;
    uint_fast8_t trueFlags;

    genCases_ui64_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_ui64_a_next();
        writeHex_ui64( genCases_ui64_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueFunction( genCases_ui64_a, &trueZ );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_extF80M( &trueZ, trueFlags ) ) break;
    }

}

#endif

#ifdef FLOAT128

void gen_a_ui64_z_f128( void trueFunction( uint64_t, float128_t * ) )
{
    float128_t trueZ;
    uint_fast8_t trueFlags;

    genCases_ui64_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_ui64_a_next();
        writeHex_ui64( genCases_ui64_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueFunction( genCases_ui64_a, &trueZ );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_f128M( &trueZ, trueFlags ) ) break;
    }

}

#endif

#ifdef FLOAT16

void gen_a_i32_z_f16( float16_t trueFunction( int32_t ) )
{
    union ui16_f16 uTrueZ;
    uint_fast8_t trueFlags;

    genCases_i32_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_i32_a_next();
        writeHex_ui32( genCases_i32_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        uTrueZ.f = trueFunction( genCases_i32_a );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui16( uTrueZ.ui, trueFlags ) ) break;
    }

}

#endif

void gen_a_i32_z_f32( float32_t trueFunction( int32_t ) )
{
    union ui32_f32 uTrueZ;
    uint_fast8_t trueFlags;

    genCases_i32_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_i32_a_next();
        writeHex_ui32( genCases_i32_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        uTrueZ.f = trueFunction( genCases_i32_a );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( uTrueZ.ui, trueFlags ) ) break;
    }

}

#ifdef FLOAT64

void gen_a_i32_z_f64( float64_t trueFunction( int32_t ) )
{
    union ui64_f64 uTrueZ;
    uint_fast8_t trueFlags;

    genCases_i32_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_i32_a_next();
        writeHex_ui32( genCases_i32_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        uTrueZ.f = trueFunction( genCases_i32_a );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( uTrueZ.ui, trueFlags ) ) break;
    }

}

#endif

#ifdef EXTFLOAT80

void gen_a_i32_z_extF80( void trueFunction( int32_t, extFloat80_t * ) )
{
    extFloat80_t trueZ;
    uint_fast8_t trueFlags;

    genCases_i32_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_i32_a_next();
        writeHex_ui32( genCases_i32_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueFunction( genCases_i32_a, &trueZ );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_extF80M( &trueZ, trueFlags ) ) break;
    }

}

#endif

#ifdef FLOAT128

void gen_a_i32_z_f128( void trueFunction( int32_t, float128_t * ) )
{
    float128_t trueZ;
    uint_fast8_t trueFlags;

    genCases_i32_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_i32_a_next();
        writeHex_ui32( genCases_i32_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueFunction( genCases_i32_a, &trueZ );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_f128M( &trueZ, trueFlags ) ) break;
    }

}

#endif

#ifdef FLOAT16

void gen_a_i64_z_f16( float16_t trueFunction( int64_t ) )
{
    union ui16_f16 uTrueZ;
    uint_fast8_t trueFlags;

    genCases_i64_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_i64_a_next();
        writeHex_ui64( genCases_i64_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        uTrueZ.f = trueFunction( genCases_i64_a );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui16( uTrueZ.ui, trueFlags ) ) break;
    }

}

#endif

void gen_a_i64_z_f32( float32_t trueFunction( int64_t ) )
{
    union ui32_f32 uTrueZ;
    uint_fast8_t trueFlags;

    genCases_i64_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_i64_a_next();
        writeHex_ui64( genCases_i64_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        uTrueZ.f = trueFunction( genCases_i64_a );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( uTrueZ.ui, trueFlags ) ) break;
    }

}

#ifdef FLOAT64

void gen_a_i64_z_f64( float64_t trueFunction( int64_t ) )
{
    union ui64_f64 uTrueZ;
    uint_fast8_t trueFlags;

    genCases_i64_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_i64_a_next();
        writeHex_ui64( genCases_i64_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        uTrueZ.f = trueFunction( genCases_i64_a );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( uTrueZ.ui, trueFlags ) ) break;
    }

}

#endif

#ifdef EXTFLOAT80

void gen_a_i64_z_extF80( void trueFunction( int64_t, extFloat80_t * ) )
{
    extFloat80_t trueZ;
    uint_fast8_t trueFlags;

    genCases_i64_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_i64_a_next();
        writeHex_ui64( genCases_i64_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueFunction( genCases_i64_a, &trueZ );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_extF80M( &trueZ, trueFlags ) ) break;
    }

}

#endif

#ifdef FLOAT128

void gen_a_i64_z_f128( void trueFunction( int64_t, float128_t * ) )
{
    float128_t trueZ;
    uint_fast8_t trueFlags;

    genCases_i64_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_i64_a_next();
        writeHex_ui64( genCases_i64_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueFunction( genCases_i64_a, &trueZ );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_f128M( &trueZ, trueFlags ) ) break;
    }

}

#endif

#ifdef FLOAT16

void
 gen_a_f16_z_ui32_rx(
     uint_fast32_t trueFunction( float16_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    union ui16_f16 uA;
    uint_fast32_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f16_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f16_a_next();
        uA.f = genCases_f16_a;
        writeHex_ui16( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( genCases_f16_a, roundingMode, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_f16_z_ui64_rx(
     uint_fast64_t trueFunction( float16_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    union ui16_f16 uA;
    uint_fast64_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f16_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f16_a_next();
        uA.f = genCases_f16_a;
        writeHex_ui16( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( genCases_f16_a, roundingMode, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_f16_z_i32_rx(
     int_fast32_t trueFunction( float16_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    union ui16_f16 uA;
    int_fast32_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f16_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f16_a_next();
        uA.f = genCases_f16_a;
        writeHex_ui16( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( genCases_f16_a, roundingMode, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_f16_z_i64_rx(
     int_fast64_t trueFunction( float16_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    union ui16_f16 uA;
    int_fast64_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f16_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f16_a_next();
        uA.f = genCases_f16_a;
        writeHex_ui16( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( genCases_f16_a, roundingMode, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_f16_z_ui32_x(
     uint_fast32_t trueFunction( float16_t, bool ), bool exact )
{
    union ui16_f16 uA;
    uint_fast32_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f16_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f16_a_next();
        uA.f = genCases_f16_a;
        writeHex_ui16( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( genCases_f16_a, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_f16_z_ui64_x(
     uint_fast64_t trueFunction( float16_t, bool ), bool exact )
{
    union ui16_f16 uA;
    uint_fast64_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f16_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f16_a_next();
        uA.f = genCases_f16_a;
        writeHex_ui16( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( genCases_f16_a, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_f16_z_i32_x( int_fast32_t trueFunction( float16_t, bool ), bool exact )
{
    union ui16_f16 uA;
    int_fast32_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f16_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f16_a_next();
        uA.f = genCases_f16_a;
        writeHex_ui16( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( genCases_f16_a, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_f16_z_i64_x( int_fast64_t trueFunction( float16_t, bool ), bool exact )
{
    union ui16_f16 uA;
    int_fast64_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f16_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f16_a_next();
        uA.f = genCases_f16_a;
        writeHex_ui16( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( genCases_f16_a, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( trueZ, trueFlags ) ) break;
    }

}

void gen_a_f16_z_f32( float32_t trueFunction( float16_t ) )
{
    union ui16_f16 uA;
    union ui32_f32 uTrueZ;
    uint_fast8_t trueFlags;

    genCases_f16_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f16_a_next();
        uA.f = genCases_f16_a;
        writeHex_ui16( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        uTrueZ.f = trueFunction( genCases_f16_a );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( uTrueZ.ui, trueFlags ) ) break;
    }

}

#ifdef FLOAT64

void gen_a_f16_z_f64( float64_t trueFunction( float16_t ) )
{
    union ui16_f16 uA;
    union ui64_f64 uTrueZ;
    uint_fast8_t trueFlags;

    genCases_f16_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f16_a_next();
        uA.f = genCases_f16_a;
        writeHex_ui16( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        uTrueZ.f = trueFunction( genCases_f16_a );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( uTrueZ.ui, trueFlags ) ) break;
    }

}

#endif

#ifdef EXTFLOAT80

void gen_a_f16_z_extF80( void trueFunction( float16_t, extFloat80_t * ) )
{
    union ui16_f16 uA;
    extFloat80_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f16_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f16_a_next();
        uA.f = genCases_f16_a;
        writeHex_ui16( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueFunction( genCases_f16_a, &trueZ );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_extF80M( &trueZ, trueFlags ) ) break;
    }

}

#endif

#ifdef FLOAT128

void gen_a_f16_z_f128( void trueFunction( float16_t, float128_t * ) )
{
    union ui16_f16 uA;
    float128_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f16_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f16_a_next();
        uA.f = genCases_f16_a;
        writeHex_ui16( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueFunction( genCases_f16_a, &trueZ );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_f128M( &trueZ, trueFlags ) ) break;
    }

}

#endif

void gen_az_f16( float16_t trueFunction( float16_t ) )
{
    union ui16_f16 u;
    uint_fast8_t trueFlags;

    genCases_f16_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f16_a_next();
        u.f = genCases_f16_a;
        writeHex_ui16( u.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        u.f = trueFunction( genCases_f16_a );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui16( u.ui, trueFlags ) ) break;
    }

}

void
 gen_az_f16_rx(
     float16_t trueFunction( float16_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    union ui16_f16 u;
    uint_fast8_t trueFlags;

    genCases_f16_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f16_a_next();
        u.f = genCases_f16_a;
        writeHex_ui16( u.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        u.f = trueFunction( genCases_f16_a, roundingMode, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui16( u.ui, trueFlags ) ) break;
    }

}

void gen_abz_f16( float16_t trueFunction( float16_t, float16_t ) )
{
    union ui16_f16 u;
    uint_fast8_t trueFlags;

    genCases_f16_ab_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f16_ab_next();
        u.f = genCases_f16_a;
        writeHex_ui16( u.ui, ' ' );
        u.f = genCases_f16_b;
        writeHex_ui16( u.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        u.f = trueFunction( genCases_f16_a, genCases_f16_b );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui16( u.ui, trueFlags ) ) break;
    }

}

void gen_abcz_f16( float16_t trueFunction( float16_t, float16_t, float16_t ) )
{
    union ui16_f16 u;
    uint_fast8_t trueFlags;

    genCases_f16_abc_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f16_abc_next();
        u.f = genCases_f16_a;
        writeHex_ui16( u.ui, ' ' );
        u.f = genCases_f16_b;
        writeHex_ui16( u.ui, ' ' );
        u.f = genCases_f16_c;
        writeHex_ui16( u.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        u.f = trueFunction( genCases_f16_a, genCases_f16_b, genCases_f16_c );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui16( u.ui, trueFlags ) ) break;
    }

}

void gen_ab_f16_z_bool( bool trueFunction( float16_t, float16_t ) )
{
    union ui16_f16 u;
    bool trueZ;
    uint_fast8_t trueFlags;

    genCases_f16_ab_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f16_ab_next();
        u.f = genCases_f16_a;
        writeHex_ui16( u.ui, ' ' );
        u.f = genCases_f16_b;
        writeHex_ui16( u.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( genCases_f16_a, genCases_f16_b );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_bool( trueZ, trueFlags ) ) break;
    }

}

#endif

void
 gen_a_f32_z_ui32_rx(
     uint_fast32_t trueFunction( float32_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    union ui32_f32 uA;
    uint_fast32_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f32_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f32_a_next();
        uA.f = genCases_f32_a;
        writeHex_ui32( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( genCases_f32_a, roundingMode, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_f32_z_ui64_rx(
     uint_fast64_t trueFunction( float32_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    union ui32_f32 uA;
    uint_fast64_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f32_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f32_a_next();
        uA.f = genCases_f32_a;
        writeHex_ui32( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( genCases_f32_a, roundingMode, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_f32_z_i32_rx(
     int_fast32_t trueFunction( float32_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    union ui32_f32 uA;
    int_fast32_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f32_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f32_a_next();
        uA.f = genCases_f32_a;
        writeHex_ui32( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( genCases_f32_a, roundingMode, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_f32_z_i64_rx(
     int_fast64_t trueFunction( float32_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    union ui32_f32 uA;
    int_fast64_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f32_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f32_a_next();
        uA.f = genCases_f32_a;
        writeHex_ui32( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( genCases_f32_a, roundingMode, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_f32_z_ui32_x(
     uint_fast32_t trueFunction( float32_t, bool ), bool exact )
{
    union ui32_f32 uA;
    uint_fast32_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f32_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f32_a_next();
        uA.f = genCases_f32_a;
        writeHex_ui32( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( genCases_f32_a, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_f32_z_ui64_x(
     uint_fast64_t trueFunction( float32_t, bool ), bool exact )
{
    union ui32_f32 uA;
    uint_fast64_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f32_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f32_a_next();
        uA.f = genCases_f32_a;
        writeHex_ui32( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( genCases_f32_a, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_f32_z_i32_x( int_fast32_t trueFunction( float32_t, bool ), bool exact )
{
    union ui32_f32 uA;
    int_fast32_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f32_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f32_a_next();
        uA.f = genCases_f32_a;
        writeHex_ui32( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( genCases_f32_a, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_f32_z_i64_x( int_fast64_t trueFunction( float32_t, bool ), bool exact )
{
    union ui32_f32 uA;
    int_fast64_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f32_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f32_a_next();
        uA.f = genCases_f32_a;
        writeHex_ui32( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( genCases_f32_a, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( trueZ, trueFlags ) ) break;
    }

}

#ifdef FLOAT16

void gen_a_f32_z_f16( float16_t trueFunction( float32_t ) )
{
    union ui32_f32 uA;
    union ui16_f16 uTrueZ;
    uint_fast8_t trueFlags;

    genCases_f32_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f32_a_next();
        uA.f = genCases_f32_a;
        writeHex_ui32( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        uTrueZ.f = trueFunction( genCases_f32_a );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui16( uTrueZ.ui, trueFlags ) ) break;
    }

}

#endif

#ifdef FLOAT64

void gen_a_f32_z_f64( float64_t trueFunction( float32_t ) )
{
    union ui32_f32 uA;
    union ui64_f64 uTrueZ;
    uint_fast8_t trueFlags;

    genCases_f32_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f32_a_next();
        uA.f = genCases_f32_a;
        writeHex_ui32( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        uTrueZ.f = trueFunction( genCases_f32_a );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( uTrueZ.ui, trueFlags ) ) break;
    }

}

#endif

#ifdef EXTFLOAT80

void gen_a_f32_z_extF80( void trueFunction( float32_t, extFloat80_t * ) )
{
    union ui32_f32 uA;
    extFloat80_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f32_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f32_a_next();
        uA.f = genCases_f32_a;
        writeHex_ui32( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueFunction( genCases_f32_a, &trueZ );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_extF80M( &trueZ, trueFlags ) ) break;
    }

}

#endif

#ifdef FLOAT128

void gen_a_f32_z_f128( void trueFunction( float32_t, float128_t * ) )
{
    union ui32_f32 uA;
    float128_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f32_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f32_a_next();
        uA.f = genCases_f32_a;
        writeHex_ui32( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueFunction( genCases_f32_a, &trueZ );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_f128M( &trueZ, trueFlags ) ) break;
    }

}

#endif

void gen_az_f32( float32_t trueFunction( float32_t ) )
{
    union ui32_f32 u;
    uint_fast8_t trueFlags;

    genCases_f32_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f32_a_next();
        u.f = genCases_f32_a;
        writeHex_ui32( u.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        u.f = trueFunction( genCases_f32_a );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( u.ui, trueFlags ) ) break;
    }

}

void
 gen_az_f32_rx(
     float32_t trueFunction( float32_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    union ui32_f32 u;
    uint_fast8_t trueFlags;

    genCases_f32_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f32_a_next();
        u.f = genCases_f32_a;
        writeHex_ui32( u.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        u.f = trueFunction( genCases_f32_a, roundingMode, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( u.ui, trueFlags ) ) break;
    }

}

void gen_abz_f32( float32_t trueFunction( float32_t, float32_t ) )
{
    union ui32_f32 u;
    uint_fast8_t trueFlags;

    genCases_f32_ab_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f32_ab_next();
        u.f = genCases_f32_a;
        writeHex_ui32( u.ui, ' ' );
        u.f = genCases_f32_b;
        writeHex_ui32( u.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        u.f = trueFunction( genCases_f32_a, genCases_f32_b );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( u.ui, trueFlags ) ) break;
    }

}

void gen_abcz_f32( float32_t trueFunction( float32_t, float32_t, float32_t ) )
{
    union ui32_f32 u;
    uint_fast8_t trueFlags;

    genCases_f32_abc_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f32_abc_next();
        u.f = genCases_f32_a;
        writeHex_ui32( u.ui, ' ' );
        u.f = genCases_f32_b;
        writeHex_ui32( u.ui, ' ' );
        u.f = genCases_f32_c;
        writeHex_ui32( u.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        u.f = trueFunction( genCases_f32_a, genCases_f32_b, genCases_f32_c );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( u.ui, trueFlags ) ) break;
    }

}

void gen_ab_f32_z_bool( bool trueFunction( float32_t, float32_t ) )
{
    union ui32_f32 u;
    bool trueZ;
    uint_fast8_t trueFlags;

    genCases_f32_ab_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f32_ab_next();
        u.f = genCases_f32_a;
        writeHex_ui32( u.ui, ' ' );
        u.f = genCases_f32_b;
        writeHex_ui32( u.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( genCases_f32_a, genCases_f32_b );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_bool( trueZ, trueFlags ) ) break;
    }

}

#ifdef FLOAT64

void
 gen_a_f64_z_ui32_rx(
     uint_fast32_t trueFunction( float64_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    union ui64_f64 uA;
    uint_fast32_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f64_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f64_a_next();
        uA.f = genCases_f64_a;
        writeHex_ui64( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( genCases_f64_a, roundingMode, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_f64_z_ui64_rx(
     uint_fast64_t trueFunction( float64_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    union ui64_f64 uA;
    uint_fast64_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f64_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f64_a_next();
        uA.f = genCases_f64_a;
        writeHex_ui64( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( genCases_f64_a, roundingMode, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_f64_z_i32_rx(
     int_fast32_t trueFunction( float64_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    union ui64_f64 uA;
    int_fast32_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f64_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f64_a_next();
        uA.f = genCases_f64_a;
        writeHex_ui64( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( genCases_f64_a, roundingMode, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_f64_z_i64_rx(
     int_fast64_t trueFunction( float64_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    union ui64_f64 uA;
    int_fast64_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f64_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f64_a_next();
        uA.f = genCases_f64_a;
        writeHex_ui64( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( genCases_f64_a, roundingMode, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_f64_z_ui32_x(
     uint_fast32_t trueFunction( float64_t, bool ), bool exact )
{
    union ui64_f64 uA;
    uint_fast32_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f64_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f64_a_next();
        uA.f = genCases_f64_a;
        writeHex_ui64( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( genCases_f64_a, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_f64_z_ui64_x(
     uint_fast64_t trueFunction( float64_t, bool ), bool exact )
{
    union ui64_f64 uA;
    uint_fast64_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f64_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f64_a_next();
        uA.f = genCases_f64_a;
        writeHex_ui64( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( genCases_f64_a, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_f64_z_i32_x( int_fast32_t trueFunction( float64_t, bool ), bool exact )
{
    union ui64_f64 uA;
    int_fast32_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f64_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f64_a_next();
        uA.f = genCases_f64_a;
        writeHex_ui64( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( genCases_f64_a, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_f64_z_i64_x( int_fast64_t trueFunction( float64_t, bool ), bool exact )
{
    union ui64_f64 uA;
    int_fast64_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f64_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f64_a_next();
        uA.f = genCases_f64_a;
        writeHex_ui64( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( genCases_f64_a, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( trueZ, trueFlags ) ) break;
    }

}

#ifdef FLOAT16

void gen_a_f64_z_f16( float16_t trueFunction( float64_t ) )
{
    union ui64_f64 uA;
    union ui16_f16 uTrueZ;
    uint_fast8_t trueFlags;

    genCases_f64_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f64_a_next();
        uA.f = genCases_f64_a;
        writeHex_ui64( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        uTrueZ.f = trueFunction( genCases_f64_a );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui16( uTrueZ.ui, trueFlags ) ) break;
    }

}

#endif

void gen_a_f64_z_f32( float32_t trueFunction( float64_t ) )
{
    union ui64_f64 uA;
    union ui32_f32 uTrueZ;
    uint_fast8_t trueFlags;

    genCases_f64_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f64_a_next();
        uA.f = genCases_f64_a;
        writeHex_ui64( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        uTrueZ.f = trueFunction( genCases_f64_a );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( uTrueZ.ui, trueFlags ) ) break;
    }

}

#ifdef EXTFLOAT80

void gen_a_f64_z_extF80( void trueFunction( float64_t, extFloat80_t * ) )
{
    union ui64_f64 uA;
    extFloat80_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f64_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f64_a_next();
        uA.f = genCases_f64_a;
        writeHex_ui64( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueFunction( genCases_f64_a, &trueZ );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_extF80M( &trueZ, trueFlags ) ) break;
    }

}

#endif

#ifdef FLOAT128

void gen_a_f64_z_f128( void trueFunction( float64_t, float128_t * ) )
{
    union ui64_f64 uA;
    float128_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f64_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f64_a_next();
        uA.f = genCases_f64_a;
        writeHex_ui64( uA.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueFunction( genCases_f64_a, &trueZ );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_f128M( &trueZ, trueFlags ) ) break;
    }

}

#endif

void gen_az_f64( float64_t trueFunction( float64_t ) )
{
    union ui64_f64 u;
    uint_fast8_t trueFlags;

    genCases_f64_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f64_a_next();
        u.f = genCases_f64_a;
        writeHex_ui64( u.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        u.f = trueFunction( genCases_f64_a );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( u.ui, trueFlags ) ) break;
    }

}

void
 gen_az_f64_rx(
     float64_t trueFunction( float64_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    union ui64_f64 u;
    uint_fast8_t trueFlags;

    genCases_f64_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f64_a_next();
        u.f = genCases_f64_a;
        writeHex_ui64( u.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        u.f = trueFunction( genCases_f64_a, roundingMode, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( u.ui, trueFlags ) ) break;
    }

}

void gen_abz_f64( float64_t trueFunction( float64_t, float64_t ) )
{
    union ui64_f64 u;
    uint_fast8_t trueFlags;

    genCases_f64_ab_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f64_ab_next();
        u.f = genCases_f64_a;
        writeHex_ui64( u.ui, ' ' );
        u.f = genCases_f64_b;
        writeHex_ui64( u.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        u.f = trueFunction( genCases_f64_a, genCases_f64_b );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( u.ui, trueFlags ) ) break;
    }

}

void gen_abcz_f64( float64_t trueFunction( float64_t, float64_t, float64_t ) )
{
    union ui64_f64 u;
    uint_fast8_t trueFlags;

    genCases_f64_abc_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f64_abc_next();
        u.f = genCases_f64_a;
        writeHex_ui64( u.ui, ' ' );
        u.f = genCases_f64_b;
        writeHex_ui64( u.ui, ' ' );
        u.f = genCases_f64_c;
        writeHex_ui64( u.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        u.f = trueFunction( genCases_f64_a, genCases_f64_b, genCases_f64_c );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( u.ui, trueFlags ) ) break;
    }

}

void gen_ab_f64_z_bool( bool trueFunction( float64_t, float64_t ) )
{
    union ui64_f64 u;
    bool trueZ;
    uint_fast8_t trueFlags;

    genCases_f64_ab_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f64_ab_next();
        u.f = genCases_f64_a;
        writeHex_ui64( u.ui, ' ' );
        u.f = genCases_f64_b;
        writeHex_ui64( u.ui, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( genCases_f64_a, genCases_f64_b );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_bool( trueZ, trueFlags ) ) break;
    }

}

#endif

#ifdef EXTFLOAT80

void
 gen_a_extF80_z_ui32_rx(
     uint_fast32_t trueFunction( const extFloat80_t *, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    uint_fast32_t trueZ;
    uint_fast8_t trueFlags;

    genCases_extF80_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_extF80_a_next();
        writeHex_uiExtF80M( &genCases_extF80_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &genCases_extF80_a, roundingMode, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_extF80_z_ui64_rx(
     uint_fast64_t trueFunction( const extFloat80_t *, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    uint_fast64_t trueZ;
    uint_fast8_t trueFlags;

    genCases_extF80_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_extF80_a_next();
        writeHex_uiExtF80M( &genCases_extF80_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &genCases_extF80_a, roundingMode, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_extF80_z_i32_rx(
     int_fast32_t trueFunction( const extFloat80_t *, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int_fast32_t trueZ;
    uint_fast8_t trueFlags;

    genCases_extF80_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_extF80_a_next();
        writeHex_uiExtF80M( &genCases_extF80_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &genCases_extF80_a, roundingMode, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_extF80_z_i64_rx(
     int_fast64_t trueFunction( const extFloat80_t *, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int_fast64_t trueZ;
    uint_fast8_t trueFlags;

    genCases_extF80_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_extF80_a_next();
        writeHex_uiExtF80M( &genCases_extF80_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &genCases_extF80_a, roundingMode, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_extF80_z_ui32_x(
     uint_fast32_t trueFunction( const extFloat80_t *, bool ), bool exact )
{
    uint_fast32_t trueZ;
    uint_fast8_t trueFlags;

    genCases_extF80_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_extF80_a_next();
        writeHex_uiExtF80M( &genCases_extF80_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &genCases_extF80_a, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_extF80_z_ui64_x(
     uint_fast64_t trueFunction( const extFloat80_t *, bool ), bool exact )
{
    uint_fast64_t trueZ;
    uint_fast8_t trueFlags;

    genCases_extF80_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_extF80_a_next();
        writeHex_uiExtF80M( &genCases_extF80_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &genCases_extF80_a, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_extF80_z_i32_x(
     int_fast32_t trueFunction( const extFloat80_t *, bool ), bool exact )
{
    int_fast32_t trueZ;
    uint_fast8_t trueFlags;

    genCases_extF80_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_extF80_a_next();
        writeHex_uiExtF80M( &genCases_extF80_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &genCases_extF80_a, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_extF80_z_i64_x(
     int_fast64_t trueFunction( const extFloat80_t *, bool ), bool exact )
{
    int_fast64_t trueZ;
    uint_fast8_t trueFlags;

    genCases_extF80_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_extF80_a_next();
        writeHex_uiExtF80M( &genCases_extF80_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &genCases_extF80_a, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( trueZ, trueFlags ) ) break;
    }

}

#ifdef FLOAT16

void gen_a_extF80_z_f16( float16_t trueFunction( const extFloat80_t * ) )
{
    union ui16_f16 uTrueZ;
    uint_fast8_t trueFlags;

    genCases_extF80_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_extF80_a_next();
        writeHex_uiExtF80M( &genCases_extF80_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        uTrueZ.f = trueFunction( &genCases_extF80_a );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui16( uTrueZ.ui, trueFlags ) ) break;
    }

}

#endif

void gen_a_extF80_z_f32( float32_t trueFunction( const extFloat80_t * ) )
{
    union ui32_f32 uTrueZ;
    uint_fast8_t trueFlags;

    genCases_extF80_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_extF80_a_next();
        writeHex_uiExtF80M( &genCases_extF80_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        uTrueZ.f = trueFunction( &genCases_extF80_a );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( uTrueZ.ui, trueFlags ) ) break;
    }

}

#ifdef FLOAT64

void gen_a_extF80_z_f64( float64_t trueFunction( const extFloat80_t * ) )
{
    union ui64_f64 uTrueZ;
    uint_fast8_t trueFlags;

    genCases_extF80_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_extF80_a_next();
        writeHex_uiExtF80M( &genCases_extF80_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        uTrueZ.f = trueFunction( &genCases_extF80_a );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( uTrueZ.ui, trueFlags ) ) break;
    }

}

#endif

#ifdef FLOAT128

void
 gen_a_extF80_z_f128( void trueFunction( const extFloat80_t *, float128_t * ) )
{
    float128_t trueZ;
    uint_fast8_t trueFlags;

    genCases_extF80_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_extF80_a_next();
        writeHex_uiExtF80M( &genCases_extF80_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueFunction( &genCases_extF80_a, &trueZ );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_f128M( &trueZ, trueFlags ) ) break;
    }

}

#endif

void gen_az_extF80( void trueFunction( const extFloat80_t *, extFloat80_t * ) )
{
    extFloat80_t trueZ;
    uint_fast8_t trueFlags;

    genCases_extF80_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_extF80_a_next();
        writeHex_uiExtF80M( &genCases_extF80_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueFunction( &genCases_extF80_a, &trueZ );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_extF80M( &trueZ, trueFlags ) ) break;
    }

}

void
 gen_az_extF80_rx(
     void
      trueFunction( const extFloat80_t *, uint_fast8_t, bool, extFloat80_t * ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    extFloat80_t trueZ;
    uint_fast8_t trueFlags;

    genCases_extF80_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_extF80_a_next();
        writeHex_uiExtF80M( &genCases_extF80_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueFunction( &genCases_extF80_a, roundingMode, exact, &trueZ );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_extF80M( &trueZ, trueFlags ) ) break;
    }

}

void
 gen_abz_extF80(
     void
      trueFunction(
          const extFloat80_t *, const extFloat80_t *, extFloat80_t * )
 )
{
    extFloat80_t trueZ;
    uint_fast8_t trueFlags;

    genCases_extF80_ab_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_extF80_ab_next();
        writeHex_uiExtF80M( &genCases_extF80_a, ' ' );
        writeHex_uiExtF80M( &genCases_extF80_b, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueFunction( &genCases_extF80_a, &genCases_extF80_b, &trueZ );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_extF80M( &trueZ, trueFlags ) ) break;
    }

}

void
 gen_ab_extF80_z_bool(
     bool trueFunction( const extFloat80_t *, const extFloat80_t * ) )
{
    bool trueZ;
    uint_fast8_t trueFlags;

    genCases_extF80_ab_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_extF80_ab_next();
        writeHex_uiExtF80M( &genCases_extF80_a, ' ' );
        writeHex_uiExtF80M( &genCases_extF80_b, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &genCases_extF80_a, &genCases_extF80_b );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_bool( trueZ, trueFlags ) ) break;
    }

}

#endif

#ifdef FLOAT128

void
 gen_a_f128_z_ui32_rx(
     uint_fast32_t trueFunction( const float128_t *, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    uint_fast32_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f128_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f128_a_next();
        writeHex_uiF128M( &genCases_f128_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &genCases_f128_a, roundingMode, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_f128_z_ui64_rx(
     uint_fast64_t trueFunction( const float128_t *, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    uint_fast64_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f128_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f128_a_next();
        writeHex_uiF128M( &genCases_f128_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &genCases_f128_a, roundingMode, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_f128_z_i32_rx(
     int_fast32_t trueFunction( const float128_t *, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int_fast32_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f128_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f128_a_next();
        writeHex_uiF128M( &genCases_f128_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &genCases_f128_a, roundingMode, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_f128_z_i64_rx(
     int_fast64_t trueFunction( const float128_t *, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int_fast64_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f128_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f128_a_next();
        writeHex_uiF128M( &genCases_f128_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &genCases_f128_a, roundingMode, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_f128_z_ui32_x(
     uint_fast32_t trueFunction( const float128_t *, bool ), bool exact )
{
    uint_fast32_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f128_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f128_a_next();
        writeHex_uiF128M( &genCases_f128_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &genCases_f128_a, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_f128_z_ui64_x(
     uint_fast64_t trueFunction( const float128_t *, bool ), bool exact )
{
    uint_fast64_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f128_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f128_a_next();
        writeHex_uiF128M( &genCases_f128_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &genCases_f128_a, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_f128_z_i32_x(
     int_fast32_t trueFunction( const float128_t *, bool ), bool exact )
{
    int_fast32_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f128_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f128_a_next();
        writeHex_uiF128M( &genCases_f128_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &genCases_f128_a, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( trueZ, trueFlags ) ) break;
    }

}

void
 gen_a_f128_z_i64_x(
     int_fast64_t trueFunction( const float128_t *, bool ), bool exact )
{
    int_fast64_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f128_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f128_a_next();
        writeHex_uiF128M( &genCases_f128_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &genCases_f128_a, exact );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( trueZ, trueFlags ) ) break;
    }

}

#ifdef FLOAT16

void gen_a_f128_z_f16( float16_t trueFunction( const float128_t * ) )
{
    union ui16_f16 uTrueZ;
    uint_fast8_t trueFlags;

    genCases_f128_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f128_a_next();
        writeHex_uiF128M( &genCases_f128_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        uTrueZ.f = trueFunction( &genCases_f128_a );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui16( uTrueZ.ui, trueFlags ) ) break;
    }

}

#endif

void gen_a_f128_z_f32( float32_t trueFunction( const float128_t * ) )
{
    union ui32_f32 uTrueZ;
    uint_fast8_t trueFlags;

    genCases_f128_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f128_a_next();
        writeHex_uiF128M( &genCases_f128_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        uTrueZ.f = trueFunction( &genCases_f128_a );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui32( uTrueZ.ui, trueFlags ) ) break;
    }

}

#ifdef FLOAT64

void gen_a_f128_z_f64( float64_t trueFunction( const float128_t * ) )
{
    union ui64_f64 uTrueZ;
    uint_fast8_t trueFlags;

    genCases_f128_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f128_a_next();
        writeHex_uiF128M( &genCases_f128_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        uTrueZ.f = trueFunction( &genCases_f128_a );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_ui64( uTrueZ.ui, trueFlags ) ) break;
    }

}

#endif

#ifdef EXTFLOAT80

void
 gen_a_f128_z_extF80( void trueFunction( const float128_t *, extFloat80_t * ) )
{
    extFloat80_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f128_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f128_a_next();
        writeHex_uiF128M( &genCases_f128_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueFunction( &genCases_f128_a, &trueZ );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_extF80M( &trueZ, trueFlags ) ) break;
    }

}

#endif

void gen_az_f128( void trueFunction( const float128_t *, float128_t * ) )
{
    float128_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f128_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f128_a_next();
        writeHex_uiF128M( &genCases_f128_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueFunction( &genCases_f128_a, &trueZ );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_f128M( &trueZ, trueFlags ) ) break;
    }

}

void
 gen_az_f128_rx(
     void trueFunction( const float128_t *, uint_fast8_t, bool, float128_t * ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    float128_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f128_a_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f128_a_next();
        writeHex_uiF128M( &genCases_f128_a, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueFunction( &genCases_f128_a, roundingMode, exact, &trueZ );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_f128M( &trueZ, trueFlags ) ) break;
    }

}

void
 gen_abz_f128(
     void trueFunction( const float128_t *, const float128_t *, float128_t * )
 )
{
    float128_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f128_ab_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f128_ab_next();
        writeHex_uiF128M( &genCases_f128_a, ' ' );
        writeHex_uiF128M( &genCases_f128_b, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueFunction( &genCases_f128_a, &genCases_f128_b, &trueZ );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_f128M( &trueZ, trueFlags ) ) break;
    }

}

void
 gen_abcz_f128(
     void
      trueFunction(
          const float128_t *,
          const float128_t *,
          const float128_t *,
          float128_t *
      )
 )
{
    float128_t trueZ;
    uint_fast8_t trueFlags;

    genCases_f128_abc_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f128_abc_next();
        writeHex_uiF128M( &genCases_f128_a, ' ' );
        writeHex_uiF128M( &genCases_f128_b, ' ' );
        writeHex_uiF128M( &genCases_f128_c, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueFunction(
            &genCases_f128_a, &genCases_f128_b, &genCases_f128_c, &trueZ );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_f128M( &trueZ, trueFlags ) ) break;
    }

}

void
 gen_ab_f128_z_bool(
     bool trueFunction( const float128_t *, const float128_t * ) )
{
    bool trueZ;
    uint_fast8_t trueFlags;

    genCases_f128_ab_init();
    checkEnoughCases();
    while ( ! genLoops_stop && (! genCases_done || genLoops_forever) ) {
        genCases_f128_ab_next();
        writeHex_uiF128M( &genCases_f128_a, ' ' );
        writeHex_uiF128M( &genCases_f128_b, ' ' );
        *genLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &genCases_f128_a, &genCases_f128_b );
        trueFlags = *genLoops_trueFlagsPtr;
        if ( writeGenOutputs_bool( trueZ, trueFlags ) ) break;
    }

}

#endif

