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
#include "vsh-ff.h"

static void append_skinning_code(MString* str, bool mix,
                                 unsigned int count, const char* type,
                                 const char* output, const char* input,
                                 const char* matrix, const char* swizzle);

void pgraph_gen_vsh_ff_glsl(const ShaderState *state, MString *header,
                             MString *body, MString *uniforms)
{
    int i, j;
    const char *u = state->vulkan ? "" : "uniform "; // FIXME: Remove

    /* generate vertex shader mimicking fixed function */
    mstring_append(header,
"#define position      v0\n"
"#define weight        v1\n"
"#define normal        v2.xyz\n"
"#define diffuse       v3\n"
"#define specular      v4\n"
"#define fogCoord      v5.x\n"
"#define pointSize     v6\n"
"#define backDiffuse   v7\n"
"#define backSpecular  v8\n"
"#define texture0      v9\n"
"#define texture1      v10\n"
"#define texture2      v11\n"
"#define texture3      v12\n"
"#define reserved1     v13\n"
"#define reserved2     v14\n"
"#define reserved3     v15\n"
"\n");
    mstring_append_fmt(uniforms,
"%svec4 ltctxa[" stringify(NV2A_LTCTXA_COUNT) "];\n"
"%svec4 ltctxb[" stringify(NV2A_LTCTXB_COUNT) "];\n"
"%svec4 ltc1[" stringify(NV2A_LTC1_COUNT) "];\n", u, u, u
);
    mstring_append(header,
"\n"
GLSL_DEFINE(projectionMat, GLSL_C_MAT4(NV_IGRAPH_XF_XFCTX_PMAT0))
GLSL_DEFINE(compositeMat, GLSL_C_MAT4(NV_IGRAPH_XF_XFCTX_CMAT0))
"\n"
GLSL_DEFINE(texPlaneS0, GLSL_C(NV_IGRAPH_XF_XFCTX_TG0MAT + 0))
GLSL_DEFINE(texPlaneT0, GLSL_C(NV_IGRAPH_XF_XFCTX_TG0MAT + 1))
GLSL_DEFINE(texPlaneR0, GLSL_C(NV_IGRAPH_XF_XFCTX_TG0MAT + 2))
GLSL_DEFINE(texPlaneQ0, GLSL_C(NV_IGRAPH_XF_XFCTX_TG0MAT + 3))
"\n"
GLSL_DEFINE(texPlaneS1, GLSL_C(NV_IGRAPH_XF_XFCTX_TG1MAT + 0))
GLSL_DEFINE(texPlaneT1, GLSL_C(NV_IGRAPH_XF_XFCTX_TG1MAT + 1))
GLSL_DEFINE(texPlaneR1, GLSL_C(NV_IGRAPH_XF_XFCTX_TG1MAT + 2))
GLSL_DEFINE(texPlaneQ1, GLSL_C(NV_IGRAPH_XF_XFCTX_TG1MAT + 3))
"\n"
GLSL_DEFINE(texPlaneS2, GLSL_C(NV_IGRAPH_XF_XFCTX_TG2MAT + 0))
GLSL_DEFINE(texPlaneT2, GLSL_C(NV_IGRAPH_XF_XFCTX_TG2MAT + 1))
GLSL_DEFINE(texPlaneR2, GLSL_C(NV_IGRAPH_XF_XFCTX_TG2MAT + 2))
GLSL_DEFINE(texPlaneQ2, GLSL_C(NV_IGRAPH_XF_XFCTX_TG2MAT + 3))
"\n"
GLSL_DEFINE(texPlaneS3, GLSL_C(NV_IGRAPH_XF_XFCTX_TG3MAT + 0))
GLSL_DEFINE(texPlaneT3, GLSL_C(NV_IGRAPH_XF_XFCTX_TG3MAT + 1))
GLSL_DEFINE(texPlaneR3, GLSL_C(NV_IGRAPH_XF_XFCTX_TG3MAT + 2))
GLSL_DEFINE(texPlaneQ3, GLSL_C(NV_IGRAPH_XF_XFCTX_TG3MAT + 3))
"\n"
GLSL_DEFINE(modelViewMat0, GLSL_C_MAT4(NV_IGRAPH_XF_XFCTX_MMAT0))
GLSL_DEFINE(modelViewMat1, GLSL_C_MAT4(NV_IGRAPH_XF_XFCTX_MMAT1))
GLSL_DEFINE(modelViewMat2, GLSL_C_MAT4(NV_IGRAPH_XF_XFCTX_MMAT2))
GLSL_DEFINE(modelViewMat3, GLSL_C_MAT4(NV_IGRAPH_XF_XFCTX_MMAT3))
"\n"
GLSL_DEFINE(invModelViewMat0, GLSL_C_MAT4(NV_IGRAPH_XF_XFCTX_IMMAT0))
GLSL_DEFINE(invModelViewMat1, GLSL_C_MAT4(NV_IGRAPH_XF_XFCTX_IMMAT1))
GLSL_DEFINE(invModelViewMat2, GLSL_C_MAT4(NV_IGRAPH_XF_XFCTX_IMMAT2))
GLSL_DEFINE(invModelViewMat3, GLSL_C_MAT4(NV_IGRAPH_XF_XFCTX_IMMAT3))
"\n"
GLSL_DEFINE(eyePosition, GLSL_C(NV_IGRAPH_XF_XFCTX_EYEP))
"\n"
"#define lightAmbientColor(i) "
    "ltctxb[" stringify(NV_IGRAPH_XF_LTCTXB_L0_AMB) " + (i)*6].xyz\n"
"#define lightDiffuseColor(i) "
    "ltctxb[" stringify(NV_IGRAPH_XF_LTCTXB_L0_DIF) " + (i)*6].xyz\n"
"#define lightSpecularColor(i) "
    "ltctxb[" stringify(NV_IGRAPH_XF_LTCTXB_L0_SPC) " + (i)*6].xyz\n"
"\n"
"#define lightSpotFalloff(i) "
    "ltctxa[" stringify(NV_IGRAPH_XF_LTCTXA_L0_K) " + (i)*2].xyz\n"
"#define lightSpotDirection(i) "
    "ltctxa[" stringify(NV_IGRAPH_XF_LTCTXA_L0_SPT) " + (i)*2]\n"
"\n"
"#define lightLocalRange(i) "
    "ltc1[" stringify(NV_IGRAPH_XF_LTC1_r0) " + (i)].x\n"
"\n"
GLSL_DEFINE(sceneAmbientColor, GLSL_LTCTXA(NV_IGRAPH_XF_LTCTXA_FR_AMB) ".xyz")
GLSL_DEFINE(materialEmissionColor, GLSL_LTCTXA(NV_IGRAPH_XF_LTCTXA_CM_COL) ".xyz")
"\n"
);
    mstring_append_fmt(uniforms,
"%smat4 invViewport;\n", u);

    /* Skinning */
    unsigned int count;
    bool mix;
    switch (state->skinning) {
    case SKINNING_OFF:
        mix = false; count = 0; break;
    case SKINNING_1WEIGHTS:
        mix = true; count = 2; break;
    case SKINNING_2WEIGHTS2MATRICES:
        mix = false; count = 2; break;
    case SKINNING_2WEIGHTS:
        mix = true; count = 3; break;
    case SKINNING_3WEIGHTS3MATRICES:
        mix = false; count = 3; break;
    case SKINNING_3WEIGHTS:
        mix = true; count = 4; break;
    case SKINNING_4WEIGHTS4MATRICES:
        mix = false; count = 4; break;
    default:
        assert(false);
        break;
    }
    mstring_append_fmt(body, "/* Skinning mode %d */\n",
                       state->skinning);

    append_skinning_code(body, mix, count, "vec4",
                         "tPosition", "position",
                         "modelViewMat", "xyzw");
    append_skinning_code(body, mix, count, "vec3",
                         "tNormal", "vec4(normal, 0.0)",
                         "invModelViewMat", "xyz");

    /* Normalization */
    if (state->normalization) {
        mstring_append(body, "tNormal = normalize(tNormal);\n");
    }

    /* Texgen */
    for (i = 0; i < NV2A_MAX_TEXTURES; i++) {
        mstring_append_fmt(body, "/* Texgen for stage %d */\n",
                           i);
        /* Set each component individually */
        /* FIXME: could be nicer if some channels share the same texgen */
        for (j = 0; j < 4; j++) {
            /* TODO: TexGen View Model missing! */
            char c = "xyzw"[j];
            char cSuffix = "STRQ"[j];
            switch (state->texgen[i][j]) {
            case TEXGEN_DISABLE:
                mstring_append_fmt(body, "oT%d.%c = texture%d.%c;\n",
                                   i, c, i, c);
                break;
            case TEXGEN_EYE_LINEAR:
                mstring_append_fmt(body, "oT%d.%c = dot(texPlane%c%d, tPosition);\n",
                                   i, c, cSuffix, i);
                break;
            case TEXGEN_OBJECT_LINEAR:
                mstring_append_fmt(body, "oT%d.%c = dot(texPlane%c%d, position);\n",
                                   i, c, cSuffix, i);
                break;
            case TEXGEN_SPHERE_MAP:
                assert(j < 2);  /* Channels S,T only! */
                mstring_append(body, "{\n");
                /* FIXME: u, r and m only have to be calculated once */
                mstring_append(body, "  vec3 u = normalize(tPosition.xyz);\n");
                //FIXME: tNormal before or after normalization? Always normalize?
                mstring_append(body, "  vec3 r = reflect(u, tNormal);\n");

                /* FIXME: This would consume 1 division fewer and *might* be
                 *        faster than length:
                 *   // [z=1/(2*x) => z=1/x*0.5]
                 *   vec3 ro = r + vec3(0.0, 0.0, 1.0);
                 *   float m = inversesqrt(dot(ro,ro))*0.5;
                 */

                mstring_append(body, "  float invM = 1.0 / (2.0 * length(r + vec3(0.0, 0.0, 1.0)));\n");
                mstring_append_fmt(body, "  oT%d.%c = r.%c * invM + 0.5;\n",
                                   i, c, c);
                mstring_append(body, "}\n");
                break;
            case TEXGEN_REFLECTION_MAP:
                assert(j < 3); /* Channels S,T,R only! */
                mstring_append(body, "{\n");
                /* FIXME: u and r only have to be calculated once, can share the one from SPHERE_MAP */
                mstring_append(body, "  vec3 u = normalize(tPosition.xyz);\n");
                mstring_append(body, "  vec3 r = reflect(u, tNormal);\n");
                mstring_append_fmt(body, "  oT%d.%c = r.%c;\n",
                                   i, c, c);
                mstring_append(body, "}\n");
                break;
            case TEXGEN_NORMAL_MAP:
                assert(j < 3); /* Channels S,T,R only! */
                mstring_append_fmt(body, "oT%d.%c = tNormal.%c;\n",
                                   i, c, c);
                break;
            default:
                assert(false);
                break;
            }
        }
    }

    /* Apply texture matrices */
    for (i = 0; i < NV2A_MAX_TEXTURES; i++) {
        if (state->texture_matrix_enable[i]) {
            mstring_append_fmt(body,
                               "oT%d = oT%d * texMat%d;\n",
                               i, i, i);
        }
    }

    /* Lighting */
    if (state->lighting) {

        //FIXME: Do 2 passes if we want 2 sided-lighting?

        static char alpha_source_diffuse[] = "diffuse.a";
        static char alpha_source_specular[] = "specular.a";
        static char alpha_source_material[] = "material_alpha";
        const char *alpha_source = alpha_source_diffuse;
        if (state->diffuse_src == MATERIAL_COLOR_SRC_MATERIAL) {
            mstring_append_fmt(uniforms, "%sfloat material_alpha;\n", u);
            alpha_source = alpha_source_material;
        } else if (state->diffuse_src == MATERIAL_COLOR_SRC_SPECULAR) {
            alpha_source = alpha_source_specular;
        }

        if (state->ambient_src == MATERIAL_COLOR_SRC_MATERIAL) {
            mstring_append_fmt(body, "oD0 = vec4(sceneAmbientColor, %s);\n", alpha_source);
        } else if (state->ambient_src == MATERIAL_COLOR_SRC_DIFFUSE) {
            mstring_append_fmt(body, "oD0 = vec4(diffuse.rgb, %s);\n", alpha_source);
        } else if (state->ambient_src == MATERIAL_COLOR_SRC_SPECULAR) {
            mstring_append_fmt(body, "oD0 = vec4(specular.rgb, %s);\n", alpha_source);
        }

        mstring_append(body, "oD0.rgb *= materialEmissionColor.rgb;\n");
        if (state->emission_src == MATERIAL_COLOR_SRC_MATERIAL) {
            mstring_append(body, "oD0.rgb += sceneAmbientColor;\n");
        } else if (state->emission_src == MATERIAL_COLOR_SRC_DIFFUSE) {
            mstring_append(body, "oD0.rgb += diffuse.rgb;\n");
        } else if (state->emission_src == MATERIAL_COLOR_SRC_SPECULAR) {
            mstring_append(body, "oD0.rgb += specular.rgb;\n");
        }

        mstring_append(body, "oD1 = vec4(0.0, 0.0, 0.0, specular.a);\n");

        for (i = 0; i < NV2A_MAX_LIGHTS; i++) {
            if (state->light[i] == LIGHT_OFF) {
                continue;
            }

            /* FIXME: It seems that we only have to handle the surface colors if
             *        they are not part of the material [= vertex colors].
             *        If they are material the cpu will premultiply light
             *        colors
             */

            mstring_append_fmt(body, "/* Light %d */ {\n", i);

            if (state->light[i] == LIGHT_LOCAL
                    || state->light[i] == LIGHT_SPOT) {

                mstring_append_fmt(uniforms,
                    "%svec3 lightLocalPosition%d;\n"
                    "%svec3 lightLocalAttenuation%d;\n",
                    u, i, u, i);
                mstring_append_fmt(body,
                    "  vec3 VP = lightLocalPosition%d - tPosition.xyz/tPosition.w;\n"
                    "  float d = length(VP);\n"
//FIXME: if (d > lightLocalRange) { .. don't process this light .. } /* inclusive?! */ - what about directional lights?
                    "  VP = normalize(VP);\n"
                    "  float attenuation = 1.0 / (lightLocalAttenuation%d.x\n"
                    "                               + lightLocalAttenuation%d.y * d\n"
                    "                               + lightLocalAttenuation%d.z * d * d);\n"
                    "  vec3 halfVector = normalize(VP + eyePosition.xyz / eyePosition.w);\n" /* FIXME: Not sure if eyePosition is correct */
                    "  float nDotVP = max(0.0, dot(tNormal, VP));\n"
                    "  float nDotHV = max(0.0, dot(tNormal, halfVector));\n",
                    i, i, i, i);

            }

            switch(state->light[i]) {
            case LIGHT_INFINITE:

                /* lightLocalRange will be 1e+30 here */

                mstring_append_fmt(uniforms,
                    "%svec3 lightInfiniteHalfVector%d;\n"
                    "%svec3 lightInfiniteDirection%d;\n",
                    u, i, u, i);
                mstring_append_fmt(body,
                    "  float attenuation = 1.0;\n"
                    "  float nDotVP = max(0.0, dot(tNormal, normalize(vec3(lightInfiniteDirection%d))));\n"
                    "  float nDotHV = max(0.0, dot(tNormal, vec3(lightInfiniteHalfVector%d)));\n",
                    i, i);

                /* FIXME: Do specular */

                /* FIXME: tBackDiffuse */

                break;
            case LIGHT_LOCAL:
                /* Everything done already */
                break;
            case LIGHT_SPOT:
                /* https://docs.microsoft.com/en-us/windows/win32/direct3d9/attenuation-and-spotlight-factor#spotlight-factor */
                mstring_append_fmt(body,
                    "  vec4 spotDir = lightSpotDirection(%d);\n"
                    "  float invScale = 1/length(spotDir.xyz);\n"
                    "  float cosHalfPhi = -invScale*spotDir.w;\n"
                    "  float cosHalfTheta = invScale + cosHalfPhi;\n"
                    "  float spotDirDotVP = dot(spotDir.xyz, VP);\n"
                    "  float rho = invScale*spotDirDotVP;\n"
                    "  if (rho > cosHalfTheta) {\n"
                    "  } else if (rho <= cosHalfPhi) {\n"
                    "    attenuation = 0.0;\n"
                    "  } else {\n"
                    "    attenuation *= spotDirDotVP + spotDir.w;\n" /* FIXME: lightSpotFalloff */
                    "  }\n",
                    i);
                break;
            default:
                assert(false);
                break;
            }

            mstring_append_fmt(body,
                "  float pf;\n"
                "  if (nDotVP == 0.0) {\n"
                "    pf = 0.0;\n"
                "  } else {\n"
                "    pf = pow(nDotHV, /* specular(l, m, n, l1, m1, n1) */ 0.001);\n"
                "  }\n"
                "  vec3 lightAmbient = lightAmbientColor(%d) * attenuation;\n"
                "  vec3 lightDiffuse = lightDiffuseColor(%d) * attenuation * nDotVP;\n"
                "  vec3 lightSpecular = lightSpecularColor(%d) * pf;\n",
                i, i, i);

            mstring_append(body,
                "  oD0.xyz += lightAmbient;\n");

            switch (state->diffuse_src) {
            case MATERIAL_COLOR_SRC_MATERIAL:
                mstring_append(body,
                               "  oD0.xyz += lightDiffuse;\n");
                break;
            case MATERIAL_COLOR_SRC_DIFFUSE:
                mstring_append(body,
                               "  oD0.xyz += diffuse.xyz * lightDiffuse;\n");
                break;
            case MATERIAL_COLOR_SRC_SPECULAR:
                mstring_append(body,
                               "  oD0.xyz += specular.xyz * lightDiffuse;\n");
                break;
            }

            mstring_append(body,
                "  oD1.xyz += specular.xyz * lightSpecular;\n");

            mstring_append(body, "}\n");
        }
    } else {
        mstring_append(body, "  oD0 = diffuse;\n");
        mstring_append(body, "  oD1 = specular;\n");
    }
    mstring_append(body, "  oB0 = backDiffuse;\n");
    mstring_append(body, "  oB1 = backSpecular;\n");

    /* Fog */
    if (state->fog_enable) {

        /* From: https://www.opengl.org/registry/specs/NV/fog_distance.txt */
        switch(state->foggen) {
        case FOGGEN_SPEC_ALPHA:
            /* FIXME: Do we have to clamp here? */
            mstring_append(body, "  float fogDistance = clamp(specular.a, 0.0, 1.0);\n");
            break;
        case FOGGEN_RADIAL:
            mstring_append(body, "  float fogDistance = length(tPosition.xyz);\n");
            break;
        case FOGGEN_PLANAR:
        case FOGGEN_ABS_PLANAR:
            mstring_append(body, "  float fogDistance = dot(fogPlane.xyz, tPosition.xyz) + fogPlane.w;\n");
            if (state->foggen == FOGGEN_ABS_PLANAR) {
                mstring_append(body, "  fogDistance = abs(fogDistance);\n");
            }
            break;
        case FOGGEN_FOG_X:
            mstring_append(body, "  float fogDistance = fogCoord;\n");
            break;
        default:
            assert(false);
            break;
        }

    }

    /* If skinning is off the composite matrix already includes the MV matrix */
    if (state->skinning == SKINNING_OFF) {
        mstring_append(body, "  tPosition = position;\n");
    }

    mstring_append(body,
    "   oPos = invViewport * (tPosition * compositeMat);\n"
    );

    if (state->vulkan) {
        mstring_append(body, "   oPos.y *= -1;\n");
    } else {
        mstring_append(body, "   oPos.z = oPos.z * 2.0 - oPos.w;\n");
    }

    /* FIXME: Testing */
    if (state->point_params_enable) {
        mstring_append_fmt(
            body,
            "  float d_e = length(position * modelViewMat0);\n"
            "  oPts.x = 1/sqrt(%f + %f*d_e + %f*d_e*d_e) + %f;\n",
            state->point_params[0], state->point_params[1], state->point_params[2],
            state->point_params[6]);
        mstring_append_fmt(body, "  oPts.x = min(oPts.x*%f + %f, 64.0) * %d;\n",
                           state->point_params[3], state->point_params[7],
                           state->surface_scale_factor);
    } else {
        mstring_append_fmt(body, "  oPts.x = %f * %d;\n", state->point_size,
                           state->surface_scale_factor);
    }

    mstring_append(body,
                   "  if (oPos.w == 0.0 || isinf(oPos.w)) {\n"
                   "    vtx_inv_w = 1.0;\n"
                   "  } else {\n"
                   "    vtx_inv_w = 1.0 / oPos.w;\n"
                   "  }\n"
                   "  vtx_inv_w_flat = vtx_inv_w;\n");
}

