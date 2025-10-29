
/*============================================================================

This C source file is part of the SoftFloat IEEE Floating-Point Arithmetic
Package, Release 3e, by John R. Hauser.

Copyright 2011, 2012, 2013, 2014, 2015, 2016 The Regents of the University of
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

#include <stdint.h>
#include "platform.h"
#include "internals.h"
#include "softfloat.h"

#ifdef SOFTFLOAT_FAST_INT64

void i64_to_f128M( int64_t a, float128_t *zPtr )
{

    *zPtr = i64_to_f128( a );

}

#else

void i64_to_f128M( int64_t a, float128_t *zPtr )
{
    uint32_t *zWPtr;
    uint32_t uiZ96, uiZ64;
    bool sign;
    uint64_t absA;
    uint_fast8_t shiftDist;
    uint32_t *ptr;

    zWPtr = (uint32_t *) zPtr;
    uiZ96 = 0;
    uiZ64 = 0;
    zWPtr[indexWord( 4, 1 )] = 0;
    zWPtr[indexWord( 4, 0 )] = 0;
    if ( a ) {
        sign = (a < 0);
        absA = sign ? -(uint64_t) a : (uint64_t) a;
        shiftDist = softfloat_countLeadingZeros64( absA ) + 17;
        if ( shiftDist < 32 ) {
            ptr = zWPtr + indexMultiwordHi( 4, 3 );
            ptr[indexWord( 3, 2 )] = 0;
            ptr[indexWord( 3, 1 )] = absA>>32;
            ptr[indexWord( 3, 0 )] = absA;
            softfloat_shortShiftLeft96M( ptr, shiftDist, ptr );
            ptr[indexWordHi( 3 )] =
                packToF128UI96(
                    sign, 0x404E - shiftDist, ptr[indexWordHi( 3 )] );
            return;
        }
        absA <<= shiftDist - 32;
        uiZ96 = packToF128UI96( sign, 0x404E - shiftDist, absA>>32 );
        uiZ64 = absA;
    }
    zWPtr[indexWord( 4, 3 )] = uiZ96;
    zWPtr[indexWord( 4, 2 )] = uiZ64;

}

#endif

