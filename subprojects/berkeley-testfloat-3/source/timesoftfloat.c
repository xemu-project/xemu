
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
#include <time.h>
#include "platform.h"
#include "uint128.h"
#include "fail.h"
#include "softfloat.h"
#include "functions.h"

enum { minIterations = 1000 };

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

static const char *functionNamePtr;
static uint_fast8_t roundingPrecision;
static int roundingCode;
static int tininessCode;
static bool usesExact;
static bool exact;

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

static void reportTime( int_fast64_t count, clock_t clockTicks )
{
    static const char *roundingModeNames[NUM_ROUNDINGMODES] = {
        0,
        ", rounding near_even",
        ", rounding minMag",
        ", rounding min",
        ", rounding max",
        ", rounding near_maxMag",
#ifdef FLOAT_ROUND_ODD
        ", rounding odd"
#endif
    };

    printf(
        "%9.4f Mop/s: %s",
        count / ((float) clockTicks / CLOCKS_PER_SEC) / 1000000,
        functionNamePtr
    );
    if ( roundingCode ) {
#ifdef EXTFLOAT80
        if ( roundingPrecision ) {
            printf( ", precision %d", (int) roundingPrecision );
        }
#endif
        fputs( roundingModeNames[roundingCode], stdout );
        if ( tininessCode ) {
            fputs(
                (tininessCode == TININESS_BEFORE_ROUNDING)
                    ? ", tininess before rounding"
                    : ", tininess after rounding",
                stdout
            );
        }
    }
    if ( usesExact ) fputs( exact ? ", exact" : ", not exact", stdout );
    fputc( '\n', stdout );
    fflush( stdout );

}

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

#ifdef FLOAT16
union ui16_f16 { uint16_t ui; float16_t f; };
#endif
union ui32_f32 { uint32_t ui; float32_t f; };
#ifdef FLOAT64
union ui64_f64 { uint64_t ui; float64_t f; };
#endif

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

enum { numInputs_ui32 = 32 };

static const uint32_t inputs_ui32[numInputs_ui32] = {
    0x00004487, 0x405CF80F, 0x00000000, 0x000002FC,
    0x000DFFFE, 0x0C8EF795, 0x0FFFEE01, 0x000006CA,
    0x00009BFE, 0x00B79D1D, 0x60001002, 0x00000049,
    0x0BFF7FFF, 0x0000F37A, 0x0011DFFE, 0x00000006,
    0x000FDFFA, 0x0000082F, 0x10200003, 0x2172089B,
    0x00003E02, 0x000019E8, 0x0008FFFE, 0x000004A4,
    0x00208002, 0x07C42FBF, 0x0FFFE3FF, 0x040B9F13,
    0x40000008, 0x0001BF56, 0x000017F6, 0x000A908A
};

#ifdef FLOAT16

