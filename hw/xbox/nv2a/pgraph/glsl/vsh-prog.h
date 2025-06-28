/*
 * Geforce NV2A PGRAPH GLSL Shader Generator
 *
 * Copyright (c) 2014 Jannik Vogel
 * Copyright (c) 2012 espes
 * Copyright (c) 2025 Matt Borgerson
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HW_XBOX_NV2A_PGRAPH_GLSL_VSH_PROG_H
#define HW_XBOX_NV2A_PGRAPH_GLSL_VSH_PROG_H

#include "qemu/mstring.h"

void pgraph_glsl_gen_vsh_prog(uint16_t version, const uint32_t *tokens,
                              unsigned int length, MString *header,
                              MString *body);

#endif