static void append_skinning_code(MString* str, bool mix,
                                 unsigned int count, const char* type,
                                 const char* output, const char* input,
                                 const char* matrix, const char* swizzle)
{
    if (count == 0) {
        mstring_append_fmt(str, "%s %s = (%s * %s0).%s;\n",
                           type, output, input, matrix, swizzle);
    } else {
        mstring_append_fmt(str, "%s %s = %s(0.0);\n", type, output, type);
        if (mix) {
            /* Generated final weight (like GL_WEIGHT_SUM_UNITY_ARB) */
            mstring_append(str, "{\n"
                                "  float weight_i;\n"
                                "  float weight_n = 1.0;\n");
            int i;
            for (i = 0; i < count; i++) {
                if (i < (count - 1)) {
                    char c = "xyzw"[i];
                    mstring_append_fmt(str, "  weight_i = weight.%c;\n"
                                            "  weight_n -= weight_i;\n",
                                       c);
                } else {
                    mstring_append(str, "  weight_i = weight_n;\n");
                }
                mstring_append_fmt(str, "  %s += (%s * %s%d).%s * weight_i;\n",
                                   output, input, matrix, i, swizzle);
            }
            mstring_append(str, "}\n");
        } else {
            /* Individual weights */
            int i;
            for (i = 0; i < count; i++) {
                char c = "xyzw"[i];
                mstring_append_fmt(str, "%s += (%s * %s%d).%s * weight.%c;\n",
                                   output, input, matrix, i, swizzle, c);
            }
        }
    }
}
