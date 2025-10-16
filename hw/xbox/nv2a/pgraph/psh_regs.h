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

/*
 * For some background, see the OpenGL extension:
 * https://www.opengl.org/registry/specs/NV/register_combiners.txt
 */

enum PS_TEXTUREMODES
{                                 // valid in stage 0 1 2 3
    PS_TEXTUREMODES_NONE=                 0x00L, // * * * *
    PS_TEXTUREMODES_PROJECT2D=            0x01L, // * * * *
    PS_TEXTUREMODES_PROJECT3D=            0x02L, // * * * *
    PS_TEXTUREMODES_CUBEMAP=              0x03L, // * * * *
    PS_TEXTUREMODES_PASSTHRU=             0x04L, // * * * *
    PS_TEXTUREMODES_CLIPPLANE=            0x05L, // * * * *
    PS_TEXTUREMODES_BUMPENVMAP=           0x06L, // - * * *
    PS_TEXTUREMODES_BUMPENVMAP_LUM=       0x07L, // - * * *
    PS_TEXTUREMODES_BRDF=                 0x08L, // - - * *
    PS_TEXTUREMODES_DOT_ST=               0x09L, // - - * *
    PS_TEXTUREMODES_DOT_ZW=               0x0aL, // - - * *
    PS_TEXTUREMODES_DOT_RFLCT_DIFF=       0x0bL, // - - * -
    PS_TEXTUREMODES_DOT_RFLCT_SPEC=       0x0cL, // - - - *
    PS_TEXTUREMODES_DOT_STR_3D=           0x0dL, // - - - *
    PS_TEXTUREMODES_DOT_STR_CUBE=         0x0eL, // - - - *
    PS_TEXTUREMODES_DPNDNT_AR=            0x0fL, // - * * *
    PS_TEXTUREMODES_DPNDNT_GB=            0x10L, // - * * *
    PS_TEXTUREMODES_DOTPRODUCT=           0x11L, // - * * -
    PS_TEXTUREMODES_DOT_RFLCT_SPEC_CONST= 0x12L, // - - - *
    // 0x13-0x1f reserved
};

enum PS_INPUTMAPPING
{
    PS_INPUTMAPPING_UNSIGNED_IDENTITY= 0x00L, // max(0,x)         OK for final combiner
    PS_INPUTMAPPING_UNSIGNED_INVERT=   0x20L, // 1 - max(0,x)     OK for final combiner
    PS_INPUTMAPPING_EXPAND_NORMAL=     0x40L, // 2*max(0,x) - 1   invalid for final combiner
    PS_INPUTMAPPING_EXPAND_NEGATE=     0x60L, // 1 - 2*max(0,x)   invalid for final combiner
    PS_INPUTMAPPING_HALFBIAS_NORMAL=   0x80L, // max(0,x) - 1/2   invalid for final combiner
    PS_INPUTMAPPING_HALFBIAS_NEGATE=   0xa0L, // 1/2 - max(0,x)   invalid for final combiner
    PS_INPUTMAPPING_SIGNED_IDENTITY=   0xc0L, // x                invalid for final combiner
    PS_INPUTMAPPING_SIGNED_NEGATE=     0xe0L, // -x               invalid for final combiner
};

enum PS_REGISTER
{
    PS_REGISTER_ZERO=              0x00L, // r
    PS_REGISTER_DISCARD=           0x00L, // w
    PS_REGISTER_C0=                0x01L, // r
    PS_REGISTER_C1=                0x02L, // r
    PS_REGISTER_FOG=               0x03L, // r
    PS_REGISTER_V0=                0x04L, // r/w
    PS_REGISTER_V1=                0x05L, // r/w
    PS_REGISTER_T0=                0x08L, // r/w
    PS_REGISTER_T1=                0x09L, // r/w
    PS_REGISTER_T2=                0x0aL, // r/w
    PS_REGISTER_T3=                0x0bL, // r/w
    PS_REGISTER_R0=                0x0cL, // r/w
    PS_REGISTER_R1=                0x0dL, // r/w
    PS_REGISTER_V1R0_SUM=          0x0eL, // r
    PS_REGISTER_EF_PROD=           0x0fL, // r

