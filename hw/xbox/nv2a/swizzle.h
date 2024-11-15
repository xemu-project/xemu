/*
 * QEMU texture swizzling routines
 *
 * Copyright (c) 2015 Jannik Vogel
 * Copyright (c) 2013 espes
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

#ifndef HW_XBOX_SWIZZLE_H
#define HW_XBOX_SWIZZLE_H

void swizzle_box(
    const uint8_t *src_buf,
    unsigned int width,
    unsigned int height,
    unsigned int depth,
    uint8_t *dst_buf,
    unsigned int row_pitch,
    unsigned int slice_pitch,
    unsigned int bytes_per_pixel);

void unswizzle_box(
    const uint8_t *src_buf,
    unsigned int width,
    unsigned int height,
    unsigned int depth,
    uint8_t *dst_buf,
    unsigned int row_pitch,
    unsigned int slice_pitch,
    unsigned int bytes_per_pixel);

void unswizzle_rect(
    const uint8_t *src_buf,
    unsigned int width,
    unsigned int height,
    uint8_t *dst_buf,
    unsigned int pitch,
    unsigned int bytes_per_pixel);

void swizzle_rect(
    const uint8_t *src_buf,
    unsigned int width,
    unsigned int height,
    uint8_t *dst_buf,
    unsigned int pitch,
    unsigned int bytes_per_pixel);

#endif
