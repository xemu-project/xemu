
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
#include "platform.h"
#include "uint128.h"
#include "softfloat.h"
#include "slowfloat.h"

uint_fast8_t slowfloat_roundingMode;
uint_fast8_t slowfloat_detectTininess;
uint_fast8_t slowfloat_exceptionFlags;
#ifdef EXTFLOAT80
uint_fast8_t slow_extF80_roundingPrecision;
#endif

#ifdef FLOAT16
union ui16_f16 { uint16_t ui; float16_t f; };
#endif
union ui32_f32 { uint32_t ui; float32_t f; };
#ifdef FLOAT64
union ui64_f64 { uint64_t ui; float64_t f; };
#endif

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

struct floatX {
    bool isNaN;
    bool isInf;
    bool isZero;
    bool sign;
    int_fast32_t exp;
    struct uint128 sig;
};

static const struct floatX floatXNaN =
    { true, false, false, false, 0, { 0, 0 } };
static const struct floatX floatXPositiveZero =
    { false, false, true, false, 0, { 0, 0 } };
static const struct floatX floatXNegativeZero =
    { false, false, true, true, 0, { 0, 0 } };

static
void
 roundFloatXTo11(
     bool isTiny, struct floatX *xPtr, uint_fast8_t roundingMode, bool exact )
{
    uint_fast64_t roundBits, sigX64;

    sigX64 = xPtr->sig.v64;
    roundBits = (sigX64 & UINT64_C( 0x1FFFFFFFFFFF )) | (xPtr->sig.v0 != 0);
    if ( roundBits ) {
        sigX64 &= UINT64_C( 0xFFFFE00000000000 );
        if ( exact ) slowfloat_exceptionFlags |= softfloat_flag_inexact;
        if ( isTiny ) slowfloat_exceptionFlags |= softfloat_flag_underflow;
        switch ( roundingMode ) {
         case softfloat_round_near_even:
            if ( roundBits < UINT64_C( 0x100000000000 ) ) goto noIncrement;
            if (
                (roundBits == UINT64_C( 0x100000000000 ))
                    && !(sigX64 & UINT64_C( 0x200000000000 ))
            ) {
                goto noIncrement;
            }
            break;
         case softfloat_round_minMag:
            goto noIncrement;
         case softfloat_round_min:
            if ( !xPtr->sign ) goto noIncrement;
            break;
         case softfloat_round_max:
            if ( xPtr->sign ) goto noIncrement;
            break;
         case softfloat_round_near_maxMag:
            if ( roundBits < UINT64_C( 0x100000000000 ) ) goto noIncrement;
            break;
#ifdef FLOAT_ROUND_ODD
         case softfloat_round_odd:
            sigX64 |= UINT64_C( 0x200000000000 );
            goto noIncrement;
#endif
        }
        sigX64 += UINT64_C( 0x200000000000 );
        if ( sigX64 == UINT64_C( 0x0100000000000000 ) ) {
            ++xPtr->exp;
            sigX64 = UINT64_C( 0x0080000000000000 );
        }
     noIncrement:
        xPtr->sig.v64 = sigX64;
        xPtr->sig.v0  = 0;
    }

}

static
void
 roundFloatXTo24(
     bool isTiny, struct floatX *xPtr, uint_fast8_t roundingMode, bool exact )
{
    uint_fast64_t sigX64;
    uint_fast32_t roundBits;

    sigX64 = xPtr->sig.v64;
    roundBits = (uint32_t) sigX64 | (xPtr->sig.v0 != 0);
    if ( roundBits ) {
        sigX64 &= UINT64_C( 0xFFFFFFFF00000000 );
        if ( exact ) slowfloat_exceptionFlags |= softfloat_flag_inexact;
        if ( isTiny ) slowfloat_exceptionFlags |= softfloat_flag_underflow;
        switch ( roundingMode ) {
         case softfloat_round_near_even:
            if ( roundBits < 0x80000000 ) goto noIncrement;
            if (
                (roundBits == 0x80000000)
                    && !(sigX64 & UINT64_C( 0x100000000 ))
            ) {
                goto noIncrement;
            }
            break;
         case softfloat_round_minMag:
            goto noIncrement;
         case softfloat_round_min:
            if ( !xPtr->sign ) goto noIncrement;
            break;
         case softfloat_round_max:
            if ( xPtr->sign ) goto noIncrement;
            break;
         case softfloat_round_near_maxMag:
            if ( roundBits < 0x80000000 ) goto noIncrement;
            break;
#ifdef FLOAT_ROUND_ODD
         case softfloat_round_odd:
            sigX64 |= UINT64_C( 0x100000000 );
            goto noIncrement;
#endif
        }
        sigX64 += UINT64_C( 0x100000000 );
        if ( sigX64 == UINT64_C( 0x0100000000000000 ) ) {
            ++xPtr->exp;
            sigX64 = UINT64_C( 0x0080000000000000 );
        }
     noIncrement:
        xPtr->sig.v64 = sigX64;
        xPtr->sig.v0  = 0;
    }

}

static
void
 roundFloatXTo53(
     bool isTiny, struct floatX *xPtr, uint_fast8_t roundingMode, bool exact )
{
    uint_fast64_t sigX64;
    uint_fast8_t roundBits;

    sigX64 = xPtr->sig.v64;
    roundBits = (sigX64 & 7) | (xPtr->sig.v0 != 0);
    if ( roundBits ) {
        sigX64 &= UINT64_C( 0xFFFFFFFFFFFFFFF8 );
        if ( exact ) slowfloat_exceptionFlags |= softfloat_flag_inexact;
        if ( isTiny ) slowfloat_exceptionFlags |= softfloat_flag_underflow;
        switch ( roundingMode ) {
         case softfloat_round_near_even:
            if ( roundBits < 4 ) goto noIncrement;
            if ( (roundBits == 4) && !(sigX64 & 8) ) goto noIncrement;
            break;
         case softfloat_round_minMag:
            goto noIncrement;
         case softfloat_round_min:
            if ( !xPtr->sign ) goto noIncrement;
            break;
         case softfloat_round_max:
            if ( xPtr->sign ) goto noIncrement;
            break;
         case softfloat_round_near_maxMag:
            if ( roundBits < 4 ) goto noIncrement;
            break;
#ifdef FLOAT_ROUND_ODD
         case softfloat_round_odd:
            sigX64 |= 8;
            goto noIncrement;
#endif
        }
        sigX64 += 8;
        if ( sigX64 == UINT64_C( 0x0100000000000000 ) ) {
            ++xPtr->exp;
            sigX64 = UINT64_C( 0x0080000000000000 );
        }
     noIncrement:
        xPtr->sig.v64 = sigX64;
        xPtr->sig.v0  = 0;
    }

}

static
void
 roundFloatXTo64(
     bool isTiny, struct floatX *xPtr, uint_fast8_t roundingMode, bool exact )
{
    uint_fast64_t sigX0, roundBits, sigX64;

    sigX0 = xPtr->sig.v0;
    roundBits = sigX0 & UINT64_C( 0x00FFFFFFFFFFFFFF );
    if ( roundBits ) {
        sigX0 &= UINT64_C( 0xFF00000000000000 );
        if ( exact ) slowfloat_exceptionFlags |= softfloat_flag_inexact;
        if ( isTiny ) slowfloat_exceptionFlags |= softfloat_flag_underflow;
        switch ( roundingMode ) {
         case softfloat_round_near_even:
            if ( roundBits < UINT64_C( 0x0080000000000000 ) ) goto noIncrement;
            if (
                (roundBits == UINT64_C( 0x0080000000000000 ))
                    && !(sigX0 & UINT64_C( 0x0100000000000000 ))
            ) {
                goto noIncrement;
            }
            break;
         case softfloat_round_minMag:
            goto noIncrement;
         case softfloat_round_min:
            if ( !xPtr->sign ) goto noIncrement;
            break;
         case softfloat_round_max:
            if ( xPtr->sign ) goto noIncrement;
            break;
         case softfloat_round_near_maxMag:
            if ( roundBits < UINT64_C( 0x0080000000000000 ) ) goto noIncrement;
            break;
#ifdef FLOAT_ROUND_ODD
         case softfloat_round_odd:
            sigX0 |= UINT64_C( 0x100000000000000 );
            goto noIncrement;
#endif
        }
        sigX0 += UINT64_C( 0x100000000000000 );
        sigX64 = xPtr->sig.v64 + !sigX0;
        if ( sigX64 == UINT64_C( 0x0100000000000000 ) ) {
            ++xPtr->exp;
            sigX64 = UINT64_C( 0x0080000000000000 );
        }
        xPtr->sig.v64 = sigX64;
     noIncrement:
        xPtr->sig.v0 = sigX0;
    }

}

static
void
 roundFloatXTo113(
     bool isTiny, struct floatX *xPtr, uint_fast8_t roundingMode, bool exact )
{
    uint_fast64_t sigX0;
    uint_fast8_t roundBits;
    uint_fast64_t sigX64;

    sigX0 = xPtr->sig.v0;
    roundBits = sigX0 & 0x7F;
    if ( roundBits ) {
        sigX0 &= UINT64_C( 0xFFFFFFFFFFFFFF80 );
        if ( exact ) slowfloat_exceptionFlags |= softfloat_flag_inexact;
        if ( isTiny ) slowfloat_exceptionFlags |= softfloat_flag_underflow;
        switch ( roundingMode ) {
         case softfloat_round_near_even:
            if ( roundBits < 0x40 ) goto noIncrement;
            if ( (roundBits == 0x40) && !(sigX0 & 0x80) ) goto noIncrement;
            break;
         case softfloat_round_minMag:
            goto noIncrement;
         case softfloat_round_min:
            if ( !xPtr->sign ) goto noIncrement;
            break;
         case softfloat_round_max:
            if ( xPtr->sign ) goto noIncrement;
            break;
         case softfloat_round_near_maxMag:
            if ( roundBits < 0x40 ) goto noIncrement;
            break;
#ifdef FLOAT_ROUND_ODD
         case softfloat_round_odd:
            sigX0 |= 0x80;
            goto noIncrement;
#endif
        }
        sigX0 += 0x80;
        sigX64 = xPtr->sig.v64 + !sigX0;
        if ( sigX64 == UINT64_C( 0x0100000000000000 ) ) {
            ++xPtr->exp;
            sigX64 = UINT64_C( 0x0080000000000000 );
        }
        xPtr->sig.v64 = sigX64;
     noIncrement:
        xPtr->sig.v0 = sigX0;
    }

}

static void ui32ToFloatX( uint_fast32_t a, struct floatX *xPtr )
{
    uint_fast64_t sig64;
    int_fast32_t exp;

    xPtr->isNaN = false;
    xPtr->isInf = false;
    xPtr->sign = false;
    sig64 = a;
    if ( a ) {
        xPtr->isZero = false;
        exp = 31;
        sig64 <<= 24;
        while ( sig64 < UINT64_C( 0x0080000000000000 ) ) {
            --exp;
            sig64 <<= 1;
        }
        xPtr->exp = exp;
    } else {
        xPtr->isZero = true;
    }
    xPtr->sig.v64 = sig64;
    xPtr->sig.v0  = 0;

}

static
uint_fast32_t
 floatXToUI32(
     const struct floatX *xPtr, uint_fast8_t roundingMode, bool exact )
{
    uint_fast8_t savedExceptionFlags;
    struct floatX x;
    int_fast32_t shiftDist;
    uint_fast32_t z;

    if ( xPtr->isInf || xPtr->isNaN ) {
        slowfloat_exceptionFlags |= softfloat_flag_invalid;
        return (xPtr->isInf && xPtr->sign) ? 0 : 0xFFFFFFFF;
    }
    if ( xPtr->isZero ) return 0;
    savedExceptionFlags = slowfloat_exceptionFlags;
    x = *xPtr;
    shiftDist = 52 - x.exp;
    if ( 56 < shiftDist ) {
        x.sig.v64 = 0;
        x.sig.v0  = 1;
    } else {
        while ( 0 < shiftDist ) {
            x.sig = shortShiftRightJam128( x.sig, 1 );
            --shiftDist;
        }
    }
    roundFloatXTo53( false, &x, roundingMode, exact );
    x.sig = shortShiftRightJam128( x.sig, 3 );
    z = x.sig.v64;
    if ( (shiftDist < 0) || x.sig.v64>>32 || (x.sign && z) ) {
        slowfloat_exceptionFlags =
            savedExceptionFlags | softfloat_flag_invalid;
        return x.sign ? 0 : 0xFFFFFFFF;
    }
    return z;

}

static void ui64ToFloatX( uint_fast64_t a, struct floatX *xPtr )
{
    struct uint128 sig;
    int_fast32_t exp;

    xPtr->isNaN = false;
    xPtr->isInf = false;
    xPtr->sign = false;
    sig.v64 = 0;
    sig.v0  = a;
    if ( a ) {
        xPtr->isZero = false;
        exp = 63;
        sig = shortShiftLeft128( sig, 56 );
        while ( sig.v64 < UINT64_C( 0x0080000000000000 ) ) {
            --exp;
            sig = shortShiftLeft128( sig, 1 );
        }
        xPtr->exp = exp;
    } else {
        xPtr->isZero = true;
    }
    xPtr->sig = sig;

}

