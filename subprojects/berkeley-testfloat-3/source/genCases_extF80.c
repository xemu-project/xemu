
/*============================================================================

This C source file is part of TestFloat, Release 3e, a package of programs for
testing the correctness of floating-point arithmetic complying with the IEEE
Standard for Floating-Point, by John R. Hauser.

Copyright 2011, 2012, 2013, 2014 The Regents of the University of California.
All rights reserved.

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
#include "platform.h"
#include "random.h"
#include "softfloat.h"
#include "genCases.h"

#ifdef EXTFLOAT80

struct sequence {
    int expNum, term1Num, term2Num;
    bool done;
};

enum {
    extF80NumQIn  =  22,
    extF80NumQOut =  76,
    extF80NumP1   =   4,
    extF80NumP2   = 248
};
static const uint16_t extF80QIn[extF80NumQIn] = {
    0x0000,    /* positive, subnormal       */
    0x0001,    /* positive, -16382          */
    0x3FBF,    /* positive,    -64          */
    0x3FFD,    /* positive,     -2          */
    0x3FFE,    /* positive,     -1          */
    0x3FFF,    /* positive,      0          */
    0x4000,    /* positive,      1          */
    0x4001,    /* positive,      2          */
    0x403F,    /* positive,     64          */
    0x7FFE,    /* positive,  16383          */
    0x7FFF,    /* positive, infinity or NaN */
    0x8000,    /* negative, subnormal       */
    0x8001,    /* negative, -16382          */
    0xBFBF,    /* negative,    -64          */
    0xBFFD,    /* negative,     -2          */
    0xBFFE,    /* negative,     -1          */
    0xBFFF,    /* negative,      0          */
    0xC000,    /* negative,      1          */
    0xC001,    /* negative,      2          */
    0xC03F,    /* negative,     64          */
    0xFFFE,    /* negative,  16383          */
    0xFFFF     /* negative, infinity or NaN */
};
static const uint16_t extF80QOut[extF80NumQOut] = {
    0x0000,    /* positive, subnormal       */
    0x0001,    /* positive, -16382          */
    0x0002,    /* positive, -16381          */
    0x3BFE,    /* positive,  -1025          */
    0x3BFF,    /* positive,  -1024          */
    0x3C00,    /* positive,  -1023          */
    0x3C01,    /* positive,  -1022          */
    0x3F7E,    /* positive,   -129          */
    0x3F7F,    /* positive,   -128          */
    0x3F80,    /* positive,   -127          */
    0x3F81,    /* positive,   -126          */
    0x3FBF,    /* positive,    -64          */
    0x3FFB,    /* positive,     -4          */
    0x3FFC,    /* positive,     -3          */
    0x3FFD,    /* positive,     -2          */
    0x3FFE,    /* positive,     -1          */
    0x3FFF,    /* positive,      0          */
    0x4000,    /* positive,      1          */
    0x4001,    /* positive,      2          */
    0x4002,    /* positive,      3          */
    0x4003,    /* positive,      4          */
    0x401C,    /* positive,     29          */
    0x401D,    /* positive,     30          */
    0x401E,    /* positive,     31          */
    0x401F,    /* positive,     32          */
    0x403C,    /* positive,     61          */
    0x403D,    /* positive,     62          */
    0x403E,    /* positive,     63          */
    0x403F,    /* positive,     64          */
    0x407E,    /* positive,    127          */
    0x407F,    /* positive,    128          */
    0x4080,    /* positive,    129          */
    0x43FE,    /* positive,   1023          */
    0x43FF,    /* positive,   1024          */
    0x4400,    /* positive,   1025          */
    0x7FFD,    /* positive,  16382          */
    0x7FFE,    /* positive,  16383          */
    0x7FFF,    /* positive, infinity or NaN */
    0x8000,    /* negative, subnormal       */
    0x8001,    /* negative, -16382          */
    0x8002,    /* negative, -16381          */
    0xBBFE,    /* negative,  -1025          */
    0xBBFF,    /* negative,  -1024          */
    0xBC00,    /* negative,  -1023          */
    0xBC01,    /* negative,  -1022          */
    0xBF7E,    /* negative,   -129          */
    0xBF7F,    /* negative,   -128          */
    0xBF80,    /* negative,   -127          */
    0xBF81,    /* negative,   -126          */
    0xBFBF,    /* negative,    -64          */
    0xBFFB,    /* negative,     -4          */
    0xBFFC,    /* negative,     -3          */
    0xBFFD,    /* negative,     -2          */
    0xBFFE,    /* negative,     -1          */
    0xBFFF,    /* negative,      0          */
    0xC000,    /* negative,      1          */
    0xC001,    /* negative,      2          */
    0xC002,    /* negative,      3          */
    0xC003,    /* negative,      4          */
    0xC01C,    /* negative,     29          */
    0xC01D,    /* negative,     30          */
    0xC01E,    /* negative,     31          */
    0xC01F,    /* negative,     32          */
    0xC03C,    /* negative,     61          */
    0xC03D,    /* negative,     62          */
    0xC03E,    /* negative,     63          */
    0xC03F,    /* negative,     64          */
    0xC07E,    /* negative,    127          */
    0xC07F,    /* negative,    128          */
    0xC080,    /* negative,    129          */
    0xC3FE,    /* negative,   1023          */
    0xC3FF,    /* negative,   1024          */
    0xC400,    /* negative,   1025          */
    0xFFFD,    /* negative,  16382          */
    0xFFFE,    /* negative,  16383          */
    0xFFFF     /* negative, infinity or NaN */
};
static const uint64_t extF80P1[extF80NumP1] = {
    UINT64_C( 0x0000000000000000 ),
    UINT64_C( 0x0000000000000001 ),
    UINT64_C( 0x7FFFFFFFFFFFFFFF ),
    UINT64_C( 0x7FFFFFFFFFFFFFFE )
};
static const uint64_t extF80P2[extF80NumP2] = {
    UINT64_C( 0x0000000000000000 ),
    UINT64_C( 0x0000000000000001 ),
    UINT64_C( 0x0000000000000002 ),
    UINT64_C( 0x0000000000000004 ),
    UINT64_C( 0x0000000000000008 ),
    UINT64_C( 0x0000000000000010 ),
    UINT64_C( 0x0000000000000020 ),
    UINT64_C( 0x0000000000000040 ),
    UINT64_C( 0x0000000000000080 ),
    UINT64_C( 0x0000000000000100 ),
    UINT64_C( 0x0000000000000200 ),
    UINT64_C( 0x0000000000000400 ),
    UINT64_C( 0x0000000000000800 ),
    UINT64_C( 0x0000000000001000 ),
    UINT64_C( 0x0000000000002000 ),
    UINT64_C( 0x0000000000004000 ),
    UINT64_C( 0x0000000000008000 ),
    UINT64_C( 0x0000000000010000 ),
    UINT64_C( 0x0000000000020000 ),
    UINT64_C( 0x0000000000040000 ),
    UINT64_C( 0x0000000000080000 ),
    UINT64_C( 0x0000000000100000 ),
    UINT64_C( 0x0000000000200000 ),
    UINT64_C( 0x0000000000400000 ),
    UINT64_C( 0x0000000000800000 ),
    UINT64_C( 0x0000000001000000 ),
    UINT64_C( 0x0000000002000000 ),
    UINT64_C( 0x0000000004000000 ),
    UINT64_C( 0x0000000008000000 ),
    UINT64_C( 0x0000000010000000 ),
    UINT64_C( 0x0000000020000000 ),
    UINT64_C( 0x0000000040000000 ),
    UINT64_C( 0x0000000080000000 ),
    UINT64_C( 0x0000000100000000 ),
    UINT64_C( 0x0000000200000000 ),
    UINT64_C( 0x0000000400000000 ),
    UINT64_C( 0x0000000800000000 ),
    UINT64_C( 0x0000001000000000 ),
    UINT64_C( 0x0000002000000000 ),
    UINT64_C( 0x0000004000000000 ),
    UINT64_C( 0x0000008000000000 ),
    UINT64_C( 0x0000010000000000 ),
    UINT64_C( 0x0000020000000000 ),
    UINT64_C( 0x0000040000000000 ),
    UINT64_C( 0x0000080000000000 ),
    UINT64_C( 0x0000100000000000 ),
    UINT64_C( 0x0000200000000000 ),
    UINT64_C( 0x0000400000000000 ),
    UINT64_C( 0x0000800000000000 ),
    UINT64_C( 0x0001000000000000 ),
    UINT64_C( 0x0002000000000000 ),
    UINT64_C( 0x0004000000000000 ),
    UINT64_C( 0x0008000000000000 ),
    UINT64_C( 0x0010000000000000 ),
    UINT64_C( 0x0020000000000000 ),
    UINT64_C( 0x0040000000000000 ),
    UINT64_C( 0x0080000000000000 ),
    UINT64_C( 0x0100000000000000 ),
    UINT64_C( 0x0200000000000000 ),
    UINT64_C( 0x0400000000000000 ),
    UINT64_C( 0x0800000000000000 ),
    UINT64_C( 0x1000000000000000 ),
    UINT64_C( 0x2000000000000000 ),
    UINT64_C( 0x4000000000000000 ),
    UINT64_C( 0x6000000000000000 ),
    UINT64_C( 0x7000000000000000 ),
    UINT64_C( 0x7800000000000000 ),
    UINT64_C( 0x7C00000000000000 ),
    UINT64_C( 0x7E00000000000000 ),
    UINT64_C( 0x7F00000000000000 ),
    UINT64_C( 0x7F80000000000000 ),
    UINT64_C( 0x7FC0000000000000 ),
    UINT64_C( 0x7FE0000000000000 ),
    UINT64_C( 0x7FF0000000000000 ),
    UINT64_C( 0x7FF8000000000000 ),
    UINT64_C( 0x7FFC000000000000 ),
    UINT64_C( 0x7FFE000000000000 ),
    UINT64_C( 0x7FFF000000000000 ),
    UINT64_C( 0x7FFF800000000000 ),
    UINT64_C( 0x7FFFC00000000000 ),
    UINT64_C( 0x7FFFE00000000000 ),
    UINT64_C( 0x7FFFF00000000000 ),
    UINT64_C( 0x7FFFF80000000000 ),
    UINT64_C( 0x7FFFFC0000000000 ),
    UINT64_C( 0x7FFFFE0000000000 ),
    UINT64_C( 0x7FFFFF0000000000 ),
    UINT64_C( 0x7FFFFF8000000000 ),
    UINT64_C( 0x7FFFFFC000000000 ),
    UINT64_C( 0x7FFFFFE000000000 ),
    UINT64_C( 0x7FFFFFF000000000 ),
    UINT64_C( 0x7FFFFFF800000000 ),
    UINT64_C( 0x7FFFFFFC00000000 ),
    UINT64_C( 0x7FFFFFFE00000000 ),
    UINT64_C( 0x7FFFFFFF00000000 ),
    UINT64_C( 0x7FFFFFFF80000000 ),
    UINT64_C( 0x7FFFFFFFC0000000 ),
    UINT64_C( 0x7FFFFFFFE0000000 ),
    UINT64_C( 0x7FFFFFFFF0000000 ),
    UINT64_C( 0x7FFFFFFFF8000000 ),
    UINT64_C( 0x7FFFFFFFFC000000 ),
    UINT64_C( 0x7FFFFFFFFE000000 ),
    UINT64_C( 0x7FFFFFFFFF000000 ),
    UINT64_C( 0x7FFFFFFFFF800000 ),
    UINT64_C( 0x7FFFFFFFFFC00000 ),
    UINT64_C( 0x7FFFFFFFFFE00000 ),
    UINT64_C( 0x7FFFFFFFFFF00000 ),
    UINT64_C( 0x7FFFFFFFFFF80000 ),
    UINT64_C( 0x7FFFFFFFFFFC0000 ),
    UINT64_C( 0x7FFFFFFFFFFE0000 ),
    UINT64_C( 0x7FFFFFFFFFFF0000 ),
    UINT64_C( 0x7FFFFFFFFFFF8000 ),
    UINT64_C( 0x7FFFFFFFFFFFC000 ),
    UINT64_C( 0x7FFFFFFFFFFFE000 ),
    UINT64_C( 0x7FFFFFFFFFFFF000 ),
    UINT64_C( 0x7FFFFFFFFFFFF800 ),
    UINT64_C( 0x7FFFFFFFFFFFFC00 ),
    UINT64_C( 0x7FFFFFFFFFFFFE00 ),
    UINT64_C( 0x7FFFFFFFFFFFFF00 ),
    UINT64_C( 0x7FFFFFFFFFFFFF80 ),
    UINT64_C( 0x7FFFFFFFFFFFFFC0 ),
    UINT64_C( 0x7FFFFFFFFFFFFFE0 ),
    UINT64_C( 0x7FFFFFFFFFFFFFF0 ),
    UINT64_C( 0x7FFFFFFFFFFFFFF8 ),
    UINT64_C( 0x7FFFFFFFFFFFFFFC ),
    UINT64_C( 0x7FFFFFFFFFFFFFFE ),
    UINT64_C( 0x7FFFFFFFFFFFFFFF ),
    UINT64_C( 0x7FFFFFFFFFFFFFFD ),
    UINT64_C( 0x7FFFFFFFFFFFFFFB ),
    UINT64_C( 0x7FFFFFFFFFFFFFF7 ),
    UINT64_C( 0x7FFFFFFFFFFFFFEF ),
    UINT64_C( 0x7FFFFFFFFFFFFFDF ),
    UINT64_C( 0x7FFFFFFFFFFFFFBF ),
    UINT64_C( 0x7FFFFFFFFFFFFF7F ),
    UINT64_C( 0x7FFFFFFFFFFFFEFF ),
    UINT64_C( 0x7FFFFFFFFFFFFDFF ),
    UINT64_C( 0x7FFFFFFFFFFFFBFF ),
    UINT64_C( 0x7FFFFFFFFFFFF7FF ),
    UINT64_C( 0x7FFFFFFFFFFFEFFF ),
    UINT64_C( 0x7FFFFFFFFFFFDFFF ),
    UINT64_C( 0x7FFFFFFFFFFFBFFF ),
    UINT64_C( 0x7FFFFFFFFFFF7FFF ),
    UINT64_C( 0x7FFFFFFFFFFEFFFF ),
    UINT64_C( 0x7FFFFFFFFFFDFFFF ),
    UINT64_C( 0x7FFFFFFFFFFBFFFF ),
    UINT64_C( 0x7FFFFFFFFFF7FFFF ),
    UINT64_C( 0x7FFFFFFFFFEFFFFF ),
    UINT64_C( 0x7FFFFFFFFFDFFFFF ),
    UINT64_C( 0x7FFFFFFFFFBFFFFF ),
    UINT64_C( 0x7FFFFFFFFF7FFFFF ),
    UINT64_C( 0x7FFFFFFFFEFFFFFF ),
    UINT64_C( 0x7FFFFFFFFDFFFFFF ),
    UINT64_C( 0x7FFFFFFFFBFFFFFF ),
    UINT64_C( 0x7FFFFFFFF7FFFFFF ),
    UINT64_C( 0x7FFFFFFFEFFFFFFF ),
    UINT64_C( 0x7FFFFFFFDFFFFFFF ),
    UINT64_C( 0x7FFFFFFFBFFFFFFF ),
    UINT64_C( 0x7FFFFFFF7FFFFFFF ),
    UINT64_C( 0x7FFFFFFEFFFFFFFF ),
    UINT64_C( 0x7FFFFFFDFFFFFFFF ),
    UINT64_C( 0x7FFFFFFBFFFFFFFF ),
    UINT64_C( 0x7FFFFFF7FFFFFFFF ),
    UINT64_C( 0x7FFFFFEFFFFFFFFF ),
    UINT64_C( 0x7FFFFFDFFFFFFFFF ),
    UINT64_C( 0x7FFFFFBFFFFFFFFF ),
    UINT64_C( 0x7FFFFF7FFFFFFFFF ),
    UINT64_C( 0x7FFFFEFFFFFFFFFF ),
    UINT64_C( 0x7FFFFDFFFFFFFFFF ),
    UINT64_C( 0x7FFFFBFFFFFFFFFF ),
    UINT64_C( 0x7FFFF7FFFFFFFFFF ),
    UINT64_C( 0x7FFFEFFFFFFFFFFF ),
    UINT64_C( 0x7FFFDFFFFFFFFFFF ),
    UINT64_C( 0x7FFFBFFFFFFFFFFF ),
    UINT64_C( 0x7FFF7FFFFFFFFFFF ),
    UINT64_C( 0x7FFEFFFFFFFFFFFF ),
    UINT64_C( 0x7FFDFFFFFFFFFFFF ),
    UINT64_C( 0x7FFBFFFFFFFFFFFF ),
    UINT64_C( 0x7FF7FFFFFFFFFFFF ),
    UINT64_C( 0x7FEFFFFFFFFFFFFF ),
    UINT64_C( 0x7FDFFFFFFFFFFFFF ),
    UINT64_C( 0x7FBFFFFFFFFFFFFF ),
    UINT64_C( 0x7F7FFFFFFFFFFFFF ),
    UINT64_C( 0x7EFFFFFFFFFFFFFF ),
    UINT64_C( 0x7DFFFFFFFFFFFFFF ),
    UINT64_C( 0x7BFFFFFFFFFFFFFF ),
    UINT64_C( 0x77FFFFFFFFFFFFFF ),
    UINT64_C( 0x6FFFFFFFFFFFFFFF ),
    UINT64_C( 0x5FFFFFFFFFFFFFFF ),
    UINT64_C( 0x3FFFFFFFFFFFFFFF ),
    UINT64_C( 0x1FFFFFFFFFFFFFFF ),
    UINT64_C( 0x0FFFFFFFFFFFFFFF ),
    UINT64_C( 0x07FFFFFFFFFFFFFF ),
    UINT64_C( 0x03FFFFFFFFFFFFFF ),
    UINT64_C( 0x01FFFFFFFFFFFFFF ),
    UINT64_C( 0x00FFFFFFFFFFFFFF ),
    UINT64_C( 0x007FFFFFFFFFFFFF ),
    UINT64_C( 0x003FFFFFFFFFFFFF ),
    UINT64_C( 0x001FFFFFFFFFFFFF ),
    UINT64_C( 0x000FFFFFFFFFFFFF ),
    UINT64_C( 0x0007FFFFFFFFFFFF ),
    UINT64_C( 0x0003FFFFFFFFFFFF ),
    UINT64_C( 0x0001FFFFFFFFFFFF ),
    UINT64_C( 0x0000FFFFFFFFFFFF ),
    UINT64_C( 0x00007FFFFFFFFFFF ),
    UINT64_C( 0x00003FFFFFFFFFFF ),
    UINT64_C( 0x00001FFFFFFFFFFF ),
    UINT64_C( 0x00000FFFFFFFFFFF ),
    UINT64_C( 0x000007FFFFFFFFFF ),
    UINT64_C( 0x000003FFFFFFFFFF ),
    UINT64_C( 0x000001FFFFFFFFFF ),
    UINT64_C( 0x000000FFFFFFFFFF ),
    UINT64_C( 0x0000007FFFFFFFFF ),
    UINT64_C( 0x0000003FFFFFFFFF ),
    UINT64_C( 0x0000001FFFFFFFFF ),
    UINT64_C( 0x0000000FFFFFFFFF ),
    UINT64_C( 0x00000007FFFFFFFF ),
    UINT64_C( 0x00000003FFFFFFFF ),
    UINT64_C( 0x00000001FFFFFFFF ),
    UINT64_C( 0x00000000FFFFFFFF ),
    UINT64_C( 0x000000007FFFFFFF ),
    UINT64_C( 0x000000003FFFFFFF ),
    UINT64_C( 0x000000001FFFFFFF ),
    UINT64_C( 0x000000000FFFFFFF ),
    UINT64_C( 0x0000000007FFFFFF ),
    UINT64_C( 0x0000000003FFFFFF ),
    UINT64_C( 0x0000000001FFFFFF ),
    UINT64_C( 0x0000000000FFFFFF ),
    UINT64_C( 0x00000000007FFFFF ),
    UINT64_C( 0x00000000003FFFFF ),
    UINT64_C( 0x00000000001FFFFF ),
    UINT64_C( 0x00000000000FFFFF ),
    UINT64_C( 0x000000000007FFFF ),
    UINT64_C( 0x000000000003FFFF ),
    UINT64_C( 0x000000000001FFFF ),
    UINT64_C( 0x000000000000FFFF ),
    UINT64_C( 0x0000000000007FFF ),
    UINT64_C( 0x0000000000003FFF ),
    UINT64_C( 0x0000000000001FFF ),
    UINT64_C( 0x0000000000000FFF ),
    UINT64_C( 0x00000000000007FF ),
    UINT64_C( 0x00000000000003FF ),
    UINT64_C( 0x00000000000001FF ),
    UINT64_C( 0x00000000000000FF ),
    UINT64_C( 0x000000000000007F ),
    UINT64_C( 0x000000000000003F ),
    UINT64_C( 0x000000000000001F ),
    UINT64_C( 0x000000000000000F ),
    UINT64_C( 0x0000000000000007 ),
    UINT64_C( 0x0000000000000003 )
};

