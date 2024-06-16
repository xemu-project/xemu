
/*============================================================================

This C source file is part of TestFloat, Release 3e, a package of programs for
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
#include "platform.h"
#include "random.h"
#include "softfloat.h"
#include "genCases.h"

#ifdef FLOAT64

struct sequence {
    int expNum, term1Num, term2Num;
    bool done;
};

union ui64_f64 { uint64_t ui; float64_t f; };

enum {
    f64NumQIn  =  22,
    f64NumQOut =  64,
    f64NumP1   =   4,
    f64NumP2   = 204
};
static const uint64_t f64QIn[f64NumQIn] = {
    UINT64_C( 0x0000000000000000 ),    /* positive, subnormal       */
    UINT64_C( 0x0010000000000000 ),    /* positive, -1022           */
    UINT64_C( 0x3CA0000000000000 ),    /* positive,   -53           */
    UINT64_C( 0x3FD0000000000000 ),    /* positive,    -2           */
    UINT64_C( 0x3FE0000000000000 ),    /* positive,    -1           */
    UINT64_C( 0x3FF0000000000000 ),    /* positive,     0           */
    UINT64_C( 0x4000000000000000 ),    /* positive,     1           */
    UINT64_C( 0x4010000000000000 ),    /* positive,     2           */
    UINT64_C( 0x4340000000000000 ),    /* positive,    53           */
    UINT64_C( 0x7FE0000000000000 ),    /* positive,  1023           */
    UINT64_C( 0x7FF0000000000000 ),    /* positive, infinity or NaN */
    UINT64_C( 0x8000000000000000 ),    /* negative, subnormal       */
    UINT64_C( 0x8010000000000000 ),    /* negative, -1022           */
    UINT64_C( 0xBCA0000000000000 ),    /* negative,   -53           */
    UINT64_C( 0xBFD0000000000000 ),    /* negative,    -2           */
    UINT64_C( 0xBFE0000000000000 ),    /* negative,    -1           */
    UINT64_C( 0xBFF0000000000000 ),    /* negative,     0           */
    UINT64_C( 0xC000000000000000 ),    /* negative,     1           */
    UINT64_C( 0xC010000000000000 ),    /* negative,     2           */
    UINT64_C( 0xC340000000000000 ),    /* negative,    53           */
    UINT64_C( 0xFFE0000000000000 ),    /* negative,  1023           */
    UINT64_C( 0xFFF0000000000000 )     /* negative, infinity or NaN */
};
static const uint64_t f64QOut[f64NumQOut] = {
    UINT64_C( 0x0000000000000000 ),    /* positive, subnormal       */
    UINT64_C( 0x0010000000000000 ),    /* positive, -1022           */
    UINT64_C( 0x0020000000000000 ),    /* positive, -1021           */
    UINT64_C( 0x37E0000000000000 ),    /* positive,  -129           */
    UINT64_C( 0x37F0000000000000 ),    /* positive,  -128           */
    UINT64_C( 0x3800000000000000 ),    /* positive,  -127           */
    UINT64_C( 0x3810000000000000 ),    /* positive,  -126           */
    UINT64_C( 0x3CA0000000000000 ),    /* positive,   -53           */
    UINT64_C( 0x3FB0000000000000 ),    /* positive,    -4           */
    UINT64_C( 0x3FC0000000000000 ),    /* positive,    -3           */
    UINT64_C( 0x3FD0000000000000 ),    /* positive,    -2           */
    UINT64_C( 0x3FE0000000000000 ),    /* positive,    -1           */
    UINT64_C( 0x3FF0000000000000 ),    /* positive,     0           */
    UINT64_C( 0x4000000000000000 ),    /* positive,     1           */
    UINT64_C( 0x4010000000000000 ),    /* positive,     2           */
    UINT64_C( 0x4020000000000000 ),    /* positive,     3           */
    UINT64_C( 0x4030000000000000 ),    /* positive,     4           */
    UINT64_C( 0x41C0000000000000 ),    /* positive,    29           */
    UINT64_C( 0x41D0000000000000 ),    /* positive,    30           */
    UINT64_C( 0x41E0000000000000 ),    /* positive,    31           */
    UINT64_C( 0x41F0000000000000 ),    /* positive,    32           */
    UINT64_C( 0x4340000000000000 ),    /* positive,    53           */
    UINT64_C( 0x43C0000000000000 ),    /* positive,    61           */
    UINT64_C( 0x43D0000000000000 ),    /* positive,    62           */
    UINT64_C( 0x43E0000000000000 ),    /* positive,    63           */
    UINT64_C( 0x43F0000000000000 ),    /* positive,    64           */
    UINT64_C( 0x47E0000000000000 ),    /* positive,   127           */
    UINT64_C( 0x47F0000000000000 ),    /* positive,   128           */
    UINT64_C( 0x4800000000000000 ),    /* positive,   129           */
    UINT64_C( 0x7FD0000000000000 ),    /* positive,  1022           */
    UINT64_C( 0x7FE0000000000000 ),    /* positive,  1023           */
    UINT64_C( 0x7FF0000000000000 ),    /* positive, infinity or NaN */
    UINT64_C( 0x8000000000000000 ),    /* negative, subnormal       */
    UINT64_C( 0x8010000000000000 ),    /* negative, -1022           */
    UINT64_C( 0x8020000000000000 ),    /* negative, -1021           */
    UINT64_C( 0xB7E0000000000000 ),    /* negative,  -129           */
    UINT64_C( 0xB7F0000000000000 ),    /* negative,  -128           */
    UINT64_C( 0xB800000000000000 ),    /* negative,  -127           */
    UINT64_C( 0xB810000000000000 ),    /* negative,  -126           */
    UINT64_C( 0xBCA0000000000000 ),    /* negative,   -53           */
    UINT64_C( 0xBFB0000000000000 ),    /* negative,    -4           */
    UINT64_C( 0xBFC0000000000000 ),    /* negative,    -3           */
    UINT64_C( 0xBFD0000000000000 ),    /* negative,    -2           */
    UINT64_C( 0xBFE0000000000000 ),    /* negative,    -1           */
    UINT64_C( 0xBFF0000000000000 ),    /* negative,     0           */
    UINT64_C( 0xC000000000000000 ),    /* negative,     1           */
    UINT64_C( 0xC010000000000000 ),    /* negative,     2           */
    UINT64_C( 0xC020000000000000 ),    /* negative,     3           */
    UINT64_C( 0xC030000000000000 ),    /* negative,     4           */
    UINT64_C( 0xC1C0000000000000 ),    /* negative,    29           */
    UINT64_C( 0xC1D0000000000000 ),    /* negative,    30           */
    UINT64_C( 0xC1E0000000000000 ),    /* negative,    31           */
    UINT64_C( 0xC1F0000000000000 ),    /* negative,    32           */
    UINT64_C( 0xC340000000000000 ),    /* negative,    53           */
    UINT64_C( 0xC3C0000000000000 ),    /* negative,    61           */
    UINT64_C( 0xC3D0000000000000 ),    /* negative,    62           */
    UINT64_C( 0xC3E0000000000000 ),    /* negative,    63           */
    UINT64_C( 0xC3F0000000000000 ),    /* negative,    64           */
    UINT64_C( 0xC7E0000000000000 ),    /* negative,   127           */
    UINT64_C( 0xC7F0000000000000 ),    /* negative,   128           */
    UINT64_C( 0xC800000000000000 ),    /* negative,   129           */
    UINT64_C( 0xFFD0000000000000 ),    /* negative,  1022           */
    UINT64_C( 0xFFE0000000000000 ),    /* negative,  1023           */
    UINT64_C( 0xFFF0000000000000 )     /* negative, infinity or NaN */
};
static const uint64_t f64P1[f64NumP1] = {
    UINT64_C( 0x0000000000000000 ),
    UINT64_C( 0x0000000000000001 ),
    UINT64_C( 0x000FFFFFFFFFFFFF ),
    UINT64_C( 0x000FFFFFFFFFFFFE )
};
static const uint64_t f64P2[f64NumP2] = {
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
    UINT64_C( 0x000C000000000000 ),
    UINT64_C( 0x000E000000000000 ),
    UINT64_C( 0x000F000000000000 ),
    UINT64_C( 0x000F800000000000 ),
    UINT64_C( 0x000FC00000000000 ),
    UINT64_C( 0x000FE00000000000 ),
    UINT64_C( 0x000FF00000000000 ),
    UINT64_C( 0x000FF80000000000 ),
    UINT64_C( 0x000FFC0000000000 ),
    UINT64_C( 0x000FFE0000000000 ),
    UINT64_C( 0x000FFF0000000000 ),
    UINT64_C( 0x000FFF8000000000 ),
    UINT64_C( 0x000FFFC000000000 ),
    UINT64_C( 0x000FFFE000000000 ),
    UINT64_C( 0x000FFFF000000000 ),
    UINT64_C( 0x000FFFF800000000 ),
    UINT64_C( 0x000FFFFC00000000 ),
    UINT64_C( 0x000FFFFE00000000 ),
    UINT64_C( 0x000FFFFF00000000 ),
    UINT64_C( 0x000FFFFF80000000 ),
    UINT64_C( 0x000FFFFFC0000000 ),
    UINT64_C( 0x000FFFFFE0000000 ),
    UINT64_C( 0x000FFFFFF0000000 ),
    UINT64_C( 0x000FFFFFF8000000 ),
    UINT64_C( 0x000FFFFFFC000000 ),
    UINT64_C( 0x000FFFFFFE000000 ),
    UINT64_C( 0x000FFFFFFF000000 ),
    UINT64_C( 0x000FFFFFFF800000 ),
    UINT64_C( 0x000FFFFFFFC00000 ),
    UINT64_C( 0x000FFFFFFFE00000 ),
    UINT64_C( 0x000FFFFFFFF00000 ),
    UINT64_C( 0x000FFFFFFFF80000 ),
    UINT64_C( 0x000FFFFFFFFC0000 ),
    UINT64_C( 0x000FFFFFFFFE0000 ),
    UINT64_C( 0x000FFFFFFFFF0000 ),
    UINT64_C( 0x000FFFFFFFFF8000 ),
    UINT64_C( 0x000FFFFFFFFFC000 ),
    UINT64_C( 0x000FFFFFFFFFE000 ),
    UINT64_C( 0x000FFFFFFFFFF000 ),
    UINT64_C( 0x000FFFFFFFFFF800 ),
    UINT64_C( 0x000FFFFFFFFFFC00 ),
    UINT64_C( 0x000FFFFFFFFFFE00 ),
    UINT64_C( 0x000FFFFFFFFFFF00 ),
    UINT64_C( 0x000FFFFFFFFFFF80 ),
    UINT64_C( 0x000FFFFFFFFFFFC0 ),
    UINT64_C( 0x000FFFFFFFFFFFE0 ),
    UINT64_C( 0x000FFFFFFFFFFFF0 ),
    UINT64_C( 0x000FFFFFFFFFFFF8 ),
    UINT64_C( 0x000FFFFFFFFFFFFC ),
    UINT64_C( 0x000FFFFFFFFFFFFE ),
    UINT64_C( 0x000FFFFFFFFFFFFF ),
    UINT64_C( 0x000FFFFFFFFFFFFD ),
    UINT64_C( 0x000FFFFFFFFFFFFB ),
    UINT64_C( 0x000FFFFFFFFFFFF7 ),
    UINT64_C( 0x000FFFFFFFFFFFEF ),
    UINT64_C( 0x000FFFFFFFFFFFDF ),
    UINT64_C( 0x000FFFFFFFFFFFBF ),
    UINT64_C( 0x000FFFFFFFFFFF7F ),
    UINT64_C( 0x000FFFFFFFFFFEFF ),
    UINT64_C( 0x000FFFFFFFFFFDFF ),
    UINT64_C( 0x000FFFFFFFFFFBFF ),
    UINT64_C( 0x000FFFFFFFFFF7FF ),
    UINT64_C( 0x000FFFFFFFFFEFFF ),
    UINT64_C( 0x000FFFFFFFFFDFFF ),
    UINT64_C( 0x000FFFFFFFFFBFFF ),
    UINT64_C( 0x000FFFFFFFFF7FFF ),
    UINT64_C( 0x000FFFFFFFFEFFFF ),
    UINT64_C( 0x000FFFFFFFFDFFFF ),
    UINT64_C( 0x000FFFFFFFFBFFFF ),
    UINT64_C( 0x000FFFFFFFF7FFFF ),
    UINT64_C( 0x000FFFFFFFEFFFFF ),
    UINT64_C( 0x000FFFFFFFDFFFFF ),
    UINT64_C( 0x000FFFFFFFBFFFFF ),
    UINT64_C( 0x000FFFFFFF7FFFFF ),
    UINT64_C( 0x000FFFFFFEFFFFFF ),
    UINT64_C( 0x000FFFFFFDFFFFFF ),
    UINT64_C( 0x000FFFFFFBFFFFFF ),
    UINT64_C( 0x000FFFFFF7FFFFFF ),
    UINT64_C( 0x000FFFFFEFFFFFFF ),
    UINT64_C( 0x000FFFFFDFFFFFFF ),
    UINT64_C( 0x000FFFFFBFFFFFFF ),
    UINT64_C( 0x000FFFFF7FFFFFFF ),
    UINT64_C( 0x000FFFFEFFFFFFFF ),
    UINT64_C( 0x000FFFFDFFFFFFFF ),
    UINT64_C( 0x000FFFFBFFFFFFFF ),
    UINT64_C( 0x000FFFF7FFFFFFFF ),
    UINT64_C( 0x000FFFEFFFFFFFFF ),
    UINT64_C( 0x000FFFDFFFFFFFFF ),
    UINT64_C( 0x000FFFBFFFFFFFFF ),
    UINT64_C( 0x000FFF7FFFFFFFFF ),
    UINT64_C( 0x000FFEFFFFFFFFFF ),
    UINT64_C( 0x000FFDFFFFFFFFFF ),
    UINT64_C( 0x000FFBFFFFFFFFFF ),
    UINT64_C( 0x000FF7FFFFFFFFFF ),
    UINT64_C( 0x000FEFFFFFFFFFFF ),
    UINT64_C( 0x000FDFFFFFFFFFFF ),
    UINT64_C( 0x000FBFFFFFFFFFFF ),
    UINT64_C( 0x000F7FFFFFFFFFFF ),
    UINT64_C( 0x000EFFFFFFFFFFFF ),
    UINT64_C( 0x000DFFFFFFFFFFFF ),
    UINT64_C( 0x000BFFFFFFFFFFFF ),
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

static const uint_fast64_t f64NumQInP1 = f64NumQIn * f64NumP1;
static const uint_fast64_t f64NumQOutP1 = f64NumQOut * f64NumP1;

static float64_t f64NextQInP1( struct sequence *sequencePtr )
{
    int expNum, sigNum;
    union ui64_f64 uZ;

    expNum = sequencePtr->expNum;
    sigNum = sequencePtr->term1Num;
    uZ.ui = f64QIn[expNum] | f64P1[sigNum];
    ++sigNum;
    if ( f64NumP1 <= sigNum ) {
        sigNum = 0;
        ++expNum;
        if ( f64NumQIn <= expNum ) {
            expNum = 0;
            sequencePtr->done = true;
        }
        sequencePtr->expNum = expNum;
    }
    sequencePtr->term1Num = sigNum;
    return uZ.f;

}

static float64_t f64NextQOutP1( struct sequence *sequencePtr )
{
    int expNum, sigNum;
    union ui64_f64 uZ;

    expNum = sequencePtr->expNum;
    sigNum = sequencePtr->term1Num;
    uZ.ui = f64QOut[expNum] | f64P1[sigNum];
    ++sigNum;
    if ( f64NumP1 <= sigNum ) {
        sigNum = 0;
        ++expNum;
        if ( f64NumQOut <= expNum ) {
            expNum = 0;
            sequencePtr->done = true;
        }
        sequencePtr->expNum = expNum;
    }
    sequencePtr->term1Num = sigNum;
    return uZ.f;

}

static const uint_fast64_t f64NumQInP2 = f64NumQIn * f64NumP2;
static const uint_fast64_t f64NumQOutP2 = f64NumQOut * f64NumP2;

static float64_t f64NextQInP2( struct sequence *sequencePtr )
{
    int expNum, sigNum;
    union ui64_f64 uZ;

    expNum = sequencePtr->expNum;
    sigNum = sequencePtr->term1Num;
    uZ.ui = f64QIn[expNum] | f64P2[sigNum];
    ++sigNum;
    if ( f64NumP2 <= sigNum ) {
        sigNum = 0;
        ++expNum;
        if ( f64NumQIn <= expNum ) {
            expNum = 0;
            sequencePtr->done = true;
        }
        sequencePtr->expNum = expNum;
    }
    sequencePtr->term1Num = sigNum;
    return uZ.f;

}

static float64_t f64NextQOutP2( struct sequence *sequencePtr )
{
    int expNum, sigNum;
    union ui64_f64 uZ;

    expNum = sequencePtr->expNum;
    sigNum = sequencePtr->term1Num;
    uZ.ui = f64QOut[expNum] | f64P2[sigNum];
    ++sigNum;
    if ( f64NumP2 <= sigNum ) {
        sigNum = 0;
        ++expNum;
        if ( f64NumQOut <= expNum ) {
            expNum = 0;
            sequencePtr->done = true;
        }
        sequencePtr->expNum = expNum;
    }
    sequencePtr->term1Num = sigNum;
    return uZ.f;

}

static float64_t f64RandomQOutP3( void )
{
    union ui64_f64 uZ;

    uZ.ui =
          f64QOut[randomN_ui8( f64NumQOut )]
        | ((f64P2[randomN_ui8( f64NumP2 )] + f64P2[randomN_ui8( f64NumP2 )])
               & UINT64_C( 0x000FFFFFFFFFFFFF ));
    return uZ.f;

}

static float64_t f64RandomQOutPInf( void )
{
    union ui64_f64 uZ;

    uZ.ui =
        f64QOut[randomN_ui8( f64NumQOut )]
            | (random_ui64() & UINT64_C( 0x000FFFFFFFFFFFFF ));
    return uZ.f;

}

enum { f64NumQInfWeightMasks = 10 };
static const uint64_t f64QInfWeightMasks[f64NumQInfWeightMasks] = {
    UINT64_C( 0xFFF0000000000000 ),
    UINT64_C( 0xFFF0000000000000 ),
    UINT64_C( 0xBFF0000000000000 ),
    UINT64_C( 0x9FF0000000000000 ),
    UINT64_C( 0x8FF0000000000000 ),
    UINT64_C( 0x87F0000000000000 ),
    UINT64_C( 0x83F0000000000000 ),
    UINT64_C( 0x81F0000000000000 ),
    UINT64_C( 0x80F0000000000000 ),
    UINT64_C( 0x8070000000000000 )
};
static const uint64_t f64QInfWeightOffsets[f64NumQInfWeightMasks] = {
    UINT64_C( 0x0000000000000000 ),
    UINT64_C( 0x0000000000000000 ),
    UINT64_C( 0x2000000000000000 ),
    UINT64_C( 0x3000000000000000 ),
    UINT64_C( 0x3800000000000000 ),
    UINT64_C( 0x3C00000000000000 ),
    UINT64_C( 0x3E00000000000000 ),
    UINT64_C( 0x3F00000000000000 ),
    UINT64_C( 0x3F80000000000000 ),
    UINT64_C( 0x3FC0000000000000 )
};

static float64_t f64RandomQInfP3( void )
{
    int weightMaskNum;
    union ui64_f64 uZ;

    weightMaskNum = randomN_ui8( f64NumQInfWeightMasks );
    uZ.ui =
          (((uint_fast64_t) random_ui16()<<48
                & f64QInfWeightMasks[weightMaskNum])
               + f64QInfWeightOffsets[weightMaskNum])
        | ((f64P2[randomN_ui8( f64NumP2 )] + f64P2[randomN_ui8( f64NumP2 )])
               & UINT64_C( 0x000FFFFFFFFFFFFF ));
    return uZ.f;

}

static float64_t f64RandomQInfPInf( void )
{
    int weightMaskNum;
    union ui64_f64 uZ;

    weightMaskNum = randomN_ui8( f64NumQInfWeightMasks );
    uZ.ui =
        (random_ui64()
             & (f64QInfWeightMasks[weightMaskNum]
                    | UINT64_C( 0x000FFFFFFFFFFFFF )))
            + f64QInfWeightOffsets[weightMaskNum];
    return uZ.f;

}

static float64_t f64Random( void )
{

    switch ( random_ui8() & 7 ) {
     case 0:
     case 1:
     case 2:
        return f64RandomQOutP3();
     case 3:
        return f64RandomQOutPInf();
     case 4:
     case 5:
     case 6:
        return f64RandomQInfP3();
     case 7:
        return f64RandomQInfPInf();
    }

}

static struct sequence sequenceA, sequenceB, sequenceC;
static float64_t currentA, currentB, currentC;
static int subcase;

float64_t genCases_f64_a, genCases_f64_b, genCases_f64_c;

void genCases_f64_a_init( void )
{

    sequenceA.expNum = 0;
    sequenceA.term1Num = 0;
    sequenceA.term2Num = 0;
    sequenceA.done = false;
    subcase = 0;
    genCases_total =
        (genCases_level == 1) ? 3 * f64NumQOutP1 : 2 * f64NumQOutP2;
    genCases_done = false;

}

void genCases_f64_a_next( void )
{

    if ( genCases_level == 1 ) {
        switch ( subcase ) {
         case 0:
         case 1:
            genCases_f64_a = f64Random();
            break;
         case 2:
            genCases_f64_a = f64NextQOutP1( &sequenceA );
            genCases_done = sequenceA.done;
            subcase = -1;
            break;
        }
     } else {
        switch ( subcase ) {
         case 0:
            genCases_f64_a = f64Random();
            break;
         case 1:
            genCases_f64_a = f64NextQOutP2( &sequenceA );
            genCases_done = sequenceA.done;
            subcase = -1;
            break;
        }
    }
    ++subcase;

}

void genCases_f64_ab_init( void )
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
        genCases_total = 6 * f64NumQInP1 * f64NumQInP1;
        currentA = f64NextQInP1( &sequenceA );
    } else {
        genCases_total = 2 * f64NumQInP2 * f64NumQInP2;
        currentA = f64NextQInP2( &sequenceA );
    }
    genCases_done = false;

}

