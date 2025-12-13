/*
 * Geforce NV2A PGRAPH GLSL Shader Generator
 *
 * Copyright (c) 2015 espes
 * Copyright (c) 2015 Jannik Vogel
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

#include "qemu/osdep.h"
#include "hw/xbox/nv2a/pgraph/pgraph.h"
#include "vsh.h"
#include "vsh-ff.h"
#include "vsh-prog.h"

DEF_UNIFORM_INFO_ARR(VshUniform, VSH_UNIFORM_DECL_X)

static void set_fixed_function_vsh_state(PGRAPHState *pg,
                                         FixedFunctionVshState *state)
{
    state->skinning = (enum VshSkinning)GET_MASK(
        pgraph_reg_r(pg, NV_PGRAPH_CSV0_D), NV_PGRAPH_CSV0_D_SKIN);
    state->normalization = pgraph_reg_r(pg, NV_PGRAPH_CSV0_C) &
                           NV_PGRAPH_CSV0_C_NORMALIZATION_ENABLE;
    state->local_eye =
        GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_CSV0_C), NV_PGRAPH_CSV0_C_LOCALEYE);

    state->emission_src = (enum MaterialColorSource)GET_MASK(
        pgraph_reg_r(pg, NV_PGRAPH_CSV0_C), NV_PGRAPH_CSV0_C_EMISSION);
    state->ambient_src = (enum MaterialColorSource)GET_MASK(
        pgraph_reg_r(pg, NV_PGRAPH_CSV0_C), NV_PGRAPH_CSV0_C_AMBIENT);
    state->diffuse_src = (enum MaterialColorSource)GET_MASK(
        pgraph_reg_r(pg, NV_PGRAPH_CSV0_C), NV_PGRAPH_CSV0_C_DIFFUSE);
    state->specular_src = (enum MaterialColorSource)GET_MASK(
        pgraph_reg_r(pg, NV_PGRAPH_CSV0_C), NV_PGRAPH_CSV0_C_SPECULAR);

    for (int i = 0; i < 4; i++) {
        state->texture_matrix_enable[i] = pg->texture_matrix_enable[i];
    }

    for (int i = 0; i < 4; i++) {
        unsigned int reg = (i < 2) ? NV_PGRAPH_CSV1_A : NV_PGRAPH_CSV1_B;
        for (int j = 0; j < 4; j++) {
            unsigned int masks[] = {
                (i % 2) ? NV_PGRAPH_CSV1_A_T1_S : NV_PGRAPH_CSV1_A_T0_S,
                (i % 2) ? NV_PGRAPH_CSV1_A_T1_T : NV_PGRAPH_CSV1_A_T0_T,
                (i % 2) ? NV_PGRAPH_CSV1_A_T1_R : NV_PGRAPH_CSV1_A_T0_R,
                (i % 2) ? NV_PGRAPH_CSV1_A_T1_Q : NV_PGRAPH_CSV1_A_T0_Q
            };
            state->texgen[i][j] =
                (enum VshTexgen)GET_MASK(pgraph_reg_r(pg, reg), masks[j]);
        }
    }

    state->lighting =
        GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_CSV0_C), NV_PGRAPH_CSV0_C_LIGHTING);
    if (state->lighting) {
        for (int i = 0; i < NV2A_MAX_LIGHTS; i++) {
            state->light[i] =
                (enum VshLight)GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_CSV0_D),
                                        NV_PGRAPH_CSV0_D_LIGHT0 << (i * 2));
        }
    }

    if (pgraph_reg_r(pg, NV_PGRAPH_CONTROL_3) & NV_PGRAPH_CONTROL_3_FOGENABLE) {
        state->foggen = (enum VshFoggen)GET_MASK(
            pgraph_reg_r(pg, NV_PGRAPH_CSV0_D), NV_PGRAPH_CSV0_D_FOGGENMODE);
    }
}

static void set_programmable_vsh_state(PGRAPHState *pg,
                                       ProgrammableVshState *prog)
{
    int program_start = GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_CSV0_C),
                                 NV_PGRAPH_CSV0_C_CHEOPS_PROGRAM_START);

    prog->program_length = 0;
    for (int i = program_start; i < NV2A_MAX_TRANSFORM_PROGRAM_LENGTH; i++) {
        uint32_t *cur_token = (uint32_t *)&pg->program_data[i];
        memcpy(&prog->program_data[prog->program_length], cur_token,
               VSH_TOKEN_SIZE * sizeof(uint32_t));
        prog->program_length++;

        if (vsh_get_field(cur_token, FLD_FINAL)) {
            break;
        }
    }
}

void pgraph_glsl_set_vsh_state(PGRAPHState *pg, VshState *vsh)
{
    bool vertex_program = GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_CSV0_D),
                                   NV_PGRAPH_CSV0_D_MODE) == 2;

    bool fixed_function = GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_CSV0_D),
                                   NV_PGRAPH_CSV0_D_MODE) == 0;

    assert(vertex_program || fixed_function);

    vsh->surface_scale_factor = pg->surface_scale_factor; // FIXME

    vsh->compressed_attrs = pg->compressed_attrs;
    vsh->uniform_attrs = pg->uniform_attrs;
    vsh->swizzle_attrs = pg->swizzle_attrs;

    vsh->specular_enable = GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_CSV0_C),
                                    NV_PGRAPH_CSV0_C_SPECULAR_ENABLE);
    vsh->separate_specular = GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_CSV0_C),
                                      NV_PGRAPH_CSV0_C_SEPARATE_SPECULAR);
    vsh->ignore_specular_alpha =
        !GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_CSV0_C),
                  NV_PGRAPH_CSV0_C_ALPHA_FROM_MATERIAL_SPECULAR);
    vsh->specular_power = pg->specular_power;
    vsh->specular_power_back = pg->specular_power_back;

    vsh->z_perspective = pgraph_reg_r(pg, NV_PGRAPH_CONTROL_0) &
                         NV_PGRAPH_CONTROL_0_Z_PERSPECTIVE_ENABLE;

    vsh->point_params_enable = GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_CSV0_D),
                                        NV_PGRAPH_CSV0_D_POINTPARAMSENABLE);
    vsh->point_size = pgraph_reg_r(pg, NV_PGRAPH_POINTSIZE) / 8.0f;
    if (vsh->point_params_enable) {
        for (int i = 0; i < 8; i++) {
            vsh->point_params[i] = pg->point_params[i];
        }
    }

    vsh->smooth_shading = GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_CONTROL_3),
                                   NV_PGRAPH_CONTROL_3_SHADEMODE) ==
                          NV_PGRAPH_CONTROL_3_SHADEMODE_SMOOTH;

    vsh->fog_enable =
        pgraph_reg_r(pg, NV_PGRAPH_CONTROL_3) & NV_PGRAPH_CONTROL_3_FOGENABLE;
    if (vsh->fog_enable) {
        /*FIXME: Use CSV0_D? */
        vsh->fog_mode =
            (enum VshFogMode)GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_CONTROL_3),
                                      NV_PGRAPH_CONTROL_3_FOG_MODE);
    }

    vsh->is_fixed_function = fixed_function;
    if (fixed_function) {
        set_fixed_function_vsh_state(pg, &vsh->fixed_function);
    } else {
        set_programmable_vsh_state(pg, &vsh->programmable);
    }
}

