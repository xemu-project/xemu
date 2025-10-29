// xxHash/tests/sanity_test.c
// SPDX-License-Identifier: GPL-2.0-only
//
// Building
// ========
//
// cc sanity_test.c && ./a.out
//
/*
notes or changes:

main()
------

- All test methods (XXH32, XXH64, ...) are context free.
  - It means that there's no restriction by order of tests and # of test.  (Ready for multi-threaded / distributed test)
  - To achieve it, some test has dedicated 'randSeed' to decouple dependency between tests.
  - Note that for() loop is not ready for distributed test.
    - randSeed still needs to be computed step by step in the for() loop so far.

*/
#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION   /* access definitions */
#include "../cli/xsum_arch.h"
#include "../cli/xsum_os_specific.h"
#include "../cli/xsum_output.h"
#include "../cli/xsum_output.c"
#define XSUM_NO_MAIN 1
#include "../cli/xsum_os_specific.h"
#include "../cli/xsum_os_specific.c"
#include "../xxhash.h"
#include "sanity_test_vectors.h"

#include <assert.h> /* assert */

/* use #define to make them constant, required for initialization */
#define PRIME32 2654435761U
#define PRIME64 11400714785074694797ULL

#define SANITY_BUFFER_SIZE (4096 + 64 + 1)


/**/
static int abortByError = 1;


/**/
static void abortSanityTest(void) {
    /* ??? : Should we show this message? */
    XSUM_log("\rNote: If you modified the hash functions, make sure to either update tests/sanity_test_vectors.h with the following command\n"
             "\r  make -C tests sanity_test_vectors.h\n");
    XSUM_log("\rAbort.\n");
    exit(1);
}

/* TODO : Share this function with sanity_check.c and xsum_sanity_check.c */
/*
 * Fills a test buffer with pseudorandom data.
 *
 * This is used in the sanity check - its values must not be changed.
 */
static void fillTestBuffer(XSUM_U8* buffer, size_t bufferLenInBytes)
{
    XSUM_U64 byteGen = PRIME32;
    size_t i;

    assert(buffer != NULL);

    for (i = 0; i < bufferLenInBytes; ++i) {
        buffer[i] = (XSUM_U8)(byteGen>>56);
        byteGen *= PRIME64;
    }
}


/* TODO : Share this function with sanity_check.c and xsum_sanity_check.c */
/*
 * Create (malloc) and fill buffer with pseudorandom data for sanity check.
 *
 * Use releaseSanityBuffer() to delete the buffer.
 */
static XSUM_U8* createSanityBuffer(size_t bufferLenInBytes)
{
    XSUM_U8* buffer = (XSUM_U8*) malloc(bufferLenInBytes);
    assert(buffer != NULL);
    fillTestBuffer(buffer, bufferLenInBytes);
    return buffer;
}


/* TODO : Share this function with sanity_check.c and xsum_sanity_check.c */
/*
 * Delete (free) the buffer which has been genereated by createSanityBuffer()
 */
static void releaseSanityBuffer(XSUM_U8* buffer)
{
    assert(buffer != NULL);
    free(buffer);
}


/* TODO : Share this function with xsum_sanity_check.c */
/**/
static void checkResult32(XXH32_hash_t r1, XXH32_hash_t r2, const char* testName, size_t testNb, size_t lineNb)
{
    if(r1 == r2) {
        return;
    }

    XSUM_log("\rError: %s #%zd, line #%zd: Sanity check failed!\n", testName, testNb, lineNb);
    XSUM_log("\rGot 0x%08X, expected 0x%08X.\n", (unsigned)r1, (unsigned)r2);

    if(abortByError) {
        abortSanityTest();
    }
}


/* TODO : Share this function with xsum_sanity_check.c */
/**/
static void checkResult64(XXH64_hash_t r1, XXH64_hash_t r2, const char* testName, size_t testNb, size_t lineNb)
{
    if(r1 == r2) {
        return;
    }

    XSUM_log("\rError: %s #%zd, line #%zd: Sanity check failed!\n", testName, testNb, lineNb);
    XSUM_log("\rGot 0x%08X%08XULL, expected 0x%08X%08XULL.\n",
            (unsigned)(r1>>32), (unsigned)r1, (unsigned)(r2>>32), (unsigned)r2);

    if(abortByError) {
        abortSanityTest();
    }
}


