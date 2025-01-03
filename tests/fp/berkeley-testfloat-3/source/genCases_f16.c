
/*============================================================================

This C source file is part of TestFloat, Release 3e, a package of programs for
testing the correctness of floating-point arithmetic complying with the IEEE
Standard for Floating-Point, by John R. Hauser.

Copyright 2011, 2012, 2013, 2014, 2015 The Regents of the University of
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

#ifdef FLOAT16

struct sequence {
    int expNum, term1Num, term2Num;
    bool done;
};

union ui16_f16 { uint16_t ui; float16_t f; };

enum {
    f16NumQIn  = 22,
    f16NumQOut = 34,
    f16NumP1   =  4,
    f16NumP2   = 36
};
static const uint16_t f16QIn[f16NumQIn] = {
    0x0000,    /* positive, subnormal       */
    0x0400,    /* positive, -14             */
    0x1000,    /* positive, -11             */
    0x3400,    /* positive,  -2             */
    0x3800,    /* positive,  -1             */
    0x3C00,    /* positive,   0             */
    0x4000,    /* positive,   1             */
    0x4400,    /* positive,   2             */
    0x6800,    /* positive,  11             */
    0x7800,    /* positive,  15             */
    0x7C00,    /* positive, infinity or NaN */
    0x8000,    /* negative, subnormal       */
    0x8400,    /* negative, -14             */
    0x9000,    /* negative, -11             */
    0xB400,    /* negative,  -2             */
    0xB800,    /* negative,  -1             */
    0xBC00,    /* negative,   0             */
    0xC000,    /* negative,   1             */
    0xC400,    /* negative,   2             */
    0xE800,    /* negative,  11             */
    0xF800,    /* negative,  15             */
    0xFC00     /* negative, infinity or NaN */
};
static const uint16_t f16QOut[f16NumQOut] = {
    0x0000,    /* positive, subnormal       */
    0x0400,    /* positive, -14             */
    0x0800,    /* positive, -13             */
    0x1000,    /* positive, -11             */
    0x2C00,    /* positive,  -4             */
    0x3000,    /* positive,  -3             */
    0x3400,    /* positive,  -2             */
    0x3800,    /* positive,  -1             */
    0x3C00,    /* positive,   0             */
    0x4000,    /* positive,   1             */
    0x4400,    /* positive,   2             */
    0x4800,    /* positive,   3             */
    0x4C00,    /* positive,   4             */
    0x6800,    /* positive,  11             */
    0x7400,    /* positive,  14             */
    0x7800,    /* positive,  15             */
    0x7C00,    /* positive, infinity or NaN */
    0x8000,    /* negative, subnormal       */
    0x8400,    /* negative, -14             */
    0x8800,    /* negative, -13             */
    0x9000,    /* negative, -11             */
    0xAC00,    /* negative,  -4             */
    0xB000,    /* negative,  -3             */
    0xB400,    /* negative,  -2             */
    0xB800,    /* negative,  -1             */
    0xBC00,    /* negative,   0             */
    0xC000,    /* negative,   1             */
    0xC400,    /* negative,   2             */
    0xC800,    /* negative,   3             */
    0xCC00,    /* negative,   4             */
    0xE800,    /* negative,  11             */
    0xF400,    /* negative,  14             */
    0xF800,    /* negative,  15             */
    0xFC00     /* negative, infinity or NaN */
};
static const uint16_t f16P1[f16NumP1] = {
    0x0000,
    0x0001,
    0x03FF,
    0x03FE
};
static const uint16_t f16P2[f16NumP2] = {
    0x0000,
    0x0001,
    0x0002,
    0x0004,
    0x0008,
    0x0010,
    0x0020,
    0x0040,
    0x0080,
    0x0100,
    0x0200,
    0x0300,
    0x0380,
    0x03C0,
    0x03E0,
    0x03F0,
    0x03F8,
    0x03FC,
    0x03FE,
    0x03FF,
    0x03FD,
    0x03FB,
    0x03F7,
    0x03EF,
    0x03DF,
    0x03BF,
    0x037F,
    0x02FF,
    0x01FF,
    0x00FF,
    0x007F,
    0x003F,
    0x001F,
    0x000F,
    0x0007,
    0x0003
};