static const uint_fast32_t extF80NumQInP1 = extF80NumQIn * extF80NumP1;
static const uint_fast32_t extF80NumQOutP1 = extF80NumQOut * extF80NumP1;

static void extF80NextQInP1( struct sequence *sequencePtr, extFloat80_t *zPtr )
{
    struct extFloat80M *zSPtr;
    int expNum, sigNum;
    uint_fast16_t uiZ64;
    uint_fast64_t uiZ0;

    zSPtr = (struct extFloat80M *) zPtr;
    expNum = sequencePtr->expNum;
    sigNum = sequencePtr->term1Num;
    uiZ64 = extF80QIn[expNum];
    uiZ0  = extF80P1[sigNum];
    if ( uiZ64 & 0x7FFF ) uiZ0 |= UINT64_C( 0x8000000000000000 );
    zSPtr->signExp = uiZ64;
    zSPtr->signif  = uiZ0;
    ++sigNum;
    if ( extF80NumP1 <= sigNum ) {
        sigNum = 0;
        ++expNum;
        if ( extF80NumQIn <= expNum ) {
            expNum = 0;
            sequencePtr->done = true;
        }
        sequencePtr->expNum = expNum;
    }
    sequencePtr->term1Num = sigNum;

}

static
 void extF80NextQOutP1( struct sequence *sequencePtr, extFloat80_t *zPtr )
{
    struct extFloat80M *zSPtr;
    int expNum, sigNum;
    uint_fast16_t uiZ64;
    uint_fast64_t uiZ0;

    zSPtr = (struct extFloat80M *) zPtr;
    expNum = sequencePtr->expNum;
    sigNum = sequencePtr->term1Num;
    uiZ64 = extF80QOut[expNum];
    uiZ0  = extF80P1[sigNum];
    if ( uiZ64 & 0x7FFF ) uiZ0 |= UINT64_C( 0x8000000000000000 );
    zSPtr->signExp = uiZ64;
    zSPtr->signif  = uiZ0;
    ++sigNum;
    if ( extF80NumP1 <= sigNum ) {
        sigNum = 0;
        ++expNum;
        if ( extF80NumQOut <= expNum ) {
            expNum = 0;
            sequencePtr->done = true;
        }
        sequencePtr->expNum = expNum;
    }
    sequencePtr->term1Num = sigNum;

}

