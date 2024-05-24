/*
 * QEMU Geforce NV2A shader generator
 *
 * Copyright (c) 2015 espes
 * Copyright (c) 2015 Jannik Vogel
 * Copyright (c) 2020-2021 Matt Borgerson
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
#include <locale.h>

#include "shaders_common.h"
#include "shaders.h"
#include "nv2a_int.h"
#include "ui/xemu-settings.h"
#include "xemu-version.h"

void mstring_append_fmt(MString *qstring, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    mstring_append_va(qstring, fmt, ap);
    va_end(ap);
}

MString *mstring_from_fmt(const char *fmt, ...)
{
    MString *ret = mstring_new();
    va_list ap;
    va_start(ap, fmt);
    mstring_append_va(ret, fmt, ap);
    va_end(ap);

    return ret;
}

void mstring_append_va(MString *qstring, const char *fmt, va_list va)
{
    char scratch[256];

    va_list ap;
    va_copy(ap, va);
    const int len = vsnprintf(scratch, sizeof(scratch), fmt, ap);
    va_end(ap);

    if (len == 0) {
        return;
    } else if (len < sizeof(scratch)) {
        mstring_append(qstring, scratch);
        return;
    }

    /* overflowed out scratch buffer, alloc and try again */
    char *buf = g_malloc(len + 1);
    va_copy(ap, va);
    vsnprintf(buf, len + 1, fmt, ap);
    va_end(ap);

    mstring_append(qstring, buf);
    g_free(buf);
}

GLenum get_gl_primitive_mode(enum ShaderPolygonMode polygon_mode, enum ShaderPrimitiveMode primitive_mode)
{
    if (polygon_mode == POLY_MODE_POINT) {
        return GL_POINTS;
    }

    switch (primitive_mode) {
    case PRIM_TYPE_POINTS: return GL_POINTS;
    case PRIM_TYPE_LINES: return GL_LINES;
    case PRIM_TYPE_LINE_LOOP: return GL_LINE_LOOP;
    case PRIM_TYPE_LINE_STRIP: return GL_LINE_STRIP;
    case PRIM_TYPE_TRIANGLES: return GL_TRIANGLES;
    case PRIM_TYPE_TRIANGLE_STRIP: return GL_TRIANGLE_STRIP;
    case PRIM_TYPE_TRIANGLE_FAN: return GL_TRIANGLE_FAN;
    case PRIM_TYPE_QUADS: return GL_LINES_ADJACENCY;
    case PRIM_TYPE_QUAD_STRIP: return GL_LINE_STRIP_ADJACENCY;
    case PRIM_TYPE_POLYGON:
        if (polygon_mode == POLY_MODE_LINE) {
            return GL_LINE_LOOP;
        } else if (polygon_mode == POLY_MODE_FILL) {
            return GL_TRIANGLE_FAN;
        }

        assert(!"PRIM_TYPE_POLYGON with invalid polygon_mode");
        return 0;
    default:
        assert(!"Invalid primitive_mode");
        return 0;
    }
}

