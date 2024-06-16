
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

struct sequence {
    int expNum, term1Num, term2Num;
    bool done;
};

union ui32_f32 { uint32_t ui; float32_t f; };

enum {
    f32NumQIn  = 22,
    f32NumQOut = 50,
    f32NumP1   =  4,
    f32NumP2   = 88
};
static const uint32_t f32QIn[f32NumQIn] = {
    0x00000000,    /* positive, subnormal       */
    0x00800000,    /* positive, -126            */
    0x33800000,    /* positive,  -24            */
    0x3E800000,    /* positive,   -2            */
    0x3F000000,    /* positive,   -1            */
    0x3F800000,    /* positive,    0            */
    0x40000000,    /* positive,    1            */
    0x40800000,    /* positive,    2            */
    0x4B800000,    /* positive,   24            */
    0x7F000000,    /* positive,  127            */
    0x7F800000,    /* positive, infinity or NaN */
    0x80000000,    /* negative, subnormal       */
    0x80800000,    /* negative, -126            */
    0xB3800000,    /* negative,  -24            */
    0xBE800000,    /* negative,   -2            */
    0xBF000000,    /* negative,   -1            */
    0xBF800000,    /* negative,    0            */
    0xC0000000,    /* negative,    1            */
    0xC0800000,    /* negative,    2            */
    0xCB800000,    /* negative,   24            */
    0xFE800000,    /* negative,  126            */
    0xFF800000     /* negative, infinity or NaN */
};
static const uint32_t f32QOut[f32NumQOut] = {
    0x00000000,    /* positive, subnormal       */
    0x00800000,    /* positive, -126            */
    0x01000000,    /* positive, -125            */
    0x33800000,    /* positive,  -24            */
    0x3D800000,    /* positive,   -4            */
    0x3E000000,    /* positive,   -3            */
    0x3E800000,    /* positive,   -2            */
    0x3F000000,    /* positive,   -1            */
    0x3F800000,    /* positive,    0            */
    0x40000000,    /* positive,    1            */
    0x40800000,    /* positive,    2            */
    0x41000000,    /* positive,    3            */
    0x41800000,    /* positive,    4            */
    0x4B800000,    /* positive,   24            */
    0x4E000000,    /* positive,   29            */
    0x4E800000,    /* positive,   30            */
    0x4F000000,    /* positive,   31            */
    0x4F800000,    /* positive,   32            */
    0x5E000000,    /* positive,   61            */
    0x5E800000,    /* positive,   62            */
    0x5F000000,    /* positive,   63            */
    0x5F800000,    /* positive,   64            */
    0x7E800000,    /* positive,  126            */
    0x7F000000,    /* positive,  127            */
    0x7F800000,    /* positive, infinity or NaN */
    0x80000000,    /* negative, subnormal       */
    0x80800000,    /* negative, -126            */
    0x81000000,    /* negative, -125            */
    0xB3800000,    /* negative,  -24            */
    0xBD800000,    /* negative,   -4            */
    0xBE000000,    /* negative,   -3            */
    0xBE800000,    /* negative,   -2            */
    0xBF000000,    /* negative,   -1            */
    0xBF800000,    /* negative,    0            */
    0xC0000000,    /* negative,    1            */
    0xC0800000,    /* negative,    2            */
    0xC1000000,    /* negative,    3            */
    0xC1800000,    /* negative,    4            */
    0xCB800000,    /* negative,   24            */
    0xCE000000,    /* negative,   29            */
    0xCE800000,    /* negative,   30            */
    0xCF000000,    /* negative,   31            */
    0xCF800000,    /* negative,   32            */
    0xDE000000,    /* negative,   61            */
    0xDE800000,    /* negative,   62            */
    0xDF000000,    /* negative,   63            */
    0xDF800000,    /* negative,   64            */
    0xFE800000,    /* negative,  126            */
    0xFF000000,    /* negative,  127            */
    0xFF800000     /* negative, infinity or NaN */
};
static const uint32_t f32P1[f32NumP1] = {
    0x00000000,
    0x00000001,
    0x007FFFFF,
    0x007FFFFE
};
static const uint32_t f32P2[f32NumP2] = {
    0x00000000,
    0x00000001,
    0x00000002,
    0x00000004,
    0x00000008,
    0x00000010,
    0x00000020,
    0x00000040,
    0x00000080,
    0x00000100,
    0x00000200,
    0x00000400,
    0x00000800,
    0x00001000,
    0x00002000,
    0x00004000,
    0x00008000,
    0x00010000,
    0x00020000,
    0x00040000,
    0x00080000,
    0x00100000,
    0x00200000,
    0x00400000,
    0x00600000,
    0x00700000,
    0x00780000,
    0x007C0000,
    0x007E0000,
    0x007F0000,
    0x007F8000,
    0x007FC000,
    0x007FE000,
    0x007FF000,
    0x007FF800,
    0x007FFC00,
    0x007FFE00,
    0x007FFF00,
    0x007FFF80,
    0x007FFFC0,
    0x007FFFE0,
    0x007FFFF0,
    0x007FFFF8,
    0x007FFFFC,
    0x007FFFFE,
    0x007FFFFF,
    0x007FFFFD,
    0x007FFFFB,
    0x007FFFF7,
    0x007FFFEF,
    0x007FFFDF,
    0x007FFFBF,
    0x007FFF7F,
    0x007FFEFF,
    0x007FFDFF,
    0x007FFBFF,
    0x007FF7FF,
    0x007FEFFF,
    0x007FDFFF,
    0x007FBFFF,
    0x007F7FFF,
    0x007EFFFF,
    0x007DFFFF,
    0x007BFFFF,
    0x0077FFFF,
    0x006FFFFF,
    0x005FFFFF,
    0x003FFFFF,
    0x001FFFFF,
    0x000FFFFF,
    0x0007FFFF,
    0x0003FFFF,
    0x0001FFFF,
    0x0000FFFF,
    0x00007FFF,
    0x00003FFF,
    0x00001FFF,
    0x00000FFF,
    0x000007FF,
    0x000003FF,
    0x000001FF,
    0x000000FF,
    0x0000007F,
    0x0000003F,
    0x0000001F,
    0x0000000F,
    0x00000007,
    0x00000003
};

