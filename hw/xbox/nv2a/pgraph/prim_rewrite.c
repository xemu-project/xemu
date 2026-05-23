/*
 * Geforce NV2A PGRAPH Primitive Index Rewrite
 *
 * Rewrites NV2A primitive types to triangle/line/point lists on CPU.
 * Handles provoking vertex placement for flat shading correctness.
 *
 * Copyright (c) 2026 Matt Borgerson
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
#include "prim_rewrite.h"

void pgraph_prim_rewrite_init(PrimRewriteBuf *buf)
{
    buf->data = NULL;
    buf->capacity = 0;
}

void pgraph_prim_rewrite_finalize(PrimRewriteBuf *buf)
{
    g_free(buf->data);
    buf->data = NULL;
    buf->capacity = 0;
}

static void ensure_capacity(PrimRewriteBuf *buf, unsigned int needed)
{
    if (needed <= buf->capacity) {
        return;
    }
    buf->capacity = MAX(needed, buf->capacity * 2);
    buf->data = g_realloc(buf->data, buf->capacity * sizeof(uint32_t));
}

enum ShaderPrimitiveMode
pgraph_prim_rewrite_get_output_mode(enum ShaderPrimitiveMode primitive_mode,
                                    enum ShaderPolygonMode polygon_mode)
{
    switch (primitive_mode) {
    case PRIM_TYPE_POINTS:
        return PRIM_TYPE_POINTS;
    case PRIM_TYPE_LINES:
    case PRIM_TYPE_LINE_STRIP:
    case PRIM_TYPE_LINE_LOOP:
        return PRIM_TYPE_LINES;
    case PRIM_TYPE_TRIANGLES:
    case PRIM_TYPE_TRIANGLE_STRIP:
    case PRIM_TYPE_TRIANGLE_FAN:
        return PRIM_TYPE_TRIANGLES;
    case PRIM_TYPE_QUADS:
    case PRIM_TYPE_QUAD_STRIP:
    case PRIM_TYPE_POLYGON:
        return polygon_mode == POLY_MODE_LINE ? PRIM_TYPE_LINES :
                                                PRIM_TYPE_TRIANGLES;
    default:
        assert(!"Unexpected primitive mode");
        return primitive_mode;
    }
}

static inline bool needs_rewrite(PrimAssemblyState mode)
{
    switch (mode.primitive_mode) {
    case PRIM_TYPE_POINTS:
        return false;
    case PRIM_TYPE_LINES:
    case PRIM_TYPE_TRIANGLES:
        return mode.last_provoking && mode.flat_shading;
    default:
        return true;
    }
}

static unsigned int max_output_indices(enum ShaderPrimitiveMode mode,
                                       enum ShaderPolygonMode polygon_mode,
                                       unsigned int input_count)
{
    switch (mode) {
    case PRIM_TYPE_LINES:
        return input_count;
    case PRIM_TYPE_LINE_STRIP:
        return (input_count >= 2) ? (input_count - 1) * 2 : 0;
    case PRIM_TYPE_LINE_LOOP:
        return (input_count >= 2) ? input_count * 2 : 0;
    case PRIM_TYPE_TRIANGLES:
        return input_count;
    case PRIM_TYPE_TRIANGLE_STRIP:
    case PRIM_TYPE_TRIANGLE_FAN:
        return (input_count >= 3) ? (input_count - 2) * 3 : 0;
    case PRIM_TYPE_POLYGON:
        if (polygon_mode == POLY_MODE_LINE) {
            return (input_count >= 2) ? input_count * 2 : 0;
        }
        return (input_count >= 3) ? (input_count - 2) * 3 : 0;
    case PRIM_TYPE_QUADS:
        if (polygon_mode == POLY_MODE_LINE) {
            return (input_count / 4) * 8;
        }
        return (input_count / 4) * 6;
    case PRIM_TYPE_QUAD_STRIP:
        if (polygon_mode == POLY_MODE_LINE) {
            return (input_count >= 4) ? ((input_count - 2) / 2) * 8 : 0;
        }
        return (input_count >= 4) ? ((input_count - 2) / 2) * 6 : 0;
    default:
        return 0;
    }
}

static inline uint32_t idx_at(const uint32_t *idx, unsigned int i,
                              uint32_t base)
{
    return idx ? idx[i] : base + i;
}

static inline void emit_vertex(PrimRewrite *r, uint32_t v)
{
    r->indices[r->num_indices++] = v;
}

static inline void emit_line(PrimRewrite *r, uint32_t a, uint32_t b)
{
    emit_vertex(r, a);
    emit_vertex(r, b);
}

/* Place provoking vertex p at index 0. */
static inline void emit_line_pv(PrimRewrite *r, uint32_t a, uint32_t b,
                                uint32_t p)
{
    if (p == a) {
        emit_line(r, a, b);
    } else {
        emit_line(r, b, a);
    }
}

