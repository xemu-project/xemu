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
    const char *smooth_s = "";
    const char *flat_s = "flat ";
    const char *qualifier_s = smooth ? smooth_s : flat_s;
    const char *in_out_s = in ? "in" : "out";
    const char *float_s = "float";
    const char *vec4_s = "vec4";
    const char *prefix_s = prefix ? "v_" : "";
    const char *suffix_s = array ? "[]" : "";
    const struct {
        const char *qualifier, *type, *name;
    } attr[] = {
        { qualifier_s, vec4_s,  "vtxD0"  },
        { qualifier_s, vec4_s,  "vtxD1"  },
        { qualifier_s, vec4_s,  "vtxB0"  },
        { qualifier_s, vec4_s,  "vtxB1"  },
        { smooth_s,    float_s, "vtxFog" },
        { smooth_s,    vec4_s,  "vtxT0"  },
        { smooth_s,    vec4_s,  "vtxT1"  },
        { smooth_s,    vec4_s,  "vtxT2"  },
        { smooth_s,    vec4_s,  "vtxT3"  },
    };

    for (int i = 0; i < ARRAY_SIZE(attr); i++) {
        if (location) {
            mstring_append_fmt(out, "layout(location = %d) ", i);
        }
        mstring_append_fmt(out, "%s%s %s %s%s%s;\n", attr[i].qualifier,
                           in_out_s, attr[i].type, prefix_s, attr[i].name,
                           suffix_s);
    }

    if (location) {
        mstring_append(out, " layout(location = 9) ");
    }
    mstring_append_fmt(out, "%s float %sdepthBuf%s;\n", in_out_s, prefix_s, suffix_s);
    return out;
}
