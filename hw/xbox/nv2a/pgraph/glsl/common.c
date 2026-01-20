/*
 * Geforce NV2A PGRAPH GLSL Shader Generator
 *
 * Copyright (c) 2024-2025 Matt Borgerson
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
#include "hw/xbox/nv2a/pgraph/pgraph.h"

#define DECL_UNIFORM_ELEMENT_NAME(type) #type,
const char *uniform_element_type_to_str[] = {
    UNIFORM_ELEMENT_TYPE_X(DECL_UNIFORM_ELEMENT_NAME)
};

MString *pgraph_glsl_get_vtx_header(MString *out, bool location, bool smooth,
                                    bool in, bool prefix, bool array)
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
        { flat_s,      vec4_s,  "vtxPos0" },
        { flat_s,      vec4_s,  "vtxPos1" },
        { flat_s,      vec4_s,  "vtxPos2" },
        { flat_s,      float_s, "triMZ"  },
    };

    for (int i = 0; i < ARRAY_SIZE(attr); i++) {
        if (location) {
            mstring_append_fmt(out, "layout(location = %d) ", i);
        }
        mstring_append_fmt(out, "%s%s %s %s%s%s;\n", attr[i].qualifier,
                           in_out_s, attr[i].type, prefix_s, attr[i].name,
                           suffix_s);
    }

    return out;
}

void pgraph_glsl_set_clip_range_uniform_value(PGRAPHState *pg, float clipRange[4])
{
    float zmax;
    switch (pg->surface_shape.zeta_format) {
    case NV097_SET_SURFACE_FORMAT_ZETA_Z16:
        zmax = pg->surface_shape.z_format ? f16_max : (float)0xFFFF;
        break;
    case NV097_SET_SURFACE_FORMAT_ZETA_Z24S8:
        zmax = pg->surface_shape.z_format ? f24_max : (float)0xFFFFFF;
        break;
    default:
        assert(0);
    }

    uint32_t zclip_min = pgraph_reg_r(pg, NV_PGRAPH_ZCLIPMIN);
    uint32_t zclip_max = pgraph_reg_r(pg, NV_PGRAPH_ZCLIPMAX);

    clipRange[0] = 0;
    clipRange[1] = zmax;
    clipRange[2] = *(float *)&zclip_min;
    clipRange[3] = *(float *)&zclip_max;
}