static const uint_fast32_t extF80NumQInP2 = extF80NumQIn * extF80NumP2;
static const uint_fast32_t extF80NumQOutP2 = extF80NumQOut * extF80NumP2;

static void extF80NextQInP2( struct sequence *sequencePtr, extFloat80_t *zPtr )
{
    struct extFloat80M *zSPtr;
    int expNum, sigNum;
    uint_fast16_t uiZ64;
    uint_fast64_t uiZ0;

    zSPtr = (struct extFloat80M *) zPtr;
    expNum = sequencePtr->expNum;
    sigNum = sequencePtr->term1Num;
    uiZ64 = extF80QIn[expNum];
    uiZ0  = extF80P2[sigNum];
    if ( uiZ64 & 0x7FFF ) uiZ0 |= UINT64_C( 0x8000000000000000 );
    zSPtr->signExp = uiZ64;
    zSPtr->signif  = uiZ0;
    ++sigNum;
    if ( extF80NumP2 <= sigNum ) {
        sigNum = 0;
        ++expNum;
        if ( extF80NumQIn <= expNum ) {
            expNum = 0;
            sequencePtr->done = true;
        }
        sequencePtr->expNum = expNum;
    }
    sequencePtr->term1Num = sigNum;

}

static
 void extF80NextQOutP2( struct sequence *sequencePtr, extFloat80_t *zPtr )
{
    struct extFloat80M *zSPtr;
    int expNum, sigNum;
    uint_fast16_t uiZ64;
    uint_fast64_t uiZ0;

    zSPtr = (struct extFloat80M *) zPtr;
    expNum = sequencePtr->expNum;
    sigNum = sequencePtr->term1Num;
    uiZ64 = extF80QOut[expNum];
    uiZ0  = extF80P2[sigNum];
    if ( uiZ64 & 0x7FFF ) uiZ0 |= UINT64_C( 0x8000000000000000 );
    zSPtr->signExp = uiZ64;
    zSPtr->signif  = uiZ0;
    ++sigNum;
    if ( extF80NumP2 <= sigNum ) {
        sigNum = 0;
        ++expNum;
        if ( extF80NumQOut <= expNum ) {
            expNum = 0;
            sequencePtr->done = true;
        }
        sequencePtr->expNum = expNum;
    }
    sequencePtr->term1Num = sigNum;

}

