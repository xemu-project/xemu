/* Narrow QEMU-to-D3D11 state/data bridge. */

#include "d3d11_bridge.h"

#include "../../nv2a_int.h"
#include "../../nv2a_regs.h"
#include "../clear.h"
#include "../pgraph.h"

#include <string.h>

static uint32_t bridge_reg(const PGRAPHState *pg, unsigned int byte_offset)
{
    return pg->regs_[byte_offset];
}

static void bridge_surface(const Surface *source, D3D11BridgeSurface *target)
{
    target->offset = source->offset;
    target->pitch = source->pitch;
    target->draw_dirty = source->draw_dirty;
    target->buffer_dirty = source->buffer_dirty;
    target->write_enabled_cache = source->write_enabled_cache;
    target->reserved = 0;
}

const PGRAPHState *d3d11_bridge_pgraph(const NV2AState *state)
{
    return state == NULL ? NULL : &state->pgraph;
}

bool d3d11_bridge_snapshot_pgraph(const PGRAPHState *pg,
                                  D3D11BridgePgraphSnapshot *out)
{
    if (pg == NULL || out == NULL) {
        return false;
    }
    D3D11BridgePgraphSnapshot snapshot = { 0 };
    snapshot.primitive_mode = pg->primitive_mode;
    snapshot.surface_type = pg->surface_type;
    snapshot.surface_scale_factor = pg->surface_scale_factor;
    snapshot.anti_aliasing = pg->surface_shape.anti_aliasing;
    snapshot.color_format = pg->surface_shape.color_format;
    snapshot.zeta_format = pg->surface_shape.zeta_format;
    snapshot.z_format = pg->surface_shape.z_format;
    snapshot.clip_x = pg->surface_shape.clip_x;
    snapshot.clip_y = pg->surface_shape.clip_y;
    snapshot.clip_width = pg->surface_shape.clip_width;
    snapshot.clip_height = pg->surface_shape.clip_height;
    snapshot.binding_clip_x = pg->surface_binding_dim.clip_x;
    snapshot.binding_clip_y = pg->surface_binding_dim.clip_y;
    snapshot.binding_clip_width = pg->surface_binding_dim.clip_width;
    snapshot.binding_clip_height = pg->surface_binding_dim.clip_height;
    snapshot.binding_width = pg->surface_binding_dim.width;
    snapshot.binding_height = pg->surface_binding_dim.height;
    bridge_surface(&pg->surface_color, &snapshot.color_surface);
    bridge_surface(&pg->surface_zeta, &snapshot.zeta_surface);
    snapshot.dma_color = pg->dma_color;
    snapshot.dma_zeta = pg->dma_zeta;
    snapshot.dma_vertex_a = pg->dma_vertex_a;
    snapshot.dma_vertex_b = pg->dma_vertex_b;
    snapshot.inline_buffer_length = pg->inline_buffer_length;
    snapshot.inline_array_length = pg->inline_array_length;
    snapshot.inline_elements_length = pg->inline_elements_length;
    snapshot.vsh_mode =
        GET_MASK(bridge_reg(pg, NV_PGRAPH_CSV0_D), NV_PGRAPH_CSV0_D_MODE);
    snapshot.vsh_program_start =
        GET_MASK(bridge_reg(pg, NV_PGRAPH_CSV0_C),
                 NV_PGRAPH_CSV0_C_CHEOPS_PROGRAM_START);
    snapshot.vertex_program_write = pg->enable_vertex_program_write;

    const uint32_t control0 = bridge_reg(pg, NV_PGRAPH_CONTROL_0);
    const uint32_t control1 = bridge_reg(pg, NV_PGRAPH_CONTROL_1);
    const uint32_t control2 = bridge_reg(pg, NV_PGRAPH_CONTROL_2);
    const uint32_t blend = bridge_reg(pg, NV_PGRAPH_BLEND);
    const uint32_t raster = bridge_reg(pg, NV_PGRAPH_SETUPRASTER);
    snapshot.alpha_test_enable =
        (control0 & NV_PGRAPH_CONTROL_0_ALPHATESTENABLE) != 0;
    snapshot.dither_enable = (control0 & NV_PGRAPH_CONTROL_0_DITHERENABLE) != 0;
    snapshot.z_enable = (control0 & NV_PGRAPH_CONTROL_0_ZENABLE) != 0;
    snapshot.z_write_enable =
        (control0 & NV_PGRAPH_CONTROL_0_ZWRITEENABLE) != 0;
    snapshot.stencil_write_enable =
        (control0 & NV_PGRAPH_CONTROL_0_STENCIL_WRITE_ENABLE) != 0;
    snapshot.red_write_enable =
        (control0 & NV_PGRAPH_CONTROL_0_RED_WRITE_ENABLE) != 0;
    snapshot.green_write_enable =
        (control0 & NV_PGRAPH_CONTROL_0_GREEN_WRITE_ENABLE) != 0;
    snapshot.blue_write_enable =
        (control0 & NV_PGRAPH_CONTROL_0_BLUE_WRITE_ENABLE) != 0;
    snapshot.alpha_write_enable =
        (control0 & NV_PGRAPH_CONTROL_0_ALPHA_WRITE_ENABLE) != 0;
    snapshot.stencil_test_enable =
        (control1 & NV_PGRAPH_CONTROL_1_STENCIL_TEST_ENABLE) != 0;
    snapshot.logic_op_enable = (blend & NV_PGRAPH_BLEND_LOGICOP_ENABLE) != 0;
    snapshot.blend_enable = (blend & NV_PGRAPH_BLEND_EN) != 0;
    snapshot.cull_enable = (raster & NV_PGRAPH_SETUPRASTER_CULLENABLE) != 0;
    snapshot.front_face = (raster & NV_PGRAPH_SETUPRASTER_FRONTFACE) != 0;
    snapshot.front_face_mode =
        GET_MASK(raster, NV_PGRAPH_SETUPRASTER_FRONTFACEMODE);
    snapshot.back_face_mode =
        GET_MASK(raster, NV_PGRAPH_SETUPRASTER_BACKFACEMODE);
    snapshot.z_func = GET_MASK(control0, NV_PGRAPH_CONTROL_0_ZFUNC);
    snapshot.stencil_func =
        GET_MASK(control1, NV_PGRAPH_CONTROL_1_STENCIL_FUNC);
    snapshot.stencil_read_mask =
        GET_MASK(control1, NV_PGRAPH_CONTROL_1_STENCIL_MASK_READ);
    snapshot.stencil_write_mask =
        GET_MASK(control1, NV_PGRAPH_CONTROL_1_STENCIL_MASK_WRITE);
    snapshot.stencil_ref = GET_MASK(control1, NV_PGRAPH_CONTROL_1_STENCIL_REF);
    snapshot.stencil_op_fail =
        GET_MASK(control2, NV_PGRAPH_CONTROL_2_STENCIL_OP_FAIL);
    snapshot.stencil_op_zfail =
        GET_MASK(control2, NV_PGRAPH_CONTROL_2_STENCIL_OP_ZFAIL);
    snapshot.stencil_op_zpass =
        GET_MASK(control2, NV_PGRAPH_CONTROL_2_STENCIL_OP_ZPASS);
    snapshot.blend_src_factor = GET_MASK(blend, NV_PGRAPH_BLEND_SFACTOR);
    snapshot.blend_dst_factor = GET_MASK(blend, NV_PGRAPH_BLEND_DFACTOR);
    snapshot.blend_equation = GET_MASK(blend, NV_PGRAPH_BLEND_EQN);
    snapshot.blend_color = bridge_reg(pg, NV_PGRAPH_BLENDCOLOR);
    snapshot.cull_face = GET_MASK(raster, NV_PGRAPH_SETUPRASTER_CULLCTRL);
    snapshot.polygon_offset_point_enable =
        (raster & NV_PGRAPH_SETUPRASTER_POFFSETPOINTENABLE) != 0;
    snapshot.polygon_offset_line_enable =
        (raster & NV_PGRAPH_SETUPRASTER_POFFSETLINEENABLE) != 0;
    snapshot.polygon_offset_fill_enable =
        (raster & NV_PGRAPH_SETUPRASTER_POFFSETFILLENABLE) != 0;
    snapshot.polygon_smooth_enable =
        (raster & NV_PGRAPH_SETUPRASTER_POLYSMOOTHENABLE) != 0;
    snapshot.window_clip_type =
        (raster & NV_PGRAPH_SETUPRASTER_WINDOWCLIPTYPE) != 0;
    snapshot.antialiasing_enable = (bridge_reg(pg, NV_PGRAPH_ANTIALIASING) &
                                    NV_PGRAPH_ANTIALIASING_ENABLE) != 0;
    snapshot.window_clip_values_valid = 1;

    for (unsigned int i = 0; i < 8; ++i) {
        const uint32_t x = bridge_reg(pg, NV_PGRAPH_WINDOWCLIPX0 + i * 4);
        const uint32_t y = bridge_reg(pg, NV_PGRAPH_WINDOWCLIPY0 + i * 4);
        if ((x & ~(NV_PGRAPH_WINDOWCLIPX0_XMIN |
                   NV_PGRAPH_WINDOWCLIPX0_XMAX)) != 0 ||
            (y & ~(NV_PGRAPH_WINDOWCLIPY0_YMIN |
                   NV_PGRAPH_WINDOWCLIPY0_YMAX)) != 0) {
            snapshot.window_clip_values_valid = 0;
        }
        snapshot.window_clip_x_min[i] =
            GET_MASK(x, NV_PGRAPH_WINDOWCLIPX0_XMIN);
        snapshot.window_clip_x_max[i] =
            GET_MASK(x, NV_PGRAPH_WINDOWCLIPX0_XMAX);
        snapshot.window_clip_y_min[i] =
            GET_MASK(y, NV_PGRAPH_WINDOWCLIPY0_YMIN);
        snapshot.window_clip_y_max[i] =
            GET_MASK(y, NV_PGRAPH_WINDOWCLIPY0_YMAX);
    }
    const uint32_t clear_x = bridge_reg(pg, NV_PGRAPH_CLEARRECTX);
    const uint32_t clear_y = bridge_reg(pg, NV_PGRAPH_CLEARRECTY);
    snapshot.clear_rect_x_min = GET_MASK(clear_x, NV_PGRAPH_CLEARRECTX_XMIN);
    snapshot.clear_rect_x_max = GET_MASK(clear_x, NV_PGRAPH_CLEARRECTX_XMAX);
    snapshot.clear_rect_y_min = GET_MASK(clear_y, NV_PGRAPH_CLEARRECTY_YMIN);
    snapshot.clear_rect_y_max = GET_MASK(clear_y, NV_PGRAPH_CLEARRECTY_YMAX);
    snapshot.z_stencil_clear_value =
        bridge_reg(pg, NV_PGRAPH_ZSTENCILCLEARVALUE);
    pgraph_get_clear_color((PGRAPHState *)pg, snapshot.clear_color);
    *out = snapshot;
    return true;
}