static const uint_fast64_t f32NumQInP1 = f32NumQIn * f32NumP1;
static const uint_fast64_t f32NumQOutP1 = f32NumQOut * f32NumP1;

static float32_t f32NextQInP1( struct sequence *sequencePtr )
{
    int expNum, sigNum;
    union ui32_f32 uZ;

    expNum = sequencePtr->expNum;
    sigNum = sequencePtr->term1Num;
    uZ.ui = f32QIn[expNum] | f32P1[sigNum];
    ++sigNum;
    if ( f32NumP1 <= sigNum ) {
        sigNum = 0;
        ++expNum;
        if ( f32NumQIn <= expNum ) {
            expNum = 0;
            sequencePtr->done = true;
        }
        sequencePtr->expNum = expNum;
    }
    sequencePtr->term1Num = sigNum;
    return uZ.f;

}

static float32_t f32NextQOutP1( struct sequence *sequencePtr )
{
    int expNum, sigNum;
    union ui32_f32 uZ;

    expNum = sequencePtr->expNum;
    sigNum = sequencePtr->term1Num;
    uZ.ui = f32QOut[expNum] | f32P1[sigNum];
    ++sigNum;
    if ( f32NumP1 <= sigNum ) {
        sigNum = 0;
        ++expNum;
        if ( f32NumQOut <= expNum ) {
            expNum = 0;
            sequencePtr->done = true;
        }
        sequencePtr->expNum = expNum;
    }
    sequencePtr->term1Num = sigNum;
    return uZ.f;

}

static const uint_fast64_t f32NumQInP2 = f32NumQIn * f32NumP2;
static const uint_fast64_t f32NumQOutP2 = f32NumQOut * f32NumP2;

static float32_t f32NextQInP2( struct sequence *sequencePtr )
{
    int expNum, sigNum;
    union ui32_f32 uZ;

    expNum = sequencePtr->expNum;
    sigNum = sequencePtr->term1Num;
    uZ.ui = f32QIn[expNum] | f32P2[sigNum];
    ++sigNum;
    if ( f32NumP2 <= sigNum ) {
        sigNum = 0;
        ++expNum;
        if ( f32NumQIn <= expNum ) {
            expNum = 0;
            sequencePtr->done = true;
        }
        sequencePtr->expNum = expNum;
    }
    sequencePtr->term1Num = sigNum;
    return uZ.f;

}

