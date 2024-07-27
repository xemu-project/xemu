/*
 * Geforce NV2A PGRAPH GLSL Shader Generator
 *
 * Copyright (c) 2024 Matt Borgerson
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


#include "common.h"


MString *pgraph_get_glsl_vtx_header(MString *out, bool location, bool smooth, bool in, bool prefix, bool array)
{
    const char *flat_s = "flat";
    const char *noperspective_s = "noperspective";
    const char *qualifier_s = smooth ? noperspective_s : flat_s;
    const char *qualifiers[11] = {
        noperspective_s, flat_s,          qualifier_s,     qualifier_s,
        qualifier_s,     qualifier_s,     noperspective_s, noperspective_s,
        noperspective_s, noperspective_s, noperspective_s
    };

    const char *in_out_s = in ? "in" : "out";

    const char *float_s = "float";
    const char *vec4_s = "vec4";
    const char *types[11] = { float_s, float_s, vec4_s, vec4_s, vec4_s, vec4_s,
                              float_s, vec4_s,  vec4_s, vec4_s, vec4_s };

    const char *prefix_s = prefix ? "v_" : "";
    const char *names[11] = {
        "vtx_inv_w", "vtx_inv_w_flat", "vtxD0", "vtxD1", "vtxB0", "vtxB1",
        "vtxFog",    "vtxT0",          "vtxT1", "vtxT2", "vtxT3",
    };
    const char *suffix_s = array ? "[]" : "";

    for (int i = 0; i < 11; i++) {
        if (location) {
            mstring_append_fmt(out, "layout(location = %d) ", i);
        }
        mstring_append_fmt(out, "%s %s %s %s%s%s;\n",
            qualifiers[i], in_out_s, types[i], prefix_s, names[i], suffix_s);
    }

    return out;
}