bool d3d11_bridge_snapshot_attributes(const PGRAPHState *pg,
                                      D3D11BridgeVertexAttribute *out,
                                      size_t capacity, size_t *count)
{
    if (pg == NULL || count == NULL) {
        return false;
    }
    *count = D3D11_BRIDGE_VERTEX_ATTRIBUTES;
    if (out == NULL || capacity < D3D11_BRIDGE_VERTEX_ATTRIBUTES) {
        return false;
    }
    for (size_t i = 0; i < D3D11_BRIDGE_VERTEX_ATTRIBUTES; ++i) {
        const VertexAttribute *source = &pg->vertex_attributes[i];
        D3D11BridgeVertexAttribute *target = &out[i];
        memset(target, 0, sizeof(*target));
        target->dma_select = source->dma_select;
        target->needs_conversion = source->needs_conversion;
        target->inline_buffer_populated = source->inline_buffer_populated;
        target->offset = source->offset;
        target->inline_array_offset = source->inline_array_offset;
        memcpy(target->inline_value, source->inline_value,
               sizeof(target->inline_value));
        target->format = source->format;
        target->size = source->size;
        target->count = source->count;
        target->stride = source->stride;
    }
    return true;
}

static bool bridge_copy_words(const uint32_t *source, size_t source_count,
                              uint32_t *out, size_t capacity, size_t *count)
{
    if (count == NULL) {
        return false;
    }
    *count = source_count;
    if (source_count != 0 && (out == NULL || capacity < source_count)) {
        return false;
    }
    if (source_count != 0 && source == NULL) {
        return false;
    }
    if (source_count != 0) {
        memcpy(out, source, source_count * sizeof(*out));
    }
    return true;
}

