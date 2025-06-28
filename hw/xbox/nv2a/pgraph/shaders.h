/*
 * QEMU Geforce NV2A shader generator
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

#ifndef HW_XBOX_NV2A_PGRAPH_SHADERS_H
#define HW_XBOX_NV2A_PGRAPH_SHADERS_H

#include <stdint.h>

#include "vsh.h"
#include "psh.h"

typedef struct ShaderState {
    VshState vsh;
    PshState psh;
} ShaderState;

typedef struct PGRAPHState PGRAPHState;

ShaderState pgraph_get_shader_state(PGRAPHState *pg);
uint32_t pgraph_get_color_key_mask_for_texture(PGRAPHState *pg, int i);

#endif
