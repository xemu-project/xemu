/*
 *  Test program for MSA instruction MUL_Q.W
 *
 *  Copyright (C) 2019  Wave Computing, Inc.
 *  Copyright (C) 2019  Aleksandar Markovic <amarkovic@wavecomp.com>
 *  Copyright (C) 2019  RT-RK Computer Based Systems LLC
 *  Copyright (C) 2019  Mateja Marjanovic <mateja.marjanovic@rt-rk.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include <sys/time.h>
#include <stdint.h>

#include "../../../../include/wrappers_msa.h"
#include "../../../../include/test_inputs_128.h"
#include "../../../../include/test_utils_128.h"

#define TEST_COUNT_TOTAL (                                                \
            (PATTERN_INPUTS_SHORT_COUNT) * (PATTERN_INPUTS_SHORT_COUNT) + \
            (RANDOM_INPUTS_SHORT_COUNT) * (RANDOM_INPUTS_SHORT_COUNT))


int32_t main(void)
{
    char *isa_ase_name = "MSA";
    char *group_name = "Fixed Multiply";
    char *instruction_name =  "MUL_Q.W";
    int32_t ret;
    uint32_t i, j;
    struct timeval start, end;
    double elapsed_time;

    uint64_t b128_result[TEST_COUNT_TOTAL][2];
    uint64_t b128_expect[TEST_COUNT_TOTAL][2] = {
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },    /*   0  */
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0xffffffffffffffffULL, 0xffffffffffffffffULL, },
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0xffffffffffffffffULL, 0xffffffffffffffffULL, },
        { 0x0000000000000000ULL, 0xffffffff00000000ULL, },
        { 0xffffffffffffffffULL, 0x00000000ffffffffULL, },
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },    /*   8  */
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },    /*  16  */
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0x38e38e3938e38e39ULL, 0x38e38e3938e38e39ULL, },
        { 0xc71c71c6c71c71c6ULL, 0xc71c71c6c71c71c6ULL, },
        { 0x2222222222222222ULL, 0x2222222222222222ULL, },
        { 0xddddddddddddddddULL, 0xddddddddddddddddULL, },
        { 0x12f684be4bda12f7ULL, 0xda12f68512f684beULL, },
        { 0xed097b42b425ed09ULL, 0x25ed097bed097b42ULL, },
        { 0xffffffffffffffffULL, 0xffffffffffffffffULL, },    /*  24  */
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0xc71c71c6c71c71c6ULL, 0xc71c71c6c71c71c6ULL, },
        { 0x38e38e3838e38e38ULL, 0x38e38e3838e38e38ULL, },
        { 0xddddddddddddddddULL, 0xddddddddddddddddULL, },
        { 0x2222222122222221ULL, 0x2222222122222221ULL, },
        { 0xed097b42b425ed09ULL, 0x25ed097aed097b42ULL, },
        { 0x12f684bd4bda12f5ULL, 0xda12f68412f684bdULL, },
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },    /*  32  */
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0x2222222222222222ULL, 0x2222222222222222ULL, },
        { 0xddddddddddddddddULL, 0xddddddddddddddddULL, },
        { 0x147ae148147ae148ULL, 0x147ae148147ae148ULL, },
        { 0xeb851eb8eb851eb8ULL, 0xeb851eb8eb851eb8ULL, },
        { 0x0b60b60b2d82d82eULL, 0xe93e93e90b60b60bULL, },
        { 0xf49f49f4d27d27d2ULL, 0x16c16c17f49f49f4ULL, },
        { 0xffffffffffffffffULL, 0xffffffffffffffffULL, },    /*  40  */
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0xddddddddddddddddULL, 0xddddddddddddddddULL, },
        { 0x2222222122222221ULL, 0x2222222122222221ULL, },
        { 0xeb851eb8eb851eb8ULL, 0xeb851eb8eb851eb8ULL, },
        { 0x147ae147147ae147ULL, 0x147ae147147ae147ULL, },
        { 0xf49f49f4d27d27d2ULL, 0x16c16c16f49f49f4ULL, },
        { 0x0b60b60b2d82d82dULL, 0xe93e93e90b60b60bULL, },
        { 0x0000000000000000ULL, 0xffffffff00000000ULL, },    /*  48  */
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0x12f684be4bda12f7ULL, 0xda12f68512f684beULL, },
        { 0xed097b42b425ed09ULL, 0x25ed097aed097b42ULL, },
        { 0x0b60b60b2d82d82eULL, 0xe93e93e90b60b60bULL, },
        { 0xf49f49f4d27d27d2ULL, 0x16c16c16f49f49f4ULL, },
        { 0x06522c3f6522c3f3ULL, 0x1948b0fc06522c3fULL, },
        { 0xf9add3c09add3c0dULL, 0xe6b74f03f9add3c0ULL, },
        { 0xffffffffffffffffULL, 0x00000000ffffffffULL, },    /*  56  */
        { 0x0000000000000000ULL, 0x0000000000000000ULL, },
        { 0xed097b42b425ed09ULL, 0x25ed097bed097b42ULL, },
        { 0x12f684bd4bda12f5ULL, 0xda12f68412f684bdULL, },
        { 0xf49f49f4d27d27d2ULL, 0x16c16c17f49f49f4ULL, },
        { 0x0b60b60b2d82d82dULL, 0xe93e93e90b60b60bULL, },
        { 0xf9add3c09add3c0dULL, 0xe6b74f03f9add3c0ULL, },
        { 0x06522c3f6522c3f1ULL, 0x1948b0fc06522c3fULL, },
        { 0x6fb7e8890cbdc0d2ULL, 0x2c6b144600049a04ULL, },    /*  64  */
        { 0x03fa514e1879c701ULL, 0x0b2c6ca9ffbf8ac6ULL, },
        { 0x4e252086e9daefbfULL, 0x1779189301015a34ULL, },
        { 0x9713a7171db7f3a5ULL, 0xbccfb4690107236fULL, },
        { 0x03fa514e1879c701ULL, 0x0b2c6ca9ffbf8ac6ULL, },
        { 0x002442012f047611ULL, 0x02cf8c140386e68eULL, },
        { 0x02c84b87d575d121ULL, 0x05e79a8af1eb1c52ULL, },
        { 0xfc439edc3916c1e4ULL, 0xef19389cf19a0fddULL, },
        { 0x4e252086e9daefbfULL, 0x1779189301015a34ULL, },    /*  72  */
        { 0x02c84b87d575d121ULL, 0x05e79a8af1eb1c52ULL, },
        { 0x36a93aff267d11c3ULL, 0x0c6788643838c14cULL, },
        { 0xb69baa39cc590fcdULL, 0xdc7e6df7397c58d9ULL, },
        { 0x9713a7171db7f3a5ULL, 0xbccfb4690107236fULL, },
        { 0xfc439edc3916c1e4ULL, 0xef19389cf19a0fddULL, },
        { 0xb69baa39cc590fcdULL, 0xdc7e6df7397c58d9ULL, },
        { 0x628a97e4455157d3ULL, 0x65a1c5e13ac736e1ULL, },
};

    reset_msa_registers();

    gettimeofday(&start, NULL);

    for (i = 0; i < PATTERN_INPUTS_SHORT_COUNT; i++) {
        for (j = 0; j < PATTERN_INPUTS_SHORT_COUNT; j++) {
            do_msa_MUL_Q_W(b128_pattern[i], b128_pattern[j],
                           b128_result[PATTERN_INPUTS_SHORT_COUNT * i + j]);
        }
    }

    for (i = 0; i < RANDOM_INPUTS_SHORT_COUNT; i++) {
        for (j = 0; j < RANDOM_INPUTS_SHORT_COUNT; j++) {
            do_msa_MUL_Q_W(b128_random[i], b128_random[j],
                           b128_result[((PATTERN_INPUTS_SHORT_COUNT) *
                                        (PATTERN_INPUTS_SHORT_COUNT)) +
                                       RANDOM_INPUTS_SHORT_COUNT * i + j]);
        }
    }

    gettimeofday(&end, NULL);

    elapsed_time = (end.tv_sec - start.tv_sec) * 1000.0;
    elapsed_time += (end.tv_usec - start.tv_usec) / 1000.0;

    ret = check_results_128(isa_ase_name, group_name, instruction_name,
                            TEST_COUNT_TOTAL, elapsed_time,
                            &b128_result[0][0], &b128_expect[0][0]);

    return ret;
}
