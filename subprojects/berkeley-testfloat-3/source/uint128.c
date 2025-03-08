
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
#include "uint128.h"

struct uint128 shortShiftLeft128( struct uint128 a, int count )
{
    struct uint128 z;

    z.v64 = a.v64<<count | a.v0>>(-count & 63);
    z.v0 = a.v0<<count;
    return z;

}

struct uint128 shortShiftRight128( struct uint128 a, int count )
{
    struct uint128 z;

    z.v64 = a.v64>>count;
    z.v0 = a.v64<<(-count & 63) | a.v0>>count;
    return z;

}

struct uint128 shortShiftRightJam128( struct uint128 a, int count )
{
    int negCount;
    struct uint128 z;

    negCount = -count;
    z.v64 = a.v64>>count;
    z.v0 =
        a.v64<<(negCount & 63) | a.v0>>count
            | ((uint64_t) (a.v0<<(negCount & 63)) != 0);
    return z;

}

struct uint128 neg128( struct uint128 a )
{

    if ( a.v0 ) {
        a.v64 = ~a.v64;
        a.v0 = -a.v0;
    } else {
        a.v64 = -a.v64;
    }
    return a;

}

struct uint128 add128( struct uint128 a, struct uint128 b )
{
    struct uint128 z;

    z.v0 = a.v0 + b.v0;
    z.v64 = a.v64 + b.v64 + (z.v0 < a.v0);
    return z;

}