static
uint_fast64_t
 floatXToUI64(
     const struct floatX *xPtr, uint_fast8_t roundingMode, bool exact )
{
    uint_fast8_t savedExceptionFlags;
    struct floatX x;
    int_fast32_t shiftDist;
    uint_fast64_t z;

    if ( xPtr->isInf || xPtr->isNaN ) {
        slowfloat_exceptionFlags |= softfloat_flag_invalid;
        return
            (xPtr->isInf && xPtr->sign) ? 0 : UINT64_C( 0xFFFFFFFFFFFFFFFF );
    }
    if ( xPtr->isZero ) return 0;
    savedExceptionFlags = slowfloat_exceptionFlags;
    x = *xPtr;
    shiftDist = 112 - x.exp;
    if ( 116 < shiftDist ) {
        x.sig.v64 = 0;
        x.sig.v0  = 1;
    } else {
        while ( 0 < shiftDist ) {
            x.sig = shortShiftRightJam128( x.sig, 1 );
            --shiftDist;
        }
    }
    roundFloatXTo113( false, &x, roundingMode, exact );
    x.sig = shortShiftRightJam128( x.sig, 7 );
    z = x.sig.v0;
    if ( (shiftDist < 0) || x.sig.v64 || (x.sign && z) ) {
        slowfloat_exceptionFlags =
            savedExceptionFlags | softfloat_flag_invalid;
        return x.sign ? 0 : UINT64_C( 0xFFFFFFFFFFFFFFFF );
    }
    return z;

}

static void i32ToFloatX( int_fast32_t a, struct floatX *xPtr )
{
    bool sign;
    uint_fast64_t sig64;
    int_fast32_t exp;

    xPtr->isNaN = false;
    xPtr->isInf = false;
    sign = (a < 0);
    xPtr->sign = sign;
    sig64 = sign ? -(uint64_t) a : a;
    if ( a ) {
        xPtr->isZero = false;
        exp = 31;
        sig64 <<= 24;
        while ( sig64 < UINT64_C( 0x0080000000000000 ) ) {
            --exp;
            sig64 <<= 1;
        }
        xPtr->exp = exp;
    } else {
        xPtr->isZero = true;
    }
    xPtr->sig.v64 = sig64;
    xPtr->sig.v0  = 0;

}

static
int_fast32_t
 floatXToI32(
     const struct floatX *xPtr, uint_fast8_t roundingMode, bool exact )
{
    uint_fast8_t savedExceptionFlags;
    struct floatX x;
    int_fast32_t shiftDist;
    union { uint32_t ui; int32_t i; } uZ;

    if ( xPtr->isInf || xPtr->isNaN ) {
        slowfloat_exceptionFlags |= softfloat_flag_invalid;
        return (xPtr->isInf && xPtr->sign) ? -0x7FFFFFFF - 1 : 0x7FFFFFFF;
    }
    if ( xPtr->isZero ) return 0;
    savedExceptionFlags = slowfloat_exceptionFlags;
    x = *xPtr;
    shiftDist = 52 - x.exp;
    if ( 56 < shiftDist ) {
        x.sig.v64 = 0;
        x.sig.v0  = 1;
    } else {
        while ( 0 < shiftDist ) {
            x.sig = shortShiftRightJam128( x.sig, 1 );
            --shiftDist;
        }
    }
    roundFloatXTo53( false, &x, roundingMode, exact );
    x.sig = shortShiftRightJam128( x.sig, 3 );
    uZ.ui = x.sig.v64;
    if ( x.sign ) uZ.ui = -uZ.ui;
    if (
        (shiftDist < 0) || x.sig.v64>>32
            || ((uZ.i != 0) && (x.sign != (uZ.i < 0)))
    ) {
        slowfloat_exceptionFlags =
            savedExceptionFlags | softfloat_flag_invalid;
        return x.sign ? -0x7FFFFFFF - 1 : 0x7FFFFFFF;
    }
    return uZ.i;

}

static void i64ToFloatX( int_fast64_t a, struct floatX *xPtr )
{
    bool sign;
    struct uint128 sig;
    int_fast32_t exp;

    xPtr->isNaN = false;
    xPtr->isInf = false;
    sign = (a < 0);
    xPtr->sign = sign;
    sig.v64 = 0;
    sig.v0  = sign ? -(uint_fast64_t) a : a;
    if ( a ) {
        xPtr->isZero = false;
        exp = 63;
        sig = shortShiftLeft128( sig, 56 );
        while ( sig.v64 < UINT64_C( 0x0080000000000000 ) ) {
            --exp;
            sig = shortShiftLeft128( sig, 1 );
        }
        xPtr->exp = exp;
    } else {
        xPtr->isZero = true;
    }
    xPtr->sig = sig;

}

static
int_fast64_t
 floatXToI64(
     const struct floatX *xPtr, uint_fast8_t roundingMode, bool exact )
{
    uint_fast8_t savedExceptionFlags;
    struct floatX x;
    int_fast32_t shiftDist;
    union { uint64_t ui; int64_t i; } uZ;

    if ( xPtr->isInf || xPtr->isNaN ) {
        slowfloat_exceptionFlags |= softfloat_flag_invalid;
        return
            (xPtr->isInf && xPtr->sign) ? -INT64_C( 0x7FFFFFFFFFFFFFFF ) - 1
                : INT64_C( 0x7FFFFFFFFFFFFFFF );
    }
    if ( xPtr->isZero ) return 0;
    savedExceptionFlags = slowfloat_exceptionFlags;
    x = *xPtr;
    shiftDist = 112 - x.exp;
    if ( 116 < shiftDist ) {
        x.sig.v64 = 0;
        x.sig.v0  = 1;
    } else {
        while ( 0 < shiftDist ) {
            x.sig = shortShiftRightJam128( x.sig, 1 );
            --shiftDist;
        }
    }
    roundFloatXTo113( false, &x, roundingMode, exact );
    x.sig = shortShiftRightJam128( x.sig, 7 );
    uZ.ui = x.sig.v0;
    if ( x.sign ) uZ.ui = -uZ.ui;
    if (
        (shiftDist < 0) || x.sig.v64 || ((uZ.i != 0) && (x.sign != (uZ.i < 0)))
    ) {
        slowfloat_exceptionFlags =
            savedExceptionFlags | softfloat_flag_invalid;
        return
            x.sign ? -INT64_C( 0x7FFFFFFFFFFFFFFF ) - 1
                : INT64_C( 0x7FFFFFFFFFFFFFFF );
    }
    return uZ.i;

}

#ifdef FLOAT16

static void f16ToFloatX( float16_t a, struct floatX *xPtr )
{
    union ui16_f16 uA;
    uint_fast16_t uiA;
    int_fast8_t exp;
    uint_fast64_t sig64;

    uA.f = a;
    uiA = uA.ui;
    xPtr->isNaN = false;
    xPtr->isInf = false;
    xPtr->isZero = false;
    xPtr->sign = ((uiA & 0x8000) != 0);
    exp = uiA>>10 & 0x1F;
    sig64 = uiA & 0x03FF;
    sig64 <<= 45;
    if ( exp == 0x1F ) {
        if ( sig64 ) {
            xPtr->isNaN = true;
        } else {
            xPtr->isInf = true;
        }
    } else if ( !exp ) {
        if ( !sig64 ) {
            xPtr->isZero = true;
        } else {
            exp = 1 - 0xF;
            do {
                --exp;
                sig64 <<= 1;
            } while ( sig64 < UINT64_C( 0x0080000000000000 ) );
            xPtr->exp = exp;
        }
    } else {
        xPtr->exp = exp - 0xF;
        sig64 |= UINT64_C( 0x0080000000000000 );
    }
    xPtr->sig.v64 = sig64;
    xPtr->sig.v0  = 0;

}

static float16_t floatXToF16( const struct floatX *xPtr )
{
    uint_fast16_t uiZ;
    struct floatX x, savedX;
    bool isTiny;
    int_fast32_t exp;
    union ui16_f16 uZ;

    if ( xPtr->isNaN ) {
        uiZ = 0xFFFF;
        goto uiZ;
    }
    if ( xPtr->isInf ) {
        uiZ = xPtr->sign ? 0xFC00 : 0x7C00;
        goto uiZ;
    }
    if ( xPtr->isZero ) {
        uiZ = xPtr->sign ? 0x8000 : 0;
        goto uiZ;
    }
    x = *xPtr;
    while ( UINT64_C( 0x0100000000000000 ) <= x.sig.v64 ) {
        ++x.exp;
        x.sig = shortShiftRightJam128( x.sig, 1 );
    }
    while ( x.sig.v64 < UINT64_C( 0x0080000000000000 ) ) {
        --x.exp;
        x.sig = shortShiftLeft128( x.sig, 1 );
    }
    savedX = x;
    isTiny =
        (slowfloat_detectTininess == softfloat_tininess_beforeRounding)
            && (x.exp + 0xF <= 0);
    roundFloatXTo11( isTiny, &x, slowfloat_roundingMode, true );
    exp = x.exp + 0xF;
    if ( 0x1F <= exp ) {
        slowfloat_exceptionFlags |=
            softfloat_flag_overflow | softfloat_flag_inexact;
        if ( x.sign ) {
            switch ( slowfloat_roundingMode ) {
             case softfloat_round_near_even:
             case softfloat_round_min:
             case softfloat_round_near_maxMag:
                uiZ = 0xFC00;
                break;
             case softfloat_round_minMag:
             case softfloat_round_max:
             case softfloat_round_odd:
                uiZ = 0xFBFF;
                break;
            }
        } else {
            switch ( slowfloat_roundingMode ) {
             case softfloat_round_near_even:
             case softfloat_round_max:
             case softfloat_round_near_maxMag:
                uiZ = 0x7C00;
                break;
             case softfloat_round_minMag:
             case softfloat_round_min:
             case softfloat_round_odd:
                uiZ = 0x7BFF;
                break;
            }
        }
        goto uiZ;
    }
    if ( exp <= 0 ) {
        isTiny = true;
        x = savedX;
        exp = x.exp + 0xF;
        if ( exp < -14 ) {
            x.sig.v0 = (x.sig.v64 != 0) || (x.sig.v0 != 0);
            x.sig.v64 = 0;
        } else {
            while ( exp <= 0 ) {
                ++exp;
                x.sig = shortShiftRightJam128( x.sig, 1 );
            }
        }
        roundFloatXTo11( isTiny, &x, slowfloat_roundingMode, true );
        exp = (UINT64_C( 0x0080000000000000 ) <= x.sig.v64) ? 1 : 0;
    }
    uiZ = (uint_fast16_t) exp<<10;
    if ( x.sign ) uiZ |= 0x8000;
    uiZ |= x.sig.v64>>45 & 0x03FF;
 uiZ:
    uZ.ui = uiZ;
    return uZ.f;

}

#endif

static void f32ToFloatX( float32_t a, struct floatX *xPtr )
{
    union ui32_f32 uA;
    uint_fast32_t uiA;
    int_fast16_t exp;
    uint_fast64_t sig64;

    uA.f = a;
    uiA = uA.ui;
    xPtr->isNaN = false;
    xPtr->isInf = false;
    xPtr->isZero = false;
    xPtr->sign = ((uiA & 0x80000000) != 0);
    exp = uiA>>23 & 0xFF;
    sig64 = uiA & 0x007FFFFF;
    sig64 <<= 32;
    if ( exp == 0xFF ) {
        if ( sig64 ) {
            xPtr->isNaN = true;
        } else {
            xPtr->isInf = true;
        }
    } else if ( !exp ) {
        if ( !sig64 ) {
            xPtr->isZero = true;
        } else {
            exp = 1 - 0x7F;
            do {
                --exp;
                sig64 <<= 1;
            } while ( sig64 < UINT64_C( 0x0080000000000000 ) );
            xPtr->exp = exp;
        }
    } else {
        xPtr->exp = exp - 0x7F;
        sig64 |= UINT64_C( 0x0080000000000000 );
    }
    xPtr->sig.v64 = sig64;
    xPtr->sig.v0  = 0;

}

static float32_t floatXToF32( const struct floatX *xPtr )
{
    uint_fast32_t uiZ;
    struct floatX x, savedX;
    bool isTiny;
    int_fast32_t exp;
    union ui32_f32 uZ;

    if ( xPtr->isNaN ) {
        uiZ = 0xFFFFFFFF;
        goto uiZ;
    }
    if ( xPtr->isInf ) {
        uiZ = xPtr->sign ? 0xFF800000 : 0x7F800000;
        goto uiZ;
    }
    if ( xPtr->isZero ) {
        uiZ = xPtr->sign ? 0x80000000 : 0;
        goto uiZ;
    }
    x = *xPtr;
    while ( UINT64_C( 0x0100000000000000 ) <= x.sig.v64 ) {
        ++x.exp;
        x.sig = shortShiftRightJam128( x.sig, 1 );
    }
    while ( x.sig.v64 < UINT64_C( 0x0080000000000000 ) ) {
        --x.exp;
        x.sig = shortShiftLeft128( x.sig, 1 );
    }
    savedX = x;
    isTiny =
        (slowfloat_detectTininess == softfloat_tininess_beforeRounding)
            && (x.exp + 0x7F <= 0);
    roundFloatXTo24( isTiny, &x, slowfloat_roundingMode, true );
    exp = x.exp + 0x7F;
    if ( 0xFF <= exp ) {
        slowfloat_exceptionFlags |=
            softfloat_flag_overflow | softfloat_flag_inexact;
        if ( x.sign ) {
            switch ( slowfloat_roundingMode ) {
             case softfloat_round_near_even:
             case softfloat_round_min:
             case softfloat_round_near_maxMag:
                uiZ = 0xFF800000;
                break;
             case softfloat_round_minMag:
             case softfloat_round_max:
             case softfloat_round_odd:
                uiZ = 0xFF7FFFFF;
                break;
            }
        } else {
            switch ( slowfloat_roundingMode ) {
             case softfloat_round_near_even:
             case softfloat_round_max:
             case softfloat_round_near_maxMag:
                uiZ = 0x7F800000;
                break;
             case softfloat_round_minMag:
             case softfloat_round_min:
             case softfloat_round_odd:
                uiZ = 0x7F7FFFFF;
                break;
            }
        }
        goto uiZ;
    }
    if ( exp <= 0 ) {
        isTiny = true;
        x = savedX;
        exp = x.exp + 0x7F;
        if ( exp < -27 ) {
            x.sig.v0 = (x.sig.v64 != 0) || (x.sig.v0 != 0);
            x.sig.v64 = 0;
        } else {
            while ( exp <= 0 ) {
                ++exp;
                x.sig = shortShiftRightJam128( x.sig, 1 );
            }
        }
        roundFloatXTo24( isTiny, &x, slowfloat_roundingMode, true );
        exp = (UINT64_C( 0x0080000000000000 ) <= x.sig.v64) ? 1 : 0;
    }
    uiZ = (uint_fast32_t) exp<<23;
    if ( x.sign ) uiZ |= 0x80000000;
    uiZ |= x.sig.v64>>32 & 0x007FFFFF;
 uiZ:
    uZ.ui = uiZ;
    return uZ.f;

}

