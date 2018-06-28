/*
 * QEMU Geforce NV2A implementation
 *
 * Copyright (c) 2012 espes
 * Copyright (c) 2015 Jannik Vogel
 * Copyright (c) 2018 Matt Borgerson
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

#ifndef HW_NV2A_H
#define HW_NV2A_H

#include "hw/hw.h"
#include "hw/i386/pc.h"
#include "ui/console.h"
#include "hw/pci/pci.h"
#include "ui/console.h"
#include "hw/display/vga.h"
#include "hw/display/vga_int.h"
#include "qemu/thread.h"
#include "qapi/qmp/qstring.h"
#include "cpu.h"

#include "g-lru-cache.h"
#include "swizzle.h"
#include "nv2a_shaders.h"
#include "nv2a_debug.h"
#include "nv2a_int.h"

#include "gl/gloffscreen.h"
#include "gl/glextensions.h"

#define USE_TEXTURE_CACHE

#define GET_MASK(v, mask) (((v) & (mask)) >> (ffs(mask)-1))

#define SET_MASK(v, mask, val) ({                                    \
        const unsigned int __val = (val);                             \
        const unsigned int __mask = (mask);                          \
        (v) &= ~(__mask);                                            \
        (v) |= ((__val) << (ffs(__mask)-1)) & (__mask);              \
    })

#define CASE_4(v, step)                                              \
    case (v):                                                        \
    case (v)+(step):                                                 \
    case (v)+(step)*2:                                               \
    case (v)+(step)*3


#define NV2A_DEVICE(obj) \
    OBJECT_CHECK(NV2AState, (obj), "nv2a")

void reg_log_read(int block, hwaddr addr, uint64_t val);
void reg_log_write(int block, hwaddr addr, uint64_t val);

enum FifoMode {
    FIFO_PIO = 0,
    FIFO_DMA = 1,
};

enum FIFOEngine {
    ENGINE_SOFTWARE = 0,
    ENGINE_GRAPHICS = 1,
    ENGINE_DVD = 2,
};

typedef struct DMAObject {
    unsigned int dma_class;
    unsigned int dma_target;
    hwaddr address;
    hwaddr limit;
} DMAObject;

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
    uint8_t *converted_buffer;
    unsigned int converted_elements;
    unsigned int converted_size;
    unsigned int converted_count;

    float *inline_buffer;

    GLint gl_count;
    GLenum gl_type;
    GLboolean gl_normalize;

    GLuint gl_converted_buffer;
    GLuint gl_inline_buffer;
} VertexAttribute;

typedef struct Surface {
    bool draw_dirty;
    bool buffer_dirty;
    bool write_enabled_cache;
    unsigned int pitch;

    hwaddr offset;
} Surface;

typedef struct SurfaceShape {
    unsigned int z_format;
    unsigned int color_format;
    unsigned int zeta_format;
    unsigned int log_width, log_height;
    unsigned int clip_x, clip_y;
    unsigned int clip_width, clip_height;
    unsigned int anti_aliasing;
} SurfaceShape;

typedef struct TextureShape {
    bool cubemap;
    unsigned int dimensionality;
    unsigned int color_format;
    unsigned int levels;
    unsigned int width, height, depth;

    unsigned int min_mipmap_level, max_mipmap_level;
    unsigned int pitch;
} TextureShape;

typedef struct TextureKey {
    TextureShape state;
    uint64_t data_hash;
    uint8_t* texture_data;
    uint8_t* palette_data;
} TextureKey;

typedef struct TextureBinding {
    GLenum gl_target;
    GLuint gl_texture;
    unsigned int refcnt;
} TextureBinding;

typedef struct KelvinState {
    hwaddr dma_notifies;
    hwaddr dma_state;
    hwaddr dma_semaphore;
    unsigned int semaphore_offset;
} KelvinState;

typedef struct ContextSurfaces2DState {
    hwaddr dma_image_source;
    hwaddr dma_image_dest;
    unsigned int color_format;
    unsigned int source_pitch, dest_pitch;
    hwaddr source_offset, dest_offset;

} ContextSurfaces2DState;

typedef struct ImageBlitState {
    hwaddr context_surfaces;
    unsigned int operation;
    unsigned int in_x, in_y;
    unsigned int out_x, out_y;
    unsigned int width, height;

} ImageBlitState;

typedef struct GraphicsObject {
    uint8_t graphics_class;
    union {
        ContextSurfaces2DState context_surfaces_2d;

        ImageBlitState image_blit;

        KelvinState kelvin;
    } data;
} GraphicsObject;

typedef struct GraphicsSubchannel {
    hwaddr object_instance;
    GraphicsObject object;
    uint32_t object_cache[5];
} GraphicsSubchannel;

typedef struct GraphicsContext {
    bool channel_3d;
    unsigned int subchannel;
} GraphicsContext;


typedef struct PGRAPHState {
    QemuMutex lock;

    uint32_t pending_interrupts;
    uint32_t enabled_interrupts;
    QemuCond interrupt_cond;

    hwaddr context_table;
    hwaddr context_address;


    unsigned int trapped_method;
    unsigned int trapped_subchannel;
    unsigned int trapped_channel_id;
    uint32_t trapped_data[2];
    uint32_t notify_source;

    bool fifo_access;
    QemuCond fifo_access_cond;

    QemuCond flip_3d;

    unsigned int channel_id;
    bool channel_valid;
    GraphicsContext context[NV2A_NUM_CHANNELS];

    hwaddr dma_color, dma_zeta;
    Surface surface_color, surface_zeta;
    unsigned int surface_type;
    SurfaceShape surface_shape;
    SurfaceShape last_surface_shape;

    hwaddr dma_a, dma_b;
    GLruCache *texture_cache;
    bool texture_dirty[NV2A_MAX_TEXTURES];
    TextureBinding *texture_binding[NV2A_MAX_TEXTURES];

    GHashTable *shader_cache;
    ShaderBinding *shader_binding;

    bool texture_matrix_enable[NV2A_MAX_TEXTURES];

    /* FIXME: Move to NV_PGRAPH_BUMPMAT... */
    float bump_env_matrix[NV2A_MAX_TEXTURES-1][4]; /* 3 allowed stages with 2x2 matrix each */

    GloContext *gl_context;
    GLuint gl_framebuffer;
    GLuint gl_color_buffer, gl_zeta_buffer;
    GraphicsSubchannel subchannel_data[NV2A_NUM_SUBCHANNELS];

    hwaddr dma_report;
    hwaddr report_offset;
    bool zpass_pixel_count_enable;
    unsigned int zpass_pixel_count_result;
    unsigned int gl_zpass_pixel_count_query_count;
    GLuint* gl_zpass_pixel_count_queries;

    hwaddr dma_vertex_a, dma_vertex_b;

    unsigned int primitive_mode;

    bool enable_vertex_program_write;

    uint32_t program_data[NV2A_MAX_TRANSFORM_PROGRAM_LENGTH][VSH_TOKEN_SIZE];

    uint32_t vsh_constants[NV2A_VERTEXSHADER_CONSTANTS][4];
    bool vsh_constants_dirty[NV2A_VERTEXSHADER_CONSTANTS];

    /* lighting constant arrays */
    uint32_t ltctxa[NV2A_LTCTXA_COUNT][4];
    bool ltctxa_dirty[NV2A_LTCTXA_COUNT];
    uint32_t ltctxb[NV2A_LTCTXB_COUNT][4];
    bool ltctxb_dirty[NV2A_LTCTXB_COUNT];
    uint32_t ltc1[NV2A_LTC1_COUNT][4];
    bool ltc1_dirty[NV2A_LTC1_COUNT];

    // should figure out where these are in lighting context
    float light_infinite_half_vector[NV2A_MAX_LIGHTS][3];
    float light_infinite_direction[NV2A_MAX_LIGHTS][3];
    float light_local_position[NV2A_MAX_LIGHTS][3];
    float light_local_attenuation[NV2A_MAX_LIGHTS][3];

    VertexAttribute vertex_attributes[NV2A_VERTEXSHADER_ATTRIBUTES];

    unsigned int inline_array_length;
    uint32_t inline_array[NV2A_MAX_BATCH_LENGTH];
    GLuint gl_inline_array_buffer;

    unsigned int inline_elements_length;
    uint32_t inline_elements[NV2A_MAX_BATCH_LENGTH];

    unsigned int inline_buffer_length;

    unsigned int draw_arrays_length;
    unsigned int draw_arrays_max_count;
    /* FIXME: Unknown size, possibly endless, 1000 will do for now */
    GLint gl_draw_arrays_start[1000];
    GLsizei gl_draw_arrays_count[1000];

    GLuint gl_element_buffer;
    GLuint gl_memory_buffer;
    GLuint gl_vertex_array;

    uint32_t regs[0x2000];
} PGRAPHState;


