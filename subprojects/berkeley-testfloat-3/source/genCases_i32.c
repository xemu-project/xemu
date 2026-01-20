
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
#include "genCases.h"

struct sequence {
    int term1Num, term2Num;
    bool done;
};

union ui32_i32 { uint32_t ui; int32_t i; };

enum { i32NumP1 = 124 };
static const uint32_t i32P1[i32NumP1] = {
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
    0x00800000,
    0x01000000,
    0x02000000,
    0x04000000,
    0x08000000,
    0x10000000,
    0x20000000,
    0x40000000,
    0x80000000,
    0xC0000000,
    0xE0000000,
    0xF0000000,
    0xF8000000,
    0xFC000000,
    0xFE000000,
    0xFF000000,
    0xFF800000,
    0xFFC00000,
    0xFFE00000,
    0xFFF00000,
    0xFFF80000,
    0xFFFC0000,
    0xFFFE0000,
    0xFFFF0000,
    0xFFFF8000,
    0xFFFFC000,
    0xFFFFE000,
    0xFFFFF000,
    0xFFFFF800,
    0xFFFFFC00,
    0xFFFFFE00,
    0xFFFFFF00,
    0xFFFFFF80,
    0xFFFFFFC0,
    0xFFFFFFE0,
    0xFFFFFFF0,
    0xFFFFFFF8,
    0xFFFFFFFC,
    0xFFFFFFFE,
    0xFFFFFFFF,
    0xFFFFFFFD,
    0xFFFFFFFB,
    0xFFFFFFF7,
    0xFFFFFFEF,
    0xFFFFFFDF,
    0xFFFFFFBF,
    0xFFFFFF7F,
    0xFFFFFEFF,
    0xFFFFFDFF,
    0xFFFFFBFF,
    0xFFFFF7FF,
    0xFFFFEFFF,
    0xFFFFDFFF,
    0xFFFFBFFF,
    0xFFFF7FFF,
    0xFFFEFFFF,
    0xFFFDFFFF,
    0xFFFBFFFF,
    0xFFF7FFFF,
    0xFFEFFFFF,
    0xFFDFFFFF,
    0xFFBFFFFF,
    0xFF7FFFFF,
    0xFEFFFFFF,
    0xFDFFFFFF,
    0xFBFFFFFF,
    0xF7FFFFFF,
    0xEFFFFFFF,
    0xDFFFFFFF,
    0xBFFFFFFF,
    0x7FFFFFFF,
    0x3FFFFFFF,
    0x1FFFFFFF,
    0x0FFFFFFF,
    0x07FFFFFF,
    0x03FFFFFF,
    0x01FFFFFF,
    0x00FFFFFF,
    0x007FFFFF,
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

static int32_t i32NextP1( struct sequence *sequencePtr )
{
    int termNum;
    union ui32_i32 uZ;

    termNum = sequencePtr->term1Num;
    uZ.ui = i32P1[termNum];
    ++termNum;
    if ( i32NumP1 <= termNum ) {
        termNum = 0;
        sequencePtr->done = true;
    }
    sequencePtr->term1Num = termNum;
    return uZ.i;

}

static const int_fast32_t i32NumP2 = (i32NumP1 * i32NumP1 + i32NumP1) / 2;

static int32_t i32NextP2( struct sequence *sequencePtr )
{
    int term1Num, term2Num;
    union ui32_i32 uZ;

    term2Num = sequencePtr->term2Num;
    term1Num = sequencePtr->term1Num;
    uZ.ui = i32P1[term1Num] + i32P1[term2Num];
    ++term2Num;
    if ( i32NumP1 <= term2Num ) {
        ++term1Num;
        if ( i32NumP1 <= term1Num ) {
            term1Num = 0;
            sequencePtr->done = true;
        }
        term2Num = term1Num;
        sequencePtr->term1Num = term1Num;
    }
    sequencePtr->term2Num = term2Num;
    return uZ.i;

}

static int32_t i32RandomP3( void )
{
    union ui32_i32 uZ;

    uZ.ui =
        i32P1[randomN_ui8( i32NumP1 )] + i32P1[randomN_ui8( i32NumP1 )]
            + i32P1[randomN_ui8( i32NumP1 )];
    return uZ.i;

}

enum { i32NumPInfWeightMasks = 29 };
static const uint32_t i32PInfWeightMasks[i32NumPInfWeightMasks] = {
    0xFFFFFFFF,
    0x7FFFFFFF,
    0x3FFFFFFF,
    0x1FFFFFFF,
    0x0FFFFFFF,
    0x07FFFFFF,
    0x03FFFFFF,
    0x01FFFFFF,
    0x00FFFFFF,
    0x007FFFFF,
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
    0x0000000F
};
static const uint32_t i32PInfWeightOffsets[i32NumPInfWeightMasks] = {
    0x00000000,
    0xC0000000,
    0xE0000000,
    0xF0000000,
    0xF8000000,
    0xFC000000,
    0xFE000000,
    0xFF000000,
    0xFF800000,
    0xFFC00000,
    0xFFE00000,
    0xFFF00000,
    0xFFF80000,
    0xFFFC0000,
    0xFFFE0000,
    0xFFFF0000,
    0xFFFF8000,
    0xFFFFC000,
    0xFFFFE000,
    0xFFFFF000,
    0xFFFFF800,
    0xFFFFFC00,
    0xFFFFFE00,
    0xFFFFFF00,
    0xFFFFFF80,
    0xFFFFFFC0,
    0xFFFFFFE0,
    0xFFFFFFF0,
    0xFFFFFFF8
};

static int32_t i32RandomPInf( void )
{
    int weightMaskNum;
    union ui32_i32 uZ;

    weightMaskNum = randomN_ui8( i32NumPInfWeightMasks );
    uZ.ui =
        (random_ui32() & i32PInfWeightMasks[weightMaskNum])
            + i32PInfWeightOffsets[weightMaskNum];
    return uZ.i;

}

static struct sequence sequenceA;
static int subcase;

int32_t genCases_i32_a;

void genCases_i32_a_init( void )
{

    sequenceA.term1Num = 0;
    sequenceA.term2Num = 0;
    sequenceA.done = false;
    subcase = 0;
    genCases_total = (genCases_level == 1) ? 3 * i32NumP1 : 2 * i32NumP2;
    genCases_done = false;

}

void genCases_i32_a_next( void )
{

    if ( genCases_level == 1 ) {
        switch ( subcase ) {
         case 0:
            genCases_i32_a = i32RandomP3();
            break;
         case 1:
            genCases_i32_a = i32RandomPInf();
            break;
         case 2:
            genCases_i32_a = i32NextP1( &sequenceA );
            genCases_done = sequenceA.done;
            subcase = -1;
            break;
        }
    } else {
        switch ( subcase ) {
         case 0:
            genCases_i32_a = i32RandomP3();
            break;
         case 2:
            genCases_i32_a = i32RandomPInf();
            break;
         case 3:
            subcase = -1;
         case 1:
            genCases_i32_a = i32NextP2( &sequenceA );
            genCases_done = sequenceA.done;
            break;
        }
    }
    ++subcase;

}

