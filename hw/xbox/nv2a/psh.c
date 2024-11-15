/*
 * QEMU Geforce NV2A pixel shader translation
 *
 * Copyright (c) 2013 espes
 * Copyright (c) 2015 Jannik Vogel
 * Copyright (c) 2020-2021 Matt Borgerson
 *
 * Based on:
 * Cxbx, PixelShader.cpp
 * Copyright (c) 2004 Aaron Robinson <caustik@caustik.com>
 *                    Kingofc <kingofc@freenet.de>
 * Xeon, XBD3DPixelShader.cpp
 * Copyright (c) 2003 _SF_
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "qapi/qmp/qstring.h"

#include "shaders_common.h"
#include "psh.h"

/*
 * This implements translation of register combiners into glsl
 * fragment shaders, but all terminology is in terms of Xbox DirectX
 * pixel shaders, since I wanted to be lazy while referencing existing
 * work / stealing code.
 *
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


// Structures to describe the PS definition

struct InputInfo {
    int reg, mod, chan;
};

struct InputVarInfo {
    struct InputInfo a, b, c, d;
};

struct FCInputInfo {
    struct InputInfo a, b, c, d, e, f, g;
    bool v1r0_sum, clamp_sum, inv_v1, inv_r0, enabled;
};

struct OutputInfo {
    int ab, cd, muxsum, flags, ab_op, cd_op, muxsum_op,
        mapping, ab_alphablue, cd_alphablue;
};

struct PSStageInfo {
    struct InputVarInfo rgb_input, alpha_input;
    struct OutputInfo rgb_output, alpha_output;
    int c0, c1;
};

struct PixelShader {
    PshState state;

    int num_stages, flags;
    struct PSStageInfo stage[8];
    struct FCInputInfo final_input;
    int tex_modes[4], input_tex[4], dot_map[4];

    MString *varE, *varF;
    MString *code;
    int cur_stage;

    int num_var_refs;
    char var_refs[32][32];
    int num_const_refs;
    char const_refs[32][32];
};

static void add_var_ref(struct PixelShader *ps, const char *var)
{
    int i;
    for (i=0; i<ps->num_var_refs; i++) {
        if (strcmp((char*)ps->var_refs[i], var) == 0) return;
    }
    strcpy((char*)ps->var_refs[ps->num_var_refs++], var);
}

static void add_const_ref(struct PixelShader *ps, const char *var)
{
    int i;
    for (i=0; i<ps->num_const_refs; i++) {
        if (strcmp((char*)ps->const_refs[i], var) == 0) return;
    }
    strcpy((char*)ps->const_refs[ps->num_const_refs++], var);
}

// Get the code for a variable used in the program
static MString* get_var(struct PixelShader *ps, int reg, bool is_dest)
{
    switch (reg) {
    case PS_REGISTER_DISCARD:
        if (is_dest) {
            return mstring_from_str("");
        } else {
            return mstring_from_str("vec4(0.0)");
        }
        break;
    case PS_REGISTER_C0:
        if (ps->flags & PS_COMBINERCOUNT_UNIQUE_C0 || ps->cur_stage == 8) {
            MString *reg = mstring_from_fmt("c0_%d", ps->cur_stage);
            add_const_ref(ps, mstring_get_str(reg));
            return reg;
        } else {  // Same c0
            add_const_ref(ps, "c0_0");
            return mstring_from_str("c0_0");
        }
        break;
    case PS_REGISTER_C1:
        if (ps->flags & PS_COMBINERCOUNT_UNIQUE_C1 || ps->cur_stage == 8) {
            MString *reg = mstring_from_fmt("c1_%d", ps->cur_stage);
            add_const_ref(ps, mstring_get_str(reg));
            return reg;
        } else {  // Same c1
            add_const_ref(ps, "c1_0");
            return mstring_from_str("c1_0");
        }
        break;
    case PS_REGISTER_FOG:
        return mstring_from_str("pFog");
    case PS_REGISTER_V0:
        return mstring_from_str("v0");
    case PS_REGISTER_V1:
        return mstring_from_str("v1");
    case PS_REGISTER_T0:
        return mstring_from_str("t0");
    case PS_REGISTER_T1:
        return mstring_from_str("t1");
    case PS_REGISTER_T2:
        return mstring_from_str("t2");
    case PS_REGISTER_T3:
        return mstring_from_str("t3");
    case PS_REGISTER_R0:
        add_var_ref(ps, "r0");
        return mstring_from_str("r0");
    case PS_REGISTER_R1:
        add_var_ref(ps, "r1");
        return mstring_from_str("r1");
    case PS_REGISTER_V1R0_SUM:
        add_var_ref(ps, "r0");
        if (ps->final_input.clamp_sum) {
            return mstring_from_fmt(
                    "clamp(vec4(%s.rgb + %s.rgb, 0.0), 0.0, 1.0)",
                    ps->final_input.inv_v1 ? "(1.0 - v1)" : "v1",
                    ps->final_input.inv_r0 ? "(1.0 - r0)" : "r0");
        } else {
            return mstring_from_fmt(
                    "vec4(%s.rgb + %s.rgb, 0.0)",
                    ps->final_input.inv_v1 ? "(1.0 - v1)" : "v1",
                    ps->final_input.inv_r0 ? "(1.0 - r0)" : "r0");
        }
    case PS_REGISTER_EF_PROD:
        return mstring_from_fmt("vec4(%s * %s, 0.0)",
                                mstring_get_str(ps->varE),
                                mstring_get_str(ps->varF));
    default:
        assert(false);
        return NULL;
    }
}

// Get input variable code
static MString* get_input_var(struct PixelShader *ps, struct InputInfo in, bool is_alpha)
{
    MString *reg = get_var(ps, in.reg, false);

    if (!is_alpha) {
        switch (in.chan) {
        case PS_CHANNEL_RGB:
            mstring_append(reg, ".rgb");
            break;
        case PS_CHANNEL_ALPHA:
            mstring_append(reg, ".aaa");
            break;
        default:
            assert(false);
            break;
        }
    } else {
        switch (in.chan) {
        case PS_CHANNEL_BLUE:
            mstring_append(reg, ".b");
            break;
        case PS_CHANNEL_ALPHA:
            mstring_append(reg, ".a");
            break;
        default:
            assert(false);
            break;
        }
    }

    MString *res;
    switch (in.mod) {
    case PS_INPUTMAPPING_UNSIGNED_IDENTITY:
        res = mstring_from_fmt("max(%s, 0.0)", mstring_get_str(reg));
        break;
    case PS_INPUTMAPPING_UNSIGNED_INVERT:
        res = mstring_from_fmt("(1.0 - clamp(%s, 0.0, 1.0))", mstring_get_str(reg));
        break;
    case PS_INPUTMAPPING_EXPAND_NORMAL:
        res = mstring_from_fmt("(2.0 * max(%s, 0.0) - 1.0)", mstring_get_str(reg));
        break;
    case PS_INPUTMAPPING_EXPAND_NEGATE:
        res = mstring_from_fmt("(-2.0 * max(%s, 0.0) + 1.0)", mstring_get_str(reg));
        break;
    case PS_INPUTMAPPING_HALFBIAS_NORMAL:
        res = mstring_from_fmt("(max(%s, 0.0) - 0.5)", mstring_get_str(reg));
        break;
    case PS_INPUTMAPPING_HALFBIAS_NEGATE:
        res = mstring_from_fmt("(-max(%s, 0.0) + 0.5)", mstring_get_str(reg));
        break;
    case PS_INPUTMAPPING_SIGNED_IDENTITY:
        mstring_ref(reg);
        res = reg;
        break;
    case PS_INPUTMAPPING_SIGNED_NEGATE:
        res = mstring_from_fmt("-%s", mstring_get_str(reg));
        break;
    default:
        assert(false);
        break;
    }

    mstring_unref(reg);
    return res;
}

// Get code for the output mapping of a stage
static MString* get_output(MString *reg, int mapping)
{
    MString *res;
    switch (mapping) {
    case PS_COMBINEROUTPUT_IDENTITY:
        mstring_ref(reg);
        res = reg;
        break;
    case PS_COMBINEROUTPUT_BIAS:
        res = mstring_from_fmt("(%s - 0.5)", mstring_get_str(reg));
        break;
    case PS_COMBINEROUTPUT_SHIFTLEFT_1:
        res = mstring_from_fmt("(%s * 2.0)", mstring_get_str(reg));
        break;
    case PS_COMBINEROUTPUT_SHIFTLEFT_1_BIAS:
        res = mstring_from_fmt("((%s - 0.5) * 2.0)", mstring_get_str(reg));
        break;
    case PS_COMBINEROUTPUT_SHIFTLEFT_2:
        res = mstring_from_fmt("(%s * 4.0)", mstring_get_str(reg));
        break;
    case PS_COMBINEROUTPUT_SHIFTRIGHT_1:
        res = mstring_from_fmt("(%s / 2.0)", mstring_get_str(reg));
        break;
    default:
        assert(false);
        break;
    }
    return res;
}

// Add the GLSL code for a stage
static MString* add_stage_code(struct PixelShader *ps,
                               struct InputVarInfo input,
                               struct OutputInfo output,
                               const char *write_mask, bool is_alpha)
{
    MString *ret = mstring_new();
    MString *a = get_input_var(ps, input.a, is_alpha);
    MString *b = get_input_var(ps, input.b, is_alpha);
    MString *c = get_input_var(ps, input.c, is_alpha);
    MString *d = get_input_var(ps, input.d, is_alpha);

    const char *caster = "";
    if (strlen(write_mask) == 3) {
        caster = "vec3";
    }

    MString *ab;
    if (output.ab_op == PS_COMBINEROUTPUT_AB_DOT_PRODUCT) {
        ab = mstring_from_fmt("dot(%s, %s)",
                              mstring_get_str(a), mstring_get_str(b));
    } else {
        ab = mstring_from_fmt("(%s * %s)",
                              mstring_get_str(a), mstring_get_str(b));
    }

    MString *cd;
    if (output.cd_op == PS_COMBINEROUTPUT_CD_DOT_PRODUCT) {
        cd = mstring_from_fmt("dot(%s, %s)",
                              mstring_get_str(c), mstring_get_str(d));
    } else {
        cd = mstring_from_fmt("(%s * %s)",
                              mstring_get_str(c), mstring_get_str(d));
    }

    MString *ab_mapping = get_output(ab, output.mapping);
    MString *cd_mapping = get_output(cd, output.mapping);
    MString *ab_dest = get_var(ps, output.ab, true);
    MString *cd_dest = get_var(ps, output.cd, true);
    MString *muxsum_dest = get_var(ps, output.muxsum, true);

    bool assign_ab = false;
    bool assign_cd = false;
    bool assign_muxsum = false;

    if (mstring_get_length(ab_dest)) {
        mstring_append_fmt(ps->code, "ab.%s = clamp(%s(%s), -1.0, 1.0);\n",
                           write_mask, caster, mstring_get_str(ab_mapping));
        assign_ab = true;
    } else {
        mstring_unref(ab_dest);
        mstring_ref(ab_mapping);
        ab_dest = ab_mapping;
    }

    if (mstring_get_length(cd_dest)) {
        mstring_append_fmt(ps->code, "cd.%s = clamp(%s(%s), -1.0, 1.0);\n",
                           write_mask, caster, mstring_get_str(cd_mapping));
        assign_cd = true;
    } else {
        mstring_unref(cd_dest);
        mstring_ref(cd_mapping);
        cd_dest = cd_mapping;
    }

    MString *muxsum;
    if (output.muxsum_op == PS_COMBINEROUTPUT_AB_CD_SUM) {
        muxsum = mstring_from_fmt("(%s + %s)", mstring_get_str(ab),
                                  mstring_get_str(cd));
    } else {
        muxsum = mstring_from_fmt("((%s) ? %s(%s) : %s(%s))",
                                  (ps->flags & PS_COMBINERCOUNT_MUX_MSB) ?
                                      "r0.a >= 0.5" :
                                      "(uint(r0.a * 255.0) & 1u) == 1u",
                                  caster, mstring_get_str(cd), caster,
                                  mstring_get_str(ab));
    }

    MString *muxsum_mapping = get_output(muxsum, output.mapping);
    if (mstring_get_length(muxsum_dest)) {
        mstring_append_fmt(ps->code, "mux_sum.%s = clamp(%s(%s), -1.0, 1.0);\n",
                           write_mask, caster, mstring_get_str(muxsum_mapping));
        assign_muxsum = true;
    }

    if (assign_ab) {
        mstring_append_fmt(ret, "%s.%s = ab.%s;\n",
                           mstring_get_str(ab_dest), write_mask, write_mask);

        if (!is_alpha && output.flags & PS_COMBINEROUTPUT_AB_BLUE_TO_ALPHA) {
            mstring_append_fmt(ret, "%s.a = ab.b;\n",
                               mstring_get_str(ab_dest));
        }
    }
    if (assign_cd) {
        mstring_append_fmt(ret, "%s.%s = cd.%s;\n",
                           mstring_get_str(cd_dest), write_mask, write_mask);

        if (!is_alpha && output.flags & PS_COMBINEROUTPUT_CD_BLUE_TO_ALPHA) {
            mstring_append_fmt(ret, "%s.a = cd.b;\n",
                               mstring_get_str(cd_dest));
        }
    }
    if (assign_muxsum) {
        mstring_append_fmt(ret, "%s.%s = mux_sum.%s;\n",
                           mstring_get_str(muxsum_dest), write_mask, write_mask);
    }

    mstring_unref(a);
    mstring_unref(b);
    mstring_unref(c);
    mstring_unref(d);
    mstring_unref(ab);
    mstring_unref(cd);
    mstring_unref(ab_mapping);
    mstring_unref(cd_mapping);
    mstring_unref(ab_dest);
    mstring_unref(cd_dest);
    mstring_unref(muxsum_dest);
    mstring_unref(muxsum);
    mstring_unref(muxsum_mapping);

    return ret;
}

// Add code for the final combiner stage
static void add_final_stage_code(struct PixelShader *ps, struct FCInputInfo final)
{
    ps->varE = get_input_var(ps, final.e, false);
    ps->varF = get_input_var(ps, final.f, false);

    MString *a = get_input_var(ps, final.a, false);
    MString *b = get_input_var(ps, final.b, false);
    MString *c = get_input_var(ps, final.c, false);
    MString *d = get_input_var(ps, final.d, false);
    MString *g = get_input_var(ps, final.g, true);

    mstring_append_fmt(ps->code, "fragColor.rgb = %s + mix(vec3(%s), vec3(%s), vec3(%s));\n",
                       mstring_get_str(d), mstring_get_str(c),
                       mstring_get_str(b), mstring_get_str(a));
    mstring_append_fmt(ps->code, "fragColor.a = %s;\n", mstring_get_str(g));

    mstring_unref(a);
    mstring_unref(b);
    mstring_unref(c);
    mstring_unref(d);
    mstring_unref(g);

    mstring_unref(ps->varE);
    mstring_unref(ps->varF);
    ps->varE = ps->varF = NULL;
}

static const char sampler2D[] = "sampler2D";
static const char sampler3D[] = "sampler3D";
static const char samplerCube[] = "samplerCube";
static const char sampler2DRect[] = "sampler2DRect";

static const char* get_sampler_type(enum PS_TEXTUREMODES mode, const PshState *state, int i)
{
    switch (mode) {
    default:
    case PS_TEXTUREMODES_NONE:
        return NULL;

    case PS_TEXTUREMODES_PROJECT2D:
        return state->rect_tex[i] ? sampler2DRect : sampler2D;

    case PS_TEXTUREMODES_BUMPENVMAP:
    case PS_TEXTUREMODES_BUMPENVMAP_LUM:
    case PS_TEXTUREMODES_DOT_ST:
        if (state->shadow_map[i]) {
            fprintf(stderr, "Shadow map support not implemented for mode %d\n", mode);
            assert(!"Shadow map support not implemented for this mode");
        }
        return state->rect_tex[i] ? sampler2DRect : sampler2D;

    case PS_TEXTUREMODES_PROJECT3D:
    case PS_TEXTUREMODES_DOT_STR_3D:
        if (state->shadow_map[i]) {
            return state->rect_tex[i] ? sampler2DRect : sampler2D;
        }
        return sampler3D;

    case PS_TEXTUREMODES_CUBEMAP:
    case PS_TEXTUREMODES_DOT_RFLCT_DIFF:
    case PS_TEXTUREMODES_DOT_RFLCT_SPEC:
    case PS_TEXTUREMODES_DOT_STR_CUBE:
        if (state->shadow_map[i]) {
            fprintf(stderr, "Shadow map support not implemented for mode %d\n", mode);
            assert(!"Shadow map support not implemented for this mode");
        }
        return samplerCube;

    case PS_TEXTUREMODES_DPNDNT_AR:
    case PS_TEXTUREMODES_DPNDNT_GB:
        if (state->shadow_map[i]) {
            fprintf(stderr, "Shadow map support not implemented for mode %d\n", mode);
            assert(!"Shadow map support not implemented for this mode");
        }
        return sampler2D;
    }
}

static const char *shadow_comparison_map[] = {
    [SHADOW_DEPTH_FUNC_LESS] = "<",
    [SHADOW_DEPTH_FUNC_EQUAL] = "==",
    [SHADOW_DEPTH_FUNC_LEQUAL] = "<=",
    [SHADOW_DEPTH_FUNC_GREATER] = ">",
    [SHADOW_DEPTH_FUNC_NOTEQUAL] = "!=",
    [SHADOW_DEPTH_FUNC_GEQUAL] = ">=",
};

static void psh_append_shadowmap(const struct PixelShader *ps, int i, bool compare_z, MString *vars)
{
    if (ps->state.shadow_depth_func == SHADOW_DEPTH_FUNC_NEVER) {
        mstring_append_fmt(vars, "vec4 t%d = vec4(0.0);\n", i);
        return;
    }

    if (ps->state.shadow_depth_func == SHADOW_DEPTH_FUNC_ALWAYS) {
        mstring_append_fmt(vars, "vec4 t%d = vec4(1.0);\n", i);
        return;
    }

    mstring_append_fmt(vars,
                       "pT%d.xy *= texScale%d;\n"
                       "vec4 t%d_depth = textureProj(texSamp%d, pT%d.xyw);\n",
                       i, i, i, i, i);

    const char *comparison = shadow_comparison_map[ps->state.shadow_depth_func];

    // Depth.y != 0 indicates 24 bit; depth.z != 0 indicates float.
    if (compare_z) {
        mstring_append_fmt(
            vars,
            "float t%d_max_depth;\n"
            "if (t%d_depth.y > 0) {\n"
            "  t%d_max_depth = 0xFFFFFF;\n"
            "} else {\n"
            "  t%d_max_depth = t%d_depth.z > 0 ? 511.9375 : 0xFFFF;\n"
            "}\n"
            "t%d_depth.x *= t%d_max_depth;\n"
            "pT%d.z = clamp(pT%d.z / pT%d.w, 0, t%d_max_depth);\n"
            "vec4 t%d = vec4(t%d_depth.x %s pT%d.z ? 1.0 : 0.0);\n",
            i, i, i, i, i,
            i, i, i, i, i, i,
            i, i, comparison, i);
    } else {
        mstring_append_fmt(
            vars,
            "vec4 t%d = vec4(t%d_depth.x %s 0.0 ? 1.0 : 0.0);\n",
            i, i, comparison);
    }
}

// Adjust the s, t coordinates in the given VAR to account for the 4 texel
// border supported by the hardware.
static void apply_border_adjustment(const struct PixelShader *ps, MString *vars, int tex_index, const char *var_template)
{
    int i = tex_index;
    if (ps->state.border_logical_size[i][0] == 0.0f) {
        return;
    }

    char var_name[32] = {0};
    snprintf(var_name, sizeof(var_name), var_template, i);

    mstring_append_fmt(
        vars,
        "vec3 t%dLogicalSize = vec3(%f, %f, %f);\n"
        "%s.xyz = (%s.xyz * t%dLogicalSize + vec3(4, 4, 4)) * vec3(%f, %f, %f);\n",
        i, ps->state.border_logical_size[i][0], ps->state.border_logical_size[i][1], ps->state.border_logical_size[i][2],
        var_name, var_name, i, ps->state.border_inv_real_size[i][0], ps->state.border_inv_real_size[i][1], ps->state.border_inv_real_size[i][2]);
}

static MString* psh_convert(struct PixelShader *ps)
{
    int i;

    MString *preflight = mstring_new();
    mstring_append(preflight, ps->state.smooth_shading ?
                                  STRUCT_VERTEX_DATA_IN_SMOOTH :
                                  STRUCT_VERTEX_DATA_IN_FLAT);
    mstring_append(preflight, "\n");
    mstring_append(preflight, "out vec4 fragColor;\n");
    mstring_append(preflight, "\n");
    mstring_append(preflight, "uniform vec4 fogColor;\n");

    const char *dotmap_funcs[] = {
        "dotmap_zero_to_one",
        "dotmap_minus1_to_1_d3d",
        "dotmap_minus1_to_1_gl",
        "dotmap_minus1_to_1",
        "dotmap_hilo_1",
        "dotmap_hilo_hemisphere_d3d",
        "dotmap_hilo_hemisphere_gl",
        "dotmap_hilo_hemisphere",
    };

    mstring_append_fmt(preflight,
        "float sign1(float x) {\n"
        "    x *= 255.0;\n"
        "    return (x-128.0)/127.0;\n"
        "}\n"
        "float sign2(float x) {\n"
        "    x *= 255.0;\n"
        "    if (x >= 128.0) return (x-255.5)/127.5;\n"
        "               else return (x+0.5)/127.5;\n"
        "}\n"
        "float sign3(float x) {\n"
        "    x *= 255.0;\n"
        "    if (x >= 128.0) return (x-256.0)/127.0;\n"
        "               else return (x)/127.0;\n"
        "}\n"
        "float sign3_to_0_to_1(float x) {\n"
        "    if (x >= 0) return x/2;\n"
        "           else return 1+x/2;\n"
        "}\n"
        "vec3 dotmap_zero_to_one(vec4 col) {\n"
        "    return col.rgb;\n"
        "}\n"
        "vec3 dotmap_minus1_to_1_d3d(vec4 col) {\n"
        "    return vec3(sign1(col.r),sign1(col.g),sign1(col.b));\n"
        "}\n"
        "vec3 dotmap_minus1_to_1_gl(vec4 col) {\n"
        "    return vec3(sign2(col.r),sign2(col.g),sign2(col.b));\n"
        "}\n"
        "vec3 dotmap_minus1_to_1(vec4 col) {\n"
        "    return vec3(sign3(col.r),sign3(col.g),sign3(col.b));\n"
        "}\n"
        "vec3 dotmap_hilo_1(vec4 col) {\n"
        "    uint hi_i = uint(col.a * float(0xff)) << 8\n"
        "              | uint(col.r * float(0xff));\n"
        "    uint lo_i = uint(col.g * float(0xff)) << 8\n"
        "              | uint(col.b * float(0xff));\n"
        "    float hi_f = float(hi_i) / float(0xffff);\n"
        "    float lo_f = float(lo_i) / float(0xffff);\n"
        "    return vec3(hi_f, lo_f, 1.0);\n"
        "}\n"
        "vec3 dotmap_hilo_hemisphere_d3d(vec4 col) {\n"
        "    return col.rgb;\n" // FIXME
        "}\n"
        "vec3 dotmap_hilo_hemisphere_gl(vec4 col) {\n"
        "    return col.rgb;\n" // FIXME
        "}\n"
        "vec3 dotmap_hilo_hemisphere(vec4 col) {\n"
        "    return col.rgb;\n" // FIXME
        "}\n"
        "const float[9] gaussian3x3 = float[9](\n"
        "    1.0/16.0, 2.0/16.0, 1.0/16.0,\n"
        "    2.0/16.0, 4.0/16.0, 2.0/16.0,\n"
        "    1.0/16.0, 2.0/16.0, 1.0/16.0);\n"
        "const vec2[9] convolution3x3 = vec2[9](\n"
        "    vec2(-1.0,-1.0),vec2(0.0,-1.0),vec2(1.0,-1.0),\n"
        "    vec2(-1.0, 0.0),vec2(0.0, 0.0),vec2(1.0, 0.0),\n"
        "    vec2(-1.0, 1.0),vec2(0.0, 1.0),vec2(1.0, 1.0));\n"
        "vec4 gaussianFilter2DRectProj(sampler2DRect sampler, vec3 texCoord) {\n"
        "    vec4 sum = vec4(0.0);\n"
        "    for (int i = 0; i < 9; i++) {\n"
        "        sum += gaussian3x3[i]*textureProj(sampler,\n"
        "                   texCoord + vec3(convolution3x3[i], 0.0));\n"
        "    }\n"
        "    return sum;\n"
        "}\n"
        );

    /* Window Clipping */
    MString *clip = mstring_new();
    mstring_append(preflight, "uniform ivec4 clipRegion[8];\n");
    mstring_append_fmt(clip, "/*  Window-clip (%s) */\n",
                       ps->state.window_clip_exclusive ?
                           "Exclusive" : "Inclusive");
    if (!ps->state.window_clip_exclusive) {
        mstring_append(clip, "bool clipContained = false;\n");
    }
    mstring_append(clip, "vec2 coord = gl_FragCoord.xy - 0.5;\n"
                         "for (int i = 0; i < 8; i++) {\n"
                         "  bool outside = any(bvec4(\n"
                         "      lessThan(coord, vec2(clipRegion[i].xy)),\n"
                         "      greaterThanEqual(coord, vec2(clipRegion[i].zw))));\n"
                         "  if (!outside) {\n");
    if (ps->state.window_clip_exclusive) {
        mstring_append(clip, "    discard;\n");
    } else {
        mstring_append(clip, "    clipContained = true;\n"
                             "    break;\n");
    }
    mstring_append(clip, "  }\n"
                         "}\n");
    if (!ps->state.window_clip_exclusive) {
        mstring_append(clip, "if (!clipContained) {\n"
                             "  discard;\n"
                             "}\n");
    }

    /* calculate perspective-correct inputs */
    MString *vars = mstring_new();
    if (ps->state.smooth_shading) {
        mstring_append(vars, "vec4 pD0 = vtxD0 / vtx_inv_w;\n");
        mstring_append(vars, "vec4 pD1 = vtxD1 / vtx_inv_w;\n");
        mstring_append(vars, "vec4 pB0 = vtxB0 / vtx_inv_w;\n");
        mstring_append(vars, "vec4 pB1 = vtxB1 / vtx_inv_w;\n");
    } else {
        mstring_append(vars, "vec4 pD0 = vtxD0 / vtx_inv_w_flat;\n");
        mstring_append(vars, "vec4 pD1 = vtxD1 / vtx_inv_w_flat;\n");
        mstring_append(vars, "vec4 pB0 = vtxB0 / vtx_inv_w_flat;\n");
        mstring_append(vars, "vec4 pB1 = vtxB1 / vtx_inv_w_flat;\n");
    }
    mstring_append(vars, "vec4 pFog = vec4(fogColor.rgb, clamp(vtxFog / vtx_inv_w, 0.0, 1.0));\n");
    mstring_append(vars, "vec4 pT0 = vtxT0 / vtx_inv_w;\n");
    mstring_append(vars, "vec4 pT1 = vtxT1 / vtx_inv_w;\n");
    mstring_append(vars, "vec4 pT2 = vtxT2 / vtx_inv_w;\n");
    if (ps->state.point_sprite) {
        assert(!ps->state.rect_tex[3]);
        mstring_append(vars, "vec4 pT3 = vec4(gl_PointCoord, 1.0, 1.0);\n");
    } else {
        mstring_append(vars, "vec4 pT3 = vtxT3 / vtx_inv_w;\n");
    }
    mstring_append(vars, "\n");
    mstring_append(vars, "vec4 v0 = pD0;\n");
    mstring_append(vars, "vec4 v1 = pD1;\n");
    mstring_append(vars, "vec4 ab;\n");
    mstring_append(vars, "vec4 cd;\n");
    mstring_append(vars, "vec4 mux_sum;\n");

    ps->code = mstring_new();

    for (i = 0; i < 4; i++) {

        const char *sampler_type = get_sampler_type(ps->tex_modes[i], &ps->state, i);

        assert(ps->dot_map[i] < 8);
        const char *dotmap_func = dotmap_funcs[ps->dot_map[i]];
        if (ps->dot_map[i] > 3) {
            NV2A_UNIMPLEMENTED("Dot Mapping mode %s", dotmap_func);
        }

        switch (ps->tex_modes[i]) {
        case PS_TEXTUREMODES_NONE:
            mstring_append_fmt(vars, "vec4 t%d = vec4(0.0); /* PS_TEXTUREMODES_NONE */\n",
                               i);
            break;
        case PS_TEXTUREMODES_PROJECT2D: {
            if (ps->state.shadow_map[i]) {
                psh_append_shadowmap(ps, i, false, vars);
            } else {
                const char *lookup = "textureProj";
                if ((ps->state.conv_tex[i] == CONVOLUTION_FILTER_GAUSSIAN)
                    || (ps->state.conv_tex[i] == CONVOLUTION_FILTER_QUINCUNX)) {
                    /* FIXME: Quincunx looks better than Linear and costs less than
                     * Gaussian, but Gaussian should be plenty fast so use it for
                     * now.
                     */
                    if (ps->state.rect_tex[i]) {
                        lookup = "gaussianFilter2DRectProj";
                    } else {
                        NV2A_UNIMPLEMENTED("Convolution for 2D textures");
                    }
                }
                apply_border_adjustment(ps, vars, i, "pT%d");
                mstring_append_fmt(vars, "pT%d.xy = texScale%d * pT%d.xy;\n", i, i, i);
                mstring_append_fmt(vars, "vec4 t%d = %s(texSamp%d, pT%d.xyw);\n",
                                   i, lookup, i, i);
            }
            break;
        }
        case PS_TEXTUREMODES_PROJECT3D:
            if (ps->state.shadow_map[i]) {
                psh_append_shadowmap(ps, i, true, vars);
            } else {
                apply_border_adjustment(ps, vars, i, "pT%d");
                mstring_append_fmt(vars, "vec4 t%d = textureProj(texSamp%d, pT%d.xyzw);\n",
                                   i, i, i);
            }
            break;
        case PS_TEXTUREMODES_CUBEMAP:
            mstring_append_fmt(vars, "vec4 t%d = texture(texSamp%d, pT%d.xyz / pT%d.w);\n",
                               i, i, i, i);
            break;
        case PS_TEXTUREMODES_PASSTHRU:
            assert(ps->state.border_logical_size[i][0] == 0.0f && "Unexpected border texture on passthru");
            mstring_append_fmt(vars, "vec4 t%d = pT%d;\n", i, i);
            break;
        case PS_TEXTUREMODES_CLIPPLANE: {
            int j;
            mstring_append_fmt(vars, "vec4 t%d = vec4(0.0); /* PS_TEXTUREMODES_CLIPPLANE */\n",
                               i);
            for (j = 0; j < 4; j++) {
                mstring_append_fmt(vars, "  if(pT%d.%c %s 0.0) { discard; };\n",
                                   i, "xyzw"[j],
                                   ps->state.compare_mode[i][j] ? ">=" : "<");
            }
            break;
        }
        case PS_TEXTUREMODES_BUMPENVMAP:
            assert(i >= 1);
            mstring_append_fmt(preflight, "uniform mat2 bumpMat%d;\n", i);

            if (ps->state.snorm_tex[ps->input_tex[i]]) {
                /* Input color channels already signed (FIXME: May not always want signed textures in this case) */
                mstring_append_fmt(vars, "vec2 dsdt%d = t%d.bg;\n",
                                   i, ps->input_tex[i]);
            } else {
                /* Convert to signed (FIXME: loss of accuracy due to filtering/interpolation) */
                mstring_append_fmt(vars, "vec2 dsdt%d = vec2(sign3(t%d.b), sign3(t%d.g));\n",
                                   i, ps->input_tex[i], ps->input_tex[i]);
            }

            mstring_append_fmt(vars, "dsdt%d = bumpMat%d * dsdt%d;\n",
                i, i, i, i);
            mstring_append_fmt(vars, "vec4 t%d = texture(texSamp%d, texScale%d * (pT%d.xy + dsdt%d));\n",
                i, i, i, i, i);
            break;
        case PS_TEXTUREMODES_BUMPENVMAP_LUM:
            assert(i >= 1);
            mstring_append_fmt(preflight, "uniform float bumpScale%d;\n", i);
            mstring_append_fmt(preflight, "uniform float bumpOffset%d;\n", i);
            mstring_append_fmt(preflight, "uniform mat2 bumpMat%d;\n", i);

            if (ps->state.snorm_tex[ps->input_tex[i]]) {
                /* Input color channels already signed (FIXME: May not always want signed textures in this case) */
                mstring_append_fmt(vars, "vec3 dsdtl%d = vec3(t%d.bg, sign3_to_0_to_1(t%d.r));\n",
                                   i, ps->input_tex[i], ps->input_tex[i]);
            } else {
                /* Convert to signed (FIXME: loss of accuracy due to filtering/interpolation) */
                mstring_append_fmt(vars, "vec3 dsdtl%d = vec3(sign3(t%d.b), sign3(t%d.g), t%d.r);\n",
                                   i, ps->input_tex[i], ps->input_tex[i], ps->input_tex[i]);
            }

            mstring_append_fmt(vars, "dsdtl%d.st = bumpMat%d * dsdtl%d.st;\n",
                i, i, i, i);
            mstring_append_fmt(vars, "vec4 t%d = texture(texSamp%d, texScale%d * (pT%d.xy + dsdtl%d.st));\n",
                i, i, i, i, i);
            mstring_append_fmt(vars, "t%d = t%d * (bumpScale%d * dsdtl%d.p + bumpOffset%d);\n",
                i, i, i, i, i);
            break;
        case PS_TEXTUREMODES_BRDF:
            assert(i >= 2);
            mstring_append_fmt(vars, "vec4 t%d = vec4(0.0); /* PS_TEXTUREMODES_BRDF */\n",
                               i);
            NV2A_UNIMPLEMENTED("PS_TEXTUREMODES_BRDF");
            break;
        case PS_TEXTUREMODES_DOT_ST:
            assert(i >= 2);
            mstring_append_fmt(vars, "/* PS_TEXTUREMODES_DOT_ST */\n");
            mstring_append_fmt(vars,
               "float dot%d = dot(pT%d.xyz, %s(t%d));\n"
               "vec2 dotST%d = vec2(dot%d, dot%d);\n",
                i, i, dotmap_func, ps->input_tex[i], i, i-1, i);

            apply_border_adjustment(ps, vars, i, "dotST%d");
            mstring_append_fmt(vars, "vec4 t%d = texture(texSamp%d, texScale%d * dotST%d);\n",
                i, i, i, i);
            break;
        case PS_TEXTUREMODES_DOT_ZW:
            assert(i >= 2);
            mstring_append_fmt(vars, "/* PS_TEXTUREMODES_DOT_ZW */\n");
            mstring_append_fmt(vars, "float dot%d = dot(pT%d.xyz, %s(t%d));\n",
                i, i, dotmap_func, ps->input_tex[i]);
            mstring_append_fmt(vars, "vec4 t%d = vec4(0.0);\n", i);
            // FIXME: mstring_append_fmt(vars, "gl_FragDepth = t%d.x;\n", i);
            break;
        case PS_TEXTUREMODES_DOT_RFLCT_DIFF:
            assert(i == 2);
            mstring_append_fmt(vars, "/* PS_TEXTUREMODES_DOT_RFLCT_DIFF */\n");
            mstring_append_fmt(vars, "float dot%d = dot(pT%d.xyz, %s(t%d));\n",
                i, i, dotmap_func, ps->input_tex[i]);
            assert(ps->dot_map[i+1] < 8);
            mstring_append_fmt(vars, "float dot%d_n = dot(pT%d.xyz, %s(t%d));\n",
                i, i+1, dotmap_funcs[ps->dot_map[i+1]], ps->input_tex[i+1]);
            mstring_append_fmt(vars, "vec3 n_%d = vec3(dot%d, dot%d, dot%d_n);\n",
                i, i-1, i, i);
            apply_border_adjustment(ps, vars, i, "n_%d");
            mstring_append_fmt(vars, "vec4 t%d = texture(texSamp%d, n_%d);\n",
                i, i, i);
            break;
        case PS_TEXTUREMODES_DOT_RFLCT_SPEC:
            assert(i == 3);
            mstring_append_fmt(vars, "/* PS_TEXTUREMODES_DOT_RFLCT_SPEC */\n");
            mstring_append_fmt(vars, "float dot%d = dot(pT%d.xyz, %s(t%d));\n",
                i, i, dotmap_func, ps->input_tex[i]);
            mstring_append_fmt(vars, "vec3 n_%d = vec3(dot%d, dot%d, dot%d);\n",
                i, i-2, i-1, i);
            mstring_append_fmt(vars, "vec3 e_%d = vec3(pT%d.w, pT%d.w, pT%d.w);\n",
                i, i-2, i-1, i);
            mstring_append_fmt(vars, "vec3 rv_%d = 2*n_%d*dot(n_%d,e_%d)/dot(n_%d,n_%d) - e_%d;\n",
                i, i, i, i, i, i, i);
            apply_border_adjustment(ps, vars, i, "rv_%d");
            mstring_append_fmt(vars, "vec4 t%d = texture(texSamp%d, rv_%d);\n",
                i, i, i);
            break;
        case PS_TEXTUREMODES_DOT_STR_3D:
            assert(i == 3);
            mstring_append_fmt(vars, "/* PS_TEXTUREMODES_DOT_STR_3D */\n");
            mstring_append_fmt(vars,
               "float dot%d = dot(pT%d.xyz, %s(t%d));\n"
               "vec3 dotSTR%d = vec3(dot%d, dot%d, dot%d);\n",
                i, i, dotmap_func, ps->input_tex[i],
                i, i-2, i-1, i);

            apply_border_adjustment(ps, vars, i, "dotSTR%d");
            mstring_append_fmt(vars, "vec4 t%d = texture(texSamp%d, dotSTR%d);\n",
                i, i, i);
            break;
        case PS_TEXTUREMODES_DOT_STR_CUBE:
            assert(i == 3);
            mstring_append_fmt(vars, "/* PS_TEXTUREMODES_DOT_STR_CUBE */\n");
            mstring_append_fmt(vars, "float dot%d = dot(pT%d.xyz, %s(t%d));\n",
                i, i, dotmap_func, ps->input_tex[i]);
            mstring_append_fmt(vars, "vec3 dotSTR%dCube = vec3(dot%d, dot%d, dot%d);\n",
                               i, i-2, i-1, i);
            apply_border_adjustment(ps, vars, i, "dotSTR%dCube");
            mstring_append_fmt(vars, "vec4 t%d = texture(texSamp%d, dotSTR%dCube);\n",
                i, i, i);
            break;
        case PS_TEXTUREMODES_DPNDNT_AR:
            assert(i >= 1);
            assert(!ps->state.rect_tex[i]);
            mstring_append_fmt(vars, "vec2 t%dAR = t%d.ar;\n", i, ps->input_tex[i]);
            apply_border_adjustment(ps, vars, i, "t%dAR");
            mstring_append_fmt(vars, "vec4 t%d = texture(texSamp%d, t%dAR);\n",
                i, i, i);
            break;
        case PS_TEXTUREMODES_DPNDNT_GB:
            assert(i >= 1);
            assert(!ps->state.rect_tex[i]);
            mstring_append_fmt(vars, "vec2 t%dGB = t%d.gb;\n", i, ps->input_tex[i]);
            apply_border_adjustment(ps, vars, i, "t%dGB");
            mstring_append_fmt(vars, "vec4 t%d = texture(texSamp%d, t%dGB);\n",
                i, i, i);
            break;
        case PS_TEXTUREMODES_DOTPRODUCT:
            assert(i == 1 || i == 2);
            mstring_append_fmt(vars, "/* PS_TEXTUREMODES_DOTPRODUCT */\n");
            mstring_append_fmt(vars, "float dot%d = dot(pT%d.xyz, %s(t%d));\n",
                i, i, dotmap_func, ps->input_tex[i]);
            mstring_append_fmt(vars, "vec4 t%d = vec4(0.0);\n", i);
            break;
        case PS_TEXTUREMODES_DOT_RFLCT_SPEC_CONST:
            assert(i == 3);
            mstring_append_fmt(vars, "vec4 t%d = vec4(0.0); /* PS_TEXTUREMODES_DOT_RFLCT_SPEC_CONST */\n",
                               i);
            NV2A_UNIMPLEMENTED("PS_TEXTUREMODES_DOT_RFLCT_SPEC_CONST");
            break;
        default:
            fprintf(stderr, "Unknown ps tex mode: 0x%x\n", ps->tex_modes[i]);
            assert(false);
            break;
        }

        mstring_append_fmt(preflight, "uniform float texScale%d;\n", i);
        if (sampler_type != NULL) {
            mstring_append_fmt(preflight, "uniform %s texSamp%d;\n", sampler_type, i);

            /* As this means a texture fetch does happen, do alphakill */
            if (ps->state.alphakill[i]) {
                mstring_append_fmt(vars, "if (t%d.a == 0.0) { discard; };\n",
                                   i);
            }
        }
    }

    for (i = 0; i < ps->num_stages; i++) {
        ps->cur_stage = i;
        mstring_append_fmt(ps->code, "// Stage %d\n", i);
        MString* color = add_stage_code(ps, ps->stage[i].rgb_input, ps->stage[i].rgb_output, "rgb", false);
        MString* alpha = add_stage_code(ps, ps->stage[i].alpha_input, ps->stage[i].alpha_output, "a", true);

        mstring_append(ps->code, mstring_get_str(color));
        mstring_append(ps->code, mstring_get_str(alpha));
        mstring_unref(color);
        mstring_unref(alpha);
    }

    if (ps->final_input.enabled) {
        ps->cur_stage = 8;
        mstring_append(ps->code, "// Final Combiner\n");
        add_final_stage_code(ps, ps->final_input);
    }

    if (ps->state.alpha_test && ps->state.alpha_func != ALPHA_FUNC_ALWAYS) {
        mstring_append_fmt(preflight, "uniform float alphaRef;\n");
        if (ps->state.alpha_func == ALPHA_FUNC_NEVER) {
            mstring_append(ps->code, "discard;\n");
        } else {
            const char* alpha_op;
            switch (ps->state.alpha_func) {
            case ALPHA_FUNC_LESS: alpha_op = "<"; break;
            case ALPHA_FUNC_EQUAL: alpha_op = "=="; break;
            case ALPHA_FUNC_LEQUAL: alpha_op = "<="; break;
            case ALPHA_FUNC_GREATER: alpha_op = ">"; break;
            case ALPHA_FUNC_NOTEQUAL: alpha_op = "!="; break;
            case ALPHA_FUNC_GEQUAL: alpha_op = ">="; break;
            default:
                assert(false);
                break;
            }
            mstring_append_fmt(ps->code, "if (!(fragColor.a %s alphaRef)) discard;\n",
                               alpha_op);
        }
    }

    for (i = 0; i < ps->num_const_refs; i++) {
        mstring_append_fmt(preflight, "uniform vec4 %s;\n", ps->const_refs[i]);
    }

    for (i = 0; i < ps->num_var_refs; i++) {
        mstring_append_fmt(vars, "vec4 %s;\n", ps->var_refs[i]);
        if (strcmp(ps->var_refs[i], "r0") == 0) {
            if (ps->tex_modes[0] != PS_TEXTUREMODES_NONE) {
                mstring_append(vars, "r0.a = t0.a;\n");
            } else {
                mstring_append(vars, "r0.a = 1.0;\n");
            }
        }
    }

    MString *final = mstring_new();
    mstring_append(final, "#version 330\n\n");
    mstring_append(final, mstring_get_str(preflight));
    mstring_append(final, "void main() {\n");
    mstring_append(final, mstring_get_str(clip));
    mstring_append(final, mstring_get_str(vars));
    mstring_append(final, mstring_get_str(ps->code));
    mstring_append(final, "}\n");

    mstring_unref(preflight);
    mstring_unref(vars);
    mstring_unref(ps->code);

    return final;
}

