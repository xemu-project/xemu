/*
 * QEMU Geforce NV2A PGRAPH internal definitions
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

#ifndef HW_XBOX_NV2A_PGRAPH_H
#define HW_XBOX_NV2A_PGRAPH_H

#include "xemu-config.h"
#include "qemu/osdep.h"
#include "qemu/bitmap.h"
#include "qemu/units.h"
#include "qemu/thread.h"
#include "cpu.h"

#include "surface.h"
#include "texture.h"
#include "util.h"
#include "vsh_regs.h"

typedef struct NV2AState NV2AState;
typedef struct PGRAPHNullState PGRAPHNullState;
typedef struct PGRAPHGLState PGRAPHGLState;
typedef struct PGRAPHVkState PGRAPHVkState;

typedef struct VertexAttribute {
    bool dma_select;
    hwaddr offset;

    /* inline arrays are packed in order?
     * Need to pass the offset to converted attributes */
    unsigned int inline_array_offset;

    float inline_value[4];

    unsigned int format;
    unsigned int size; /* size of the data type */
    unsigned int count; /* number of components */
    uint32_t stride;

    bool needs_conversion;

    float *inline_buffer;
    bool inline_buffer_populated;
} VertexAttribute;

typedef struct Surface {
    bool draw_dirty;
    bool buffer_dirty;
    bool write_enabled_cache;
    unsigned int pitch;

    hwaddr offset;
} Surface;

typedef struct KelvinState {
    hwaddr object_instance;
} KelvinState;

typedef struct ContextSurfaces2DState {
    hwaddr object_instance;
    hwaddr dma_image_source;
    hwaddr dma_image_dest;
    unsigned int color_format;
    unsigned int source_pitch, dest_pitch;
    hwaddr source_offset, dest_offset;
} ContextSurfaces2DState;

typedef struct ImageBlitState {
    hwaddr object_instance;
    hwaddr context_surfaces;
    unsigned int operation;
    unsigned int in_x, in_y;
    unsigned int out_x, out_y;
    unsigned int width, height;
} ImageBlitState;

typedef struct BetaState {
  hwaddr object_instance;
  uint32_t beta;
} BetaState;

typedef struct GPUProperties {
    struct {
        short tri;
        short tri_strip0;
        short tri_strip1;
        short tri_fan;
    } geom_shader_winding;
} GPUProperties;

typedef struct PGRAPHRenderer {
    CONFIG_DISPLAY_RENDERER type;
    const char *name;
    struct {
        void (*early_context_init)(void);
        void (*init)(NV2AState *d, Error **errp);
        void (*finalize)(NV2AState *d);
        void (*clear_report_value)(NV2AState *d);
        void (*clear_surface)(NV2AState *d, uint32_t parameter);
        void (*draw_begin)(NV2AState *d);
        void (*draw_end)(NV2AState *d);
        void (*flip_stall)(NV2AState *d);
        void (*flush_draw)(NV2AState *d);
        void (*get_report)(NV2AState *d, uint32_t parameter);
        void (*image_blit)(NV2AState *d);
        void (*pre_savevm_trigger)(NV2AState *d);
        void (*pre_savevm_wait)(NV2AState *d);
        void (*pre_shutdown_trigger)(NV2AState *d);
        void (*pre_shutdown_wait)(NV2AState *d);
        void (*process_pending)(NV2AState *d);
        void (*process_pending_reports)(NV2AState *d);
        void (*surface_flush)(NV2AState *d);
        void (*surface_update)(NV2AState *d, bool upload, bool color_write, bool zeta_write);
        void (*set_surface_scale_factor)(NV2AState *d, unsigned int scale);
        unsigned int (*get_surface_scale_factor)(NV2AState *d);
        int (*get_framebuffer_surface)(NV2AState *d);
        GPUProperties *(*get_gpu_properties)(void);
    } ops;
} PGRAPHRenderer;