/* TODO : Share this function with xsum_sanity_check.c */
/**/
static void checkResult128(XXH128_hash_t r1, XXH128_hash_t r2, const char* testName, size_t testNb, size_t lineNb)
{
    if ((r1.low64 == r2.low64) && (r1.high64 == r2.high64)) {
        return;
    }

    XSUM_log("\rError: %s #%zd, line #%zd: Sanity check failed!\n", testName, testNb, lineNb);
    XSUM_log("\rGot { 0x%08X%08XULL, 0x%08X%08XULL }, expected { 0x%08X%08XULL, 0x%08X%08XULL } \n",
            (unsigned)(r1.low64>>32), (unsigned)r1.low64, (unsigned)(r1.high64>>32), (unsigned)r1.high64,
            (unsigned)(r2.low64>>32), (unsigned)r2.low64, (unsigned)(r2.high64>>32), (unsigned)r2.high64 );

    if(abortByError) {
        abortSanityTest();
    }
}


/* TODO : Share this function with xsum_sanity_check.c */
/**/
static void checkResultTestDataSample(const XSUM_U8* r1, const XSUM_U8* r2, const char* testName, size_t testNb, size_t lineNb)
{
    if(memcmp(r1, r2, SECRET_SAMPLE_NBBYTES) == 0) {
        return;
    }

    XSUM_log("\rError: %s #%zd, line #%zd: Sanity check failed!\n", testName, testNb, lineNb);
    XSUM_log("\rGot { 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X }, expected { 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X } \n",
            r1[0], r1[1], r1[2], r1[3], r1[4],
            r2[0], r2[1], r2[2], r2[3], r2[4] );

    if(abortByError) {
        abortSanityTest();
    }
}


/* TODO : Share this function with xsum_sanity_check.c */
/**/
static void testXXH32(
    const void* data,
    const XSUM_testdata32_t* testData,
    const char* testName,
    size_t testNb
) {
    size_t   const len     = testData->len;
    XSUM_U32 const seed    = testData->seed;
    XSUM_U32 const Nresult = testData->Nresult;

    XXH32_state_t * const state = XXH32_createState();

    if (len == 0) {
        data = NULL;
    } else {
        assert(data != NULL);
    }

    assert(state != NULL);

    checkResult32(XXH32(data, len, seed), Nresult, testName, testNb, __LINE__);

    (void)XXH32_reset(state, seed);
    (void)XXH32_update(state, data, len);
    checkResult32(XXH32_digest(state), Nresult, testName, testNb, __LINE__);

    (void)XXH32_reset(state, seed);
    {
        size_t pos;
        for (pos = 0; pos < len; ++pos) {
            (void)XXH32_update(state, ((const char*)data)+pos, 1);
        }
    }
    checkResult32(XXH32_digest(state), Nresult, testName, testNb, __LINE__);

    XXH32_freeState(state);
}


/* TODO : Share this function with xsum_sanity_check.c */
/**/
static void testXXH64(
    const void* data,
    const XSUM_testdata64_t* testData,
    const char* testName,
    size_t testNb
)
{
    size_t   const len     = (size_t)testData->len;
    XSUM_U64 const seed    = testData->seed;
    XSUM_U64 const Nresult = testData->Nresult;

    XXH64_state_t * const state = XXH64_createState();

    if (len == 0) {
        data = NULL;
    } else {
        assert(data != NULL);
    }

    assert(state != NULL);

    checkResult64(XXH64(data, len, seed), Nresult, testName, testNb, __LINE__);

    (void)XXH64_reset(state, seed);
    (void)XXH64_update(state, data, len);
    checkResult64(XXH64_digest(state), Nresult, testName, testNb, __LINE__);

    (void)XXH64_reset(state, seed);
    {
        size_t pos;
        for (pos = 0; pos < len; ++pos) {
            (void)XXH64_update(state, ((const char*)data)+pos, 1);
        }
    }
    checkResult64(XXH64_digest(state), Nresult, testName, testNb, __LINE__);
    XXH64_freeState(state);
}


/* TODO : Share this function with xsum_sanity_check.c */
/*
 * Used to get "random" (but actually 100% reproducible) lengths for
 * XSUM_XXH3_randomUpdate.
 */
static XSUM_U32 SANITY_TEST_rand(XSUM_U64* pRandSeed)
{
    XSUM_U64 seed = *pRandSeed;
    seed *= PRIME64;
    *pRandSeed = seed;
    return (XSUM_U32)(seed >> 40);
}


