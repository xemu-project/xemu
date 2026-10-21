
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
#include <stdio.h>
#include "platform.h"
#include "readHex.h"

bool readHex_bool( bool *aPtr, char sepChar )
{
    int i;
    bool a;

    i = fgetc( stdin );
    if ( (i == EOF) || (i < '0') || ('1' < i) ) return false;
    a = i - '0';
    if ( sepChar ) {
        i = fgetc( stdin );
        if ( (sepChar != '\n') || (i != '\r') ) {
            if ( i != sepChar ) return false;
        }
    }
    *aPtr = a;
    return true;

}

bool readHex_ui8( uint_least8_t *aPtr, char sepChar )
{
    int i;
    uint_fast8_t a;

    i = fgetc( stdin );
    if ( i == EOF ) return false;
    if ( ('0' <= i) && (i <= '9') ) {
        i -= '0';
    } else if ( ('A' <= i) && (i <= 'F') ) {
        i -= 'A' - 10;
    } else if ( ('a' <= i) && (i <= 'f') ) {
        i -= 'a' - 10;
    } else {
        return false;
    }
    a = i<<4;
    i = fgetc( stdin );
    if ( i == EOF ) return false;
    if ( ('0' <= i) && (i <= '9') ) {
        i -= '0';
    } else if ( ('A' <= i) && (i <= 'F') ) {
        i -= 'A' - 10;
    } else if ( ('a' <= i) && (i <= 'f') ) {
        i -= 'a' - 10;
    } else {
        return false;
    }
    a |= i;
    if ( sepChar ) {
        i = fgetc( stdin );
        if ( (sepChar != '\n') || (i != '\r') ) {
            if ( i != sepChar ) return false;
        }
    }
    *aPtr = a;
    return true;

}

bool readHex_ui16( uint16_t *aPtr, char sepChar )
{
    int i;
    uint_fast16_t a;

    i = fgetc( stdin );
    if ( i == EOF ) return false;
    if ( ('0' <= i) && (i <= '9') ) {
        i -= '0';
    } else if ( ('A' <= i) && (i <= 'F') ) {
        i -= 'A' - 10;
    } else if ( ('a' <= i) && (i <= 'f') ) {
        i -= 'a' - 10;
    } else {
        return false;
    }
    a = (uint_fast16_t) i<<12;
    i = fgetc( stdin );
    if ( i == EOF ) return false;
    if ( ('0' <= i) && (i <= '9') ) {
        i -= '0';
    } else if ( ('A' <= i) && (i <= 'F') ) {
        i -= 'A' - 10;
    } else if ( ('a' <= i) && (i <= 'f') ) {
        i -= 'a' - 10;
    } else {
        return false;
    }
    a |= (uint_fast16_t) i<<8;
    i = fgetc( stdin );
    if ( i == EOF ) return false;
    if ( ('0' <= i) && (i <= '9') ) {
        i -= '0';
    } else if ( ('A' <= i) && (i <= 'F') ) {
        i -= 'A' - 10;
    } else if ( ('a' <= i) && (i <= 'f') ) {
        i -= 'a' - 10;
    } else {
        return false;
    }
    a |= (uint_fast16_t) i<<4;
    i = fgetc( stdin );
    if ( i == EOF ) return false;
    if ( ('0' <= i) && (i <= '9') ) {
        i -= '0';
    } else if ( ('A' <= i) && (i <= 'F') ) {
        i -= 'A' - 10;
    } else if ( ('a' <= i) && (i <= 'f') ) {
        i -= 'a' - 10;
    } else {
        return false;
    }
    a |= i;
    if ( sepChar ) {
        i = fgetc( stdin );
        if ( (sepChar != '\n') || (i != '\r') ) {
            if ( i != sepChar ) return false;
        }
    }
    *aPtr = a;
    return true;

}

bool readHex_ui32( uint32_t *aPtr, char sepChar )
{
    uint16_t v16, v0;

    if ( ! readHex_ui16( &v16, 0 ) || ! readHex_ui16( &v0, sepChar ) ) {
        return false;
    }
    *aPtr = (uint_fast32_t) v16<<16 | v0;
    return true;

}

bool readHex_ui64( uint64_t *aPtr, char sepChar )
{
    uint32_t v32, v0;

    if ( ! readHex_ui32( &v32, 0 ) || ! readHex_ui32( &v0, sepChar ) ) {
        return false;
    }
    *aPtr = (uint_fast64_t) v32<<32 | v0;
    return true;

}

