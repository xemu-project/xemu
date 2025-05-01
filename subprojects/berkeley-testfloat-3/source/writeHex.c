
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
#include <stdio.h>
#include "platform.h"
#include "uint128.h"
#include "softfloat.h"
#include "writeHex.h"

void writeHex_bool( bool a, char sepChar )
{

    fputc( a ? '1' : '0', stdout );
    if ( sepChar ) fputc( sepChar, stdout );

}

void writeHex_ui8( uint_fast8_t a, char sepChar )
{
    int digit;

    digit = a>>4 & 0xF;
    if ( 9 < digit ) digit += 'A' - ('0' + 10);
    fputc( '0' + digit, stdout );
    digit = a & 0xF;
    if ( 9 < digit ) digit += 'A' - ('0' + 10);
    fputc( '0' + digit, stdout );
    if ( sepChar ) fputc( sepChar, stdout );

}

static void writeHex_ui12( uint_fast16_t a, char sepChar )
{
    int digit;

    digit = a>>8 & 0xF;
    if ( 9 < digit ) digit += 'A' - ('0' + 10);
    fputc( '0' + digit, stdout );
    digit = a>>4 & 0xF;
    if ( 9 < digit ) digit += 'A' - ('0' + 10);
    fputc( '0' + digit, stdout );
    digit = a & 0xF;
    if ( 9 < digit ) digit += 'A' - ('0' + 10);
    fputc( '0' + digit, stdout );
    if ( sepChar ) fputc( sepChar, stdout );

}

void writeHex_ui16( uint_fast16_t a, char sepChar )
{
    int digit;

    digit = a>>12 & 0xF;
    if ( 9 < digit ) digit += 'A' - ('0' + 10);
    fputc( '0' + digit, stdout );
    digit = a>>8 & 0xF;
    if ( 9 < digit ) digit += 'A' - ('0' + 10);
    fputc( '0' + digit, stdout );
    digit = a>>4 & 0xF;
    if ( 9 < digit ) digit += 'A' - ('0' + 10);
    fputc( '0' + digit, stdout );
    digit = a & 0xF;
    if ( 9 < digit ) digit += 'A' - ('0' + 10);
    fputc( '0' + digit, stdout );
    if ( sepChar ) fputc( sepChar, stdout );

}

void writeHex_ui32( uint_fast32_t a, char sepChar )
{

    writeHex_ui16( a>>16, 0 );
    writeHex_ui16( a, sepChar );

}

void writeHex_ui64( uint_fast64_t a, char sepChar )
{

    writeHex_ui32( a>>32, 0 );
    writeHex_ui32( a, sepChar );

}

#ifdef FLOAT16

void writeHex_f16( float16_t a, char sepChar )
{
    union { uint16_t ui; float16_t f; } uA;
    uint_fast16_t uiA;

    uA.f = a;
    uiA = uA.ui;
    fputc( uiA & 0x8000 ? '-' : '+', stdout );
    writeHex_ui8( uiA>>10 & 0x1F, 0 );
    fputc( '.', stdout );
    fputc( '0' + (uiA>>8 & 3), stdout );
    writeHex_ui8( uiA, sepChar );

}

#endif

void writeHex_f32( float32_t a, char sepChar )
{
    union { uint32_t ui; float32_t f; } uA;
    uint_fast32_t uiA;

    uA.f = a;
    uiA = uA.ui;
    fputc( uiA & 0x80000000 ? '-' : '+', stdout );
    writeHex_ui8( uiA>>23, 0 );
    fputc( '.', stdout );
    writeHex_ui8( uiA>>16 & 0x7F, 0 );
    writeHex_ui16( uiA, sepChar );

}

#ifdef FLOAT64

void writeHex_f64( float64_t a, char sepChar )
{
    union { uint64_t ui; float64_t f; } uA;
    uint_fast64_t uiA;

    uA.f = a;
    uiA = uA.ui;
    fputc( uiA & UINT64_C( 0x8000000000000000 ) ? '-' : '+', stdout );
    writeHex_ui12( uiA>>52 & 0x7FF, 0 );
    fputc( '.', stdout );
    writeHex_ui12( uiA>>40, 0 );
    writeHex_ui8( uiA>>32, 0 );
    writeHex_ui32( uiA, sepChar );

}

#endif

#ifdef EXTFLOAT80

void writeHex_extF80M( const extFloat80_t *aPtr, char sepChar )
{
    const struct extFloat80M *aSPtr;
    uint_fast16_t uiA64;

    aSPtr = (const struct extFloat80M *) aPtr;
    uiA64 = aSPtr->signExp;
    fputc( uiA64 & 0x8000 ? '-' : '+', stdout );
    writeHex_ui16( uiA64 & 0x7FFF, 0 );
    fputc( '.', stdout );
    writeHex_ui64( aSPtr->signif, sepChar );

}

#endif

#ifdef FLOAT128

void writeHex_f128M( const float128_t *aPtr, char sepChar )
{
    const struct uint128 *uiAPtr;
    uint_fast64_t uiA64;

    uiAPtr = (const struct uint128 *) aPtr;
    uiA64 = uiAPtr->v64;
    fputc( uiA64 & UINT64_C( 0x8000000000000000 ) ? '-' : '+', stdout );
    writeHex_ui16( uiA64>>48 & 0x7FFF, 0 );
    fputc( '.', stdout );
    writeHex_ui16( uiA64>>32, 0 );
    writeHex_ui32( uiA64, 0 );
    writeHex_ui64( uiAPtr->v0, sepChar );

}

#endif

void writeHex_softfloat_flags( uint_fast8_t flags, char sepChar )
{

    fputc( flags & softfloat_flag_invalid   ? 'v' : '.', stdout );
    fputc( flags & softfloat_flag_infinite  ? 'i' : '.', stdout );
    fputc( flags & softfloat_flag_overflow  ? 'o' : '.', stdout );
    fputc( flags & softfloat_flag_underflow ? 'u' : '.', stdout );
    fputc( flags & softfloat_flag_inexact   ? 'x' : '.', stdout );
    if ( sepChar ) fputc( sepChar, stdout );

}