typedef struct PGRAPHState {
    QemuMutex lock;
    QemuMutex renderer_lock;

    uint32_t pending_interrupts;
    uint32_t enabled_interrupts;

    int frame_time;
    int draw_time;

    /* subchannels state we're not sure the location of... */
    ContextSurfaces2DState context_surfaces_2d;
    ImageBlitState image_blit;
    KelvinState kelvin;
    BetaState beta;

    hwaddr dma_color, dma_zeta;
    Surface surface_color, surface_zeta;
    unsigned int surface_type;
    SurfaceShape surface_shape;
    SurfaceShape last_surface_shape;

    struct {
        int clip_x;
        int clip_width;
        int clip_y;
        int clip_height;
        int width;
        int height;
    } surface_binding_dim; // FIXME: Refactor

    hwaddr dma_a, dma_b;
    bool texture_dirty[NV2A_MAX_TEXTURES];

    bool texture_matrix_enable[NV2A_MAX_TEXTURES];

    hwaddr dma_state;
    hwaddr dma_notifies;
    hwaddr dma_semaphore;

    hwaddr dma_report;
    hwaddr report_offset;
    bool zpass_pixel_count_enable;

    hwaddr dma_vertex_a, dma_vertex_b;

    uint32_t primitive_mode;

    bool enable_vertex_program_write; // FIXME: Not used anywhere???

    uint32_t vertex_state_shader_v0[4];
    uint32_t program_data[NV2A_MAX_TRANSFORM_PROGRAM_LENGTH][VSH_TOKEN_SIZE];
    bool program_data_dirty;

    uint32_t vsh_constants[NV2A_VERTEXSHADER_CONSTANTS][4];
    bool vsh_constants_dirty[NV2A_VERTEXSHADER_CONSTANTS];

    /* lighting constant arrays */
    uint32_t ltctxa[NV2A_LTCTXA_COUNT][4];
    bool ltctxa_dirty[NV2A_LTCTXA_COUNT];
    uint32_t ltctxb[NV2A_LTCTXB_COUNT][4];
    bool ltctxb_dirty[NV2A_LTCTXB_COUNT];
    uint32_t ltc1[NV2A_LTC1_COUNT][4];
    bool ltc1_dirty[NV2A_LTC1_COUNT];

    float material_alpha;

    // should figure out where these are in lighting context
    float light_infinite_half_vector[NV2A_MAX_LIGHTS][3];
    float light_infinite_direction[NV2A_MAX_LIGHTS][3];
    float light_local_position[NV2A_MAX_LIGHTS][3];
    float light_local_attenuation[NV2A_MAX_LIGHTS][3];

    float specular_params[6];
    float specular_power;
    float specular_params_back[6];
    float specular_power_back;

    float point_params[8];

    VertexAttribute vertex_attributes[NV2A_VERTEXSHADER_ATTRIBUTES];
    uint16_t compressed_attrs;
    uint16_t uniform_attrs;
    uint16_t swizzle_attrs;

    unsigned int inline_array_length;
    uint32_t inline_array[NV2A_MAX_BATCH_LENGTH];

    unsigned int inline_elements_length;
    uint32_t inline_elements[NV2A_MAX_BATCH_LENGTH];

    unsigned int inline_buffer_length;

    unsigned int draw_arrays_length;
    unsigned int draw_arrays_min_start;
    unsigned int draw_arrays_max_count;
    /* FIXME: Unknown size, possibly endless, 1250 will do for now */
    /* Keep in sync with size used in nv2a.c */
    int32_t draw_arrays_start[1250];
    int32_t draw_arrays_count[1250];
    bool draw_arrays_prevent_connect;

    uint32_t regs_[0x2000];
    DECLARE_BITMAP(regs_dirty, 0x2000 / sizeof(uint32_t));

    bool clearing; // FIXME: Internal
    bool waiting_for_nop;
    bool waiting_for_flip;
    bool waiting_for_context_switch;

    bool flush_pending;
    QemuEvent flush_complete;

    bool sync_pending;
    QemuEvent sync_complete;

    bool framebuffer_in_use;
    QemuCond framebuffer_released;

    enum {
        PGRAPH_RENDERER_SWITCH_PHASE_IDLE,
        PGRAPH_RENDERER_SWITCH_PHASE_STARTED,
        PGRAPH_RENDERER_SWITCH_PHASE_CPU_WAITING,
    } renderer_switch_phase;
    QemuEvent renderer_switch_complete;

    unsigned int surface_scale_factor;
    uint8_t *scale_buf;

    const PGRAPHRenderer *renderer;
    union {
        PGRAPHNullState *null_renderer_state;
        PGRAPHGLState *gl_renderer_state;
        PGRAPHVkState *vk_renderer_state;
    };
} PGRAPHState;

void pgraph_init(NV2AState *d);
void pgraph_init_thread(NV2AState *d);
void pgraph_destroy(PGRAPHState *pg);
void pgraph_context_switch(NV2AState *d, unsigned int channel_id);
void pgraph_process_pending(NV2AState *d);
void pgraph_process_pending_reports(NV2AState *d);
void pgraph_pre_savevm_trigger(NV2AState *d);
void pgraph_pre_savevm_wait(NV2AState *d);
void pgraph_pre_shutdown_trigger(NV2AState *d);
void pgraph_pre_shutdown_wait(NV2AState *d);

int pgraph_method(NV2AState *d, unsigned int subchannel, unsigned int method,
                  uint32_t parameter, uint32_t *parameters,
                  size_t num_words_available, size_t max_lookahead_words,
                  bool inc);
void pgraph_check_within_begin_end_block(PGRAPHState *pg);

void *pfifo_thread(void *arg);
void pfifo_kick(NV2AState *d);

void pgraph_renderer_register(const PGRAPHRenderer *renderer);

// FIXME: Move from here

extern NV2AState *g_nv2a;

// FIXME: Add new function pgraph_is_texture_sampler_active()

static inline uint32_t pgraph_reg_r(PGRAPHState *pg, unsigned int r)
{
    assert(r % 4 == 0);
    return pg->regs_[r];
}

static inline void pgraph_reg_w(PGRAPHState *pg, unsigned int r, uint32_t v)
{
    assert(r % 4 == 0);
    if (pg->regs_[r] != v) {
        bitmap_set(pg->regs_dirty, r / sizeof(uint32_t), 1);
    }
    pg->regs_[r] = v;
}