static const uint_fast64_t f16NumQInP1 = f16NumQIn * f16NumP1;
static const uint_fast64_t f16NumQOutP1 = f16NumQOut * f16NumP1;

static float16_t f16NextQInP1( struct sequence *sequencePtr )
{
    int expNum, sigNum;
    union ui16_f16 uZ;

    expNum = sequencePtr->expNum;
    sigNum = sequencePtr->term1Num;
    uZ.ui = f16QIn[expNum] | f16P1[sigNum];
    ++sigNum;
    if ( f16NumP1 <= sigNum ) {
        sigNum = 0;
        ++expNum;
        if ( f16NumQIn <= expNum ) {
            expNum = 0;
            sequencePtr->done = true;
        }
        sequencePtr->expNum = expNum;
    }
    sequencePtr->term1Num = sigNum;
    return uZ.f;

}

static float16_t f16NextQOutP1( struct sequence *sequencePtr )
{
    int expNum, sigNum;
    union ui16_f16 uZ;

    expNum = sequencePtr->expNum;
    sigNum = sequencePtr->term1Num;
    uZ.ui = f16QOut[expNum] | f16P1[sigNum];
    ++sigNum;
    if ( f16NumP1 <= sigNum ) {
        sigNum = 0;
        ++expNum;
        if ( f16NumQOut <= expNum ) {
            expNum = 0;
            sequencePtr->done = true;
        }
        sequencePtr->expNum = expNum;
    }
    sequencePtr->term1Num = sigNum;
    return uZ.f;

}

static const uint_fast64_t f16NumQInP2 = f16NumQIn * f16NumP2;
static const uint_fast64_t f16NumQOutP2 = f16NumQOut * f16NumP2;

static float16_t f16NextQInP2( struct sequence *sequencePtr )
{
    int expNum, sigNum;
    union ui16_f16 uZ;

    expNum = sequencePtr->expNum;
    sigNum = sequencePtr->term1Num;
    uZ.ui = f16QIn[expNum] | f16P2[sigNum];
    ++sigNum;
    if ( f16NumP2 <= sigNum ) {
        sigNum = 0;
        ++expNum;
        if ( f16NumQIn <= expNum ) {
            expNum = 0;
            sequencePtr->done = true;
        }
        sequencePtr->expNum = expNum;
    }
    sequencePtr->term1Num = sigNum;
    return uZ.f;

}

static float16_t f16NextQOutP2( struct sequence *sequencePtr )
{
    int expNum, sigNum;
    union ui16_f16 uZ;

    expNum = sequencePtr->expNum;
    sigNum = sequencePtr->term1Num;
    uZ.ui = f16QOut[expNum] | f16P2[sigNum];
    ++sigNum;
    if ( f16NumP2 <= sigNum ) {
        sigNum = 0;
        ++expNum;
        if ( f16NumQOut <= expNum ) {
            expNum = 0;
            sequencePtr->done = true;
        }
        sequencePtr->expNum = expNum;
    }
    sequencePtr->term1Num = sigNum;
    return uZ.f;

}

static float16_t f16RandomQOutP3( void )
{
    union ui16_f16 uZ;

    uZ.ui =
          f16QOut[randomN_ui8( f16NumQOut )]
        | ((f16P2[randomN_ui8( f16NumP2 )] + f16P2[randomN_ui8( f16NumP2 )])
               & 0x03FF);
    return uZ.f;

}

static float16_t f16RandomQOutPInf( void )
{
    union ui16_f16 uZ;

    uZ.ui = f16QOut[randomN_ui8( f16NumQOut )] | (random_ui16() & 0x03FF);
    return uZ.f;

}

