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
#include "geom.h"

void pgraph_glsl_set_geom_state(PGRAPHState *pg, GeomState *state)
{
    state->primitive_mode = (enum ShaderPrimitiveMode)pg->primitive_mode;

    state->polygon_front_mode = (enum ShaderPolygonMode)GET_MASK(
        pgraph_reg_r(pg, NV_PGRAPH_SETUPRASTER),
        NV_PGRAPH_SETUPRASTER_FRONTFACEMODE);
    state->polygon_back_mode = (enum ShaderPolygonMode)GET_MASK(
        pgraph_reg_r(pg, NV_PGRAPH_SETUPRASTER),
        NV_PGRAPH_SETUPRASTER_BACKFACEMODE);

    state->smooth_shading = GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_CONTROL_3),
                                     NV_PGRAPH_CONTROL_3_SHADEMODE) ==
                            NV_PGRAPH_CONTROL_3_SHADEMODE_SMOOTH;
}

bool pgraph_glsl_need_geom(const GeomState *state)
{
    /* FIXME: Missing support for 2-sided-poly mode */
    assert(state->polygon_front_mode == state->polygon_back_mode);
    enum ShaderPolygonMode polygon_mode = state->polygon_front_mode;

    /* POINT mode shouldn't require any special work */
    if (polygon_mode == POLY_MODE_POINT) {
        return false;
    }

    switch (state->primitive_mode) {
    case PRIM_TYPE_TRIANGLES:
        if (polygon_mode == POLY_MODE_FILL) {
            return false;
        }
        return true;
    case PRIM_TYPE_TRIANGLE_STRIP:
        if (polygon_mode == POLY_MODE_FILL) {
            return false;
        }
        assert(polygon_mode == POLY_MODE_LINE);
        return true;
    case PRIM_TYPE_TRIANGLE_FAN:
        if (polygon_mode == POLY_MODE_FILL) {
            return false;
        }
        assert(polygon_mode == POLY_MODE_LINE);
        return true;
    case PRIM_TYPE_QUADS:
        if (polygon_mode == POLY_MODE_LINE) {
            return true;
        } else if (polygon_mode == POLY_MODE_FILL) {
            return true;
        } else {
            assert(false);
            return false;
        }
        break;
    case PRIM_TYPE_QUAD_STRIP:
        if (polygon_mode == POLY_MODE_LINE) {
            return true;
        } else if (polygon_mode == POLY_MODE_FILL) {
            return true;
        } else {
            assert(false);
            return false;
        }
        break;
    case PRIM_TYPE_POLYGON:
        if (polygon_mode == POLY_MODE_LINE) {
            return false;
        }
        if (polygon_mode == POLY_MODE_FILL) {
            if (state->smooth_shading) {
                return false;
            }
            return true;
        } else {
            assert(false);
            return false;
        }
        break;
    default:
        return false;
    }
}

MString *pgraph_glsl_gen_geom(const GeomState *state, GenGeomGlslOptions opts)
{
    /* FIXME: Missing support for 2-sided-poly mode */
    assert(state->polygon_front_mode == state->polygon_back_mode);
    enum ShaderPolygonMode polygon_mode = state->polygon_front_mode;

    /* POINT mode shouldn't require any special work */
    if (polygon_mode == POLY_MODE_POINT) {
        return NULL;
    }

    /* Handle LINE and FILL mode */
    const char *layout_in = NULL;
    const char *layout_out = NULL;
    const char *body = NULL;
    switch (state->primitive_mode) {
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
            if (state->smooth_shading) {
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
    MString *output =
        mstring_from_fmt("#version %d\n\n"
                         "%s"
                         "%s"
                         "\n",
                         opts.vulkan ? 450 : 400, layout_in, layout_out);
    pgraph_glsl_get_vtx_header(output, opts.vulkan, state->smooth_shading, true,
                               true, true);
    pgraph_glsl_get_vtx_header(output, opts.vulkan, state->smooth_shading,
                               false, false, false);

    if (state->smooth_shading) {
        mstring_append(output,
                       "void emit_vertex(int index, int _unused) {\n"
                       "  gl_Position = gl_in[index].gl_Position;\n"
                       "  gl_PointSize = gl_in[index].gl_PointSize;\n"
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
        mstring_append(output,
                       "void emit_vertex(int index, int provoking_index) {\n"
                       "  gl_Position = gl_in[index].gl_Position;\n"
                       "  gl_PointSize = gl_in[index].gl_PointSize;\n"
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

    mstring_append_fmt(output,
                       "\n"
                       "void main() {\n"
                       "%s"
                       "}\n",
                       body);

    return output;
}