bool d3d11_bridge_copy_inline_array(const PGRAPHState *pg, uint32_t *out,
                                    size_t capacity, size_t *count)
{
    if (pg == NULL || count == NULL) {
        return false;
    }
    *count = pg->inline_array_length;
    return pg->inline_array_length <= D3D11_BRIDGE_MAX_BATCH_WORDS &&
           bridge_copy_words(pg->inline_array, pg->inline_array_length, out,
                             capacity, count);
}

bool d3d11_bridge_copy_inline_elements(const PGRAPHState *pg, uint32_t *out,
                                       size_t capacity, size_t *count)
{
    if (pg == NULL || count == NULL) {
        return false;
    }
    *count = pg->inline_elements_length;
    return pg->inline_elements_length <= D3D11_BRIDGE_MAX_BATCH_WORDS &&
           bridge_copy_words(pg->inline_elements, pg->inline_elements_length,
                             out, capacity, count);
}

bool d3d11_bridge_copy_draw_arrays(const PGRAPHState *pg,
                                   D3D11BridgeDrawArraysRange *out,
                                   size_t capacity, size_t *count)
{
    if (pg == NULL || count == NULL) {
        return false;
    }
    *count = pg->draw_arrays_length;
    if (pg->draw_arrays_length > D3D11_BRIDGE_MAX_DRAW_ARRAY_RANGES) {
        return false;
    }
    if (pg->draw_arrays_length != 0 &&
        (out == NULL || capacity < pg->draw_arrays_length)) {
        return false;
    }
    for (size_t i = 0; i < pg->draw_arrays_length; ++i) {
        out[i].start = pg->draw_arrays_start[i];
        out[i].count = pg->draw_arrays_count[i];
    }
    return true;
}