static MString* generate_geometry_shader(
                                      enum ShaderPolygonMode polygon_front_mode,
                                      enum ShaderPolygonMode polygon_back_mode,
                                      enum ShaderPrimitiveMode primitive_mode,
                                      GLenum *gl_primitive_mode,
                                      bool smooth_shading)
{
    /* FIXME: Missing support for 2-sided-poly mode */
    assert(polygon_front_mode == polygon_back_mode);
    enum ShaderPolygonMode polygon_mode = polygon_front_mode;

    *gl_primitive_mode = get_gl_primitive_mode(polygon_mode, primitive_mode);

    /* POINT mode shouldn't require any special work */
    if (polygon_mode == POLY_MODE_POINT) {
        return NULL;
    }

    /* Handle LINE and FILL mode */
    const char *layout_in = NULL;
    const char *layout_out = NULL;
    const char *body = NULL;
    switch (primitive_mode) {
    case PRIM_TYPE_POINTS: return NULL;
    case PRIM_TYPE_LINES: return NULL;
    case PRIM_TYPE_LINE_LOOP: return NULL;
    case PRIM_TYPE_LINE_STRIP: return NULL;
    case PRIM_TYPE_TRIANGLES:
        if (polygon_mode == POLY_MODE_FILL) { return NULL; }
        assert(polygon_mode == POLY_MODE_LINE);
        layout_in = "layout(triangles) in;\n";
        layout_out = "layout(line_strip, max_vertices = 4) out;\n";
        body = "  emit_vertex(0, 0);\n"
               "  emit_vertex(1, 0);\n"
               "  emit_vertex(2, 0);\n"
               "  emit_vertex(0, 0);\n"
               "  EndPrimitive();\n";
        break;
    case PRIM_TYPE_TRIANGLE_STRIP:
        if (polygon_mode == POLY_MODE_FILL) { return NULL; }
        assert(polygon_mode == POLY_MODE_LINE);
        layout_in = "layout(triangles) in;\n";
        layout_out = "layout(line_strip, max_vertices = 4) out;\n";
        /* Imagine a quad made of a tristrip, the comments tell you which
         * vertex we are using */
        body = "  if ((gl_PrimitiveIDIn & 1) == 0) {\n"
               "    if (gl_PrimitiveIDIn == 0) {\n"
               "      emit_vertex(0, 0);\n" /* bottom right */
               "    }\n"
               "    emit_vertex(1, 0);\n" /* top right */
               "    emit_vertex(2, 0);\n" /* bottom left */
               "    emit_vertex(0, 0);\n" /* bottom right */
               "  } else {\n"
               "    emit_vertex(2, 0);\n" /* bottom left */
               "    emit_vertex(1, 0);\n" /* top left */
               "    emit_vertex(0, 0);\n" /* top right */
               "  }\n"
               "  EndPrimitive();\n";
        break;
    case PRIM_TYPE_TRIANGLE_FAN:
        if (polygon_mode == POLY_MODE_FILL) { return NULL; }
        assert(polygon_mode == POLY_MODE_LINE);
        layout_in = "layout(triangles) in;\n";
        layout_out = "layout(line_strip, max_vertices = 4) out;\n";
        body = "  if (gl_PrimitiveIDIn == 0) {\n"
               "    emit_vertex(0, 0);\n"
               "  }\n"
               "  emit_vertex(1, 0);\n"
               "  emit_vertex(2, 0);\n"
               "  emit_vertex(0, 0);\n"
               "  EndPrimitive();\n";
        break;
    case PRIM_TYPE_QUADS:
        layout_in = "layout(lines_adjacency) in;\n";
        if (polygon_mode == POLY_MODE_LINE) {
            layout_out = "layout(line_strip, max_vertices = 5) out;\n";
            body = "  emit_vertex(0, 3);\n"
                   "  emit_vertex(1, 3);\n"
                   "  emit_vertex(2, 3);\n"
                   "  emit_vertex(3, 3);\n"
                   "  emit_vertex(0, 3);\n"
                   "  EndPrimitive();\n";
        } else if (polygon_mode == POLY_MODE_FILL) {
            layout_out = "layout(triangle_strip, max_vertices = 4) out;\n";
            body = "  emit_vertex(3, 3);\n"
                   "  emit_vertex(0, 3);\n"
                   "  emit_vertex(2, 3);\n"
                   "  emit_vertex(1, 3);\n"
                   "  EndPrimitive();\n";
        } else {
            assert(false);
            return NULL;
        }
        break;
    case PRIM_TYPE_QUAD_STRIP:
        layout_in = "layout(lines_adjacency) in;\n";
        if (polygon_mode == POLY_MODE_LINE) {
            layout_out = "layout(line_strip, max_vertices = 5) out;\n";
            body = "  if ((gl_PrimitiveIDIn & 1) != 0) { return; }\n"
                   "  if (gl_PrimitiveIDIn == 0) {\n"
                   "    emit_vertex(0, 3);\n"
                   "  }\n"
                   "  emit_vertex(1, 3);\n"
                   "  emit_vertex(3, 3);\n"
                   "  emit_vertex(2, 3);\n"
                   "  emit_vertex(0, 3);\n"
                   "  EndPrimitive();\n";
        } else if (polygon_mode == POLY_MODE_FILL) {
            layout_out = "layout(triangle_strip, max_vertices = 4) out;\n";
            body = "  if ((gl_PrimitiveIDIn & 1) != 0) { return; }\n"
                   "  emit_vertex(0, 3);\n"
                   "  emit_vertex(1, 3);\n"
                   "  emit_vertex(2, 3);\n"
                   "  emit_vertex(3, 3);\n"
                   "  EndPrimitive();\n";
        } else {
            assert(false);
            return NULL;
        }
        break;
    case PRIM_TYPE_POLYGON:
        if (polygon_mode == POLY_MODE_LINE) {
            return NULL;
        }
        if (polygon_mode == POLY_MODE_FILL) {
            if (smooth_shading) {
                return NULL;
            }
            layout_in = "layout(triangles) in;\n";
            layout_out = "layout(triangle_strip, max_vertices = 3) out;\n";
            body = "  emit_vertex(0, 2);\n"
                   "  emit_vertex(1, 2);\n"
                   "  emit_vertex(2, 2);\n"
                   "  EndPrimitive();\n";
        } else {
            assert(false);
            return NULL;
        }
        break;

    default:
        assert(false);
        return NULL;
    }

    /* generate a geometry shader to support deprecated primitive types */
    assert(layout_in);
    assert(layout_out);
    assert(body);
    MString* s = mstring_from_str("#version 330\n"
                                  "\n");
    mstring_append(s, layout_in);
    mstring_append(s, layout_out);
    mstring_append(s, "\n");
    if (smooth_shading) {
        mstring_append(s,
                       STRUCT_V_VERTEX_DATA_IN_ARRAY_SMOOTH
                       "\n"
                       STRUCT_VERTEX_DATA_OUT_SMOOTH
                       "\n"
                       "void emit_vertex(int index, int _unused) {\n"
                       "  gl_Position = gl_in[index].gl_Position;\n"
                       "  gl_PointSize = gl_in[index].gl_PointSize;\n"
                       "  gl_ClipDistance[0] = gl_in[index].gl_ClipDistance[0];\n"
                       "  gl_ClipDistance[1] = gl_in[index].gl_ClipDistance[1];\n"
                       "  vtx_inv_w = v_vtx_inv_w[index];\n"
                       "  vtx_inv_w_flat = v_vtx_inv_w[index];\n"
                       "  vtxD0 = v_vtxD0[index];\n"
                       "  vtxD1 = v_vtxD1[index];\n"
                       "  vtxB0 = v_vtxB0[index];\n"
                       "  vtxB1 = v_vtxB1[index];\n"
                       "  vtxFog = v_vtxFog[index];\n"
                       "  vtxT0 = v_vtxT0[index];\n"
                       "  vtxT1 = v_vtxT1[index];\n"
                       "  vtxT2 = v_vtxT2[index];\n"
                       "  vtxT3 = v_vtxT3[index];\n"
                       "  EmitVertex();\n"
                       "}\n");
    } else {
        mstring_append(s,
                       STRUCT_V_VERTEX_DATA_IN_ARRAY_FLAT
                       "\n"
                       STRUCT_VERTEX_DATA_OUT_FLAT
                       "\n"
                       "void emit_vertex(int index, int provoking_index) {\n"
                       "  gl_Position = gl_in[index].gl_Position;\n"
                       "  gl_PointSize = gl_in[index].gl_PointSize;\n"
                       "  gl_ClipDistance[0] = gl_in[index].gl_ClipDistance[0];\n"
                       "  gl_ClipDistance[1] = gl_in[index].gl_ClipDistance[1];\n"
                       "  vtx_inv_w = v_vtx_inv_w[index];\n"
                       "  vtx_inv_w_flat = v_vtx_inv_w[provoking_index];\n"
                       "  vtxD0 = v_vtxD0[provoking_index];\n"
                       "  vtxD1 = v_vtxD1[provoking_index];\n"
                       "  vtxB0 = v_vtxB0[provoking_index];\n"
                       "  vtxB1 = v_vtxB1[provoking_index];\n"
                       "  vtxFog = v_vtxFog[index];\n"
                       "  vtxT0 = v_vtxT0[index];\n"
                       "  vtxT1 = v_vtxT1[index];\n"
                       "  vtxT2 = v_vtxT2[index];\n"
                       "  vtxT3 = v_vtxT3[index];\n"
                       "  EmitVertex();\n"
                       "}\n");
    }

    mstring_append(s, "\n"
                      "void main() {\n");
    mstring_append(s, body);
    mstring_append(s, "}\n");

    return s;
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

#define GLSL_C(idx) "c[" stringify(idx) "]"
#define GLSL_LTCTXA(idx) "ltctxa[" stringify(idx) "]"

#define GLSL_C_MAT4(idx) \
    "mat4(" GLSL_C(idx) ", " GLSL_C(idx+1) ", " \
            GLSL_C(idx+2) ", " GLSL_C(idx+3) ")"

#define GLSL_DEFINE(a, b) "#define " stringify(a) " " b "\n"

static void generate_fixed_function(const ShaderState *state,
                                    MString *header, MString *body)
{
    int i, j;

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
"\n"
"uniform vec4 ltctxa[" stringify(NV2A_LTCTXA_COUNT) "];\n"
"uniform vec4 ltctxb[" stringify(NV2A_LTCTXB_COUNT) "];\n"
"uniform vec4 ltc1[" stringify(NV2A_LTC1_COUNT) "];\n"
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
"uniform mat4 invViewport;\n"
"\n");

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
            mstring_append(header, "uniform float material_alpha;\n");
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

                mstring_append_fmt(header,
                    "uniform vec3 lightLocalPosition%d;\n"
                    "uniform vec3 lightLocalAttenuation%d;\n",
                    i, i);
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

                mstring_append_fmt(header,
                    "uniform vec3 lightInfiniteHalfVector%d;\n"
                    "uniform vec3 lightInfiniteDirection%d;\n",
                    i, i);
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
    "   oPos.z = oPos.z * 2.0 - oPos.w;\n");

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