enum { f16NumQInfWeightMasks = 4 };
static const uint16_t f16QInfWeightMasks[f16NumQInfWeightMasks] =
    { 0xFC00, 0xFC00, 0xBC00, 0x9C00 };
static const uint16_t f16QInfWeightOffsets[f16NumQInfWeightMasks] =
    { 0x0000, 0x0000, 0x2000, 0x3000 };

static float16_t f16RandomQInfP3( void )
{
    int weightMaskNum;
    union ui16_f16 uZ;

    weightMaskNum = randomN_ui8( f16NumQInfWeightMasks );
    uZ.ui =
          ((random_ui16() & f16QInfWeightMasks[weightMaskNum])
               + f16QInfWeightOffsets[weightMaskNum])
        | ((f16P2[randomN_ui8( f16NumP2 )] + f16P2[randomN_ui8( f16NumP2 )])
               & 0x03FF);
    return uZ.f;

}

static float16_t f16RandomQInfPInf( void )
{
    int weightMaskNum;
    union ui16_f16 uZ;

    weightMaskNum = randomN_ui8( f16NumQInfWeightMasks );
    uZ.ui =
        (random_ui16() & (f16QInfWeightMasks[weightMaskNum] | 0x03FF))
            + f16QInfWeightOffsets[weightMaskNum];
    return uZ.f;

}

static float16_t f16Random( void )
{

    switch ( random_ui8() & 7 ) {
     case 0:
     case 1:
     case 2:
        return f16RandomQOutP3();
     case 3:
        return f16RandomQOutPInf();
     case 4:
     case 5:
     case 6:
        return f16RandomQInfP3();
     case 7:
        return f16RandomQInfPInf();
    }

}

static struct sequence sequenceA, sequenceB, sequenceC;
static float16_t currentA, currentB, currentC;
static int subcase;

float16_t genCases_f16_a, genCases_f16_b, genCases_f16_c;

void genCases_f16_a_init( void )
{

    sequenceA.expNum = 0;
    sequenceA.term1Num = 0;
    sequenceA.term2Num = 0;
    sequenceA.done = false;
    subcase = 0;
    genCases_total =
        (genCases_level == 1) ? 3 * f16NumQOutP1 : 2 * f16NumQOutP2;
    genCases_done = false;

}

void genCases_f16_a_next( void )
{

    if ( genCases_level == 1 ) {
        switch ( subcase ) {
         case 0:
         case 1:
            genCases_f16_a = f16Random();
            break;
         case 2:
            genCases_f16_a = f16NextQOutP1( &sequenceA );
            genCases_done = sequenceA.done;
            subcase = -1;
            break;
        }
    } else {
        switch ( subcase ) {
         case 0:
            genCases_f16_a = f16Random();
            break;
         case 1:
            genCases_f16_a = f16NextQOutP2( &sequenceA );
            genCases_done = sequenceA.done;
            subcase = -1;
            break;
        }
    }
    ++subcase;

}

void genCases_f16_ab_init( void )
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
        genCases_total = 6 * f16NumQInP1 * f16NumQInP1;
        currentA = f16NextQInP1( &sequenceA );
    } else {
        genCases_total = 2 * f16NumQInP2 * f16NumQInP2;
        currentA = f16NextQInP2( &sequenceA );
    }
    genCases_done = false;

}