typedef struct CacheEntry {
    QSIMPLEQ_ENTRY(CacheEntry) entry;
    unsigned int method : 14;
    unsigned int subchannel : 3;
    bool nonincreasing;
    uint32_t parameter;
} CacheEntry;

typedef struct Cache1State {
    unsigned int channel_id;
    enum FifoMode mode;

    /* Pusher state */
    bool push_enabled;
    bool dma_push_enabled;
    bool dma_push_suspended;
    hwaddr dma_instance;

    bool method_nonincreasing;
    unsigned int method : 14;
    unsigned int subchannel : 3;
    unsigned int method_count : 24;
    uint32_t dcount;
    bool subroutine_active;
    hwaddr subroutine_return;
    hwaddr get_jmp_shadow;
    uint32_t rsvd_shadow;
    uint32_t data_shadow;
    uint32_t error;

    bool pull_enabled;
    enum FIFOEngine bound_engines[NV2A_NUM_SUBCHANNELS];
    enum FIFOEngine last_engine;

    /* The actual command queue */
    QemuSpin alloc_lock;
    QemuMutex cache_lock;
    QemuCond cache_cond;
    QSIMPLEQ_HEAD(, CacheEntry) cache;
    QSIMPLEQ_HEAD(, CacheEntry) working_cache;
    QSIMPLEQ_HEAD(, CacheEntry) available_entries;
    QSIMPLEQ_HEAD(, CacheEntry) retired_entries;
} Cache1State;

