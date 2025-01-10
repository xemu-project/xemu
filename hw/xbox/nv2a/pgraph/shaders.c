/*
 * Geforce NV2A PGRAPH OpenGL Renderer
 *
 * Copyright (c) 2015 espes
 * Copyright (c) 2015 Jannik Vogel
 * Copyright (c) 2020-2024 Matt Borgerson
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

#include "hw/xbox/nv2a/debug.h"
#include "texture.h"
#include "pgraph.h"
#include "shaders.h"

ShaderState pgraph_get_shader_state(PGRAPHState *pg)
{
    bool vertex_program = GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_CSV0_D),
                                   NV_PGRAPH_CSV0_D_MODE) == 2;

    bool fixed_function = GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_CSV0_D),
                                   NV_PGRAPH_CSV0_D_MODE) == 0;

    int program_start = GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_CSV0_C),
                                 NV_PGRAPH_CSV0_C_CHEOPS_PROGRAM_START);

    pg->program_data_dirty = false;

    ShaderState state;

    // We will hash it, so make sure any padding is zeroed
    memset(&state, 0, sizeof(ShaderState));

    state.surface_scale_factor = pg->surface_scale_factor;

    state.compressed_attrs = pg->compressed_attrs;
    state.uniform_attrs = pg->uniform_attrs;
    state.swizzle_attrs = pg->swizzle_attrs;

    /* register combiner stuff */
    state.psh.window_clip_exclusive =
        pgraph_reg_r(pg, NV_PGRAPH_SETUPRASTER) & NV_PGRAPH_SETUPRASTER_WINDOWCLIPTYPE;
    state.psh.combiner_control = pgraph_reg_r(pg, NV_PGRAPH_COMBINECTL);
    state.psh.shader_stage_program = pgraph_reg_r(pg, NV_PGRAPH_SHADERPROG);
    state.psh.other_stage_input = pgraph_reg_r(pg, NV_PGRAPH_SHADERCTL);
    state.psh.final_inputs_0 = pgraph_reg_r(pg, NV_PGRAPH_COMBINESPECFOG0);
    state.psh.final_inputs_1 = pgraph_reg_r(pg, NV_PGRAPH_COMBINESPECFOG1);

    state.psh.alpha_test =
        pgraph_reg_r(pg, NV_PGRAPH_CONTROL_0) & NV_PGRAPH_CONTROL_0_ALPHATESTENABLE;
    state.psh.alpha_func = (enum PshAlphaFunc)GET_MASK(
        pgraph_reg_r(pg, NV_PGRAPH_CONTROL_0), NV_PGRAPH_CONTROL_0_ALPHAFUNC);

    state.psh.point_sprite = pgraph_reg_r(pg, NV_PGRAPH_SETUPRASTER) &
                             NV_PGRAPH_SETUPRASTER_POINTSMOOTHENABLE;

    state.psh.shadow_depth_func = (enum PshShadowDepthFunc)GET_MASK(
        pgraph_reg_r(pg, NV_PGRAPH_SHADOWCTL), NV_PGRAPH_SHADOWCTL_SHADOW_ZFUNC);

    state.fixed_function = fixed_function;

    /* fixed function stuff */
    if (fixed_function) {
        state.skinning = (enum VshSkinning)GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_CSV0_D),
                                                    NV_PGRAPH_CSV0_D_SKIN);
        state.lighting =
            GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_CSV0_C), NV_PGRAPH_CSV0_C_LIGHTING);
        state.normalization =
            pgraph_reg_r(pg, NV_PGRAPH_CSV0_C) & NV_PGRAPH_CSV0_C_NORMALIZATION_ENABLE;

        /* color material */
        state.emission_src = (enum MaterialColorSource)GET_MASK(
            pgraph_reg_r(pg, NV_PGRAPH_CSV0_C), NV_PGRAPH_CSV0_C_EMISSION);
        state.ambient_src = (enum MaterialColorSource)GET_MASK(
            pgraph_reg_r(pg, NV_PGRAPH_CSV0_C), NV_PGRAPH_CSV0_C_AMBIENT);
        state.diffuse_src = (enum MaterialColorSource)GET_MASK(
            pgraph_reg_r(pg, NV_PGRAPH_CSV0_C), NV_PGRAPH_CSV0_C_DIFFUSE);
        state.specular_src = (enum MaterialColorSource)GET_MASK(
            pgraph_reg_r(pg, NV_PGRAPH_CSV0_C), NV_PGRAPH_CSV0_C_SPECULAR);
    }

    /* vertex program stuff */
    state.vertex_program = vertex_program,
    state.z_perspective = pgraph_reg_r(pg, NV_PGRAPH_CONTROL_0) &
                          NV_PGRAPH_CONTROL_0_Z_PERSPECTIVE_ENABLE;

    state.point_params_enable = GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_CSV0_D),
                                         NV_PGRAPH_CSV0_D_POINTPARAMSENABLE);
    state.point_size =
        GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_POINTSIZE), NV097_SET_POINT_SIZE_V) / 8.0f;
    if (state.point_params_enable) {
        for (int i = 0; i < 8; i++) {
            state.point_params[i] = pg->point_params[i];
        }
    }

    /* geometry shader stuff */
    state.primitive_mode = (enum ShaderPrimitiveMode)pg->primitive_mode;
    state.polygon_front_mode = (enum ShaderPolygonMode)GET_MASK(
        pgraph_reg_r(pg, NV_PGRAPH_SETUPRASTER), NV_PGRAPH_SETUPRASTER_FRONTFACEMODE);
    state.polygon_back_mode = (enum ShaderPolygonMode)GET_MASK(
        pgraph_reg_r(pg, NV_PGRAPH_SETUPRASTER), NV_PGRAPH_SETUPRASTER_BACKFACEMODE);

    state.smooth_shading = GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_CONTROL_3),
                                    NV_PGRAPH_CONTROL_3_SHADEMODE) ==
                           NV_PGRAPH_CONTROL_3_SHADEMODE_SMOOTH;
    state.psh.smooth_shading = state.smooth_shading;

    state.program_length = 0;

    if (vertex_program) {
        // copy in vertex program tokens
        for (int i = program_start; i < NV2A_MAX_TRANSFORM_PROGRAM_LENGTH;
             i++) {
            uint32_t *cur_token = (uint32_t *)&pg->program_data[i];
            memcpy(&state.program_data[state.program_length], cur_token,
                   VSH_TOKEN_SIZE * sizeof(uint32_t));
            state.program_length++;

            if (vsh_get_field(cur_token, FLD_FINAL)) {
                break;
            }
        }
    }

    /* Texgen */
    for (int i = 0; i < 4; i++) {
        unsigned int reg = (i < 2) ? NV_PGRAPH_CSV1_A : NV_PGRAPH_CSV1_B;
        for (int j = 0; j < 4; j++) {
            unsigned int masks[] = {
                (i % 2) ? NV_PGRAPH_CSV1_A_T1_S : NV_PGRAPH_CSV1_A_T0_S,
                (i % 2) ? NV_PGRAPH_CSV1_A_T1_T : NV_PGRAPH_CSV1_A_T0_T,
                (i % 2) ? NV_PGRAPH_CSV1_A_T1_R : NV_PGRAPH_CSV1_A_T0_R,
                (i % 2) ? NV_PGRAPH_CSV1_A_T1_Q : NV_PGRAPH_CSV1_A_T0_Q
            };
            state.texgen[i][j] =
                (enum VshTexgen)GET_MASK(pgraph_reg_r(pg, reg), masks[j]);
        }
    }

    /* Fog */
    state.fog_enable =
        pgraph_reg_r(pg, NV_PGRAPH_CONTROL_3) & NV_PGRAPH_CONTROL_3_FOGENABLE;
    if (state.fog_enable) {
        /*FIXME: Use CSV0_D? */
        state.fog_mode = (enum VshFogMode)GET_MASK(
            pgraph_reg_r(pg, NV_PGRAPH_CONTROL_3), NV_PGRAPH_CONTROL_3_FOG_MODE);
        state.foggen = (enum VshFoggen)GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_CSV0_D),
                                                NV_PGRAPH_CSV0_D_FOGGENMODE);
    } else {
        /* FIXME: Do we still pass the fogmode? */
        state.fog_mode = (enum VshFogMode)0;
        state.foggen = (enum VshFoggen)0;
    }

    /* Texture matrices */
    for (int i = 0; i < 4; i++) {
        state.texture_matrix_enable[i] = pg->texture_matrix_enable[i];
    }

    /* Lighting */
    if (state.lighting) {
        for (int i = 0; i < NV2A_MAX_LIGHTS; i++) {
            state.light[i] = (enum VshLight)GET_MASK(
                pgraph_reg_r(pg, NV_PGRAPH_CSV0_D), NV_PGRAPH_CSV0_D_LIGHT0 << (i * 2));
        }
    }

    /* Copy content of enabled combiner stages */
    int num_stages = pgraph_reg_r(pg, NV_PGRAPH_COMBINECTL) & 0xFF;
    for (int i = 0; i < num_stages; i++) {
        state.psh.rgb_inputs[i] = pgraph_reg_r(pg, NV_PGRAPH_COMBINECOLORI0 + i * 4);
        state.psh.rgb_outputs[i] = pgraph_reg_r(pg, NV_PGRAPH_COMBINECOLORO0 + i * 4);
        state.psh.alpha_inputs[i] = pgraph_reg_r(pg, NV_PGRAPH_COMBINEALPHAI0 + i * 4);
        state.psh.alpha_outputs[i] = pgraph_reg_r(pg, NV_PGRAPH_COMBINEALPHAO0 + i * 4);
        // constant_0[i] = pgraph_reg_r(pg, NV_PGRAPH_COMBINEFACTOR0 + i * 4);
        // constant_1[i] = pgraph_reg_r(pg, NV_PGRAPH_COMBINEFACTOR1 + i * 4);
    }

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            state.psh.compare_mode[i][j] =
                (pgraph_reg_r(pg, NV_PGRAPH_SHADERCLIPMODE) >> (4 * i + j)) & 1;
        }

        uint32_t ctl_0 = pgraph_reg_r(pg, NV_PGRAPH_TEXCTL0_0 + i * 4);
        bool enabled = pgraph_is_texture_stage_active(pg, i) &&
                       (ctl_0 & NV_PGRAPH_TEXCTL0_0_ENABLE);
        if (!enabled) {
            continue;
        }

        state.psh.alphakill[i] = ctl_0 & NV_PGRAPH_TEXCTL0_0_ALPHAKILLEN;

        uint32_t tex_fmt = pgraph_reg_r(pg, NV_PGRAPH_TEXFMT0 + i * 4);
        state.psh.dim_tex[i] = GET_MASK(tex_fmt, NV_PGRAPH_TEXFMT0_DIMENSIONALITY);

        unsigned int color_format = GET_MASK(tex_fmt, NV_PGRAPH_TEXFMT0_COLOR);
        BasicColorFormatInfo f = kelvin_color_format_info_map[color_format];
        state.psh.rect_tex[i] = f.linear;
        state.psh.tex_x8y24[i] = color_format == NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_DEPTH_X8_Y24_FIXED ||
                                color_format == NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_DEPTH_X8_Y24_FLOAT;

        uint32_t border_source =
            GET_MASK(tex_fmt, NV_PGRAPH_TEXFMT0_BORDER_SOURCE);
        bool cubemap = GET_MASK(tex_fmt, NV_PGRAPH_TEXFMT0_CUBEMAPENABLE);
        state.psh.border_logical_size[i][0] = 0.0f;
        state.psh.border_logical_size[i][1] = 0.0f;
        state.psh.border_logical_size[i][2] = 0.0f;
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

                state.psh.border_logical_size[i][0] = reported_width;
                state.psh.border_logical_size[i][1] = reported_height;
                state.psh.border_logical_size[i][2] = reported_depth;

                if (reported_width < 8) {
                    state.psh.border_inv_real_size[i][0] = 0.0625f;
                } else {
                    state.psh.border_inv_real_size[i][0] =
                        1.0f / (reported_width * 2.0f);
                }
                if (reported_height < 8) {
                    state.psh.border_inv_real_size[i][1] = 0.0625f;
                } else {
                    state.psh.border_inv_real_size[i][1] =
                        1.0f / (reported_height * 2.0f);
                }
                if (reported_depth < 8) {
                    state.psh.border_inv_real_size[i][2] = 0.0625f;
                } else {
                    state.psh.border_inv_real_size[i][2] =
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
        state.psh.snorm_tex[i] = (f.gl_internal_format == GL_RGB8_SNORM)
                                 || (f.gl_internal_format == GL_RG8_SNORM);
#endif
        state.psh.shadow_map[i] = f.depth;

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

        state.psh.conv_tex[i] = kernel;
    }

    return state;
}