void genCases_f16_ab_next( void )
{

    if ( genCases_level == 1 ) {
        switch ( subcase ) {
         case 0:
            if ( sequenceB.done ) {
                sequenceB.done = false;
                currentA = f16NextQInP1( &sequenceA );
            }
            currentB = f16NextQInP1( &sequenceB );
         case 2:
         case 4:
            genCases_f16_a = f16Random();
            genCases_f16_b = f16Random();
            break;
         case 1:
            genCases_f16_a = currentA;
            genCases_f16_b = f16Random();
            break;
         case 3:
            genCases_f16_a = f16Random();
            genCases_f16_b = currentB;
            break;
         case 5:
            genCases_f16_a = currentA;
            genCases_f16_b = currentB;
            genCases_done = sequenceA.done & sequenceB.done;
            subcase = -1;
            break;
        }
    } else {
        switch ( subcase ) {
         case 0:
            genCases_f16_a = f16Random();
            genCases_f16_b = f16Random();
            break;
         case 1:
            if ( sequenceB.done ) {
                sequenceB.done = false;
                currentA = f16NextQInP2( &sequenceA );
            }
            genCases_f16_a = currentA;
            genCases_f16_b = f16NextQInP2( &sequenceB );
            genCases_done = sequenceA.done & sequenceB.done;
            subcase = -1;
            break;
        }
    }
    ++subcase;

}

void genCases_f16_abc_init( void )
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
        genCases_total = 9 * f16NumQInP1 * f16NumQInP1 * f16NumQInP1;
        currentA = f16NextQInP1( &sequenceA );
        currentB = f16NextQInP1( &sequenceB );
    } else {
        genCases_total = 2 * f16NumQInP2 * f16NumQInP2 * f16NumQInP2;
        currentA = f16NextQInP2( &sequenceA );
        currentB = f16NextQInP2( &sequenceB );
    }
    genCases_done = false;

}

void genCases_f16_abc_next( void )
{

    if ( genCases_level == 1 ) {
        switch ( subcase ) {
         case 0:
            if ( sequenceC.done ) {
                sequenceC.done = false;
                if ( sequenceB.done ) {
                    sequenceB.done = false;
                    currentA = f16NextQInP1( &sequenceA );
                }
                currentB = f16NextQInP1( &sequenceB );
            }
            currentC = f16NextQInP1( &sequenceC );
            genCases_f16_a = f16Random();
            genCases_f16_b = f16Random();
            genCases_f16_c = currentC;
            break;
         case 1:
            genCases_f16_a = currentA;
            genCases_f16_b = currentB;
            genCases_f16_c = f16Random();
            break;
         case 2:
            genCases_f16_a = f16Random();
            genCases_f16_b = f16Random();
            genCases_f16_c = f16Random();
            break;
         case 3:
            genCases_f16_a = f16Random();
            genCases_f16_b = currentB;
            genCases_f16_c = currentC;
            break;
         case 4:
            genCases_f16_a = currentA;
            genCases_f16_b = f16Random();
            genCases_f16_c = f16Random();
            break;
         case 5:
            genCases_f16_a = f16Random();
            genCases_f16_b = currentB;
            genCases_f16_c = f16Random();
            break;
         case 6:
            genCases_f16_a = currentA;
            genCases_f16_b = f16Random();
            genCases_f16_c = currentC;
            break;
         case 7:
            genCases_f16_a = f16Random();
            genCases_f16_b = f16Random();
            genCases_f16_c = f16Random();
            break;
         case 8:
            genCases_f16_a = currentA;
            genCases_f16_b = currentB;
            genCases_f16_c = currentC;
            genCases_done = sequenceA.done & sequenceB.done & sequenceC.done;
            subcase = -1;
            break;
        }
    } else {
        switch ( subcase ) {
         case 0:
            genCases_f16_a = f16Random();
            genCases_f16_b = f16Random();
            genCases_f16_c = f16Random();
            break;
         case 1:
            if ( sequenceC.done ) {
                sequenceC.done = false;
                if ( sequenceB.done ) {
                    sequenceB.done = false;
                    currentA = f16NextQInP2( &sequenceA );
                }
                currentB = f16NextQInP2( &sequenceB );
            }
            genCases_f16_a = currentA;
            genCases_f16_b = currentB;
            genCases_f16_c = f16NextQInP2( &sequenceC );
            genCases_done = sequenceA.done & sequenceB.done & sequenceC.done;
            subcase = -1;
            break;
        }
    }
    ++subcase;

}

#endif