void pgraph_clear_dirty_reg_map(PGRAPHState *pg);

static inline bool pgraph_is_reg_dirty(PGRAPHState *pg, unsigned int reg)
{
    return test_bit(reg / sizeof(uint32_t), pg->regs_dirty);
}

static inline bool pgraph_is_texture_stage_active(PGRAPHState *pg, unsigned int stage)
{
    assert(stage < NV2A_MAX_TEXTURES);
    uint32_t mode = (pgraph_reg_r(pg, NV_PGRAPH_SHADERPROG) >> (stage * 5)) & 0x1F;
    return mode != 0 && mode != 4;// && mode != 0x11 && mode != 0x0a && mode != 0x09 && mode != 5;
}

static inline bool pgraph_is_texture_enabled(PGRAPHState *pg, int texture_idx)
{
    uint32_t ctl_0 = pgraph_reg_r(pg, NV_PGRAPH_TEXCTL0_0 + texture_idx*4);
    return // pgraph_is_texture_stage_active(pg, texture_idx) &&
                       GET_MASK(ctl_0, NV_PGRAPH_TEXCTL0_0_ENABLE);
}

static inline bool pgraph_is_texture_format_compressed(PGRAPHState *pg, int color_format)
{
    return color_format == NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT1_A1R5G5B5 ||
           color_format == NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT23_A8R8G8B8 ||
           color_format == NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT45_A8R8G8B8;
}

static inline bool pgraph_color_write_enabled(PGRAPHState *pg)
{
    return pgraph_reg_r(pg, NV_PGRAPH_CONTROL_0) & (
        NV_PGRAPH_CONTROL_0_ALPHA_WRITE_ENABLE
        | NV_PGRAPH_CONTROL_0_RED_WRITE_ENABLE
        | NV_PGRAPH_CONTROL_0_GREEN_WRITE_ENABLE
        | NV_PGRAPH_CONTROL_0_BLUE_WRITE_ENABLE);
}

static inline bool pgraph_zeta_write_enabled(PGRAPHState *pg)
{
    return pgraph_reg_r(pg, NV_PGRAPH_CONTROL_0) & (
        NV_PGRAPH_CONTROL_0_ZWRITEENABLE
        | NV_PGRAPH_CONTROL_0_STENCIL_WRITE_ENABLE);
}

static inline void pgraph_apply_anti_aliasing_factor(PGRAPHState *pg,
                                              unsigned int *width,
                                              unsigned int *height)
{
    switch (pg->surface_shape.anti_aliasing) {
    case NV097_SET_SURFACE_FORMAT_ANTI_ALIASING_CENTER_1:
        break;
    case NV097_SET_SURFACE_FORMAT_ANTI_ALIASING_CENTER_CORNER_2:
        if (width) { *width *= 2; }
        break;
    case NV097_SET_SURFACE_FORMAT_ANTI_ALIASING_SQUARE_OFFSET_4:
        if (width) { *width *= 2; }
        if (height) { *height *= 2; }
        break;
    default:
        assert(false);
        break;
    }
}

static inline void pgraph_apply_scaling_factor(PGRAPHState *pg,
                                        unsigned int *width,
                                        unsigned int *height)
{
    *width *= pg->surface_scale_factor;
    *height *= pg->surface_scale_factor;
}

void pgraph_get_clear_color(PGRAPHState *pg, float rgba[4]);
void pgraph_get_clear_depth_stencil_value(PGRAPHState *pg, float *depth, int *stencil);

/* Vertex */
void pgraph_allocate_inline_buffer_vertices(PGRAPHState *pg, unsigned int attr);
void pgraph_finish_inline_buffer_vertex(PGRAPHState *pg);
void pgraph_reset_inline_buffers(PGRAPHState *pg);
void pgraph_reset_draw_arrays(PGRAPHState *pg);
void pgraph_update_inline_value(VertexAttribute *attr, const uint8_t *data);
void pgraph_get_inline_values(PGRAPHState *pg, uint16_t attrs,
                               float values[NV2A_VERTEXSHADER_ATTRIBUTES][4],
                               int *count);

/* RDI */
uint32_t pgraph_rdi_read(PGRAPHState *pg, unsigned int select,
                         unsigned int address);
void pgraph_rdi_write(PGRAPHState *pg, unsigned int select,
                      unsigned int address, uint32_t val);

static inline void pgraph_argb_pack32_to_rgba_float(uint32_t argb, float *rgba)
{
    rgba[0] = ((argb >> 16) & 0xFF) / 255.0f; /* red */
    rgba[1] = ((argb >> 8) & 0xFF) / 255.0f; /* green */
    rgba[2] = (argb & 0xFF) / 255.0f; /* blue */
    rgba[3] = ((argb >> 24) & 0xFF) / 255.0f; /* alpha */
}

void pgraph_write_zpass_pixel_cnt_report(NV2AState *d, uint32_t parameter, uint32_t result);

#endif