static MString *generate_vertex_shader(const ShaderState *state,
                                       bool prefix_outputs)
{
    int i;
    MString *header = mstring_from_str(
"#version 400\n"
"\n"
"uniform vec4 clipRange;\n"
"uniform vec2 surfaceSize;\n"
"\n"
/* All constants in 1 array declaration */
"uniform vec4 c[" stringify(NV2A_VERTEXSHADER_CONSTANTS) "];\n"
"\n"
"uniform vec4 fogColor;\n"
"uniform float fogParam[2];\n"
"\n"

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
    if (prefix_outputs) {
        mstring_append(header, state->smooth_shading ?
                                   STRUCT_V_VERTEX_DATA_OUT_SMOOTH :
                                   STRUCT_V_VERTEX_DATA_OUT_FLAT);
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
    } else {
        mstring_append(header, state->smooth_shading ?
                                   STRUCT_VERTEX_DATA_OUT_SMOOTH :
                                   STRUCT_VERTEX_DATA_OUT_FLAT);
    }
    mstring_append(header, "\n");
    for (i = 0; i < NV2A_VERTEXSHADER_ATTRIBUTES; i++) {
        if (state->compressed_attrs & (1 << i)) {
            mstring_append_fmt(header,
                               "layout(location = %d) in int v%d_cmp;\n", i, i);
        } else {
            mstring_append_fmt(header, "layout(location = %d) in vec4 v%d;\n",
                               i, i);
        }
    }
    mstring_append(header, "\n");

    MString *body = mstring_from_str("void main() {\n");

    for (i = 0; i < NV2A_VERTEXSHADER_ATTRIBUTES; i++) {
        if (state->compressed_attrs & (1 << i)) {
            mstring_append_fmt(
                body, "vec4 v%d = decompress_11_11_10(v%d_cmp);\n", i, i);
        }
    }

    if (state->fixed_function) {
        generate_fixed_function(state, header, body);
    } else if (state->vertex_program) {
        vsh_translate(VSH_VERSION_XVS,
                      (uint32_t*)state->program_data,
                      state->program_length,
                      state->z_perspective,
                      header, body);
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
             *    fogParam[1] = -1 / (end - start)
             *    fogParam[0] = 1 - end * fogParam[1];
             */

            mstring_append(body,
                "  if (isinf(fogDistance)) {\n"
                "    fogDistance = 0.0;\n"
                "  }\n"
            );
            mstring_append(body, "  float fogFactor = fogParam[0] + fogDistance * fogParam[1];\n");
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
             *    fogParam[1] = -density / (2 * ln(256))
             *    fogParam[0] = 1.5
             */

            mstring_append(body, "  float fogFactor = fogParam[0] + exp2(fogDistance * fogParam[1] * 16.0);\n");
            mstring_append(body, "  fogFactor -= 1.5;\n");
            break;
        case FOG_MODE_EXP2:
        case FOG_MODE_EXP2_ABS:

            /* f = 1 / (e^((d * density)^2))
             *    fogParam[1] = -density / (2 * sqrt(ln(256)))
             *    fogParam[0] = 1.5
             */

            mstring_append(body, "  float fogFactor = fogParam[0] + exp2(-fogDistance * fogDistance * fogParam[1] * fogParam[1] * 32.0);\n");
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
                      "  gl_ClipDistance[0] = oPos.z - oPos.w*clipRange.z;\n" // Near
                      "  gl_ClipDistance[1] = oPos.w*clipRange.w - oPos.z;\n" // Far
                      "\n"
                      "}\n",
                       shade_model_mult,
                       shade_model_mult,
                       shade_model_mult,
                       shade_model_mult);


    /* Return combined header + source */
    mstring_append(header, mstring_get_str(body));
    mstring_unref(body);
    return header;

}

static GLuint create_gl_shader(GLenum gl_shader_type,
                               const char *code,
                               const char *name)
{
    GLint compiled = 0;

    NV2A_GL_DGROUP_BEGIN("Creating new %s", name);

    NV2A_DPRINTF("compile new %s, code:\n%s\n", name, code);

    GLuint shader = glCreateShader(gl_shader_type);
    glShaderSource(shader, 1, &code, 0);
    glCompileShader(shader);

    /* Check it compiled */
    compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLchar* log;
        GLint log_length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
        log = g_malloc(log_length * sizeof(GLchar));
        glGetShaderInfoLog(shader, log_length, NULL, log);
        fprintf(stderr, "%s\n\n" "nv2a: %s compilation failed: %s\n", code, name, log);
        g_free(log);

        NV2A_GL_DGROUP_END();
        abort();
    }

    NV2A_GL_DGROUP_END();

    return shader;
}

