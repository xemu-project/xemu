
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
#include <stdlib.h>
#include <stdio.h>
#include "platform.h"
#include "uint128.h"
#include "fail.h"
#include "softfloat.h"
#include "readHex.h"
#include "verCases.h"
#include "writeCase.h"
#include "verLoops.h"

uint_fast8_t *verLoops_trueFlagsPtr;

static bool atEndOfInput( void )
{
    int i;

    i = fgetc( stdin );
    if ( i == EOF ) {
        if ( ! ferror( stdin ) && feof( stdin ) ) return true;
        fail( "Error reading input" );
    }
    ungetc( i, stdin );
    return false;

}

static void failFromBadInput( void )
{

    fail( "Invalid input format" );

}

static void readVerInput_bool( bool *aPtr )
{

    if ( ! readHex_bool( aPtr, ' ' ) ) failFromBadInput();

}

static void readVerInput_ui32( uint_fast32_t *aPtr )
{
    uint32_t a;

    if ( ! readHex_ui32( &a, ' ' ) ) failFromBadInput();
    *aPtr = a;

}

static void readVerInput_ui64( uint_fast64_t *aPtr )
{
    uint64_t a;

    if ( ! readHex_ui64( &a, ' ' ) ) failFromBadInput();
    *aPtr = a;

}

static void readVerInput_i32( int_fast32_t *aPtr )
{
    union { uint32_t ui; int32_t i; } uA;

    if ( ! readHex_ui32( &uA.ui, ' ' ) ) failFromBadInput();
    *aPtr = uA.i;

}

static void readVerInput_i64( int_fast64_t *aPtr )
{
    union { uint64_t ui; int64_t i; } uA;

    if ( ! readHex_ui64( &uA.ui, ' ' ) ) failFromBadInput();
    *aPtr = uA.i;

}

#ifdef FLOAT16

static void readVerInput_f16( float16_t *aPtr )
{
    union { uint16_t ui; float16_t f; } uA;

    if ( ! readHex_ui16( &uA.ui, ' ' ) ) failFromBadInput();
    *aPtr = uA.f;

}

#endif

static void readVerInput_f32( float32_t *aPtr )
{
    union { uint32_t ui; float32_t f; } uA;

    if ( ! readHex_ui32( &uA.ui, ' ' ) ) failFromBadInput();
    *aPtr = uA.f;

}

#ifdef FLOAT64

static void readVerInput_f64( float64_t *aPtr )
{
    union { uint64_t ui; float64_t f; } uA;

    if ( ! readHex_ui64( &uA.ui, ' ' ) ) failFromBadInput();
    *aPtr = uA.f;

}

#endif

#ifdef EXTFLOAT80

static void readVerInput_extF80( extFloat80_t *aPtr )
{
    struct extFloat80M *aSPtr;

    aSPtr = (struct extFloat80M *) aPtr;
    if (
           ! readHex_ui16( &aSPtr->signExp, 0 )
        || ! readHex_ui64( &aSPtr->signif, ' ' )
    ) {
        failFromBadInput();
    }

}

#endif

#ifdef FLOAT128

static void readVerInput_f128( float128_t *aPtr )
{
    struct uint128 *uiAPtr;

    uiAPtr = (struct uint128 *) aPtr;
    if (
        ! readHex_ui64( &uiAPtr->v64, 0 ) || ! readHex_ui64( &uiAPtr->v0, ' ' )
    ) {
        failFromBadInput();
    }

}

#endif

static void readVerInput_flags( uint_fast8_t *flagsPtr )
{
    uint_least8_t commonFlags;
    uint_fast8_t flags;

    if ( ! readHex_ui8( &commonFlags, '\n' ) || (0x20 <= commonFlags) ) {
        failFromBadInput();
    }
    flags = 0;
    if ( commonFlags & 0x10 ) flags |= softfloat_flag_invalid;
    if ( commonFlags & 0x08 ) flags |= softfloat_flag_infinite;
    if ( commonFlags & 0x04 ) flags |= softfloat_flag_overflow;
    if ( commonFlags & 0x02 ) flags |= softfloat_flag_underflow;
    if ( commonFlags & 0x01 ) flags |= softfloat_flag_inexact;
    *flagsPtr = flags;

}

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

#ifdef FLOAT16