MString *pgraph_glsl_gen_vsh(const VshState *state, GenVshGlslOptions opts)
{
    MString *uniforms = mstring_new();
    const char *u = opts.vulkan ? "" : "uniform ";
    for (int i = 0; i < ARRAY_SIZE(VshUniformInfo); i++) {
        const UniformInfo *info = &VshUniformInfo[i];
        const char *type_str = uniform_element_type_to_str[info->type];
        if (i == VshUniform_inlineValue &&
            (!state->uniform_attrs ||
             opts.use_push_constants_for_uniform_attrs)) {
            continue;
        }
        if (info->count == 1) {
            mstring_append_fmt(uniforms, "%s%s %s;\n", u, type_str,
                               info->name);
        } else {
            mstring_append_fmt(uniforms, "%s%s %s[%zd];\n", u, type_str,
                               info->name, info->count);
        }
    }

    MString *header = mstring_from_str(
        GLSL_DEFINE(fogPlane, GLSL_C(NV_IGRAPH_XF_XFCTX_FOG))
        GLSL_DEFINE(texMat0, GLSL_C_MAT4(NV_IGRAPH_XF_XFCTX_T0MAT))
        GLSL_DEFINE(texMat1, GLSL_C_MAT4(NV_IGRAPH_XF_XFCTX_T1MAT))
        GLSL_DEFINE(texMat2, GLSL_C_MAT4(NV_IGRAPH_XF_XFCTX_T2MAT))
        GLSL_DEFINE(texMat3, GLSL_C_MAT4(NV_IGRAPH_XF_XFCTX_T3MAT))

        "\n"
        "#define FLOAT_MAX uintBitsToFloat(0x7F7FFFFFu)\n"
        "\n"
        "vec4 oPos = vec4(0.0,0.0,0.0,1.0);\n"
        "vec4 oD0 = vec4(0.0,0.0,0.0,1.0);\n"
        "vec4 oD1 = vec4(0.0,0.0,0.0,1.0);\n"
        "vec4 oB0 = vec4(0.0,0.0,0.0,1.0);\n"
        "vec4 oB1 = vec4(0.0,0.0,0.0,1.0);\n"
        "vec4 oPts = vec4(0.0,0.0,0.0,1.0);\n"
        "vec4 oFog = vec4(0.0,0.0,0.0,1.0);\n"
        "vec4 oT0 = vec4(0.0,0.0,0.0,1.0);\n"
        "vec4 oT1 = vec4(0.0,0.0,0.0,1.0);\n"
        "vec4 oT2 = vec4(0.0,0.0,0.0,1.0);\n"
        "vec4 oT3 = vec4(0.0,0.0,0.0,1.0);\n"
        "\n"
        "vec4 decompress_11_11_10(int cmp) {\n"
        "    float x = float(bitfieldExtract(cmp, 0,  11)) / 1023.0;\n"
        "    float y = float(bitfieldExtract(cmp, 11, 11)) / 1023.0;\n"
        "    float z = float(bitfieldExtract(cmp, 22, 10)) / 511.0;\n"
        "    return vec4(x, y, z, 1);\n"
        "}\n"
        "\n"
        // Clamp to range [2^(-64), 2^64] or [-2^64, -2^(-64)].
        "float clampAwayZeroInf(float t) {\n"
        "  if (t > 0.0 || floatBitsToUint(t) == 0) {\n"
        "    t = clamp(t, uintBitsToFloat(0x1F800000), uintBitsToFloat(0x5F800000));\n"
        "  } else {\n"
        "    t = clamp(t, uintBitsToFloat(0xDF800000), uintBitsToFloat(0x9F800000));\n"
        "  }\n"
        "  return t;\n"
        "}\n"
        "\n"
        "vec4 NaNToOne(vec4 src) {\n"
        "  return mix(src, vec4(1.0), isnan(src));\n"
        "}\n"
        "vec4 NaNToValue(vec4 src, float replacement) {\n"
        "  return mix(src, vec4(replacement), isnan(src));\n"
        "}\n"
        "\n"
        // Xbox NV2A rasterizer appears to have 4 bit precision fixed-point
        // fractional part and to convert floating-point coordinates by
        // by truncating (not flooring).
        "vec2 roundScreenCoords(vec2 pos) {\n"
        "  return trunc(pos * 16.0f) / 16.0f;\n"
        "}\n");

    pgraph_glsl_get_vtx_header(header, opts.vulkan, state->smooth_shading,
                               false, opts.prefix_outputs, false);

    if (opts.prefix_outputs) {
        mstring_append(header,
                       "#define vtxD0 v_vtxD0\n"
                       "#define vtxD1 v_vtxD1\n"
                       "#define vtxB0 v_vtxB0\n"
                       "#define vtxB1 v_vtxB1\n"
                       "#define vtxFog v_vtxFog\n"
                       "#define vtxT0 v_vtxT0\n"
                       "#define vtxT1 v_vtxT1\n"
                       "#define vtxT2 v_vtxT2\n"
                       "#define vtxT3 v_vtxT3\n"
                       "#define vtxPos0 v_vtxPos0\n"
                       "#define vtxPos1 v_vtxPos1\n"
                       "#define vtxPos2 v_vtxPos2\n"
                       "#define triMZ v_triMZ\n"
                       );
    }
    mstring_append(header, "\n");

    int num_uniform_attrs = 0;

    for (int i = 0; i < NV2A_VERTEXSHADER_ATTRIBUTES; i++) {
        bool is_uniform = state->uniform_attrs & (1 << i);
        bool is_swizzled = state->swizzle_attrs & (1 << i);
        bool is_compressed = state->compressed_attrs & (1 << i);

        assert(!(is_uniform && is_compressed));
        assert(!(is_uniform && is_swizzled));

        if (is_uniform) {
            mstring_append_fmt(header, "vec4 v%d = inlineValue[%d];\n", i,
                               num_uniform_attrs);
            num_uniform_attrs += 1;
        } else {
            if (state->compressed_attrs & (1 << i)) {
                mstring_append_fmt(header,
                                   "layout(location = %d) in int v%d_cmp;\n", i, i);
            } else if (state->swizzle_attrs & (1 << i)) {
                mstring_append_fmt(header, "layout(location = %d) in vec4 v%d_sw;\n",
                                   i, i);
            } else {
                mstring_append_fmt(header, "layout(location = %d) in vec4 v%d;\n",
                                   i, i);
            }
        }
    }

    mstring_append(header, "\n");

    MString *body = mstring_from_str("void main() {\n");

    for (int i = 0; i < NV2A_VERTEXSHADER_ATTRIBUTES; i++) {
        if (state->compressed_attrs & (1 << i)) {
            mstring_append_fmt(
                body, "vec4 v%d = decompress_11_11_10(v%d_cmp);\n", i, i);
        }

        if (state->swizzle_attrs & (1 << i)) {
            mstring_append_fmt(body, "vec4 v%d = v%d_sw.bgra;\n", i, i);
        }

    }

    if (state->is_fixed_function) {
        pgraph_glsl_gen_vsh_ff(state, header, body);
    } else {
        pgraph_glsl_gen_vsh_prog(
            VSH_VERSION_XVS, (uint32_t *)state->programmable.program_data,
            state->programmable.program_length, header, body);
        if (!state->point_params_enable) {
            mstring_append_fmt(body, "  oPts.x = %f * %d;\n",
                               state->point_size <= 0.f ? 1.f :
                                                          state->point_size,
                               state->surface_scale_factor);
        }
    }

    if (!state->fog_enable) {
        /* FIXME: Is the fog still calculated / passed somehow?! */
        mstring_append(body, "  oFog = vec4(1.0);\n");
    } else {
        if (!state->is_fixed_function) {
            /* FIXME: Does foggen do something here? Let's do some tracking..
             *
             *   "RollerCoaster Tycoon" has
             *      state->vertex_program = true; state->foggen == FOGGEN_PLANAR
             *      but expects oFog.x as fogdistance?! Writes oFog.xyzw = v0.z
             */
            mstring_append(body, "  float fogDistance = oFog.x;\n");
        }

        /* FIXME: Do this per pixel? */

        float infinite_fogdistance_result = 0.0f;
        float nan_fogfactor_result = 0.0f;

        switch (state->fog_mode) {
        case FOG_MODE_LINEAR:
        case FOG_MODE_LINEAR_ABS:

            /* f = (end - d) / (end - start)
             *    fogParam.y = -1 / (end - start)
             *    fogParam.x = 1 - end * fogParam.y;
             */
            infinite_fogdistance_result = 1.0f;
            nan_fogfactor_result = 1.0f;
            mstring_append(body, "  float fogFactor = fogParam.x + fogDistance * fogParam.y;\n");
            mstring_append(body, "  fogFactor -= 1.0;\n");
            break;
        case FOG_MODE_EXP:
            infinite_fogdistance_result = 1.0f;
            nan_fogfactor_result = 1.0f;
            /* fallthrough */
        case FOG_MODE_EXP_ABS:

            /* f = 1 / (e^(d * density))
             *    fogParam.y = -density / (2 * ln(256))
             *    fogParam.x = 1.5
             */
            mstring_append(body, "  float fogFactor = fogParam.x + exp2(fogDistance * fogParam.y * 16.0);\n");
            mstring_append(body, "  fogFactor -= 1.5;\n");
            break;
        case FOG_MODE_EXP2:
        case FOG_MODE_EXP2_ABS:

            /* f = 1 / (e^((d * density)^2))
             *    fogParam.y = -density / (2 * sqrt(ln(256)))
             *    fogParam.x = 1.5
             */

            mstring_append(body, "  float fogFactor = fogParam.x + exp2(-fogDistance * fogDistance * fogParam.y * fogParam.y * 32.0);\n");
            mstring_append(body, "  fogFactor -= 1.5;\n");
            break;
        default:
            assert(false);
            break;
        }
        switch (state->fog_mode) {
        case FOG_MODE_LINEAR_ABS:
        case FOG_MODE_EXP_ABS:
        case FOG_MODE_EXP2_ABS:
            mstring_append(body, "  fogFactor = abs(fogFactor);\n");
            break;
        default:
            break;
        }

        /* Fog is clamped to min/max normal float values here to match HW
         * interpolation. It is then clamped to [0,1] in the pixel shader.
         */
        // clang-format off
        mstring_append_fmt(
            body,
            "  if (isinf(fogDistance)) {\n"
            "    oFog = vec4(%f);\n"
            "  } else {\n"
            "    oFog = clamp(NaNToValue(vec4(fogFactor), %f), -FLOAT_MAX, FLOAT_MAX);\n"
            "  }\n",
            infinite_fogdistance_result, nan_fogfactor_result);
        // clang-format on
    }

    mstring_append(body, "\n"
                   "  vtxD0 = clamp(NaNToOne(oD0), 0.0, 1.0);\n"
                   "  vtxB0 = clamp(NaNToOne(oB0), 0.0, 1.0);\n"
                   "  vtxFog = oFog.x;\n"
                   "  vtxT0 = oT0;\n"
                   "  vtxT1 = oT1;\n"
                   "  vtxT2 = oT2;\n"
                   "  vtxT3 = oT3;\n"
                   "  vtxPos0 = vtxPos;\n"
                   "  vtxPos1 = vtxPos;\n"
                   "  vtxPos2 = vtxPos;\n"
                   "  triMZ = 0.0;\n"
                   "  gl_PointSize = oPts.x;\n"
    );

    if (state->specular_enable) {
        mstring_append(body,
                       "  vtxD1 = clamp(NaNToOne(oD1), 0.0, 1.0);\n"
                       "  vtxB1 = clamp(NaNToOne(oB1), 0.0, 1.0);\n"
        );

        if (state->ignore_specular_alpha) {
            mstring_append(body,
                           "  vtxD1.w = 1.0;\n"
                           "  vtxB1.w = 1.0;\n"
            );
        }
    } else {
        mstring_append(body,
                       "  vtxD1 = vec4(0.0, 0.0, 0.0, 1.0);\n"
                       "  vtxB1 = vec4(0.0, 0.0, 0.0, 1.0);\n"
        );
    }

    if (opts.vulkan) {
        mstring_append(body,
                   "  gl_Position = oPos;\n"
        );
    } else {
        mstring_append(body,
                   "  gl_Position = vec4(oPos.x, oPos.y, 2.0*oPos.z - oPos.w, oPos.w);\n"
        );
    }

    mstring_append(body, "}\n");

    /* Return combined header + source */
    MString *output =
        mstring_from_fmt("#version %d\n\n", opts.vulkan ? 450 : 400);

    if (opts.vulkan) {
        // FIXME: Optimize uniforms
        if (num_uniform_attrs > 0 &&
            opts.use_push_constants_for_uniform_attrs) {
            mstring_append_fmt(output,
                               "layout(push_constant) uniform PushConstants {\n"
                               "    vec4 inlineValue[%d];\n"
                               "};\n\n",
                               num_uniform_attrs);
        }
        mstring_append_fmt(
            output,
            "layout(binding = %d, std140) uniform VshUniforms {\n"
            "%s"
            "};\n\n",
            opts.ubo_binding, mstring_get_str(uniforms));
    } else {
        mstring_append(
            output, mstring_get_str(uniforms));
    }

    mstring_append(output, mstring_get_str(header));
    mstring_unref(header);

    mstring_append(output, mstring_get_str(body));
    mstring_unref(body);

    return output;
}