static inline void emit_tri(PrimRewrite *r, uint32_t a, uint32_t b, uint32_t c)
{
    emit_vertex(r, a);
    emit_vertex(r, b);
    emit_vertex(r, c);
}

/* Rotate provoking vertex p to index 0, preserving winding of (a, b, c). */
static inline void emit_tri_pv(PrimRewrite *r, uint32_t a, uint32_t b,
                               uint32_t c, uint32_t p)
{
    if (p == a) {
        emit_tri(r, a, b, c);
    } else if (p == b) {
        emit_tri(r, b, c, a);
    } else {
        emit_tri(r, c, a, b);
    }
}

static void rewrite_lines(PrimRewrite *r, const uint32_t *idx, uint32_t base,
                          unsigned int count, bool last_provoking)
{
    for (unsigned int i = 0; i + 1 < count; i += 2) {
        uint32_t v0 = idx_at(idx, i, base);
        uint32_t v1 = idx_at(idx, i + 1, base);
        uint32_t pv = last_provoking ? v1 : v0;

        emit_line_pv(r, v0, v1, pv);
    }
}

static void rewrite_line_strip(PrimRewrite *r, const uint32_t *idx,
                               uint32_t base, unsigned int count,
                               bool last_provoking)
{
    for (unsigned int i = 0; i + 1 < count; i++) {
        uint32_t v0 = idx_at(idx, i, base);
        uint32_t v1 = idx_at(idx, i + 1, base);
        uint32_t pv = last_provoking ? v1 : v0;

        emit_line_pv(r, v0, v1, pv);
    }
}

static void rewrite_line_loop(PrimRewrite *r, const uint32_t *idx,
                              uint32_t base, unsigned int count,
                              bool last_provoking)
{
    if (count < 2) {
        return;
    }

    for (unsigned int i = 0; i + 1 < count; i++) {
        uint32_t v0 = idx_at(idx, i, base);
        uint32_t v1 = idx_at(idx, i + 1, base);
        uint32_t pv = last_provoking ? v1 : v0;

        emit_line_pv(r, v0, v1, pv);
    }

    uint32_t v_last = idx_at(idx, count - 1, base);
    uint32_t v_first = idx_at(idx, 0, base);
    uint32_t pv = last_provoking ? v_first : v_last;

    emit_line_pv(r, v_last, v_first, pv);
}

static void rewrite_triangles(PrimRewrite *r, const uint32_t *idx,
                              uint32_t base, unsigned int count,
                              bool last_provoking)
{
    for (unsigned int i = 0; i + 2 < count; i += 3) {
        uint32_t v0 = idx_at(idx, i, base);
        uint32_t v1 = idx_at(idx, i + 1, base);
        uint32_t v2 = idx_at(idx, i + 2, base);
        uint32_t pv = last_provoking ? v2 : v0;

        emit_tri_pv(r, v0, v1, v2, pv);
    }
}

static void rewrite_triangle_strip(PrimRewrite *r, const uint32_t *idx,
                                   uint32_t base, unsigned int count,
                                   bool last_provoking)
{
    for (unsigned int i = 0; i + 2 < count; i++) {
        uint32_t v0 = idx_at(idx, i, base);
        uint32_t v1 = idx_at(idx, i + 1, base);
        uint32_t v2 = idx_at(idx, i + 2, base);
        uint32_t pv = last_provoking ? v2 : v0;

        if (i & 1) {
            emit_tri_pv(r, v1, v0, v2, pv);
        } else {
            emit_tri_pv(r, v0, v1, v2, pv);
        }
    }
}