/* TODO : Share this function with xsum_sanity_check.c */
/**/
static XSUM_U64 SANITY_TEST_computeRandSeed(size_t step)
{
    XSUM_U64 randSeed = PRIME32;
    size_t i = 0;
    for(i = 0; i < step; ++i) {
        SANITY_TEST_rand(&randSeed);
    }
    return randSeed;
}


/* TODO : Share this definition with xsum_sanity_check.c */
/*
 * Technically, XXH3_64bits_update is identical to XXH3_128bits_update as of
 * v0.8.0, but we treat them as separate.
 */
typedef XXH_errorcode (*SANITY_TEST_XXH3_update_t)(XXH3_state_t* state, const void* input, size_t length);


/* TODO : Share this function with xsum_sanity_check.c */
/*
 * Runs the passed XXH3_update variant on random lengths. This is to test the
 * more complex logic of the update function, catching bugs like this one:
 *    https://github.com/Cyan4973/xxHash/issues/378
 */
static void SANITY_TEST_XXH3_randomUpdate(
    XXH3_state_t* state,
    const void* data,
    size_t len,
    XSUM_U64* pRandSeed,
    SANITY_TEST_XXH3_update_t update_fn
)
{
    size_t p = 0;
    while (p < len) {
        size_t const modulo = len > 2 ? len : 2;
        size_t l = (size_t)(SANITY_TEST_rand(pRandSeed)) % modulo;
        if (p + l > len) l = len - p;
        (void)update_fn(state, (const char*)data+p, l);
        p += l;
    }
}


/* TODO : Share this function with xsum_sanity_check.c */
/**/
static void testXXH3(
    const void* data,
    const void* secret,
    size_t secretSize,
    const XSUM_testdata64_t* testData,
    XSUM_U64* pRandSeed,
    const char* testName,
    size_t testNb
)
{
    size_t   const len     = testData->len;
    XSUM_U64 const seed    = testData->seed;
    XSUM_U64 const Nresult = testData->Nresult;
    if (len == 0) {
        data = NULL;
    } else {
        assert(data != NULL);
    }
    {   XSUM_U64 const Dresult = XXH3_64bits_withSeed(data, len, seed);
        checkResult64(Dresult, Nresult, testName, testNb, __LINE__);
    }

    /* check that the no-seed variant produces same result as seed==0 */
    if (seed == 0) {
        XSUM_U64 const Dresult = XXH3_64bits(data, len);
        checkResult64(Dresult, Nresult, testName, testNb, __LINE__);
    }

    /* check that the combination of
     * XXH3_generateSecret_fromSeed() and XXH3_64bits_withSecretandSeed()
     * results in exactly the same hash generation as XXH3_64bits_withSeed() */
    {   char secretBuffer[XXH3_SECRET_DEFAULT_SIZE+1];
        char* const secretFromSeed = secretBuffer + 1;  /* intentional unalignment */
        XXH3_generateSecret_fromSeed(secretFromSeed, seed);
        {   XSUM_U64 const Dresult = XXH3_64bits_withSecretandSeed(data, len, secretFromSeed, XXH3_SECRET_DEFAULT_SIZE, seed);
            checkResult64(Dresult, Nresult, testName, testNb, __LINE__);
    }   }

    /* check that XXH3_64bits_withSecretandSeed()
     * results in exactly the same return value as XXH3_64bits_withSeed() */
    if (len <= XXH3_MIDSIZE_MAX) {
        XSUM_U64 const Dresult = XXH3_64bits_withSecretandSeed(data, len, secret, secretSize, seed);
        checkResult64(Dresult, Nresult, testName, testNb, __LINE__);
    }

    /* streaming API test */
    {   XXH3_state_t* const state = XXH3_createState();
        assert(state != NULL);
        /* single ingestion */
        (void)XXH3_64bits_reset_withSeed(state, seed);
        (void)XXH3_64bits_update(state, data, len);
        checkResult64(XXH3_64bits_digest(state), Nresult, testName, testNb, __LINE__);

        /* random ingestion */
        (void)XXH3_64bits_reset_withSeed(state, seed);
        SANITY_TEST_XXH3_randomUpdate(state, data, len, pRandSeed, &XXH3_64bits_update);
        checkResult64(XXH3_64bits_digest(state), Nresult, testName, testNb, __LINE__);

        /* byte by byte ingestion */
        {   size_t pos;
            (void)XXH3_64bits_reset_withSeed(state, seed);
            for (pos=0; pos<len; pos++)
                (void)XXH3_64bits_update(state, ((const char*)data)+pos, 1);
            checkResult64(XXH3_64bits_digest(state), Nresult, testName, testNb, __LINE__);
        }

        /* check that streaming with a combination of
         * XXH3_generateSecret_fromSeed() and XXH3_64bits_reset_withSecretandSeed()
         * results in exactly the same hash generation as XXH3_64bits_reset_withSeed() */
        {   char secretBuffer[XXH3_SECRET_DEFAULT_SIZE+1];
            char* const secretFromSeed = secretBuffer + 1;  /* intentional unalignment */
            XXH3_generateSecret_fromSeed(secretFromSeed, seed);
            /* single ingestion */
            (void)XXH3_64bits_reset_withSecretandSeed(state, secretFromSeed, XXH3_SECRET_DEFAULT_SIZE, seed);
            (void)XXH3_64bits_update(state, data, len);
            checkResult64(XXH3_64bits_digest(state), Nresult, testName, testNb, __LINE__);
        }

        /* check that XXH3_64bits_withSecretandSeed()
         * results in exactly the same return value as XXH3_64bits_withSeed() */
        if (len <= XXH3_MIDSIZE_MAX) {
            /* single ingestion */
            (void)XXH3_64bits_reset_withSecretandSeed(state, secret, secretSize, seed);
            (void)XXH3_64bits_update(state, data, len);
            checkResult64(XXH3_64bits_digest(state), Nresult, testName, testNb, __LINE__);
        }

        XXH3_freeState(state);
    }
}