void genCases_f64_ab_next( void )
{

    if ( genCases_level == 1 ) {
        switch ( subcase ) {
         case 0:
            if ( sequenceB.done ) {
                sequenceB.done = false;
                currentA = f64NextQInP1( &sequenceA );
            }
            currentB = f64NextQInP1( &sequenceB );
         case 2:
         case 4:
            genCases_f64_a = f64Random();
            genCases_f64_b = f64Random();
            break;
         case 1:
            genCases_f64_a = currentA;
            genCases_f64_b = f64Random();
            break;
         case 3:
            genCases_f64_a = f64Random();
            genCases_f64_b = currentB;
            break;
         case 5:
            genCases_f64_a = currentA;
            genCases_f64_b = currentB;
            genCases_done = sequenceA.done & sequenceB.done;
            subcase = -1;
            break;
        }
    } else {
        switch ( subcase ) {
         case 0:
            genCases_f64_a = f64Random();
            genCases_f64_b = f64Random();
            break;
         case 1:
            if ( sequenceB.done ) {
                sequenceB.done = false;
                currentA = f64NextQInP2( &sequenceA );
            }
            genCases_f64_a = currentA;
            genCases_f64_b = f64NextQInP2( &sequenceB );
            genCases_done = sequenceA.done & sequenceB.done;
            subcase = -1;
            break;
        }
    }
    ++subcase;

}

