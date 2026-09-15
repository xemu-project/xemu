/*
 * Narrow C ABI between the QEMU/NV2A implementation and the D3D11 backend.
 *
 * This header is intentionally usable from a standalone Windows C++
 * translation unit.  Keep QEMU headers, MemoryRegion, DMAObject and NV097
 * definitions on the C side (d3d11_bridge.c); the D3D11 implementation only
 * consumes these fixed-width snapshots.
 */

#ifndef XEMU_NV2A_PGRAPH_D3D11_BRIDGE_H
#define XEMU_NV2A_PGRAPH_D3D11_BRIDGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NV2AState NV2AState;
typedef struct PGRAPHState PGRAPHState;

enum {
    D3D11_BRIDGE_VERTEX_ATTRIBUTES = 16,
    D3D11_BRIDGE_MAX_DRAW_ARRAY_RANGES = 1250,
    D3D11_BRIDGE_MAX_BATCH_WORDS = 0x07ffff,
    D3D11_BRIDGE_CLEAR_Z = 1,
    D3D11_BRIDGE_CLEAR_STENCIL = 2,
    D3D11_BRIDGE_CLEAR_COLOR = 0xf0,
    D3D11_BRIDGE_SURFACE_TYPE_PITCH = 1,
    D3D11_BRIDGE_ANTIALIAS_CENTER_1 = 0,
    D3D11_BRIDGE_ZETA_Z16 = 1,
    D3D11_BRIDGE_ZETA_Z24S8 = 2,
    D3D11_BRIDGE_COLOR_LE_A8R8G8B8 = 8,
    D3D11_BRIDGE_VERTEX_ATTR_POSITION = 0,
    D3D11_BRIDGE_VERTEX_ATTR_DIFFUSE = 3,
    D3D11_BRIDGE_VERTEX_FORMAT_F = 2,
    D3D11_BRIDGE_VERTEX_FORMAT_UB_D3D = 0,
    D3D11_BRIDGE_VERTEX_FORMAT_UB_OGL = 4,
    D3D11_BRIDGE_VERTEX_FORMAT_S1 = 1,
    D3D11_BRIDGE_VERTEX_FORMAT_S32K = 5,
    D3D11_BRIDGE_VERTEX_FORMAT_CMP = 6,
    D3D11_BRIDGE_MAX_PROGRAM_TOKENS = 136,
    D3D11_BRIDGE_VSH_TOKEN_WORDS = 4,
    D3D11_BRIDGE_VSH_CONSTANTS = 192,
};

typedef struct D3D11BridgeSurface {
    uint64_t offset;
    uint32_t pitch;
    uint8_t draw_dirty;
    uint8_t buffer_dirty;
    uint8_t write_enabled_cache;
    uint8_t reserved;
} D3D11BridgeSurface;

typedef struct D3D11BridgePgraphSnapshot {
    uint32_t primitive_mode;
    uint32_t surface_type;
    uint32_t surface_scale_factor;
    uint32_t anti_aliasing;
    uint32_t color_format;
    uint32_t zeta_format;
    uint32_t z_format;
    uint32_t clip_x;
    uint32_t clip_y;
    uint32_t clip_width;
    uint32_t clip_height;
    int32_t binding_clip_x;
    int32_t binding_clip_y;
    int32_t binding_clip_width;
    int32_t binding_clip_height;
    int32_t binding_width;
    int32_t binding_height;
    D3D11BridgeSurface color_surface;
    D3D11BridgeSurface zeta_surface;
    uint64_t dma_color;
    uint64_t dma_zeta;
    uint64_t dma_vertex_a;
    uint64_t dma_vertex_b;
    uint32_t inline_buffer_length;
    uint32_t inline_array_length;
    uint32_t inline_elements_length;
    uint32_t vsh_mode;
    uint32_t vsh_program_start;
    uint8_t vertex_program_write;
    uint8_t reserved_program[3];

    /* Decoded fixed-function state.  Values retain the NV2A enumerant
     * numbering, but are no longer read from a register array by C++. */
    uint8_t alpha_test_enable;
    uint8_t dither_enable;
    uint8_t z_enable;
    uint8_t z_write_enable;
    uint8_t stencil_write_enable;
    uint8_t red_write_enable;
    uint8_t green_write_enable;
    uint8_t blue_write_enable;
    uint8_t alpha_write_enable;
    uint8_t stencil_test_enable;
    uint8_t logic_op_enable;
    uint8_t blend_enable;
    uint8_t cull_enable;
    uint8_t front_face;
    uint8_t reserved_state[3];
    uint32_t front_face_mode;
    uint32_t back_face_mode;
    uint32_t z_func;
    uint32_t stencil_func;
    uint32_t stencil_read_mask;
    uint32_t stencil_write_mask;
    uint32_t stencil_ref;
    uint32_t stencil_op_fail;
    uint32_t stencil_op_zfail;
    uint32_t stencil_op_zpass;
    uint32_t blend_src_factor;
    uint32_t blend_dst_factor;
    uint32_t blend_equation;
    uint32_t blend_color;
    uint32_t cull_face;
    uint8_t polygon_offset_point_enable;
    uint8_t polygon_offset_line_enable;
    uint8_t polygon_offset_fill_enable;
    uint8_t polygon_smooth_enable;
    uint8_t window_clip_type;
    uint8_t antialiasing_enable;
    uint8_t reserved_raster[2];
    uint32_t window_clip_x_min[8];
    uint32_t window_clip_x_max[8];
    uint32_t window_clip_y_min[8];
    uint32_t window_clip_y_max[8];
    uint8_t window_clip_values_valid;
    uint8_t reserved_window_clip[3];

    uint32_t clear_rect_x_min;
    uint32_t clear_rect_x_max;
    uint32_t clear_rect_y_min;
    uint32_t clear_rect_y_max;
    uint32_t z_stencil_clear_value;
    float clear_color[4];
} D3D11BridgePgraphSnapshot;

