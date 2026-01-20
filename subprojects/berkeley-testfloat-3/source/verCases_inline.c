
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
#include "platform.h"
#include "uint128.h"
#include "softfloat.h"

#ifdef FLOAT16

bool f16_same( float16_t a, float16_t b )
{
    union { uint16_t ui; float16_t f; } uA, uB;
    uA.f = a;
    uB.f = b;
    return (uA.ui == uB.ui);
}

bool f16_isNaN( float16_t a )
{
    union { uint16_t ui; float16_t f; } uA;
    uA.f = a;
    return 0x7C00 < (uA.ui & 0x7FFF);
}

#endif

bool f32_same( float32_t a, float32_t b )
{
    union { uint32_t ui; float32_t f; } uA, uB;
    uA.f = a;
    uB.f = b;
    return (uA.ui == uB.ui);
}

bool f32_isNaN( float32_t a )
{
    union { uint32_t ui; float32_t f; } uA;
    uA.f = a;
    return 0x7F800000 < (uA.ui & 0x7FFFFFFF);
}

#ifdef FLOAT64

bool f64_same( float64_t a, float64_t b )
{
    union { uint64_t ui; float64_t f; } uA, uB;
    uA.f = a;
    uB.f = b;
    return (uA.ui == uB.ui);
}

bool f64_isNaN( float64_t a )
{
    union { uint64_t ui; float64_t f; } uA;
    uA.f = a;
    return
        UINT64_C( 0x7FF0000000000000 )
            < (uA.ui & UINT64_C( 0x7FFFFFFFFFFFFFFF ));
}

#endif

#ifdef EXTFLOAT80

bool extF80M_same( const extFloat80_t *aPtr, const extFloat80_t *bPtr )
{
    const struct extFloat80M *aSPtr = (const struct extFloat80M *) aPtr;
    const struct extFloat80M *bSPtr = (const struct extFloat80M *) bPtr;
    return
        (aSPtr->signExp == bSPtr->signExp) && (aSPtr->signif == bSPtr->signif);
}

bool extF80M_isNaN( const extFloat80_t *aPtr )
{
    const struct extFloat80M *aSPtr = (const struct extFloat80M *) aPtr;
    return
        ((aSPtr->signExp & 0x7FFF) == 0x7FFF)
            && (aSPtr->signif & UINT64_C( 0x7FFFFFFFFFFFFFFF ));
}

#endif

#ifdef FLOAT128

bool f128M_same( const float128_t *aPtr, const float128_t *bPtr )
{
    const struct uint128 *uiAPtr = (const struct uint128 *) aPtr;
    const struct uint128 *uiBPtr = (const struct uint128 *) bPtr;
    return (uiAPtr->v64 == uiBPtr->v64) && (uiAPtr->v0 == uiBPtr->v0);
}

bool f128M_isNaN( const float128_t *aPtr )
{
    const struct uint128 *uiAPtr = (const struct uint128 *) aPtr;
    uint_fast64_t absA64 = uiAPtr->v64 & UINT64_C( 0x7FFFFFFFFFFFFFFF );
    return
        (UINT64_C( 0x7FFF000000000000 ) < absA64)
            || ((absA64 == UINT64_C( 0x7FFF000000000000 )) && uiAPtr->v0);
}

#endif