static void time_a_ui32_z_f16( float16_t function( uint32_t ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_ui32[inputNum] );
            inputNum = (inputNum + 1) & (numInputs_ui32 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_ui32[inputNum] );
        inputNum = (inputNum + 1) & (numInputs_ui32 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

static void time_a_ui32_z_f32( float32_t function( uint32_t ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_ui32[inputNum] );
            inputNum = (inputNum + 1) & (numInputs_ui32 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_ui32[inputNum] );
        inputNum = (inputNum + 1) & (numInputs_ui32 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#ifdef FLOAT64

static void time_a_ui32_z_f64( float64_t function( uint32_t ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_ui32[inputNum] );
            inputNum = (inputNum + 1) & (numInputs_ui32 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_ui32[inputNum] );
        inputNum = (inputNum + 1) & (numInputs_ui32 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

#ifdef EXTFLOAT80

static void time_a_ui32_z_extF80( void function( uint32_t, extFloat80_t * ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    extFloat80_t z;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_ui32[inputNum], &z );
            inputNum = (inputNum + 1) & (numInputs_ui32 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_ui32[inputNum], &z );
        inputNum = (inputNum + 1) & (numInputs_ui32 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

#ifdef FLOAT128

static void time_a_ui32_z_f128( void function( uint32_t, float128_t * ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    float128_t z;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_ui32[inputNum], &z );
            inputNum = (inputNum + 1) & (numInputs_ui32 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_ui32[inputNum], &z );
        inputNum = (inputNum + 1) & (numInputs_ui32 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

enum { numInputs_ui64 = 32 };

static const int64_t inputs_ui64[numInputs_ui64] = {
    UINT64_C( 0x04003C0000000001 ), UINT64_C( 0x0000000003C589BC ),
    UINT64_C( 0x00000000400013FE ), UINT64_C( 0x0000000000186171 ),
    UINT64_C( 0x0000000000010406 ), UINT64_C( 0x000002861920038D ),
    UINT64_C( 0x0000000010001DFF ), UINT64_C( 0x22E5F0F387AEC8F0 ),
    UINT64_C( 0x00007C0000010002 ), UINT64_C( 0x00756EBD1AD0C1C7 ),
    UINT64_C( 0x0003FDFFFFFFFFBE ), UINT64_C( 0x0007D0FB2C2CA951 ),
    UINT64_C( 0x0007FC0007FFFFFE ), UINT64_C( 0x0000001F942B18BB ),
    UINT64_C( 0x0000080101FFFFFE ), UINT64_C( 0x000000000000F688 ),
    UINT64_C( 0x000000000008BFFF ), UINT64_C( 0x0000000006F5AF08 ),
    UINT64_C( 0x0021008000000002 ), UINT64_C( 0x0000000000000003 ),
    UINT64_C( 0x3FFFFFFFFF80007D ), UINT64_C( 0x0000000000000078 ),
    UINT64_C( 0x0007FFFFFF802003 ), UINT64_C( 0x1BBC775B78016AB0 ),
    UINT64_C( 0x0006FFE000000002 ), UINT64_C( 0x0002B89854671BC1 ),
    UINT64_C( 0x0000010001FFFFE2 ), UINT64_C( 0x00000000000FB103 ),
    UINT64_C( 0x07FFFFFFFFFFF7FF ), UINT64_C( 0x00036155C7076FB0 ),
    UINT64_C( 0x00000020FBFFFFFE ), UINT64_C( 0x0000099AE6455357 )
};

#ifdef FLOAT16

static void time_a_ui64_z_f16( float16_t function( uint64_t ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_ui64[inputNum] );
            inputNum = (inputNum + 1) & (numInputs_ui64 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_ui64[inputNum] );
        inputNum = (inputNum + 1) & (numInputs_ui64 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

static void time_a_ui64_z_f32( float32_t function( uint64_t ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_ui64[inputNum] );
            inputNum = (inputNum + 1) & (numInputs_ui64 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_ui64[inputNum] );
        inputNum = (inputNum + 1) & (numInputs_ui64 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#ifdef FLOAT64

static void time_a_ui64_z_f64( float64_t function( uint64_t ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_ui64[inputNum] );
            inputNum = (inputNum + 1) & (numInputs_ui64 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_ui64[inputNum] );
        inputNum = (inputNum + 1) & (numInputs_ui64 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

#ifdef EXTFLOAT80

static void time_a_ui64_z_extF80( void function( uint64_t, extFloat80_t * ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    extFloat80_t z;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_ui64[inputNum], &z );
            inputNum = (inputNum + 1) & (numInputs_ui64 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_ui64[inputNum], &z );
        inputNum = (inputNum + 1) & (numInputs_ui64 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

#ifdef FLOAT128

static void time_a_ui64_z_f128( void function( uint64_t, float128_t * ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    float128_t z;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_ui64[inputNum], &z );
            inputNum = (inputNum + 1) & (numInputs_ui64 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_ui64[inputNum], &z );
        inputNum = (inputNum + 1) & (numInputs_ui64 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

enum { numInputs_i32 = 32 };

static const int32_t inputs_i32[numInputs_i32] = {
    -0x00004487,  0x405CF80F,  0x00000000, -0x000002FC,
    -0x000DFFFE,  0x0C8EF795, -0x0FFFEE01,  0x000006CA,
     0x00009BFE, -0x00B79D1D, -0x60001002, -0x00000049,
     0x0BFF7FFF,  0x0000F37A,  0x0011DFFE,  0x00000006,
    -0x000FDFFA, -0x0000082F,  0x10200003, -0x2172089B,
     0x00003E02,  0x000019E8,  0x0008FFFE, -0x000004A4,
    -0x00208002,  0x07C42FBF,  0x0FFFE3FF,  0x040B9F13,
    -0x40000008,  0x0001BF56,  0x000017F6,  0x000A908A
};

#ifdef FLOAT16

static void time_a_i32_z_f16( float16_t function( int32_t ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_i32[inputNum] );
            inputNum = (inputNum + 1) & (numInputs_i32 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_i32[inputNum] );
        inputNum = (inputNum + 1) & (numInputs_i32 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

static void time_a_i32_z_f32( float32_t function( int32_t ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_i32[inputNum] );
            inputNum = (inputNum + 1) & (numInputs_i32 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_i32[inputNum] );
        inputNum = (inputNum + 1) & (numInputs_i32 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#ifdef FLOAT64

static void time_a_i32_z_f64( float64_t function( int32_t ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_i32[inputNum] );
            inputNum = (inputNum + 1) & (numInputs_i32 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_i32[inputNum] );
        inputNum = (inputNum + 1) & (numInputs_i32 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

#ifdef EXTFLOAT80

static void time_a_i32_z_extF80( void function( int32_t, extFloat80_t * ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    extFloat80_t z;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_i32[inputNum], &z );
            inputNum = (inputNum + 1) & (numInputs_i32 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_i32[inputNum], &z );
        inputNum = (inputNum + 1) & (numInputs_i32 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

#ifdef FLOAT128

static void time_a_i32_z_f128( void function( int32_t, float128_t * ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    float128_t z;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_i32[inputNum], &z );
            inputNum = (inputNum + 1) & (numInputs_i32 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_i32[inputNum], &z );
        inputNum = (inputNum + 1) & (numInputs_i32 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

enum { numInputs_i64 = 32 };

static const int64_t inputs_i64[numInputs_i64] = {
    -INT64_C( 0x04003C0000000001 ),  INT64_C( 0x0000000003C589BC ),
     INT64_C( 0x00000000400013FE ),  INT64_C( 0x0000000000186171 ),
    -INT64_C( 0x0000000000010406 ), -INT64_C( 0x000002861920038D ),
     INT64_C( 0x0000000010001DFF ), -INT64_C( 0x22E5F0F387AEC8F0 ),
    -INT64_C( 0x00007C0000010002 ),  INT64_C( 0x00756EBD1AD0C1C7 ),
     INT64_C( 0x0003FDFFFFFFFFBE ),  INT64_C( 0x0007D0FB2C2CA951 ),
     INT64_C( 0x0007FC0007FFFFFE ),  INT64_C( 0x0000001F942B18BB ),
     INT64_C( 0x0000080101FFFFFE ), -INT64_C( 0x000000000000F688 ),
     INT64_C( 0x000000000008BFFF ),  INT64_C( 0x0000000006F5AF08 ),
    -INT64_C( 0x0021008000000002 ),  INT64_C( 0x0000000000000003 ),
     INT64_C( 0x3FFFFFFFFF80007D ),  INT64_C( 0x0000000000000078 ),
    -INT64_C( 0x0007FFFFFF802003 ),  INT64_C( 0x1BBC775B78016AB0 ),
    -INT64_C( 0x0006FFE000000002 ), -INT64_C( 0x0002B89854671BC1 ),
    -INT64_C( 0x0000010001FFFFE2 ), -INT64_C( 0x00000000000FB103 ),
     INT64_C( 0x07FFFFFFFFFFF7FF ), -INT64_C( 0x00036155C7076FB0 ),
     INT64_C( 0x00000020FBFFFFFE ),  INT64_C( 0x0000099AE6455357 )
};

#ifdef FLOAT16

static void time_a_i64_z_f16( float16_t function( int64_t ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_i64[inputNum] );
            inputNum = (inputNum + 1) & (numInputs_i64 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_i64[inputNum] );
        inputNum = (inputNum + 1) & (numInputs_i64 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

static void time_a_i64_z_f32( float32_t function( int64_t ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_i64[inputNum] );
            inputNum = (inputNum + 1) & (numInputs_i64 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_i64[inputNum] );
        inputNum = (inputNum + 1) & (numInputs_i64 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#ifdef FLOAT64

static void time_a_i64_z_f64( float64_t function( int64_t ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_i64[inputNum] );
            inputNum = (inputNum + 1) & (numInputs_i64 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_i64[inputNum] );
        inputNum = (inputNum + 1) & (numInputs_i64 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

#ifdef EXTFLOAT80

static void time_a_i64_z_extF80( void function( int64_t, extFloat80_t * ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    extFloat80_t z;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_i64[inputNum], &z );
            inputNum = (inputNum + 1) & (numInputs_i64 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_i64[inputNum], &z );
        inputNum = (inputNum + 1) & (numInputs_i64 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

#ifdef FLOAT128

static void time_a_i64_z_f128( void function( int64_t, float128_t * ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    float128_t z;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( inputs_i64[inputNum], &z );
            inputNum = (inputNum + 1) & (numInputs_i64 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( inputs_i64[inputNum], &z );
        inputNum = (inputNum + 1) & (numInputs_i64 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

#ifdef FLOAT16

enum { numInputs_f16 = 32 };

static const uint16_t inputs_F16UI[numInputs_f16] = {
    0x0BBA, 0x77FE, 0x084F, 0x9C0F, 0x7800, 0x4436, 0xCE67, 0x80F3,
    0x87EF, 0xC2FA, 0x7BFF, 0x13FE, 0x7BFE, 0x1C00, 0xAC46, 0xEAFA,
    0x3813, 0x4804, 0x385E, 0x8000, 0xB86C, 0x4B7D, 0xC7FD, 0xC97F,
    0x260C, 0x78EE, 0xB84F, 0x249E, 0x0D27, 0x37DC, 0x8400, 0xE8EF
};

static
void
 time_a_f16_z_ui32_rx(
     uint_fast32_t function( float16_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui16_f16 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F16UI[inputNum];
            function( uA.f, roundingMode, exact );
            inputNum = (inputNum + 1) & (numInputs_f16 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F16UI[inputNum];
        function( uA.f, roundingMode, exact );
        inputNum = (inputNum + 1) & (numInputs_f16 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_a_f16_z_ui64_rx(
     uint_fast64_t function( float16_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui16_f16 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F16UI[inputNum];
            function( uA.f, roundingMode, exact );
            inputNum = (inputNum + 1) & (numInputs_f16 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F16UI[inputNum];
        function( uA.f, roundingMode, exact );
        inputNum = (inputNum + 1) & (numInputs_f16 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_a_f16_z_i32_rx(
     int_fast32_t function( float16_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui16_f16 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F16UI[inputNum];
            function( uA.f, roundingMode, exact );
            inputNum = (inputNum + 1) & (numInputs_f16 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F16UI[inputNum];
        function( uA.f, roundingMode, exact );
        inputNum = (inputNum + 1) & (numInputs_f16 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_a_f16_z_i64_rx(
     int_fast64_t function( float16_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui16_f16 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F16UI[inputNum];
            function( uA.f, roundingMode, exact );
            inputNum = (inputNum + 1) & (numInputs_f16 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F16UI[inputNum];
        function( uA.f, roundingMode, exact );
        inputNum = (inputNum + 1) & (numInputs_f16 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_a_f16_z_ui32_x( uint_fast32_t function( float16_t, bool ), bool exact )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui16_f16 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F16UI[inputNum];
            function( uA.f, exact );
            inputNum = (inputNum + 1) & (numInputs_f16 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F16UI[inputNum];
        function( uA.f, exact );
        inputNum = (inputNum + 1) & (numInputs_f16 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_a_f16_z_ui64_x( uint_fast64_t function( float16_t, bool ), bool exact )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui16_f16 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F16UI[inputNum];
            function( uA.f, exact );
            inputNum = (inputNum + 1) & (numInputs_f16 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F16UI[inputNum];
        function( uA.f, exact );
        inputNum = (inputNum + 1) & (numInputs_f16 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void time_a_f16_z_i32_x( int_fast32_t function( float16_t, bool ), bool exact )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui16_f16 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F16UI[inputNum];
            function( uA.f, exact );
            inputNum = (inputNum + 1) & (numInputs_f16 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F16UI[inputNum];
        function( uA.f, exact );
        inputNum = (inputNum + 1) & (numInputs_f16 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void time_a_f16_z_i64_x( int_fast64_t function( float16_t, bool ), bool exact )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui16_f16 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F16UI[inputNum];
            function( uA.f, exact );
            inputNum = (inputNum + 1) & (numInputs_f16 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F16UI[inputNum];
        function( uA.f, exact );
        inputNum = (inputNum + 1) & (numInputs_f16 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static void time_a_f16_z_f32( float32_t function( float16_t ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui16_f16 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F16UI[inputNum];
            function( uA.f );
            inputNum = (inputNum + 1) & (numInputs_f16 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F16UI[inputNum];
        function( uA.f );
        inputNum = (inputNum + 1) & (numInputs_f16 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#ifdef FLOAT64

static void time_a_f16_z_f64( float64_t function( float16_t ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui16_f16 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F16UI[inputNum];
            function( uA.f );
            inputNum = (inputNum + 1) & (numInputs_f16 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F16UI[inputNum];
        function( uA.f );
        inputNum = (inputNum + 1) & (numInputs_f16 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

#ifdef EXTFLOAT80

static void time_a_f16_z_extF80( void function( float16_t, extFloat80_t * ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui16_f16 uA;
    extFloat80_t z;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F16UI[inputNum];
            function( uA.f, &z );
            inputNum = (inputNum + 1) & (numInputs_f16 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F16UI[inputNum];
        function( uA.f, &z );
        inputNum = (inputNum + 1) & (numInputs_f16 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

#ifdef FLOAT128

static void time_a_f16_z_f128( void function( float16_t, float128_t * ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui16_f16 uA;
    float128_t z;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F16UI[inputNum];
            function( uA.f, &z );
            inputNum = (inputNum + 1) & (numInputs_f16 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F16UI[inputNum];
        function( uA.f, &z );
        inputNum = (inputNum + 1) & (numInputs_f16 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

static
void
 time_az_f16_rx(
     float16_t function( float16_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui16_f16 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F16UI[inputNum];
            function( uA.f, roundingMode, exact );
            inputNum = (inputNum + 1) & (numInputs_f16 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F16UI[inputNum];
        function( uA.f, roundingMode, exact );
        inputNum = (inputNum + 1) & (numInputs_f16 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static void time_abz_f16( float16_t function( float16_t, float16_t ) )
{
    int_fast64_t count;
    int inputNumA, inputNumB;
    clock_t startClock;
    int_fast64_t i;
    union ui16_f16 uA, uB;
    clock_t endClock;

    count = 0;
    inputNumA = 0;
    inputNumB = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F16UI[inputNumA];
            uB.ui = inputs_F16UI[inputNumB];
            function( uA.f, uB.f );
            inputNumA = (inputNumA + 1) & (numInputs_f16 - 1);
            if ( ! inputNumA ) ++inputNumB;
            inputNumB = (inputNumB + 1) & (numInputs_f16 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNumA = 0;
    inputNumB = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F16UI[inputNumA];
        uB.ui = inputs_F16UI[inputNumB];
        function( uA.f, uB.f );
        inputNumA = (inputNumA + 1) & (numInputs_f16 - 1);
        if ( ! inputNumA ) ++inputNumB;
        inputNumB = (inputNumB + 1) & (numInputs_f16 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void time_abcz_f16( float16_t function( float16_t, float16_t, float16_t ) )
{
    int_fast64_t count;
    int inputNumA, inputNumB, inputNumC;
    clock_t startClock;
    int_fast64_t i;
    union ui16_f16 uA, uB, uC;
    clock_t endClock;

    count = 0;
    inputNumA = 0;
    inputNumB = 0;
    inputNumC = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F16UI[inputNumA];
            uB.ui = inputs_F16UI[inputNumB];
            uC.ui = inputs_F16UI[inputNumC];
            function( uA.f, uB.f, uC.f );
            inputNumA = (inputNumA + 1) & (numInputs_f16 - 1);
            if ( ! inputNumA ) ++inputNumB;
            inputNumB = (inputNumB + 1) & (numInputs_f16 - 1);
            if ( ! inputNumB ) ++inputNumC;
            inputNumC = (inputNumC + 1) & (numInputs_f16 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNumA = 0;
    inputNumB = 0;
    inputNumC = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F16UI[inputNumA];
        uB.ui = inputs_F16UI[inputNumB];
        uC.ui = inputs_F16UI[inputNumC];
        function( uA.f, uB.f, uC.f );
        inputNumA = (inputNumA + 1) & (numInputs_f16 - 1);
        if ( ! inputNumA ) ++inputNumB;
        inputNumB = (inputNumB + 1) & (numInputs_f16 - 1);
        if ( ! inputNumB ) ++inputNumC;
        inputNumC = (inputNumC + 1) & (numInputs_f16 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static void time_ab_f16_z_bool( bool function( float16_t, float16_t ) )
{
    int_fast64_t count;
    int inputNumA, inputNumB;
    clock_t startClock;
    int_fast64_t i;
    union ui16_f16 uA, uB;
    clock_t endClock;

    count = 0;
    inputNumA = 0;
    inputNumB = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F16UI[inputNumA];
            uB.ui = inputs_F16UI[inputNumB];
            function( uA.f, uB.f );
            inputNumA = (inputNumA + 1) & (numInputs_f16 - 1);
            if ( ! inputNumA ) ++inputNumB;
            inputNumB = (inputNumB + 1) & (numInputs_f16 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNumA = 0;
    inputNumB = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F16UI[inputNumA];
        uB.ui = inputs_F16UI[inputNumB];
        function( uA.f, uB.f );
        inputNumA = (inputNumA + 1) & (numInputs_f16 - 1);
        if ( ! inputNumA ) ++inputNumB;
        inputNumB = (inputNumB + 1) & (numInputs_f16 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static const uint16_t inputs_F16UI_pos[numInputs_f16] = {
    0x0BBA, 0x77FE, 0x084F, 0x1C0F, 0x7800, 0x4436, 0x4E67, 0x00F3,
    0x07EF, 0x42FA, 0x7BFF, 0x13FE, 0x7BFE, 0x1C00, 0x2C46, 0x6AFA,
    0x3813, 0x4804, 0x385E, 0x0000, 0x386C, 0x4B7D, 0x47FD, 0x497F,
    0x260C, 0x78EE, 0x384F, 0x249E, 0x0D27, 0x37DC, 0x0400, 0x68EF
};

static void time_az_f16_pos( float16_t function( float16_t ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui16_f16 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F16UI_pos[inputNum];
            function( uA.f );
            inputNum = (inputNum + 1) & (numInputs_f16 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F16UI_pos[inputNum];
        function( uA.f );
        inputNum = (inputNum + 1) & (numInputs_f16 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

enum { numInputs_f32 = 32 };

static const uint32_t inputs_F32UI[numInputs_f32] = {
    0x4EFA0000, 0xC1D0B328, 0x80000000, 0x3E69A31E,
    0xAF803EFF, 0x3F800000, 0x17BF8000, 0xE74A301A,
    0x4E010003, 0x7EE3C75D, 0xBD803FE0, 0xBFFEFF00,
    0x7981F800, 0x431FFFFC, 0xC100C000, 0x3D87EFFF,
    0x4103FEFE, 0xBC000007, 0xBF01F7FF, 0x4E6C6B5C,
    0xC187FFFE, 0xC58B9F13, 0x4F88007F, 0xDF004007,
    0xB7FFD7FE, 0x7E8001FB, 0x46EFFBFF, 0x31C10000,
    0xDB428661, 0x33F89B1F, 0xA3BFEFFF, 0x537BFFBE
};

static
void
 time_a_f32_z_ui32_rx(
     uint_fast32_t function( float32_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui32_f32 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F32UI[inputNum];
            function( uA.f, roundingMode, exact );
            inputNum = (inputNum + 1) & (numInputs_f32 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F32UI[inputNum];
        function( uA.f, roundingMode, exact );
        inputNum = (inputNum + 1) & (numInputs_f32 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_a_f32_z_ui64_rx(
     uint_fast64_t function( float32_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui32_f32 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F32UI[inputNum];
            function( uA.f, roundingMode, exact );
            inputNum = (inputNum + 1) & (numInputs_f32 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F32UI[inputNum];
        function( uA.f, roundingMode, exact );
        inputNum = (inputNum + 1) & (numInputs_f32 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_a_f32_z_i32_rx(
     int_fast32_t function( float32_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui32_f32 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F32UI[inputNum];
            function( uA.f, roundingMode, exact );
            inputNum = (inputNum + 1) & (numInputs_f32 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F32UI[inputNum];
        function( uA.f, roundingMode, exact );
        inputNum = (inputNum + 1) & (numInputs_f32 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_a_f32_z_i64_rx(
     int_fast64_t function( float32_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui32_f32 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F32UI[inputNum];
            function( uA.f, roundingMode, exact );
            inputNum = (inputNum + 1) & (numInputs_f32 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F32UI[inputNum];
        function( uA.f, roundingMode, exact );
        inputNum = (inputNum + 1) & (numInputs_f32 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_a_f32_z_ui32_x( uint_fast32_t function( float32_t, bool ), bool exact )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui32_f32 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F32UI[inputNum];
            function( uA.f, exact );
            inputNum = (inputNum + 1) & (numInputs_f32 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F32UI[inputNum];
        function( uA.f, exact );
        inputNum = (inputNum + 1) & (numInputs_f32 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_a_f32_z_ui64_x( uint_fast64_t function( float32_t, bool ), bool exact )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui32_f32 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F32UI[inputNum];
            function( uA.f, exact );
            inputNum = (inputNum + 1) & (numInputs_f32 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F32UI[inputNum];
        function( uA.f, exact );
        inputNum = (inputNum + 1) & (numInputs_f32 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void time_a_f32_z_i32_x( int_fast32_t function( float32_t, bool ), bool exact )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui32_f32 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F32UI[inputNum];
            function( uA.f, exact );
            inputNum = (inputNum + 1) & (numInputs_f32 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F32UI[inputNum];
        function( uA.f, exact );
        inputNum = (inputNum + 1) & (numInputs_f32 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void time_a_f32_z_i64_x( int_fast64_t function( float32_t, bool ), bool exact )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui32_f32 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F32UI[inputNum];
            function( uA.f, exact );
            inputNum = (inputNum + 1) & (numInputs_f32 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F32UI[inputNum];
        function( uA.f, exact );
        inputNum = (inputNum + 1) & (numInputs_f32 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#ifdef FLOAT16

static void time_a_f32_z_f16( float16_t function( float32_t ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui32_f32 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F32UI[inputNum];
            function( uA.f );
            inputNum = (inputNum + 1) & (numInputs_f32 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F32UI[inputNum];
        function( uA.f );
        inputNum = (inputNum + 1) & (numInputs_f32 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

#ifdef FLOAT64

static void time_a_f32_z_f64( float64_t function( float32_t ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui32_f32 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F32UI[inputNum];
            function( uA.f );
            inputNum = (inputNum + 1) & (numInputs_f32 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F32UI[inputNum];
        function( uA.f );
        inputNum = (inputNum + 1) & (numInputs_f32 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

#ifdef EXTFLOAT80

static void time_a_f32_z_extF80( void function( float32_t, extFloat80_t * ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui32_f32 uA;
    extFloat80_t z;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F32UI[inputNum];
            function( uA.f, &z );
            inputNum = (inputNum + 1) & (numInputs_f32 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F32UI[inputNum];
        function( uA.f, &z );
        inputNum = (inputNum + 1) & (numInputs_f32 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

#ifdef FLOAT128

static void time_a_f32_z_f128( void function( float32_t, float128_t * ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui32_f32 uA;
    float128_t z;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F32UI[inputNum];
            function( uA.f, &z );
            inputNum = (inputNum + 1) & (numInputs_f32 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F32UI[inputNum];
        function( uA.f, &z );
        inputNum = (inputNum + 1) & (numInputs_f32 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

static
void
 time_az_f32_rx(
     float32_t function( float32_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui32_f32 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F32UI[inputNum];
            function( uA.f, roundingMode, exact );
            inputNum = (inputNum + 1) & (numInputs_f32 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F32UI[inputNum];
        function( uA.f, roundingMode, exact );
        inputNum = (inputNum + 1) & (numInputs_f32 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static void time_abz_f32( float32_t function( float32_t, float32_t ) )
{
    int_fast64_t count;
    int inputNumA, inputNumB;
    clock_t startClock;
    int_fast64_t i;
    union ui32_f32 uA, uB;
    clock_t endClock;

    count = 0;
    inputNumA = 0;
    inputNumB = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F32UI[inputNumA];
            uB.ui = inputs_F32UI[inputNumB];
            function( uA.f, uB.f );
            inputNumA = (inputNumA + 1) & (numInputs_f32 - 1);
            if ( ! inputNumA ) ++inputNumB;
            inputNumB = (inputNumB + 1) & (numInputs_f32 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNumA = 0;
    inputNumB = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F32UI[inputNumA];
        uB.ui = inputs_F32UI[inputNumB];
        function( uA.f, uB.f );
        inputNumA = (inputNumA + 1) & (numInputs_f32 - 1);
        if ( ! inputNumA ) ++inputNumB;
        inputNumB = (inputNumB + 1) & (numInputs_f32 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void time_abcz_f32( float32_t function( float32_t, float32_t, float32_t ) )
{
    int_fast64_t count;
    int inputNumA, inputNumB, inputNumC;
    clock_t startClock;
    int_fast64_t i;
    union ui32_f32 uA, uB, uC;
    clock_t endClock;

    count = 0;
    inputNumA = 0;
    inputNumB = 0;
    inputNumC = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F32UI[inputNumA];
            uB.ui = inputs_F32UI[inputNumB];
            uC.ui = inputs_F32UI[inputNumC];
            function( uA.f, uB.f, uC.f );
            inputNumA = (inputNumA + 1) & (numInputs_f32 - 1);
            if ( ! inputNumA ) ++inputNumB;
            inputNumB = (inputNumB + 1) & (numInputs_f32 - 1);
            if ( ! inputNumB ) ++inputNumC;
            inputNumC = (inputNumC + 1) & (numInputs_f32 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNumA = 0;
    inputNumB = 0;
    inputNumC = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F32UI[inputNumA];
        uB.ui = inputs_F32UI[inputNumB];
        uC.ui = inputs_F32UI[inputNumC];
        function( uA.f, uB.f, uC.f );
        inputNumA = (inputNumA + 1) & (numInputs_f32 - 1);
        if ( ! inputNumA ) ++inputNumB;
        inputNumB = (inputNumB + 1) & (numInputs_f32 - 1);
        if ( ! inputNumB ) ++inputNumC;
        inputNumC = (inputNumC + 1) & (numInputs_f32 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static void time_ab_f32_z_bool( bool function( float32_t, float32_t ) )
{
    int_fast64_t count;
    int inputNumA, inputNumB;
    clock_t startClock;
    int_fast64_t i;
    union ui32_f32 uA, uB;
    clock_t endClock;

    count = 0;
    inputNumA = 0;
    inputNumB = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F32UI[inputNumA];
            uB.ui = inputs_F32UI[inputNumB];
            function( uA.f, uB.f );
            inputNumA = (inputNumA + 1) & (numInputs_f32 - 1);
            if ( ! inputNumA ) ++inputNumB;
            inputNumB = (inputNumB + 1) & (numInputs_f32 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNumA = 0;
    inputNumB = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F32UI[inputNumA];
        uB.ui = inputs_F32UI[inputNumB];
        function( uA.f, uB.f );
        inputNumA = (inputNumA + 1) & (numInputs_f32 - 1);
        if ( ! inputNumA ) ++inputNumB;
        inputNumB = (inputNumB + 1) & (numInputs_f32 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static const uint32_t inputs_F32UI_pos[numInputs_f32] = {
    0x4EFA0000, 0x41D0B328, 0x00000000, 0x3E69A31E,
    0x2F803EFF, 0x3F800000, 0x17BF8000, 0x674A301A,
    0x4E010003, 0x7EE3C75D, 0x3D803FE0, 0x3FFEFF00,
    0x7981F800, 0x431FFFFC, 0x4100C000, 0x3D87EFFF,
    0x4103FEFE, 0x3C000007, 0x3F01F7FF, 0x4E6C6B5C,
    0x4187FFFE, 0x458B9F13, 0x4F88007F, 0x5F004007,
    0x37FFD7FE, 0x7E8001FB, 0x46EFFBFF, 0x31C10000,
    0x5B428661, 0x33F89B1F, 0x23BFEFFF, 0x537BFFBE
};

static void time_az_f32_pos( float32_t function( float32_t ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui32_f32 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F32UI_pos[inputNum];
            function( uA.f );
            inputNum = (inputNum + 1) & (numInputs_f32 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F32UI_pos[inputNum];
        function( uA.f );
        inputNum = (inputNum + 1) & (numInputs_f32 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

#ifdef FLOAT64

enum { numInputs_f64 = 32 };

static const uint64_t inputs_F64UI[numInputs_f64] = {
    UINT64_C( 0x422FFFC008000000 ),
    UINT64_C( 0xB7E0000480000000 ),
    UINT64_C( 0xF3FD2546120B7935 ),
    UINT64_C( 0x3FF0000000000000 ),
    UINT64_C( 0xCE07F766F09588D6 ),
    UINT64_C( 0x8000000000000000 ),
    UINT64_C( 0x3FCE000400000000 ),
    UINT64_C( 0x8313B60F0032BED8 ),
    UINT64_C( 0xC1EFFFFFC0002000 ),
    UINT64_C( 0x3FB3C75D224F2B0F ),
    UINT64_C( 0x7FD00000004000FF ),
    UINT64_C( 0xA12FFF8000001FFF ),
    UINT64_C( 0x3EE0000000FE0000 ),
    UINT64_C( 0x0010000080000004 ),
    UINT64_C( 0x41CFFFFE00000020 ),
    UINT64_C( 0x40303FFFFFFFFFFD ),
    UINT64_C( 0x3FD000003FEFFFFF ),
    UINT64_C( 0xBFD0000010000000 ),
    UINT64_C( 0xB7FC6B5C16CA55CF ),
    UINT64_C( 0x413EEB940B9D1301 ),
    UINT64_C( 0xC7E00200001FFFFF ),
    UINT64_C( 0x47F00021FFFFFFFE ),
    UINT64_C( 0xBFFFFFFFF80000FF ),
    UINT64_C( 0xC07FFFFFE00FFFFF ),
    UINT64_C( 0x001497A63740C5E8 ),
    UINT64_C( 0xC4BFFFE0001FFFFF ),
    UINT64_C( 0x96FFDFFEFFFFFFFF ),
    UINT64_C( 0x403FC000000001FE ),
    UINT64_C( 0xFFD00000000001F6 ),
    UINT64_C( 0x0640400002000000 ),
    UINT64_C( 0x479CEE1E4F789FE0 ),
    UINT64_C( 0xC237FFFFFFFFFDFE )
};

static
void
 time_a_f64_z_ui32_rx(
     uint_fast32_t function( float64_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui64_f64 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F64UI[inputNum];
            function( uA.f, roundingMode, exact );
            inputNum = (inputNum + 1) & (numInputs_f64 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F64UI[inputNum];
        function( uA.f, roundingMode, exact );
        inputNum = (inputNum + 1) & (numInputs_f64 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_a_f64_z_ui64_rx(
     uint_fast64_t function( float64_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui64_f64 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F64UI[inputNum];
            function( uA.f, roundingMode, exact );
            inputNum = (inputNum + 1) & (numInputs_f64 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F64UI[inputNum];
        function( uA.f, roundingMode, exact );
        inputNum = (inputNum + 1) & (numInputs_f64 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_a_f64_z_i32_rx(
     int_fast32_t function( float64_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui64_f64 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F64UI[inputNum];
            function( uA.f, roundingMode, exact );
            inputNum = (inputNum + 1) & (numInputs_f64 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F64UI[inputNum];
        function( uA.f, roundingMode, exact );
        inputNum = (inputNum + 1) & (numInputs_f64 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_a_f64_z_i64_rx(
     int_fast64_t function( float64_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui64_f64 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F64UI[inputNum];
            function( uA.f, roundingMode, exact );
            inputNum = (inputNum + 1) & (numInputs_f64 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F64UI[inputNum];
        function( uA.f, roundingMode, exact );
        inputNum = (inputNum + 1) & (numInputs_f64 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_a_f64_z_ui32_x( uint_fast32_t function( float64_t, bool ), bool exact )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui64_f64 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F64UI[inputNum];
            function( uA.f, exact );
            inputNum = (inputNum + 1) & (numInputs_f64 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F64UI[inputNum];
        function( uA.f, exact );
        inputNum = (inputNum + 1) & (numInputs_f64 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_a_f64_z_ui64_x( uint_fast64_t function( float64_t, bool ), bool exact )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui64_f64 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F64UI[inputNum];
            function( uA.f, exact );
            inputNum = (inputNum + 1) & (numInputs_f64 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F64UI[inputNum];
        function( uA.f, exact );
        inputNum = (inputNum + 1) & (numInputs_f64 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void time_a_f64_z_i32_x( int_fast32_t function( float64_t, bool ), bool exact )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui64_f64 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F64UI[inputNum];
            function( uA.f, exact );
            inputNum = (inputNum + 1) & (numInputs_f64 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F64UI[inputNum];
        function( uA.f, exact );
        inputNum = (inputNum + 1) & (numInputs_f64 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void time_a_f64_z_i64_x( int_fast64_t function( float64_t, bool ), bool exact )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui64_f64 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F64UI[inputNum];
            function( uA.f, exact );
            inputNum = (inputNum + 1) & (numInputs_f64 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F64UI[inputNum];
        function( uA.f, exact );
        inputNum = (inputNum + 1) & (numInputs_f64 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#ifdef FLOAT16

static void time_a_f64_z_f16( float16_t function( float64_t ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui64_f64 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F64UI[inputNum];
            function( uA.f );
            inputNum = (inputNum + 1) & (numInputs_f64 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F64UI[inputNum];
        function( uA.f );
        inputNum = (inputNum + 1) & (numInputs_f64 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

static void time_a_f64_z_f32( float32_t function( float64_t ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui64_f64 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F64UI[inputNum];
            function( uA.f );
            inputNum = (inputNum + 1) & (numInputs_f64 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F64UI[inputNum];
        function( uA.f );
        inputNum = (inputNum + 1) & (numInputs_f64 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#ifdef EXTFLOAT80

static void time_a_f64_z_extF80( void function( float64_t, extFloat80_t * ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui64_f64 uA;
    extFloat80_t z;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F64UI[inputNum];
            function( uA.f, &z );
            inputNum = (inputNum + 1) & (numInputs_f64 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F64UI[inputNum];
        function( uA.f, &z );
        inputNum = (inputNum + 1) & (numInputs_f64 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

#ifdef FLOAT128

static void time_a_f64_z_f128( void function( float64_t, float128_t * ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui64_f64 uA;
    float128_t z;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F64UI[inputNum];
            function( uA.f, &z );
            inputNum = (inputNum + 1) & (numInputs_f64 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F64UI[inputNum];
        function( uA.f, &z );
        inputNum = (inputNum + 1) & (numInputs_f64 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

static
void
 time_az_f64_rx(
     float64_t function( float64_t, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui64_f64 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F64UI[inputNum];
            function( uA.f, roundingMode, exact );
            inputNum = (inputNum + 1) & (numInputs_f64 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F64UI[inputNum];
        function( uA.f, roundingMode, exact );
        inputNum = (inputNum + 1) & (numInputs_f64 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static void time_abz_f64( float64_t function( float64_t, float64_t ) )
{
    int_fast64_t count;
    int inputNumA, inputNumB;
    clock_t startClock;
    int_fast64_t i;
    union ui64_f64 uA, uB;
    clock_t endClock;

    count = 0;
    inputNumA = 0;
    inputNumB = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F64UI[inputNumA];
            uB.ui = inputs_F64UI[inputNumB];
            function( uA.f, uB.f );
            inputNumA = (inputNumA + 1) & (numInputs_f64 - 1);
            if ( ! inputNumA ) ++inputNumB;
            inputNumB = (inputNumB + 1) & (numInputs_f64 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNumA = 0;
    inputNumB = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F64UI[inputNumA];
        uB.ui = inputs_F64UI[inputNumB];
        function( uA.f, uB.f );
        inputNumA = (inputNumA + 1) & (numInputs_f64 - 1);
        if ( ! inputNumA ) ++inputNumB;
        inputNumB = (inputNumB + 1) & (numInputs_f64 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void time_abcz_f64( float64_t function( float64_t, float64_t, float64_t ) )
{
    int_fast64_t count;
    int inputNumA, inputNumB, inputNumC;
    clock_t startClock;
    int_fast64_t i;
    union ui64_f64 uA, uB, uC;
    clock_t endClock;

    count = 0;
    inputNumA = 0;
    inputNumB = 0;
    inputNumC = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F64UI[inputNumA];
            uB.ui = inputs_F64UI[inputNumB];
            uC.ui = inputs_F64UI[inputNumC];
            function( uA.f, uB.f, uC.f );
            inputNumA = (inputNumA + 1) & (numInputs_f64 - 1);
            if ( ! inputNumA ) ++inputNumB;
            inputNumB = (inputNumB + 1) & (numInputs_f64 - 1);
            if ( ! inputNumB ) ++inputNumC;
            inputNumC = (inputNumC + 1) & (numInputs_f64 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNumA = 0;
    inputNumB = 0;
    inputNumC = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F64UI[inputNumA];
        uB.ui = inputs_F64UI[inputNumB];
        uC.ui = inputs_F64UI[inputNumC];
        function( uA.f, uB.f, uC.f );
        inputNumA = (inputNumA + 1) & (numInputs_f64 - 1);
        if ( ! inputNumA ) ++inputNumB;
        inputNumB = (inputNumB + 1) & (numInputs_f64 - 1);
        if ( ! inputNumB ) ++inputNumC;
        inputNumC = (inputNumC + 1) & (numInputs_f64 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static void time_ab_f64_z_bool( bool function( float64_t, float64_t ) )
{
    int_fast64_t count;
    int inputNumA, inputNumB;
    clock_t startClock;
    int_fast64_t i;
    union ui64_f64 uA, uB;
    clock_t endClock;

    count = 0;
    inputNumA = 0;
    inputNumB = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F64UI[inputNumA];
            uB.ui = inputs_F64UI[inputNumB];
            function( uA.f, uB.f );
            inputNumA = (inputNumA + 1) & (numInputs_f64 - 1);
            if ( ! inputNumA ) ++inputNumB;
            inputNumB = (inputNumB + 1) & (numInputs_f64 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNumA = 0;
    inputNumB = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F64UI[inputNumA];
        uB.ui = inputs_F64UI[inputNumB];
        function( uA.f, uB.f );
        inputNumA = (inputNumA + 1) & (numInputs_f64 - 1);
        if ( ! inputNumA ) ++inputNumB;
        inputNumB = (inputNumB + 1) & (numInputs_f64 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static const uint64_t inputs_F64UI_pos[numInputs_f64] = {
    UINT64_C( 0x422FFFC008000000 ),
    UINT64_C( 0x37E0000480000000 ),
    UINT64_C( 0x73FD2546120B7935 ),
    UINT64_C( 0x3FF0000000000000 ),
    UINT64_C( 0x4E07F766F09588D6 ),
    UINT64_C( 0x0000000000000000 ),
    UINT64_C( 0x3FCE000400000000 ),
    UINT64_C( 0x0313B60F0032BED8 ),
    UINT64_C( 0x41EFFFFFC0002000 ),
    UINT64_C( 0x3FB3C75D224F2B0F ),
    UINT64_C( 0x7FD00000004000FF ),
    UINT64_C( 0x212FFF8000001FFF ),
    UINT64_C( 0x3EE0000000FE0000 ),
    UINT64_C( 0x0010000080000004 ),
    UINT64_C( 0x41CFFFFE00000020 ),
    UINT64_C( 0x40303FFFFFFFFFFD ),
    UINT64_C( 0x3FD000003FEFFFFF ),
    UINT64_C( 0x3FD0000010000000 ),
    UINT64_C( 0x37FC6B5C16CA55CF ),
    UINT64_C( 0x413EEB940B9D1301 ),
    UINT64_C( 0x47E00200001FFFFF ),
    UINT64_C( 0x47F00021FFFFFFFE ),
    UINT64_C( 0x3FFFFFFFF80000FF ),
    UINT64_C( 0x407FFFFFE00FFFFF ),
    UINT64_C( 0x001497A63740C5E8 ),
    UINT64_C( 0x44BFFFE0001FFFFF ),
    UINT64_C( 0x16FFDFFEFFFFFFFF ),
    UINT64_C( 0x403FC000000001FE ),
    UINT64_C( 0x7FD00000000001F6 ),
    UINT64_C( 0x0640400002000000 ),
    UINT64_C( 0x479CEE1E4F789FE0 ),
    UINT64_C( 0x4237FFFFFFFFFDFE )
};

static void time_az_f64_pos( float64_t function( float64_t ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    union ui64_f64 uA;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            uA.ui = inputs_F64UI_pos[inputNum];
            function( uA.f );
            inputNum = (inputNum + 1) & (numInputs_f64 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        uA.ui = inputs_F64UI_pos[inputNum];
        function( uA.f );
        inputNum = (inputNum + 1) & (numInputs_f64 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

#ifdef EXTFLOAT80

#ifdef LITTLEENDIAN
#define extF80Const( v64, v0 ) { UINT64_C( v0 ), v64 }
#else
#define extF80Const( v64, v0 ) { v64, UINT64_C( v0 ) }
#endif

enum { numInputs_extF80 = 32 };

static
const union { struct extFloat80M s; extFloat80_t f; }
    inputs_extF80[numInputs_extF80] = {
        extF80Const( 0xC03F, 0xA9BE15A19C1E8B62 ),
        extF80Const( 0x8000, 0x0000000000000000 ),
        extF80Const( 0x75A8, 0xE59591E4788957A5 ),
        extF80Const( 0xBFFF, 0xFFF0000000000040 ),
        extF80Const( 0x0CD8, 0xFC000000000007FE ),
        extF80Const( 0x43BA, 0x99A4000000000000 ),
        extF80Const( 0x3FFF, 0x8000000000000000 ),
        extF80Const( 0x4081, 0x94FBF1BCEB5545F0 ),
        extF80Const( 0x403E, 0xFFF0000000002000 ),
        extF80Const( 0x3FFE, 0xC860E3C75D224F28 ),
        extF80Const( 0x407E, 0xFC00000FFFFFFFFE ),
        extF80Const( 0x737A, 0x800000007FFDFFFE ),
        extF80Const( 0x4044, 0xFFFFFF80000FFFFF ),
        extF80Const( 0xBBFE, 0x8000040000001FFE ),
        extF80Const( 0xC002, 0xFF80000000000020 ),
        extF80Const( 0xDE8D, 0xFFFFFFFFFFE00004 ),
        extF80Const( 0xC004, 0x8000000000003FFB ),
        extF80Const( 0x407F, 0x800000000003FFFE ),
        extF80Const( 0xC000, 0xA459EE6A5C16CA55 ),
        extF80Const( 0x8003, 0xC42CBF7399AEEB94 ),
        extF80Const( 0xBF7F, 0xF800000000000006 ),
        extF80Const( 0xC07F, 0xBF56BE8871F28FEA ),
        extF80Const( 0xC07E, 0xFFFF77FFFFFFFFFE ),
        extF80Const( 0xADC9, 0x8000000FFFFFFFDE ),
        extF80Const( 0xC001, 0xEFF7FFFFFFFFFFFF ),
        extF80Const( 0x4001, 0xBE84F30125C497A6 ),
        extF80Const( 0xC06B, 0xEFFFFFFFFFFFFFFF ),
        extF80Const( 0x4080, 0xFFFFFFFFBFFFFFFF ),
        extF80Const( 0x87E9, 0x81FFFFFFFFFFFBFF ),
        extF80Const( 0xA63F, 0x801FFFFFFEFFFFFE ),
        extF80Const( 0x403C, 0x801FFFFFFFF7FFFF ),
        extF80Const( 0x4018, 0x8000000000080003 )
    };

static
void
 time_a_extF80_z_ui32_rx(
     uint_fast32_t function( const extFloat80_t *, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( &inputs_extF80[inputNum].f, roundingMode, exact );
            inputNum = (inputNum + 1) & (numInputs_extF80 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_extF80[inputNum].f, roundingMode, exact );
        inputNum = (inputNum + 1) & (numInputs_extF80 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_a_extF80_z_ui64_rx(
     uint_fast64_t function( const extFloat80_t *, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( &inputs_extF80[inputNum].f, roundingMode, exact );
            inputNum = (inputNum + 1) & (numInputs_extF80 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_extF80[inputNum].f, roundingMode, exact );
        inputNum = (inputNum + 1) & (numInputs_extF80 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_a_extF80_z_i32_rx(
     int_fast32_t function( const extFloat80_t *, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( &inputs_extF80[inputNum].f, roundingMode, exact );
            inputNum = (inputNum + 1) & (numInputs_extF80 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_extF80[inputNum].f, roundingMode, exact );
        inputNum = (inputNum + 1) & (numInputs_extF80 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_a_extF80_z_i64_rx(
     int_fast64_t function( const extFloat80_t *, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( &inputs_extF80[inputNum].f, roundingMode, exact );
            inputNum = (inputNum + 1) & (numInputs_extF80 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_extF80[inputNum].f, roundingMode, exact );
        inputNum = (inputNum + 1) & (numInputs_extF80 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_a_extF80_z_ui32_x(
     uint_fast32_t function( const extFloat80_t *, bool ), bool exact )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( &inputs_extF80[inputNum].f, exact );
            inputNum = (inputNum + 1) & (numInputs_extF80 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_extF80[inputNum].f, exact );
        inputNum = (inputNum + 1) & (numInputs_extF80 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_a_extF80_z_ui64_x(
     uint_fast64_t function( const extFloat80_t *, bool ), bool exact )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( &inputs_extF80[inputNum].f, exact );
            inputNum = (inputNum + 1) & (numInputs_extF80 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_extF80[inputNum].f, exact );
        inputNum = (inputNum + 1) & (numInputs_extF80 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_a_extF80_z_i32_x(
     int_fast32_t function( const extFloat80_t *, bool ), bool exact )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( &inputs_extF80[inputNum].f, exact );
            inputNum = (inputNum + 1) & (numInputs_extF80 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_extF80[inputNum].f, exact );
        inputNum = (inputNum + 1) & (numInputs_extF80 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_a_extF80_z_i64_x(
     int_fast64_t function( const extFloat80_t *, bool ), bool exact )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( &inputs_extF80[inputNum].f, exact );
            inputNum = (inputNum + 1) & (numInputs_extF80 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_extF80[inputNum].f, exact );
        inputNum = (inputNum + 1) & (numInputs_extF80 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#ifdef FLOAT16

static void time_a_extF80_z_f16( float16_t function( const extFloat80_t * ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( &inputs_extF80[inputNum].f );
            inputNum = (inputNum + 1) & (numInputs_extF80 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_extF80[inputNum].f );
        inputNum = (inputNum + 1) & (numInputs_extF80 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

static void time_a_extF80_z_f32( float32_t function( const extFloat80_t * ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( &inputs_extF80[inputNum].f );
            inputNum = (inputNum + 1) & (numInputs_extF80 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_extF80[inputNum].f );
        inputNum = (inputNum + 1) & (numInputs_extF80 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#ifdef FLOAT64

static void time_a_extF80_z_f64( float64_t function( const extFloat80_t * ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( &inputs_extF80[inputNum].f );
            inputNum = (inputNum + 1) & (numInputs_extF80 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_extF80[inputNum].f );
        inputNum = (inputNum + 1) & (numInputs_extF80 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

#ifdef FLOAT128

static
void
 time_a_extF80_z_f128( void function( const extFloat80_t *, float128_t * ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    float128_t z;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( &inputs_extF80[inputNum].f, &z );
            inputNum = (inputNum + 1) & (numInputs_extF80 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_extF80[inputNum].f, &z );
        inputNum = (inputNum + 1) & (numInputs_extF80 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

static
void
 time_az_extF80_rx(
     void function( const extFloat80_t *, uint_fast8_t, bool, extFloat80_t * ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    extFloat80_t z;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( &inputs_extF80[inputNum].f, roundingMode, exact, &z );
            inputNum = (inputNum + 1) & (numInputs_extF80 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_extF80[inputNum].f, roundingMode, exact, &z );
        inputNum = (inputNum + 1) & (numInputs_extF80 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_abz_extF80(
     void
      function( const extFloat80_t *, const extFloat80_t *, extFloat80_t * )
 )
{
    int_fast64_t count;
    int inputNumA, inputNumB;
    clock_t startClock;
    int_fast64_t i;
    extFloat80_t z;
    clock_t endClock;

    count = 0;
    inputNumA = 0;
    inputNumB = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function(
                &inputs_extF80[inputNumA].f, &inputs_extF80[inputNumB].f, &z );
            inputNumA = (inputNumA + 1) & (numInputs_extF80 - 1);
            if ( ! inputNumA ) ++inputNumB;
            inputNumB = (inputNumB + 1) & (numInputs_extF80 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNumA = 0;
    inputNumB = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function(
            &inputs_extF80[inputNumA].f, &inputs_extF80[inputNumB].f, &z );
        inputNumA = (inputNumA + 1) & (numInputs_extF80 - 1);
        if ( ! inputNumA ) ++inputNumB;
        inputNumB = (inputNumB + 1) & (numInputs_extF80 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_ab_extF80_z_bool(
     bool function( const extFloat80_t *, const extFloat80_t * ) )
{
    int_fast64_t count;
    int inputNumA, inputNumB;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNumA = 0;
    inputNumB = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function(
                &inputs_extF80[inputNumA].f, &inputs_extF80[inputNumB].f );
            inputNumA = (inputNumA + 1) & (numInputs_extF80 - 1);
            if ( ! inputNumA ) ++inputNumB;
            inputNumB = (inputNumB + 1) & (numInputs_extF80 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNumA = 0;
    inputNumB = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_extF80[inputNumA].f, &inputs_extF80[inputNumB].f );
        inputNumA = (inputNumA + 1) & (numInputs_extF80 - 1);
        if ( ! inputNumA ) ++inputNumB;
        inputNumB = (inputNumB + 1) & (numInputs_extF80 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
const union { struct extFloat80M s; extFloat80_t f; }
    inputs_extF80_pos[numInputs_extF80] = {
        extF80Const( 0x403F, 0xA9BE15A19C1E8B62 ),
        extF80Const( 0x0000, 0x0000000000000000 ),
        extF80Const( 0x75A8, 0xE59591E4788957A5 ),
        extF80Const( 0x3FFF, 0xFFF0000000000040 ),
        extF80Const( 0x0CD8, 0xFC000000000007FE ),
        extF80Const( 0x43BA, 0x99A4000000000000 ),
        extF80Const( 0x3FFF, 0x8000000000000000 ),
        extF80Const( 0x4081, 0x94FBF1BCEB5545F0 ),
        extF80Const( 0x403E, 0xFFF0000000002000 ),
        extF80Const( 0x3FFE, 0xC860E3C75D224F28 ),
        extF80Const( 0x407E, 0xFC00000FFFFFFFFE ),
        extF80Const( 0x737A, 0x800000007FFDFFFE ),
        extF80Const( 0x4044, 0xFFFFFF80000FFFFF ),
        extF80Const( 0x3BFE, 0x8000040000001FFE ),
        extF80Const( 0x4002, 0xFF80000000000020 ),
        extF80Const( 0x5E8D, 0xFFFFFFFFFFE00004 ),
        extF80Const( 0x4004, 0x8000000000003FFB ),
        extF80Const( 0x407F, 0x800000000003FFFE ),
        extF80Const( 0x4000, 0xA459EE6A5C16CA55 ),
        extF80Const( 0x0003, 0xC42CBF7399AEEB94 ),
        extF80Const( 0x3F7F, 0xF800000000000006 ),
        extF80Const( 0x407F, 0xBF56BE8871F28FEA ),
        extF80Const( 0x407E, 0xFFFF77FFFFFFFFFE ),
        extF80Const( 0x2DC9, 0x8000000FFFFFFFDE ),
        extF80Const( 0x4001, 0xEFF7FFFFFFFFFFFF ),
        extF80Const( 0x4001, 0xBE84F30125C497A6 ),
        extF80Const( 0x406B, 0xEFFFFFFFFFFFFFFF ),
        extF80Const( 0x4080, 0xFFFFFFFFBFFFFFFF ),
        extF80Const( 0x07E9, 0x81FFFFFFFFFFFBFF ),
        extF80Const( 0x263F, 0x801FFFFFFEFFFFFE ),
        extF80Const( 0x403C, 0x801FFFFFFFF7FFFF ),
        extF80Const( 0x4018, 0x8000000000080003 )
    };

static
void
 time_az_extF80_pos( void function( const extFloat80_t *, extFloat80_t * ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    extFloat80_t z;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( &inputs_extF80[inputNum].f, &z );
            inputNum = (inputNum + 1) & (numInputs_extF80 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_extF80[inputNum].f, &z );
        inputNum = (inputNum + 1) & (numInputs_extF80 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

#ifdef FLOAT128

#ifdef LITTLEENDIAN
#define f128Const( v64, v0 ) { UINT64_C( v0 ), UINT64_C( v64 ) }
#else
#define f128Const( v64, v0 ) { UINT64_C( v64 ), UINT64_C( v0 ) }
#endif

enum { numInputs_f128 = 32 };

static
const union { struct uint128 ui; float128_t f; }
    inputs_f128[numInputs_f128] = {
        f128Const( 0x3FDA200000100000, 0x0000000000000000 ),
        f128Const( 0x3FFF000000000000, 0x0000000000000000 ),
        f128Const( 0x85F14776190C8306, 0xD8715F4E3D54BB92 ),
        f128Const( 0xF2B00000007FFFFF, 0xFFFFFFFFFFF7FFFF ),
        f128Const( 0x8000000000000000, 0x0000000000000000 ),
        f128Const( 0xBFFFFFFFFFE00000, 0x0000008000000000 ),
        f128Const( 0x407F1719CE722F3E, 0xDA6B3FE5FF29425B ),
        f128Const( 0x43FFFF8000000000, 0x0000000000400000 ),
        f128Const( 0x401E000000000100, 0x0000000000002000 ),
        f128Const( 0x3FFED71DACDA8E47, 0x4860E3C75D224F28 ),
        f128Const( 0xBF7ECFC1E90647D1, 0x7A124FE55623EE44 ),
        f128Const( 0x0DF7007FFFFFFFFF, 0xFFFFFFFFEFFFFFFF ),
        f128Const( 0x3FE5FFEFFFFFFFFF, 0xFFFFFFFFFFFFEFFF ),
        f128Const( 0x403FFFFFFFFFFFFF, 0xFFFFFFFFFFFFFBFE ),
        f128Const( 0xBFFB2FBF7399AFEB, 0xA459EE6A5C16CA55 ),
        f128Const( 0xBDB8FFFFFFFFFFFC, 0x0000000000000400 ),
        f128Const( 0x3FC8FFDFFFFFFFFF, 0xFFFFFFFFF0000000 ),
        f128Const( 0x3FFBFFFFFFDFFFFF, 0xFFF8000000000000 ),
        f128Const( 0x407043C11737BE84, 0xDDD58212ADC937F4 ),
        f128Const( 0x8001000000000000, 0x0000001000000001 ),
        f128Const( 0xC036FFFFFFFFFFFF, 0xFE40000000000000 ),
        f128Const( 0x4002FFFFFE000002, 0x0000000000000000 ),
        f128Const( 0x4000C3FEDE897773, 0x326AC4FD8EFBE6DC ),
        f128Const( 0xBFFF0000000FFFFF, 0xFFFFFE0000000000 ),
        f128Const( 0x62C3E502146E426D, 0x43F3CAA0DC7DF1A0 ),
        f128Const( 0xB5CBD32E52BB570E, 0xBCC477CB11C6236C ),
        f128Const( 0xE228FFFFFFC00000, 0x0000000000000000 ),
        f128Const( 0x3F80000000000000, 0x0000000080000008 ),
        f128Const( 0xC1AFFFDFFFFFFFFF, 0xFFFC000000000000 ),
        f128Const( 0xC96F000000000000, 0x00000001FFFBFFFF ),
        f128Const( 0x3DE09BFE7923A338, 0xBCC8FBBD7CEC1F4F ),
        f128Const( 0x401CFFFFFFFFFFFF, 0xFFFFFFFEFFFFFF80 )
    };

static
void
 time_a_f128_z_ui32_rx(
     uint_fast32_t function( const float128_t *, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( &inputs_f128[inputNum].f, roundingMode, exact );
            inputNum = (inputNum + 1) & (numInputs_f128 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_f128[inputNum].f, roundingMode, exact );
        inputNum = (inputNum + 1) & (numInputs_f128 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_a_f128_z_ui64_rx(
     uint_fast64_t function( const float128_t *, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( &inputs_f128[inputNum].f, roundingMode, exact );
            inputNum = (inputNum + 1) & (numInputs_f128 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_f128[inputNum].f, roundingMode, exact );
        inputNum = (inputNum + 1) & (numInputs_f128 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_a_f128_z_i32_rx(
     int_fast32_t function( const float128_t *, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( &inputs_f128[inputNum].f, roundingMode, exact );
            inputNum = (inputNum + 1) & (numInputs_f128 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_f128[inputNum].f, roundingMode, exact );
        inputNum = (inputNum + 1) & (numInputs_f128 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_a_f128_z_i64_rx(
     int_fast64_t function( const float128_t *, uint_fast8_t, bool ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( &inputs_f128[inputNum].f, roundingMode, exact );
            inputNum = (inputNum + 1) & (numInputs_f128 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_f128[inputNum].f, roundingMode, exact );
        inputNum = (inputNum + 1) & (numInputs_f128 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_a_f128_z_ui32_x(
     uint_fast32_t function( const float128_t *, bool ), bool exact )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( &inputs_f128[inputNum].f, exact );
            inputNum = (inputNum + 1) & (numInputs_f128 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_f128[inputNum].f, exact );
        inputNum = (inputNum + 1) & (numInputs_f128 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_a_f128_z_ui64_x(
     uint_fast64_t function( const float128_t *, bool ), bool exact )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( &inputs_f128[inputNum].f, exact );
            inputNum = (inputNum + 1) & (numInputs_f128 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_f128[inputNum].f, exact );
        inputNum = (inputNum + 1) & (numInputs_f128 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_a_f128_z_i32_x(
     int_fast32_t function( const float128_t *, bool ), bool exact )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( &inputs_f128[inputNum].f, exact );
            inputNum = (inputNum + 1) & (numInputs_f128 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_f128[inputNum].f, exact );
        inputNum = (inputNum + 1) & (numInputs_f128 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_a_f128_z_i64_x(
     int_fast64_t function( const float128_t *, bool ), bool exact )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( &inputs_f128[inputNum].f, exact );
            inputNum = (inputNum + 1) & (numInputs_f128 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_f128[inputNum].f, exact );
        inputNum = (inputNum + 1) & (numInputs_f128 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#ifdef FLOAT16

static void time_a_f128_z_f16( float16_t function( const float128_t * ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( &inputs_f128[inputNum].f );
            inputNum = (inputNum + 1) & (numInputs_f128 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_f128[inputNum].f );
        inputNum = (inputNum + 1) & (numInputs_f128 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

static void time_a_f128_z_f32( float32_t function( const float128_t * ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( &inputs_f128[inputNum].f );
            inputNum = (inputNum + 1) & (numInputs_f128 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_f128[inputNum].f );
        inputNum = (inputNum + 1) & (numInputs_f128 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#ifdef FLOAT64

static void time_a_f128_z_f64( float64_t function( const float128_t * ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( &inputs_f128[inputNum].f );
            inputNum = (inputNum + 1) & (numInputs_f128 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_f128[inputNum].f );
        inputNum = (inputNum + 1) & (numInputs_f128 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

#ifdef EXTFLOAT80

static
void
 time_a_f128_z_extF80( void function( const float128_t *, extFloat80_t * ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    extFloat80_t z;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( &inputs_f128[inputNum].f, &z );
            inputNum = (inputNum + 1) & (numInputs_f128 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_f128[inputNum].f, &z );
        inputNum = (inputNum + 1) & (numInputs_f128 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

static
void
 time_az_f128_rx(
     void function( const float128_t *, uint_fast8_t, bool, float128_t * ),
     uint_fast8_t roundingMode,
     bool exact
 )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    float128_t z;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( &inputs_f128[inputNum].f, roundingMode, exact, &z );
            inputNum = (inputNum + 1) & (numInputs_f128 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_f128[inputNum].f, roundingMode, exact, &z );
        inputNum = (inputNum + 1) & (numInputs_f128 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_abz_f128(
     void function( const float128_t *, const float128_t *, float128_t * ) )
{
    int_fast64_t count;
    int inputNumA, inputNumB;
    clock_t startClock;
    int_fast64_t i;
    float128_t z;
    clock_t endClock;

    count = 0;
    inputNumA = 0;
    inputNumB = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function(
                &inputs_f128[inputNumA].f, &inputs_f128[inputNumB].f, &z );
            inputNumA = (inputNumA + 1) & (numInputs_f128 - 1);
            if ( ! inputNumA ) ++inputNumB;
            inputNumB = (inputNumB + 1) & (numInputs_f128 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNumA = 0;
    inputNumB = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_f128[inputNumA].f, &inputs_f128[inputNumB].f, &z );
        inputNumA = (inputNumA + 1) & (numInputs_f128 - 1);
        if ( ! inputNumA ) ++inputNumB;
        inputNumB = (inputNumB + 1) & (numInputs_f128 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_abcz_f128(
     void
      function(
          const float128_t *,
          const float128_t *,
          const float128_t *,
          float128_t *
      )
 )
{
    int_fast64_t count;
    int inputNumA, inputNumB, inputNumC;
    clock_t startClock;
    int_fast64_t i;
    float128_t z;
    clock_t endClock;

    count = 0;
    inputNumA = 0;
    inputNumB = 0;
    inputNumC = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function(
                &inputs_f128[inputNumA].f,
                &inputs_f128[inputNumB].f,
                &inputs_f128[inputNumC].f,
                &z
            );
            inputNumA = (inputNumA + 1) & (numInputs_f128 - 1);
            if ( ! inputNumA ) ++inputNumB;
            inputNumB = (inputNumB + 1) & (numInputs_f128 - 1);
            if ( ! inputNumB ) ++inputNumC;
            inputNumC = (inputNumC + 1) & (numInputs_f128 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNumA = 0;
    inputNumB = 0;
    inputNumC = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function(
            &inputs_f128[inputNumA].f,
            &inputs_f128[inputNumB].f,
            &inputs_f128[inputNumC].f,
            &z
        );
        inputNumA = (inputNumA + 1) & (numInputs_f128 - 1);
        if ( ! inputNumA ) ++inputNumB;
        inputNumB = (inputNumB + 1) & (numInputs_f128 - 1);
        if ( ! inputNumB ) ++inputNumC;
        inputNumC = (inputNumC + 1) & (numInputs_f128 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
void
 time_ab_f128_z_bool( bool function( const float128_t *, const float128_t * ) )
{
    int_fast64_t count;
    int inputNumA, inputNumB;
    clock_t startClock;
    int_fast64_t i;
    clock_t endClock;

    count = 0;
    inputNumA = 0;
    inputNumB = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( &inputs_f128[inputNumA].f, &inputs_f128[inputNumB].f );
            inputNumA = (inputNumA + 1) & (numInputs_f128 - 1);
            if ( ! inputNumA ) ++inputNumB;
            inputNumB = (inputNumB + 1) & (numInputs_f128 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNumA = 0;
    inputNumB = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_f128[inputNumA].f, &inputs_f128[inputNumB].f );
        inputNumA = (inputNumA + 1) & (numInputs_f128 - 1);
        if ( ! inputNumA ) ++inputNumB;
        inputNumB = (inputNumB + 1) & (numInputs_f128 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

static
const union { struct uint128 ui; float128_t f; }
    inputs_f128_pos[numInputs_f128] = {
        f128Const( 0x3FDA200000100000, 0x0000000000000000 ),
        f128Const( 0x3FFF000000000000, 0x0000000000000000 ),
        f128Const( 0x05F14776190C8306, 0xD8715F4E3D54BB92 ),
        f128Const( 0x72B00000007FFFFF, 0xFFFFFFFFFFF7FFFF ),
        f128Const( 0x0000000000000000, 0x0000000000000000 ),
        f128Const( 0x3FFFFFFFFFE00000, 0x0000008000000000 ),
        f128Const( 0x407F1719CE722F3E, 0xDA6B3FE5FF29425B ),
        f128Const( 0x43FFFF8000000000, 0x0000000000400000 ),
        f128Const( 0x401E000000000100, 0x0000000000002000 ),
        f128Const( 0x3FFED71DACDA8E47, 0x4860E3C75D224F28 ),
        f128Const( 0x3F7ECFC1E90647D1, 0x7A124FE55623EE44 ),
        f128Const( 0x0DF7007FFFFFFFFF, 0xFFFFFFFFEFFFFFFF ),
        f128Const( 0x3FE5FFEFFFFFFFFF, 0xFFFFFFFFFFFFEFFF ),
        f128Const( 0x403FFFFFFFFFFFFF, 0xFFFFFFFFFFFFFBFE ),
        f128Const( 0x3FFB2FBF7399AFEB, 0xA459EE6A5C16CA55 ),
        f128Const( 0x3DB8FFFFFFFFFFFC, 0x0000000000000400 ),
        f128Const( 0x3FC8FFDFFFFFFFFF, 0xFFFFFFFFF0000000 ),
        f128Const( 0x3FFBFFFFFFDFFFFF, 0xFFF8000000000000 ),
        f128Const( 0x407043C11737BE84, 0xDDD58212ADC937F4 ),
        f128Const( 0x0001000000000000, 0x0000001000000001 ),
        f128Const( 0x4036FFFFFFFFFFFF, 0xFE40000000000000 ),
        f128Const( 0x4002FFFFFE000002, 0x0000000000000000 ),
        f128Const( 0x4000C3FEDE897773, 0x326AC4FD8EFBE6DC ),
        f128Const( 0x3FFF0000000FFFFF, 0xFFFFFE0000000000 ),
        f128Const( 0x62C3E502146E426D, 0x43F3CAA0DC7DF1A0 ),
        f128Const( 0x35CBD32E52BB570E, 0xBCC477CB11C6236C ),
        f128Const( 0x6228FFFFFFC00000, 0x0000000000000000 ),
        f128Const( 0x3F80000000000000, 0x0000000080000008 ),
        f128Const( 0x41AFFFDFFFFFFFFF, 0xFFFC000000000000 ),
        f128Const( 0x496F000000000000, 0x00000001FFFBFFFF ),
        f128Const( 0x3DE09BFE7923A338, 0xBCC8FBBD7CEC1F4F ),
        f128Const( 0x401CFFFFFFFFFFFF, 0xFFFFFFFEFFFFFF80 )
    };

static
void time_az_f128_pos( void function( const float128_t *, float128_t * ) )
{
    int_fast64_t count;
    int inputNum;
    clock_t startClock;
    int_fast64_t i;
    float128_t z;
    clock_t endClock;

    count = 0;
    inputNum = 0;
    startClock = clock();
    do {
        for ( i = minIterations; i; --i ) {
            function( &inputs_f128[inputNum].f, &z );
            inputNum = (inputNum + 1) & (numInputs_f128 - 1);
        }
        count += minIterations;
    } while ( clock() - startClock < CLOCKS_PER_SEC );
    inputNum = 0;
    startClock = clock();
    for ( i = count; i; --i ) {
        function( &inputs_f128[inputNum].f, &z );
        inputNum = (inputNum + 1) & (numInputs_f128 - 1);
    }
    endClock = clock();
    reportTime( count, endClock - startClock );

}

#endif

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

static
void
 timeFunctionInstance(
     int functionCode, uint_fast8_t roundingMode, bool exact )
{
#ifdef FLOAT16
    float16_t (*function_abz_f16)( float16_t, float16_t );
    bool (*function_ab_f16_z_bool)( float16_t, float16_t );
#endif
    float32_t (*function_abz_f32)( float32_t, float32_t );
    bool (*function_ab_f32_z_bool)( float32_t, float32_t );
#ifdef FLOAT64
    float64_t (*function_abz_f64)( float64_t, float64_t );
    bool (*function_ab_f64_z_bool)( float64_t, float64_t );
#endif
#ifdef EXTFLOAT80
    void
     (*function_abz_extF80)(
         const extFloat80_t *, const extFloat80_t *, extFloat80_t * );
    bool
     (*function_ab_extF80_z_bool)(
         const extFloat80_t *, const extFloat80_t * );
#endif
#ifdef FLOAT128
    void
     (*function_abz_f128)(
         const float128_t *, const float128_t *, float128_t * );
    bool (*function_ab_f128_z_bool)( const float128_t *, const float128_t * );
#endif

    switch ( functionCode ) {
        /*--------------------------------------------------------------------
        *--------------------------------------------------------------------*/
#ifdef FLOAT16
     case UI32_TO_F16:
        time_a_ui32_z_f16( ui32_to_f16 );
        break;
#endif
     case UI32_TO_F32:
        time_a_ui32_z_f32( ui32_to_f32 );
        break;
#ifdef FLOAT64
     case UI32_TO_F64:
        time_a_ui32_z_f64( ui32_to_f64 );
        break;
#endif
#ifdef EXTFLOAT80
     case UI32_TO_EXTF80:
        time_a_ui32_z_extF80( ui32_to_extF80M );
        break;
#endif
#ifdef FLOAT128
     case UI32_TO_F128:
        time_a_ui32_z_f128( ui32_to_f128M );
        break;
#endif
#ifdef FLOAT16
     case UI64_TO_F16:
        time_a_ui64_z_f16( ui64_to_f16 );
        break;
#endif
     case UI64_TO_F32:
        time_a_ui64_z_f32( ui64_to_f32 );
        break;
#ifdef FLOAT64
     case UI64_TO_F64:
        time_a_ui64_z_f64( ui64_to_f64 );
        break;
#endif
#ifdef EXTFLOAT80
     case UI64_TO_EXTF80:
        time_a_ui64_z_extF80( ui64_to_extF80M );
        break;
#endif
#ifdef FLOAT128
     case UI64_TO_F128:
        time_a_ui64_z_f128( ui64_to_f128M );
        break;
#endif
#ifdef FLOAT16
     case I32_TO_F16:
        time_a_i32_z_f16( i32_to_f16 );
        break;
#endif
     case I32_TO_F32:
        time_a_i32_z_f32( i32_to_f32 );
        break;
#ifdef FLOAT64
     case I32_TO_F64:
        time_a_i32_z_f64( i32_to_f64 );
        break;
#endif
#ifdef EXTFLOAT80
     case I32_TO_EXTF80:
        time_a_i32_z_extF80( i32_to_extF80M );
        break;
#endif
#ifdef FLOAT128
     case I32_TO_F128:
        time_a_i32_z_f128( i32_to_f128M );
        break;
#endif
#ifdef FLOAT16
     case I64_TO_F16:
        time_a_i64_z_f16( i64_to_f16 );
        break;
#endif
     case I64_TO_F32:
        time_a_i64_z_f32( i64_to_f32 );
        break;
#ifdef FLOAT64
     case I64_TO_F64:
        time_a_i64_z_f64( i64_to_f64 );
        break;
#endif
#ifdef EXTFLOAT80
     case I64_TO_EXTF80:
        time_a_i64_z_extF80( i64_to_extF80M );
        break;
#endif
#ifdef FLOAT128
     case I64_TO_F128:
        time_a_i64_z_f128( i64_to_f128M );
        break;
#endif
        /*--------------------------------------------------------------------
        *--------------------------------------------------------------------*/
#ifdef FLOAT16
     case F16_TO_UI32:
        time_a_f16_z_ui32_rx( f16_to_ui32, roundingMode, exact );
        break;
     case F16_TO_UI64:
        time_a_f16_z_ui64_rx( f16_to_ui64, roundingMode, exact );
        break;
     case F16_TO_I32:
        time_a_f16_z_i32_rx( f16_to_i32, roundingMode, exact );
        break;
     case F16_TO_I64:
        time_a_f16_z_i64_rx( f16_to_i64, roundingMode, exact );
        break;
     case F16_TO_UI32_R_MINMAG:
        time_a_f16_z_ui32_x( f16_to_ui32_r_minMag, exact );
        break;
     case F16_TO_UI64_R_MINMAG:
        time_a_f16_z_ui64_x( f16_to_ui64_r_minMag, exact );
        break;
     case F16_TO_I32_R_MINMAG:
        time_a_f16_z_i32_x( f16_to_i32_r_minMag, exact );
        break;
     case F16_TO_I64_R_MINMAG:
        time_a_f16_z_i64_x( f16_to_i64_r_minMag, exact );
        break;
     case F16_TO_F32:
        time_a_f16_z_f32( f16_to_f32 );
        break;
#ifdef FLOAT64
     case F16_TO_F64:
        time_a_f16_z_f64( f16_to_f64 );
        break;
#endif
#ifdef EXTFLOAT80
     case F16_TO_EXTF80:
        time_a_f16_z_extF80( f16_to_extF80M );
        break;
#endif
#ifdef FLOAT128
     case F16_TO_F128:
        time_a_f16_z_f128( f16_to_f128M );
        break;
#endif
     case F16_ROUNDTOINT:
        time_az_f16_rx( f16_roundToInt, roundingMode, exact );
        break;
     case F16_ADD:
        function_abz_f16 = f16_add;
        goto time_abz_f16;
     case F16_SUB:
        function_abz_f16 = f16_sub;
        goto time_abz_f16;
     case F16_MUL:
        function_abz_f16 = f16_mul;
        goto time_abz_f16;
     case F16_DIV:
        function_abz_f16 = f16_div;
        goto time_abz_f16;
     case F16_REM:
        function_abz_f16 = f16_rem;
     time_abz_f16:
        time_abz_f16( function_abz_f16 );
        break;
     case F16_MULADD:
        time_abcz_f16( f16_mulAdd );
        break;
     case F16_SQRT:
        time_az_f16_pos( f16_sqrt );
        break;
     case F16_EQ:
        function_ab_f16_z_bool = f16_eq;
        goto time_ab_f16_z_bool;
     case F16_LE:
        function_ab_f16_z_bool = f16_le;
        goto time_ab_f16_z_bool;
     case F16_LT:
        function_ab_f16_z_bool = f16_lt;
        goto time_ab_f16_z_bool;
     case F16_EQ_SIGNALING:
        function_ab_f16_z_bool = f16_eq_signaling;
        goto time_ab_f16_z_bool;
     case F16_LE_QUIET:
        function_ab_f16_z_bool = f16_le_quiet;
        goto time_ab_f16_z_bool;
     case F16_LT_QUIET:
        function_ab_f16_z_bool = f16_lt_quiet;
     time_ab_f16_z_bool:
        time_ab_f16_z_bool( function_ab_f16_z_bool );
        break;
#endif
        /*--------------------------------------------------------------------
        *--------------------------------------------------------------------*/
     case F32_TO_UI32:
        time_a_f32_z_ui32_rx( f32_to_ui32, roundingMode, exact );
        break;
     case F32_TO_UI64:
        time_a_f32_z_ui64_rx( f32_to_ui64, roundingMode, exact );
        break;
     case F32_TO_I32:
        time_a_f32_z_i32_rx( f32_to_i32, roundingMode, exact );
        break;
     case F32_TO_I64:
        time_a_f32_z_i64_rx( f32_to_i64, roundingMode, exact );
        break;
     case F32_TO_UI32_R_MINMAG:
        time_a_f32_z_ui32_x( f32_to_ui32_r_minMag, exact );
        break;
     case F32_TO_UI64_R_MINMAG:
        time_a_f32_z_ui64_x( f32_to_ui64_r_minMag, exact );
        break;
     case F32_TO_I32_R_MINMAG:
        time_a_f32_z_i32_x( f32_to_i32_r_minMag, exact );
        break;
     case F32_TO_I64_R_MINMAG:
        time_a_f32_z_i64_x( f32_to_i64_r_minMag, exact );
        break;
#ifdef FLOAT16
     case F32_TO_F16:
        time_a_f32_z_f16( f32_to_f16 );
        break;
#endif
#ifdef FLOAT64
     case F32_TO_F64:
        time_a_f32_z_f64( f32_to_f64 );
        break;
#endif
#ifdef EXTFLOAT80
     case F32_TO_EXTF80:
        time_a_f32_z_extF80( f32_to_extF80M );
        break;
#endif
#ifdef FLOAT128
     case F32_TO_F128:
        time_a_f32_z_f128( f32_to_f128M );
        break;
#endif
     case F32_ROUNDTOINT:
        time_az_f32_rx( f32_roundToInt, roundingMode, exact );
        break;
     case F32_ADD:
        function_abz_f32 = f32_add;
        goto time_abz_f32;
     case F32_SUB:
        function_abz_f32 = f32_sub;
        goto time_abz_f32;
     case F32_MUL:
        function_abz_f32 = f32_mul;
        goto time_abz_f32;
     case F32_DIV:
        function_abz_f32 = f32_div;
        goto time_abz_f32;
     case F32_REM:
        function_abz_f32 = f32_rem;
     time_abz_f32:
        time_abz_f32( function_abz_f32 );
        break;
     case F32_MULADD:
        time_abcz_f32( f32_mulAdd );
        break;
     case F32_SQRT:
        time_az_f32_pos( f32_sqrt );
        break;
     case F32_EQ:
        function_ab_f32_z_bool = f32_eq;
        goto time_ab_f32_z_bool;
     case F32_LE:
        function_ab_f32_z_bool = f32_le;
        goto time_ab_f32_z_bool;
     case F32_LT:
        function_ab_f32_z_bool = f32_lt;
        goto time_ab_f32_z_bool;
     case F32_EQ_SIGNALING:
        function_ab_f32_z_bool = f32_eq_signaling;
        goto time_ab_f32_z_bool;
     case F32_LE_QUIET:
        function_ab_f32_z_bool = f32_le_quiet;
        goto time_ab_f32_z_bool;
     case F32_LT_QUIET:
        function_ab_f32_z_bool = f32_lt_quiet;
     time_ab_f32_z_bool:
        time_ab_f32_z_bool( function_ab_f32_z_bool );
        break;
        /*--------------------------------------------------------------------
        *--------------------------------------------------------------------*/
#ifdef FLOAT64
     case F64_TO_UI32:
        time_a_f64_z_ui32_rx( f64_to_ui32, roundingMode, exact );
        break;
     case F64_TO_UI64:
        time_a_f64_z_ui64_rx( f64_to_ui64, roundingMode, exact );
        break;
     case F64_TO_I32:
        time_a_f64_z_i32_rx( f64_to_i32, roundingMode, exact );
        break;
     case F64_TO_I64:
        time_a_f64_z_i64_rx( f64_to_i64, roundingMode, exact );
        break;
     case F64_TO_UI32_R_MINMAG:
        time_a_f64_z_ui32_x( f64_to_ui32_r_minMag, exact );
        break;
     case F64_TO_UI64_R_MINMAG:
        time_a_f64_z_ui64_x( f64_to_ui64_r_minMag, exact );
        break;
     case F64_TO_I32_R_MINMAG:
        time_a_f64_z_i32_x( f64_to_i32_r_minMag, exact );
        break;
     case F64_TO_I64_R_MINMAG:
        time_a_f64_z_i64_x( f64_to_i64_r_minMag, exact );
        break;
#ifdef FLOAT16
     case F64_TO_F16:
        time_a_f64_z_f16( f64_to_f16 );
        break;
#endif
     case F64_TO_F32:
        time_a_f64_z_f32( f64_to_f32 );
        break;
#ifdef EXTFLOAT80
     case F64_TO_EXTF80:
        time_a_f64_z_extF80( f64_to_extF80M );
        break;
#endif
#ifdef FLOAT128
     case F64_TO_F128:
        time_a_f64_z_f128( f64_to_f128M );
        break;
#endif
     case F64_ROUNDTOINT:
        time_az_f64_rx( f64_roundToInt, roundingMode, exact );
        break;
     case F64_ADD:
        function_abz_f64 = f64_add;
        goto time_abz_f64;
     case F64_SUB:
        function_abz_f64 = f64_sub;
        goto time_abz_f64;
     case F64_MUL:
        function_abz_f64 = f64_mul;
        goto time_abz_f64;
     case F64_DIV:
        function_abz_f64 = f64_div;
        goto time_abz_f64;
     case F64_REM:
        function_abz_f64 = f64_rem;
     time_abz_f64:
        time_abz_f64( function_abz_f64 );
        break;
     case F64_MULADD:
        time_abcz_f64( f64_mulAdd );
        break;
     case F64_SQRT:
        time_az_f64_pos( f64_sqrt );
        break;
     case F64_EQ:
        function_ab_f64_z_bool = f64_eq;
        goto time_ab_f64_z_bool;
     case F64_LE:
        function_ab_f64_z_bool = f64_le;
        goto time_ab_f64_z_bool;
     case F64_LT:
        function_ab_f64_z_bool = f64_lt;
        goto time_ab_f64_z_bool;
     case F64_EQ_SIGNALING:
        function_ab_f64_z_bool = f64_eq_signaling;
        goto time_ab_f64_z_bool;
     case F64_LE_QUIET:
        function_ab_f64_z_bool = f64_le_quiet;
        goto time_ab_f64_z_bool;
     case F64_LT_QUIET:
        function_ab_f64_z_bool = f64_lt_quiet;
     time_ab_f64_z_bool:
        time_ab_f64_z_bool( function_ab_f64_z_bool );
        break;
#endif
        /*--------------------------------------------------------------------
        *--------------------------------------------------------------------*/
#ifdef EXTFLOAT80
     case EXTF80_TO_UI32:
        time_a_extF80_z_ui32_rx( extF80M_to_ui32, roundingMode, exact );
        break;
     case EXTF80_TO_UI64:
        time_a_extF80_z_ui64_rx( extF80M_to_ui64, roundingMode, exact );
        break;
     case EXTF80_TO_I32:
        time_a_extF80_z_i32_rx( extF80M_to_i32, roundingMode, exact );
        break;
     case EXTF80_TO_I64:
        time_a_extF80_z_i64_rx( extF80M_to_i64, roundingMode, exact );
        break;
     case EXTF80_TO_UI32_R_MINMAG:
        time_a_extF80_z_ui32_x( extF80M_to_ui32_r_minMag, exact );
        break;
     case EXTF80_TO_UI64_R_MINMAG:
        time_a_extF80_z_ui64_x( extF80M_to_ui64_r_minMag, exact );
        break;
     case EXTF80_TO_I32_R_MINMAG:
        time_a_extF80_z_i32_x( extF80M_to_i32_r_minMag, exact );
        break;
     case EXTF80_TO_I64_R_MINMAG:
        time_a_extF80_z_i64_x( extF80M_to_i64_r_minMag, exact );
        break;
#ifdef FLOAT16
     case EXTF80_TO_F16:
        time_a_extF80_z_f16( extF80M_to_f16 );
        break;
#endif
     case EXTF80_TO_F32:
        time_a_extF80_z_f32( extF80M_to_f32 );
        break;
#ifdef FLOAT64
     case EXTF80_TO_F64:
        time_a_extF80_z_f64( extF80M_to_f64 );
        break;
#endif
#ifdef FLOAT128
     case EXTF80_TO_F128:
        time_a_extF80_z_f128( extF80M_to_f128M );
        break;
#endif
     case EXTF80_ROUNDTOINT:
        time_az_extF80_rx( extF80M_roundToInt, roundingMode, exact );
        break;
     case EXTF80_ADD:
        function_abz_extF80 = extF80M_add;
        goto time_abz_extF80;
     case EXTF80_SUB:
        function_abz_extF80 = extF80M_sub;
        goto time_abz_extF80;
     case EXTF80_MUL:
        function_abz_extF80 = extF80M_mul;
        goto time_abz_extF80;
     case EXTF80_DIV:
        function_abz_extF80 = extF80M_div;
        goto time_abz_extF80;
     case EXTF80_REM:
        function_abz_extF80 = extF80M_rem;
     time_abz_extF80:
        time_abz_extF80( function_abz_extF80 );
        break;
     case EXTF80_SQRT:
        time_az_extF80_pos( extF80M_sqrt );
        break;
     case EXTF80_EQ:
        function_ab_extF80_z_bool = extF80M_eq;
        goto time_ab_extF80_z_bool;
     case EXTF80_LE:
        function_ab_extF80_z_bool = extF80M_le;
        goto time_ab_extF80_z_bool;
     case EXTF80_LT:
        function_ab_extF80_z_bool = extF80M_lt;
        goto time_ab_extF80_z_bool;
     case EXTF80_EQ_SIGNALING:
        function_ab_extF80_z_bool = extF80M_eq_signaling;
        goto time_ab_extF80_z_bool;
     case EXTF80_LE_QUIET:
        function_ab_extF80_z_bool = extF80M_le_quiet;
        goto time_ab_extF80_z_bool;
     case EXTF80_LT_QUIET:
        function_ab_extF80_z_bool = extF80M_lt_quiet;
     time_ab_extF80_z_bool:
        time_ab_extF80_z_bool( function_ab_extF80_z_bool );
        break;
#endif
        /*--------------------------------------------------------------------
        *--------------------------------------------------------------------*/
#ifdef FLOAT128
     case F128_TO_UI32:
        time_a_f128_z_ui32_rx( f128M_to_ui32, roundingMode, exact );
        break;
     case F128_TO_UI64:
        time_a_f128_z_ui64_rx( f128M_to_ui64, roundingMode, exact );
        break;
     case F128_TO_I32:
        time_a_f128_z_i32_rx( f128M_to_i32, roundingMode, exact );
        break;
     case F128_TO_I64:
        time_a_f128_z_i64_rx( f128M_to_i64, roundingMode, exact );
        break;
     case F128_TO_UI32_R_MINMAG:
        time_a_f128_z_ui32_x( f128M_to_ui32_r_minMag, exact );
        break;
     case F128_TO_UI64_R_MINMAG:
        time_a_f128_z_ui64_x( f128M_to_ui64_r_minMag, exact );
        break;
     case F128_TO_I32_R_MINMAG:
        time_a_f128_z_i32_x( f128M_to_i32_r_minMag, exact );
        break;
     case F128_TO_I64_R_MINMAG:
        time_a_f128_z_i64_x( f128M_to_i64_r_minMag, exact );
        break;
#ifdef FLOAT16
     case F128_TO_F16:
        time_a_f128_z_f16( f128M_to_f16 );
        break;
#endif
     case F128_TO_F32:
        time_a_f128_z_f32( f128M_to_f32 );
        break;
#ifdef FLOAT64
     case F128_TO_F64:
        time_a_f128_z_f64( f128M_to_f64 );
        break;
#endif
#ifdef EXTFLOAT80
     case F128_TO_EXTF80:
        time_a_f128_z_extF80( f128M_to_extF80M );
        break;
#endif
     case F128_ROUNDTOINT:
        time_az_f128_rx( f128M_roundToInt, roundingMode, exact );
        break;
     case F128_ADD:
        function_abz_f128 = f128M_add;
        goto time_abz_f128;
     case F128_SUB:
        function_abz_f128 = f128M_sub;
        goto time_abz_f128;
     case F128_MUL:
        function_abz_f128 = f128M_mul;
        goto time_abz_f128;
     case F128_DIV:
        function_abz_f128 = f128M_div;
        goto time_abz_f128;
     case F128_REM:
        function_abz_f128 = f128M_rem;
     time_abz_f128:
        time_abz_f128( function_abz_f128 );
        break;
     case F128_MULADD:
        time_abcz_f128( f128M_mulAdd );
        break;
     case F128_SQRT:
        time_az_f128_pos( f128M_sqrt );
        break;
     case F128_EQ:
        function_ab_f128_z_bool = f128M_eq;
        goto time_ab_f128_z_bool;
     case F128_LE:
        function_ab_f128_z_bool = f128M_le;
        goto time_ab_f128_z_bool;
     case F128_LT:
        function_ab_f128_z_bool = f128M_lt;
        goto time_ab_f128_z_bool;
     case F128_EQ_SIGNALING:
        function_ab_f128_z_bool = f128M_eq_signaling;
        goto time_ab_f128_z_bool;
     case F128_LE_QUIET:
        function_ab_f128_z_bool = f128M_le_quiet;
        goto time_ab_f128_z_bool;
     case F128_LT_QUIET:
        function_ab_f128_z_bool = f128M_lt_quiet;
     time_ab_f128_z_bool:
        time_ab_f128_z_bool( function_ab_f128_z_bool );
        break;
#endif
    }

}

enum { EXACT_FALSE = 1, EXACT_TRUE };

static
void
 timeFunction(
     int functionCode,
     uint_fast8_t roundingPrecisionIn,
     int roundingCodeIn,
     int tininessCodeIn,
     int exactCodeIn
 )
{
    int functionAttribs, exactCode;
    uint_fast8_t roundingMode, tininessMode;

    functionNamePtr = functionInfos[functionCode].namePtr;
    functionAttribs = functionInfos[functionCode].attribs;
    roundingPrecision = 32;
    for (;;) {
        if ( functionAttribs & FUNC_EFF_ROUNDINGPRECISION ) {
            if ( roundingPrecisionIn ) roundingPrecision = roundingPrecisionIn;
        } else {
            roundingPrecision = 0;
        }
#ifdef EXTFLOAT80
        if ( roundingPrecision ) extF80_roundingPrecision = roundingPrecision;
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
            if ( roundingCode ) {
                roundingMode = roundingModes[roundingCode];
                if ( functionAttribs & FUNC_EFF_ROUNDINGMODE ) {
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
                exact = (exactCode == EXACT_TRUE );
                usesExact = (exactCode != 0 );
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
                    if ( tininessCode ) {
                        tininessMode = tininessModes[tininessCode];
                        softfloat_detectTininess = tininessMode;
                    }
                    timeFunctionInstance( functionCode, roundingMode, exact );
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

int main( int argc, char *argv[] )
{
    bool haveFunctionArg;
    int functionCode, numOperands;
    uint_fast8_t roundingPrecision;
    int roundingCode, tininessCode, exactCode;
    const char *argPtr;

    fail_programName = "timesoftfloat";
    if ( argc <= 1 ) goto writeHelpMessage;
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
"timesoftfloat [<option>...] <function>\n"
"  <option>:  (* is default)\n"
"    -help            --Write this message and exit.\n"
#ifdef EXTFLOAT80
"    -precision32     --For extF80, time only 32-bit rounding precision.\n"
"    -precision64     --For extF80, time only 64-bit rounding precision.\n"
"    -precision80     --For extF80, time only 80-bit rounding precision.\n"
#endif
"    -rnear_even      --Time only rounding to nearest/even.\n"
"    -rminMag         --Time only rounding to minimum magnitude (toward zero).\n"
"    -rmin            --Time only rounding to minimum (down).\n"
"    -rmax            --Time only rounding to maximum (up).\n"
"    -rnear_maxMag    --Time only rounding to nearest/maximum magnitude\n"
"                         (nearest/away).\n"
#ifdef FLOAT_ROUND_ODD
"    -rodd            --Time only rounding to odd (jamming).\n"
#endif
"    -tininessbefore  --Time only underflow tininess detected before rounding.\n"
"    -tininessafter   --Time only underflow tininess detected after rounding.\n"
"    -notexact        --Time only non-exact rounding to integer (no inexact\n"
"                         exception).\n"
"    -exact           --Time only exact rounding to integer (allow inexact\n"
"                         exception).\n"
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
"    -all             --All functions.\n"
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
        } else if ( ! strcmp( argPtr, "all" ) ) {
            haveFunctionArg = true;
            functionCode = 0;
            numOperands = 0;
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
    if ( functionCode ) {
        timeFunction(
            functionCode,
            roundingPrecision,
            roundingCode,
            tininessCode,
            exactCode
        );
    } else {
        for (
            functionCode = 1; functionCode < NUM_FUNCTIONS; ++functionCode
        ) {
            if (
                ! numOperands
                    || (functionInfos[functionCode].attribs
                            & ((numOperands == 1) ? FUNC_ARG_UNARY
                                   : FUNC_ARG_BINARY))
            ) {
                timeFunction(
                    functionCode,
                    roundingPrecision,
                    roundingCode,
                    tininessCode,
                    exactCode
                );
            }
        }
    }
    return EXIT_SUCCESS;

}