void update_shader_constant_locations(ShaderBinding *binding, const ShaderState *state)
{
    int i, j;
    char tmp[64];

    /* set texture samplers */
    for (i = 0; i < NV2A_MAX_TEXTURES; i++) {
        char samplerName[16];
        snprintf(samplerName, sizeof(samplerName), "texSamp%d", i);
        GLint texSampLoc = glGetUniformLocation(binding->gl_program, samplerName);
        if (texSampLoc >= 0) {
            glUniform1i(texSampLoc, i);
        }
    }

    /* validate the program */
    glValidateProgram(binding->gl_program);
    GLint valid = 0;
    glGetProgramiv(binding->gl_program, GL_VALIDATE_STATUS, &valid);
    if (!valid) {
        GLchar log[1024];
        glGetProgramInfoLog(binding->gl_program, 1024, NULL, log);
        fprintf(stderr, "nv2a: shader validation failed: %s\n", log);
        abort();
    }

    /* lookup fragment shader uniforms */
    for (i = 0; i < 9; i++) {
        for (j = 0; j < 2; j++) {
            snprintf(tmp, sizeof(tmp), "c%d_%d", j, i);
            binding->psh_constant_loc[i][j] = glGetUniformLocation(binding->gl_program, tmp);
        }
    }
    binding->alpha_ref_loc = glGetUniformLocation(binding->gl_program, "alphaRef");
    for (i = 1; i < NV2A_MAX_TEXTURES; i++) {
        snprintf(tmp, sizeof(tmp), "bumpMat%d", i);
        binding->bump_mat_loc[i] = glGetUniformLocation(binding->gl_program, tmp);
        snprintf(tmp, sizeof(tmp), "bumpScale%d", i);
        binding->bump_scale_loc[i] = glGetUniformLocation(binding->gl_program, tmp);
        snprintf(tmp, sizeof(tmp), "bumpOffset%d", i);
        binding->bump_offset_loc[i] = glGetUniformLocation(binding->gl_program, tmp);
    }

    for (int i = 0; i < NV2A_MAX_TEXTURES; i++) {
        snprintf(tmp, sizeof(tmp), "texScale%d", i);
        binding->tex_scale_loc[i] = glGetUniformLocation(binding->gl_program, tmp);
    }

    /* lookup vertex shader uniforms */
    for(i = 0; i < NV2A_VERTEXSHADER_CONSTANTS; i++) {
        snprintf(tmp, sizeof(tmp), "c[%d]", i);
        binding->vsh_constant_loc[i] = glGetUniformLocation(binding->gl_program, tmp);
    }
    binding->surface_size_loc = glGetUniformLocation(binding->gl_program, "surfaceSize");
    binding->clip_range_loc = glGetUniformLocation(binding->gl_program, "clipRange");
    binding->fog_color_loc = glGetUniformLocation(binding->gl_program, "fogColor");
    binding->fog_param_loc[0] = glGetUniformLocation(binding->gl_program, "fogParam[0]");
    binding->fog_param_loc[1] = glGetUniformLocation(binding->gl_program, "fogParam[1]");

    binding->inv_viewport_loc = glGetUniformLocation(binding->gl_program, "invViewport");
    for (i = 0; i < NV2A_LTCTXA_COUNT; i++) {
        snprintf(tmp, sizeof(tmp), "ltctxa[%d]", i);
        binding->ltctxa_loc[i] = glGetUniformLocation(binding->gl_program, tmp);
    }
    for (i = 0; i < NV2A_LTCTXB_COUNT; i++) {
        snprintf(tmp, sizeof(tmp), "ltctxb[%d]", i);
        binding->ltctxb_loc[i] = glGetUniformLocation(binding->gl_program, tmp);
    }
    for (i = 0; i < NV2A_LTC1_COUNT; i++) {
        snprintf(tmp, sizeof(tmp), "ltc1[%d]", i);
        binding->ltc1_loc[i] = glGetUniformLocation(binding->gl_program, tmp);
    }
    for (i = 0; i < NV2A_MAX_LIGHTS; i++) {
        snprintf(tmp, sizeof(tmp), "lightInfiniteHalfVector%d", i);
        binding->light_infinite_half_vector_loc[i] =
            glGetUniformLocation(binding->gl_program, tmp);
        snprintf(tmp, sizeof(tmp), "lightInfiniteDirection%d", i);
        binding->light_infinite_direction_loc[i] =
            glGetUniformLocation(binding->gl_program, tmp);

        snprintf(tmp, sizeof(tmp), "lightLocalPosition%d", i);
        binding->light_local_position_loc[i] = glGetUniformLocation(binding->gl_program, tmp);
        snprintf(tmp, sizeof(tmp), "lightLocalAttenuation%d", i);
        binding->light_local_attenuation_loc[i] =
            glGetUniformLocation(binding->gl_program, tmp);
    }
    for (i = 0; i < 8; i++) {
        snprintf(tmp, sizeof(tmp), "clipRegion[%d]", i);
        binding->clip_region_loc[i] = glGetUniformLocation(binding->gl_program, tmp);
    }

    if (state->fixed_function) {
        binding->material_alpha_loc =
            glGetUniformLocation(binding->gl_program, "material_alpha");
    } else {
        binding->material_alpha_loc = -1;
    }
}