static float32_t f32NextQOutP2( struct sequence *sequencePtr )
{
    int expNum, sigNum;
    union ui32_f32 uZ;

    expNum = sequencePtr->expNum;
    sigNum = sequencePtr->term1Num;
    uZ.ui = f32QOut[expNum] | f32P2[sigNum];
    ++sigNum;
    if ( f32NumP2 <= sigNum ) {
        sigNum = 0;
        ++expNum;
        if ( f32NumQOut <= expNum ) {
            expNum = 0;
            sequencePtr->done = true;
        }
        sequencePtr->expNum = expNum;
    }
    sequencePtr->term1Num = sigNum;
    return uZ.f;

}

static float32_t f32RandomQOutP3( void )
{
    union ui32_f32 uZ;

    uZ.ui =
          f32QOut[randomN_ui8( f32NumQOut )]
        | ((f32P2[randomN_ui8( f32NumP2 )] + f32P2[randomN_ui8( f32NumP2 )])
               & 0x007FFFFF);
    return uZ.f;

}

static float32_t f32RandomQOutPInf( void )
{
    union ui32_f32 uZ;

    uZ.ui = f32QOut[randomN_ui8( f32NumQOut )] | (random_ui32() & 0x007FFFFF);
    return uZ.f;

}

enum { f32NumQInfWeightMasks = 7 };
static const uint32_t f32QInfWeightMasks[f32NumQInfWeightMasks] = {
    0xFF800000,
    0xFF800000,
    0xBF800000,
    0x9F800000,
    0x8F800000,
    0x87800000,
    0x83800000
};
static const uint32_t f32QInfWeightOffsets[f32NumQInfWeightMasks] = {
    0x00000000,
    0x00000000,
    0x20000000,
    0x30000000,
    0x38000000,
    0x3C000000,
    0x3E000000
};

static float32_t f32RandomQInfP3( void )
{
    int weightMaskNum;
    union ui32_f32 uZ;

    weightMaskNum = randomN_ui8( f32NumQInfWeightMasks );
    uZ.ui =
          (((uint_fast32_t) random_ui16()<<16
                & f32QInfWeightMasks[weightMaskNum])
               + f32QInfWeightOffsets[weightMaskNum])
        | ((f32P2[randomN_ui8( f32NumP2 )] + f32P2[randomN_ui8( f32NumP2 )])
               & 0x007FFFFF);
    return uZ.f;

}

static float32_t f32RandomQInfPInf( void )
{
    int weightMaskNum;
    union ui32_f32 uZ;

    weightMaskNum = randomN_ui8( f32NumQInfWeightMasks );
    uZ.ui =
        (random_ui32() & (f32QInfWeightMasks[weightMaskNum] | 0x007FFFFF))
            + f32QInfWeightOffsets[weightMaskNum];
    return uZ.f;

}

static float32_t f32Random( void )
{

    switch ( random_ui8() & 7 ) {
     case 0:
     case 1:
     case 2:
        return f32RandomQOutP3();
     case 3:
        return f32RandomQOutPInf();
     case 4:
     case 5:
     case 6:
        return f32RandomQInfP3();
     case 7:
        return f32RandomQInfPInf();
    }

}

static struct sequence sequenceA, sequenceB, sequenceC;
static float32_t currentA, currentB, currentC;
static int subcase;

float32_t genCases_f32_a, genCases_f32_b, genCases_f32_c;

void genCases_f32_a_init( void )
{

    sequenceA.expNum = 0;
    sequenceA.term1Num = 0;
    sequenceA.term2Num = 0;
    sequenceA.done = false;
    subcase = 0;
    genCases_total =
        (genCases_level == 1) ? 3 * f32NumQOutP1 : 2 * f32NumQOutP2;
    genCases_done = false;

}

void genCases_f32_a_next( void )
{

    if ( genCases_level == 1 ) {
        switch ( subcase ) {
         case 0:
         case 1:
            genCases_f32_a = f32Random();
            break;
         case 2:
            genCases_f32_a = f32NextQOutP1( &sequenceA );
            genCases_done = sequenceA.done;
            subcase = -1;
            break;
        }
    } else {
        switch ( subcase ) {
         case 0:
            genCases_f32_a = f32Random();
            break;
         case 1:
            genCases_f32_a = f32NextQOutP2( &sequenceA );
            genCases_done = sequenceA.done;
            subcase = -1;
            break;
        }
    }
    ++subcase;

}

void genCases_f32_ab_init( void )
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
        genCases_total = 6 * f32NumQInP1 * f32NumQInP1;
        currentA = f32NextQInP1( &sequenceA );
    } else {
        genCases_total = 2 * f32NumQInP2 * f32NumQInP2;
        currentA = f32NextQInP2( &sequenceA );
    }
    genCases_done = false;

}