#ifdef FLOAT64

static void f64ToFloatX( float64_t a, struct floatX *xPtr )
{
    union ui64_f64 uA;
    uint_fast64_t uiA;
    int_fast16_t exp;
    uint_fast64_t sig64;

    uA.f = a;
    uiA = uA.ui;
    xPtr->isNaN = false;
    xPtr->isInf = false;
    xPtr->isZero = false;
    xPtr->sign = ((uiA & UINT64_C( 0x8000000000000000 )) != 0);
    exp = uiA>>52 & 0x7FF;
    sig64 = uiA & UINT64_C( 0x000FFFFFFFFFFFFF );
    if ( exp == 0x7FF ) {
        if ( sig64 ) {
            xPtr->isNaN = true;
        } else {
            xPtr->isInf = true;
        }
    } else if ( !exp ) {
        if ( !sig64 ) {
            xPtr->isZero = true;
        } else {
            exp = 1 - 0x3FF;
            do {
                --exp;
                sig64 <<= 1;
            } while ( sig64 < UINT64_C( 0x0010000000000000 ) );
            xPtr->exp = exp;
        }
    } else {
        xPtr->exp = exp - 0x3FF;
        sig64 |= UINT64_C( 0x0010000000000000 );
    }
    xPtr->sig.v64 = sig64<<3;
    xPtr->sig.v0  = 0;

}

static float64_t floatXToF64( const struct floatX *xPtr )
{
    uint_fast64_t uiZ;
    struct floatX x, savedX;
    bool isTiny;
    int_fast32_t exp;
    union ui64_f64 uZ;

    if ( xPtr->isNaN ) {
        uiZ = UINT64_C( 0xFFFFFFFFFFFFFFFF );
        goto uiZ;
    }
    if ( xPtr->isInf ) {
        uiZ =
            xPtr->sign ? UINT64_C( 0xFFF0000000000000 )
                : UINT64_C( 0x7FF0000000000000 );
        goto uiZ;
    }
    if ( xPtr->isZero ) {
        uiZ = xPtr->sign ? UINT64_C( 0x8000000000000000 ) : 0;
        goto uiZ;
    }
    x = *xPtr;
    while ( UINT64_C( 0x0100000000000000 ) <= x.sig.v64 ) {
        ++x.exp;
        x.sig = shortShiftRightJam128( x.sig, 1 );
    }
    while ( x.sig.v64 < UINT64_C( 0x0080000000000000 ) ) {
        --x.exp;
        x.sig = shortShiftLeft128( x.sig, 1 );
    }
    savedX = x;
    isTiny =
        (slowfloat_detectTininess == softfloat_tininess_beforeRounding)
            && (x.exp + 0x3FF <= 0);
    roundFloatXTo53( isTiny, &x, slowfloat_roundingMode, true );
    exp = x.exp + 0x3FF;
    if ( 0x7FF <= exp ) {
        slowfloat_exceptionFlags |=
            softfloat_flag_overflow | softfloat_flag_inexact;
        if ( x.sign ) {
            switch ( slowfloat_roundingMode ) {
             case softfloat_round_near_even:
             case softfloat_round_min:
             case softfloat_round_near_maxMag:
                uiZ = UINT64_C( 0xFFF0000000000000 );
                break;
             case softfloat_round_minMag:
             case softfloat_round_max:
             case softfloat_round_odd:
                uiZ = UINT64_C( 0xFFEFFFFFFFFFFFFF );
                break;
            }
        } else {
            switch ( slowfloat_roundingMode ) {
             case softfloat_round_near_even:
             case softfloat_round_max:
             case softfloat_round_near_maxMag:
                uiZ = UINT64_C( 0x7FF0000000000000 );
                break;
             case softfloat_round_minMag:
             case softfloat_round_min:
             case softfloat_round_odd:
                uiZ = UINT64_C( 0x7FEFFFFFFFFFFFFF );
                break;
            }
        }
        goto uiZ;
    }
    if ( exp <= 0 ) {
        isTiny = true;
        x = savedX;
        exp = x.exp + 0x3FF;
        if ( exp < -56 ) {
            x.sig.v0 = (x.sig.v64 != 0) || (x.sig.v0 != 0);
            x.sig.v64 = 0;
        } else {
            while ( exp <= 0 ) {
                ++exp;
                x.sig = shortShiftRightJam128( x.sig, 1 );
            }
        }
        roundFloatXTo53( isTiny, &x, slowfloat_roundingMode, true );
        exp = (UINT64_C( 0x0080000000000000 ) <= x.sig.v64) ? 1 : 0;
    }
    uiZ = (uint_fast64_t) exp<<52;
    if ( x.sign ) uiZ |= UINT64_C( 0x8000000000000000 );
    uiZ |= x.sig.v64>>3 & UINT64_C( 0x000FFFFFFFFFFFFF );
 uiZ:
    uZ.ui = uiZ;
    return uZ.f;

}

#endif

#ifdef EXTFLOAT80

static void extF80MToFloatX( const extFloat80_t *aPtr, struct floatX *xPtr )
{
    const struct extFloat80M *aSPtr;
    uint_fast16_t uiA64;
    int_fast32_t exp;
    struct uint128 sig;

    aSPtr = (const struct extFloat80M *) aPtr;
    xPtr->isNaN = false;
    xPtr->isInf = false;
    xPtr->isZero = false;
    uiA64 = aSPtr->signExp;
    xPtr->sign = ((uiA64 & 0x8000) != 0);
    exp = uiA64 & 0x7FFF;
    sig.v64 = 0;
    sig.v0  = aSPtr->signif;
    if ( exp == 0x7FFF ) {
        if ( sig.v0 & UINT64_C( 0x7FFFFFFFFFFFFFFF ) ) {
            xPtr->isNaN = true;
        } else {
            xPtr->isInf = true;
        }
    } else {
        if ( !exp ) ++exp;
        exp -= 0x3FFF;
        if ( !(sig.v0 & UINT64_C( 0x8000000000000000 )) ) {
            if ( !sig.v0 ) {
                xPtr->isZero = true;
            } else {
                do {
                    --exp;
                    sig.v0 <<= 1;
                } while ( sig.v0 < UINT64_C( 0x8000000000000000 ) );
            }
        }
        xPtr->exp = exp;
    }
    xPtr->sig = shortShiftLeft128( sig, 56 );

}

static void floatXToExtF80M( const struct floatX *xPtr, extFloat80_t *zPtr )
{
    struct extFloat80M *zSPtr;
    struct floatX x, savedX;
    bool isTiny;
    int_fast32_t exp;
    uint_fast64_t uiZ0;
    uint_fast16_t uiZ64;

    zSPtr = (struct extFloat80M *) zPtr;
    if ( xPtr->isNaN ) {
        zSPtr->signExp = 0xFFFF;
        zSPtr->signif = UINT64_C( 0xFFFFFFFFFFFFFFFF );
        return;
    }
    if ( xPtr->isInf ) {
        zSPtr->signExp = xPtr->sign ? 0xFFFF : 0x7FFF;
        zSPtr->signif = UINT64_C( 0x8000000000000000 );
        return;
    }
    if ( xPtr->isZero ) {
        zSPtr->signExp = xPtr->sign ? 0x8000 : 0;
        zSPtr->signif = 0;
        return;
    }
    x = *xPtr;
    while ( UINT64_C( 0x0100000000000000 ) <= x.sig.v64 ) {
        ++x.exp;
        x.sig = shortShiftRightJam128( x.sig, 1 );
    }
    while ( x.sig.v64 < UINT64_C( 0x0080000000000000 ) ) {
        --x.exp;
        x.sig = shortShiftLeft128( x.sig, 1 );
    }
    savedX = x;
    isTiny =
        (slowfloat_detectTininess == softfloat_tininess_beforeRounding)
            && (x.exp + 0x3FFF <= 0);
    switch ( slow_extF80_roundingPrecision ) {
     case 32:
        roundFloatXTo24( isTiny, &x, slowfloat_roundingMode, true );
        break;
     case 64:
        roundFloatXTo53( isTiny, &x, slowfloat_roundingMode, true );
        break;
     default:
        roundFloatXTo64( isTiny, &x, slowfloat_roundingMode, true );
        break;
    }
    exp = x.exp + 0x3FFF;
    if ( 0x7FFF <= exp ) {
        slowfloat_exceptionFlags |=
            softfloat_flag_overflow | softfloat_flag_inexact;
        if ( x.sign ) {
            switch ( slowfloat_roundingMode ) {
             case softfloat_round_near_even:
             case softfloat_round_min:
             case softfloat_round_near_maxMag:
                zSPtr->signExp = 0xFFFF;
                zSPtr->signif = UINT64_C( 0x8000000000000000 );
                break;
             case softfloat_round_minMag:
             case softfloat_round_max:
             case softfloat_round_odd:
                switch ( slow_extF80_roundingPrecision ) {
                 case 32:
                    uiZ0 = UINT64_C( 0xFFFFFF0000000000 );
                    break;
                 case 64:
                    uiZ0 = UINT64_C( 0xFFFFFFFFFFFFF800 );
                    break;
                 default:
                    uiZ0 = UINT64_C( 0xFFFFFFFFFFFFFFFF );
                    break;
                }
                zSPtr->signExp = 0xFFFE;
                zSPtr->signif = uiZ0;
                break;
            }
        } else {
            switch ( slowfloat_roundingMode ) {
             case softfloat_round_near_even:
             case softfloat_round_max:
             case softfloat_round_near_maxMag:
                zSPtr->signExp = 0x7FFF;
                zSPtr->signif = UINT64_C( 0x8000000000000000 );
                break;
             case softfloat_round_minMag:
             case softfloat_round_min:
             case softfloat_round_odd:
                switch ( slow_extF80_roundingPrecision ) {
                 case 32:
                    uiZ0 = UINT64_C( 0xFFFFFF0000000000 );
                    break;
                 case 64:
                    uiZ0 = UINT64_C( 0xFFFFFFFFFFFFF800 );
                    break;
                 default:
                    uiZ0 = UINT64_C( 0xFFFFFFFFFFFFFFFF );
                    break;
                }
                zSPtr->signExp = 0x7FFE;
                zSPtr->signif = uiZ0;
                break;
            }
        }
        return;
    }
    if ( exp <= 0 ) {
        isTiny = true;
        x = savedX;
        exp = x.exp + 0x3FFF;
        if ( exp < -70 ) {
            x.sig.v0 = (x.sig.v64 != 0) || (x.sig.v0 != 0);
            x.sig.v64 = 0;
        } else {
            while ( exp <= 0 ) {
                ++exp;
                x.sig = shortShiftRightJam128( x.sig, 1 );
            }
        }
        switch ( slow_extF80_roundingPrecision ) {
         case 32:
            roundFloatXTo24( isTiny, &x, slowfloat_roundingMode, true );
            break;
         case 64:
            roundFloatXTo53( isTiny, &x, slowfloat_roundingMode, true );
            break;
         default:
            roundFloatXTo64( isTiny, &x, slowfloat_roundingMode, true );
            break;
        }
        exp = (UINT64_C( 0x0080000000000000 ) <= x.sig.v64) ? 1 : 0;
    }
    uiZ64 = exp;
    if ( x.sign ) uiZ64 |= 0x8000;
    zSPtr->signExp = uiZ64;
    zSPtr->signif = shortShiftRightJam128( x.sig, 56 ).v0;

}

#endif

#ifdef FLOAT128