bool d3d11_bridge_copy_inline_buffer_value(const PGRAPHState *pg,
                                           size_t attribute, size_t vertex,
                                           float out[4])
{
    if (pg == NULL || out == NULL ||
        attribute >= D3D11_BRIDGE_VERTEX_ATTRIBUTES ||
        vertex >= pg->inline_buffer_length ||
        vertex >= D3D11_BRIDGE_MAX_BATCH_WORDS ||
        !pg->vertex_attributes[attribute].inline_buffer_populated ||
        pg->vertex_attributes[attribute].inline_buffer == NULL) {
        return false;
    }
    memcpy(out, pg->vertex_attributes[attribute].inline_buffer + vertex * 4,
           sizeof(float) * 4);
    return true;
}

bool d3d11_bridge_copy_program(const PGRAPHState *pg, uint32_t *out,
                               size_t capacity, size_t *token_count)
{
    if (pg == NULL || token_count == NULL) {
        return false;
    }
    *token_count = 0;
    const uint32_t start = GET_MASK(bridge_reg(pg, NV_PGRAPH_CSV0_C),
                                    NV_PGRAPH_CSV0_C_CHEOPS_PROGRAM_START);
    if (start >= D3D11_BRIDGE_MAX_PROGRAM_TOKENS) {
        return false;
    }
    size_t count = 0;
    bool found_final = false;
    for (; start + count < D3D11_BRIDGE_MAX_PROGRAM_TOKENS; ++count) {
        if ((pg->program_data[start + count][3] & 1u) != 0) {
            ++count;
            found_final = true;
            break;
        }
    }
    if (!found_final || count == 0 || count > D3D11_BRIDGE_MAX_PROGRAM_TOKENS ||
        out == NULL || capacity < count * VSH_TOKEN_SIZE) {
        *token_count = count;
        return false;
    }
    memcpy(out, &pg->program_data[start][0],
           count * VSH_TOKEN_SIZE * sizeof(uint32_t));
    *token_count = count;
    return true;
}