ShaderBinding *generate_shaders(const ShaderState *state)
{
    char *previous_numeric_locale = setlocale(LC_NUMERIC, NULL);
    if (previous_numeric_locale) {
        previous_numeric_locale = g_strdup(previous_numeric_locale);
    }

    /* Ensure numeric values are printed with '.' radix, no grouping */
    setlocale(LC_NUMERIC, "C");
    GLuint program = glCreateProgram();

    /* Create an optional geometry shader and find primitive type */
    GLenum gl_primitive_mode;
    MString* geometry_shader_code =
        generate_geometry_shader(state->polygon_front_mode,
                                 state->polygon_back_mode,
                                 state->primitive_mode,
                                 &gl_primitive_mode,
                                 state->smooth_shading);
    if (geometry_shader_code) {
        const char* geometry_shader_code_str =
             mstring_get_str(geometry_shader_code);
        GLuint geometry_shader = create_gl_shader(GL_GEOMETRY_SHADER,
                                                  geometry_shader_code_str,
                                                  "geometry shader");
        glAttachShader(program, geometry_shader);
        mstring_unref(geometry_shader_code);
    }

    /* create the vertex shader */
    MString *vertex_shader_code =
        generate_vertex_shader(state, geometry_shader_code != NULL);
    GLuint vertex_shader = create_gl_shader(GL_VERTEX_SHADER,
                                            mstring_get_str(vertex_shader_code),
                                            "vertex shader");
    glAttachShader(program, vertex_shader);
    mstring_unref(vertex_shader_code);

    /* generate a fragment shader from register combiners */
    MString *fragment_shader_code = psh_translate(state->psh);
    const char *fragment_shader_code_str =
        mstring_get_str(fragment_shader_code);
    GLuint fragment_shader = create_gl_shader(GL_FRAGMENT_SHADER,
                                              fragment_shader_code_str,
                                              "fragment shader");
    glAttachShader(program, fragment_shader);
    mstring_unref(fragment_shader_code);

    /* link the program */
    glLinkProgram(program);
    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if(!linked) {
        GLchar log[2048];
        glGetProgramInfoLog(program, 2048, NULL, log);
        fprintf(stderr, "nv2a: shader linking failed: %s\n", log);
        abort();
    }

    glUseProgram(program);

    ShaderBinding* ret = g_malloc0(sizeof(ShaderBinding));
    ret->gl_program = program;
    ret->gl_primitive_mode = gl_primitive_mode;

    update_shader_constant_locations(ret, state);

    if (previous_numeric_locale) {
        setlocale(LC_NUMERIC, previous_numeric_locale);
        g_free(previous_numeric_locale);
    }

    return ret;
}

