/*
 *  Copyright(c) 2019-2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

int err;

#include "hex_test.h"

#define NBITS          8
#define SIZE           (1 << NBITS)

int64_t       dbuf[SIZE] __attribute__((aligned(1 << 16))) = {0};
int32_t       wbuf[SIZE] __attribute__((aligned(1 << 16))) = {0};
int16_t       hbuf[SIZE] __attribute__((aligned(1 << 16))) = {0};
uint8_t       bbuf[SIZE] __attribute__((aligned(1 << 16))) = {0};

/*
 * We use the C preporcessor to deal with the combinations of types
 */

#define BREV_LOAD(SZ, RES, ADDR, INC) \
    __asm__( \
        "m0 = %2\n\t" \
        "%0 = mem" #SZ "(%1++m0:brev)\n\t" \
        : "=r"(RES), "+r"(ADDR) \
        : "r"(INC) \
        : "m0")

#define BREV_LOAD_b(RES, ADDR, INC) \
    BREV_LOAD(b, RES, ADDR, INC)
#define BREV_LOAD_ub(RES, ADDR, INC) \
    BREV_LOAD(ub, RES, ADDR, INC)
#define BREV_LOAD_h(RES, ADDR, INC) \
    BREV_LOAD(h, RES, ADDR, INC)
#define BREV_LOAD_uh(RES, ADDR, INC) \
    BREV_LOAD(uh, RES, ADDR, INC)
#define BREV_LOAD_w(RES, ADDR, INC) \
    BREV_LOAD(w, RES, ADDR, INC)
#define BREV_LOAD_d(RES, ADDR, INC) \
    BREV_LOAD(d, RES, ADDR, INC)

#define BREV_STORE(SZ, PART, ADDR, VAL, INC) \
    __asm__( \
        "m0 = %2\n\t" \
        "mem" #SZ "(%0++m0:brev) = %1" PART "\n\t" \
        : "+r"(ADDR) \
        : "r"(VAL), "r"(INC) \
        : "m0", "memory")

#define BREV_STORE_b(ADDR, VAL, INC) \
    BREV_STORE(b, "", ADDR, VAL, INC)
#define BREV_STORE_h(ADDR, VAL, INC) \
    BREV_STORE(h, "", ADDR, VAL, INC)
#define BREV_STORE_f(ADDR, VAL, INC) \
    BREV_STORE(h, ".H", ADDR, VAL, INC)
#define BREV_STORE_w(ADDR, VAL, INC) \
    BREV_STORE(w, "", ADDR, VAL, INC)
#define BREV_STORE_d(ADDR, VAL, INC) \
    BREV_STORE(d, "", ADDR, VAL, INC)

#define BREV_STORE_NEW(SZ, ADDR, VAL, INC) \
    __asm__( \
        "m0 = %2\n\t" \
        "{\n\t" \
        "    r5 = %1\n\t" \
        "    mem" #SZ "(%0++m0:brev) = r5.new\n\t" \
        "}\n\t" \
        : "+r"(ADDR) \
        : "r"(VAL), "r"(INC) \
        : "r5", "m0", "memory")

#define BREV_STORE_bnew(ADDR, VAL, INC) \
    BREV_STORE_NEW(b, ADDR, VAL, INC)
#define BREV_STORE_hnew(ADDR, VAL, INC) \
    BREV_STORE_NEW(h, ADDR, VAL, INC)
#define BREV_STORE_wnew(ADDR, VAL, INC) \
    BREV_STORE_NEW(w, ADDR, VAL, INC)

uint32_t bitreverse(uint32_t x)
{
    uint32_t result = 0;
    for (int i = 0; i < NBITS; i++) {
        result <<= 1;
        result |= x & 1;
        x >>= 1;
    }
    return result;
}

int32_t sext8(int32_t x)
{
    return (x << 24) >> 24;
}

#define TEST_BREV_LOAD(SZ, TYPE, BUF, SHIFT, EXP) \
    do { \
        p = BUF; \
        for (int i = 0; i < SIZE; i++) { \
            TYPE result; \
            BREV_LOAD_##SZ(result, p, 1 << (SHIFT - NBITS)); \
            check32(result, EXP); \
        } \
    } while (0)

#define TEST_BREV_STORE(SZ, TYPE, BUF, VAL, SHIFT) \
    do { \
        p = BUF; \
        memset(BUF, 0xff, sizeof(BUF)); \
        for (int i = 0; i < SIZE; i++) { \
            BREV_STORE_##SZ(p, (TYPE)(VAL), 1 << (SHIFT - NBITS)); \
        } \
        for (int i = 0; i < SIZE; i++) { \
            check32(BUF[i], bitreverse(i)); \
        } \
    } while (0)

#define TEST_BREV_STORE_NEW(SZ, BUF, SHIFT) \
    do { \
        p = BUF; \
        memset(BUF, 0xff, sizeof(BUF)); \
        for (int i = 0; i < SIZE; i++) { \
            BREV_STORE_##SZ(p, i, 1 << (SHIFT - NBITS)); \
        } \
        for (int i = 0; i < SIZE; i++) { \
            check32(BUF[i], bitreverse(i)); \
        } \
    } while (0)

/*
 * We'll set high_half[i] = i << 16 for use in the .H form of store
 * which stores from the high half of the word.
 */
int high_half[SIZE];

int main()
{
    void *p;

    for (int i = 0; i < SIZE; i++) {
        bbuf[i] = bitreverse(i);
        hbuf[i] = bitreverse(i);
        wbuf[i] = bitreverse(i);
        dbuf[i] = bitreverse(i);
        high_half[i] = i << 16;
    }

    TEST_BREV_LOAD(b,  int32_t,   bbuf, 16, sext8(i));
    TEST_BREV_LOAD(ub, int32_t,   bbuf, 16, i);
    TEST_BREV_LOAD(h,  int32_t,   hbuf, 15, i);
    TEST_BREV_LOAD(uh, int32_t,   hbuf, 15, i);
    TEST_BREV_LOAD(w,  int32_t,   wbuf, 14, i);
    TEST_BREV_LOAD(d,  int64_t,   dbuf, 13, i);

    TEST_BREV_STORE(b, int32_t,   bbuf, i,            16);
    TEST_BREV_STORE(h, int32_t,   hbuf, i,            15);
    TEST_BREV_STORE(f, int32_t,   hbuf, high_half[i], 15);
    TEST_BREV_STORE(w, int32_t,   wbuf, i,            14);
    TEST_BREV_STORE(d, int64_t,   dbuf, i,            13);

    TEST_BREV_STORE_NEW(bnew, bbuf, 16);
    TEST_BREV_STORE_NEW(hnew, hbuf, 15);
    TEST_BREV_STORE_NEW(wnew, wbuf, 14);

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}