bool d3d11_bridge_copy_vsh_constants(const PGRAPHState *pg, uint32_t *out,
                                     size_t capacity, size_t *word_count)
{
    if (pg == NULL || word_count == NULL) {
        return false;
    }
    *word_count = D3D11_BRIDGE_VSH_CONSTANTS * 4;
    if (out == NULL || capacity < *word_count) {
        return false;
    }
    memcpy(out, pg->vsh_constants, *word_count * sizeof(uint32_t));
    return true;
}

bool d3d11_bridge_vram_view(const NV2AState *state, uint8_t **data,
                            uint64_t *size)
{
    if (state == NULL || state->vram == NULL || state->vram_ptr == NULL ||
        data == NULL || size == NULL) {
        return false;
    }
    *data = state->vram_ptr;
    *size = memory_region_size(state->vram);
    return *size != 0;
}

bool d3d11_bridge_copy_dma(const NV2AState *state, uint64_t dma_object,
                           uint64_t offset, uint64_t size, uint8_t *out,
                           size_t capacity)
{
    const uint64_t ramin_size =
        state == NULL ? 0 : memory_region_size(&state->ramin);
    if (state == NULL || state->vram == NULL || state->vram_ptr == NULL ||
        state->ramin_ptr == NULL || size > capacity ||
        (dma_object & (sizeof(uint32_t) - 1)) != 0 ||
        ramin_size < 3 * sizeof(uint32_t) ||
        dma_object > ramin_size - 3 * sizeof(uint32_t)) {
        return false;
    }
    const DMAObject dma = nv_dma_load((NV2AState *)state, dma_object);
    const uint64_t address = dma.address & UINT64_C(0x07ffffff);
    const uint64_t vram_size = memory_region_size(state->vram);
    if (dma.dma_class != NV_DMA_IN_MEMORY_CLASS ||
        dma.dma_target != NV_DMA_TARGET_NVM || address >= vram_size ||
        dma.limit >= vram_size - address || offset > dma.limit ||
        size > dma.limit - offset || offset > UINT64_MAX - address ||
        address + offset > vram_size || size > vram_size - address - offset ||
        (size != 0 && out == NULL)) {
        return false;
    }
    if (size != 0) {
        memcpy(out, state->vram_ptr + address + offset, (size_t)size);
    }
    return true;
}