typedef struct ChannelControl {
    hwaddr dma_put;
    hwaddr dma_get;
    uint32_t ref;
} ChannelControl;

typedef struct NV2AState {
    PCIDevice dev;
    qemu_irq irq;
    bool exiting;

    VGACommonState vga;
    GraphicHwOps hw_ops;
    QEMUTimer *vblank_timer;

    MemoryRegion *vram;
    MemoryRegion vram_pci;
    uint8_t *vram_ptr;
    MemoryRegion ramin;
    uint8_t *ramin_ptr;

    MemoryRegion mmio;
    MemoryRegion block_mmio[NV_NUM_BLOCKS];

    struct {
        uint32_t pending_interrupts;
        uint32_t enabled_interrupts;
    } pmc;

    struct {
        QemuThread puller_thread;
        uint32_t pending_interrupts;
        uint32_t enabled_interrupts;
        Cache1State cache1;
        uint32_t regs[0x2000];
    } pfifo;

    struct {
        uint32_t regs[0x1000];
    } pvideo;

    struct {
        uint32_t pending_interrupts;
        uint32_t enabled_interrupts;
        uint32_t numerator;
        uint32_t denominator;
        uint32_t alarm_time;
    } ptimer;

    struct {
        uint32_t regs[0x1000];
    } pfb;

    struct PGRAPHState pgraph;

    struct {
        uint32_t pending_interrupts;
        uint32_t enabled_interrupts;
        hwaddr start;
    } pcrtc;

    struct {
        uint32_t core_clock_coeff;
        uint64_t core_clock_freq;
        uint32_t memory_clock_coeff;
        uint32_t video_clock_coeff;
    } pramdac;

    struct {
        ChannelControl channel_control[NV2A_NUM_CHANNELS];
    } user;

} NV2AState;

typedef struct NV2ABlockInfo {
    const char* name;
    hwaddr offset;
    uint64_t size;
    MemoryRegionOps ops;
} NV2ABlockInfo;

extern const struct NV2ABlockInfo blocktable[];
extern const int blocktable_len;

void pgraph_init(NV2AState *d);
void *pfifo_puller_thread(void *opaque);
void pgraph_destroy(PGRAPHState *pg);
void update_irq(NV2AState *d);

#endif
