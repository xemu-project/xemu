
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

#include <stdint.h>
#include <stdlib.h>
#include "platform.h"
#include "random.h"

uint_fast8_t random_ui8( void )
{

    return rand()>>4 & 0xFF;

}

uint_fast16_t random_ui16( void )
{

    return (rand() & 0x0FF0)<<4 | (rand()>>4 & 0xFF);

}

uint_fast32_t random_ui32( void )
{

    return
          (uint_fast32_t) (rand() & 0x0FF0)<<20
        | (uint_fast32_t) (rand() & 0x0FF0)<<12
        | (rand() & 0x0FF0)<<4
        | (rand()>>4 & 0xFF);

}

uint_fast64_t random_ui64( void )
{

    return (uint_fast64_t) random_ui32()<<32 | random_ui32();

}

uint_fast8_t randomN_ui8( uint_fast8_t N )
{
    uint_fast8_t scale, z;

    scale = 0;
    while ( N < 0x80 ) {
        ++scale;
        N <<= 1;
    }
    do {
        z = random_ui8();
    } while ( N <= z );
    return z>>scale;

}

uint_fast16_t randomN_ui16( uint_fast16_t N )
{
    uint_fast16_t scale, z;

    scale = 0;
    while ( N < 0x8000 ) {
        ++scale;
        N <<= 1;
    }
    do {
        z = random_ui16();
    } while ( N <= z );
    return z>>scale;

}

uint_fast32_t randomN_ui32( uint_fast32_t N )
{
    uint_fast32_t scale, z;

    scale = 0;
    while ( N < 0x8000 ) {
        ++scale;
        N <<= 1;
    }
    do {
        z = random_ui32();
    } while ( N <= z );
    return z>>scale;

}

uint_fast64_t randomN_ui64( uint_fast64_t N )
{
    uint_fast64_t scale, z;

    scale = 0;
    while ( N < 0x8000 ) {
        ++scale;
        N <<= 1;
    }
    do {
        z = random_ui64();
    } while ( N <= z );
    return z>>scale;

}