static void rewrite_triangle_fan(PrimRewrite *r, const uint32_t *idx,
                                 uint32_t base, unsigned int count,
                                 bool last_provoking)
{
    if (count < 3) {
        return;
    }

    uint32_t hub = idx_at(idx, 0, base);

    for (unsigned int i = 0; i + 2 < count; i++) {
        uint32_t v1 = idx_at(idx, i + 1, base);
        uint32_t v2 = idx_at(idx, i + 2, base);
        uint32_t pv = last_provoking ? v2 : v1;

        emit_tri_pv(r, hub, v1, v2, pv);
    }
}

static void rewrite_quads(PrimRewrite *r, const uint32_t *idx, uint32_t base,
                          unsigned int count, bool flat_shading)
{
    for (unsigned int i = 0; i + 3 < count; i += 4) {
        uint32_t v0 = idx_at(idx, i, base);
        uint32_t v1 = idx_at(idx, i + 1, base);
        uint32_t v2 = idx_at(idx, i + 2, base);
        uint32_t v3 = idx_at(idx, i + 3, base);

        if (flat_shading) {
            /* Use v1-v3 diagonal so provoking vertex v3 is in both triangles.
             * This gives correct flat shading color but slightly different
             * depth slope vs hardware. */
            emit_tri(r, v3, v0, v1);
            emit_tri(r, v3, v1, v2);
        } else {
            /* v0-v2 diagonal: matches hardware quad tessellation */
            emit_tri(r, v0, v1, v2);
            emit_tri(r, v0, v2, v3);
        }
    }
}

static void rewrite_quads_line(PrimRewrite *r, const uint32_t *idx,
                               uint32_t base, unsigned int count)
{
    for (unsigned int i = 0; i + 3 < count; i += 4) {
        uint32_t v0 = idx_at(idx, i, base);
        uint32_t v1 = idx_at(idx, i + 1, base);
        uint32_t v2 = idx_at(idx, i + 2, base);
        uint32_t v3 = idx_at(idx, i + 3, base);

        emit_line(r, v0, v1);
        emit_line(r, v1, v2);
        emit_line(r, v2, v3);
        emit_line(r, v3, v0);
    }
}

static void rewrite_quad_strip(PrimRewrite *r, const uint32_t *idx,
                               uint32_t base, unsigned int count,
                               bool flat_shading)
{
    if (count < 4) {
        return;
    }

    for (unsigned int i = 0; i + 3 < count; i += 2) {
        uint32_t v0 = idx_at(idx, i, base);
        uint32_t v1 = idx_at(idx, i + 1, base);
        uint32_t v2 = idx_at(idx, i + 2, base);
        uint32_t v3 = idx_at(idx, i + 3, base);

        if (flat_shading) {
            /* Use v0-v3 diagonal so provoking vertex v3 is in both triangles.
             * This gives correct flat shading color but slightly different
             * depth slope vs hardware. */
            emit_tri(r, v3, v2, v0);
            emit_tri(r, v3, v0, v1);
        } else {
            /* v1-v2 diagonal: matches hardware quad strip tessellation */
            emit_tri(r, v0, v1, v2);
            emit_tri(r, v2, v1, v3);
        }
    }
}

static void rewrite_quad_strip_line(PrimRewrite *r, const uint32_t *idx,
                                    uint32_t base, unsigned int count)
{
    if (count < 4) {
        return;
    }

    for (unsigned int i = 0; i + 3 < count; i += 2) {
        uint32_t v0 = idx_at(idx, i, base);
        uint32_t v1 = idx_at(idx, i + 1, base);
        uint32_t v2 = idx_at(idx, i + 2, base);
        uint32_t v3 = idx_at(idx, i + 3, base);

        emit_line(r, v0, v1);
        emit_line(r, v1, v3);
        emit_line(r, v3, v2);
        emit_line(r, v2, v0);
    }
}

static void rewrite_polygon(PrimRewrite *r, const uint32_t *idx, uint32_t base,
                            unsigned int count)
{
    if (count < 3) {
        return;
    }

    uint32_t hub = idx_at(idx, 0, base);

    for (unsigned int i = 0; i + 2 < count; i++) {
        uint32_t v1 = idx_at(idx, i + 1, base);
        uint32_t v2 = idx_at(idx, i + 2, base);

        emit_tri(r, hub, v1, v2);
    }
}