/* TODO : Share this function with xsum_sanity_check.c */
/**/
static void testXXH3_withSecret(
    const void* data,
    const void* secret,
    size_t secretSize,
    const XSUM_testdata64_t* testData,
    XSUM_U64* pRandSeed,
    const char* testName,
    size_t testNb
)
{
    size_t   const len     = (size_t)testData->len;
    XSUM_U64 const Nresult = testData->Nresult;

    if (len == 0) {
        data = NULL;
    } else {
        assert(data != NULL);
    }
    {   XSUM_U64 const Dresult = XXH3_64bits_withSecret(data, len, secret, secretSize);
        checkResult64(Dresult, Nresult, testName, testNb, __LINE__);
    }

    /* check that XXH3_64bits_withSecretandSeed()
     * results in exactly the same return value as XXH3_64bits_withSecret() */
    if (len > XXH3_MIDSIZE_MAX)
    {   XSUM_U64 const Dresult = XXH3_64bits_withSecretandSeed(data, len, secret, secretSize, 0);
        checkResult64(Dresult, Nresult, testName, testNb, __LINE__);
    }

    /* streaming API test */
    {   XXH3_state_t * const state = XXH3_createState();
        assert(state != NULL);
        (void)XXH3_64bits_reset_withSecret(state, secret, secretSize);
        (void)XXH3_64bits_update(state, data, len);
        checkResult64(XXH3_64bits_digest(state), Nresult, testName, testNb, __LINE__);

        /* random ingestion */
        (void)XXH3_64bits_reset_withSecret(state, secret, secretSize);
        SANITY_TEST_XXH3_randomUpdate(state, data, len, pRandSeed, &XXH3_64bits_update);
        checkResult64(XXH3_64bits_digest(state), Nresult, testName, testNb, __LINE__);

        /* byte by byte ingestion */
        {   size_t pos;
            (void)XXH3_64bits_reset_withSecret(state, secret, secretSize);
            for (pos=0; pos<len; pos++)
                (void)XXH3_64bits_update(state, ((const char*)data)+pos, 1);
            checkResult64(XXH3_64bits_digest(state), Nresult, testName, testNb, __LINE__);
        }

        /* check that XXH3_64bits_reset_withSecretandSeed()
         * results in exactly the same return value as XXH3_64bits_reset_withSecret() */
         if (len > XXH3_MIDSIZE_MAX) {
            /* single ingestion */
            (void)XXH3_64bits_reset_withSecretandSeed(state, secret, secretSize, 0);
            (void)XXH3_64bits_update(state, data, len);
            checkResult64(XXH3_64bits_digest(state), Nresult, testName, testNb, __LINE__);
        }

        XXH3_freeState(state);
    }
}