static const char *shader_gl_vendor = NULL;

static void shader_create_cache_folder(void)
{
    char *shader_path = g_strdup_printf("%sshaders", xemu_settings_get_base_path());
    qemu_mkdir(shader_path);
    g_free(shader_path);
}

static char *shader_get_lru_cache_path(void)
{
    return g_strdup_printf("%s/shader_cache_list", xemu_settings_get_base_path());
}

static void shader_write_lru_list_entry_to_disk(Lru *lru, LruNode *node, void *opaque)
{
    FILE *lru_list_file = (FILE*) opaque;
    size_t written = fwrite(&node->hash, sizeof(uint64_t), 1, lru_list_file);
    if (written != 1) {
        fprintf(stderr, "nv2a: Failed to write shader list entry %llx to disk\n",
                (unsigned long long) node->hash);
    }
}

void shader_write_cache_reload_list(PGRAPHState *pg)
{
    if (!g_config.perf.cache_shaders) {
        qatomic_set(&pg->shader_cache_writeback_pending, false);
        qemu_event_set(&pg->shader_cache_writeback_complete);
        return;
    }

    char *shader_lru_path = shader_get_lru_cache_path();
    qemu_thread_join(&pg->shader_disk_thread);

    FILE *lru_list = qemu_fopen(shader_lru_path, "wb");
    g_free(shader_lru_path);
    if (!lru_list) {
        fprintf(stderr, "nv2a: Failed to open shader LRU cache for writing\n");
        return;
    }

    lru_visit_active(&pg->shader_cache, shader_write_lru_list_entry_to_disk, lru_list);
    fclose(lru_list);

    lru_flush(&pg->shader_cache);

    qatomic_set(&pg->shader_cache_writeback_pending, false);
    qemu_event_set(&pg->shader_cache_writeback_complete);
}

bool shader_load_from_memory(ShaderLruNode *snode)
{
    assert(glGetError() == GL_NO_ERROR);

    if (!snode->program) {
        return false;
    }

    GLuint gl_program = glCreateProgram();
    glProgramBinary(gl_program, snode->program_format, snode->program, snode->program_size);
    GLint gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        NV2A_DPRINTF("failed to load shader binary from disk: GL error code %d\n", gl_error);
        glDeleteProgram(gl_program);
        return false;
    }

    glValidateProgram(gl_program);
    GLint valid = 0;
    glGetProgramiv(gl_program, GL_VALIDATE_STATUS, &valid);
    if (!valid) {
        GLchar log[1024];
        glGetProgramInfoLog(gl_program, 1024, NULL, log);
        NV2A_DPRINTF("failed to load shader binary from disk: %s\n", log);
        glDeleteProgram(gl_program);
        return false;
    }

    glUseProgram(gl_program);

    ShaderBinding* binding = g_malloc0(sizeof(ShaderBinding));
    binding->gl_program = gl_program;
    binding->gl_primitive_mode = get_gl_primitive_mode(snode->state.polygon_front_mode,
                                                       snode->state.primitive_mode);
    snode->binding = binding;

    g_free(snode->program);
    snode->program = NULL;

    update_shader_constant_locations(binding, &snode->state);

    return true;
}