static void extF80RandomQOutP3( extFloat80_t *zPtr )
{
    struct extFloat80M *zSPtr;
    uint_fast16_t uiZ64;
    uint_fast64_t uiZ0;

    zSPtr = (struct extFloat80M *) zPtr;
    uiZ64 = extF80QOut[randomN_ui8( extF80NumQOut )];
    uiZ0 =
        (extF80P2[randomN_ui8( extF80NumP2 )]
             + extF80P2[randomN_ui8( extF80NumP2 )])
            & UINT64_C( 0x7FFFFFFFFFFFFFFF );
    if ( uiZ64 & 0x7FFF ) uiZ0 |= UINT64_C( 0x8000000000000000 );
    zSPtr->signExp = uiZ64;
    zSPtr->signif  = uiZ0;

}

static void extF80RandomQOutPInf( extFloat80_t *zPtr )
{
    struct extFloat80M *zSPtr;
    uint_fast16_t uiZ64;
    uint_fast64_t uiZ0;

    zSPtr = (struct extFloat80M *) zPtr;
    uiZ64 = extF80QOut[randomN_ui8( extF80NumQOut )];
    uiZ0 = random_ui64() & UINT64_C( 0x7FFFFFFFFFFFFFFF );
    if ( uiZ64 & 0x7FFF ) uiZ0 |= UINT64_C( 0x8000000000000000 );
    zSPtr->signExp = uiZ64;
    zSPtr->signif  = uiZ0;

}

