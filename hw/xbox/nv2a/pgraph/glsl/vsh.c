/*
 * Geforce NV2A PGRAPH GLSL Shader Generator
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

#include "qemu/osdep.h"
#include "hw/xbox/nv2a/pgraph/shaders.h"
#include "common.h"
#include "vsh.h"
#include "vsh-ff.h"
#include "vsh-prog.h"
#include <stdbool.h>

MString *pgraph_gen_vsh_glsl(const ShaderState *state, bool prefix_outputs)
{
    int i;
    MString *output = mstring_new();
    mstring_append_fmt(output, "#version %d\n\n", state->vulkan ? 450 : 400);

    MString *header = mstring_from_str("");
    
    MString *uniforms = mstring_from_str("");

    const char *u = state->vulkan ? "" : "uniform "; // FIXME: Remove

    mstring_append_fmt(uniforms,
        "%svec4 clipRange;\n"
        "%svec2 surfaceSize;\n"
        "%svec4 c[" stringify(NV2A_VERTEXSHADER_CONSTANTS) "];\n"
        "%svec2 fogParam;\n",
        u, u, u, u
        );

    mstring_append(header,
        GLSL_DEFINE(fogPlane, GLSL_C(NV_IGRAPH_XF_XFCTX_FOG))
        GLSL_DEFINE(texMat0, GLSL_C_MAT4(NV_IGRAPH_XF_XFCTX_T0MAT))
        GLSL_DEFINE(texMat1, GLSL_C_MAT4(NV_IGRAPH_XF_XFCTX_T1MAT))
        GLSL_DEFINE(texMat2, GLSL_C_MAT4(NV_IGRAPH_XF_XFCTX_T2MAT))
        GLSL_DEFINE(texMat3, GLSL_C_MAT4(NV_IGRAPH_XF_XFCTX_T3MAT))

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
        "}\n");

    pgraph_get_glsl_vtx_header(header, state->vulkan, state->smooth_shading,
                             false, prefix_outputs, false);

    if (prefix_outputs) {
        mstring_append(header,
                       "#define vtx_inv_w v_vtx_inv_w\n"
                       "#define vtx_inv_w_flat v_vtx_inv_w_flat\n"
                       "#define vtxD0 v_vtxD0\n"
                       "#define vtxD1 v_vtxD1\n"
                       "#define vtxB0 v_vtxB0\n"
                       "#define vtxB1 v_vtxB1\n"
                       "#define vtxFog v_vtxFog\n"
                       "#define vtxT0 v_vtxT0\n"
                       "#define vtxT1 v_vtxT1\n"
                       "#define vtxT2 v_vtxT2\n"
                       "#define vtxT3 v_vtxT3\n"
                       );
    }
    mstring_append(header, "\n");

    int num_uniform_attrs = 0;

    for (i = 0; i < NV2A_VERTEXSHADER_ATTRIBUTES; i++) {
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

    for (i = 0; i < NV2A_VERTEXSHADER_ATTRIBUTES; i++) {
        if (state->compressed_attrs & (1 << i)) {
            mstring_append_fmt(
                body, "vec4 v%d = decompress_11_11_10(v%d_cmp);\n", i, i);
        }

        if (state->swizzle_attrs & (1 << i)) {
            mstring_append_fmt(body, "vec4 v%d = v%d_sw.bgra;\n", i, i);
        }

    }

    if (state->fixed_function) {
        pgraph_gen_vsh_ff_glsl(state, header, body, uniforms);
    } else if (state->vertex_program) {
        pgraph_gen_vsh_prog_glsl(VSH_VERSION_XVS,
                                 (uint32_t *)state->program_data,
                                 state->program_length, state->z_perspective,
                                 state->vulkan, header, body);
    } else {
        assert(false);
    }


    /* Fog */

    if (state->fog_enable) {

        if (state->vertex_program) {
            /* FIXME: Does foggen do something here? Let's do some tracking..
             *
             *   "RollerCoaster Tycoon" has
             *      state->vertex_program = true; state->foggen == FOGGEN_PLANAR
             *      but expects oFog.x as fogdistance?! Writes oFog.xyzw = v0.z
             */
            mstring_append(body, "  float fogDistance = oFog.x;\n");
        }

        /* FIXME: Do this per pixel? */

        switch (state->fog_mode) {
        case FOG_MODE_LINEAR:
        case FOG_MODE_LINEAR_ABS:

            /* f = (end - d) / (end - start)
             *    fogParam.y = -1 / (end - start)
             *    fogParam.x = 1 - end * fogParam.y;
             */

            mstring_append(body,
                "  if (isinf(fogDistance)) {\n"
                "    fogDistance = 0.0;\n"
                "  }\n"
            );
            mstring_append(body, "  float fogFactor = fogParam.x + fogDistance * fogParam.y;\n");
            mstring_append(body, "  fogFactor -= 1.0;\n");
            break;
        case FOG_MODE_EXP:
          mstring_append(body,
                         "  if (isinf(fogDistance)) {\n"
                         "    fogDistance = 0.0;\n"
                         "  }\n"
          );
          /* fallthru */
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
        /* Calculate absolute for the modes which need it */
        switch (state->fog_mode) {
        case FOG_MODE_LINEAR_ABS:
        case FOG_MODE_EXP_ABS:
        case FOG_MODE_EXP2_ABS:
            mstring_append(body, "  fogFactor = abs(fogFactor);\n");
            break;
        default:
            break;
        }

        mstring_append(body, "  oFog.xyzw = vec4(fogFactor);\n");
    } else {
        /* FIXME: Is the fog still calculated / passed somehow?!
         */
        mstring_append(body, "  oFog.xyzw = vec4(1.0);\n");
    }

    /* Set outputs */
    const char *shade_model_mult = state->smooth_shading ? "vtx_inv_w" : "vtx_inv_w_flat";
    mstring_append_fmt(body, "\n"
                      "  vtxD0 = clamp(oD0, 0.0, 1.0) * %s;\n"
                      "  vtxD1 = clamp(oD1, 0.0, 1.0) * %s;\n"
                      "  vtxB0 = clamp(oB0, 0.0, 1.0) * %s;\n"
                      "  vtxB1 = clamp(oB1, 0.0, 1.0) * %s;\n"
                      "  vtxFog = oFog.x * vtx_inv_w;\n"
                      "  vtxT0 = oT0 * vtx_inv_w;\n"
                      "  vtxT1 = oT1 * vtx_inv_w;\n"
                      "  vtxT2 = oT2 * vtx_inv_w;\n"
                      "  vtxT3 = oT3 * vtx_inv_w;\n"
                      "  gl_Position = oPos;\n"
                      "  gl_PointSize = oPts.x;\n"
                      // "  gl_ClipDistance[0] = oPos.z - oPos.w*clipRange.z;\n" // Near
                      // "  gl_ClipDistance[1] = oPos.w*clipRange.w - oPos.z;\n" // Far
                      "\n"
                      "}\n",
                       shade_model_mult,
                       shade_model_mult,
                       shade_model_mult,
                       shade_model_mult);

    /* Return combined header + source */
    if (state->vulkan) {
        // FIXME: Optimize uniforms
        if (num_uniform_attrs > 0) {
            if (state->use_push_constants_for_uniform_attrs) {
                mstring_append_fmt(output,
                    "layout(push_constant) uniform PushConstants {\n"
                    "    vec4 inlineValue[%d];\n"
                    "};\n\n", num_uniform_attrs);
            } else {
                mstring_append_fmt(uniforms, "    vec4 inlineValue[%d];\n",
                                   num_uniform_attrs);
            }
        }
        mstring_append_fmt(
            output,
            "layout(binding = %d, std140) uniform VshUniforms {\n"
            "%s"
            "};\n\n",
            VSH_UBO_BINDING, mstring_get_str(uniforms));
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