static void f128MToFloatX( const float128_t *aPtr, struct floatX *xPtr )
{
    const struct uint128 *uiAPtr;
    uint_fast64_t uiA64;
    int_fast32_t exp;
    struct uint128 sig;

    uiAPtr = (const struct uint128 *) aPtr;
    xPtr->isNaN = false;
    xPtr->isInf = false;
    xPtr->isZero = false;
    uiA64 = uiAPtr->v64;
    xPtr->sign = ((uiA64 & UINT64_C( 0x8000000000000000 )) != 0);
    exp = uiA64>>48 & 0x7FFF;
    sig.v64 = uiA64 & UINT64_C( 0x0000FFFFFFFFFFFF );
    sig.v0  = uiAPtr->v0;
    if ( exp == 0x7FFF ) {
        if ( sig.v64 || sig.v0 ) {
            xPtr->isNaN = true;
        } else {
            xPtr->isInf = true;
        }
    } else if ( !exp ) {
        if ( !sig.v64 && !sig.v0 ) {
            xPtr->isZero = true;
        } else {
            exp = 1 - 0x3FFF;
            do {
                --exp;
                sig = shortShiftLeft128( sig, 1 );
            } while ( sig.v64 < UINT64_C( 0x0001000000000000 ) );
            xPtr->exp = exp;
        }
    } else {
        xPtr->exp = exp - 0x3FFF;
        sig.v64 |= UINT64_C( 0x0001000000000000 );
    }
    xPtr->sig = shortShiftLeft128( sig, 7 );

}

static void floatXToF128M( const struct floatX *xPtr, float128_t *zPtr )
{
    struct uint128 *uiZPtr;
    struct floatX x, savedX;
    bool isTiny;
    int_fast32_t exp;
    uint_fast64_t uiZ64;

    uiZPtr = (struct uint128 *) zPtr;
    if ( xPtr->isNaN ) {
        uiZPtr->v64 = uiZPtr->v0 = UINT64_C( 0xFFFFFFFFFFFFFFFF );
        return;
    }
    if ( xPtr->isInf ) {
        uiZPtr->v64 =
            xPtr->sign ? UINT64_C( 0xFFFF000000000000 )
                : UINT64_C( 0x7FFF000000000000 );
        uiZPtr->v0 = 0;
        return;
    }
    if ( xPtr->isZero ) {
        uiZPtr->v64 = xPtr->sign ? UINT64_C( 0x8000000000000000 ) : 0;
        uiZPtr->v0  = 0;
        return;
    }
    x = *xPtr;
    while ( UINT64_C( 0x0100000000000000 ) <= x.sig.v64 ) {
        ++x.exp;
        x.sig = shortShiftRightJam128( x.sig, 1 );
    }
    while ( x.sig.v64 < UINT64_C( 0x0080000000000000 ) ) {
        --x.exp;
        x.sig = shortShiftLeft128( x.sig, 1 );
    }
    savedX = x;
    isTiny =
        (slowfloat_detectTininess == softfloat_tininess_beforeRounding)
            && (x.exp + 0x3FFF <= 0);
    roundFloatXTo113( isTiny, &x, slowfloat_roundingMode, true );
    exp = x.exp + 0x3FFF;
    if ( 0x7FFF <= exp ) {
        slowfloat_exceptionFlags |=
            softfloat_flag_overflow | softfloat_flag_inexact;
        if ( x.sign ) {
            switch ( slowfloat_roundingMode ) {
             case softfloat_round_near_even:
             case softfloat_round_min:
             case softfloat_round_near_maxMag:
                uiZPtr->v64 = UINT64_C( 0xFFFF000000000000 );
                uiZPtr->v0  = 0;
                break;
             case softfloat_round_minMag:
             case softfloat_round_max:
             case softfloat_round_odd:
                uiZPtr->v64 = UINT64_C( 0xFFFEFFFFFFFFFFFF );
                uiZPtr->v0  = UINT64_C( 0xFFFFFFFFFFFFFFFF );
                break;
            }
        } else {
            switch ( slowfloat_roundingMode ) {
             case softfloat_round_near_even:
             case softfloat_round_max:
             case softfloat_round_near_maxMag:
                uiZPtr->v64 = UINT64_C( 0x7FFF000000000000 );
                uiZPtr->v0  = 0;
                break;
             case softfloat_round_minMag:
             case softfloat_round_min:
             case softfloat_round_odd:
                uiZPtr->v64 = UINT64_C( 0x7FFEFFFFFFFFFFFF );
                uiZPtr->v0  = UINT64_C( 0xFFFFFFFFFFFFFFFF );
                break;
            }
        }
        return;
    }
    if ( exp <= 0 ) {
        isTiny = true;
        x = savedX;
        exp = x.exp + 0x3FFF;
        if ( exp < -120 ) {
            x.sig.v0 = (x.sig.v64 != 0) || (x.sig.v0 != 0);
            x.sig.v64 = 0;
        } else {
            while ( exp <= 0 ) {
                ++exp;
                x.sig = shortShiftRightJam128( x.sig, 1 );
            }
        }
        roundFloatXTo113( isTiny, &x, slowfloat_roundingMode, true );
        exp = (UINT64_C( 0x0080000000000000 ) <= x.sig.v64) ? 1 : 0;
    }
    uiZ64 = (uint_fast64_t) exp<<48;
    if ( x.sign ) uiZ64 |= UINT64_C( 0x8000000000000000 );
    x.sig = shortShiftRightJam128( x.sig, 7 );
    uiZPtr->v64 = uiZ64 | (x.sig.v64 & UINT64_C( 0x0000FFFFFFFFFFFF ));
    uiZPtr->v0  = x.sig.v0;

}

#endif

static void floatXInvalid( struct floatX *xPtr )
{

    slowfloat_exceptionFlags |= softfloat_flag_invalid;
    *xPtr = floatXNaN;

}

static
void
 floatXRoundToInt( struct floatX *xPtr, uint_fast8_t roundingMode, bool exact )
{
    int_fast32_t exp, shiftDist;
    struct uint128 sig;

    if ( xPtr->isNaN || xPtr->isInf ) return;
    exp = xPtr->exp;
    shiftDist = 112 - exp;
    if ( shiftDist <= 0 ) return;
    if ( 119 < shiftDist ) {
        xPtr->exp = 112;
        xPtr->sig.v64 = 0;
        xPtr->sig.v0 = !xPtr->isZero;
    } else {
        sig = xPtr->sig;
        while ( 0 < shiftDist ) {
            ++exp;
            sig = shortShiftRightJam128( sig, 1 );
            --shiftDist;
        }
        xPtr->exp = exp;
        xPtr->sig = sig;
    }
    roundFloatXTo113( false, xPtr, roundingMode, exact );
    if ( !xPtr->sig.v64 && !xPtr->sig.v0 ) xPtr->isZero = true;

}

static void floatXAdd( struct floatX *xPtr, const struct floatX *yPtr )
{
    int_fast32_t expX, expY, expDiff;
    struct uint128 sigY;

    if ( xPtr->isNaN ) return;
    if ( yPtr->isNaN ) goto copyY;
    if ( xPtr->isInf && yPtr->isInf ) {
        if ( xPtr->sign != yPtr->sign ) floatXInvalid( xPtr );
        return;
    }
    if ( xPtr->isInf ) return;
    if ( yPtr->isInf ) goto copyY;
    if ( xPtr->isZero && yPtr->isZero ) {
        if ( xPtr->sign == yPtr->sign ) return;
        goto completeCancellation;
    }
    expX = xPtr->exp;
    expY = yPtr->exp;
    if (
        (xPtr->sign != yPtr->sign) && (expX == expY)
            && eq128( xPtr->sig, yPtr->sig )
    ) {
 completeCancellation:
        if (slowfloat_roundingMode == softfloat_round_min) {
            *xPtr = floatXNegativeZero;
        } else {
            *xPtr = floatXPositiveZero;
        }
        return;
    }
    if ( xPtr->isZero ) goto copyY;
    if ( yPtr->isZero ) return;
    expDiff = expX - expY;
    if ( expDiff < 0 ) {
        xPtr->exp = expY;
        if ( expDiff < -120 ) {
            xPtr->sig.v64 = 0;
            xPtr->sig.v0  = 1;
        } else {
            while ( expDiff < 0 ) {
                ++expDiff;
                xPtr->sig = shortShiftRightJam128( xPtr->sig, 1 );
            }
        }
        if ( xPtr->sign != yPtr->sign ) xPtr->sig = neg128( xPtr->sig );
        xPtr->sign = yPtr->sign;
        xPtr->sig = add128( xPtr->sig, yPtr->sig );
    } else {
        sigY = yPtr->sig;
        if ( 120 < expDiff ) {
            sigY.v64 = 0;
            sigY.v0  = 1;
        } else {
            while ( 0 < expDiff ) {
                --expDiff;
                sigY = shortShiftRightJam128( sigY, 1 );
            }
        }
        if ( xPtr->sign != yPtr->sign ) sigY = neg128( sigY );
        xPtr->sig = add128( xPtr->sig, sigY );
    }
    if ( xPtr->sig.v64 & UINT64_C( 0x8000000000000000 ) ) {
        xPtr->sign = !xPtr->sign;
        xPtr->sig = neg128( xPtr->sig );
    }
    return;
 copyY:
    *xPtr = *yPtr;

}

static void floatXMul( struct floatX *xPtr, const struct floatX *yPtr )
{
    struct uint128 sig;
    int bitNum;

    if ( xPtr->isNaN ) return;
    if ( yPtr->isNaN ) {
        xPtr->isNaN = true;
        xPtr->isInf = false;
        xPtr->isZero = false;
        xPtr->sign = yPtr->sign;
        return;
    }
    if ( yPtr->sign ) xPtr->sign = !xPtr->sign;
    if ( xPtr->isInf ) {
        if ( yPtr->isZero ) floatXInvalid( xPtr );
        return;
    }
    if ( yPtr->isInf ) {
        if ( xPtr->isZero ) {
            floatXInvalid( xPtr );
            return;
        }
        xPtr->isInf = true;
        return;
    }
    if ( xPtr->isZero || yPtr->isZero ) {
        if ( xPtr->sign ) {
            *xPtr = floatXNegativeZero;
        } else {
            *xPtr = floatXPositiveZero;
        }
        return;
    }
    xPtr->exp += yPtr->exp;
    sig.v64 = 0;
    sig.v0  = 0;
    for ( bitNum = 0; bitNum < 120; ++bitNum ) {
        sig = shortShiftRightJam128( sig, 1 );
        if ( xPtr->sig.v0 & 1 ) sig = add128( sig, yPtr->sig );
        xPtr->sig = shortShiftRight128( xPtr->sig, 1 );
    }
    if ( UINT64_C( 0x0100000000000000 ) <= sig.v64 ) {
        ++xPtr->exp;
        sig = shortShiftRightJam128( sig, 1 );
    }
    xPtr->sig = sig;

}

static void floatXDiv( struct floatX *xPtr, const struct floatX *yPtr )
{
    struct uint128 sig, negSigY;
    int bitNum;

    if ( xPtr->isNaN ) return;
    if ( yPtr->isNaN ) {
        xPtr->isNaN = true;
        xPtr->isInf = false;
        xPtr->isZero = false;
        xPtr->sign = yPtr->sign;
        return;
    }
    if ( yPtr->sign ) xPtr->sign = !xPtr->sign;
    if ( xPtr->isInf ) {
        if ( yPtr->isInf ) floatXInvalid( xPtr );
        return;
    }
    if ( yPtr->isZero ) {
        if ( xPtr->isZero ) {
            floatXInvalid( xPtr );
            return;
        }
        slowfloat_exceptionFlags |= softfloat_flag_infinite;
        xPtr->isInf = true;
        return;
    }
    if ( xPtr->isZero || yPtr->isInf ) {
        if ( xPtr->sign ) {
            *xPtr = floatXNegativeZero;
        } else {
            *xPtr = floatXPositiveZero;
        }
        return;
    }
    xPtr->exp -= yPtr->exp + 1;
    sig.v64 = 0;
    sig.v0  = 0;
    negSigY = neg128( yPtr->sig );
    for ( bitNum = 0; bitNum < 120; ++bitNum ) {
        if ( le128( yPtr->sig, xPtr->sig ) ) {
            sig.v0 |= 1;
            xPtr->sig = add128( xPtr->sig, negSigY );
        }
        xPtr->sig = shortShiftLeft128( xPtr->sig, 1 );
        sig = shortShiftLeft128( sig, 1 );
    }
    if ( xPtr->sig.v64 || xPtr->sig.v0 ) sig.v0 |= 1;
    xPtr->sig = sig;

}

static void floatXRem( struct floatX *xPtr, const struct floatX *yPtr )
{
    int_fast32_t expX, expY;
    struct uint128 sigY, negSigY;
    bool lastQuotientBit;
    struct uint128 savedSigX;

    if ( xPtr->isNaN ) return;
    if ( yPtr->isNaN ) {
        xPtr->isNaN = true;
        xPtr->isInf = false;
        xPtr->isZero = false;
        xPtr->sign = yPtr->sign;
        return;
    }
    if ( xPtr->isInf || yPtr->isZero ) {
        floatXInvalid( xPtr );
        return;
    }
    if ( xPtr->isZero || yPtr->isInf ) return;
    expX = xPtr->exp;
    expY = yPtr->exp - 1;
    if ( expX < expY ) return;
    sigY = shortShiftLeft128( yPtr->sig, 1 );
    negSigY = neg128( sigY );
    while ( expY < expX ) {
        --expX;
        if ( le128( sigY, xPtr->sig ) ) {
            xPtr->sig = add128( xPtr->sig, negSigY );
        }
        xPtr->sig = shortShiftLeft128( xPtr->sig, 1 );
    }
    xPtr->exp = expX;
    lastQuotientBit = le128( sigY, xPtr->sig );
    if ( lastQuotientBit ) xPtr->sig = add128( xPtr->sig, negSigY );
    savedSigX = xPtr->sig;
    xPtr->sig = neg128( add128( xPtr->sig, negSigY ) );
    if ( lt128( xPtr->sig, savedSigX ) ) {
        xPtr->sign = !xPtr->sign;
    } else if ( lt128( savedSigX, xPtr->sig ) ) {
        goto restoreSavedSigX;
    } else {
        if ( lastQuotientBit ) {
            xPtr->sign = !xPtr->sign;
        } else {
 restoreSavedSigX:
            xPtr->sig = savedSigX;
        }
    }
    if ( !xPtr->sig.v64 && !xPtr->sig.v0 ) xPtr->isZero = true;

}

