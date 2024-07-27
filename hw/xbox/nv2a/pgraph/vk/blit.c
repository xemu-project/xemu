/*
 * Geforce NV2A PGRAPH Vulkan Renderer
 *
 * Copyright (c) 2024 Matt Borgerson
 *
 * Based on GL implementation:
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

void pgraph_vk_image_blit(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;
    ContextSurfaces2DState *context_surfaces = &pg->context_surfaces_2d;
    ImageBlitState *image_blit = &pg->image_blit;
    BetaState *beta = &pg->beta;

    pgraph_vk_surface_update(d, false, true, true);

    assert(context_surfaces->object_instance == image_blit->context_surfaces);

    unsigned int bytes_per_pixel;
    switch (context_surfaces->color_format) {
        case NV062_SET_COLOR_FORMAT_LE_Y8:
            bytes_per_pixel = 1;
            break;
        case NV062_SET_COLOR_FORMAT_LE_R5G6B5:
            bytes_per_pixel = 2;
            break;
        case NV062_SET_COLOR_FORMAT_LE_A8R8G8B8:
        case NV062_SET_COLOR_FORMAT_LE_X8R8G8B8:
        case NV062_SET_COLOR_FORMAT_LE_X8R8G8B8_Z8R8G8B8:
        case NV062_SET_COLOR_FORMAT_LE_Y32:
            bytes_per_pixel = 4;
            break;
        default:
            fprintf(stderr, "Unknown blit surface format: 0x%x\n",
                    context_surfaces->color_format);
            assert(false);
            break;
    }

    hwaddr source_dma_len, dest_dma_len;

    uint8_t *source = (uint8_t *)nv_dma_map(
        d, context_surfaces->dma_image_source, &source_dma_len);
    assert(context_surfaces->source_offset < source_dma_len);
    source += context_surfaces->source_offset;

    uint8_t *dest = (uint8_t *)nv_dma_map(d, context_surfaces->dma_image_dest,
                                          &dest_dma_len);
    assert(context_surfaces->dest_offset < dest_dma_len);
    dest += context_surfaces->dest_offset;

    hwaddr source_addr = source - d->vram_ptr;
    hwaddr dest_addr = dest - d->vram_ptr;

    SurfaceBinding *surf_src = pgraph_vk_surface_get(d, source_addr);
    if (surf_src) {
        pgraph_vk_surface_download_if_dirty(d, surf_src);
    }

    SurfaceBinding *surf_dest = pgraph_vk_surface_get(d, dest_addr);
    if (surf_dest) {
        if (image_blit->height < surf_dest->height ||
            image_blit->width < surf_dest->width) {
            pgraph_vk_surface_download_if_dirty(d, surf_dest);
        } else {
            // The blit will completely replace the surface so any pending
            // download should be discarded.
            surf_dest->download_pending = false;
            surf_dest->draw_dirty = false;
        }
        surf_dest->upload_pending = true;
        pg->draw_time++;
    }

    hwaddr source_offset = image_blit->in_y * context_surfaces->source_pitch +
                           image_blit->in_x * bytes_per_pixel;
    hwaddr dest_offset = image_blit->out_y * context_surfaces->dest_pitch +
                         image_blit->out_x * bytes_per_pixel;

    hwaddr source_size =
        (image_blit->height - 1) * context_surfaces->source_pitch +
        image_blit->width * bytes_per_pixel;
    hwaddr dest_size = (image_blit->height - 1) * context_surfaces->dest_pitch +
                       image_blit->width * bytes_per_pixel;

    /* FIXME: What does hardware do in this case? */
    assert(source_addr + source_offset + source_size <=
           memory_region_size(d->vram));
    assert(dest_addr + dest_offset + dest_size <= memory_region_size(d->vram));

    uint8_t *source_row = source + source_offset;
    uint8_t *dest_row = dest + dest_offset;

    if (image_blit->operation == NV09F_SET_OPERATION_SRCCOPY) {
        // NV2A_GL_DPRINTF(false, "NV09F_SET_OPERATION_SRCCOPY");
        for (unsigned int y = 0; y < image_blit->height; y++) {
            memmove(dest_row, source_row, image_blit->width * bytes_per_pixel);
            source_row += context_surfaces->source_pitch;
            dest_row += context_surfaces->dest_pitch;
        }
    } else if (image_blit->operation == NV09F_SET_OPERATION_BLEND_AND) {
        // NV2A_GL_DPRINTF(false, "NV09F_SET_OPERATION_BLEND_AND");
        uint32_t max_beta_mult = 0x7f80;
        uint32_t beta_mult = beta->beta >> 16;
        uint32_t inv_beta_mult = max_beta_mult - beta_mult;
        for (unsigned int y = 0; y < image_blit->height; y++) {
            for (unsigned int x = 0; x < image_blit->width; x++) {
                for (unsigned int ch = 0; ch < 3; ch++) {
                    uint32_t a = source_row[x * 4 + ch] * beta_mult;
                    uint32_t b = dest_row[x * 4 + ch] * inv_beta_mult;
                    dest_row[x * 4 + ch] = (a + b) / max_beta_mult;
                }
            }
            source_row += context_surfaces->source_pitch;
            dest_row += context_surfaces->dest_pitch;
        }
    } else {
        fprintf(stderr, "Unknown blit operation: 0x%x\n",
                image_blit->operation);
        assert(false && "Unknown blit operation");
    }

    NV2A_DPRINTF("  - 0x%tx -> 0x%tx\n", source_addr, dest_addr);

    bool needs_alpha_patching;
    uint8_t alpha_override;
    switch (context_surfaces->color_format) {
    case NV062_SET_COLOR_FORMAT_LE_X8R8G8B8:
        needs_alpha_patching = true;
        alpha_override = 0xff;
        break;
    case NV062_SET_COLOR_FORMAT_LE_X8R8G8B8_Z8R8G8B8:
        needs_alpha_patching = true;
        alpha_override = 0;
        break;
    default:
        needs_alpha_patching = false;
        alpha_override = 0;
    }

    if (needs_alpha_patching) {
        dest_row = dest + dest_offset;
        for (unsigned int y = 0; y < image_blit->height; y++) {
            for (unsigned int x = 0; x < image_blit->width; x++) {
                dest_row[x * 4 + 3] = alpha_override;
            }
            dest_row += context_surfaces->dest_pitch;
        }
    }

    dest_addr += dest_offset;
    memory_region_set_client_dirty(d->vram, dest_addr, dest_size,
                                   DIRTY_MEMORY_VGA);
    memory_region_set_client_dirty(d->vram, dest_addr, dest_size,
                                   DIRTY_MEMORY_NV2A_TEX);
}
