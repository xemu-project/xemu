/*
 * QEMU Geforce NV2A pixel shader translation
 *
 * Copyright (c) 2013 espes
 * Copyright (c) 2015 Jannik Vogel
 * Copyright (c) 2020-2025 Matt Borgerson
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
#include "hw/xbox/nv2a/debug.h"
#include "hw/xbox/nv2a/pgraph/pgraph.h"
#include "psh.h"

DEF_UNIFORM_INFO_ARR(PshUniform, PSH_UNIFORM_DECL_X)

// TODO: https://github.com/xemu-project/xemu/issues/2260
//   Investigate how color keying is handled for components with no alpha or
//   only alpha.
static uint32_t get_colorkey_mask(unsigned int color_format)
{
    switch (color_format) {
    case NV097_SET_TEXTURE_FORMAT_COLOR_SZ_X1R5G5B5:
    case NV097_SET_TEXTURE_FORMAT_COLOR_SZ_X8R8G8B8:
    case NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_X1R5G5B5:
    case NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_X8R8G8B8:
        return 0x00FFFFFF;

    default:
        return 0xFFFFFFFF;
    }
}

static uint32_t get_color_key_mask_for_texture(PGRAPHState *pg, int i)
{
    assert(i < NV2A_MAX_TEXTURES);
    uint32_t fmt = pgraph_reg_r(pg, NV_PGRAPH_TEXFMT0 + i * 4);
    unsigned int color_format = GET_MASK(fmt, NV_PGRAPH_TEXFMT0_COLOR);
    return get_colorkey_mask(color_format);
}

void pgraph_glsl_set_psh_state(PGRAPHState *pg, PshState *state)
{
    state->window_clip_exclusive = pgraph_reg_r(pg, NV_PGRAPH_SETUPRASTER) &
                                   NV_PGRAPH_SETUPRASTER_WINDOWCLIPTYPE;
    state->combiner_control = pgraph_reg_r(pg, NV_PGRAPH_COMBINECTL);
    state->shader_stage_program = pgraph_reg_r(pg, NV_PGRAPH_SHADERPROG);
    state->other_stage_input = pgraph_reg_r(pg, NV_PGRAPH_SHADERCTL);
    state->final_inputs_0 = pgraph_reg_r(pg, NV_PGRAPH_COMBINESPECFOG0);
    state->final_inputs_1 = pgraph_reg_r(pg, NV_PGRAPH_COMBINESPECFOG1);

    state->alpha_test = pgraph_reg_r(pg, NV_PGRAPH_CONTROL_0) &
                        NV_PGRAPH_CONTROL_0_ALPHATESTENABLE;
    state->alpha_func = (enum PshAlphaFunc)GET_MASK(
        pgraph_reg_r(pg, NV_PGRAPH_CONTROL_0), NV_PGRAPH_CONTROL_0_ALPHAFUNC);

    state->point_sprite = pgraph_reg_r(pg, NV_PGRAPH_SETUPRASTER) &
                          NV_PGRAPH_SETUPRASTER_POINTSMOOTHENABLE;

    state->shadow_depth_func =
        (enum PshShadowDepthFunc)GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_SHADOWCTL),
                                          NV_PGRAPH_SHADOWCTL_SHADOW_ZFUNC);
    state->z_perspective = pgraph_reg_r(pg, NV_PGRAPH_CONTROL_0) &
                           NV_PGRAPH_CONTROL_0_Z_PERSPECTIVE_ENABLE;

    state->smooth_shading = GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_CONTROL_3),
                                     NV_PGRAPH_CONTROL_3_SHADEMODE) ==
                            NV_PGRAPH_CONTROL_3_SHADEMODE_SMOOTH;

    state->depth_clipping =
        GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_ZCOMPRESSOCCLUDE),
                 NV_PGRAPH_ZCOMPRESSOCCLUDE_ZCLAMP_EN) ==
        NV_PGRAPH_ZCOMPRESSOCCLUDE_ZCLAMP_EN_CULL;

    int num_stages = pgraph_reg_r(pg, NV_PGRAPH_COMBINECTL) & 0xFF;
    for (int i = 0; i < num_stages; i++) {
        state->rgb_inputs[i] =
            pgraph_reg_r(pg, NV_PGRAPH_COMBINECOLORI0 + i * 4);
        state->rgb_outputs[i] =
            pgraph_reg_r(pg, NV_PGRAPH_COMBINECOLORO0 + i * 4);
        state->alpha_inputs[i] =
            pgraph_reg_r(pg, NV_PGRAPH_COMBINEALPHAI0 + i * 4);
        state->alpha_outputs[i] =
            pgraph_reg_r(pg, NV_PGRAPH_COMBINEALPHAO0 + i * 4);
    }

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            state->compare_mode[i][j] =
                (pgraph_reg_r(pg, NV_PGRAPH_SHADERCLIPMODE) >> (4 * i + j)) & 1;
        }

        uint32_t ctl_0 = pgraph_reg_r(pg, NV_PGRAPH_TEXCTL0_0 + i * 4);
        bool enabled = pgraph_is_texture_stage_active(pg, i) &&
                       (ctl_0 & NV_PGRAPH_TEXCTL0_0_ENABLE);
        if (!enabled) {
            continue;
        }

        state->alphakill[i] = ctl_0 & NV_PGRAPH_TEXCTL0_0_ALPHAKILLEN;
        state->colorkey_mode[i] = ctl_0 & NV_PGRAPH_TEXCTL0_0_COLORKEYMODE;

        uint32_t tex_fmt = pgraph_reg_r(pg, NV_PGRAPH_TEXFMT0 + i * 4);
        state->dim_tex[i] = GET_MASK(tex_fmt, NV_PGRAPH_TEXFMT0_DIMENSIONALITY);

        unsigned int color_format = GET_MASK(tex_fmt, NV_PGRAPH_TEXFMT0_COLOR);
        BasicColorFormatInfo f = kelvin_color_format_info_map[color_format];
        state->rect_tex[i] = f.linear;
        state->tex_x8y24[i] =
            color_format ==
                NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_DEPTH_X8_Y24_FIXED ||
            color_format ==
                NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_DEPTH_X8_Y24_FLOAT;

        uint32_t border_source =
            GET_MASK(tex_fmt, NV_PGRAPH_TEXFMT0_BORDER_SOURCE);
        bool cubemap = GET_MASK(tex_fmt, NV_PGRAPH_TEXFMT0_CUBEMAPENABLE);
        state->tex_cubemap[i] = cubemap;
        state->border_logical_size[i][0] = 0.0f;
        state->border_logical_size[i][1] = 0.0f;
        state->border_logical_size[i][2] = 0.0f;
        if (border_source != NV_PGRAPH_TEXFMT0_BORDER_SOURCE_COLOR) {
            if (!f.linear && !cubemap) {
                // The actual texture will be (at least) double the reported
                // size and shifted by a 4 texel border but texture coordinates
                // will still be relative to the reported size.
                unsigned int reported_width =
                    1 << GET_MASK(tex_fmt, NV_PGRAPH_TEXFMT0_BASE_SIZE_U);
                unsigned int reported_height =
                    1 << GET_MASK(tex_fmt, NV_PGRAPH_TEXFMT0_BASE_SIZE_V);
                unsigned int reported_depth =
                    1 << GET_MASK(tex_fmt, NV_PGRAPH_TEXFMT0_BASE_SIZE_P);

                state->border_logical_size[i][0] = reported_width;
                state->border_logical_size[i][1] = reported_height;
                state->border_logical_size[i][2] = reported_depth;

                if (reported_width < 8) {
                    state->border_inv_real_size[i][0] = 0.0625f;
                } else {
                    state->border_inv_real_size[i][0] =
                        1.0f / (reported_width * 2.0f);
                }
                if (reported_height < 8) {
                    state->border_inv_real_size[i][1] = 0.0625f;
                } else {
                    state->border_inv_real_size[i][1] =
                        1.0f / (reported_height * 2.0f);
                }
                if (reported_depth < 8) {
                    state->border_inv_real_size[i][2] = 0.0625f;
                } else {
                    state->border_inv_real_size[i][2] =
                        1.0f / (reported_depth * 2.0f);
                }
            } else {
                NV2A_UNIMPLEMENTED(
                    "Border source texture with linear %d cubemap %d", f.linear,
                    cubemap);
            }
        }

        /* Keep track of whether texture data has been loaded as signed
         * normalized integers or not. This dictates whether or not we will need
         * to re-map in fragment shader for certain texture modes (e.g.
         * bumpenvmap).
         *
         * FIXME: When signed texture data is loaded as unsigned and remapped in
         * fragment shader, there may be interpolation artifacts. Fix this to
         * support signed textures more appropriately.
         */