static void floatXSqrt( struct floatX *xPtr )
{
    struct uint128 sig, bitSig;
    int bitNum;
    struct uint128 savedSigX;

    if ( xPtr->isNaN || xPtr->isZero ) return;
    if ( xPtr->sign ) {
        floatXInvalid( xPtr );
        return;
    }
    if ( xPtr->isInf ) return;
    if ( !(xPtr->exp & 1) ) xPtr->sig = shortShiftRightJam128( xPtr->sig, 1 );
    xPtr->exp >>= 1;
    sig.v64 = 0;
    sig.v0  = 0;
    bitSig.v64 = UINT64_C( 0x0080000000000000 );
    bitSig.v0  = 0;
    for ( bitNum = 0; bitNum < 120; ++bitNum ) {
        savedSigX = xPtr->sig;
        xPtr->sig = add128( xPtr->sig, neg128( sig ) );
        xPtr->sig = shortShiftLeft128( xPtr->sig, 1 );
        xPtr->sig = add128( xPtr->sig, neg128( bitSig ) );
        if ( xPtr->sig.v64 & UINT64_C( 0x8000000000000000 ) ) {
            xPtr->sig = shortShiftLeft128( savedSigX, 1 );
        } else {
            sig.v64 |= bitSig.v64;
            sig.v0  |= bitSig.v0;
        }
        bitSig = shortShiftRightJam128( bitSig, 1 );
    }
    if ( xPtr->sig.v64 || xPtr->sig.v0 ) sig.v0 |= 1;
    xPtr->sig = sig;

}

static bool floatXEq( const struct floatX *xPtr, const struct floatX *yPtr )
{

    if ( xPtr->isNaN || yPtr->isNaN ) return false;
    if ( xPtr->isZero && yPtr->isZero ) return true;
    if ( xPtr->sign != yPtr->sign ) return false;
    if ( xPtr->isInf || yPtr->isInf ) return xPtr->isInf && yPtr->isInf;
    return ( xPtr->exp == yPtr->exp ) && eq128( xPtr->sig, yPtr->sig );

}

static bool floatXLe( const struct floatX *xPtr, const struct floatX *yPtr )
{

    if ( xPtr->isNaN || yPtr->isNaN ) return false;
    if ( xPtr->isZero && yPtr->isZero ) return true;
    if ( xPtr->sign != yPtr->sign ) return xPtr->sign;
    if ( xPtr->sign ) {
        if ( xPtr->isInf || yPtr->isZero ) return true;
        if ( yPtr->isInf || xPtr->isZero ) return false;
        if ( yPtr->exp < xPtr->exp ) return true;
        if ( xPtr->exp < yPtr->exp ) return false;
        return le128( yPtr->sig, xPtr->sig );
    } else {
        if ( yPtr->isInf || xPtr->isZero ) return true;
        if ( xPtr->isInf || yPtr->isZero ) return false;
        if ( xPtr->exp < yPtr->exp ) return true;
        if ( yPtr->exp < xPtr->exp ) return false;
        return le128( xPtr->sig, yPtr->sig );
    }

}

static bool floatXLt( const struct floatX *xPtr, const struct floatX *yPtr )
{

    if ( xPtr->isNaN || yPtr->isNaN ) return false;
    if ( xPtr->isZero && yPtr->isZero ) return false;
    if ( xPtr->sign != yPtr->sign ) return xPtr->sign;
    if ( xPtr->isInf && yPtr->isInf ) return false;
    if ( xPtr->sign ) {
        if ( xPtr->isInf || yPtr->isZero ) return true;
        if ( yPtr->isInf || xPtr->isZero ) return false;
        if ( yPtr->exp < xPtr->exp ) return true;
        if ( xPtr->exp < yPtr->exp ) return false;
        return lt128( yPtr->sig, xPtr->sig );
    } else {
        if ( yPtr->isInf || xPtr->isZero ) return true;
        if ( xPtr->isInf || yPtr->isZero ) return false;
        if ( xPtr->exp < yPtr->exp ) return true;
        if ( yPtr->exp < xPtr->exp ) return false;
        return lt128( xPtr->sig, yPtr->sig );
    }

}

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

#if defined EXTFLOAT80 || defined FLOAT128

#ifdef LITTLEENDIAN
struct uint256 { uint64_t v0, v64, v128, v192; };
#else
struct uint256 { uint64_t v192, v128, v64, v0; };
#endif

static bool eq256M( const struct uint256 *aPtr, const struct uint256 *bPtr )
{

    return
        (aPtr->v192 == bPtr->v192) && (aPtr->v128 == bPtr->v128)
            && (aPtr->v64 == bPtr->v64) && (aPtr->v0 == bPtr->v0);

}

static void shiftLeft1256M( struct uint256 *ptr )
{
    uint64_t dword1, dword2;

    dword1 = ptr->v128;
    ptr->v192 = ptr->v192<<1 | dword1>>63;
    dword2 = ptr->v64;
    ptr->v128 = dword1<<1 | dword2>>63;
    dword1 = ptr->v0;
    ptr->v64 = dword2<<1 | dword1>>63;
    ptr->v0 = dword1<<1;

}

static void shiftRight1256M( struct uint256 *ptr )
{
    uint64_t dword1, dword2;

    dword1 = ptr->v64;
    ptr->v0 = dword1<<63 | ptr->v0>>1;
    dword2 = ptr->v128;
    ptr->v64 = dword2<<63 | dword1>>1;
    dword1 = ptr->v192;
    ptr->v128 = dword1<<63 | dword2>>1;
    ptr->v192 = dword1>>1;

}

static void shiftRight1Jam256M( struct uint256 *ptr )
{
    int extra;

    extra = ptr->v0 & 1;
    shiftRight1256M( ptr );
    ptr->v0 |= extra;

}

static void neg256M( struct uint256 *ptr )
{
    uint64_t v64, v0, v128;

    v64 = ptr->v64;
    v0  = ptr->v0;
    if ( v64 | v0 ) {
        ptr->v192 = ~ptr->v192;
        ptr->v128 = ~ptr->v128;
        if ( v0 ) {
            ptr->v64 = ~v64;
            ptr->v0  = -v0;
        } else {
            ptr->v64 = -v64;
        }
    } else {
        v128 = ptr->v128;
        if ( v128 ) {
            ptr->v192 = ~ptr->v192;
            ptr->v128 = -v128;
        } else {
            ptr->v192 = -ptr->v192;
        }
    }

}

static void add256M( struct uint256 *aPtr, const struct uint256 *bPtr )
{
    uint64_t dwordA, dwordZ;
    unsigned int carry1, carry2;

    dwordA = aPtr->v0;
    dwordZ = dwordA + bPtr->v0;
    carry1 = (dwordZ < dwordA);
    aPtr->v0 = dwordZ;
    dwordA = aPtr->v64;
    dwordZ = dwordA + bPtr->v64;
    carry2 = (dwordZ < dwordA);
    dwordZ += carry1;
    carry2 += (dwordZ < carry1);
    aPtr->v64 = dwordZ;
    dwordA = aPtr->v128;
    dwordZ = dwordA + bPtr->v128;
    carry1 = (dwordZ < dwordA);
    dwordZ += carry2;
    carry1 += (dwordZ < carry2);
    aPtr->v128 = dwordZ;
    aPtr->v192 = aPtr->v192 + bPtr->v192 + carry1;

}

struct floatX256 {
    bool isNaN;
    bool isInf;
    bool isZero;
    bool sign;
    int_fast32_t exp;
    struct uint256 sig;
};

static const struct floatX256 floatX256NaN =
    { true, false, false, false, 0, { 0, 0, 0, 0 } };
static const struct floatX256 floatX256PositiveZero =
    { false, false, true, false, 0, { 0, 0, 0, 0 } };
static const struct floatX256 floatX256NegativeZero =
    { false, false, true, true, 0, { 0, 0, 0, 0 } };

#ifdef FLOAT128

static void f128MToFloatX256( const float128_t *aPtr, struct floatX256 *xPtr )
{
    struct floatX x;

    f128MToFloatX( aPtr, &x );
    xPtr->isNaN  = x.isNaN;
    xPtr->isInf  = x.isInf;
    xPtr->isZero = x.isZero;
    xPtr->sign   = x.sign;
    xPtr->exp    = x.exp;
    xPtr->sig.v192 = x.sig.v64;
    xPtr->sig.v128 = x.sig.v0;
    xPtr->sig.v64  = 0;
    xPtr->sig.v0   = 0;

}

static void floatX256ToF128M( const struct floatX256 *xPtr, float128_t *zPtr )
{
    struct floatX x;
    int_fast32_t expZ;
    struct uint256 sig;

    x.isNaN  = xPtr->isNaN;
    x.isInf  = xPtr->isInf;
    x.isZero = xPtr->isZero;
    x.sign   = xPtr->sign;
    if ( !(x.isNaN | x.isInf | x.isZero) ) {
        expZ = xPtr->exp;
        sig = xPtr->sig;
        while ( !sig.v192 ) {
            expZ -= 64;
            sig.v192 = sig.v128;
            sig.v128 = sig.v64;
            sig.v64  = sig.v0;
            sig.v0   = 0;
        }
        while ( sig.v192 < UINT64_C( 0x0100000000000000 ) ) {
            --expZ;
            shiftLeft1256M( &sig );
        }
        x.exp = expZ;
        x.sig.v64 = sig.v192;
        x.sig.v0 = sig.v128 | ((sig.v64 | sig.v0) != 0);
    }
    floatXToF128M( &x, zPtr );

}

#endif

static void floatX256Invalid( struct floatX256 *xPtr )
{

    slowfloat_exceptionFlags |= softfloat_flag_invalid;
    *xPtr = floatX256NaN;

}

static
void floatX256Add( struct floatX256 *xPtr, const struct floatX256 *yPtr )
{
    int_fast32_t expX, expY, expDiff;
    struct uint256 sigY;

    if ( xPtr->isNaN ) return;
    if ( yPtr->isNaN ) goto copyY;
    if ( xPtr->isInf && yPtr->isInf ) {
        if ( xPtr->sign != yPtr->sign ) floatX256Invalid( xPtr );
        return;
    }
    if ( xPtr->isInf ) return;
    if ( yPtr->isInf ) goto copyY;
    if ( xPtr->isZero && yPtr->isZero ) {
        if ( xPtr->sign == yPtr->sign ) return;
        goto completeCancellation;
    }
    expX = xPtr->exp;
    expY = yPtr->exp;
    if (
        (xPtr->sign != yPtr->sign) && (expX == expY)
            && eq256M( &xPtr->sig, &yPtr->sig )
    ) {
 completeCancellation:
        if (slowfloat_roundingMode == softfloat_round_min) {
            *xPtr = floatX256NegativeZero;
        } else {
            *xPtr = floatX256PositiveZero;
        }
        return;
    }
    if ( xPtr->isZero ) goto copyY;
    if ( yPtr->isZero ) return;
    expDiff = expX - expY;
    if ( expDiff < 0 ) {
        xPtr->exp = expY;
        if ( expDiff < -248 ) {
            xPtr->sig.v192 = 0;
            xPtr->sig.v128 = 0;
            xPtr->sig.v64  = 0;
            xPtr->sig.v0   = 1;
        } else {
            while ( expDiff < 0 ) {
                ++expDiff;
                shiftRight1Jam256M( &xPtr->sig );
            }
        }
        if ( xPtr->sign != yPtr->sign ) neg256M( &xPtr->sig );
        xPtr->sign = yPtr->sign;
        add256M( &xPtr->sig, &yPtr->sig );
    } else {
        sigY = yPtr->sig;
        if ( 248 < expDiff ) {
            sigY.v192 = 0;
            sigY.v128 = 0;
            sigY.v64  = 0;
            sigY.v0   = 1;
        } else {
            while ( 0 < expDiff ) {
                --expDiff;
                shiftRight1Jam256M( &sigY );
            }
        }
        if ( xPtr->sign != yPtr->sign ) neg256M( &sigY );
        add256M( &xPtr->sig, &sigY );
    }
    if ( xPtr->sig.v192 & UINT64_C( 0x8000000000000000 ) ) {
        xPtr->sign = !xPtr->sign;
        neg256M( &xPtr->sig );
    }
    return;
 copyY:
    *xPtr = *yPtr;

}