/* TODO : Share this function with xsum_sanity_check.c */
/**/
static void testXXH128(
    const void* data,
    const void* secret,
    size_t secretSize,
    const XSUM_testdata128_t* testData,
    XSUM_U64* pRandSeed,
    const char* testName,
    size_t testNb
)
{
    size_t        const len     = (size_t)testData->len;
    XSUM_U64      const seed    = testData->seed;
    XXH128_hash_t const Nresult = testData->Nresult;
    if (len == 0) {
        data = NULL;
    } else {
        assert(data != NULL);
    }

    {   XXH128_hash_t const Dresult = XXH3_128bits_withSeed(data, len, seed);
        checkResult128(Dresult, Nresult, testName, testNb, __LINE__);
    }

    /* check that XXH128() is identical to XXH3_128bits_withSeed() */
    {   XXH128_hash_t const Dresult2 = XXH128(data, len, seed);
        checkResult128(Dresult2, Nresult, testName, testNb, __LINE__);
    }

    /* check that the no-seed variant produces same result as seed==0 */
    if (seed == 0) {
        XXH128_hash_t const Dresult = XXH3_128bits(data, len);
        checkResult128(Dresult, Nresult, testName, testNb, __LINE__);
    }

    /* check that the combination of
     * XXH3_generateSecret_fromSeed() and XXH3_128bits_withSecretandSeed()
     * results in exactly the same hash generation as XXH3_64bits_withSeed() */
    {   char secretBuffer[XXH3_SECRET_DEFAULT_SIZE+1];
        char* const secretFromSeed = secretBuffer + 1;  /* intentional unalignment */
        XXH3_generateSecret_fromSeed(secretFromSeed, seed);
        {   XXH128_hash_t const Dresult = XXH3_128bits_withSecretandSeed(data, len, secretFromSeed, XXH3_SECRET_DEFAULT_SIZE, seed);
            checkResult128(Dresult, Nresult, testName, testNb, __LINE__);
    }   }

    /* check that XXH3_128bits_withSecretandSeed()
     * results in exactly the same return value as XXH3_128bits_withSeed() */
    if (len <= XXH3_MIDSIZE_MAX) {
        XXH128_hash_t const Dresult = XXH3_128bits_withSecretandSeed(data, len, secret, secretSize, seed);
        checkResult128(Dresult, Nresult, testName, testNb, __LINE__);
    }

    /* streaming API test */
    {   XXH3_state_t * const state = XXH3_createState();
        assert(state != NULL);

        /* single ingestion */
        (void)XXH3_128bits_reset_withSeed(state, seed);
        (void)XXH3_128bits_update(state, data, len);
        checkResult128(XXH3_128bits_digest(state), Nresult, testName, testNb, __LINE__);

        /* random ingestion */
        (void)XXH3_128bits_reset_withSeed(state, seed);
        SANITY_TEST_XXH3_randomUpdate(state, data, len, pRandSeed, &XXH3_128bits_update);
        checkResult128(XXH3_128bits_digest(state), Nresult, testName, testNb, __LINE__);

        /* byte by byte ingestion */
        {   size_t pos;
            (void)XXH3_128bits_reset_withSeed(state, seed);
            for (pos=0; pos<len; pos++)
                (void)XXH3_128bits_update(state, ((const char*)data)+pos, 1);
            checkResult128(XXH3_128bits_digest(state), Nresult, testName, testNb, __LINE__);
        }

        /* check that streaming with a combination of
         * XXH3_generateSecret_fromSeed() and XXH3_128bits_reset_withSecretandSeed()
         * results in exactly the same hash generation as XXH3_128bits_reset_withSeed() */
        {   char secretBuffer[XXH3_SECRET_DEFAULT_SIZE+1];
            char* const secretFromSeed = secretBuffer + 1;  /* intentional unalignment */
            XXH3_generateSecret_fromSeed(secretFromSeed, seed);
            /* single ingestion */
            (void)XXH3_128bits_reset_withSecretandSeed(state, secretFromSeed, XXH3_SECRET_DEFAULT_SIZE, seed);
            (void)XXH3_128bits_update(state, data, len);
            checkResult128(XXH3_128bits_digest(state), Nresult, testName, testNb, __LINE__);
        }

        /* check that XXH3_128bits_reset_withSecretandSeed()
         * results in exactly the same return value as XXH3_128bits_reset_withSeed() */
        if (len <= XXH3_MIDSIZE_MAX) {
            /* single ingestion */
            (void)XXH3_128bits_reset_withSecretandSeed(state, secret, secretSize, seed);
            (void)XXH3_128bits_update(state, data, len);
            checkResult128(XXH3_128bits_digest(state), Nresult, testName, testNb, __LINE__);
        }

        XXH3_freeState(state);
    }
}


