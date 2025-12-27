/*
 * Crosscheck and benchmark swizzle.
 *
 * Copyright (c) 2025 Matt Borgerson
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define X_METHODS \
    X(A)
    // X(B)

typedef void (*swizzle_box_handler)(
    const uint8_t *src_buf,
    unsigned int width,
    unsigned int height,
    unsigned int depth,
    uint8_t *dst_buf,
    unsigned int row_pitch,
    unsigned int slice_pitch,
    unsigned int bytes_per_pixel);

typedef struct Method {
    const char *name;
    swizzle_box_handler swizzle, unswizzle;
} Method;

#define PROTO(m) \
    void m( \
    const uint8_t *src_buf, \
    unsigned int width, \
    unsigned int height, \
    unsigned int depth, \
    uint8_t *dst_buf, \
    unsigned int row_pitch, \
    unsigned int slice_pitch, \
    unsigned int bytes_per_pixel);

#define X(m) \
    PROTO(swizzle_box_ ## m) \
    PROTO(unswizzle_box_ ## m)
X_METHODS
#undef X

const Method methods[] = {
    #define X(m) { #m, swizzle_box_ ## m, unswizzle_box_ ## m},
    X_METHODS
    #undef X
};

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

int widths[] = { 1, 2, 4, 8, 16, 32 };
int heights[] = { 1, 2, 4, 8, 16, 32 };
int depths[] = { 1, 2, 4, 8, 16, 32 };
int bpps[] = { 1, 2, 3, 4 };

static void crosscheck(void)
{
    assert(ARRAY_SIZE(methods) > 0);
    fprintf(stderr, "%s...", __func__);
    for (int row_pitch_adjust = 0; row_pitch_adjust < 4; row_pitch_adjust++)
    for (int slice_pitch_adjust = 0; slice_pitch_adjust < 4; slice_pitch_adjust++)
    for (int depth_idx = 0; depth_idx < ARRAY_SIZE(depths); depth_idx++)
    for (int width_idx = 0; width_idx < ARRAY_SIZE(widths); width_idx++)
    for (int height_idx = 0; height_idx < ARRAY_SIZE(heights); height_idx++)
    for (int bpp_idx = 0; bpp_idx < ARRAY_SIZE(bpps); bpp_idx++) {

        int width = widths[width_idx];
        int height = heights[height_idx];
        int depth = depths[depth_idx];
        int bpp = bpps[bpp_idx];

        size_t row_pitch = width * bpp + row_pitch_adjust;
        size_t slice_pitch = row_pitch * height;
        size_t size_bytes = slice_pitch * depth + slice_pitch_adjust;

        uint8_t *original_data = malloc(size_bytes);
        for (int i = 0; i < size_bytes; i++) {
            original_data[i] = rand();
        }

        void *swizzled_data_A = malloc(size_bytes);
        memcpy(swizzled_data_A, original_data, size_bytes);
        methods[0].swizzle(original_data, width, height, depth, swizzled_data_A,
                           row_pitch, slice_pitch, bpp);

        void *unswizzled_data_A = malloc(size_bytes);
        memcpy(unswizzled_data_A, original_data, size_bytes);
        methods[0].unswizzle(swizzled_data_A, width, height, depth,
                             unswizzled_data_A, row_pitch, slice_pitch, bpp);
        assert(!memcmp(original_data, unswizzled_data_A, size_bytes));

        for (int method_idx = 1;
             method_idx < ARRAY_SIZE(methods);
             method_idx++) {
            void *swizzled_data_B = malloc(size_bytes);
            memcpy(swizzled_data_B, original_data, size_bytes);
            methods[method_idx].swizzle(original_data, width, height, depth,
                                        swizzled_data_B, row_pitch, slice_pitch,
                                        bpp);
            assert(!memcmp(swizzled_data_B, swizzled_data_A, size_bytes));

            void *unswizzled_data_B = malloc(size_bytes);
            memcpy(unswizzled_data_B, original_data, size_bytes);
            methods[method_idx].unswizzle(swizzled_data_B, width, height, depth,
                                          unswizzled_data_B, row_pitch,
                                          slice_pitch, bpp);
            assert(!memcmp(original_data, unswizzled_data_B, size_bytes));

            free(unswizzled_data_B);
            free(swizzled_data_B);
        }

        free(unswizzled_data_A);
        free(swizzled_data_A);
        free(original_data);

        // fprintf(stderr, "w:%d, h:%d, d:%d, bpp:%d pitch:%d,%d\n", width, height, depth, bpp, row_pitch_adjust, slice_pitch_adjust);
    }

    fprintf(stderr, "ok!\n");
}

#define NUM_ITERATIONS 10

static int compare_ints(const void *a, const void *b)
{
    return *(int*)a - *(int*)b;
}

static void bench(void)
{
    fprintf(stderr, "%s...", __func__);

    int width = 256;
    int height = 256;
    int depth = 256;
    int bpp = 4;

    size_t row_pitch = width * bpp;
    size_t slice_pitch = row_pitch * height;
    size_t size_bytes = slice_pitch * depth;
    size_t size_mib = size_bytes / (1024*1024);
    fprintf(stderr, "with w: %d, h: %d, d: %d, bpp: %d, "
                    "size: %zu MiB, iterations: %d\n",
                    width, height, depth, bpp, size_mib, NUM_ITERATIONS);

    void *original_data = malloc(size_bytes);
    memset(original_data, 0, size_bytes);

    void *swizzled_data = malloc(size_bytes);
    memset(swizzled_data, 0, size_bytes);


    for (int method_idx = 0; method_idx < ARRAY_SIZE(methods); method_idx++) {
        const Method * const method = &methods[method_idx];
        fprintf(stderr, "[%6s] ", method->name);

        int samples[NUM_ITERATIONS];
        int sum = 0;

        for (int iter = 0; iter < NUM_ITERATIONS; iter++ ) {
            struct timespec start, end;

            clock_gettime(CLOCK_MONOTONIC, &start);
            method->swizzle(original_data, width, height, depth, swizzled_data, row_pitch, slice_pitch, bpp);
            clock_gettime(CLOCK_MONOTONIC, &end);

            uint64_t start_ns = (uint64_t)start.tv_sec * (uint64_t)1000000000 + start.tv_nsec;
            uint64_t end_ns   = (uint64_t)end.tv_sec   * (uint64_t)1000000000 + end.tv_nsec;

            samples[iter] = (end_ns - start_ns) / 1000;
            sum += samples[iter];
        }

        qsort(samples, ARRAY_SIZE(samples), sizeof(samples[0]), compare_ints);

        int min = samples[0],
            max = samples[ARRAY_SIZE(samples) - 1],
            avg = sum / ARRAY_SIZE(samples),
            med = samples[ARRAY_SIZE(samples) / 2];
        fprintf(stderr, "min: %6d us, max: %6d us, avg: %6d us, med: %6d us  -- %.2g GiB/s\n",
                min, max, avg, med, (size_mib / 1024.0) / (med / 1000000.0));
    }

    free(swizzled_data);
    free(original_data);
}

int main(int argc, char const *argv[])
{
    srand(1337);

    crosscheck();
    bench();

    return 0;
}