    PS_REGISTER_ONE=               PS_REGISTER_ZERO | PS_INPUTMAPPING_UNSIGNED_INVERT, // OK for final combiner
    PS_REGISTER_NEGATIVE_ONE=      PS_REGISTER_ZERO | PS_INPUTMAPPING_EXPAND_NORMAL,   // invalid for final combiner
    PS_REGISTER_ONE_HALF=          PS_REGISTER_ZERO | PS_INPUTMAPPING_HALFBIAS_NEGATE, // invalid for final combiner
    PS_REGISTER_NEGATIVE_ONE_HALF= PS_REGISTER_ZERO | PS_INPUTMAPPING_HALFBIAS_NORMAL, // invalid for final combiner
};

enum PS_COMBINERCOUNTFLAGS
{
    PS_COMBINERCOUNT_MUX_LSB=     0x0000L, // mux on r0.a lsb
    PS_COMBINERCOUNT_MUX_MSB=     0x0001L, // mux on r0.a msb

    PS_COMBINERCOUNT_SAME_C0=     0x0000L, // c0 same in each stage
    PS_COMBINERCOUNT_UNIQUE_C0=   0x0010L, // c0 unique in each stage

    PS_COMBINERCOUNT_SAME_C1=     0x0000L, // c1 same in each stage
    PS_COMBINERCOUNT_UNIQUE_C1=   0x0100L  // c1 unique in each stage
};

enum PS_COMBINEROUTPUT
{
    PS_COMBINEROUTPUT_IDENTITY=            0x00L, // y = x
    PS_COMBINEROUTPUT_BIAS=                0x08L, // y = x - 0.5
    PS_COMBINEROUTPUT_SHIFTLEFT_1=         0x10L, // y = x*2
    PS_COMBINEROUTPUT_SHIFTLEFT_1_BIAS=    0x18L, // y = (x - 0.5)*2
    PS_COMBINEROUTPUT_SHIFTLEFT_2=         0x20L, // y = x*4
    PS_COMBINEROUTPUT_SHIFTRIGHT_1=        0x30L, // y = x/2

    PS_COMBINEROUTPUT_AB_BLUE_TO_ALPHA=    0x80L, // RGB only

    PS_COMBINEROUTPUT_CD_BLUE_TO_ALPHA=    0x40L, // RGB only

    PS_COMBINEROUTPUT_AB_MULTIPLY=         0x00L,
    PS_COMBINEROUTPUT_AB_DOT_PRODUCT=      0x02L, // RGB only

    PS_COMBINEROUTPUT_CD_MULTIPLY=         0x00L,
    PS_COMBINEROUTPUT_CD_DOT_PRODUCT=      0x01L, // RGB only

    PS_COMBINEROUTPUT_AB_CD_SUM=           0x00L, // 3rd output is AB+CD
    PS_COMBINEROUTPUT_AB_CD_MUX=           0x04L, // 3rd output is MUX(AB,CD) based on R0.a
};

enum PS_CHANNEL
{
    PS_CHANNEL_RGB=   0x00, // used as RGB source
    PS_CHANNEL_BLUE=  0x00, // used as ALPHA source
    PS_CHANNEL_ALPHA= 0x10, // used as RGB or ALPHA source
};


enum PS_FINALCOMBINERSETTING
{
    PS_FINALCOMBINERSETTING_CLAMP_SUM=     0x80, // V1+R0 sum clamped to [0,1]

    PS_FINALCOMBINERSETTING_COMPLEMENT_V1= 0x40, // unsigned invert mapping

    PS_FINALCOMBINERSETTING_COMPLEMENT_R0= 0x20, // unsigned invert mapping
};

enum PS_DOTMAPPING
{                              // valid in stage 0 1 2 3
    PS_DOTMAPPING_ZERO_TO_ONE=         0x00L, // - * * *
    PS_DOTMAPPING_MINUS1_TO_1_D3D=     0x01L, // - * * *
    PS_DOTMAPPING_MINUS1_TO_1_GL=      0x02L, // - * * *
    PS_DOTMAPPING_MINUS1_TO_1=         0x03L, // - * * *
    PS_DOTMAPPING_HILO_1=              0x04L, // - * * *
    PS_DOTMAPPING_HILO_HEMISPHERE_D3D= 0x05L, // - * * *
    PS_DOTMAPPING_HILO_HEMISPHERE_GL=  0x06L, // - * * *
    PS_DOTMAPPING_HILO_HEMISPHERE=     0x07L, // - * * *
};

enum PS_COLORKEYMODE {
    COLOR_KEY_NONE = 0,
    COLOR_KEY_KILL_ALPHA = 1,
    COLOR_KEY_KILL_COLOR_AND_ALPHA = 2,
    COLOR_KEY_DISCARD = 3,
};

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
