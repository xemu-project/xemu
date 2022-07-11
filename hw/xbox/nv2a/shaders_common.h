/*
 * QEMU Geforce NV2A shader common definitions
 *
 * Copyright (c) 2015 espes
 * Copyright (c) 2015 Jannik Vogel
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

#ifndef HW_NV2A_SHADERS_COMMON_H
#define HW_NV2A_SHADERS_COMMON_H

#include "debug.h"

#define DEF_VERTEX_DATA(qualifier, in_out, prefix, suffix) \
    "noperspective " in_out " float " prefix "vtx_inv_w" suffix ";\n" \
    "flat " in_out " float " prefix "vtx_inv_w_flat" suffix ";\n" \
    qualifier " " in_out " vec4 " prefix "vtxD0" suffix ";\n" \
    qualifier " " in_out " vec4 " prefix "vtxD1" suffix ";\n" \
    qualifier " " in_out " vec4 " prefix "vtxB0" suffix ";\n" \
    qualifier " " in_out " vec4 " prefix "vtxB1" suffix ";\n" \
    "noperspective " in_out " float " prefix "vtxFog" suffix ";\n" \
    "noperspective " in_out " vec4 " prefix "vtxT0" suffix ";\n" \
    "noperspective " in_out " vec4 " prefix "vtxT1" suffix ";\n" \
    "noperspective " in_out " vec4 " prefix "vtxT2" suffix ";\n" \
    "noperspective " in_out " vec4 " prefix "vtxT3" suffix ";\n"

#define STRUCT_VERTEX_DATA_OUT_SMOOTH DEF_VERTEX_DATA("noperspective", "out", "", "")
#define STRUCT_VERTEX_DATA_IN_SMOOTH  DEF_VERTEX_DATA("noperspective", "in", "", "")
#define STRUCT_V_VERTEX_DATA_OUT_SMOOTH DEF_VERTEX_DATA("noperspective", "out", "v_", "")
#define STRUCT_V_VERTEX_DATA_IN_ARRAY_SMOOTH DEF_VERTEX_DATA("noperspective", "in", "v_", "[]")

#define STRUCT_VERTEX_DATA_OUT_FLAT DEF_VERTEX_DATA("flat", "out", "", "")
#define STRUCT_VERTEX_DATA_IN_FLAT  DEF_VERTEX_DATA("flat", "in", "", "")
#define STRUCT_V_VERTEX_DATA_OUT_FLAT DEF_VERTEX_DATA("flat", "out", "v_", "")
#define STRUCT_V_VERTEX_DATA_IN_ARRAY_FLAT DEF_VERTEX_DATA("flat", "in", "v_", "[]")

typedef struct {
   int ref;
   gchar *string;
} MString;

void mstring_append_fmt(MString *mstring, const char *fmt, ...);
MString *mstring_from_fmt(const char *fmt, ...);
void mstring_append_va(MString *mstring, const char *fmt, va_list va);

static inline
void mstring_ref(MString *mstr)
{
   mstr->ref++;
}

static inline
void mstring_unref(MString *mstr)
{
   mstr->ref--;
   if (!mstr->ref) {
      g_free(mstr->string);
      g_free(mstr);
   }
}

static inline
void mstring_append(MString *mstr, const char *str)
{
   gchar *n = g_strconcat(mstr->string, str, NULL);
   g_free(mstr->string);
   mstr->string = n;
}

static inline
void mstring_append_chr(MString *mstr, char chr)
{
   mstring_append_fmt(mstr, "%c", chr);
}

static inline
void mstring_append_int(MString *mstr, int val)
{
   mstring_append_fmt(mstr, "%" PRId64, val);
}

static inline
MString *mstring_new(void)
{
   MString *mstr = g_malloc(sizeof(MString));
   mstr->ref = 1;
   mstr->string = g_strdup("");
   return mstr;
}

static inline
MString *mstring_from_str(const char *str)
{
   MString *mstr = g_malloc(sizeof(MString));
   mstr->ref = 1;
   mstr->string = g_strdup(str);
   return mstr;
}

static inline
const gchar *mstring_get_str(MString *mstr)
{
   return mstr->string;
}

static inline
size_t mstring_get_length(MString *mstr)
{
   return strlen(mstr->string);
}


#endif