static
void floatX256Mul( struct floatX256 *xPtr, const struct floatX256 *yPtr )
{
    struct uint256 sig;
    int bitNum;

    if ( xPtr->isNaN ) return;
    if ( yPtr->isNaN ) {
        xPtr->isNaN = true;
        xPtr->isInf = false;
        xPtr->isZero = false;
        xPtr->sign = yPtr->sign;
        return;
    }
    if ( yPtr->sign ) xPtr->sign = !xPtr->sign;
    if ( xPtr->isInf ) {
        if ( yPtr->isZero ) floatX256Invalid( xPtr );
        return;
    }
    if ( yPtr->isInf ) {
        if ( xPtr->isZero ) {
            floatX256Invalid( xPtr );
            return;
        }
        xPtr->isInf = true;
        return;
    }
    if ( xPtr->isZero || yPtr->isZero ) {
        if ( xPtr->sign ) {
            *xPtr = floatX256NegativeZero;
        } else {
            *xPtr = floatX256PositiveZero;
        }
        return;
    }
    xPtr->exp += yPtr->exp;
    sig.v192 = 0;
    sig.v128 = 0;
    sig.v64  = 0;
    sig.v0   = 0;
    for ( bitNum = 0; bitNum < 248; ++bitNum ) {
        shiftRight1Jam256M( &sig );
        if ( xPtr->sig.v0 & 1 ) add256M( &sig, &yPtr->sig );
        shiftRight1256M( &xPtr->sig );
    }
    if ( UINT64_C( 0x0100000000000000 ) <= sig.v192 ) {
        ++xPtr->exp;
        shiftRight1Jam256M( &sig );
    }
    xPtr->sig = sig;

}

#endif

/*----------------------------------------------------------------------------
*----------------------------------------------------------------------------*/

#ifdef FLOAT16

float16_t slow_ui32_to_f16( uint32_t a )
{
    struct floatX x;

    ui32ToFloatX( a, &x );
    return floatXToF16( &x );

}

#endif

float32_t slow_ui32_to_f32( uint32_t a )
{
    struct floatX x;

    ui32ToFloatX( a, &x );
    return floatXToF32( &x );

}

#ifdef FLOAT64

float64_t slow_ui32_to_f64( uint32_t a )
{
    struct floatX x;

    ui32ToFloatX( a, &x );
    return floatXToF64( &x );

}

#endif

#ifdef EXTFLOAT80

void slow_ui32_to_extF80M( uint32_t a, extFloat80_t *zPtr )
{
    struct floatX x;

    ui32ToFloatX( a, &x );
    floatXToExtF80M( &x, zPtr );

}

#endif

#ifdef FLOAT128

void slow_ui32_to_f128M( uint32_t a, float128_t *zPtr )
{
    struct floatX x;

    ui32ToFloatX( a, &x );
    floatXToF128M( &x, zPtr );

}

#endif

#ifdef FLOAT16

float16_t slow_ui64_to_f16( uint64_t a )
{
    struct floatX x;

    ui64ToFloatX( a, &x );
    return floatXToF16( &x );

}

#endif

float32_t slow_ui64_to_f32( uint64_t a )
{
    struct floatX x;

    ui64ToFloatX( a, &x );
    return floatXToF32( &x );

}

#ifdef FLOAT64

float64_t slow_ui64_to_f64( uint64_t a )
{
    struct floatX x;

    ui64ToFloatX( a, &x );
    return floatXToF64( &x );

}

#endif

#ifdef EXTFLOAT80

void slow_ui64_to_extF80M( uint64_t a, extFloat80_t *zPtr )
{
    struct floatX x;

    ui64ToFloatX( a, &x );
    floatXToExtF80M( &x, zPtr );

}

#endif

#ifdef FLOAT128

void slow_ui64_to_f128M( uint64_t a, float128_t *zPtr )
{
    struct floatX x;

    ui64ToFloatX( a, &x );
    floatXToF128M( &x, zPtr );

}

#endif

#ifdef FLOAT16

float16_t slow_i32_to_f16( int32_t a )
{
    struct floatX x;

    i32ToFloatX( a, &x );
    return floatXToF16( &x );

}

#endif

float32_t slow_i32_to_f32( int32_t a )
{
    struct floatX x;

    i32ToFloatX( a, &x );
    return floatXToF32( &x );

}

#ifdef FLOAT64

float64_t slow_i32_to_f64( int32_t a )
{
    struct floatX x;

    i32ToFloatX( a, &x );
    return floatXToF64( &x );

}

#endif

#ifdef EXTFLOAT80

void slow_i32_to_extF80M( int32_t a, extFloat80_t *zPtr )
{
    struct floatX x;

    i32ToFloatX( a, &x );
    floatXToExtF80M( &x, zPtr );

}

#endif

#ifdef FLOAT128

void slow_i32_to_f128M( int32_t a, float128_t *zPtr )
{
    struct floatX x;

    i32ToFloatX( a, &x );
    floatXToF128M( &x, zPtr );

}

#endif

#ifdef FLOAT16

float16_t slow_i64_to_f16( int64_t a )
{
    struct floatX x;

    i64ToFloatX( a, &x );
    return floatXToF16( &x );

}

#endif

float32_t slow_i64_to_f32( int64_t a )
{
    struct floatX x;

    i64ToFloatX( a, &x );
    return floatXToF32( &x );

}

#ifdef FLOAT64

float64_t slow_i64_to_f64( int64_t a )
{
    struct floatX x;

    i64ToFloatX( a, &x );
    return floatXToF64( &x );

}

#endif

#ifdef EXTFLOAT80

void slow_i64_to_extF80M( int64_t a, extFloat80_t *zPtr )
{
    struct floatX x;

    i64ToFloatX( a, &x );
    floatXToExtF80M( &x, zPtr );

}

#endif

#ifdef FLOAT128

void slow_i64_to_f128M( int64_t a, float128_t *zPtr )
{
    struct floatX x;

    i64ToFloatX( a, &x );
    floatXToF128M( &x, zPtr );

}

#endif

#ifdef FLOAT16

uint_fast32_t
 slow_f16_to_ui32( float16_t a, uint_fast8_t roundingMode, bool exact )
{
    struct floatX x;

    f16ToFloatX( a, &x );
    return floatXToUI32( &x, roundingMode, exact );

}

uint_fast64_t
 slow_f16_to_ui64( float16_t a, uint_fast8_t roundingMode, bool exact )
{
    struct floatX x;

    f16ToFloatX( a, &x );
    return floatXToUI64( &x, roundingMode, exact );

}

int_fast32_t
 slow_f16_to_i32( float16_t a, uint_fast8_t roundingMode, bool exact )
{
    struct floatX x;

    f16ToFloatX( a, &x );
    return floatXToI32( &x, roundingMode, exact );

}

int_fast64_t
 slow_f16_to_i64( float16_t a, uint_fast8_t roundingMode, bool exact )
{
    struct floatX x;

    f16ToFloatX( a, &x );
    return floatXToI64( &x, roundingMode, exact );

}

uint_fast32_t slow_f16_to_ui32_r_minMag( float16_t a, bool exact )
{
    struct floatX x;

    f16ToFloatX( a, &x );
    return floatXToUI32( &x, softfloat_round_minMag, exact );

}

uint_fast64_t slow_f16_to_ui64_r_minMag( float16_t a, bool exact )
{
    struct floatX x;

    f16ToFloatX( a, &x );
    return floatXToUI64( &x, softfloat_round_minMag, exact );

}

int_fast32_t slow_f16_to_i32_r_minMag( float16_t a, bool exact )
{
    struct floatX x;

    f16ToFloatX( a, &x );
    return floatXToI32( &x, softfloat_round_minMag, exact );

}

int_fast64_t slow_f16_to_i64_r_minMag( float16_t a, bool exact )
{
    struct floatX x;

    f16ToFloatX( a, &x );
    return floatXToI64( &x, softfloat_round_minMag, exact );

}

float32_t slow_f16_to_f32( float16_t a )
{
    struct floatX x;

    f16ToFloatX( a, &x );
    return floatXToF32( &x );

}

#ifdef FLOAT64

float64_t slow_f16_to_f64( float16_t a )
{
    struct floatX x;

    f16ToFloatX( a, &x );
    return floatXToF64( &x );

}

#endif

#ifdef EXTFLOAT80

void slow_f16_to_extF80M( float16_t a, extFloat80_t *zPtr )
{
    struct floatX x;

    f16ToFloatX( a, &x );
    floatXToExtF80M( &x, zPtr );

}

#endif

#ifdef FLOAT128

void slow_f16_to_f128M( float16_t a, float128_t *zPtr )
{
    struct floatX x;

    f16ToFloatX( a, &x );
    floatXToF128M( &x, zPtr );

}

#endif

float16_t
 slow_f16_roundToInt( float16_t a, uint_fast8_t roundingMode, bool exact )
{
    struct floatX x;

    f16ToFloatX( a, &x );
    floatXRoundToInt( &x, roundingMode, exact );
    return floatXToF16( &x );

}

float16_t slow_f16_add( float16_t a, float16_t b )
{
    struct floatX x, y;

    f16ToFloatX( a, &x );
    f16ToFloatX( b, &y );
    floatXAdd( &x, &y );
    return floatXToF16( &x );

}

float16_t slow_f16_sub( float16_t a, float16_t b )
{
    struct floatX x, y;

    f16ToFloatX( a, &x );
    f16ToFloatX( b, &y );
    y.sign = !y.sign;
    floatXAdd( &x, &y );
    return floatXToF16( &x );


}

float16_t slow_f16_mul( float16_t a, float16_t b )
{
    struct floatX x, y;

    f16ToFloatX( a, &x );
    f16ToFloatX( b, &y );
    floatXMul( &x, &y );
    return floatXToF16( &x );

}

float16_t slow_f16_mulAdd( float16_t a, float16_t b, float16_t c )
{
    struct floatX x, y;

    f16ToFloatX( a, &x );
    f16ToFloatX( b, &y );
    floatXMul( &x, &y );
    f16ToFloatX( c, &y );
    floatXAdd( &x, &y );
    return floatXToF16( &x );

}

float16_t slow_f16_div( float16_t a, float16_t b )
{
    struct floatX x, y;

    f16ToFloatX( a, &x );
    f16ToFloatX( b, &y );
    floatXDiv( &x, &y );
    return floatXToF16( &x );

}

float16_t slow_f16_rem( float16_t a, float16_t b )
{
    struct floatX x, y;

    f16ToFloatX( a, &x );
    f16ToFloatX( b, &y );
    floatXRem( &x, &y );
    return floatXToF16( &x );

}

float16_t slow_f16_sqrt( float16_t a )
{
    struct floatX x;

    f16ToFloatX( a, &x );
    floatXSqrt( &x );
    return floatXToF16( &x );

}

bool slow_f16_eq( float16_t a, float16_t b )
{
    struct floatX x, y;

    f16ToFloatX( a, &x );
    f16ToFloatX( b, &y );
    return floatXEq( &x, &y );

}

bool slow_f16_le( float16_t a, float16_t b )
{
    struct floatX x, y;

    f16ToFloatX( a, &x );
    f16ToFloatX( b, &y );
    if ( x.isNaN || y.isNaN ) {
        slowfloat_exceptionFlags |= softfloat_flag_invalid;
    }
    return floatXLe( &x, &y );

}

bool slow_f16_lt( float16_t a, float16_t b )
{
    struct floatX x, y;

    f16ToFloatX( a, &x );
    f16ToFloatX( b, &y );
    if ( x.isNaN || y.isNaN ) {
        slowfloat_exceptionFlags |= softfloat_flag_invalid;
    }
    return floatXLt( &x, &y );

}

bool slow_f16_eq_signaling( float16_t a, float16_t b )
{
    struct floatX x, y;

    f16ToFloatX( a, &x );
    f16ToFloatX( b, &y );
    if ( x.isNaN || y.isNaN ) {
        slowfloat_exceptionFlags |= softfloat_flag_invalid;
    }
    return floatXEq( &x, &y );

}

bool slow_f16_le_quiet( float16_t a, float16_t b )
{
    struct floatX x, y;

    f16ToFloatX( a, &x );
    f16ToFloatX( b, &y );
    return floatXLe( &x, &y );

}

bool slow_f16_lt_quiet( float16_t a, float16_t b )
{
    struct floatX x, y;

    f16ToFloatX( a, &x );
    f16ToFloatX( b, &y );
    return floatXLt( &x, &y );

}

#endif

uint_fast32_t
 slow_f32_to_ui32( float32_t a, uint_fast8_t roundingMode, bool exact )
{
    struct floatX x;

    f32ToFloatX( a, &x );
    return floatXToUI32( &x, roundingMode, exact );

}

uint_fast64_t
 slow_f32_to_ui64( float32_t a, uint_fast8_t roundingMode, bool exact )
{
    struct floatX x;

    f32ToFloatX( a, &x );
    return floatXToUI64( &x, roundingMode, exact );

}

int_fast32_t
 slow_f32_to_i32( float32_t a, uint_fast8_t roundingMode, bool exact )
{
    struct floatX x;

    f32ToFloatX( a, &x );
    return floatXToI32( &x, roundingMode, exact );

}

int_fast64_t
 slow_f32_to_i64( float32_t a, uint_fast8_t roundingMode, bool exact )
{
    struct floatX x;

    f32ToFloatX( a, &x );
    return floatXToI64( &x, roundingMode, exact );

}

uint_fast32_t slow_f32_to_ui32_r_minMag( float32_t a, bool exact )
{
    struct floatX x;

    f32ToFloatX( a, &x );
    return floatXToUI32( &x, softfloat_round_minMag, exact );

}

uint_fast64_t slow_f32_to_ui64_r_minMag( float32_t a, bool exact )
{
    struct floatX x;

    f32ToFloatX( a, &x );
    return floatXToUI64( &x, softfloat_round_minMag, exact );

}

int_fast32_t slow_f32_to_i32_r_minMag( float32_t a, bool exact )
{
    struct floatX x;

    f32ToFloatX( a, &x );
    return floatXToI32( &x, softfloat_round_minMag, exact );

}

