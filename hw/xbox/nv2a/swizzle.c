/*
 * QEMU texture swizzling routines
 *
 * Copyright (c) 2015 Jannik Vogel
 * Copyright (c) 2013 espes
 * Copyright (c) 2007-2010 The Nouveau Project.
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

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "qemu/osdep.h"

#include "swizzle.h"

#ifdef __BMI2__
#include <x86intrin.h>
#endif

/* This should be pretty straightforward.
 * It creates a bit pattern like ..zyxzyxzyx from ..xxx, ..yyy and ..zzz
 * If there are no bits left from any component it will pack the other masks
 * more tighly (Example: zzxzxzyx = Fewer x than z and even fewer y)
 */
static void generate_swizzle_masks(unsigned int width,
                                   unsigned int height,
                                   unsigned int depth,
                                   uint32_t* mask_x,
                                   uint32_t* mask_y,
                                   uint32_t* mask_z)
{
    uint32_t x = 0, y = 0, z = 0;
    uint32_t bit = 1;
    uint32_t mask_bit = 1;
    bool done;
    do {
        done = true;
        if (bit < width) { x |= mask_bit; mask_bit <<= 1; done = false; }
        if (bit < height) { y |= mask_bit; mask_bit <<= 1; done = false; }
        if (bit < depth) { z |= mask_bit; mask_bit <<= 1; done = false; }
        bit <<= 1;
    } while(!done);
    assert((x ^ y ^ z) == (mask_bit - 1));
    *mask_x = x;
    *mask_y = y;
    *mask_z = z;
}

/* Move low bits to positions given by a mask, while keeping bits in order.
 * e.g.
 * expand(0000abcd, 10011010) = a00bc0d0.
 *
 * Implementation from Hacker's delight chapter 7 "Expand"
 * https://stackoverflow.com/questions/77834169/what-is-a-fast-fallback-algorithm-which-emulates-pdep-and-pext-in-software
 */
typedef struct expand_mask {
    uint32_t mask;
    uint32_t moves[5];
} expand_mask;

static void generate_expand_mask_moves(expand_mask* expand_mask) {
    uint32_t mk, mp, mv;
    uint32_t mask = expand_mask->mask;
    int i;
    mk = ~mask << 1; // We will count 0's to right.
    for (i = 0; i < 5; i++) {
        mp = mk ^ (mk << 1); // Parallel suffix.
        mp = mp ^ (mp << 2);
        mp = mp ^ (mp << 4);
        mp = mp ^ (mp << 8);
        mp = mp ^ (mp << 16);
        mv = mp & mask; // Bits to move.
        expand_mask->moves[i] = mv;
        mask = (mask ^ mv) | (mv >> (1 << i)); // Compress m.
        mk = mk & ~mp;
    }

}

static uint32_t expand(uint32_t x, expand_mask* expand_mask) {
    #ifdef __BMI2__
    return _pdep_u32(x, expand_mask->mask);
    #else
    uint32_t mv, t;
    for (int i = 4; i >= 0; i--) {
        mv = expand_mask->moves[i];
        t = x << (1 << i);
        x = (x & ~mv) | (t & mv);
    }
    return x & expand_mask->mask; // Clear out extraneous bits.
    #endif
}

static inline unsigned int get_swizzled_offset(
    unsigned int x, unsigned int y, unsigned int z,
    expand_mask *mask_x, expand_mask *mask_y, expand_mask *mask_z,
    unsigned int bytes_per_pixel)
{ 
    return bytes_per_pixel * (expand(x, mask_x)
                           | expand(y, mask_y)
                           | expand(z, mask_z));
}

void swizzle_box(
    const uint8_t *src_buf,
    unsigned int width,
    unsigned int height,
    unsigned int depth,
    uint8_t *dst_buf,
    unsigned int row_pitch,
    unsigned int slice_pitch,
    unsigned int bytes_per_pixel)
{
    expand_mask mask_x, mask_y, mask_z;
    generate_swizzle_masks(width, height, depth, &mask_x.mask, &mask_y.mask, &mask_z.mask);
    generate_expand_mask_moves(&mask_x);
    generate_expand_mask_moves(&mask_y);
    generate_expand_mask_moves(&mask_z);

    int x, y, z;
    for (z = 0; z < depth; z++) {
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                const uint8_t *src = src_buf
                                         + y * row_pitch + x * bytes_per_pixel;
                uint8_t *dst = dst_buf +
                    get_swizzled_offset(x, y, z, &mask_x, &mask_y, &mask_z,
                                        bytes_per_pixel);
                memcpy(dst, src, bytes_per_pixel);
            }
        }
        src_buf += slice_pitch;
    }
}

void unswizzle_box(
    const uint8_t *src_buf,
    unsigned int width,
    unsigned int height,
    unsigned int depth,
    uint8_t *dst_buf,
    unsigned int row_pitch,
    unsigned int slice_pitch,
    unsigned int bytes_per_pixel)
{
    expand_mask mask_x, mask_y, mask_z;
    generate_swizzle_masks(width, height, depth, &mask_x.mask, &mask_y.mask, &mask_z.mask);
    generate_expand_mask_moves(&mask_x);
    generate_expand_mask_moves(&mask_y);
    generate_expand_mask_moves(&mask_z);

    int x, y, z;
    for (z = 0; z < depth; z++) {
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                const uint8_t *src = src_buf
                    + get_swizzled_offset(x, y, z, &mask_x, &mask_y, &mask_z,
                                          bytes_per_pixel);
                uint8_t *dst = dst_buf + y * row_pitch + x * bytes_per_pixel;
                memcpy(dst, src, bytes_per_pixel);
            }
        }
        dst_buf += slice_pitch;
    }
}

void unswizzle_rect(
    const uint8_t *src_buf,
    unsigned int width,
    unsigned int height,
    uint8_t *dst_buf,
    unsigned int pitch,
    unsigned int bytes_per_pixel)
{
    unswizzle_box(src_buf, width, height, 1, dst_buf, pitch, 0, bytes_per_pixel);
}

void swizzle_rect(
    const uint8_t *src_buf,
    unsigned int width,
    unsigned int height,
    uint8_t *dst_buf,
    unsigned int pitch,
    unsigned int bytes_per_pixel)
{
    swizzle_box(src_buf, width, height, 1, dst_buf, pitch, 0, bytes_per_pixel);
}