bool d3d11_bridge_describe_surface(const NV2AState *state, bool color,
                                   uint32_t width, uint32_t height,
                                   D3D11BridgeSurfaceDescriptor *out,
                                   uint32_t *status)
{
    if (status != NULL) {
        *status = D3D11_BRIDGE_SURFACE_INVALID;
    }
    if (state == NULL || state->vram == NULL || state->vram_ptr == NULL ||
        state->ramin_ptr == NULL || out == NULL || status == NULL ||
        width == 0 || height == 0) {
        return false;
    }
    const PGRAPHState *pg = &state->pgraph;
    const Surface *surface = color ? &pg->surface_color : &pg->surface_zeta;
    uint32_t format = 1;
    uint32_t bytes_per_pixel = 0;
    if (color) {
        if (pgraph_clear_classify_color_storage(
                pg->surface_shape.color_format) !=
            PGRAPH_CLEAR_COLOR_STORAGE_BGR8A8) {
            *status = D3D11_BRIDGE_SURFACE_UNSUPPORTED;
            return false;
        }
        bytes_per_pixel = 4;
    } else if (pg->surface_shape.z_format != 0) {
        *status = D3D11_BRIDGE_SURFACE_UNSUPPORTED;
        return false;
    } else if (pg->surface_shape.zeta_format == D3D11_BRIDGE_ZETA_Z16) {
        format = 2;
        bytes_per_pixel = 2;
    } else if (pg->surface_shape.zeta_format == D3D11_BRIDGE_ZETA_Z24S8) {
        format = 3;
        bytes_per_pixel = 4;
    } else {
        *status = D3D11_BRIDGE_SURFACE_UNSUPPORTED;
        return false;
    }
    if (pg->surface_type != D3D11_BRIDGE_SURFACE_TYPE_PITCH ||
        pg->surface_shape.anti_aliasing != D3D11_BRIDGE_ANTIALIAS_CENTER_1 ||
        pg->surface_scale_factor != 1) {
        *status = D3D11_BRIDGE_SURFACE_UNSUPPORTED;
        return false;
    }
    const uint64_t minimum_pitch = (uint64_t)width * bytes_per_pixel;
    const uint64_t surface_bytes = (uint64_t)surface->pitch * height;
    if (minimum_pitch > UINT32_MAX || surface->pitch < minimum_pitch ||
        surface->pitch % bytes_per_pixel != 0 || surface_bytes == 0 ||
        surface->offset > UINT64_MAX - surface_bytes) {
        return false;
    }
    const uint64_t dma_address = color ? pg->dma_color : pg->dma_zeta;
    const uint64_t ramin_size = memory_region_size(&state->ramin);
    if ((dma_address & 3) != 0 || ramin_size < 3 * sizeof(uint32_t) ||
        dma_address > ramin_size - 3 * sizeof(uint32_t)) {
        *status = D3D11_BRIDGE_SURFACE_OUT_OF_RANGE;
        return false;
    }
    const DMAObject dma = nv_dma_load((NV2AState *)state, dma_address);
    if (dma.dma_class != NV_DMA_IN_MEMORY_CLASS ||
        dma.dma_target != NV_DMA_TARGET_NVM) {
        *status = D3D11_BRIDGE_SURFACE_UNSUPPORTED;
        return false;
    }
    if (dma.address > UINT64_C(0x07ffffff)) {
        *status = D3D11_BRIDGE_SURFACE_OUT_OF_RANGE;
        return false;
    }
    const uint64_t address = dma.address;
    if (address > UINT64_MAX - surface->offset || surface->offset > dma.limit ||
        surface_bytes == 0 || surface_bytes - 1 > dma.limit - surface->offset) {
        *status = dma.dma_class == NV_DMA_IN_MEMORY_CLASS &&
                          dma.dma_target == NV_DMA_TARGET_NVM ?
                      D3D11_BRIDGE_SURFACE_OUT_OF_RANGE :
                      D3D11_BRIDGE_SURFACE_UNSUPPORTED;
        return false;
    }
    const uint64_t absolute = address + surface->offset;
    const uint64_t vram_size = memory_region_size(state->vram);
    if (address >= vram_size || absolute > vram_size ||
        surface_bytes > vram_size - absolute) {
        *status = D3D11_BRIDGE_SURFACE_OUT_OF_RANGE;
        return false;
    }
    *out = (D3D11BridgeSurfaceDescriptor){ absolute, width, height,
                                           surface->pitch, format };
    *status = D3D11_BRIDGE_SURFACE_OK;
    return true;
}

bool d3d11_bridge_mark_surface_draw_dirty(NV2AState *state, bool color,
                                          bool value)
{
    if (state == NULL) {
        return false;
    }
    (color ? &state->pgraph.surface_color : &state->pgraph.surface_zeta)
        ->draw_dirty = value;
    return true;
}

bool d3d11_bridge_mark_vram_dirty(NV2AState *state, uint64_t offset,
                                  uint64_t size)
{
    if (state == NULL || state->vram == NULL || state->vram_ptr == NULL ||
        size == 0 || offset > memory_region_size(state->vram) ||
        size > memory_region_size(state->vram) - offset) {
        return false;
    }
    memory_region_set_client_dirty(state->vram, offset, size, DIRTY_MEMORY_VGA);
    memory_region_set_client_dirty(state->vram, offset, size,
                                   DIRTY_MEMORY_NV2A_TEX);
    return true;
}