void genCases_f64_abc_init( void )
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
        genCases_total = 9 * f64NumQInP1 * f64NumQInP1 * f64NumQInP1;
        currentA = f64NextQInP1( &sequenceA );
        currentB = f64NextQInP1( &sequenceB );
    } else {
        genCases_total = 2 * f64NumQInP2 * f64NumQInP2 * f64NumQInP2;
        currentA = f64NextQInP2( &sequenceA );
        currentB = f64NextQInP2( &sequenceB );
    }
    genCases_done = false;

}

void genCases_f64_abc_next( void )
{

    if ( genCases_level == 1 ) {
        switch ( subcase ) {
         case 0:
            if ( sequenceC.done ) {
                sequenceC.done = false;
                if ( sequenceB.done ) {
                    sequenceB.done = false;
                    currentA = f64NextQInP1( &sequenceA );
                }
                currentB = f64NextQInP1( &sequenceB );
            }
            currentC = f64NextQInP1( &sequenceC );
            genCases_f64_a = f64Random();
            genCases_f64_b = f64Random();
            genCases_f64_c = currentC;
            break;
         case 1:
            genCases_f64_a = currentA;
            genCases_f64_b = currentB;
            genCases_f64_c = f64Random();
            break;
         case 2:
            genCases_f64_a = f64Random();
            genCases_f64_b = f64Random();
            genCases_f64_c = f64Random();
            break;
         case 3:
            genCases_f64_a = f64Random();
            genCases_f64_b = currentB;
            genCases_f64_c = currentC;
            break;
         case 4:
            genCases_f64_a = currentA;
            genCases_f64_b = f64Random();
            genCases_f64_c = f64Random();
            break;
         case 5:
            genCases_f64_a = f64Random();
            genCases_f64_b = currentB;
            genCases_f64_c = f64Random();
            break;
         case 6:
            genCases_f64_a = currentA;
            genCases_f64_b = f64Random();
            genCases_f64_c = currentC;
            break;
         case 7:
            genCases_f64_a = f64Random();
            genCases_f64_b = f64Random();
            genCases_f64_c = f64Random();
            break;
         case 8:
            genCases_f64_a = currentA;
            genCases_f64_b = currentB;
            genCases_f64_c = currentC;
            genCases_done = sequenceA.done & sequenceB.done & sequenceC.done;
            subcase = -1;
            break;
        }
    } else {
        switch ( subcase ) {
         case 0:
            genCases_f64_a = f64Random();
            genCases_f64_b = f64Random();
            genCases_f64_c = f64Random();
            break;
         case 1:
            if ( sequenceC.done ) {
                sequenceC.done = false;
                if ( sequenceB.done ) {
                    sequenceB.done = false;
                    currentA = f64NextQInP2( &sequenceA );
                }
                currentB = f64NextQInP2( &sequenceB );
            }
            genCases_f64_a = currentA;
            genCases_f64_b = currentB;
            genCases_f64_c = f64NextQInP2( &sequenceC );
            genCases_done = sequenceA.done & sequenceB.done & sequenceC.done;
            subcase = -1;
            break;
        }
    }
    ++subcase;

}

#endif