int_fast64_t slow_f32_to_i64_r_minMag( float32_t a, bool exact )
{
    struct floatX x;

    f32ToFloatX( a, &x );
    return floatXToI64( &x, softfloat_round_minMag, exact );

}

#ifdef FLOAT16

float16_t slow_f32_to_f16( float32_t a )
{
    struct floatX x;

    f32ToFloatX( a, &x );
    return floatXToF16( &x );

}

#endif

#ifdef FLOAT64

float64_t slow_f32_to_f64( float32_t a )
{
    struct floatX x;

    f32ToFloatX( a, &x );
    return floatXToF64( &x );

}

#endif

#ifdef EXTFLOAT80

void slow_f32_to_extF80M( float32_t a, extFloat80_t *zPtr )
{
    struct floatX x;

    f32ToFloatX( a, &x );
    floatXToExtF80M( &x, zPtr );

}

#endif

#ifdef FLOAT128

void slow_f32_to_f128M( float32_t a, float128_t *zPtr )
{
    struct floatX x;

    f32ToFloatX( a, &x );
    floatXToF128M( &x, zPtr );

}

#endif

float32_t
 slow_f32_roundToInt( float32_t a, uint_fast8_t roundingMode, bool exact )
{
    struct floatX x;

    f32ToFloatX( a, &x );
    floatXRoundToInt( &x, roundingMode, exact );
    return floatXToF32( &x );

}

float32_t slow_f32_add( float32_t a, float32_t b )
{
    struct floatX x, y;

    f32ToFloatX( a, &x );
    f32ToFloatX( b, &y );
    floatXAdd( &x, &y );
    return floatXToF32( &x );

}

float32_t slow_f32_sub( float32_t a, float32_t b )
{
    struct floatX x, y;

    f32ToFloatX( a, &x );
    f32ToFloatX( b, &y );
    y.sign = !y.sign;
    floatXAdd( &x, &y );
    return floatXToF32( &x );


}

float32_t slow_f32_mul( float32_t a, float32_t b )
{
    struct floatX x, y;

    f32ToFloatX( a, &x );
    f32ToFloatX( b, &y );
    floatXMul( &x, &y );
    return floatXToF32( &x );

}

float32_t slow_f32_mulAdd( float32_t a, float32_t b, float32_t c )
{
    struct floatX x, y;

    f32ToFloatX( a, &x );
    f32ToFloatX( b, &y );
    floatXMul( &x, &y );
    f32ToFloatX( c, &y );
    floatXAdd( &x, &y );
    return floatXToF32( &x );

}

float32_t slow_f32_div( float32_t a, float32_t b )
{
    struct floatX x, y;

    f32ToFloatX( a, &x );
    f32ToFloatX( b, &y );
    floatXDiv( &x, &y );
    return floatXToF32( &x );

}

float32_t slow_f32_rem( float32_t a, float32_t b )
{
    struct floatX x, y;

    f32ToFloatX( a, &x );
    f32ToFloatX( b, &y );
    floatXRem( &x, &y );
    return floatXToF32( &x );

}

float32_t slow_f32_sqrt( float32_t a )
{
    struct floatX x;

    f32ToFloatX( a, &x );
    floatXSqrt( &x );
    return floatXToF32( &x );

}

bool slow_f32_eq( float32_t a, float32_t b )
{
    struct floatX x, y;

    f32ToFloatX( a, &x );
    f32ToFloatX( b, &y );
    return floatXEq( &x, &y );

}

bool slow_f32_le( float32_t a, float32_t b )
{
    struct floatX x, y;

    f32ToFloatX( a, &x );
    f32ToFloatX( b, &y );
    if ( x.isNaN || y.isNaN ) {
        slowfloat_exceptionFlags |= softfloat_flag_invalid;
    }
    return floatXLe( &x, &y );

}

bool slow_f32_lt( float32_t a, float32_t b )
{
    struct floatX x, y;

    f32ToFloatX( a, &x );
    f32ToFloatX( b, &y );
    if ( x.isNaN || y.isNaN ) {
        slowfloat_exceptionFlags |= softfloat_flag_invalid;
    }
    return floatXLt( &x, &y );

}

bool slow_f32_eq_signaling( float32_t a, float32_t b )
{
    struct floatX x, y;

    f32ToFloatX( a, &x );
    f32ToFloatX( b, &y );
    if ( x.isNaN || y.isNaN ) {
        slowfloat_exceptionFlags |= softfloat_flag_invalid;
    }
    return floatXEq( &x, &y );

}

bool slow_f32_le_quiet( float32_t a, float32_t b )
{
    struct floatX x, y;

    f32ToFloatX( a, &x );
    f32ToFloatX( b, &y );
    return floatXLe( &x, &y );

}

bool slow_f32_lt_quiet( float32_t a, float32_t b )
{
    struct floatX x, y;

    f32ToFloatX( a, &x );
    f32ToFloatX( b, &y );
    return floatXLt( &x, &y );

}

#ifdef FLOAT64

uint_fast32_t
 slow_f64_to_ui32( float64_t a, uint_fast8_t roundingMode, bool exact )
{
    struct floatX x;

    f64ToFloatX( a, &x );
    return floatXToUI32( &x, roundingMode, exact );

}

uint_fast64_t
 slow_f64_to_ui64( float64_t a, uint_fast8_t roundingMode, bool exact )
{
    struct floatX x;

    f64ToFloatX( a, &x );
    return floatXToUI64( &x, roundingMode, exact );

}

int_fast32_t
 slow_f64_to_i32( float64_t a, uint_fast8_t roundingMode, bool exact )
{
    struct floatX x;

    f64ToFloatX( a, &x );
    return floatXToI32( &x, roundingMode, exact );

}

int_fast64_t
 slow_f64_to_i64( float64_t a, uint_fast8_t roundingMode, bool exact )
{
    struct floatX x;

    f64ToFloatX( a, &x );
    return floatXToI64( &x, roundingMode, exact );

}

uint_fast32_t slow_f64_to_ui32_r_minMag( float64_t a, bool exact )
{
    struct floatX x;

    f64ToFloatX( a, &x );
    return floatXToUI32( &x, softfloat_round_minMag, exact );

}

uint_fast64_t slow_f64_to_ui64_r_minMag( float64_t a, bool exact )
{
    struct floatX x;

    f64ToFloatX( a, &x );
    return floatXToUI64( &x, softfloat_round_minMag, exact );

}

int_fast32_t slow_f64_to_i32_r_minMag( float64_t a, bool exact )
{
    struct floatX x;

    f64ToFloatX( a, &x );
    return floatXToI32( &x, softfloat_round_minMag, exact );

}

int_fast64_t slow_f64_to_i64_r_minMag( float64_t a, bool exact )
{
    struct floatX x;

    f64ToFloatX( a, &x );
    return floatXToI64( &x, softfloat_round_minMag, exact );

}

#ifdef FLOAT16

float16_t slow_f64_to_f16( float64_t a )
{
    struct floatX x;

    f64ToFloatX( a, &x );
    return floatXToF16( &x );

}

#endif

float32_t slow_f64_to_f32( float64_t a )
{
    struct floatX x;

    f64ToFloatX( a, &x );
    return floatXToF32( &x );

}

#ifdef EXTFLOAT80

void slow_f64_to_extF80M( float64_t a, extFloat80_t *zPtr )
{
    struct floatX x;

    f64ToFloatX( a, &x );
    floatXToExtF80M( &x, zPtr );

}

#endif

#ifdef FLOAT128

void slow_f64_to_f128M( float64_t a, float128_t *zPtr )
{
    struct floatX x;

    f64ToFloatX( a, &x );
    floatXToF128M( &x, zPtr );

}

#endif

float64_t
 slow_f64_roundToInt( float64_t a, uint_fast8_t roundingMode, bool exact )
{
    struct floatX x;

    f64ToFloatX( a, &x );
    floatXRoundToInt( &x, roundingMode, exact );
    return floatXToF64( &x );

}

float64_t slow_f64_add( float64_t a, float64_t b )
{
    struct floatX x, y;

    f64ToFloatX( a, &x );
    f64ToFloatX( b, &y );
    floatXAdd( &x, &y );
    return floatXToF64( &x );

}

float64_t slow_f64_sub( float64_t a, float64_t b )
{
    struct floatX x, y;

    f64ToFloatX( a, &x );
    f64ToFloatX( b, &y );
    y.sign = !y.sign;
    floatXAdd( &x, &y );
    return floatXToF64( &x );

}

float64_t slow_f64_mul( float64_t a, float64_t b )
{
    struct floatX x, y;

    f64ToFloatX( a, &x );
    f64ToFloatX( b, &y );
    floatXMul( &x, &y );
    return floatXToF64( &x );

}

float64_t slow_f64_mulAdd( float64_t a, float64_t b, float64_t c )
{
    struct floatX x, y;

    f64ToFloatX( a, &x );
    f64ToFloatX( b, &y );
    floatXMul( &x, &y );
    f64ToFloatX( c, &y );
    floatXAdd( &x, &y );
    return floatXToF64( &x );

}

float64_t slow_f64_div( float64_t a, float64_t b )
{
    struct floatX x, y;

    f64ToFloatX( a, &x );
    f64ToFloatX( b, &y );
    floatXDiv( &x, &y );
    return floatXToF64( &x );

}

float64_t slow_f64_rem( float64_t a, float64_t b )
{
    struct floatX x, y;

    f64ToFloatX( a, &x );
    f64ToFloatX( b, &y );
    floatXRem( &x, &y );
    return floatXToF64( &x );

}

float64_t slow_f64_sqrt( float64_t a )
{
    struct floatX x;

    f64ToFloatX( a, &x );
    floatXSqrt( &x );
    return floatXToF64( &x );

}

bool slow_f64_eq( float64_t a, float64_t b )
{
    struct floatX x, y;

    f64ToFloatX( a, &x );
    f64ToFloatX( b, &y );
    return floatXEq( &x, &y );

}

bool slow_f64_le( float64_t a, float64_t b )
{
    struct floatX x, y;

    f64ToFloatX( a, &x );
    f64ToFloatX( b, &y );
    if ( x.isNaN || y.isNaN ) {
        slowfloat_exceptionFlags |= softfloat_flag_invalid;
    }
    return floatXLe( &x, &y );

}

bool slow_f64_lt( float64_t a, float64_t b )
{
    struct floatX x, y;

    f64ToFloatX( a, &x );
    f64ToFloatX( b, &y );
    if ( x.isNaN || y.isNaN ) {
        slowfloat_exceptionFlags |= softfloat_flag_invalid;
    }
    return floatXLt( &x, &y );

}

bool slow_f64_eq_signaling( float64_t a, float64_t b )
{
    struct floatX x, y;

    f64ToFloatX( a, &x );
    f64ToFloatX( b, &y );
    if ( x.isNaN || y.isNaN ) {
        slowfloat_exceptionFlags |= softfloat_flag_invalid;
    }
    return floatXEq( &x, &y );

}

bool slow_f64_le_quiet( float64_t a, float64_t b )
{
    struct floatX x, y;

    f64ToFloatX( a, &x );
    f64ToFloatX( b, &y );
    return floatXLe( &x, &y );

}

bool slow_f64_lt_quiet( float64_t a, float64_t b )
{
    struct floatX x, y;

    f64ToFloatX( a, &x );
    f64ToFloatX( b, &y );
    return floatXLt( &x, &y );

}

#endif

#ifdef EXTFLOAT80

uint_fast32_t
 slow_extF80M_to_ui32(
     const extFloat80_t *aPtr, uint_fast8_t roundingMode, bool exact )
{
    struct floatX x;

    extF80MToFloatX( aPtr, &x );
    return floatXToUI32( &x, roundingMode, exact );

}

uint_fast64_t
 slow_extF80M_to_ui64(
     const extFloat80_t *aPtr, uint_fast8_t roundingMode, bool exact )
{
    struct floatX x;

    extF80MToFloatX( aPtr, &x );
    return floatXToUI64( &x, roundingMode, exact );

}

int_fast32_t
 slow_extF80M_to_i32(
     const extFloat80_t *aPtr, uint_fast8_t roundingMode, bool exact )
{
    struct floatX x;

    extF80MToFloatX( aPtr, &x );
    return floatXToI32( &x, roundingMode, exact );

}

int_fast64_t
 slow_extF80M_to_i64(
     const extFloat80_t *aPtr, uint_fast8_t roundingMode, bool exact )
{
    struct floatX x;

    extF80MToFloatX( aPtr, &x );
    return floatXToI64( &x, roundingMode, exact );

}

uint_fast32_t
 slow_extF80M_to_ui32_r_minMag( const extFloat80_t *aPtr, bool exact )
{
    struct floatX x;

    extF80MToFloatX( aPtr, &x );
    return floatXToUI32( &x, softfloat_round_minMag, exact );

}

uint_fast64_t
 slow_extF80M_to_ui64_r_minMag( const extFloat80_t *aPtr, bool exact )
{
    struct floatX x;

    extF80MToFloatX( aPtr, &x );
    return floatXToUI64( &x, softfloat_round_minMag, exact );

}

int_fast32_t
 slow_extF80M_to_i32_r_minMag( const extFloat80_t *aPtr, bool exact )
{
    struct floatX x;

    extF80MToFloatX( aPtr, &x );
    return floatXToI32( &x, softfloat_round_minMag, exact );

}