typedef struct D3D11BridgeVertexAttribute {
    uint8_t dma_select;
    uint8_t needs_conversion;
    uint8_t inline_buffer_populated;
    uint8_t reserved;
    uint64_t offset;
    uint32_t inline_array_offset;
    float inline_value[4];
    uint32_t format;
    uint32_t size;
    uint32_t count;
    uint32_t stride;
} D3D11BridgeVertexAttribute;

typedef struct D3D11BridgeDrawArraysRange {
    int32_t start;
    int32_t count;
} D3D11BridgeDrawArraysRange;

typedef struct D3D11BridgeSurfaceDescriptor {
    uint64_t offset;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t format; /* 1 = BGR8A8, 2 = Z16, 3 = Z24S8 */
} D3D11BridgeSurfaceDescriptor;

enum {
    D3D11_BRIDGE_SURFACE_OK = 0,
    D3D11_BRIDGE_SURFACE_INVALID = 1,
    D3D11_BRIDGE_SURFACE_UNSUPPORTED = 2,
    D3D11_BRIDGE_SURFACE_OUT_OF_RANGE = 3,
};

/* Snapshot calls copy state synchronously and never retain a QEMU pointer. */
bool d3d11_bridge_snapshot_pgraph(const PGRAPHState *pg,
                                  D3D11BridgePgraphSnapshot *out);
const PGRAPHState *d3d11_bridge_pgraph(const NV2AState *state);
bool d3d11_bridge_snapshot_attributes(const PGRAPHState *pg,
                                      D3D11BridgeVertexAttribute *out,
                                      size_t capacity, size_t *count);
bool d3d11_bridge_copy_inline_array(const PGRAPHState *pg, uint32_t *out,
                                    size_t capacity, size_t *count);
bool d3d11_bridge_copy_inline_elements(const PGRAPHState *pg, uint32_t *out,
                                       size_t capacity, size_t *count);
bool d3d11_bridge_copy_draw_arrays(const PGRAPHState *pg,
                                   D3D11BridgeDrawArraysRange *out,
                                   size_t capacity, size_t *count);
bool d3d11_bridge_copy_inline_buffer_value(const PGRAPHState *pg,
                                           size_t attribute, size_t vertex,
                                           float out[4]);
bool d3d11_bridge_copy_program(const PGRAPHState *pg, uint32_t *out,
                               size_t capacity, size_t *token_count);
bool d3d11_bridge_copy_vsh_constants(const PGRAPHState *pg, uint32_t *out,
                                     size_t capacity, size_t *word_count);

/* Copy a DMA-backed vertex range into caller-owned storage after validating
 * RAMIN, the DMA object, VRAM bounds and integer overflow. */
bool d3d11_bridge_copy_dma(const NV2AState *state, uint64_t dma_object,
                           uint64_t offset, uint64_t size, uint8_t *out,
                           size_t capacity);
bool d3d11_bridge_vram_view(const NV2AState *state, uint8_t **data,
                            uint64_t *size);
bool d3d11_bridge_describe_surface(const NV2AState *state, bool color,
                                   uint32_t width, uint32_t height,
                                   D3D11BridgeSurfaceDescriptor *out,
                                   uint32_t *status);

bool d3d11_bridge_mark_surface_draw_dirty(NV2AState *state, bool color,
                                          bool value);
bool d3d11_bridge_mark_vram_dirty(NV2AState *state, uint64_t offset,
                                  uint64_t size);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* XEMU_NV2A_PGRAPH_D3D11_BRIDGE_H */