enum { extF80NumQInfWeightMasks = 14 };
static const uint16_t extF80QInfWeightMasks[extF80NumQInfWeightMasks] = {
    0xFFFF,
    0xFFFF,
    0xBFFF,
    0x9FFF,
    0x87FF,
    0x87FF,
    0x83FF,
    0x81FF,
    0x80FF,
    0x807F,
    0x803F,
    0x801F,
    0x800F,
    0x8007
};
static const uint16_t extF80QInfWeightOffsets[extF80NumQInfWeightMasks] = {
    0x0000,
    0x0000,
    0x2000,
    0x3000,
    0x3800,
    0x3C00,
    0x3E00,
    0x3F00,
    0x3F80,
    0x3FC0,
    0x3FE0,
    0x3FF0,
    0x3FF8,
    0x3FFC
};

static void extF80RandomQInfP3( extFloat80_t *zPtr )
{
    struct extFloat80M *zSPtr;
    int weightMaskNum;
    uint_fast16_t uiZ64;
    uint_fast64_t uiZ0;

    zSPtr = (struct extFloat80M *) zPtr;
    weightMaskNum = randomN_ui8( extF80NumQInfWeightMasks );
    uiZ64 =
        (random_ui16() & extF80QInfWeightMasks[weightMaskNum])
            + extF80QInfWeightOffsets[weightMaskNum];
    uiZ0 =
        (extF80P2[randomN_ui8( extF80NumP2 )]
             + extF80P2[randomN_ui8( extF80NumP2 )])
            & UINT64_C( 0x7FFFFFFFFFFFFFFF );
    if ( uiZ64 & 0x7FFF ) uiZ0 |= UINT64_C( 0x8000000000000000 );
    zSPtr->signExp = uiZ64;
    zSPtr->signif  = uiZ0;

}