static char *shader_get_bin_directory(uint64_t hash)
{
    const char *cfg_dir = xemu_settings_get_base_path();
    uint64_t bin_mask = 0xffffUL << 48;
    char *shader_bin_dir = g_strdup_printf("%s/shaders/%04lx",
                                           cfg_dir, (hash & bin_mask) >> 48);
    return shader_bin_dir;
}

static char *shader_get_binary_path(const char *shader_bin_dir, uint64_t hash)
{
    uint64_t bin_mask = 0xffffUL << 48;
    return g_strdup_printf("%s/%012lx", shader_bin_dir,
                           hash & (~bin_mask));
}

static void shader_load_from_disk(PGRAPHState *pg, uint64_t hash)
{
    char *shader_bin_dir = shader_get_bin_directory(hash);
    char *shader_path = shader_get_binary_path(shader_bin_dir, hash);
    char *cached_xemu_version = NULL;
    char *cached_gl_vendor = NULL;
    void *program_buffer = NULL;

    uint64_t cached_xemu_version_len;
    uint64_t gl_vendor_len;
    GLenum program_binary_format;
    ShaderState state;
    size_t shader_size;

    g_free(shader_bin_dir);

    qemu_mutex_lock(&pg->shader_cache_lock);
    if (lru_contains_hash(&pg->shader_cache, hash)) {
        qemu_mutex_unlock(&pg->shader_cache_lock);
        return;
    }
    qemu_mutex_unlock(&pg->shader_cache_lock);

    FILE *shader_file = qemu_fopen(shader_path, "rb");
    if (!shader_file) {
        goto error;
    }

    size_t nread;
    #define READ_OR_ERR(data, data_len) \
        do { \
            nread = fread(data, data_len, 1, shader_file); \
            if (nread != 1) { \
                fclose(shader_file); \
                goto error; \
            } \
        } while (0)

    READ_OR_ERR(&cached_xemu_version_len, sizeof(cached_xemu_version_len));

    cached_xemu_version = g_malloc(cached_xemu_version_len +1);
    READ_OR_ERR(cached_xemu_version, cached_xemu_version_len);
    if (strcmp(cached_xemu_version, xemu_version) != 0) {
        fclose(shader_file);
        goto error;
    }

    READ_OR_ERR(&gl_vendor_len, sizeof(gl_vendor_len));

    cached_gl_vendor = g_malloc(gl_vendor_len);
    READ_OR_ERR(cached_gl_vendor, gl_vendor_len);
    if (strcmp(cached_gl_vendor, shader_gl_vendor) != 0) {
        fclose(shader_file);
        goto error;
    }

    READ_OR_ERR(&program_binary_format, sizeof(program_binary_format));
    READ_OR_ERR(&state, sizeof(state));
    READ_OR_ERR(&shader_size, sizeof(shader_size));

    program_buffer = g_malloc(shader_size);
    READ_OR_ERR(program_buffer, shader_size);

    #undef READ_OR_ERR

    fclose(shader_file);
    g_free(shader_path);
    g_free(cached_xemu_version);
    g_free(cached_gl_vendor);

    qemu_mutex_lock(&pg->shader_cache_lock);
    LruNode *node = lru_lookup(&pg->shader_cache, hash, &state);
    ShaderLruNode *snode = container_of(node, ShaderLruNode, node);

    /* If we happened to regenerate this shader already, then we may as well use the new one */
    if (snode->binding) {
        qemu_mutex_unlock(&pg->shader_cache_lock);
        return;
    }

    snode->program_format = program_binary_format;
    snode->program_size = shader_size;
    snode->program = program_buffer;
    snode->cached = true;
    qemu_mutex_unlock(&pg->shader_cache_lock);
    return;

error:
    /* Delete the shader so it won't be loaded again */
    qemu_unlink(shader_path);
    g_free(shader_path);
    g_free(program_buffer);
    g_free(cached_xemu_version);
    g_free(cached_gl_vendor);
}

static void *shader_reload_lru_from_disk(void *arg)
{
    if (!g_config.perf.cache_shaders) {
        return NULL;
    }

    PGRAPHState *pg = (PGRAPHState*) arg;
    char *shader_lru_path = shader_get_lru_cache_path();

    FILE *lru_shaders_list = qemu_fopen(shader_lru_path, "rb");
    g_free(shader_lru_path);
    if (!lru_shaders_list) {
        return NULL;
    }

    uint64_t hash;
    while (fread(&hash, sizeof(uint64_t), 1, lru_shaders_list) == 1) {
        shader_load_from_disk(pg, hash);
    }

    return NULL;
}

static void shader_cache_entry_init(Lru *lru, LruNode *node, void *state)
{
    ShaderLruNode *snode = container_of(node, ShaderLruNode, node);
    memcpy(&snode->state, state, sizeof(ShaderState));
    snode->cached = false;
    snode->binding = NULL;
    snode->program = NULL;
    snode->save_thread = NULL;
}

