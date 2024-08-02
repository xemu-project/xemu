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

#include "hw/xbox/nv2a/pgraph/shaders.h"
#include "common.h"
#include "geom.h"

MString *pgraph_gen_geom_glsl(enum ShaderPolygonMode polygon_front_mode,
                              enum ShaderPolygonMode polygon_back_mode,
                              enum ShaderPrimitiveMode primitive_mode,
                              bool smooth_shading,
                              bool vulkan)
{
    /* FIXME: Missing support for 2-sided-poly mode */
    assert(polygon_front_mode == polygon_back_mode);
    enum ShaderPolygonMode polygon_mode = polygon_front_mode;

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
    MString *s = mstring_new();
    mstring_append_fmt(s, "#version %d\n\n", vulkan ? 450 : 400);
    mstring_append(s, layout_in);
    mstring_append(s, layout_out);
    mstring_append(s, "\n");
    pgraph_get_glsl_vtx_header(s, vulkan, smooth_shading, true, true, true);
    pgraph_get_glsl_vtx_header(s, vulkan, smooth_shading, false, false, false);

    if (smooth_shading) {
        mstring_append(s,
                       "void emit_vertex(int index, int _unused) {\n"
                       "  gl_Position = gl_in[index].gl_Position;\n"
                       "  gl_PointSize = gl_in[index].gl_PointSize;\n"
                       // "  gl_ClipDistance[0] = gl_in[index].gl_ClipDistance[0];\n"
                       // "  gl_ClipDistance[1] = gl_in[index].gl_ClipDistance[1];\n"
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
                       "void emit_vertex(int index, int provoking_index) {\n"
                       "  gl_Position = gl_in[index].gl_Position;\n"
                       "  gl_PointSize = gl_in[index].gl_PointSize;\n"
                       // "  gl_ClipDistance[0] = gl_in[index].gl_ClipDistance[0];\n"
                       // "  gl_ClipDistance[1] = gl_in[index].gl_ClipDistance[1];\n"
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