static void extF80RandomQInfPInf( extFloat80_t *zPtr )
{
    struct extFloat80M *zSPtr;
    int weightMaskNum;
    uint_fast16_t uiZ64;
    uint_fast64_t uiZ0;

    zSPtr = (struct extFloat80M *) zPtr;
    weightMaskNum = randomN_ui8( extF80NumQInfWeightMasks );
    uiZ64 =
        (random_ui16() & extF80QInfWeightMasks[weightMaskNum])
            + extF80QInfWeightOffsets[weightMaskNum];
    uiZ0 = random_ui64() & UINT64_C( 0x7FFFFFFFFFFFFFFF );
    if ( uiZ64 & 0x7FFF ) uiZ0 |= UINT64_C( 0x8000000000000000 );
    zSPtr->signExp = uiZ64;
    zSPtr->signif  = uiZ0;

}

static void extF80Random( extFloat80_t *zPtr )
{

    switch ( random_ui8() & 7 ) {
     case 0:
     case 1:
     case 2:
        extF80RandomQOutP3( zPtr );
        break;
     case 3:
        extF80RandomQOutPInf( zPtr );
        break;
     case 4:
     case 5:
     case 6:
        extF80RandomQInfP3( zPtr );
        break;
     case 7:
        extF80RandomQInfPInf( zPtr );
        break;
    }

}