/* TODO : Share this function with xsum_sanity_check.c */
/**/
static void testXXH128_withSecret(
    const void* data,
    const void* secret,
    size_t secretSize,
    const XSUM_testdata128_t* testData,
    XSUM_U64* pRandSeed,
    const char* testName,
    size_t testNb
)
{
    size_t        const len     = testData->len;
    XXH128_hash_t const Nresult = testData->Nresult;
    if (len == 0) {
        data = NULL;
    } else {
        assert(data != NULL);
    }

    {   XXH128_hash_t const Dresult = XXH3_128bits_withSecret(data, len, secret, secretSize);
        checkResult128(Dresult, Nresult, testName, testNb, __LINE__);
    }

    /* check that XXH3_128bits_withSecretandSeed()
     * results in exactly the same return value as XXH3_128bits_withSecret() */
    if (len > XXH3_MIDSIZE_MAX)
    {   XXH128_hash_t const Dresult = XXH3_128bits_withSecretandSeed(data, len, secret, secretSize, 0);
        checkResult128(Dresult, Nresult, testName, testNb, __LINE__);
    }

    /* streaming API test */
    {   XXH3_state_t* const state = XXH3_createState();
        assert(state != NULL);
        (void)XXH3_128bits_reset_withSecret(state, secret, secretSize);
        (void)XXH3_128bits_update(state, data, len);
        checkResult128(XXH3_128bits_digest(state), Nresult, testName, testNb, __LINE__);

        /* random ingestion */
        (void)XXH3_128bits_reset_withSecret(state, secret, secretSize);
        SANITY_TEST_XXH3_randomUpdate(state, data, len, pRandSeed, &XXH3_128bits_update);
        checkResult128(XXH3_128bits_digest(state), Nresult, testName, testNb, __LINE__);

        /* byte by byte ingestion */
        {   size_t pos;
            (void)XXH3_128bits_reset_withSecret(state, secret, secretSize);
            for (pos=0; pos<len; pos++)
                (void)XXH3_128bits_update(state, ((const char*)data)+pos, 1);
            checkResult128(XXH3_128bits_digest(state), Nresult, testName, testNb, __LINE__);
        }

        /* check that XXH3_128bits_reset_withSecretandSeed()
         * results in exactly the same return value as XXH3_128bits_reset_withSecret() */
         if (len > XXH3_MIDSIZE_MAX) {
            /* single ingestion */
            (void)XXH3_128bits_reset_withSecretandSeed(state, secret, secretSize, 0);
            (void)XXH3_128bits_update(state, data, len);
            checkResult128(XXH3_128bits_digest(state), Nresult, testName, testNb, __LINE__);
        }

        XXH3_freeState(state);
    }
}


/* TODO : Share this function with xsum_sanity_check.c */
/**/
static void testSecretGenerator(
    const void* customSeed,
    const XSUM_testdata_sample_t* testData,
    const char* testName,
    size_t testNb
)
{
    /* TODO : Share this array with sanity_check.c and xsum_sanity_check.c */
    static const int sampleIndex[SECRET_SAMPLE_NBBYTES] = { 0, 62, 131, 191, 241 };  /* position of sampled bytes */
    XSUM_U8 secretBuffer[SECRET_SIZE_MAX] = {0};
    XSUM_U8 samples[SECRET_SAMPLE_NBBYTES];
    int i;

    assert(testData->secretLen <= SECRET_SIZE_MAX);
    XXH3_generateSecret(secretBuffer, testData->secretLen, customSeed, testData->seedLen);
    for (i=0; i<SECRET_SAMPLE_NBBYTES; i++) {
        samples[i] = secretBuffer[sampleIndex[i]];
    }
    checkResultTestDataSample(samples, testData->byte, testName, testNb, __LINE__);
}


