/*
 * Geforce NV2A PGRAPH OpenGL Renderer
 *
 * Copyright (c) 2012 espes
 * Copyright (c) 2015 Jannik Vogel
 * Copyright (c) 2018-2024 Matt Borgerson
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

#include "hw/xbox/nv2a/nv2a_int.h"
#include "renderer.h"
#include "hw/xbox/nv2a/pgraph/blit.h"

// TODO: Optimize.
void pgraph_gl_image_blit(NV2AState *d)
{
    static const PGRAPHSurfaceOps ops = {
        .surface_update = pgraph_gl_surface_update,
        .surface_get = pgraph_gl_surface_get,
        .surface_download_if_dirty = pgraph_gl_surface_download_if_dirty,
    };

    pgraph_common_image_blit(d, &ops);
}
