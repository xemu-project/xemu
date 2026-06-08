
/*============================================================================

This C source file is part of TestFloat, Release 3e, a package of programs for
testing the correctness of floating-point arithmetic complying with the IEEE
Standard for Floating-Point, by John R. Hauser.

Copyright 2011, 2012, 2013, 2014, 2017, 2018 The Regents of the University of
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
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include "platform.h"
#include "verCases.h"

const char *verCases_functionNamePtr;
uint_fast8_t verCases_roundingPrecision = 0;
int verCases_roundingCode = 0;
int verCases_tininessCode = 0;
bool verCases_usesExact = false;
bool verCases_exact;
bool verCases_checkNaNs = false;
bool verCases_checkInvInts = false;
uint_fast32_t verCases_maxErrorCount = 0;
bool verCases_errorStop = false;

volatile sig_atomic_t verCases_stop = false;
int verCases_verbosity = 1;

bool verCases_anyErrors = false;

void verCases_exitWithStatus( void )
{

    exit( verCases_anyErrors ? EXIT_FAILURE : EXIT_SUCCESS );

}

uint_fast32_t verCases_tenThousandsCount, verCases_errorCount;

void verCases_writeTestsPerformed( int count )
{

    if ( verCases_verbosity) {
        if ( verCases_tenThousandsCount ) {
            fprintf(
                stderr,
                "\r%lu%04d tests performed",
                (unsigned long) verCases_tenThousandsCount,
                count
                );
        } else {
            fprintf( stderr, "\r%d tests performed", count );
        }
    }
    if ( verCases_errorCount ) {
        fprintf(
            stderr,
            "; %lu error%s found.\n",
            (unsigned long) verCases_errorCount,
            (verCases_errorCount == 1) ? "" : "s"
        );
    } else {
        if ( verCases_verbosity) {
            fputs( ".\n", stderr );
        }
        if ( verCases_tenThousandsCount ) {
            fprintf(
                stdout,
                "In %lu%04d tests, no errors found in ",
                (unsigned long) verCases_tenThousandsCount,
                count
            );
        } else {
            fprintf( stdout, "In %d tests, no errors found in ", count );
        }
        verCases_writeFunctionName( stdout );
        fputs( ".\n", stdout );
        fflush( stdout );
    }

}

void verCases_perTenThousand( void )
{

    ++verCases_tenThousandsCount;
    if ( verCases_stop ) {
        verCases_writeTestsPerformed( 0 );
        verCases_exitWithStatus();
    }
    if (verCases_verbosity) {
        fprintf(
            stderr, "\r%3lu0000", (unsigned long) verCases_tenThousandsCount );
    }
}

void verCases_writeErrorFound( int count )
{

    fputc( '\r', stderr );
    if ( verCases_errorCount == 1 ) {
        fputs( "Errors found in ", stdout );
        verCases_writeFunctionName( stdout );
        fputs( ":\n", stdout );
    }
    if ( verCases_stop ) {
        verCases_writeTestsPerformed( count );
        verCases_exitWithStatus();
    }
    verCases_anyErrors = true;

}