void pgraph_glsl_set_vsh_uniform_values(PGRAPHState *pg, const VshState *state,
                                        const VshUniformLocs locs,
                                        VshUniformValues *values)
{
    if (locs[VshUniform_c] != -1) {
        QEMU_BUILD_BUG_MSG(sizeof(values->c) != sizeof(pg->vsh_constants),
                           "Uniform value size inconsistency");
        memcpy(values->c, pg->vsh_constants, sizeof(pg->vsh_constants));
    }

    if (locs[VshUniform_clipRange] != -1) {
        pgraph_glsl_set_clip_range_uniform_value(pg, values->clipRange[0]);
    }

    if (locs[VshUniform_fogParam] != -1) {
        uint32_t param_0 = pgraph_reg_r(pg, NV_PGRAPH_FOGPARAM0);
        uint32_t param_1 = pgraph_reg_r(pg, NV_PGRAPH_FOGPARAM1);
        values->fogParam[0][0] = *(float *)&param_0;
        values->fogParam[0][1] = *(float *)&param_1;
    }

    if (locs[VshUniform_pointParams] != -1) {
        QEMU_BUILD_BUG_MSG(sizeof(values->pointParams) !=
                               sizeof(pg->point_params),
                           "Uniform value size inconsistency");
        memcpy(values->pointParams, pg->point_params, sizeof(pg->point_params));
    }

    if (locs[VshUniform_material_alpha] != -1) {
        values->material_alpha[0] = pg->material_alpha;
    }

    if (locs[VshUniform_inlineValue] != -1) {
        pgraph_get_inline_values(pg, state->uniform_attrs, values->inlineValue,
                                 NULL);
    }

    if (locs[VshUniform_surfaceSize] != -1) {
        unsigned int aa_width = 1, aa_height = 1;
        pgraph_apply_anti_aliasing_factor(pg, &aa_width, &aa_height);
        float width = (float)pg->surface_binding_dim.width / aa_width;
        float height = (float)pg->surface_binding_dim.height / aa_height;
        values->surfaceSize[0][0] = width;
        values->surfaceSize[0][1] = height;
    }

    if (state->is_fixed_function) {
        if (locs[VshUniform_ltctxa] != -1) {
            QEMU_BUILD_BUG_MSG(sizeof(values->ltctxa) != sizeof(pg->ltctxa),
                               "Uniform value size inconsistency");
            memcpy(values->ltctxa, pg->ltctxa, sizeof(pg->ltctxa));
        }

        if (locs[VshUniform_ltctxb] != -1) {
            QEMU_BUILD_BUG_MSG(sizeof(values->ltctxb) != sizeof(pg->ltctxb),
                               "Uniform value size inconsistency");
            memcpy(values->ltctxb, pg->ltctxb, sizeof(pg->ltctxb));
        }

        if (locs[VshUniform_ltc1] != -1) {
            QEMU_BUILD_BUG_MSG(sizeof(values->ltc1) != sizeof(pg->ltc1),
                               "Uniform value size inconsistency");
            memcpy(values->ltc1, pg->ltc1, sizeof(pg->ltc1));
        }

        if (locs[VshUniform_lightInfiniteHalfVector] != -1) {
            QEMU_BUILD_BUG_MSG(sizeof(values->lightInfiniteHalfVector) !=
                                   sizeof(pg->light_infinite_half_vector),
                               "Uniform value size inconsistency");
            memcpy(values->lightInfiniteHalfVector,
                   pg->light_infinite_half_vector,
                   sizeof(pg->light_infinite_half_vector));
        }

        if (locs[VshUniform_lightInfiniteDirection] != -1) {
            QEMU_BUILD_BUG_MSG(sizeof(values->lightInfiniteDirection) !=
                                   sizeof(pg->light_infinite_direction),
                               "Uniform value size inconsistency");
            memcpy(values->lightInfiniteDirection, pg->light_infinite_direction,
                   sizeof(pg->light_infinite_direction));
        }

        if (locs[VshUniform_lightLocalPosition] != -1) {
            QEMU_BUILD_BUG_MSG(sizeof(values->lightLocalPosition) !=
                                   sizeof(pg->light_local_position),
                               "Uniform value size inconsistency");
            memcpy(values->lightLocalPosition, pg->light_local_position,
                   sizeof(pg->light_local_position));
        }

        if (locs[VshUniform_lightLocalAttenuation] != -1) {
            QEMU_BUILD_BUG_MSG(sizeof(values->lightLocalAttenuation) !=
                                   sizeof(pg->light_local_attenuation),
                               "Uniform value size inconsistency");
            memcpy(values->lightLocalAttenuation, pg->light_local_attenuation,
                   sizeof(pg->light_local_attenuation));
        }

        if (locs[VshUniform_specularPower] != -1) {
            values->specularPower[0] = pg->specular_power;
        }
    }
}
