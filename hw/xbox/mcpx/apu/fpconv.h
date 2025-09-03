/*
 * Helper FP conversions
 *
 * Copyright (c) 2020-2025 Matt Borgerson
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


#ifndef FLOATCONV_H
#define FLOATCONV_H

#include <stdint.h>

static inline float int8_to_float(int8_t x)
{
    return x / 128.0f;
}

static inline float uint8_to_float(uint8_t value)
{
    return ((int)value - 0x80) / (1.0 * 0x80);
}

static inline float int16_to_float(int16_t value)
{
    return value / (1.0 * 0x8000);
}

static inline float s6p9_to_float(int16_t value)
{
    return value / 512.0f;
}

static inline float int32_to_float(int32_t value)
{
    return value / (1.0 * 0x80000000);
}

static inline float int24_to_float(int32_t value)
{
    return int32_to_float((uint32_t)value << 8);
}

static inline uint32_t float_to_24b(float value)
{
    double scaled_value = value * (8.0 * 0x100000);
    int int24;
    if (scaled_value >= (1.0 * 0x7fffff)) {
        int24 = 0x7fffff;
    } else if (scaled_value <= (-8.0 * 0x100000)) {
        int24 = -1 - 0x7fffff;
    } else {
        int24 = lrint(scaled_value);
    }
    return int24 & 0xffffff;
}

#endif