void ver_a_ui32_z_f16( float16_t trueFunction( uint32_t ) )
{
    int count;
    uint_fast32_t a;
    float16_t subjZ;
    uint_fast8_t subjFlags;
    float16_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_ui32( &a );
        readVerInput_f16( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f16_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkNaNs
                || ! f16_isNaN( trueZ )
                || ! f16_isNaN( subjZ )
                || f16_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_ui32( a, "  " );
                writeCase_z_f16( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

void ver_a_ui32_z_f32( float32_t trueFunction( uint32_t ) )
{
    int count;
    uint_fast32_t a;
    float32_t subjZ;
    uint_fast8_t subjFlags;
    float32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_ui32( &a );
        readVerInput_f32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f32_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkNaNs
                || ! f32_isNaN( trueZ )
                || ! f32_isNaN( subjZ )
                || f32_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_ui32( a, "  " );
                writeCase_z_f32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#ifdef FLOAT64

void ver_a_ui32_z_f64( float64_t trueFunction( uint32_t ) )
{
    int count;
    uint_fast32_t a;
    float64_t subjZ;
    uint_fast8_t subjFlags;
    float64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_ui32( &a );
        readVerInput_f64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f64_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkNaNs
                || ! f64_isNaN( trueZ )
                || ! f64_isNaN( subjZ )
                || f64_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_ui32( a, "  " );
                writeCase_z_f64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

#ifdef EXTFLOAT80

void ver_a_ui32_z_extF80( void trueFunction( uint32_t, extFloat80_t * ) )
{
    int count;
    uint_fast32_t a;
    extFloat80_t subjZ;
    uint_fast8_t subjFlags;
    extFloat80_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_ui32( &a );
        readVerInput_extF80( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueFunction( a, &trueZ );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! extF80M_same( &trueZ, &subjZ ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkNaNs
                || ! extF80M_isNaN( &trueZ )
                || ! extF80M_isNaN( &subjZ )
                || extF80M_isSignalingNaN( &subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_ui32( a, "\n\t" );
                writeCase_z_extF80M( &trueZ, trueFlags, &subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

#ifdef FLOAT128

void ver_a_ui32_z_f128( void trueFunction( uint32_t, float128_t * ) )
{
    int count;
    uint_fast32_t a;
    float128_t subjZ;
    uint_fast8_t subjFlags;
    float128_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_ui32( &a );
        readVerInput_f128( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueFunction( a, &trueZ );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f128M_same( &trueZ, &subjZ ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkNaNs
                || ! f128M_isNaN( &trueZ )
                || ! f128M_isNaN( &subjZ )
                || f128M_isSignalingNaN( &subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_ui32( a, "  " );
                writeCase_z_f128M( &trueZ, trueFlags, &subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

#ifdef FLOAT16

void ver_a_ui64_z_f16( float16_t trueFunction( uint64_t ) )
{
    int count;
    uint_fast64_t a;
    float16_t subjZ;
    uint_fast8_t subjFlags;
    float16_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_ui64( &a );
        readVerInput_f16( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f16_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkNaNs
                || ! f16_isNaN( trueZ )
                || ! f16_isNaN( subjZ )
                || f16_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_ui64( a, "  " );
                writeCase_z_f16( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

void ver_a_ui64_z_f32( float32_t trueFunction( uint64_t ) )
{
    int count;
    uint_fast64_t a;
    float32_t subjZ;
    uint_fast8_t subjFlags;
    float32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_ui64( &a );
        readVerInput_f32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f32_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkNaNs
                || ! f32_isNaN( trueZ )
                || ! f32_isNaN( subjZ )
                || f32_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_ui64( a, "  " );
                writeCase_z_f32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#ifdef FLOAT64

void ver_a_ui64_z_f64( float64_t trueFunction( uint64_t ) )
{
    int count;
    uint_fast64_t a;
    float64_t subjZ;
    uint_fast8_t subjFlags;
    float64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_ui64( &a );
        readVerInput_f64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f64_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkNaNs
                || ! f64_isNaN( trueZ )
                || ! f64_isNaN( subjZ )
                || f64_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_ui64( a, "\n\t" );
                writeCase_z_f64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

#ifdef EXTFLOAT80

void ver_a_ui64_z_extF80( void trueFunction( uint64_t, extFloat80_t * ) )
{
    int count;
    uint_fast64_t a;
    extFloat80_t subjZ;
    uint_fast8_t subjFlags;
    extFloat80_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_ui64( &a );
        readVerInput_extF80( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueFunction( a, &trueZ );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! extF80M_same( &trueZ, &subjZ ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkNaNs
                || ! extF80M_isNaN( &trueZ )
                || ! extF80M_isNaN( &subjZ )
                || extF80M_isSignalingNaN( &subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_ui64( a, "\n\t" );
                writeCase_z_extF80M( &trueZ, trueFlags, &subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

#ifdef FLOAT128

void ver_a_ui64_z_f128( void trueFunction( uint64_t, float128_t * ) )
{
    int count;
    uint_fast64_t a;
    float128_t subjZ;
    uint_fast8_t subjFlags;
    float128_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_ui64( &a );
        readVerInput_f128( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueFunction( a, &trueZ );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f128M_same( &trueZ, &subjZ ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkNaNs
                || ! f128M_isNaN( &trueZ )
                || ! f128M_isNaN( &subjZ )
                || f128M_isSignalingNaN( &subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_ui64( a, "  " );
                writeCase_z_f128M( &trueZ, trueFlags, &subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

#ifdef FLOAT16

void ver_a_i32_z_f16( float16_t trueFunction( int32_t ) )
{
    int count;
    int_fast32_t a;
    float16_t subjZ;
    uint_fast8_t subjFlags;
    float16_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_i32( &a );
        readVerInput_f16( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f16_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkNaNs
                || ! f16_isNaN( trueZ )
                || ! f16_isNaN( subjZ )
                || f16_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_i32( a, "  " );
                writeCase_z_f16( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

void ver_a_i32_z_f32( float32_t trueFunction( int32_t ) )
{
    int count;
    int_fast32_t a;
    float32_t subjZ;
    uint_fast8_t subjFlags;
    float32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_i32( &a );
        readVerInput_f32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f32_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkNaNs
                || ! f32_isNaN( trueZ )
                || ! f32_isNaN( subjZ )
                || f32_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_i32( a, "  " );
                writeCase_z_f32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#ifdef FLOAT64

void ver_a_i32_z_f64( float64_t trueFunction( int32_t ) )
{
    int count;
    int_fast32_t a;
    float64_t subjZ;
    uint_fast8_t subjFlags;
    float64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_i32( &a );
        readVerInput_f64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f64_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkNaNs
                || ! f64_isNaN( trueZ )
                || ! f64_isNaN( subjZ )
                || f64_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_i32( a, "  " );
                writeCase_z_f64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

#ifdef EXTFLOAT80

void ver_a_i32_z_extF80( void trueFunction( int32_t, extFloat80_t * ) )
{
    int count;
    int_fast32_t a;
    extFloat80_t subjZ;
    uint_fast8_t subjFlags;
    extFloat80_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_i32( &a );
        readVerInput_extF80( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueFunction( a, &trueZ );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! extF80M_same( &trueZ, &subjZ ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkNaNs
                || ! extF80M_isNaN( &trueZ )
                || ! extF80M_isNaN( &subjZ )
                || extF80M_isSignalingNaN( &subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_i32( a, "\n\t" );
                writeCase_z_extF80M( &trueZ, trueFlags, &subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

#ifdef FLOAT128

void ver_a_i32_z_f128( void trueFunction( int32_t, float128_t * ) )
{
    int count;
    int_fast32_t a;
    float128_t subjZ;
    uint_fast8_t subjFlags;
    float128_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_i32( &a );
        readVerInput_f128( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueFunction( a, &trueZ );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f128M_same( &trueZ, &subjZ ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkNaNs
                || ! f128M_isNaN( &trueZ )
                || ! f128M_isNaN( &subjZ )
                || f128M_isSignalingNaN( &subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_i32( a, "  " );
                writeCase_z_f128M( &trueZ, trueFlags, &subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

#ifdef FLOAT16

void ver_a_i64_z_f16( float16_t trueFunction( int64_t ) )
{
    int count;
    int_fast64_t a;
    float16_t subjZ;
    uint_fast8_t subjFlags;
    float16_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_i64( &a );
        readVerInput_f16( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f16_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkNaNs
                || ! f16_isNaN( trueZ )
                || ! f16_isNaN( subjZ )
                || f16_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_i64( a, "  " );
                writeCase_z_f16( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

void ver_a_i64_z_f32( float32_t trueFunction( int64_t ) )
{
    int count;
    int_fast64_t a;
    float32_t subjZ;
    uint_fast8_t subjFlags;
    float32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_i64( &a );
        readVerInput_f32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f32_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkNaNs
                || ! f32_isNaN( trueZ )
                || ! f32_isNaN( subjZ )
                || f32_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_i64( a, "  " );
                writeCase_z_f32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#ifdef FLOAT64

void ver_a_i64_z_f64( float64_t trueFunction( int64_t ) )
{
    int count;
    int_fast64_t a;
    float64_t subjZ;
    uint_fast8_t subjFlags;
    float64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_i64( &a );
        readVerInput_f64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f64_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkNaNs
                || ! f64_isNaN( trueZ )
                || ! f64_isNaN( subjZ )
                || f64_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_i64( a, "\n\t" );
                writeCase_z_f64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

#ifdef EXTFLOAT80

void ver_a_i64_z_extF80( void trueFunction( int64_t, extFloat80_t * ) )
{
    int count;
    int_fast64_t a;
    extFloat80_t subjZ;
    uint_fast8_t subjFlags;
    extFloat80_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_i64( &a );
        readVerInput_extF80( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueFunction( a, &trueZ );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! extF80M_same( &trueZ, &subjZ ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkNaNs
                || ! extF80M_isNaN( &trueZ )
                || ! extF80M_isNaN( &subjZ )
                || extF80M_isSignalingNaN( &subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_i64( a, "\n\t" );
                writeCase_z_extF80M( &trueZ, trueFlags, &subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

#ifdef FLOAT128

void ver_a_i64_z_f128( void trueFunction( int64_t, float128_t * ) )
{
    int count;
    int_fast64_t a;
    float128_t subjZ;
    uint_fast8_t subjFlags;
    float128_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_i64( &a );
        readVerInput_f128( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueFunction( a, &trueZ );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f128M_same( &trueZ, &subjZ ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkNaNs
                || ! f128M_isNaN( &trueZ )
                || ! f128M_isNaN( &subjZ )
                || f128M_isSignalingNaN( &subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_i64( a, "  " );
                writeCase_z_f128M( &trueZ, trueFlags, &subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

#ifdef FLOAT16

void
 ver_a_f16_z_ui32_rx(
     uint_fast32_t trueFunction( float16_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int count;
    float16_t a;
    uint_fast32_t subjZ;
    uint_fast8_t subjFlags;
    uint_fast32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f16( &a );
        readVerInput_ui32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, roundingMode, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != 0xFFFFFFFF) && (subjZ != 0))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f16( a );
                writeCase_z_ui32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_f16_z_ui64_rx(
     uint_fast64_t trueFunction( float16_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int count;
    float16_t a;
    uint_fast64_t subjZ;
    uint_fast8_t subjFlags;
    uint_fast64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f16( &a );
        readVerInput_ui64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, roundingMode, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != UINT64_C( 0xFFFFFFFFFFFFFFFF )) && (subjZ != 0))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f16( a );
                writeCase_z_ui64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_f16_z_i32_rx(
     int_fast32_t trueFunction( float16_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int count;
    float16_t a;
    int_fast32_t subjZ;
    uint_fast8_t subjFlags;
    int_fast32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f16( &a );
        readVerInput_i32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, roundingMode, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != 0x7FFFFFFF) && (subjZ != -0x7FFFFFFF - 1)
                        && (! f16_isNaN( a ) || (subjZ != 0)))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f16( a );
                writeCase_z_i32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_f16_z_i64_rx(
     int_fast64_t trueFunction( float16_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int count;
    float16_t a;
    int_fast64_t subjZ;
    uint_fast8_t subjFlags;
    int_fast64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f16( &a );
        readVerInput_i64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, roundingMode, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != INT64_C( 0x7FFFFFFFFFFFFFFF ))
                        && (subjZ != -INT64_C( 0x7FFFFFFFFFFFFFFF ) - 1)
                        && (! f16_isNaN( a ) || (subjZ != 0)))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f16( a );
                writeCase_z_i64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_f16_z_ui32_x(
     uint_fast32_t trueFunction( float16_t, bool ), bool exact )
{
    int count;
    float16_t a;
    uint_fast32_t subjZ;
    uint_fast8_t subjFlags;
    uint_fast32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f16( &a );
        readVerInput_ui32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != 0xFFFFFFFF) && (subjZ != 0))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f16( a );
                writeCase_z_ui32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_f16_z_ui64_x(
     uint_fast64_t trueFunction( float16_t, bool ), bool exact )
{
    int count;
    float16_t a;
    uint_fast64_t subjZ;
    uint_fast8_t subjFlags;
    uint_fast64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f16( &a );
        readVerInput_ui64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != UINT64_C( 0xFFFFFFFFFFFFFFFF )) && (subjZ != 0))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f16( a );
                writeCase_z_ui64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_f16_z_i32_x(
     int_fast32_t trueFunction( float16_t, bool ), bool exact )
{
    int count;
    float16_t a;
    int_fast32_t subjZ;
    uint_fast8_t subjFlags;
    int_fast32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f16( &a );
        readVerInput_i32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != 0x7FFFFFFF) && (subjZ != -0x7FFFFFFF - 1)
                        && (! f16_isNaN( a ) || (subjZ != 0)))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f16( a );
                writeCase_z_i32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_f16_z_i64_x(
     int_fast64_t trueFunction( float16_t, bool ), bool exact )
{
    int count;
    float16_t a;
    int_fast64_t subjZ;
    uint_fast8_t subjFlags;
    int_fast64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f16( &a );
        readVerInput_i64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != INT64_C( 0x7FFFFFFFFFFFFFFF ))
                        && (subjZ != -INT64_C( 0x7FFFFFFFFFFFFFFF ) - 1)
                        && (! f16_isNaN( a ) || (subjZ != 0)))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f16( a );
                writeCase_z_i64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void ver_a_f16_z_f32( float32_t trueFunction( float16_t ) )
{
    int count;
    float16_t a;
    float32_t subjZ;
    uint_fast8_t subjFlags;
    float32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f16( &a );
        readVerInput_f32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f32_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && f16_isSignalingNaN( a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f32_isNaN( trueZ )
                || ! f32_isNaN( subjZ )
                || f32_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f16( a );
                writeCase_z_f32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#ifdef FLOAT64

void ver_a_f16_z_f64( float64_t trueFunction( float16_t ) )
{
    int count;
    float16_t a;
    float64_t subjZ;
    uint_fast8_t subjFlags;
    float64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f16( &a );
        readVerInput_f64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f64_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && f16_isSignalingNaN( a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f64_isNaN( trueZ )
                || ! f64_isNaN( subjZ )
                || f64_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f16( a );
                writeCase_z_f64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

#ifdef EXTFLOAT80

void ver_a_f16_z_extF80( void trueFunction( float16_t, extFloat80_t * ) )
{
    int count;
    float16_t a;
    extFloat80_t subjZ;
    uint_fast8_t subjFlags;
    extFloat80_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f16( &a );
        readVerInput_extF80( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueFunction( a, &trueZ );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! extF80M_same( &trueZ, &subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && f16_isSignalingNaN( a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! extF80M_isNaN( &trueZ )
                || ! extF80M_isNaN( &subjZ )
                || extF80M_isSignalingNaN( &subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f16( a );
                writeCase_z_extF80M( &trueZ, trueFlags, &subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

#ifdef FLOAT128

void ver_a_f16_z_f128( void trueFunction( float16_t, float128_t * ) )
{
    int count;
    float16_t a;
    float128_t subjZ;
    uint_fast8_t subjFlags;
    float128_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f16( &a );
        readVerInput_f128( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueFunction( a, &trueZ );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f128M_same( &trueZ, &subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && f16_isSignalingNaN( a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f128M_isNaN( &trueZ )
                || ! f128M_isNaN( &subjZ )
                || f128M_isSignalingNaN( &subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f16( a );
                writeCase_z_f128M( &trueZ, trueFlags, &subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

void ver_az_f16( float16_t trueFunction( float16_t ) )
{
    int count;
    float16_t a, subjZ;
    uint_fast8_t subjFlags;
    float16_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f16( &a );
        readVerInput_f16( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f16_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && f16_isSignalingNaN( a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f16_isNaN( trueZ )
                || ! f16_isNaN( subjZ )
                || f16_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f16( a );
                writeCase_z_f16( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_az_f16_rx(
     float16_t trueFunction( float16_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int count;
    float16_t a, subjZ;
    uint_fast8_t subjFlags;
    float16_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f16( &a );
        readVerInput_f16( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, roundingMode, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f16_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && f16_isSignalingNaN( a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f16_isNaN( trueZ )
                || ! f16_isNaN( subjZ )
                || f16_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f16( a );
                writeCase_z_f16( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void ver_abz_f16( float16_t trueFunction( float16_t, float16_t ) )
{
    int count;
    float16_t a, b, subjZ;
    uint_fast8_t subjFlags;
    float16_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f16( &a );
        readVerInput_f16( &b );
        readVerInput_f16( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, b );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f16_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if (
                ! verCases_checkNaNs
                    && (f16_isSignalingNaN( a ) || f16_isSignalingNaN( b ))
            ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f16_isNaN( trueZ )
                || ! f16_isNaN( subjZ )
                || f16_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_ab_f16( a, b );
                writeCase_z_f16( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void ver_abcz_f16( float16_t trueFunction( float16_t, float16_t, float16_t ) )
{
    int count;
    float16_t a, b, c, subjZ;
    uint_fast8_t subjFlags;
    float16_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f16( &a );
        readVerInput_f16( &b );
        readVerInput_f16( &c );
        readVerInput_f16( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, b, c );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f16_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if (
                   ! verCases_checkNaNs
                && (f16_isSignalingNaN( a ) || f16_isSignalingNaN( b )
                        || f16_isSignalingNaN( c ))
            ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f16_isNaN( trueZ )
                || ! f16_isNaN( subjZ )
                || f16_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_abc_f16( a, b, c );
                writeCase_z_f16( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void ver_ab_f16_z_bool( bool trueFunction( float16_t, float16_t ) )
{
    int count;
    float16_t a, b;
    bool subjZ;
    uint_fast8_t subjFlags;
    bool trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f16( &a );
        readVerInput_f16( &b );
        readVerInput_bool( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, b );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                ! verCases_checkNaNs
                    && (f16_isSignalingNaN( a ) || f16_isSignalingNaN( b ))
            ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_ab_f16( a, b );
                writeCase_z_bool( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

void
 ver_a_f32_z_ui32_rx(
     uint_fast32_t trueFunction( float32_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int count;
    float32_t a;
    uint_fast32_t subjZ;
    uint_fast8_t subjFlags;
    uint_fast32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f32( &a );
        readVerInput_ui32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, roundingMode, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != 0xFFFFFFFF) && (subjZ != 0))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f32( a, "  " );
                writeCase_z_ui32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_f32_z_ui64_rx(
     uint_fast64_t trueFunction( float32_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int count;
    float32_t a;
    uint_fast64_t subjZ;
    uint_fast8_t subjFlags;
    uint_fast64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f32( &a );
        readVerInput_ui64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, roundingMode, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != UINT64_C( 0xFFFFFFFFFFFFFFFF )) && (subjZ != 0))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f32( a, "  " );
                writeCase_z_ui64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_f32_z_i32_rx(
     int_fast32_t trueFunction( float32_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int count;
    float32_t a;
    int_fast32_t subjZ;
    uint_fast8_t subjFlags;
    int_fast32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f32( &a );
        readVerInput_i32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, roundingMode, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != 0x7FFFFFFF) && (subjZ != -0x7FFFFFFF - 1)
                        && (! f32_isNaN( a ) || (subjZ != 0)))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f32( a, "  " );
                writeCase_z_i32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_f32_z_i64_rx(
     int_fast64_t trueFunction( float32_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int count;
    float32_t a;
    int_fast64_t subjZ;
    uint_fast8_t subjFlags;
    int_fast64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f32( &a );
        readVerInput_i64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, roundingMode, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != INT64_C( 0x7FFFFFFFFFFFFFFF ))
                        && (subjZ != -INT64_C( 0x7FFFFFFFFFFFFFFF ) - 1)
                        && (! f32_isNaN( a ) || (subjZ != 0)))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f32( a, "  " );
                writeCase_z_i64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_f32_z_ui32_x(
     uint_fast32_t trueFunction( float32_t, bool ), bool exact )
{
    int count;
    float32_t a;
    uint_fast32_t subjZ;
    uint_fast8_t subjFlags;
    uint_fast32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f32( &a );
        readVerInput_ui32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != 0xFFFFFFFF) && (subjZ != 0))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f32( a, "  " );
                writeCase_z_ui32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_f32_z_ui64_x(
     uint_fast64_t trueFunction( float32_t, bool ), bool exact )
{
    int count;
    float32_t a;
    uint_fast64_t subjZ;
    uint_fast8_t subjFlags;
    uint_fast64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f32( &a );
        readVerInput_ui64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != UINT64_C( 0xFFFFFFFFFFFFFFFF )) && (subjZ != 0))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f32( a, "  " );
                writeCase_z_ui64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_f32_z_i32_x(
     int_fast32_t trueFunction( float32_t, bool ), bool exact )
{
    int count;
    float32_t a;
    int_fast32_t subjZ;
    uint_fast8_t subjFlags;
    int_fast32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f32( &a );
        readVerInput_i32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != 0x7FFFFFFF) && (subjZ != -0x7FFFFFFF - 1)
                        && (! f32_isNaN( a ) || (subjZ != 0)))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f32( a, "  " );
                writeCase_z_i32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_f32_z_i64_x(
     int_fast64_t trueFunction( float32_t, bool ), bool exact )
{
    int count;
    float32_t a;
    int_fast64_t subjZ;
    uint_fast8_t subjFlags;
    int_fast64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f32( &a );
        readVerInput_i64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != INT64_C( 0x7FFFFFFFFFFFFFFF ))
                        && (subjZ != -INT64_C( 0x7FFFFFFFFFFFFFFF ) - 1)
                        && (! f32_isNaN( a ) || (subjZ != 0)))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f32( a, "  " );
                writeCase_z_i64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#ifdef FLOAT16

void ver_a_f32_z_f16( float16_t trueFunction( float32_t ) )
{
    int count;
    float32_t a;
    float16_t subjZ;
    uint_fast8_t subjFlags;
    float16_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f32( &a );
        readVerInput_f16( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f16_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && f32_isSignalingNaN( a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f16_isNaN( trueZ )
                || ! f16_isNaN( subjZ )
                || f16_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f32( a, "  " );
                writeCase_z_f16( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

#ifdef FLOAT64

void ver_a_f32_z_f64( float64_t trueFunction( float32_t ) )
{
    int count;
    float32_t a;
    float64_t subjZ;
    uint_fast8_t subjFlags;
    float64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f32( &a );
        readVerInput_f64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f64_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && f32_isSignalingNaN( a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f64_isNaN( trueZ )
                || ! f64_isNaN( subjZ )
                || f64_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f32( a, "  " );
                writeCase_z_f64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

#ifdef EXTFLOAT80

void ver_a_f32_z_extF80( void trueFunction( float32_t, extFloat80_t * ) )
{
    int count;
    float32_t a;
    extFloat80_t subjZ;
    uint_fast8_t subjFlags;
    extFloat80_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f32( &a );
        readVerInput_extF80( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueFunction( a, &trueZ );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! extF80M_same( &trueZ, &subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && f32_isSignalingNaN( a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! extF80M_isNaN( &trueZ )
                || ! extF80M_isNaN( &subjZ )
                || extF80M_isSignalingNaN( &subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f32( a, "\n\t" );
                writeCase_z_extF80M( &trueZ, trueFlags, &subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

#ifdef FLOAT128

void ver_a_f32_z_f128( void trueFunction( float32_t, float128_t * ) )
{
    int count;
    float32_t a;
    float128_t subjZ;
    uint_fast8_t subjFlags;
    float128_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f32( &a );
        readVerInput_f128( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueFunction( a, &trueZ );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f128M_same( &trueZ, &subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && f32_isSignalingNaN( a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f128M_isNaN( &trueZ )
                || ! f128M_isNaN( &subjZ )
                || f128M_isSignalingNaN( &subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f32( a, "  " );
                writeCase_z_f128M( &trueZ, trueFlags, &subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

void ver_az_f32( float32_t trueFunction( float32_t ) )
{
    int count;
    float32_t a, subjZ;
    uint_fast8_t subjFlags;
    float32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f32( &a );
        readVerInput_f32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f32_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && f32_isSignalingNaN( a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f32_isNaN( trueZ )
                || ! f32_isNaN( subjZ )
                || f32_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f32( a, "  " );
                writeCase_z_f32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_az_f32_rx(
     float32_t trueFunction( float32_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int count;
    float32_t a, subjZ;
    uint_fast8_t subjFlags;
    float32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f32( &a );
        readVerInput_f32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, roundingMode, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f32_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && f32_isSignalingNaN( a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f32_isNaN( trueZ )
                || ! f32_isNaN( subjZ )
                || f32_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f32( a, "  " );
                writeCase_z_f32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void ver_abz_f32( float32_t trueFunction( float32_t, float32_t ) )
{
    int count;
    float32_t a, b, subjZ;
    uint_fast8_t subjFlags;
    float32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f32( &a );
        readVerInput_f32( &b );
        readVerInput_f32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, b );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f32_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if (
                ! verCases_checkNaNs
                    && (f32_isSignalingNaN( a ) || f32_isSignalingNaN( b ))
            ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f32_isNaN( trueZ )
                || ! f32_isNaN( subjZ )
                || f32_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_ab_f32( a, b );
                writeCase_z_f32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void ver_abcz_f32( float32_t trueFunction( float32_t, float32_t, float32_t ) )
{
    int count;
    float32_t a, b, c, subjZ;
    uint_fast8_t subjFlags;
    float32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f32( &a );
        readVerInput_f32( &b );
        readVerInput_f32( &c );
        readVerInput_f32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, b, c );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f32_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if (
                   ! verCases_checkNaNs
                && (f32_isSignalingNaN( a ) || f32_isSignalingNaN( b )
                        || f32_isSignalingNaN( c ))
            ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f32_isNaN( trueZ )
                || ! f32_isNaN( subjZ )
                || f32_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_abc_f32( a, b, c );
                writeCase_z_f32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void ver_ab_f32_z_bool( bool trueFunction( float32_t, float32_t ) )
{
    int count;
    float32_t a, b;
    bool subjZ;
    uint_fast8_t subjFlags;
    bool trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f32( &a );
        readVerInput_f32( &b );
        readVerInput_bool( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, b );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                ! verCases_checkNaNs
                    && (f32_isSignalingNaN( a ) || f32_isSignalingNaN( b ))
            ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_ab_f32( a, b );
                writeCase_z_bool( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

#ifdef FLOAT64

void
 ver_a_f64_z_ui32_rx(
     uint_fast32_t trueFunction( float64_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int count;
    float64_t a;
    uint_fast32_t subjZ;
    uint_fast8_t subjFlags;
    uint_fast32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f64( &a );
        readVerInput_ui32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, roundingMode, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != 0xFFFFFFFF) && (subjZ != 0))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f64( a, "  " );
                writeCase_z_ui32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_f64_z_ui64_rx(
     uint_fast64_t trueFunction( float64_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int count;
    float64_t a;
    uint_fast64_t subjZ;
    uint_fast8_t subjFlags;
    uint_fast64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f64( &a );
        readVerInput_ui64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, roundingMode, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != UINT64_C( 0xFFFFFFFFFFFFFFFF )) && (subjZ != 0))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f64( a, "  " );
                writeCase_z_ui64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_f64_z_i32_rx(
     int_fast32_t trueFunction( float64_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int count;
    float64_t a;
    int_fast32_t subjZ;
    uint_fast8_t subjFlags;
    int_fast32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f64( &a );
        readVerInput_i32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, roundingMode, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != 0x7FFFFFFF) && (subjZ != -0x7FFFFFFF - 1)
                        && (! f64_isNaN( a ) || (subjZ != 0)))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f64( a, "  " );
                writeCase_z_i32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_f64_z_i64_rx(
     int_fast64_t trueFunction( float64_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int count;
    float64_t a;
    int_fast64_t subjZ;
    uint_fast8_t subjFlags;
    int_fast64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f64( &a );
        readVerInput_i64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, roundingMode, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != INT64_C( 0x7FFFFFFFFFFFFFFF ))
                        && (subjZ != -INT64_C( 0x7FFFFFFFFFFFFFFF ) - 1)
                        && (! f64_isNaN( a ) || (subjZ != 0)))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f64( a, "  " );
                writeCase_z_i64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_f64_z_ui32_x(
     uint_fast32_t trueFunction( float64_t, bool ), bool exact )
{
    int count;
    float64_t a;
    uint_fast32_t subjZ;
    uint_fast8_t subjFlags;
    uint_fast32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f64( &a );
        readVerInput_ui32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != 0xFFFFFFFF) && (subjZ != 0))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f64( a, "  " );
                writeCase_z_ui32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_f64_z_ui64_x(
     uint_fast64_t trueFunction( float64_t, bool ), bool exact )
{
    int count;
    float64_t a;
    uint_fast64_t subjZ;
    uint_fast8_t subjFlags;
    uint_fast64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f64( &a );
        readVerInput_ui64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != UINT64_C( 0xFFFFFFFFFFFFFFFF )) && (subjZ != 0))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f64( a, "  " );
                writeCase_z_ui64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_f64_z_i32_x(
     int_fast32_t trueFunction( float64_t, bool ), bool exact )
{
    int count;
    float64_t a;
    int_fast32_t subjZ;
    uint_fast8_t subjFlags;
    int_fast32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f64( &a );
        readVerInput_i32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != 0x7FFFFFFF) && (subjZ != -0x7FFFFFFF - 1)
                        && (! f64_isNaN( a ) || (subjZ != 0)))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f64( a, "  " );
                writeCase_z_i32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_f64_z_i64_x(
     int_fast64_t trueFunction( float64_t, bool ), bool exact )
{
    int count;
    float64_t a;
    int_fast64_t subjZ;
    uint_fast8_t subjFlags;
    int_fast64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f64( &a );
        readVerInput_i64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != INT64_C( 0x7FFFFFFFFFFFFFFF ))
                        && (subjZ != -INT64_C( 0x7FFFFFFFFFFFFFFF ) - 1)
                        && (! f64_isNaN( a ) || (subjZ != 0)))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f64( a, "  " );
                writeCase_z_i64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#ifdef FLOAT16

void ver_a_f64_z_f16( float16_t trueFunction( float64_t ) )
{
    int count;
    float64_t a;
    float16_t subjZ;
    uint_fast8_t subjFlags;
    float16_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f64( &a );
        readVerInput_f16( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f16_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && f64_isSignalingNaN( a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f16_isNaN( trueZ )
                || ! f16_isNaN( subjZ )
                || f16_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f64( a, "  " );
                writeCase_z_f16( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

void ver_a_f64_z_f32( float32_t trueFunction( float64_t ) )
{
    int count;
    float64_t a;
    float32_t subjZ;
    uint_fast8_t subjFlags;
    float32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f64( &a );
        readVerInput_f32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f32_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && f64_isSignalingNaN( a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f32_isNaN( trueZ )
                || ! f32_isNaN( subjZ )
                || f32_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f64( a, "  " );
                writeCase_z_f32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#ifdef EXTFLOAT80

void ver_a_f64_z_extF80( void trueFunction( float64_t, extFloat80_t * ) )
{
    int count;
    float64_t a;
    extFloat80_t subjZ;
    uint_fast8_t subjFlags;
    extFloat80_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f64( &a );
        readVerInput_extF80( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueFunction( a, &trueZ );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! extF80M_same( &trueZ, &subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && f64_isSignalingNaN( a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! extF80M_isNaN( &trueZ )
                || ! extF80M_isNaN( &subjZ )
                || extF80M_isSignalingNaN( &subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f64( a, "\n\t" );
                writeCase_z_extF80M( &trueZ, trueFlags, &subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

#ifdef FLOAT128

void ver_a_f64_z_f128( void trueFunction( float64_t, float128_t * ) )
{
    int count;
    float64_t a;
    float128_t subjZ;
    uint_fast8_t subjFlags;
    float128_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f64( &a );
        readVerInput_f128( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueFunction( a, &trueZ );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f128M_same( &trueZ, &subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && f64_isSignalingNaN( a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f128M_isNaN( &trueZ )
                || ! f128M_isNaN( &subjZ )
                || f128M_isSignalingNaN( &subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f64( a, "  " );
                writeCase_z_f128M( &trueZ, trueFlags, &subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

void ver_az_f64( float64_t trueFunction( float64_t ) )
{
    int count;
    float64_t a, subjZ;
    uint_fast8_t subjFlags;
    float64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f64( &a );
        readVerInput_f64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f64_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && f64_isSignalingNaN( a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f64_isNaN( trueZ )
                || ! f64_isNaN( subjZ )
                || f64_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f64( a, "\n\t" );
                writeCase_z_f64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_az_f64_rx(
     float64_t trueFunction( float64_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int count;
    float64_t a, subjZ;
    uint_fast8_t subjFlags;
    float64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f64( &a );
        readVerInput_f64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, roundingMode, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f64_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && f64_isSignalingNaN( a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f64_isNaN( trueZ )
                || ! f64_isNaN( subjZ )
                || f64_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f64( a, "\n\t" );
                writeCase_z_f64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void ver_abz_f64( float64_t trueFunction( float64_t, float64_t ) )
{
    int count;
    float64_t a, b, subjZ;
    uint_fast8_t subjFlags;
    float64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f64( &a );
        readVerInput_f64( &b );
        readVerInput_f64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, b );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f64_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if (
                ! verCases_checkNaNs
                    && (f64_isSignalingNaN( a ) || f64_isSignalingNaN( b ))
            ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f64_isNaN( trueZ )
                || ! f64_isNaN( subjZ )
                || f64_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_ab_f64( a, b, "\n\t" );
                writeCase_z_f64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void ver_abcz_f64( float64_t trueFunction( float64_t, float64_t, float64_t ) )
{
    int count;
    float64_t a, b, c, subjZ;
    uint_fast8_t subjFlags;
    float64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f64( &a );
        readVerInput_f64( &b );
        readVerInput_f64( &c );
        readVerInput_f64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, b, c );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f64_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if (
                ! verCases_checkNaNs
                    && (f64_isSignalingNaN( a ) || f64_isSignalingNaN( b )
                             || f64_isSignalingNaN( c ))
            ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f64_isNaN( trueZ )
                || ! f64_isNaN( subjZ )
                || f64_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_abc_f64( a, b, c );
                writeCase_z_f64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void ver_ab_f64_z_bool( bool trueFunction( float64_t, float64_t ) )
{
    int count;
    float64_t a, b;
    bool subjZ;
    uint_fast8_t subjFlags;
    bool trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f64( &a );
        readVerInput_f64( &b );
        readVerInput_bool( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( a, b );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                ! verCases_checkNaNs
                    && (f64_isSignalingNaN( a ) || f64_isSignalingNaN( b ))
            ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_ab_f64( a, b, "  " );
                writeCase_z_bool( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

#ifdef EXTFLOAT80

void
 ver_a_extF80_z_ui32_rx(
     uint_fast32_t trueFunction( const extFloat80_t *, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int count;
    extFloat80_t a;
    uint_fast32_t subjZ;
    uint_fast8_t subjFlags;
    uint_fast32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_extF80( &a );
        readVerInput_ui32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &a, roundingMode, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != 0xFFFFFFFF) && (subjZ != 0))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_extF80M( &a, "  " );
                writeCase_z_ui32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_extF80_z_ui64_rx(
     uint_fast64_t trueFunction( const extFloat80_t *, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int count;
    extFloat80_t a;
    uint_fast64_t subjZ;
    uint_fast8_t subjFlags;
    uint_fast64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_extF80( &a );
        readVerInput_ui64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &a, roundingMode, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != UINT64_C( 0xFFFFFFFFFFFFFFFF )) && (subjZ != 0))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_extF80M( &a, "\n\t" );
                writeCase_z_ui64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_extF80_z_i32_rx(
     int_fast32_t trueFunction( const extFloat80_t *, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int count;
    extFloat80_t a;
    int_fast32_t subjZ;
    uint_fast8_t subjFlags;
    int_fast32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_extF80( &a );
        readVerInput_i32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &a, roundingMode, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != 0x7FFFFFFF) && (subjZ != -0x7FFFFFFF - 1)
                        && (! extF80M_isNaN( &a ) || (subjZ != 0)))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_extF80M( &a, "  " );
                writeCase_z_i32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_extF80_z_i64_rx(
     int_fast64_t trueFunction( const extFloat80_t *, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int count;
    extFloat80_t a;
    int_fast64_t subjZ;
    uint_fast8_t subjFlags;
    int_fast64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_extF80( &a );
        readVerInput_i64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &a, roundingMode, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != INT64_C( 0x7FFFFFFFFFFFFFFF ))
                        && (subjZ != -INT64_C( 0x7FFFFFFFFFFFFFFF ) - 1)
                        && (! extF80M_isNaN( &a ) || (subjZ != 0)))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_extF80M( &a, "\n\t" );
                writeCase_z_i64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_extF80_z_ui32_x(
     uint_fast32_t trueFunction( const extFloat80_t *, bool ), bool exact )
{
    int count;
    extFloat80_t a;
    uint_fast32_t subjZ;
    uint_fast8_t subjFlags;
    uint_fast32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_extF80( &a );
        readVerInput_ui32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &a, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != 0xFFFFFFFF) && (subjZ != 0))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_extF80M( &a, "  " );
                writeCase_z_ui32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_extF80_z_ui64_x(
     uint_fast64_t trueFunction( const extFloat80_t *, bool ), bool exact )
{
    int count;
    extFloat80_t a;
    uint_fast64_t subjZ;
    uint_fast8_t subjFlags;
    uint_fast64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_extF80( &a );
        readVerInput_ui64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &a, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != UINT64_C( 0xFFFFFFFFFFFFFFFF )) && (subjZ != 0))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_extF80M( &a, "\n\t" );
                writeCase_z_ui64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_extF80_z_i32_x(
     int_fast32_t trueFunction( const extFloat80_t *, bool ), bool exact )
{
    int count;
    extFloat80_t a;
    int_fast32_t subjZ;
    uint_fast8_t subjFlags;
    int_fast32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_extF80( &a );
        readVerInput_i32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &a, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != 0x7FFFFFFF) && (subjZ != -0x7FFFFFFF - 1)
                        && (! extF80M_isNaN( &a ) || (subjZ != 0)))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_extF80M( &a, "  " );
                writeCase_z_i32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_extF80_z_i64_x(
     int_fast64_t trueFunction( const extFloat80_t *, bool ), bool exact )
{
    int count;
    extFloat80_t a;
    int_fast64_t subjZ;
    uint_fast8_t subjFlags;
    int_fast64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_extF80( &a );
        readVerInput_i64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &a, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != INT64_C( 0x7FFFFFFFFFFFFFFF ))
                        && (subjZ != -INT64_C( 0x7FFFFFFFFFFFFFFF ) - 1)
                        && (! extF80M_isNaN( &a ) || (subjZ != 0)))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_extF80M( &a, "\n\t" );
                writeCase_z_i64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#ifdef FLOAT16

void ver_a_extF80_z_f16( float16_t trueFunction( const extFloat80_t * ) )
{
    int count;
    extFloat80_t a;
    float16_t subjZ;
    uint_fast8_t subjFlags;
    float16_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_extF80( &a );
        readVerInput_f16( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &a );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f16_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && extF80M_isSignalingNaN( &a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f16_isNaN( trueZ )
                || ! f16_isNaN( subjZ )
                || f16_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_extF80M( &a, "  " );
                writeCase_z_f16( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

void ver_a_extF80_z_f32( float32_t trueFunction( const extFloat80_t * ) )
{
    int count;
    extFloat80_t a;
    float32_t subjZ;
    uint_fast8_t subjFlags;
    float32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_extF80( &a );
        readVerInput_f32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &a );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f32_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && extF80M_isSignalingNaN( &a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f32_isNaN( trueZ )
                || ! f32_isNaN( subjZ )
                || f32_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_extF80M( &a, "  " );
                writeCase_z_f32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#ifdef FLOAT64

void ver_a_extF80_z_f64( float64_t trueFunction( const extFloat80_t * ) )
{
    int count;
    extFloat80_t a;
    float64_t subjZ;
    uint_fast8_t subjFlags;
    float64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_extF80( &a );
        readVerInput_f64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &a );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f64_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && extF80M_isSignalingNaN( &a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f64_isNaN( trueZ )
                || ! f64_isNaN( subjZ )
                || f64_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_extF80M( &a, "\n\t" );
                writeCase_z_f64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

#ifdef FLOAT128

void
 ver_a_extF80_z_f128( void trueFunction( const extFloat80_t *, float128_t * ) )
{
    int count;
    extFloat80_t a;
    float128_t subjZ;
    uint_fast8_t subjFlags;
    float128_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_extF80( &a );
        readVerInput_f128( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueFunction( &a, &trueZ );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f128M_same( &trueZ, &subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && extF80M_isSignalingNaN( &a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f128M_isNaN( &trueZ )
                || ! f128M_isNaN( &subjZ )
                || f128M_isSignalingNaN( &subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_extF80M( &a, "  " );
                writeCase_z_f128M( &trueZ, trueFlags, &subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

void ver_az_extF80( void trueFunction( const extFloat80_t *, extFloat80_t * ) )
{
    int count;
    extFloat80_t a, subjZ;
    uint_fast8_t subjFlags;
    extFloat80_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_extF80( &a );
        readVerInput_extF80( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueFunction( &a, &trueZ );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! extF80M_same( &trueZ, &subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && extF80M_isSignalingNaN( &a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! extF80M_isNaN( &trueZ )
                || ! extF80M_isNaN( &subjZ )
                || extF80M_isSignalingNaN( &subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_extF80M( &a, "\n\t" );
                writeCase_z_extF80M( &trueZ, trueFlags, &subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_az_extF80_rx(
     void
      trueFunction( const extFloat80_t *, uint_fast8_t, bool, extFloat80_t * ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int count;
    extFloat80_t a, subjZ;
    uint_fast8_t subjFlags;
    extFloat80_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_extF80( &a );
        readVerInput_extF80( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueFunction( &a, roundingMode, exact, &trueZ );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! extF80M_same( &trueZ, &subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && extF80M_isSignalingNaN( &a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! extF80M_isNaN( &trueZ )
                || ! extF80M_isNaN( &subjZ )
                || extF80M_isSignalingNaN( &subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_extF80M( &a, "\n\t" );
                writeCase_z_extF80M( &trueZ, trueFlags, &subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_abz_extF80(
     void
      trueFunction(
          const extFloat80_t *, const extFloat80_t *, extFloat80_t * )
 )
{
    int count;
    extFloat80_t a, b, subjZ;
    uint_fast8_t subjFlags;
    extFloat80_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_extF80( &a );
        readVerInput_extF80( &b );
        readVerInput_extF80( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueFunction( &a, &b, &trueZ );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! extF80M_same( &trueZ, &subjZ ) || (trueFlags != subjFlags) ) {
            if (
                ! verCases_checkNaNs
                    && (extF80M_isSignalingNaN( &a )
                            || extF80M_isSignalingNaN( &b ))
            ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! extF80M_isNaN( &trueZ )
                || ! extF80M_isNaN( &subjZ )
                || extF80M_isSignalingNaN( &subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_ab_extF80M( &a, &b, "\n\t" );
                writeCase_z_extF80M( &trueZ, trueFlags, &subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_ab_extF80_z_bool(
     bool trueFunction( const extFloat80_t *, const extFloat80_t * ) )
{
    int count;
    extFloat80_t a, b;
    bool subjZ;
    uint_fast8_t subjFlags;
    bool trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_extF80( &a );
        readVerInput_extF80( &b );
        readVerInput_bool( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &a, &b );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                ! verCases_checkNaNs
                    && (extF80M_isSignalingNaN( &a )
                            || extF80M_isSignalingNaN( &b ))
            ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_ab_extF80M( &a, &b, "  " );
                writeCase_z_bool( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

#ifdef FLOAT128

void
 ver_a_f128_z_ui32_rx(
     uint_fast32_t trueFunction( const float128_t *, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int count;
    float128_t a;
    uint_fast32_t subjZ;
    uint_fast8_t subjFlags;
    uint_fast32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f128( &a );
        readVerInput_ui32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &a, roundingMode, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != 0xFFFFFFFF) && (subjZ != 0))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f128M( &a, "  " );
                writeCase_z_ui32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_f128_z_ui64_rx(
     uint_fast64_t trueFunction( const float128_t *, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int count;
    float128_t a;
    uint_fast64_t subjZ;
    uint_fast8_t subjFlags;
    uint_fast64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f128( &a );
        readVerInput_ui64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &a, roundingMode, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != UINT64_C( 0xFFFFFFFFFFFFFFFF )) && (subjZ != 0))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f128M( &a, "\n\t" );
                writeCase_z_ui64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_f128_z_i32_rx(
     int_fast32_t trueFunction( const float128_t *, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int count;
    float128_t a;
    int_fast32_t subjZ;
    uint_fast8_t subjFlags;
    int_fast32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f128( &a );
        readVerInput_i32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &a, roundingMode, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != 0x7FFFFFFF) && (subjZ != -0x7FFFFFFF - 1)
                        && (! f128M_isNaN( &a ) || (subjZ != 0)))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f128M( &a, "  " );
                writeCase_z_i32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_f128_z_i64_rx(
     int_fast64_t trueFunction( const float128_t *, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int count;
    float128_t a;
    int_fast64_t subjZ;
    uint_fast8_t subjFlags;
    int_fast64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f128( &a );
        readVerInput_i64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &a, roundingMode, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != INT64_C( 0x7FFFFFFFFFFFFFFF ))
                        && (subjZ != -INT64_C( 0x7FFFFFFFFFFFFFFF ) - 1)
                        && (! f128M_isNaN( &a ) || (subjZ != 0)))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f128M( &a, "\n\t" );
                writeCase_z_i64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_f128_z_ui32_x(
     uint_fast32_t trueFunction( const float128_t *, bool ), bool exact )
{
    int count;
    float128_t a;
    uint_fast32_t subjZ;
    uint_fast8_t subjFlags;
    uint_fast32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f128( &a );
        readVerInput_ui32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &a, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != 0xFFFFFFFF) && (subjZ != 0))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f128M( &a, "  " );
                writeCase_z_ui32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_f128_z_ui64_x(
     uint_fast64_t trueFunction( const float128_t *, bool ), bool exact )
{
    int count;
    float128_t a;
    uint_fast64_t subjZ;
    uint_fast8_t subjFlags;
    uint_fast64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f128( &a );
        readVerInput_ui64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &a, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != UINT64_C( 0xFFFFFFFFFFFFFFFF )) && (subjZ != 0))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f128M( &a, "\n\t" );
                writeCase_z_ui64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_f128_z_i32_x(
     int_fast32_t trueFunction( const float128_t *, bool ), bool exact )
{
    int count;
    float128_t a;
    int_fast32_t subjZ;
    uint_fast8_t subjFlags;
    int_fast32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f128( &a );
        readVerInput_i32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &a, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != 0x7FFFFFFF) && (subjZ != -0x7FFFFFFF - 1)
                        && (! f128M_isNaN( &a ) || (subjZ != 0)))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f128M( &a, "  " );
                writeCase_z_i32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_a_f128_z_i64_x(
     int_fast64_t trueFunction( const float128_t *, bool ), bool exact )
{
    int count;
    float128_t a;
    int_fast64_t subjZ;
    uint_fast8_t subjFlags;
    int_fast64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f128( &a );
        readVerInput_i64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &a, exact );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                   verCases_checkInvInts
                || (trueFlags != softfloat_flag_invalid)
                || (subjFlags != softfloat_flag_invalid)
                || ((subjZ != INT64_C( 0x7FFFFFFFFFFFFFFF ))
                        && (subjZ != -INT64_C( 0x7FFFFFFFFFFFFFFF ) - 1)
                        && (! f128M_isNaN( &a ) || (subjZ != 0)))
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f128M( &a, "\n\t" );
                writeCase_z_i64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#ifdef FLOAT16

void ver_a_f128_z_f16( float16_t trueFunction( const float128_t * ) )
{
    int count;
    float128_t a;
    float16_t subjZ;
    uint_fast8_t subjFlags;
    float16_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f128( &a );
        readVerInput_f16( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &a );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f16_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && f128M_isSignalingNaN( &a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f16_isNaN( trueZ )
                || ! f16_isNaN( subjZ )
                || f16_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f128M( &a, "  " );
                writeCase_z_f16( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

void ver_a_f128_z_f32( float32_t trueFunction( const float128_t * ) )
{
    int count;
    float128_t a;
    float32_t subjZ;
    uint_fast8_t subjFlags;
    float32_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f128( &a );
        readVerInput_f32( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &a );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f32_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && f128M_isSignalingNaN( &a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f32_isNaN( trueZ )
                || ! f32_isNaN( subjZ )
                || f32_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f128M( &a, "\n\t" );
                writeCase_z_f32( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#ifdef FLOAT64

void ver_a_f128_z_f64( float64_t trueFunction( const float128_t * ) )
{
    int count;
    float128_t a;
    float64_t subjZ;
    uint_fast8_t subjFlags;
    float64_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f128( &a );
        readVerInput_f64( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &a );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f64_same( trueZ, subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && f128M_isSignalingNaN( &a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f64_isNaN( trueZ )
                || ! f64_isNaN( subjZ )
                || f64_isSignalingNaN( subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f128M( &a, "\n\t" );
                writeCase_z_f64( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

#ifdef EXTFLOAT80

void
 ver_a_f128_z_extF80( void trueFunction( const float128_t *, extFloat80_t * ) )
{
    int count;
    float128_t a;
    extFloat80_t subjZ;
    uint_fast8_t subjFlags;
    extFloat80_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f128( &a );
        readVerInput_extF80( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueFunction( &a, &trueZ );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! extF80M_same( &trueZ, &subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && f128M_isSignalingNaN( &a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! extF80M_isNaN( &trueZ )
                || ! extF80M_isNaN( &subjZ )
                || extF80M_isSignalingNaN( &subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f128M( &a, "\n\t" );
                writeCase_z_extF80M( &trueZ, trueFlags, &subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

void ver_az_f128( void trueFunction( const float128_t *, float128_t * ) )
{
    int count;
    float128_t a, subjZ;
    uint_fast8_t subjFlags;
    float128_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f128( &a );
        readVerInput_f128( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueFunction( &a, &trueZ );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f128M_same( &trueZ, &subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && f128M_isSignalingNaN( &a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f128M_isNaN( &trueZ )
                || ! f128M_isNaN( &subjZ )
                || f128M_isSignalingNaN( &subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f128M( &a, "  " );
                writeCase_z_f128M( &trueZ, trueFlags, &subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_az_f128_rx(
     void trueFunction( const float128_t *, uint_fast8_t, bool, float128_t * ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int count;
    float128_t a, subjZ;
    uint_fast8_t subjFlags;
    float128_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f128( &a );
        readVerInput_f128( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueFunction( &a, roundingMode, exact, &trueZ );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f128M_same( &trueZ, &subjZ ) || (trueFlags != subjFlags) ) {
            if ( ! verCases_checkNaNs && f128M_isSignalingNaN( &a ) ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f128M_isNaN( &trueZ )
                || ! f128M_isNaN( &subjZ )
                || f128M_isSignalingNaN( &subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_a_f128M( &a, "  " );
                writeCase_z_f128M( &trueZ, trueFlags, &subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_abz_f128(
     void trueFunction( const float128_t *, const float128_t *, float128_t * )
 )
{
    int count;
    float128_t a, b, subjZ;
    uint_fast8_t subjFlags;
    float128_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f128( &a );
        readVerInput_f128( &b );
        readVerInput_f128( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueFunction( &a, &b, &trueZ );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f128M_same( &trueZ, &subjZ ) || (trueFlags != subjFlags) ) {
            if (
                ! verCases_checkNaNs
                    && (f128M_isSignalingNaN( &a )
                            || f128M_isSignalingNaN( &b ))
            ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f128M_isNaN( &trueZ )
                || ! f128M_isNaN( &subjZ )
                || f128M_isSignalingNaN( &subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_ab_f128M( &a, &b );
                writeCase_z_f128M( &trueZ, trueFlags, &subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_abcz_f128(
     void
      trueFunction(
          const float128_t *,
          const float128_t *,
          const float128_t *,
          float128_t *
      )
 )
{
    int count;
    float128_t a, b, c, subjZ;
    uint_fast8_t subjFlags;
    float128_t trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f128( &a );
        readVerInput_f128( &b );
        readVerInput_f128( &c );
        readVerInput_f128( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueFunction( &a, &b, &c, &trueZ );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( ! f128M_same( &trueZ, &subjZ ) || (trueFlags != subjFlags) ) {
            if (
                ! verCases_checkNaNs
                    && (   f128M_isSignalingNaN( &a )
                        || f128M_isSignalingNaN( &b )
                        || f128M_isSignalingNaN( &c )
                       )
            ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if (
                   verCases_checkNaNs
                || ! f128M_isNaN( &trueZ )
                || ! f128M_isNaN( &subjZ )
                || f128M_isSignalingNaN( &subjZ )
                || (trueFlags != subjFlags)
            ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_abc_f128M( &a, &b, &c );
                writeCase_z_f128M( &trueZ, trueFlags, &subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

void
 ver_ab_f128_z_bool(
     bool trueFunction( const float128_t *, const float128_t * ) )
{
    int count;
    float128_t a, b;
    bool subjZ;
    uint_fast8_t subjFlags;
    bool trueZ;
    uint_fast8_t trueFlags;

    verCases_errorCount = 0;
    verCases_tenThousandsCount = 0;
    count = 10000;
    while ( ! atEndOfInput() ) {
        readVerInput_f128( &a );
        readVerInput_f128( &b );
        readVerInput_bool( &subjZ );
        readVerInput_flags( &subjFlags );
        *verLoops_trueFlagsPtr = 0;
        trueZ = trueFunction( &a, &b );
        trueFlags = *verLoops_trueFlagsPtr;
        --count;
        if ( ! count ) {
            verCases_perTenThousand();
            count = 10000;
        }
        if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
            if (
                ! verCases_checkNaNs
                    && (f128M_isSignalingNaN( &a )
                            || f128M_isSignalingNaN( &b ))
            ) {
                trueFlags |= softfloat_flag_invalid;
            }
            if ( (trueZ != subjZ) || (trueFlags != subjFlags) ) {
                ++verCases_errorCount;
                verCases_writeErrorFound( 10000 - count );
                writeCase_ab_f128M( &a, &b );
                writeCase_z_bool( trueZ, trueFlags, subjZ, subjFlags );
                if ( verCases_errorCount == verCases_maxErrorCount ) break;
            }
        }
    }
    verCases_writeTestsPerformed( 10000 - count );

}

#endif