static void rewrite_polygon_line(PrimRewrite *r, const uint32_t *idx,
                                 uint32_t base, unsigned int count)
{
    if (count < 2) {
        return;
    }

    for (unsigned int i = 0; i + 1 < count; i++) {
        emit_line(r, idx_at(idx, i, base), idx_at(idx, i + 1, base));
    }

    /* Close the loop */
    emit_line(r, idx_at(idx, count - 1, base), idx_at(idx, 0, base));
}

static void rewrite_indices(PrimRewrite *r, const PrimAssemblyState *mode,
                            const uint32_t *idx, uint32_t base,
                            unsigned int num_indices)
{
    switch (mode->primitive_mode) {
    case PRIM_TYPE_LINES:
        rewrite_lines(r, idx, base, num_indices, mode->last_provoking);
        break;
    case PRIM_TYPE_LINE_STRIP:
        rewrite_line_strip(r, idx, base, num_indices, mode->last_provoking);
        break;
    case PRIM_TYPE_LINE_LOOP:
        rewrite_line_loop(r, idx, base, num_indices, mode->last_provoking);
        break;
    case PRIM_TYPE_TRIANGLES:
        rewrite_triangles(r, idx, base, num_indices, mode->last_provoking);
        break;
    case PRIM_TYPE_TRIANGLE_STRIP:
        rewrite_triangle_strip(r, idx, base, num_indices, mode->last_provoking);
        break;
    case PRIM_TYPE_TRIANGLE_FAN:
        rewrite_triangle_fan(r, idx, base, num_indices, mode->last_provoking);
        break;
    case PRIM_TYPE_QUADS:
        if (mode->polygon_mode == POLY_MODE_LINE) {
            rewrite_quads_line(r, idx, base, num_indices);
        } else {
            rewrite_quads(r, idx, base, num_indices, mode->flat_shading);
        }
        break;
    case PRIM_TYPE_QUAD_STRIP:
        if (mode->polygon_mode == POLY_MODE_LINE) {
            rewrite_quad_strip_line(r, idx, base, num_indices);
        } else {
            rewrite_quad_strip(r, idx, base, num_indices, mode->flat_shading);
        }
        break;
    case PRIM_TYPE_POLYGON:
        if (mode->polygon_mode == POLY_MODE_LINE) {
            rewrite_polygon_line(r, idx, base, num_indices);
        } else {
            rewrite_polygon(r, idx, base, num_indices);
        }
        break;
    default:
        assert(!"Unexpected primitive mode");
        break;
    }
}

PrimRewrite pgraph_prim_rewrite_ranges(PrimRewriteBuf *buf,
                                       PrimAssemblyState mode,
                                       const int32_t *starts,
                                       const int32_t *counts,
                                       unsigned int num_ranges)
{
    PrimRewrite result = { 0 };

    assert(mode.polygon_mode != POLY_MODE_POINT ||
           mode.primitive_mode != PRIM_TYPE_POLYGON);

    if (!needs_rewrite(mode)) {
        return result;
    }

    unsigned int total_max_output = 0;
    for (unsigned int r = 0; r < num_ranges; r++) {
        total_max_output += max_output_indices(mode.primitive_mode,
                                               mode.polygon_mode, counts[r]);
    }

    if (total_max_output == 0) {
        return result;
    }

    ensure_capacity(buf, total_max_output);
    result.indices = buf->data;

    for (unsigned int r = 0; r < num_ranges; r++) {
        if (counts[r] == 0) {
            continue;
        }

        rewrite_indices(&result, &mode, NULL, starts[r], counts[r]);
    }

    return result;
}

PrimRewrite pgraph_prim_rewrite_indexed(PrimRewriteBuf *buf,
                                        PrimAssemblyState mode,
                                        const uint32_t *input_indices,
                                        unsigned int num_input_indices)
{
    PrimRewrite result = { 0 };

    assert(mode.polygon_mode != POLY_MODE_POINT ||
           mode.primitive_mode != PRIM_TYPE_POLYGON);

    if (!needs_rewrite(mode)) {
        return result;
    }

    unsigned int max_output = max_output_indices(
        mode.primitive_mode, mode.polygon_mode, num_input_indices);

    if (max_output == 0) {
        return result;
    }

    ensure_capacity(buf, max_output);
    result.indices = buf->data;

    rewrite_indices(&result, &mode, input_indices, 0, num_input_indices);

    return result;
}