/**/
int main(int argc, const char* argv[])
{
    size_t testCount = 0;
    size_t      const sanityBufferSizeInBytes = SANITY_BUFFER_SIZE;
    XSUM_U8*    const sanityBuffer            = createSanityBuffer(sanityBufferSizeInBytes);
    const void* const secret                  = sanityBuffer + 7;
    size_t      const secretSize              = XXH3_SECRET_SIZE_MIN + 11;
    assert(sanityBufferSizeInBytes >= 7 + secretSize);
    (void) argc;
    (void) argv;

    {
        /* XXH32 */
        size_t const n = sizeof(XSUM_XXH32_testdata) / sizeof(XSUM_XXH32_testdata[0]);
        size_t i;
        for (i = 0; i < n; ++i, ++testCount) {
            testXXH32(
                sanityBuffer,
                &XSUM_XXH32_testdata[i],
                "XSUM_XXH32_testdata",
                i
            );
        }
    }

    {
        /* XXH64 */
        size_t const n = sizeof(XSUM_XXH64_testdata) / sizeof(XSUM_XXH64_testdata[0]);
        size_t i;
        for (i = 0; i < n; ++i, ++testCount) {
            testXXH64(
                sanityBuffer,
                &XSUM_XXH64_testdata[i],
                "XSUM_XXH64_testdata",
                i
            );
        }
    }

    {
        /* XXH3_64bits, seeded */
        size_t const randCount = 0;
        XSUM_U64 randSeed = SANITY_TEST_computeRandSeed(randCount);
        size_t const n = sizeof(XSUM_XXH3_testdata) / sizeof(XSUM_XXH3_testdata[0]);
        size_t i;
        for (i = 0; i < n; ++i, ++testCount) {
            testXXH3(
                sanityBuffer,
                secret,
                secretSize,
                &XSUM_XXH3_testdata[i],
                &randSeed,
                "XSUM_XXH3_testdata",
                i
            );
        }
    }

    {
        /* XXH3_64bits, custom secret */
        size_t const randCount = 22730;
        XSUM_U64 randSeed = SANITY_TEST_computeRandSeed(randCount);
        size_t const n = sizeof(XSUM_XXH3_withSecret_testdata) / sizeof(XSUM_XXH3_withSecret_testdata[0]);
        size_t i;
        for (i = 0; i < n; ++i, ++testCount) {
            testXXH3_withSecret(
                sanityBuffer,
                secret,
                secretSize,
                &XSUM_XXH3_withSecret_testdata[i],
                &randSeed,
                "XSUM_XXH3_withSecret_testdata",
                i
            );
            testCount++;
        }
    }

    {
        /* XXH128 */
        size_t const randCount = 34068;
        XSUM_U64 randSeed = SANITY_TEST_computeRandSeed(randCount);
        size_t const n = (sizeof(XSUM_XXH128_testdata)/sizeof(XSUM_XXH128_testdata[0]));
        size_t i;
        for (i = 0; i < n; ++i, ++testCount) {
            testXXH128(
                sanityBuffer,
                secret,
                secretSize,
                &XSUM_XXH128_testdata[i],
                &randSeed,
                "XSUM_XXH128_testdata",
                i
            );
        }
    }

    {
        /* XXH128 with custom Secret */
        size_t const randCount = 68019;
        XSUM_U64 randSeed = SANITY_TEST_computeRandSeed(randCount);
        size_t const n = (sizeof(XSUM_XXH128_withSecret_testdata)/sizeof(XSUM_XXH128_withSecret_testdata[0]));
        size_t i;
        for (i = 0; i < n; ++i, ++testCount) {
            testXXH128_withSecret(
                sanityBuffer,
                secret,
                secretSize,
                &XSUM_XXH128_withSecret_testdata[i],
                &randSeed,
                "XSUM_XXH128_withSecret_testdata",
                i
            );
        }
    }

    {
        /* secret generator */
        size_t const n = sizeof(XSUM_XXH3_generateSecret_testdata) / sizeof(XSUM_XXH3_generateSecret_testdata[0]);
        size_t i;
        for (i = 0; i < n; ++i, ++testCount) {
            assert(XSUM_XXH3_generateSecret_testdata[i].seedLen <= SANITY_BUFFER_SIZE);
            testSecretGenerator(
                sanityBuffer,
                &XSUM_XXH3_generateSecret_testdata[i],
                "XSUM_XXH3_generateSecret_testdata",
                i
            );
        }
    }

    releaseSanityBuffer(sanityBuffer);

    XSUM_log("\rOK. (passes %zd tests)\n", testCount);

    return EXIT_SUCCESS;
}
