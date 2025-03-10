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


MString *pgraph_get_glsl_vtx_header(MString *out, bool location, bool smooth, bool in, bool prefix, bool array, bool z_perspective)
{
    const char *flat_s = "flat ";
    const char *smooth_s = "";
    const char *qualifier_s = smooth ? smooth_s : flat_s;
    const char *qualifiers[9] = { qualifier_s, qualifier_s, qualifier_s,
                                  qualifier_s, smooth_s,    smooth_s,
                                  smooth_s,    smooth_s,    smooth_s };

    const char *in_out_s = in ? "in" : "out";

    const char *float_s = "float";
    const char *vec4_s = "vec4";

    const char *types[9] = { vec4_s, vec4_s, vec4_s, vec4_s, float_s,
                             vec4_s, vec4_s, vec4_s, vec4_s };

    const char *prefix_s = prefix ? "v_" : "";
    const char *names[9] = {
        "vtxD0", "vtxD1", "vtxB0", "vtxB1", "vtxFog",
        "vtxT0", "vtxT1", "vtxT2", "vtxT3",
    };
    const char *suffix_s = array ? "[]" : "";

    for (int i = 0; i < 9; i++) {
        if (location) {
            mstring_append_fmt(out, "layout(location = %d) ", i);
        }
        mstring_append_fmt(out, "%s%s %s %s%s%s;\n",
            qualifiers[i], in_out_s, types[i], prefix_s, names[i], suffix_s);
    }

    if (location) {
        mstring_append(out, " layout(location = 9) ");
    }
    mstring_append_fmt(out, "%s float %sdepthBuf%s;\n", in_out_s, prefix_s, suffix_s);
    return out;
}