#if 0 // FIXME
        psh->snorm_tex[i] = (f.gl_internal_format == GL_RGB8_SNORM)
                                 || (f.gl_internal_format == GL_RG8_SNORM);
#endif
        state->shadow_map[i] = f.depth;

        uint32_t filter = pgraph_reg_r(pg, NV_PGRAPH_TEXFILTER0 + i * 4);
        unsigned int min_filter = GET_MASK(filter, NV_PGRAPH_TEXFILTER0_MIN);
        enum ConvolutionFilter kernel = CONVOLUTION_FILTER_DISABLED;
        /* FIXME: We do not distinguish between min and mag when
         * performing convolution. Just use it if specified for min (common AA
         * case).
         */
        if (min_filter == NV_PGRAPH_TEXFILTER0_MIN_CONVOLUTION_2D_LOD0) {
            int k = GET_MASK(filter, NV_PGRAPH_TEXFILTER0_CONVOLUTION_KERNEL);
            assert(k == NV_PGRAPH_TEXFILTER0_CONVOLUTION_KERNEL_QUINCUNX ||
                   k == NV_PGRAPH_TEXFILTER0_CONVOLUTION_KERNEL_GAUSSIAN_3);
            kernel = (enum ConvolutionFilter)k;
        }

        state->conv_tex[i] = kernel;
    }

    state->surface_zeta_format = pg->surface_shape.zeta_format;
    unsigned int z_format = GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_SETUPRASTER),
                                     NV_PGRAPH_SETUPRASTER_Z_FORMAT);

    switch (pg->surface_shape.zeta_format) {
    case NV097_SET_SURFACE_FORMAT_ZETA_Z16:
        state->depth_format =
            z_format ? DEPTH_FORMAT_F16 : DEPTH_FORMAT_D16;
        break;
    case NV097_SET_SURFACE_FORMAT_ZETA_Z24S8:
        state->depth_format =
            z_format ? DEPTH_FORMAT_F24 : DEPTH_FORMAT_D24;
        break;
    default:
        fprintf(stderr, "Unknown zeta surface format: 0x%x\n",
                pg->surface_shape.zeta_format);
        assert(false);
        break;
    }
}

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
    GenPshGlslOptions opts;
    const PshState *state;

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
            MString *reg_name = mstring_from_fmt("c0_%d", ps->cur_stage);
            add_const_ref(ps, mstring_get_str(reg_name));
            return reg_name;
        } else {  // Same c0
            add_const_ref(ps, "c0_0");
            return mstring_from_str("c0_0");
        }
        break;
    case PS_REGISTER_C1:
        if (ps->flags & PS_COMBINERCOUNT_UNIQUE_C1 || ps->cur_stage == 8) {
            MString *reg_name = mstring_from_fmt("c1_%d", ps->cur_stage);
            add_const_ref(ps, mstring_get_str(reg_name));
            return reg_name;
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

static const char *get_sampler_type(struct PixelShader *ps, enum PS_TEXTUREMODES mode, int i)
{
    const char *sampler2D = "sampler2D";
    const char *sampler3D = "sampler3D";
    const char *samplerCube = "samplerCube";
    const struct PshState *state = ps->state;
    int dim = state->dim_tex[i];

    // FIXME: Cleanup
    switch (mode) {
    default:
    case PS_TEXTUREMODES_NONE:
        return NULL;

    case PS_TEXTUREMODES_PROJECT2D:
        if (dim == 2) {
            if (state->tex_x8y24[i] && ps->opts.vulkan) {
                return "usampler2D";
            }
            if (state->tex_cubemap[i]) {
                return samplerCube;
            }
            return sampler2D;
        }
        if (dim == 3) return sampler3D;
        assert(!"Unhandled texture dimensions");
        return NULL;

    case PS_TEXTUREMODES_BUMPENVMAP:
    case PS_TEXTUREMODES_BUMPENVMAP_LUM:
    case PS_TEXTUREMODES_DOT_ST:
        if (state->shadow_map[i]) {
            fprintf(stderr, "Shadow map support not implemented for mode %d\n", mode);
            assert(!"Shadow map support not implemented for this mode");
        }
        if (dim == 2) return sampler2D;
        if (dim == 3 && mode != PS_TEXTUREMODES_DOT_ST) return sampler3D;
        assert(!"Unhandled texture dimensions");
        return NULL;

    case PS_TEXTUREMODES_PROJECT3D:
    case PS_TEXTUREMODES_DOT_STR_3D:
        if (state->tex_x8y24[i] && ps->opts.vulkan) {
            return "usampler2D";
        }
        if (state->shadow_map[i]) {
            return sampler2D;
        }
        return dim == 2 ? sampler2D : sampler3D;

    case PS_TEXTUREMODES_CUBEMAP:
    case PS_TEXTUREMODES_DOT_RFLCT_DIFF:
    case PS_TEXTUREMODES_DOT_RFLCT_SPEC:
    case PS_TEXTUREMODES_DOT_STR_CUBE:
        if (state->shadow_map[i]) {
            fprintf(stderr, "Shadow map support not implemented for mode %d\n", mode);
            assert(!"Shadow map support not implemented for this mode");
        }
        assert(dim == 2);
        if (state->tex_cubemap[i]) {
            return samplerCube;
        }
        return sampler2D;

    case PS_TEXTUREMODES_DPNDNT_AR:
    case PS_TEXTUREMODES_DPNDNT_GB:
        if (state->shadow_map[i]) {
            fprintf(stderr, "Shadow map support not implemented for mode %d\n", mode);
            assert(!"Shadow map support not implemented for this mode");
        }
        assert(dim == 2);
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
    if (ps->state->shadow_depth_func == SHADOW_DEPTH_FUNC_NEVER) {
        mstring_append_fmt(vars, "vec4 t%d = vec4(0.0);\n", i);
        return;
    }

    if (ps->state->shadow_depth_func == SHADOW_DEPTH_FUNC_ALWAYS) {
        mstring_append_fmt(vars, "vec4 t%d = vec4(1.0);\n", i);
        return;
    }

    g_autofree gchar *normalize_tex_coords = g_strdup_printf("norm%d", i);
    const char *tex_remap = ps->state->rect_tex[i] ? normalize_tex_coords : "";

    const char *comparison = shadow_comparison_map[ps->state->shadow_depth_func];

    bool extract_msb_24b = ps->state->tex_x8y24[i] && ps->opts.vulkan;

    mstring_append_fmt(
        vars, "%svec4 t%d_depth%s = textureProj(texSamp%d, %s(pT%d.xyw));\n",
        extract_msb_24b ? "u" : "", i, extract_msb_24b ? "_raw" : "", i,
        tex_remap, i);

    if (extract_msb_24b) {
        mstring_append_fmt(vars,
                           "vec4 t%d_depth = vec4(float(t%d_depth_raw.x >> 8) "
                           "/ 0xFFFFFF, 1.0, 0.0, 0.0);\n",
                           i, i);
    }

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
    if (ps->state->border_logical_size[i][0] == 0.0f) {
        return;
    }

    char var_name[32] = {0};
    snprintf(var_name, sizeof(var_name), var_template, i);

    mstring_append_fmt(
        vars,
        "vec3 t%dLogicalSize = vec3(%f, %f, %f);\n"
        "%s.xyz = (%s.xyz * t%dLogicalSize + vec3(4, 4, 4)) * vec3(%f, %f, %f);\n",
        i, ps->state->border_logical_size[i][0], ps->state->border_logical_size[i][1], ps->state->border_logical_size[i][2],
        var_name, var_name, i, ps->state->border_inv_real_size[i][0], ps->state->border_inv_real_size[i][1], ps->state->border_inv_real_size[i][2]);
}

static void apply_convolution_filter(const struct PixelShader *ps, MString *vars, int tex)
{
    assert(ps->state->dim_tex[tex] == 2);
    // FIXME: Quincunx

    g_autofree gchar *normalize_tex_coords = g_strdup_printf("norm%d", tex);
    const char *tex_remap = ps->state->rect_tex[tex] ? normalize_tex_coords : "";

    mstring_append_fmt(vars,
        "vec4 t%d = vec4(0.0);\n"
        "for (int i = 0; i < 9; i++) {\n"
        "    vec3 texCoordDelta = vec3(convolution3x3[i], 0);\n"
        "    texCoordDelta.xy /= textureSize(texSamp%d, 0);\n"
        "    t%d += textureProj(texSamp%d, %s(pT%d.xyw) + texCoordDelta) * gaussian3x3[i];\n"
        "}\n", tex, tex, tex, tex, tex_remap, tex);
}

static void define_colorkey_comparator(MString *preflight)
{
    // clang-format off
    mstring_append(
        preflight,
        "bool check_color_key(vec4 texel, uint color_key, uint color_key_mask) {\n"
        "    uvec4 c = uvec4(texel * 255.0 + 0.5);\n"
        "    uint color = (c.a << 24) | (c.r << 16) | (c.g << 8) | c.b;\n"
        "    return (color & color_key_mask) == (color_key & color_key_mask);\n"
        "}\n");
    // clang-format on
}

static MString* psh_convert(struct PixelShader *ps)
{
    MString *preflight = mstring_new();
    pgraph_glsl_get_vtx_header(preflight, ps->opts.vulkan,
                             ps->state->smooth_shading, true, false, false);

    if (ps->opts.vulkan) {
        mstring_append_fmt(
            preflight,
            "layout(location = 0) out vec4 fragColor;\n"
            "layout(binding = %d, std140) uniform PshUniforms {\n",
            ps->opts.ubo_binding);
    } else {
        mstring_append_fmt(preflight,
                           "layout(location = 0) out vec4 fragColor;\n");
    }

    const char *u = ps->opts.vulkan ? "" : "uniform ";
    for (int i = 0; i < ARRAY_SIZE(PshUniformInfo); i++) {
        const UniformInfo *info = &PshUniformInfo[i];
        const char *type_str = uniform_element_type_to_str[info->type];
        if (info->count == 1) {
            mstring_append_fmt(preflight, "%s%s %s;\n", u, type_str,
                               info->name);
        } else {
            mstring_append_fmt(preflight, "%s%s %s[%zd];\n", u, type_str,
                               info->name, info->count);
        }
    }

    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 2; j++) {
            mstring_append_fmt(preflight, "#define c%d_%d consts[%d]\n", j, i, i*2+j);
        }
    }

    if (ps->opts.vulkan) {
        mstring_append(preflight, "};\n");
    }

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
        // Kahan's algorithm for computing determinant using FMA for higher
        // precision. See e.g.:
        // Muller et al, "Handbook of Floating-Point Arithmetic", 2nd ed.
        // or
        // Claude-Pierre Jeannerod, Nicolas Louvet, and Jean-Michel Muller,
        // Further analysis of Kahan's algorithm for the accurate
        // computation of 2x2 determinants,
        // Mathematics of Computation 82(284), October 2013.
        "float kahan_det(vec2 a, vec2 b) {\n"
        "    precise float cd = a.y*b.x;\n"
        "    precise float err = fma(-a.y, b.x, cd);\n"
        "    precise float res = fma(a.x, b.y, -cd) + err;\n"
        "    return res;\n"
        "}\n"
        "float area(vec2 a, vec2 b, vec2 c) {\n"
        "    return kahan_det(b - a, c - a);\n"
        "}\n"
        "const float[9] gaussian3x3 = float[9](\n"
        "    1.0/16.0, 2.0/16.0, 1.0/16.0,\n"
        "    2.0/16.0, 4.0/16.0, 2.0/16.0,\n"
        "    1.0/16.0, 2.0/16.0, 1.0/16.0);\n"
        "const vec2[9] convolution3x3 = vec2[9](\n"
        "    vec2(-1.0,-1.0),vec2(0.0,-1.0),vec2(1.0,-1.0),\n"
        "    vec2(-1.0, 0.0),vec2(0.0, 0.0),vec2(1.0, 0.0),\n"
        "    vec2(-1.0, 1.0),vec2(0.0, 1.0),vec2(1.0, 1.0));\n"
        "vec2 remapCubeTo2D(vec3 texCoord) {\n"
        "    vec2 uv;\n"
        "    vec3 absTexCoord = abs(texCoord);\n"
        "    if (absTexCoord.x > absTexCoord.y && absTexCoord.x > absTexCoord.z) {\n"
        "        if (texCoord.x > 0.0) {\n"
        "            // +X: Right\n"
        "            uv = vec2(-texCoord.z, texCoord.y);\n"
        "        } else {\n"
        "            // -X: Left\n"
        "            uv = vec2(texCoord.z, texCoord.y);\n"
        "        }\n"
        "        uv /= absTexCoord.x;\n"
        "    }\n"
        "    else if (absTexCoord.y > absTexCoord.x && absTexCoord.y > absTexCoord.z) {\n"
        "        if (texCoord.y > 0.0) {\n"
        "            // +Y: Top\n"
        "            uv = vec2(texCoord.x, -texCoord.z);\n"
        "        } else {\n"
        "            // -Y: Bottom\n"
        "            uv = vec2(texCoord.x, texCoord.z);\n"
        "        }\n"
        "        uv /= absTexCoord.y;\n"
        "    }\n"
        "    else {\n"
        "        if (texCoord.z > 0.0) {\n"
        "            // +Z: Front\n"
        "            uv = vec2(texCoord.x, texCoord.y);\n"
        "        } else {\n"
        "            // -Z: Back\n"
        "            uv = vec2(-texCoord.x, texCoord.y);\n"
        "        }\n"
        "        uv /= absTexCoord.z;\n"
        "    }\n"
        "    return uv;\n"
        "}\n"
        "\n"
        "vec3 remap2DToCube(vec3 texCoord2DProjective) {\n"
        "    vec2 st = (texCoord2DProjective.xy / texCoord2DProjective.z);"
        "    return normalize(vec3(1.0, st.y, -st.x));"
        "}\n"
        );

    MString *clip = mstring_new();
    mstring_append_fmt(clip, "/*  Window-clip (%slusive) */\n",
                       ps->state->window_clip_exclusive ? "Exc" : "Inc");
    if (!ps->state->window_clip_exclusive) {
        mstring_append(clip, "bool clipContained = false;\n");
    }
    mstring_append(clip, "vec2 coord = gl_FragCoord.xy - 0.5;\n"
                         "for (int i = 0; i < 8; i++) {\n"
                         "  bool outside = any(bvec4(\n"
                         "      lessThan(coord, vec2(clipRegion[i].xy)),\n"
                         "      greaterThanEqual(coord, vec2(clipRegion[i].zw))));\n"
                         "  if (!outside) {\n");
    if (ps->state->window_clip_exclusive) {
        mstring_append(clip, "    discard;\n");
    } else {
        mstring_append(clip, "    clipContained = true;\n"
                             "    break;\n");
    }
    mstring_append(clip, "  }\n"
                         "}\n");
    if (!ps->state->window_clip_exclusive) {
        mstring_append(clip, "if (!clipContained) {\n"
                             "  discard;\n"
                             "}\n");
    }

    if (ps->state->z_perspective) {
        mstring_append(
            clip,
            "vec2 unscaled_xy = gl_FragCoord.xy / surfaceScale;\n"
            "precise float bc0 = area(unscaled_xy, vtxPos1.xy, vtxPos2.xy);\n"
            "precise float bc1 = area(unscaled_xy, vtxPos2.xy, vtxPos0.xy);\n"
            "precise float bc2 = area(unscaled_xy, vtxPos0.xy, vtxPos1.xy);\n"
            "bc0 /= vtxPos0.w;\n"
            "bc1 /= vtxPos1.w;\n"
            "bc2 /= vtxPos2.w;\n"
            "float inv_bcsum = 1.0 / (bc0 + bc1 + bc2);\n"
            // Denominator can be zero in case the rasterized primitive is a
            // point or a degenerate line or triangle.
            "if (isinf(inv_bcsum)) {\n"
            "  inv_bcsum = 0.0;\n"
            "}\n"
            "bc1 *= inv_bcsum;\n"
            "bc2 *= inv_bcsum;\n"
            "precise float zvalue = vtxPos0.w + (bc1*(vtxPos1.w - vtxPos0.w) + bc2*(vtxPos2.w - vtxPos0.w));\n"
            // If GPU clipping is inaccurate, the point gl_FragCoord.xy might
            // be above the horizon of the plane of a rasterized triangle
            // making the interpolated w-coordinate above zero or negative. We
            // should prevent such wrapping through infinity by clamping to
            // infinity.
            "if (zvalue > 0.0) {\n"
            "  float zslopeofs = depthFactor*triMZ*zvalue*zvalue;\n"
            "  zvalue += depthOffset;\n"
            "  zvalue += zslopeofs;\n"
            "} else {\n"
            "  zvalue = uintBitsToFloat(0x7F7FFFFFu);\n"
            "}\n"
            "if (isnan(zvalue)) {\n"
            "  zvalue = uintBitsToFloat(0x7F7FFFFFu);\n"
            "}\n");
    } else {
        mstring_append(
            clip,
            "vec2 unscaled_xy = gl_FragCoord.xy / surfaceScale;\n"
            "precise float bc0 = area(unscaled_xy, vtxPos1.xy, vtxPos2.xy);\n"
            "precise float bc1 = area(unscaled_xy, vtxPos2.xy, vtxPos0.xy);\n"
            "precise float bc2 = area(unscaled_xy, vtxPos0.xy, vtxPos1.xy);\n"
            "float inv_bcsum = 1.0 / (bc0 + bc1 + bc2);\n"
            // Denominator can be zero in case the rasterized primitive is a
            // point or a degenerate line or triangle.
            "if (isinf(inv_bcsum)) {\n"
            "  inv_bcsum = 0.0;\n"
            "}\n"
            "bc1 *= inv_bcsum;\n"
            "bc2 *= inv_bcsum;\n"
            "precise float zvalue = vtxPos0.z + (bc1*(vtxPos1.z - vtxPos0.z) + bc2*(vtxPos2.z - vtxPos0.z));\n"
            "zvalue += depthOffset;\n"
            "zvalue += depthFactor*triMZ;\n");
    }

    /* Depth clipping */
    if (ps->state->depth_clipping) {
        mstring_append(
            clip, "if (zvalue < clipRange.z || clipRange.w < zvalue) {\n"
                  "  discard;\n"
                  "}\n");
    } else {
        mstring_append(
            clip, "zvalue = clamp(zvalue, clipRange.z, clipRange.w);\n");
    }

    MString *vars = mstring_new();
    mstring_append(vars, "vec4 pD0 = vtxD0;\n");
    mstring_append(vars, "vec4 pD1 = vtxD1;\n");
    mstring_append(vars, "vec4 pB0 = vtxB0;\n");
    mstring_append(vars, "vec4 pB1 = vtxB1;\n");
    mstring_append(vars, "vec4 pFog = vec4(fogColor.rgb, clamp(vtxFog, 0.0, 1.0));\n");
    mstring_append(vars, "vec4 pT0 = vtxT0;\n");
    mstring_append(vars, "vec4 pT1 = vtxT1;\n");
    mstring_append(vars, "vec4 pT2 = vtxT2;\n");
    if (ps->state->point_sprite) {
        assert(!ps->state->rect_tex[3]);
        mstring_append(vars, "vec4 pT3 = vec4(gl_PointCoord, 1.0, 1.0);\n");
    } else {
        mstring_append(vars, "vec4 pT3 = vtxT3;\n");
    }
    mstring_append(vars, "\n");
    mstring_append(vars, "vec4 v0 = pD0;\n");
    mstring_append(vars, "vec4 v1 = pD1;\n");
    mstring_append(vars, "vec4 ab;\n");
    mstring_append(vars, "vec4 cd;\n");
    mstring_append(vars, "vec4 mux_sum;\n");

    ps->code = mstring_new();

    bool color_key_comparator_defined = false;

    for (int i = 0; i < 4; i++) {

        const char *sampler_type = get_sampler_type(ps, ps->tex_modes[i], i);

        g_autofree gchar *normalize_tex_coords = g_strdup_printf("norm%d", i);
        const char *tex_remap = ps->state->rect_tex[i] ? normalize_tex_coords : "";

        assert(ps->dot_map[i] < 8);
        const char *dotmap_func = dotmap_funcs[ps->dot_map[i]];
        if (ps->dot_map[i] > 3) {
            NV2A_UNIMPLEMENTED("Dot Mapping mode %s", dotmap_func);
        }

        switch (ps->tex_modes[i]) {
        case PS_TEXTUREMODES_NONE:
            mstring_append_fmt(vars, "vec4 t%d = vec4(0.0, 0.0, 0.0, 1.0); /* PS_TEXTUREMODES_NONE */\n",
                               i);
            break;
        case PS_TEXTUREMODES_PROJECT2D: {
            if (ps->state->shadow_map[i]) {
                psh_append_shadowmap(ps, i, false, vars);
            } else {
                apply_border_adjustment(ps, vars, i, "pT%d");
                if (((ps->state->conv_tex[i] == CONVOLUTION_FILTER_GAUSSIAN) ||
                     (ps->state->conv_tex[i] == CONVOLUTION_FILTER_QUINCUNX))) {
                    apply_convolution_filter(ps, vars, i);
                } else {
                    if (ps->state->dim_tex[i] == 2) {
                        if (ps->state->tex_cubemap[i]) {
                            mstring_append_fmt(
                                vars,
                                "vec4 t%d = texture(texSamp%d, remap2DToCube(%s(pT%d.xyw)));\n",
                                i, i, tex_remap, i);
                        } else {
                            mstring_append_fmt(
                                vars,
                                "vec4 t%d = textureProj(texSamp%d, %s(pT%d.xyw));\n",
                                i, i, tex_remap, i);
                        }
                    } else if (ps->state->dim_tex[i] == 3) {
                        mstring_append_fmt(vars, "vec4 t%d = textureProj(texSamp%d, vec4(pT%d.xy, 0.0, pT%d.w));\n",
                                           i, i, i, i);
                    } else {
                        assert(!"Unhandled texture dimensions");
                    }
                }
            }
            break;
        }
        case PS_TEXTUREMODES_PROJECT3D:
            if (ps->state->shadow_map[i]) {
                psh_append_shadowmap(ps, i, true, vars);
            } else {
                apply_border_adjustment(ps, vars, i, "pT%d");
                mstring_append_fmt(vars, "vec4 t%d = textureProj(texSamp%d, %s(pT%d.xyzw));\n",
                                   i, i, tex_remap, i);
            }
            break;
        case PS_TEXTUREMODES_CUBEMAP:
            if (!ps->state->tex_cubemap[i]) {
                mstring_append_fmt(vars,
                    "pT%d.xy = remapCubeTo2D(pT%d.xyz);\n",
                    i, i);
            }
            mstring_append_fmt(vars,
                "vec4 t%d = texture(texSamp%d, pT%d.xy%s);\n",
                i, i, i, ps->state->tex_cubemap[i] ? "z" : "");
            break;
        case PS_TEXTUREMODES_PASSTHRU:
            assert(ps->state->border_logical_size[i][0] == 0.0f && "Unexpected border texture on passthru");
            mstring_append_fmt(vars, "vec4 t%d = pT%d;\n", i, i);
            break;
        case PS_TEXTUREMODES_CLIPPLANE: {
            int j;
            mstring_append_fmt(vars, "vec4 t%d = vec4(0.0); /* PS_TEXTUREMODES_CLIPPLANE */\n",
                               i);
            for (j = 0; j < 4; j++) {
                mstring_append_fmt(vars, "  if(pT%d.%c %s 0.0) { discard; };\n",
                                   i, "xyzw"[j],
                                   ps->state->compare_mode[i][j] ? ">=" : "<");
            }
            break;
        }
        case PS_TEXTUREMODES_BUMPENVMAP:
            assert(i >= 1);

            if (ps->state->snorm_tex[ps->input_tex[i]]) {
                /* Input color channels already signed (FIXME: May not always want signed textures in this case) */
                mstring_append_fmt(vars, "vec2 dsdt%d = t%d.bg;\n",
                                   i, ps->input_tex[i]);
            } else {
                /* Convert to signed (FIXME: loss of accuracy due to filtering/interpolation) */
                mstring_append_fmt(vars, "vec2 dsdt%d = vec2(sign3(t%d.b), sign3(t%d.g));\n",
                                   i, ps->input_tex[i], ps->input_tex[i]);
            }

            mstring_append_fmt(vars, "dsdt%d = bumpMat[%d] * dsdt%d;\n", i, i, i);

            if (ps->state->dim_tex[i] == 2) {
                mstring_append_fmt(vars, "vec4 t%d = texture(texSamp%d, %s(pT%d.xy + dsdt%d));\n",
                    i, i, tex_remap, i, i);
            } else if (ps->state->dim_tex[i] == 3) {
                // FIXME: Does hardware pass through the r/z coordinate or is it 0?
                mstring_append_fmt(vars, "vec4 t%d = texture(texSamp%d, vec3(pT%d.xy + dsdt%d, pT%d.z));\n",
                    i, i, i, i, i);
            } else {
                assert(!"Unhandled texture dimensions");
            }
            break;
        case PS_TEXTUREMODES_BUMPENVMAP_LUM:
            assert(i >= 1);

            if (ps->state->snorm_tex[ps->input_tex[i]]) {
                /* Input color channels already signed (FIXME: May not always want signed textures in this case) */
                mstring_append_fmt(vars, "vec3 dsdtl%d = vec3(t%d.bg, sign3_to_0_to_1(t%d.r));\n",
                                   i, ps->input_tex[i], ps->input_tex[i]);
            } else {
                /* Convert to signed (FIXME: loss of accuracy due to filtering/interpolation) */
                mstring_append_fmt(vars, "vec3 dsdtl%d = vec3(sign3(t%d.b), sign3(t%d.g), t%d.r);\n",
                                   i, ps->input_tex[i], ps->input_tex[i], ps->input_tex[i]);
            }

            mstring_append_fmt(vars, "dsdtl%d.st = bumpMat[%d] * dsdtl%d.st;\n",
                               i, i, i);

            if (ps->state->dim_tex[i] == 2) {
                mstring_append_fmt(vars, "vec4 t%d = texture(texSamp%d, %s(pT%d.xy + dsdtl%d.st));\n",
                    i, i, tex_remap, i, i);
            } else if (ps->state->dim_tex[i] == 3) {
                // FIXME: Does hardware pass through the r/z coordinate or is it 0?
                mstring_append_fmt(vars, "vec4 t%d = texture(texSamp%d, vec3(pT%d.xy + dsdtl%d.st, pT%d.z));\n",
                    i, i, i, i, i);
            } else {
                assert(!"Unhandled texture dimensions");
            }

            mstring_append_fmt(vars, "t%d = t%d * (bumpScale[%d] * dsdtl%d.p + bumpOffset[%d]);\n",
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
            mstring_append_fmt(vars, "vec4 t%d = texture(texSamp%d, %s(dotST%d));\n",
                i, i, tex_remap, i);
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
            if (!ps->state->tex_cubemap[i]) {
                mstring_append_fmt(vars,
                    "n_%d.xy = remapCubeTo2D(n_%d);\n", i, i);
            }
            mstring_append_fmt(vars,
                "vec4 t%d = texture(texSamp%d, n_%d%s);\n",
                i, i, i, ps->state->tex_cubemap[i] ? "" : ".xy");
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
            if (!ps->state->tex_cubemap[i]) {
                mstring_append_fmt(vars,
                    "rv_%d.xy = remapCubeTo2D(rv_%d);\n", i, i);
            }
            mstring_append_fmt(vars,
                "vec4 t%d = texture(texSamp%d, rv_%d%s);\n",
                i, i, i, ps->state->tex_cubemap[i] ? "" : ".xy");
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
            mstring_append_fmt(vars,
                "vec4 t%d = texture(texSamp%d, %s(dotSTR%d%s));\n",
                i, i, tex_remap, i, ps->state->dim_tex[i] == 2 ? ".xy" : "");
            break;
        case PS_TEXTUREMODES_DOT_STR_CUBE:
            assert(i == 3);
            mstring_append_fmt(vars, "/* PS_TEXTUREMODES_DOT_STR_CUBE */\n");
            mstring_append_fmt(vars, "float dot%d = dot(pT%d.xyz, %s(t%d));\n",
                i, i, dotmap_func, ps->input_tex[i]);
            mstring_append_fmt(vars, "vec3 dotSTR%dCube = vec3(dot%d, dot%d, dot%d);\n",
                               i, i-2, i-1, i);
            apply_border_adjustment(ps, vars, i, "dotSTR%dCube");
            if (!ps->state->tex_cubemap[i]) {
                mstring_append_fmt(vars,
                    "dotSTR%dCube.xy = remapCubeTo2D(dotSTR%dCube);\n",
                    i, i);
            }
            mstring_append_fmt(vars,
                "vec4 t%d = texture(texSamp%d, dotSTR%dCube%s);\n",
                i, i, i, ps->state->tex_cubemap[i] ? "" : ".xy");
            break;
        case PS_TEXTUREMODES_DPNDNT_AR:
            assert(i >= 1);
            assert(!ps->state->rect_tex[i]);
            mstring_append_fmt(vars, "vec2 t%dAR = t%d.ar;\n", i, ps->input_tex[i]);
            apply_border_adjustment(ps, vars, i, "t%dAR");
            mstring_append_fmt(vars, "vec4 t%d = texture(texSamp%d, %s(t%dAR));\n",
                i, i, tex_remap, i);
            break;
        case PS_TEXTUREMODES_DPNDNT_GB:
            assert(i >= 1);
            assert(!ps->state->rect_tex[i]);
            mstring_append_fmt(vars, "vec2 t%dGB = t%d.gb;\n", i, ps->input_tex[i]);
            apply_border_adjustment(ps, vars, i, "t%dGB");
            mstring_append_fmt(vars, "vec4 t%d = texture(texSamp%d, %s(t%dGB));\n",
                i, i, tex_remap, i);
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

        if (sampler_type != NULL) {
            if (ps->opts.vulkan) {
                mstring_append_fmt(preflight, "layout(binding = %d) ", ps->opts.tex_binding + i);
            }
            mstring_append_fmt(preflight, "uniform %s texSamp%d;\n", sampler_type, i);

            /* As this means a texture fetch does happen, do alphakill */
            if (ps->state->alphakill[i]) {
                mstring_append_fmt(vars, "if (t%d.a == 0.0) { discard; };\n",
                                   i);
            }

            enum PS_COLORKEYMODE color_key_mode = ps->state->colorkey_mode[i];
            if (color_key_mode != COLOR_KEY_NONE) {
                if (!color_key_comparator_defined) {
                    define_colorkey_comparator(preflight);
                    color_key_comparator_defined = true;
                }

                // clang-format off
                mstring_append_fmt(
                    vars,
                    "if (check_color_key(t%d, colorKey[%d], colorKeyMask[%d])) {\n",
                    i, i, i);
                // clang-format on

                switch (color_key_mode) {
                case COLOR_KEY_DISCARD:
                    mstring_append(vars, "  discard;\n");
                    break;

                case COLOR_KEY_KILL_ALPHA:
                    mstring_append_fmt(vars, "  t%d.a = 0.0;\n", i);
                    break;

                case COLOR_KEY_KILL_COLOR_AND_ALPHA:
                    mstring_append_fmt(vars, "  t%d = vec4(0.0);\n", i);
                    break;

                default:
                    assert(!"Unhandled key mode.");
                }

                mstring_append(vars, "}\n");
            }

            if (ps->state->rect_tex[i]) {
                mstring_append_fmt(preflight,
                "vec2 norm%d(vec2 coord) {\n"
                "    return coord / (textureSize(texSamp%d, 0) / texScale[%d]);\n"
                "}\n",
                i, i, i);
                mstring_append_fmt(preflight,
                "vec3 norm%d(vec3 coord) {\n"
                "    return vec3(norm%d(coord.xy), coord.z);\n"
                "}\n",
                i, i);
                mstring_append_fmt(preflight,
                "vec4 norm%d(vec4 coord) {\n"
                "    return vec4(norm%d(coord.xy), 0, coord.w);\n"
                "}\n",
                i, i);
            }
        }
    }

    for (int i = 0; i < ps->num_stages; i++) {
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

    if (ps->state->alpha_test && ps->state->alpha_func != ALPHA_FUNC_ALWAYS) {
        if (ps->state->alpha_func == ALPHA_FUNC_NEVER) {
            mstring_append(ps->code, "discard;\n");
        } else {
            const char* alpha_op;
            switch (ps->state->alpha_func) {
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
            mstring_append_fmt(ps->code,
                               "int fragAlpha = int(round(fragColor.a * 255.0));\n"
                               "if (!(fragAlpha %s alphaRef)) discard;\n",
                               alpha_op);
        }
    }

    for (int i = 0; i < ps->num_var_refs; i++) {
        mstring_append_fmt(vars, "vec4 %s = vec4(0);\n", ps->var_refs[i]);
        if (strcmp(ps->var_refs[i], "r0") == 0) {
            if (ps->tex_modes[0] != PS_TEXTUREMODES_NONE) {
                mstring_append(vars, "r0.a = t0.a;\n");
            } else {
                mstring_append(vars, "r0.a = 1.0;\n");
            }
        }
    }

    /* With integer depth buffers Xbox hardware floors values. For gl_FragDepth
     * range [0,1] Radeon floors values to integer depth buffer, but Intel UHD
     * 770 rounds to nearest. For 24-bit OpenGL/Vulkan integer depth buffer,
     * we divide the desired depth integer value by 16777216.0, then add 1 in
     * integer bit representation to get the same result as dividing the
     * desired depth integer by 16777215.0 would give. (GPUs can't divide by
     * 16777215.0, only multiply by 1.0/16777215.0 which gives different results
     * due to rounding.)
     */

    switch (ps->state->depth_format) {
    case DEPTH_FORMAT_D16:
        // 16-bit unsigned int
        mstring_append(
            ps->code,
            "gl_FragDepth = floor(zvalue) / 65535.0;\n");
        break;
    case DEPTH_FORMAT_D24:
        // 24-bit unsigned int
        mstring_append(
            ps->code,
            "gl_FragDepth = uintBitsToFloat(floatBitsToUint(floor(zvalue) / 16777216.0) + 1u);\n");
        break;
    default:
        // TODO: handle floating-point depth buffers properly
        mstring_append(ps->code, "gl_FragDepth = zvalue / clipRange.y;\n");
        break;
    }

    MString *final = mstring_new();
    mstring_append_fmt(final, "#version %d\n\n", ps->opts.vulkan ? 450 : 400);
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

MString *pgraph_glsl_gen_psh(const PshState *state, GenPshGlslOptions opts)
{
    int i;
    struct PixelShader ps;
    memset(&ps, 0, sizeof(ps));

    ps.opts = opts;
    ps.state = state;

    ps.num_stages = state->combiner_control & 0xFF;
    ps.flags = state->combiner_control >> 8;
    for (i = 0; i < 4; i++) {
        ps.tex_modes[i] = (state->shader_stage_program >> (i * 5)) & 0x1F;
    }

    ps.dot_map[0] = 0;
    ps.dot_map[1] = (state->other_stage_input >> 0) & 0xf;
    ps.dot_map[2] = (state->other_stage_input >> 4) & 0xf;
    ps.dot_map[3] = (state->other_stage_input >> 8) & 0xf;

    ps.input_tex[0] = -1;
    ps.input_tex[1] = 0;
    ps.input_tex[2] = (state->other_stage_input >> 16) & 0xF;
    ps.input_tex[3] = (state->other_stage_input >> 20) & 0xF;
    for (i = 0; i < ps.num_stages; i++) {
        parse_combiner_inputs(state->rgb_inputs[i],
            &ps.stage[i].rgb_input.a, &ps.stage[i].rgb_input.b,
            &ps.stage[i].rgb_input.c, &ps.stage[i].rgb_input.d);
        parse_combiner_inputs(state->alpha_inputs[i],
            &ps.stage[i].alpha_input.a, &ps.stage[i].alpha_input.b,
            &ps.stage[i].alpha_input.c, &ps.stage[i].alpha_input.d);

        parse_combiner_output(state->rgb_outputs[i], &ps.stage[i].rgb_output);
        parse_combiner_output(state->alpha_outputs[i], &ps.stage[i].alpha_output);
    }

    struct InputInfo blank;
    ps.final_input.enabled = state->final_inputs_0 || state->final_inputs_1;
    if (ps.final_input.enabled) {
        parse_combiner_inputs(state->final_inputs_0,
                              &ps.final_input.a, &ps.final_input.b,
                              &ps.final_input.c, &ps.final_input.d);
        parse_combiner_inputs(state->final_inputs_1,
                              &ps.final_input.e, &ps.final_input.f,
                              &ps.final_input.g, &blank);
        int flags = state->final_inputs_1 & 0xFF;
        ps.final_input.clamp_sum = flags & PS_FINALCOMBINERSETTING_CLAMP_SUM;
        ps.final_input.inv_v1 = flags & PS_FINALCOMBINERSETTING_COMPLEMENT_V1;
        ps.final_input.inv_r0 = flags & PS_FINALCOMBINERSETTING_COMPLEMENT_R0;
    }

    return psh_convert(&ps);
}

void pgraph_glsl_set_psh_uniform_values(PGRAPHState *pg,
                                        const PshUniformLocs locs,
                                        PshUniformValues *values)
{
    if (locs[PshUniform_consts] != -1) {
        for (int i = 0; i < 9; i++) {
            uint32_t constant[2];
            if (i == 8) {
                /* final combiner */
                constant[0] = pgraph_reg_r(pg, NV_PGRAPH_SPECFOGFACTOR0);
                constant[1] = pgraph_reg_r(pg, NV_PGRAPH_SPECFOGFACTOR1);
            } else {
                constant[0] =
                    pgraph_reg_r(pg, NV_PGRAPH_COMBINEFACTOR0 + i * 4);
                constant[1] =
                    pgraph_reg_r(pg, NV_PGRAPH_COMBINEFACTOR1 + i * 4);
            }

            for (int j = 0; j < 2; j++) {
                int idx = i * 2 + j;
                pgraph_argb_pack32_to_rgba_float(constant[j],
                                                 values->consts[idx]);
            }
        }
    }
    if (locs[PshUniform_alphaRef] != -1) {
        int alpha_ref = GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_CONTROL_0),
                                 NV_PGRAPH_CONTROL_0_ALPHAREF);
        values->alphaRef[0] = alpha_ref;
    }
    if (locs[PshUniform_colorKey] != -1) {
        values->colorKey[0] = pgraph_reg_r(pg, NV_PGRAPH_COLORKEYCOLOR0);
        values->colorKey[1] = pgraph_reg_r(pg, NV_PGRAPH_COLORKEYCOLOR1);
        values->colorKey[2] = pgraph_reg_r(pg, NV_PGRAPH_COLORKEYCOLOR2);
        values->colorKey[3] = pgraph_reg_r(pg, NV_PGRAPH_COLORKEYCOLOR3);
    }
    if (locs[PshUniform_colorKeyMask] != -1) {
       for (int i = 0; i < NV2A_MAX_TEXTURES; i++) {
            values->colorKeyMask[i] =
                get_color_key_mask_for_texture(pg, i);
        }
    }

    for (int i = 0; i < NV2A_MAX_TEXTURES; i++) {
        /* Bump luminance only during stages 1 - 3 */
        if (i > 0) {
            if (locs[PshUniform_bumpMat] != -1) {
                uint32_t m_u32[4];
                m_u32[0] = pgraph_reg_r(pg, NV_PGRAPH_BUMPMAT00 + 4 * (i - 1));
                m_u32[1] = pgraph_reg_r(pg, NV_PGRAPH_BUMPMAT01 + 4 * (i - 1));
                m_u32[2] = pgraph_reg_r(pg, NV_PGRAPH_BUMPMAT10 + 4 * (i - 1));
                m_u32[3] = pgraph_reg_r(pg, NV_PGRAPH_BUMPMAT11 + 4 * (i - 1));
                values->bumpMat[i][0] = *(float *)&m_u32[0];
                values->bumpMat[i][1] = *(float *)&m_u32[1];
                values->bumpMat[i][2] = *(float *)&m_u32[2];
                values->bumpMat[i][3] = *(float *)&m_u32[3];
            }
            if (locs[PshUniform_bumpScale] != -1) {
                uint32_t v =
                    pgraph_reg_r(pg, NV_PGRAPH_BUMPSCALE1 + (i - 1) * 4);
                values->bumpScale[i] = *(float *)&v;
            }
            if (locs[PshUniform_bumpOffset] != -1) {
                uint32_t v =
                    pgraph_reg_r(pg, NV_PGRAPH_BUMPOFFSET1 + (i - 1) * 4);
                values->bumpOffset[i] = *(float *)&v;
            }
        }
        if (locs[PshUniform_texScale] != -1) {
            values->texScale[0] = 1.0; /* Renderer will override this */
        }
    }

    if (locs[PshUniform_fogColor] != -1) {
        uint32_t fog_color = pgraph_reg_r(pg, NV_PGRAPH_FOGCOLOR);
        values->fogColor[0][0] =
            GET_MASK(fog_color, NV_PGRAPH_FOGCOLOR_RED) / 255.0;
        values->fogColor[0][1] =
            GET_MASK(fog_color, NV_PGRAPH_FOGCOLOR_GREEN) / 255.0;
        values->fogColor[0][2] =
            GET_MASK(fog_color, NV_PGRAPH_FOGCOLOR_BLUE) / 255.0;
        values->fogColor[0][3] =
            GET_MASK(fog_color, NV_PGRAPH_FOGCOLOR_ALPHA) / 255.0;
    }

    if (locs[PshUniform_clipRange] != -1) {
        pgraph_glsl_set_clip_range_uniform_value(pg, values->clipRange[0]);
    }

    bool polygon_offset_enabled = false;
    if (pg->primitive_mode >= PRIM_TYPE_TRIANGLES) {
        uint32_t raster = pgraph_reg_r(pg, NV_PGRAPH_SETUPRASTER);
        uint32_t polygon_mode =
            GET_MASK(raster, NV_PGRAPH_SETUPRASTER_FRONTFACEMODE);

        if ((polygon_mode == NV_PGRAPH_SETUPRASTER_FRONTFACEMODE_FILL &&
             (raster & NV_PGRAPH_SETUPRASTER_POFFSETFILLENABLE)) ||
            (polygon_mode == NV_PGRAPH_SETUPRASTER_FRONTFACEMODE_LINE &&
             (raster & NV_PGRAPH_SETUPRASTER_POFFSETLINEENABLE)) ||
            (polygon_mode == NV_PGRAPH_SETUPRASTER_FRONTFACEMODE_POINT &&
             (raster & NV_PGRAPH_SETUPRASTER_POFFSETPOINTENABLE))) {
            polygon_offset_enabled = true;
        }
    }

    if (locs[PshUniform_depthOffset] != -1) {
        float zbias = 0.0f;

        if (polygon_offset_enabled) {
            uint32_t zbias_u32 = pgraph_reg_r(pg, NV_PGRAPH_ZOFFSETBIAS);
            zbias = *(float *)&zbias_u32;
        }

        values->depthOffset[0] = zbias;
    }

    if (locs[PshUniform_depthFactor] != -1) {
        float zfactor = 0.0f;

        if (polygon_offset_enabled) {
            uint32_t zfactor_u32 = pgraph_reg_r(pg, NV_PGRAPH_ZOFFSETFACTOR);
            zfactor = *(float *)&zfactor_u32;
            if (zfactor != 0.0f &&
                (pgraph_reg_r(pg, NV_PGRAPH_CONTROL_0) &
                 NV_PGRAPH_CONTROL_0_Z_PERSPECTIVE_ENABLE)) {
                /* FIXME: for w-buffering, polygon slope in screen-space is
                 * computed per-pixel, but Xbox appears to use constant that
                 * is the polygon slope at the first visible pixel in top-left
                 * order.
                 */
                NV2A_UNIMPLEMENTED("NV_PGRAPH_ZOFFSETFACTOR only partially implemented for w-buffering");
            }
        }

        values->depthFactor[0] = zfactor;
    }

    if (locs[PshUniform_surfaceScale] != -1) {
        unsigned int wscale = 1, hscale = 1;
        pgraph_apply_anti_aliasing_factor(pg, &wscale, &hscale);
        pgraph_apply_scaling_factor(pg, &wscale, &hscale);
        values->surfaceScale[0][0] = wscale;
        values->surfaceScale[0][1] = hscale;
    }

    unsigned int max_gl_width = pg->surface_binding_dim.width;
    unsigned int max_gl_height = pg->surface_binding_dim.height;
    pgraph_apply_scaling_factor(pg, &max_gl_width, &max_gl_height);

    for (int i = 0; i < 8; i++) {
        uint32_t x = pgraph_reg_r(pg, NV_PGRAPH_WINDOWCLIPX0 + i * 4);
        unsigned int x_min = GET_MASK(x, NV_PGRAPH_WINDOWCLIPX0_XMIN);
        unsigned int x_max = GET_MASK(x, NV_PGRAPH_WINDOWCLIPX0_XMAX) + 1;

        uint32_t y = pgraph_reg_r(pg, NV_PGRAPH_WINDOWCLIPY0 + i * 4);
        unsigned int y_min = GET_MASK(y, NV_PGRAPH_WINDOWCLIPY0_YMIN);
        unsigned int y_max = GET_MASK(y, NV_PGRAPH_WINDOWCLIPY0_YMAX) + 1;

        pgraph_apply_anti_aliasing_factor(pg, &x_min, &y_min);
        pgraph_apply_anti_aliasing_factor(pg, &x_max, &y_max);

        pgraph_apply_scaling_factor(pg, &x_min, &y_min);
        pgraph_apply_scaling_factor(pg, &x_max, &y_max);

        values->clipRegion[i][0] = x_min;
        values->clipRegion[i][1] = y_min;
        values->clipRegion[i][2] = x_max;
        values->clipRegion[i][3] = y_max;
    }
}