static void shader_cache_entry_post_evict(Lru *lru, LruNode *node)
{
    ShaderLruNode *snode = container_of(node, ShaderLruNode, node);

    if (snode->save_thread) {
        qemu_thread_join(snode->save_thread);
        g_free(snode->save_thread);
    }

    if (snode->binding) {
        glDeleteProgram(snode->binding->gl_program);
        g_free(snode->binding);
    }

    if (snode->program) {
        g_free(snode->program);
    }

    snode->cached = false;
    snode->save_thread = NULL;
    snode->binding = NULL;
    snode->program = NULL;
    memset(&snode->state, 0, sizeof(ShaderState));
}

static bool shader_cache_entry_compare(Lru *lru, LruNode *node, void *key)
{
    ShaderLruNode *snode = container_of(node, ShaderLruNode, node);
    return memcmp(&snode->state, key, sizeof(ShaderState));
}

void shader_cache_init(PGRAPHState *pg)
{
    if (!shader_gl_vendor) {
        shader_gl_vendor = (const char *) glGetString(GL_VENDOR);
    }

    shader_create_cache_folder();

    /* FIXME: Make this configurable */
    const size_t shader_cache_size = 50*1024;
    lru_init(&pg->shader_cache);
    pg->shader_cache_entries = malloc(shader_cache_size * sizeof(ShaderLruNode));
    assert(pg->shader_cache_entries != NULL);
    for (int i = 0; i < shader_cache_size; i++) {
        lru_add_free(&pg->shader_cache, &pg->shader_cache_entries[i].node);
    }

    pg->shader_cache.init_node = shader_cache_entry_init;
    pg->shader_cache.compare_nodes = shader_cache_entry_compare;
    pg->shader_cache.post_node_evict = shader_cache_entry_post_evict;

    qemu_thread_create(&pg->shader_disk_thread, "pgraph.shader_cache",
                       shader_reload_lru_from_disk, pg, QEMU_THREAD_JOINABLE);
}

static void *shader_write_to_disk(void *arg)
{
    ShaderLruNode *snode = (ShaderLruNode*) arg;

    char *shader_bin = shader_get_bin_directory(snode->node.hash);
    char *shader_path = shader_get_binary_path(shader_bin, snode->node.hash);

    static uint64_t gl_vendor_len;
    if (gl_vendor_len == 0) {
        gl_vendor_len = (uint64_t) (strlen(shader_gl_vendor) + 1);
    }

    static uint64_t xemu_version_len = 0;
    if (xemu_version_len == 0) {
        xemu_version_len = (uint64_t) (strlen(xemu_version) + 1);
    }

    qemu_mkdir(shader_bin);
    g_free(shader_bin);

    FILE *shader_file = qemu_fopen(shader_path, "wb");
    if (!shader_file) {
        goto error;
    }

    size_t written;
    #define WRITE_OR_ERR(data, data_size) \
        do { \
            written = fwrite(data, data_size, 1, shader_file); \
            if (written != 1) { \
                fclose(shader_file); \
                goto error; \
            } \
        } while (0)

    WRITE_OR_ERR(&xemu_version_len, sizeof(xemu_version_len));
    WRITE_OR_ERR(xemu_version, xemu_version_len);

    WRITE_OR_ERR(&gl_vendor_len, sizeof(gl_vendor_len));
    WRITE_OR_ERR(shader_gl_vendor, gl_vendor_len);

    WRITE_OR_ERR(&snode->program_format, sizeof(snode->program_format));
    WRITE_OR_ERR(&snode->state, sizeof(snode->state));

    WRITE_OR_ERR(&snode->program_size, sizeof(snode->program_size));
    WRITE_OR_ERR(snode->program, snode->program_size);

    #undef WRITE_OR_ERR

    fclose(shader_file);

    g_free(shader_path);
    g_free(snode->program);
    snode->program = NULL;

    return NULL;

error:
    fprintf(stderr, "nv2a: Failed to write shader binary file to %s\n", shader_path);
    qemu_unlink(shader_path);
    g_free(shader_path);
    g_free(snode->program);
    snode->program = NULL;
    return NULL;
}

void shader_cache_to_disk(ShaderLruNode *snode)
{
    if (!snode->binding || snode->cached) {
        return;
    }

    GLint program_size;
    glGetProgramiv(snode->binding->gl_program, GL_PROGRAM_BINARY_LENGTH, &program_size);

    if (snode->program) {
        g_free(snode->program);
        snode->program = NULL;
    }

    /* program_size might be zero on some systems, if no binary formats are supported */
    if (program_size == 0) {
        return;
    }

    snode->program = g_malloc(program_size);
    GLsizei program_size_copied;
    glGetProgramBinary(snode->binding->gl_program, program_size, &program_size_copied,
                       &snode->program_format, snode->program);
    assert(glGetError() == GL_NO_ERROR);

    snode->program_size = program_size_copied;
    snode->cached = true;

    char name[24];
    snprintf(name, sizeof(name), "scache-%llx", (unsigned long long) snode->node.hash);
    snode->save_thread = g_malloc0(sizeof(QemuThread));
    qemu_thread_create(snode->save_thread, name, shader_write_to_disk, snode, QEMU_THREAD_JOINABLE);
}