void genCases_f32_ab_next( void )
{

    if ( genCases_level == 1 ) {
        switch ( subcase ) {
         case 0:
            if ( sequenceB.done ) {
                sequenceB.done = false;
                currentA = f32NextQInP1( &sequenceA );
            }
            currentB = f32NextQInP1( &sequenceB );
         case 2:
         case 4:
            genCases_f32_a = f32Random();
            genCases_f32_b = f32Random();
            break;
         case 1:
            genCases_f32_a = currentA;
            genCases_f32_b = f32Random();
            break;
         case 3:
            genCases_f32_a = f32Random();
            genCases_f32_b = currentB;
            break;
         case 5:
            genCases_f32_a = currentA;
            genCases_f32_b = currentB;
            genCases_done = sequenceA.done & sequenceB.done;
            subcase = -1;
            break;
        }
    } else {
        switch ( subcase ) {
         case 0:
            genCases_f32_a = f32Random();
            genCases_f32_b = f32Random();
            break;
         case 1:
            if ( sequenceB.done ) {
                sequenceB.done = false;
                currentA = f32NextQInP2( &sequenceA );
            }
            genCases_f32_a = currentA;
            genCases_f32_b = f32NextQInP2( &sequenceB );
            genCases_done = sequenceA.done & sequenceB.done;
            subcase = -1;
            break;
        }
    }
    ++subcase;

}

void genCases_f32_abc_init( void )
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
        genCases_total = 9 * f32NumQInP1 * f32NumQInP1 * f32NumQInP1;
        currentA = f32NextQInP1( &sequenceA );
        currentB = f32NextQInP1( &sequenceB );
    } else {
        genCases_total = 2 * f32NumQInP2 * f32NumQInP2 * f32NumQInP2;
        currentA = f32NextQInP2( &sequenceA );
        currentB = f32NextQInP2( &sequenceB );
    }
    genCases_done = false;

}

void genCases_f32_abc_next( void )
{

    if ( genCases_level == 1 ) {
        switch ( subcase ) {
         case 0:
            if ( sequenceC.done ) {
                sequenceC.done = false;
                if ( sequenceB.done ) {
                    sequenceB.done = false;
                    currentA = f32NextQInP1( &sequenceA );
                }
                currentB = f32NextQInP1( &sequenceB );
            }
            currentC = f32NextQInP1( &sequenceC );
            genCases_f32_a = f32Random();
            genCases_f32_b = f32Random();
            genCases_f32_c = currentC;
            break;
         case 1:
            genCases_f32_a = currentA;
            genCases_f32_b = currentB;
            genCases_f32_c = f32Random();
            break;
         case 2:
            genCases_f32_a = f32Random();
            genCases_f32_b = f32Random();
            genCases_f32_c = f32Random();
            break;
         case 3:
            genCases_f32_a = f32Random();
            genCases_f32_b = currentB;
            genCases_f32_c = currentC;
            break;
         case 4:
            genCases_f32_a = currentA;
            genCases_f32_b = f32Random();
            genCases_f32_c = f32Random();
            break;
         case 5:
            genCases_f32_a = f32Random();
            genCases_f32_b = currentB;
            genCases_f32_c = f32Random();
            break;
         case 6:
            genCases_f32_a = currentA;
            genCases_f32_b = f32Random();
            genCases_f32_c = currentC;
            break;
         case 7:
            genCases_f32_a = f32Random();
            genCases_f32_b = f32Random();
            genCases_f32_c = f32Random();
            break;
         case 8:
            genCases_f32_a = currentA;
            genCases_f32_b = currentB;
            genCases_f32_c = currentC;
            genCases_done = sequenceA.done & sequenceB.done & sequenceC.done;
            subcase = -1;
            break;
        }
    } else {
        switch ( subcase ) {
         case 0:
            genCases_f32_a = f32Random();
            genCases_f32_b = f32Random();
            genCases_f32_c = f32Random();
            break;
         case 1:
            if ( sequenceC.done ) {
                sequenceC.done = false;
                if ( sequenceB.done ) {
                    sequenceB.done = false;
                    currentA = f32NextQInP2( &sequenceA );
                }
                currentB = f32NextQInP2( &sequenceB );
            }
            genCases_f32_a = currentA;
            genCases_f32_b = currentB;
            genCases_f32_c = f32NextQInP2( &sequenceC );
            genCases_done = sequenceA.done & sequenceB.done & sequenceC.done;
            subcase = -1;
            break;
        }
    }
    ++subcase;

}

