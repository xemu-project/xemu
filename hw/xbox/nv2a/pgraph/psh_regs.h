/*
 * QEMU Geforce NV2A pixel shader translation
 *
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

#ifndef HW_NV2A_PSH_H
#define HW_NV2A_PSH_H

#include <stdint.h>
#include <stdbool.h>

enum PshAlphaFunc {
    ALPHA_FUNC_NEVER,
    ALPHA_FUNC_LESS,
    ALPHA_FUNC_EQUAL,
    ALPHA_FUNC_LEQUAL,
    ALPHA_FUNC_GREATER,
    ALPHA_FUNC_NOTEQUAL,
    ALPHA_FUNC_GEQUAL,
    ALPHA_FUNC_ALWAYS,
};

enum PshShadowDepthFunc {
    SHADOW_DEPTH_FUNC_NEVER,
    SHADOW_DEPTH_FUNC_LESS,
    SHADOW_DEPTH_FUNC_EQUAL,
    SHADOW_DEPTH_FUNC_LEQUAL,
    SHADOW_DEPTH_FUNC_GREATER,
    SHADOW_DEPTH_FUNC_NOTEQUAL,
    SHADOW_DEPTH_FUNC_GEQUAL,
    SHADOW_DEPTH_FUNC_ALWAYS,
};

enum ConvolutionFilter {
    CONVOLUTION_FILTER_DISABLED,
    CONVOLUTION_FILTER_QUINCUNX,
    CONVOLUTION_FILTER_GAUSSIAN,
};

#endif