int_fast64_t
 slow_extF80M_to_i64_r_minMag( const extFloat80_t *aPtr, bool exact )
{
    struct floatX x;

    extF80MToFloatX( aPtr, &x );
    return floatXToI64( &x, softfloat_round_minMag, exact );

}

#ifdef FLOAT16

float16_t slow_extF80M_to_f16( const extFloat80_t *aPtr )
{
    struct floatX x;

    extF80MToFloatX( aPtr, &x );
    return floatXToF16( &x );

}

#endif

float32_t slow_extF80M_to_f32( const extFloat80_t *aPtr )
{
    struct floatX x;

    extF80MToFloatX( aPtr, &x );
    return floatXToF32( &x );

}

#ifdef FLOAT64

float64_t slow_extF80M_to_f64( const extFloat80_t *aPtr )
{
    struct floatX x;

    extF80MToFloatX( aPtr, &x );
    return floatXToF64( &x );

}

#endif

#ifdef FLOAT128

void slow_extF80M_to_f128M( const extFloat80_t *aPtr, float128_t *zPtr )
{
    struct floatX x;

    extF80MToFloatX( aPtr, &x );
    floatXToF128M( &x, zPtr );

}

#endif

void
 slow_extF80M_roundToInt(
     const extFloat80_t *aPtr,
     uint_fast8_t roundingMode,
     bool exact,
     extFloat80_t *zPtr
 )
{
    struct floatX x;

    extF80MToFloatX( aPtr, &x );
    floatXRoundToInt( &x, roundingMode, exact );
    floatXToExtF80M( &x, zPtr );

}

void
 slow_extF80M_add(
     const extFloat80_t *aPtr, const extFloat80_t *bPtr, extFloat80_t *zPtr )
{
    struct floatX x, y;

    extF80MToFloatX( aPtr, &x );
    extF80MToFloatX( bPtr, &y );
    floatXAdd( &x, &y );
    floatXToExtF80M( &x, zPtr );

}

void
 slow_extF80M_sub(
     const extFloat80_t *aPtr, const extFloat80_t *bPtr, extFloat80_t *zPtr )
{
    struct floatX x, y;

    extF80MToFloatX( aPtr, &x );
    extF80MToFloatX( bPtr, &y );
    y.sign = !y.sign;
    floatXAdd( &x, &y );
    floatXToExtF80M( &x, zPtr );

}

void
 slow_extF80M_mul(
     const extFloat80_t *aPtr, const extFloat80_t *bPtr, extFloat80_t *zPtr )
{
    struct floatX x, y;

    extF80MToFloatX( aPtr, &x );
    extF80MToFloatX( bPtr, &y );
    floatXMul( &x, &y );
    floatXToExtF80M( &x, zPtr );

}

void
 slow_extF80M_div(
     const extFloat80_t *aPtr, const extFloat80_t *bPtr, extFloat80_t *zPtr )
{
    struct floatX x, y;

    extF80MToFloatX( aPtr, &x );
    extF80MToFloatX( bPtr, &y );
    floatXDiv( &x, &y );
    floatXToExtF80M( &x, zPtr );

}

void
 slow_extF80M_rem(
     const extFloat80_t *aPtr, const extFloat80_t *bPtr, extFloat80_t *zPtr )
{
    struct floatX x, y;

    extF80MToFloatX( aPtr, &x );
    extF80MToFloatX( bPtr, &y );
    floatXRem( &x, &y );
    floatXToExtF80M( &x, zPtr );

}

void slow_extF80M_sqrt( const extFloat80_t *aPtr, extFloat80_t *zPtr )
{
    struct floatX x;

    extF80MToFloatX( aPtr, &x );
    floatXSqrt( &x );
    floatXToExtF80M( &x, zPtr );

}

bool slow_extF80M_eq( const extFloat80_t *aPtr, const extFloat80_t *bPtr )
{
    struct floatX x, y;

    extF80MToFloatX( aPtr, &x );
    extF80MToFloatX( bPtr, &y );
    return floatXEq( &x, &y );

}

bool slow_extF80M_le( const extFloat80_t *aPtr, const extFloat80_t *bPtr )
{
    struct floatX x, y;

    extF80MToFloatX( aPtr, &x );
    extF80MToFloatX( bPtr, &y );
    if ( x.isNaN || y.isNaN ) {
        slowfloat_exceptionFlags |= softfloat_flag_invalid;
    }
    return floatXLe( &x, &y );

}

bool slow_extF80M_lt( const extFloat80_t *aPtr, const extFloat80_t *bPtr )
{
    struct floatX x, y;

    extF80MToFloatX( aPtr, &x );
    extF80MToFloatX( bPtr, &y );
    if ( x.isNaN || y.isNaN ) {
        slowfloat_exceptionFlags |= softfloat_flag_invalid;
    }
    return floatXLt( &x, &y );

}

bool
 slow_extF80M_eq_signaling(
     const extFloat80_t *aPtr, const extFloat80_t *bPtr )
{
    struct floatX x, y;

    extF80MToFloatX( aPtr, &x );
    extF80MToFloatX( bPtr, &y );
    if ( x.isNaN || y.isNaN ) {
        slowfloat_exceptionFlags |= softfloat_flag_invalid;
    }
    return floatXEq( &x, &y );

}

bool slow_extF80M_le_quiet( const extFloat80_t *aPtr, const extFloat80_t *bPtr )
{
    struct floatX x, y;

    extF80MToFloatX( aPtr, &x );
    extF80MToFloatX( bPtr, &y );
    return floatXLe( &x, &y );

}

bool slow_extF80M_lt_quiet( const extFloat80_t *aPtr, const extFloat80_t *bPtr )
{
    struct floatX x, y;

    extF80MToFloatX( aPtr, &x );
    extF80MToFloatX( bPtr, &y );
    return floatXLt( &x, &y );

}

#endif

#ifdef FLOAT128

uint_fast32_t
 slow_f128M_to_ui32(
     const float128_t *aPtr, uint_fast8_t roundingMode, bool exact )
{
    struct floatX x;

    f128MToFloatX( aPtr, &x );
    return floatXToUI32( &x, roundingMode, exact );

}

uint_fast64_t
 slow_f128M_to_ui64(
     const float128_t *aPtr, uint_fast8_t roundingMode, bool exact )
{
    struct floatX x;

    f128MToFloatX( aPtr, &x );
    return floatXToUI64( &x, roundingMode, exact );

}

int_fast32_t
 slow_f128M_to_i32(
     const float128_t *aPtr, uint_fast8_t roundingMode, bool exact )
{
    struct floatX x;

    f128MToFloatX( aPtr, &x );
    return floatXToI32( &x, roundingMode, exact );

}

int_fast64_t
 slow_f128M_to_i64(
     const float128_t *aPtr, uint_fast8_t roundingMode, bool exact )
{
    struct floatX x;

    f128MToFloatX( aPtr, &x );
    return floatXToI64( &x, roundingMode, exact );

}

uint_fast32_t slow_f128M_to_ui32_r_minMag( const float128_t *aPtr, bool exact )
{
    struct floatX x;

    f128MToFloatX( aPtr, &x );
    return floatXToUI32( &x, softfloat_round_minMag, exact );

}

uint_fast64_t slow_f128M_to_ui64_r_minMag( const float128_t *aPtr, bool exact )
{
    struct floatX x;

    f128MToFloatX( aPtr, &x );
    return floatXToUI64( &x, softfloat_round_minMag, exact );

}

int_fast32_t slow_f128M_to_i32_r_minMag( const float128_t *aPtr, bool exact )
{
    struct floatX x;

    f128MToFloatX( aPtr, &x );
    return floatXToI32( &x, softfloat_round_minMag, exact );

}

int_fast64_t slow_f128M_to_i64_r_minMag( const float128_t *aPtr, bool exact )
{
    struct floatX x;

    f128MToFloatX( aPtr, &x );
    return floatXToI64( &x, softfloat_round_minMag, exact );

}

#ifdef FLOAT16

float16_t slow_f128M_to_f16( const float128_t *aPtr )
{
    struct floatX x;

    f128MToFloatX( aPtr, &x );
    return floatXToF16( &x );

}

#endif

float32_t slow_f128M_to_f32( const float128_t *aPtr )
{
    struct floatX x;

    f128MToFloatX( aPtr, &x );
    return floatXToF32( &x );

}

#ifdef FLOAT64

float64_t slow_f128M_to_f64( const float128_t *aPtr )
{
    struct floatX x;

    f128MToFloatX( aPtr, &x );
    return floatXToF64( &x );

}

#endif

#ifdef EXTFLOAT80

void slow_f128M_to_extF80M( const float128_t *aPtr, extFloat80_t *zPtr )
{
    struct floatX x;

    f128MToFloatX( aPtr, &x );
    floatXToExtF80M( &x, zPtr );

}

#endif

void
 slow_f128M_roundToInt(
     const float128_t *aPtr,
     uint_fast8_t roundingMode,
     bool exact,
     float128_t *zPtr
 )
{
    struct floatX x;

    f128MToFloatX( aPtr, &x );
    floatXRoundToInt( &x, roundingMode, exact );
    floatXToF128M( &x, zPtr );

}

void
 slow_f128M_add(
     const float128_t *aPtr, const float128_t *bPtr, float128_t *zPtr )
{
    struct floatX x, y;

    f128MToFloatX( aPtr, &x );
    f128MToFloatX( bPtr, &y );
    floatXAdd( &x, &y );
    floatXToF128M( &x, zPtr );

}

void
 slow_f128M_sub(
     const float128_t *aPtr, const float128_t *bPtr, float128_t *zPtr )
{
    struct floatX x, y;

    f128MToFloatX( aPtr, &x );
    f128MToFloatX( bPtr, &y );
    y.sign = !y.sign;
    floatXAdd( &x, &y );
    floatXToF128M( &x, zPtr );

}

void
 slow_f128M_mul(
     const float128_t *aPtr, const float128_t *bPtr, float128_t *zPtr )
{
    struct floatX x, y;

    f128MToFloatX( aPtr, &x );
    f128MToFloatX( bPtr, &y );
    floatXMul( &x, &y );
    floatXToF128M( &x, zPtr );

}

void
 slow_f128M_mulAdd(
     const float128_t *aPtr,
     const float128_t *bPtr,
     const float128_t *cPtr,
     float128_t *zPtr
 )
{
    struct floatX256 x, y;

    f128MToFloatX256( aPtr, &x );
    f128MToFloatX256( bPtr, &y );
    floatX256Mul( &x, &y );
    f128MToFloatX256( cPtr, &y );
    floatX256Add( &x, &y );
    floatX256ToF128M( &x, zPtr );

}

void
 slow_f128M_div(
     const float128_t *aPtr, const float128_t *bPtr, float128_t *zPtr )
{
    struct floatX x, y;

    f128MToFloatX( aPtr, &x );
    f128MToFloatX( bPtr, &y );
    floatXDiv( &x, &y );
    floatXToF128M( &x, zPtr );

}

void
 slow_f128M_rem(
     const float128_t *aPtr, const float128_t *bPtr, float128_t *zPtr )
{
    struct floatX x, y;

    f128MToFloatX( aPtr, &x );
    f128MToFloatX( bPtr, &y );
    floatXRem( &x, &y );
    floatXToF128M( &x, zPtr );

}

void slow_f128M_sqrt( const float128_t *aPtr, float128_t *zPtr )
{
    struct floatX x;

    f128MToFloatX( aPtr, &x );
    floatXSqrt( &x );
    floatXToF128M( &x, zPtr );

}

bool slow_f128M_eq( const float128_t *aPtr, const float128_t *bPtr )
{
    struct floatX x, y;

    f128MToFloatX( aPtr, &x );
    f128MToFloatX( bPtr, &y );
    return floatXEq( &x, &y );

}

bool slow_f128M_le( const float128_t *aPtr, const float128_t *bPtr )
{
    struct floatX x, y;

    f128MToFloatX( aPtr, &x );
    f128MToFloatX( bPtr, &y );
    if ( x.isNaN || y.isNaN ) {
        slowfloat_exceptionFlags |= softfloat_flag_invalid;
    }
    return floatXLe( &x, &y );

}

bool slow_f128M_lt( const float128_t *aPtr, const float128_t *bPtr )
{
    struct floatX x, y;

    f128MToFloatX( aPtr, &x );
    f128MToFloatX( bPtr, &y );
    if ( x.isNaN || y.isNaN ) {
        slowfloat_exceptionFlags |= softfloat_flag_invalid;
    }
    return floatXLt( &x, &y );

}

bool slow_f128M_eq_signaling( const float128_t *aPtr, const float128_t *bPtr )
{
    struct floatX x, y;

    f128MToFloatX( aPtr, &x );
    f128MToFloatX( bPtr, &y );
    if ( x.isNaN || y.isNaN ) {
        slowfloat_exceptionFlags |= softfloat_flag_invalid;
    }
    return floatXEq( &x, &y );

}

bool slow_f128M_le_quiet( const float128_t *aPtr, const float128_t *bPtr )
{
    struct floatX x, y;

    f128MToFloatX( aPtr, &x );
    f128MToFloatX( bPtr, &y );
    return floatXLe( &x, &y );

}

bool slow_f128M_lt_quiet( const float128_t *aPtr, const float128_t *bPtr )
{
    struct floatX x, y;

    f128MToFloatX( aPtr, &x );
    f128MToFloatX( bPtr, &y );
    return floatXLt( &x, &y );

}

#endif