static struct sequence sequenceA, sequenceB, sequenceC;
static extFloat80_t currentA, currentB, currentC;
static int subcase;

extFloat80_t genCases_extF80_a, genCases_extF80_b, genCases_extF80_c;

void genCases_extF80_a_init( void )
{

    sequenceA.expNum = 0;
    sequenceA.term1Num = 0;
    sequenceA.term2Num = 0;
    sequenceA.done = false;
    subcase = 0;
    genCases_total =
        (genCases_level == 1) ? 3 * extF80NumQOutP1 : 2 * extF80NumQOutP2;
    genCases_done = false;

}

void genCases_extF80_a_next( void )
{

    if ( genCases_level == 1 ) {
        switch ( subcase ) {
         case 0:
         case 1:
            extF80Random( &genCases_extF80_a );
            break;
         case 2:
            extF80NextQOutP1( &sequenceA, &genCases_extF80_a );
            genCases_done = sequenceA.done;
            subcase = -1;
            break;
        }
     } else {
        switch ( subcase ) {
         case 0:
            extF80Random( &genCases_extF80_a );
            break;
         case 1:
            extF80NextQOutP2( &sequenceA, &genCases_extF80_a );
            genCases_done = sequenceA.done;
            subcase = -1;
            break;
        }
    }
    ++subcase;

}

void genCases_extF80_ab_init( void )
{

    sequenceA.expNum = 0;
    sequenceA.term1Num = 0;
    sequenceA.term2Num = 0;
    sequenceA.done = false;
    sequenceB.expNum = 0;
    sequenceB.term1Num = 0;
    sequenceB.term2Num = 0;
    sequenceB.done = false;
    subcase = 0;
    if ( genCases_level == 1 ) {
        genCases_total = 6 * extF80NumQInP1 * extF80NumQInP1;
        extF80NextQInP1( &sequenceA, &currentA );
    } else {
        genCases_total = 2 * extF80NumQInP2 * extF80NumQInP2;
        extF80NextQInP2( &sequenceA, &currentA );
    }
    genCases_done = false;

}