static void parse_input(struct InputInfo *var, int value)
{
    var->reg = value & 0xF;
    var->chan = value & 0x10;
    var->mod = value & 0xE0;
}

static void parse_combiner_inputs(uint32_t value,
                                struct InputInfo *a, struct InputInfo *b,
                                struct InputInfo *c, struct InputInfo *d)
{
    parse_input(d, value & 0xFF);
    parse_input(c, (value >> 8) & 0xFF);
    parse_input(b, (value >> 16) & 0xFF);
    parse_input(a, (value >> 24) & 0xFF);
}

static void parse_combiner_output(uint32_t value, struct OutputInfo *out)
{
    out->cd = value & 0xF;
    out->ab = (value >> 4) & 0xF;
    out->muxsum = (value >> 8) & 0xF;
    int flags = value >> 12;
    out->flags = flags;
    out->cd_op = flags & 1;
    out->ab_op = flags & 2;
    out->muxsum_op = flags & 4;
    out->mapping = flags & 0x38;
    out->ab_alphablue = flags & 0x80;
    out->cd_alphablue = flags & 0x40;
}

MString *psh_translate(const PshState state)
{
    int i;
    struct PixelShader ps;
    memset(&ps, 0, sizeof(ps));

    ps.state = state;

    ps.num_stages = state.combiner_control & 0xFF;
    ps.flags = state.combiner_control >> 8;
    for (i = 0; i < 4; i++) {
        ps.tex_modes[i] = (state.shader_stage_program >> (i * 5)) & 0x1F;
    }

    ps.dot_map[0] = 0;
    ps.dot_map[1] = (state.other_stage_input >> 0) & 0xf;
    ps.dot_map[2] = (state.other_stage_input >> 4) & 0xf;
    ps.dot_map[3] = (state.other_stage_input >> 8) & 0xf;

    ps.input_tex[0] = -1;
    ps.input_tex[1] = 0;
    ps.input_tex[2] = (state.other_stage_input >> 16) & 0xF;
    ps.input_tex[3] = (state.other_stage_input >> 20) & 0xF;
    for (i = 0; i < ps.num_stages; i++) {
        parse_combiner_inputs(state.rgb_inputs[i],
            &ps.stage[i].rgb_input.a, &ps.stage[i].rgb_input.b,
            &ps.stage[i].rgb_input.c, &ps.stage[i].rgb_input.d);
        parse_combiner_inputs(state.alpha_inputs[i],
            &ps.stage[i].alpha_input.a, &ps.stage[i].alpha_input.b,
            &ps.stage[i].alpha_input.c, &ps.stage[i].alpha_input.d);

        parse_combiner_output(state.rgb_outputs[i], &ps.stage[i].rgb_output);
        parse_combiner_output(state.alpha_outputs[i], &ps.stage[i].alpha_output);
    }

    struct InputInfo blank;
    ps.final_input.enabled = state.final_inputs_0 || state.final_inputs_1;
    if (ps.final_input.enabled) {
        parse_combiner_inputs(state.final_inputs_0,
                              &ps.final_input.a, &ps.final_input.b,
                              &ps.final_input.c, &ps.final_input.d);
        parse_combiner_inputs(state.final_inputs_1,
                              &ps.final_input.e, &ps.final_input.f,
                              &ps.final_input.g, &blank);
        int flags = state.final_inputs_1 & 0xFF;
        ps.final_input.clamp_sum = flags & PS_FINALCOMBINERSETTING_CLAMP_SUM;
        ps.final_input.inv_v1 = flags & PS_FINALCOMBINERSETTING_COMPLEMENT_V1;
        ps.final_input.inv_r0 = flags & PS_FINALCOMBINERSETTING_COMPLEMENT_R0;
    }

    return psh_convert(&ps);
}