void genCases_extF80_ab_next( void )
{

    if ( genCases_level == 1 ) {
        switch ( subcase ) {
         case 0:
            if ( sequenceB.done ) {
                sequenceB.done = false;
                extF80NextQInP1( &sequenceA, &currentA );
            }
            extF80NextQInP1( &sequenceB, &currentB );
         case 2:
         case 4:
            extF80Random( &genCases_extF80_a );
            extF80Random( &genCases_extF80_b );
            break;
         case 1:
            genCases_extF80_a = currentA;
            extF80Random( &genCases_extF80_b );
            break;
         case 3:
            extF80Random( &genCases_extF80_a );
            genCases_extF80_b = currentB;
            break;
         case 5:
            genCases_extF80_a = currentA;
            genCases_extF80_b = currentB;
            genCases_done = sequenceA.done & sequenceB.done;
            subcase = -1;
            break;
        }
    } else {
        switch ( subcase ) {
         case 0:
            extF80Random( &genCases_extF80_a );
            extF80Random( &genCases_extF80_b );
            break;
         case 1:
            if ( sequenceB.done ) {
                sequenceB.done = false;
                extF80NextQInP2( &sequenceA, &currentA );
            }
            genCases_extF80_a = currentA;
            extF80NextQInP2( &sequenceB, &genCases_extF80_b );
            genCases_done = sequenceA.done & sequenceB.done;
            subcase = -1;
            break;
        }
    }
    ++subcase;

}

void genCases_extF80_abc_init( void )
{

    sequenceA.expNum = 0;
    sequenceA.term1Num = 0;
    sequenceA.term2Num = 0;
    sequenceA.done = false;
    sequenceB.expNum = 0;
    sequenceB.term1Num = 0;
    sequenceB.term2Num = 0;
    sequenceB.done = false;
    sequenceC.expNum = 0;
    sequenceC.term1Num = 0;
    sequenceC.term2Num = 0;
    sequenceC.done = false;
    subcase = 0;
    if ( genCases_level == 1 ) {
        genCases_total = 9 * extF80NumQInP1 * extF80NumQInP1 * extF80NumQInP1;
        extF80NextQInP1( &sequenceA, &currentA );
        extF80NextQInP1( &sequenceB, &currentB );
    } else {
        genCases_total = 2 * extF80NumQInP2 * extF80NumQInP2 * extF80NumQInP2;
        extF80NextQInP2( &sequenceA, &currentA );
        extF80NextQInP2( &sequenceB, &currentB );
    }
    genCases_done = false;

}

void genCases_extF80_abc_next( void )
{

    if ( genCases_level == 1 ) {
        switch ( subcase ) {
         case 0:
            if ( sequenceC.done ) {
                sequenceC.done = false;
                if ( sequenceB.done ) {
                    sequenceB.done = false;
                    extF80NextQInP1( &sequenceA, &currentA );
                }
                extF80NextQInP1( &sequenceB, &currentB );
            }
            extF80NextQInP1( &sequenceC, &currentC );
            extF80Random( &genCases_extF80_a );
            extF80Random( &genCases_extF80_b );
            genCases_extF80_c = currentC;
            break;
         case 1:
            genCases_extF80_a = currentA;
            genCases_extF80_b = currentB;
            extF80Random( &genCases_extF80_c );
            break;
         case 2:
            extF80Random( &genCases_extF80_a );
            extF80Random( &genCases_extF80_b );
            extF80Random( &genCases_extF80_c );
            break;
         case 3:
            extF80Random( &genCases_extF80_a );
            genCases_extF80_b = currentB;
            genCases_extF80_c = currentC;
            break;
         case 4:
            genCases_extF80_a = currentA;
            extF80Random( &genCases_extF80_b );
            extF80Random( &genCases_extF80_c );
            break;
         case 5:
            extF80Random( &genCases_extF80_a );
            genCases_extF80_b = currentB;
            extF80Random( &genCases_extF80_c );
            break;
         case 6:
            genCases_extF80_a = currentA;
            extF80Random( &genCases_extF80_b );
            genCases_extF80_c = currentC;
            break;
         case 7:
            extF80Random( &genCases_extF80_a );
            extF80Random( &genCases_extF80_b );
            extF80Random( &genCases_extF80_c );
            break;
         case 8:
            genCases_extF80_a = currentA;
            genCases_extF80_b = currentB;
            genCases_extF80_c = currentC;
            genCases_done = sequenceA.done & sequenceB.done & sequenceC.done;
            subcase = -1;
            break;
        }
    } else {
        switch ( subcase ) {
         case 0:
            extF80Random( &genCases_extF80_a );
            extF80Random( &genCases_extF80_b );
            extF80Random( &genCases_extF80_c );
            break;
         case 1:
            if ( sequenceC.done ) {
                sequenceC.done = false;
                if ( sequenceB.done ) {
                    sequenceB.done = false;
                    extF80NextQInP2( &sequenceA, &currentA );
                }
                extF80NextQInP2( &sequenceB, &currentB );
            }
            genCases_extF80_a = currentA;
            genCases_extF80_b = currentB;
            extF80NextQInP2( &sequenceC, &genCases_extF80_c );
            genCases_done = sequenceA.done & sequenceB.done & sequenceC.done;
            subcase = -1;
            break;
        }
    }
    ++subcase;

}

#endif

