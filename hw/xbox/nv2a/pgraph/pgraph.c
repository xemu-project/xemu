/*
 * QEMU Geforce NV2A implementation
 *
 * Copyright (c) 2012 espes
 * Copyright (c) 2015 Jannik Vogel
 * Copyright (c) 2018-2025 Matt Borgerson
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

#include <math.h>

#include "hw/xbox/nv2a/nv2a_int.h"
#include "ui/xemu-notifications.h"
#include "ui/xemu-settings.h"
#include "util.h"
#include "swizzle.h"
#include "nv2a_vsh_emulator.h"

#define PG_GET_MASK(reg, mask) GET_MASK(pgraph_reg_r(pg, reg), mask)
#define PG_SET_MASK(reg, mask, value)        \
    do {                                     \
        uint32_t rv = pgraph_reg_r(pg, reg); \
        SET_MASK(rv, mask, value);           \
        pgraph_reg_w(pg, reg, rv);           \
    } while (0)


NV2AState *g_nv2a;

uint64_t pgraph_read(void *opaque, hwaddr addr, unsigned int size)
{
    NV2AState *d = (NV2AState *)opaque;
    PGRAPHState *pg = &d->pgraph;

    qemu_mutex_lock(&pg->lock);

    uint64_t r = 0;
    switch (addr) {
    case NV_PGRAPH_INTR:
        r = pg->pending_interrupts;
        break;
    case NV_PGRAPH_INTR_EN:
        r = pg->enabled_interrupts;
        break;
    case NV_PGRAPH_RDI_DATA: {
        unsigned int select = PG_GET_MASK(NV_PGRAPH_RDI_INDEX,
                                       NV_PGRAPH_RDI_INDEX_SELECT);
        unsigned int address = PG_GET_MASK(NV_PGRAPH_RDI_INDEX,
                                        NV_PGRAPH_RDI_INDEX_ADDRESS);

        r = pgraph_rdi_read(pg, select, address);

        /* FIXME: Overflow into select? */
        assert(address < GET_MASK(NV_PGRAPH_RDI_INDEX_ADDRESS,
                                  NV_PGRAPH_RDI_INDEX_ADDRESS));
        PG_SET_MASK(NV_PGRAPH_RDI_INDEX,
                 NV_PGRAPH_RDI_INDEX_ADDRESS, address + 1);
        break;
    }
    default:
        r = pgraph_reg_r(pg, addr);
        break;
    }

    qemu_mutex_unlock(&pg->lock);

    nv2a_reg_log_read(NV_PGRAPH, addr, size, r);
    return r;
}

void pgraph_write(void *opaque, hwaddr addr, uint64_t val, unsigned int size)
{
    NV2AState *d = (NV2AState *)opaque;
    PGRAPHState *pg = &d->pgraph;

    nv2a_reg_log_write(NV_PGRAPH, addr, size, val);

    qemu_mutex_lock(&d->pfifo.lock); // FIXME: Factor out fifo lock here
    qemu_mutex_lock(&pg->lock);

    switch (addr) {
    case NV_PGRAPH_INTR:
        pg->pending_interrupts &= ~val;

        if (!(pg->pending_interrupts & NV_PGRAPH_INTR_ERROR)) {
            pg->waiting_for_nop = false;
        }
        if (!(pg->pending_interrupts & NV_PGRAPH_INTR_CONTEXT_SWITCH)) {
            pg->waiting_for_context_switch = false;
        }
        pfifo_kick(d);
        break;
    case NV_PGRAPH_INTR_EN:
        pg->enabled_interrupts = val;
        break;
    case NV_PGRAPH_INCREMENT:
        if (val & NV_PGRAPH_INCREMENT_READ_3D) {
            PG_SET_MASK(NV_PGRAPH_SURFACE,
                     NV_PGRAPH_SURFACE_READ_3D,
                     (PG_GET_MASK(NV_PGRAPH_SURFACE,
                              NV_PGRAPH_SURFACE_READ_3D)+1)
                        % PG_GET_MASK(NV_PGRAPH_SURFACE,
                                   NV_PGRAPH_SURFACE_MODULO_3D) );
            nv2a_profile_increment();
            pfifo_kick(d);
        }
        break;
    case NV_PGRAPH_RDI_DATA: {
        unsigned int select = PG_GET_MASK(NV_PGRAPH_RDI_INDEX,
                                       NV_PGRAPH_RDI_INDEX_SELECT);
        unsigned int address = PG_GET_MASK(NV_PGRAPH_RDI_INDEX,
                                        NV_PGRAPH_RDI_INDEX_ADDRESS);

        pgraph_rdi_write(pg, select, address, val);

        /* FIXME: Overflow into select? */
        assert(address < GET_MASK(NV_PGRAPH_RDI_INDEX_ADDRESS,
                                  NV_PGRAPH_RDI_INDEX_ADDRESS));
        PG_SET_MASK(NV_PGRAPH_RDI_INDEX,
                 NV_PGRAPH_RDI_INDEX_ADDRESS, address + 1);
        break;
    }
    case NV_PGRAPH_CHANNEL_CTX_TRIGGER: {
        hwaddr context_address =
            PG_GET_MASK(NV_PGRAPH_CHANNEL_CTX_POINTER,
                     NV_PGRAPH_CHANNEL_CTX_POINTER_INST) << 4;

        if (val & NV_PGRAPH_CHANNEL_CTX_TRIGGER_READ_IN) {
#if DEBUG_NV2A
            unsigned pgraph_channel_id =
                PG_GET_MASK(NV_PGRAPH_CTX_USER, NV_PGRAPH_CTX_USER_CHID);
#endif
            NV2A_DPRINTF("PGRAPH: read channel %d context from %" HWADDR_PRIx "\n",
                         pgraph_channel_id, context_address);

            assert(context_address < memory_region_size(&d->ramin));

            uint8_t *context_ptr = d->ramin_ptr + context_address;
            uint32_t context_user = ldl_le_p((uint32_t*)context_ptr);

            NV2A_DPRINTF("    - CTX_USER = 0x%x\n", context_user);

            pgraph_reg_w(pg, NV_PGRAPH_CTX_USER, context_user);
            // pgraph_set_context_user(d, context_user);
        }
        if (val & NV_PGRAPH_CHANNEL_CTX_TRIGGER_WRITE_OUT) {
            /* do stuff ... */
        }

        break;
    }
    default:
        pgraph_reg_w(pg, addr, val);
        break;
    }

    // events
    switch (addr) {
    case NV_PGRAPH_FIFO:
        pfifo_kick(d);
        break;
    }

    qemu_mutex_unlock(&pg->lock);
    qemu_mutex_unlock(&d->pfifo.lock);
}

void pgraph_context_switch(NV2AState *d, unsigned int channel_id)
{
    PGRAPHState *pg = &d->pgraph;

    bool channel_valid =
        pgraph_reg_r(pg, NV_PGRAPH_CTX_CONTROL) & NV_PGRAPH_CTX_CONTROL_CHID;
    unsigned pgraph_channel_id =
        PG_GET_MASK(NV_PGRAPH_CTX_USER, NV_PGRAPH_CTX_USER_CHID);

    bool valid = channel_valid && pgraph_channel_id == channel_id;
    if (!valid) {
        PG_SET_MASK(NV_PGRAPH_TRAPPED_ADDR,
                 NV_PGRAPH_TRAPPED_ADDR_CHID, channel_id);

        NV2A_DPRINTF("pgraph switching to ch %d\n", channel_id);

        /* TODO: hardware context switching */
        assert(!PG_GET_MASK(NV_PGRAPH_DEBUG_3,
                            NV_PGRAPH_DEBUG_3_HW_CONTEXT_SWITCH));

        pg->waiting_for_context_switch = true;
        qemu_mutex_unlock(&pg->lock);
        bql_lock();
        pg->pending_interrupts |= NV_PGRAPH_INTR_CONTEXT_SWITCH;
        nv2a_update_irq(d);
        bql_unlock();
        qemu_mutex_lock(&pg->lock);
    }
}

static const PGRAPHRenderer *renderers[CONFIG_DISPLAY_RENDERER__COUNT];

void pgraph_renderer_register(const PGRAPHRenderer *renderer)
{
    assert(renderer->type < CONFIG_DISPLAY_RENDERER__COUNT);
    renderers[renderer->type] = renderer;
}

void pgraph_init(NV2AState *d)
{
    g_nv2a = d;

    PGRAPHState *pg = &d->pgraph;
    qemu_mutex_init(&pg->lock);
    qemu_mutex_init(&pg->renderer_lock);
    qemu_event_init(&pg->sync_complete, false);
    qemu_event_init(&pg->flush_complete, false);
    qemu_cond_init(&pg->framebuffer_released);
    qemu_event_init(&pg->renderer_switch_complete, false);
    pg->renderer_switch_phase = PGRAPH_RENDERER_SWITCH_PHASE_IDLE;

    pg->frame_time = 0;
    pg->draw_time = 0;

    pg->material_alpha = 0.0f;
    PG_SET_MASK(NV_PGRAPH_CONTROL_3, NV_PGRAPH_CONTROL_3_SHADEMODE,
         NV_PGRAPH_CONTROL_3_SHADEMODE_SMOOTH);
    pg->primitive_mode = PRIM_TYPE_INVALID;

    for (int i = 0; i < NV2A_VERTEXSHADER_ATTRIBUTES; i++) {
        VertexAttribute *attribute = &pg->vertex_attributes[i];
        attribute->inline_buffer = (float*)g_malloc(NV2A_MAX_BATCH_LENGTH
                                              * sizeof(float) * 4);
        attribute->inline_buffer_populated = false;
    }

    pgraph_clear_dirty_reg_map(pg);
}

void pgraph_clear_dirty_reg_map(PGRAPHState *pg)
{
    memset(pg->regs_dirty, 0, sizeof(pg->regs_dirty));
}

static CONFIG_DISPLAY_RENDERER get_default_renderer(void)
{
#ifdef CONFIG_OPENGL
    if (renderers[CONFIG_DISPLAY_RENDERER_OPENGL]) {
        return CONFIG_DISPLAY_RENDERER_OPENGL;
    }
#endif
#ifdef CONFIG_VULKAN
    if (renderers[CONFIG_DISPLAY_RENDERER_VULKAN]) {
        return CONFIG_DISPLAY_RENDERER_VULKAN;
    }
#endif
    fprintf(stderr, "Warning: No available renderer\n");
    return CONFIG_DISPLAY_RENDERER_NULL;
}

void nv2a_context_init(void)
{
    if (!renderers[g_config.display.renderer]) {
        g_config.display.renderer = get_default_renderer();
        fprintf(stderr,
                "Warning: Configured renderer unavailable. Switching to %s.\n",
                renderers[g_config.display.renderer]->name);
    }

    // FIXME: We need a mechanism for renderer to initialize new GL contexts
    //        on the main thread at run time. For now, just let them all create
    //        what they need.
    for (int i = 0; i < ARRAY_SIZE(renderers); i++) {
        const PGRAPHRenderer *r = renderers[i];
        if (!r) {
            continue;
        }
        if (r->ops.early_context_init) {
            r->ops.early_context_init();
        }
    }
}

static bool attempt_renderer_init(PGRAPHState *pg)
{
    NV2AState *d = container_of(pg, NV2AState, pgraph);

    pg->renderer = renderers[g_config.display.renderer];
    if (!pg->renderer) {
        xemu_queue_error_message("Configured renderer not available");
        return false;
    }

    Error *local_err = NULL;
    if (pg->renderer->ops.init) {
        pg->renderer->ops.init(d, &local_err);
    }
    if (local_err) {
        const char *msg = error_get_pretty(local_err);
        xemu_queue_error_message(msg);
        error_free(local_err);
        local_err = NULL;
        return false;
    }

    return true;
}

static void init_renderer(PGRAPHState *pg)
{
    if (attempt_renderer_init(pg)) {
        return;  // Success
    }

    CONFIG_DISPLAY_RENDERER default_renderer = get_default_renderer();
    if (default_renderer != g_config.display.renderer) {
        g_config.display.renderer = default_renderer;
        if (attempt_renderer_init(pg)) {
            g_autofree gchar *msg = g_strdup_printf(
                "Switched to default renderer: %s", pg->renderer->name);
            xemu_queue_notification(msg);
            return;
        }
    }

    // FIXME: Try others

    fprintf(stderr, "Fatal error: cannot initialize renderer\n");
    exit(1);
}

void pgraph_init_thread(NV2AState *d)
{
    init_renderer(&d->pgraph);
}

void pgraph_destroy(PGRAPHState *pg)
{
    NV2AState *d = container_of(pg, NV2AState, pgraph);

    if (pg->renderer->ops.finalize) {
       pg->renderer->ops.finalize(d);
    }

    qemu_mutex_destroy(&pg->lock);
}

int nv2a_get_framebuffer_surface(void)
{
    NV2AState *d = g_nv2a;
    PGRAPHState *pg = &d->pgraph;
    int s = 0;

    qemu_mutex_lock(&pg->renderer_lock);
    assert(!pg->framebuffer_in_use);
    pg->framebuffer_in_use = true;
    if (pg->renderer->ops.get_framebuffer_surface) {
        s = pg->renderer->ops.get_framebuffer_surface(d);
    }
    qemu_mutex_unlock(&pg->renderer_lock);

    return s;
}

void nv2a_release_framebuffer_surface(void)
{
    NV2AState *d = g_nv2a;
    PGRAPHState *pg = &d->pgraph;
    qemu_mutex_lock(&pg->renderer_lock);
    pg->framebuffer_in_use = false;
    qemu_cond_broadcast(&pg->framebuffer_released);
    qemu_mutex_unlock(&pg->renderer_lock);
}

void nv2a_set_surface_scale_factor(unsigned int scale)
{
    NV2AState *d = g_nv2a;

    bql_unlock();
    qemu_mutex_lock(&d->pgraph.renderer_lock);
    if (d->pgraph.renderer->ops.set_surface_scale_factor) {
        d->pgraph.renderer->ops.set_surface_scale_factor(d, scale);
    }
    qemu_mutex_unlock(&d->pgraph.renderer_lock);
    bql_lock();
}

unsigned int nv2a_get_surface_scale_factor(void)
{
    NV2AState *d = g_nv2a;
    int s = 1;

    bql_unlock();
    qemu_mutex_lock(&d->pgraph.renderer_lock);
    if (d->pgraph.renderer->ops.get_surface_scale_factor) {
        s = d->pgraph.renderer->ops.get_surface_scale_factor(d);
    }
    qemu_mutex_unlock(&d->pgraph.renderer_lock);
    bql_lock();

    return s;
}

#define METHOD_ADDR(gclass, name) \
    gclass ## _ ## name
#define METHOD_ADDR_TO_INDEX(x) ((x)>>2)
#define METHOD_NAME_STR(gclass, name) \
    tostring(gclass ## _ ## name)
#define METHOD_FUNC_NAME(gclass, name) \
    pgraph_ ## gclass ## _ ## name ## _handler
#define METHOD_HANDLER_ARG_DECL \
    NV2AState *d, PGRAPHState *pg, \
    unsigned int subchannel, unsigned int method, \
    uint32_t parameter, uint32_t *parameters, \
    size_t num_words_available, size_t *num_words_consumed, bool inc
#define METHOD_HANDLER_ARGS \
    d, pg, subchannel, method, parameter, parameters, \
    num_words_available, num_words_consumed, inc
#define DEF_METHOD_PROTO(gclass, name) \
    static void METHOD_FUNC_NAME(gclass, name)(METHOD_HANDLER_ARG_DECL)

#define DEF_METHOD(gclass, name) \
    DEF_METHOD_PROTO(gclass, name);
#define DEF_METHOD_RANGE(gclass, name, range) \
    DEF_METHOD_PROTO(gclass, name);
#define DEF_METHOD_CASE_4_OFFSET(gclass, name, offset, stride) /* Drop */
#define DEF_METHOD_CASE_4(gclass, name, stride) \
    DEF_METHOD_PROTO(gclass, name);
#include "methods.h.inc"
#undef DEF_METHOD
#undef DEF_METHOD_RANGE
#undef DEF_METHOD_CASE_4_OFFSET
#undef DEF_METHOD_CASE_4

typedef void (*MethodFunc)(METHOD_HANDLER_ARG_DECL);
static const struct {
    uint32_t base;
    const char *name;
    MethodFunc handler;
} pgraph_kelvin_methods[0x800] = {
#define DEF_METHOD(gclass, name)                        \
    [METHOD_ADDR_TO_INDEX(METHOD_ADDR(gclass, name))] = \
    { \
        METHOD_ADDR(gclass, name), \
        METHOD_NAME_STR(gclass, name), \
        METHOD_FUNC_NAME(gclass, name), \
    },
#define DEF_METHOD_RANGE(gclass, name, range) \
    [METHOD_ADDR_TO_INDEX(METHOD_ADDR(gclass, name)) \
     ... METHOD_ADDR_TO_INDEX(METHOD_ADDR(gclass, name) + 4*range - 1)] = \
    { \
        METHOD_ADDR(gclass, name), \
        METHOD_NAME_STR(gclass, name), \
        METHOD_FUNC_NAME(gclass, name), \
    },
#define DEF_METHOD_CASE_4_OFFSET(gclass, name, offset, stride) \
    [METHOD_ADDR_TO_INDEX(METHOD_ADDR(gclass, name) + offset)] = \
    { \
        METHOD_ADDR(gclass, name), \
        METHOD_NAME_STR(gclass, name), \
        METHOD_FUNC_NAME(gclass, name), \
    }, \
    [METHOD_ADDR_TO_INDEX(METHOD_ADDR(gclass, name) + offset + stride)] = \
    { \
        METHOD_ADDR(gclass, name), \
        METHOD_NAME_STR(gclass, name), \
        METHOD_FUNC_NAME(gclass, name), \
    }, \
    [METHOD_ADDR_TO_INDEX(METHOD_ADDR(gclass, name) + offset + stride * 2)] = \
    { \
        METHOD_ADDR(gclass, name), \
        METHOD_NAME_STR(gclass, name), \
        METHOD_FUNC_NAME(gclass, name), \
    }, \
    [METHOD_ADDR_TO_INDEX(METHOD_ADDR(gclass, name) + offset + stride * 3)] = \
    { \
        METHOD_ADDR(gclass, name), \
        METHOD_NAME_STR(gclass, name), \
        METHOD_FUNC_NAME(gclass, name), \
    },
#define DEF_METHOD_CASE_4(gclass, name, stride) \
    DEF_METHOD_CASE_4_OFFSET(gclass, name, 0, stride)
#include "methods.h.inc"
#undef DEF_METHOD
#undef DEF_METHOD_RANGE
#undef DEF_METHOD_CASE_4_OFFSET
#undef DEF_METHOD_CASE_4
};

#define METHOD_RANGE_END_NAME(gclass, name) \
    pgraph_ ## gclass ## _ ## name ## __END
#define DEF_METHOD(gclass, name) \
    static const size_t METHOD_RANGE_END_NAME(gclass, name) = \
        METHOD_ADDR(gclass, name) + 4;
#define DEF_METHOD_RANGE(gclass, name, range) \
    static const size_t METHOD_RANGE_END_NAME(gclass, name) = \
        METHOD_ADDR(gclass, name) + 4*range;
#define DEF_METHOD_CASE_4_OFFSET(gclass, name, offset, stride) /* drop */
#define DEF_METHOD_CASE_4(gclass, name, stride) \
    static const size_t METHOD_RANGE_END_NAME(gclass, name) = \
        METHOD_ADDR(gclass, name) + 4*stride;
#include "methods.h.inc"
#undef DEF_METHOD
#undef DEF_METHOD_RANGE
#undef DEF_METHOD_CASE_4_OFFSET
#undef DEF_METHOD_CASE_4

static void pgraph_method_log(unsigned int subchannel,
                              unsigned int graphics_class,
                              unsigned int method, uint32_t parameter)
{
    const char *method_name = "?";
    static unsigned int last = 0;
    static unsigned int count = 0;

    if (last == NV097_ARRAY_ELEMENT16 && method != last) {
        method_name = "NV097_ARRAY_ELEMENT16";
        trace_nv2a_pgraph_method_abbrev(subchannel, graphics_class, last,
                                        method_name, count);
    }

    if (method != NV097_ARRAY_ELEMENT16) {
        uint32_t base = method;
        switch (graphics_class) {
        case NV_KELVIN_PRIMITIVE: {
            int idx = METHOD_ADDR_TO_INDEX(method);
            if (idx < ARRAY_SIZE(pgraph_kelvin_methods) &&
                pgraph_kelvin_methods[idx].handler) {
                method_name = pgraph_kelvin_methods[idx].name;
                base = pgraph_kelvin_methods[idx].base;
            }
            break;
        }
        default:
            break;
        }

        uint32_t offset = method - base;
        trace_nv2a_pgraph_method(subchannel, graphics_class, method,
                                 method_name, offset, parameter);
    }

    if (method == last) {
        count++;
    } else {
        count = 0;
    }
    last = method;
}

static void pgraph_method_inc(MethodFunc handler, uint32_t end,
                              METHOD_HANDLER_ARG_DECL)
{
    if (!inc) {
        handler(METHOD_HANDLER_ARGS);
        return;
    }
    size_t count = MIN(num_words_available, (end - method) / 4);
    for (size_t i = 0; i < count; i++) {
        parameter = ldl_le_p(parameters + i);
        if (i) {
            pgraph_method_log(subchannel, NV_KELVIN_PRIMITIVE, method,
                              parameter);
        }
        handler(METHOD_HANDLER_ARGS);
        method += 4;
    }
    *num_words_consumed = count;
}

static void pgraph_method_non_inc(MethodFunc handler, METHOD_HANDLER_ARG_DECL)
{
    if (inc) {
        handler(METHOD_HANDLER_ARGS);
        return;
    }

    for (size_t i = 0; i < num_words_available; i++) {
        parameter = ldl_le_p(parameters + i);
        if (i) {
            pgraph_method_log(subchannel, NV_KELVIN_PRIMITIVE, method,
                              parameter);
        }
        handler(METHOD_HANDLER_ARGS);
    }
    *num_words_consumed = num_words_available;
}

#define METHOD_FUNC_NAME_INT(gclass, name) METHOD_FUNC_NAME(gclass, name##_int)
#define DEF_METHOD_INT(gclass, name) DEF_METHOD(gclass, name##_int)
#define DEF_METHOD(gclass, name) DEF_METHOD_PROTO(gclass, name)

#define DEF_METHOD_INC(gclass, name)                           \
    DEF_METHOD_INT(gclass, name);                              \
    DEF_METHOD(gclass, name)                                   \
    {                                                          \
        pgraph_method_inc(METHOD_FUNC_NAME_INT(gclass, name),  \
                          METHOD_RANGE_END_NAME(gclass, name), \
                          METHOD_HANDLER_ARGS);                \
    }                                                          \
    DEF_METHOD_INT(gclass, name)

#define DEF_METHOD_NON_INC(gclass, name)                          \
    DEF_METHOD_INT(gclass, name);                                 \
    DEF_METHOD(gclass, name)                                      \
    {                                                             \
        pgraph_method_non_inc(METHOD_FUNC_NAME_INT(gclass, name), \
                              METHOD_HANDLER_ARGS);               \
    }                                                             \
    DEF_METHOD_INT(gclass, name)

int pgraph_method(NV2AState *d, unsigned int subchannel,
                   unsigned int method, uint32_t parameter,
                   uint32_t *parameters, size_t num_words_available,
                   size_t max_lookahead_words, bool inc)
{
    int num_processed = 1;

    PGRAPHState *pg = &d->pgraph;

    bool channel_valid =
        PG_GET_MASK(NV_PGRAPH_CTX_CONTROL, NV_PGRAPH_CTX_CONTROL_CHID);
    assert(channel_valid);

    ContextSurfaces2DState *context_surfaces_2d = &pg->context_surfaces_2d;
    ImageBlitState *image_blit = &pg->image_blit;
    BetaState *beta = &pg->beta;

    assert(subchannel < 8);

    if (method == NV_SET_OBJECT) {
        assert(parameter < memory_region_size(&d->ramin));
        uint8_t *obj_ptr = d->ramin_ptr + parameter;

        uint32_t ctx_1 = ldl_le_p((uint32_t*)obj_ptr);
        uint32_t ctx_2 = ldl_le_p((uint32_t*)(obj_ptr+4));
        uint32_t ctx_3 = ldl_le_p((uint32_t*)(obj_ptr+8));
        uint32_t ctx_4 = ldl_le_p((uint32_t*)(obj_ptr+12));
        uint32_t ctx_5 = parameter;

        pgraph_reg_w(pg, NV_PGRAPH_CTX_CACHE1 + subchannel * 4, ctx_1);
        pgraph_reg_w(pg, NV_PGRAPH_CTX_CACHE2 + subchannel * 4, ctx_2);
        pgraph_reg_w(pg, NV_PGRAPH_CTX_CACHE3 + subchannel * 4, ctx_3);
        pgraph_reg_w(pg, NV_PGRAPH_CTX_CACHE4 + subchannel * 4, ctx_4);
        pgraph_reg_w(pg, NV_PGRAPH_CTX_CACHE5 + subchannel * 4, ctx_5);
    }

    // is this right?
    pgraph_reg_w(pg, NV_PGRAPH_CTX_SWITCH1,
                 pgraph_reg_r(pg, NV_PGRAPH_CTX_CACHE1 + subchannel * 4));
    pgraph_reg_w(pg, NV_PGRAPH_CTX_SWITCH2,
                 pgraph_reg_r(pg, NV_PGRAPH_CTX_CACHE2 + subchannel * 4));
    pgraph_reg_w(pg, NV_PGRAPH_CTX_SWITCH3,
                 pgraph_reg_r(pg, NV_PGRAPH_CTX_CACHE3 + subchannel * 4));
    pgraph_reg_w(pg, NV_PGRAPH_CTX_SWITCH4,
                 pgraph_reg_r(pg, NV_PGRAPH_CTX_CACHE4 + subchannel * 4));
    pgraph_reg_w(pg, NV_PGRAPH_CTX_SWITCH5,
                 pgraph_reg_r(pg, NV_PGRAPH_CTX_CACHE5 + subchannel * 4));

    uint32_t graphics_class = PG_GET_MASK(NV_PGRAPH_CTX_SWITCH1,
                                       NV_PGRAPH_CTX_SWITCH1_GRCLASS);

    pgraph_method_log(subchannel, graphics_class, method, parameter);

    if (subchannel != 0) {
        // catches context switching issues on xbox d3d
        assert(graphics_class != 0x97);
    }

    /* ugly switch for now */
    switch (graphics_class) {
    case NV_BETA: {
        switch (method) {
        case NV012_SET_OBJECT:
            beta->object_instance = parameter;
            break;
        case NV012_SET_BETA:
            if (parameter & 0x80000000) {
                beta->beta = 0;
            } else {
                // The parameter is a signed fixed-point number with a sign bit
                // and 31 fractional bits. Note that negative values are clamped
                // to 0, and only 8 fractional bits are actually implemented in
                // hardware.
                beta->beta = parameter & 0x7f800000;
            }
            break;
        default:
            goto unhandled;
        }
        break;
    }
    case NV_CONTEXT_PATTERN: {
        switch (method) {
        case NV044_SET_MONOCHROME_COLOR0:
            pgraph_reg_w(pg, NV_PGRAPH_PATT_COLOR0, parameter);
            break;
        default:
            goto unhandled;
        }
        break;
    }
    case NV_CONTEXT_SURFACES_2D: {
        switch (method) {
        case NV062_SET_OBJECT:
            context_surfaces_2d->object_instance = parameter;
            break;
        case NV062_SET_CONTEXT_DMA_IMAGE_SOURCE:
            context_surfaces_2d->dma_image_source = parameter;
            break;
        case NV062_SET_CONTEXT_DMA_IMAGE_DESTIN:
            context_surfaces_2d->dma_image_dest = parameter;
            break;
        case NV062_SET_COLOR_FORMAT:
            context_surfaces_2d->color_format = parameter;
            break;
        case NV062_SET_PITCH:
            context_surfaces_2d->source_pitch = parameter & 0xFFFF;
            context_surfaces_2d->dest_pitch = parameter >> 16;
            break;
        case NV062_SET_OFFSET_SOURCE:
            context_surfaces_2d->source_offset = parameter & 0x07FFFFFF;
            break;
        case NV062_SET_OFFSET_DESTIN:
            context_surfaces_2d->dest_offset = parameter & 0x07FFFFFF;
            break;
        default:
            goto unhandled;
        }
        break;
    }
    case NV_IMAGE_BLIT: {
        switch (method) {
        case NV09F_SET_OBJECT:
            image_blit->object_instance = parameter;
            break;
        case NV09F_SET_CONTEXT_SURFACES:
            image_blit->context_surfaces = parameter;
            break;
        case NV09F_SET_OPERATION:
            image_blit->operation = parameter;
            break;
        case NV09F_CONTROL_POINT_IN:
            image_blit->in_x = parameter & 0xFFFF;
            image_blit->in_y = parameter >> 16;
            break;
        case NV09F_CONTROL_POINT_OUT:
            image_blit->out_x = parameter & 0xFFFF;
            image_blit->out_y = parameter >> 16;
            break;
        case NV09F_SIZE:
            image_blit->width = parameter & 0xFFFF;
            image_blit->height = parameter >> 16;

            if (image_blit->width && image_blit->height) {
                d->pgraph.renderer->ops.image_blit(d);
            }
            break;
        default:
            goto unhandled;
        }
        break;
    }
    case NV_KELVIN_PRIMITIVE: {
        MethodFunc handler =
            pgraph_kelvin_methods[METHOD_ADDR_TO_INDEX(method)].handler;
        if (handler == NULL) {
            goto unhandled;
        }
        size_t num_words_consumed = 1;
        handler(d, pg, subchannel, method, parameter, parameters,
                num_words_available, &num_words_consumed, inc);

        /* Squash repeated BEGIN,DRAW_ARRAYS,END */
        #define LAM(i, mthd) ((parameters[i*2+1] & 0x31fff) == (mthd))
        #define LAP(i, prm) (parameters[i*2+2] == (prm))
        #define LAMP(i, mthd, prm) (LAM(i, mthd) && LAP(i, prm))

        if (method == NV097_DRAW_ARRAYS && (max_lookahead_words >= 7) &&
            pg->inline_elements_length == 0 &&
            pg->draw_arrays_length <
                (ARRAY_SIZE(pg->draw_arrays_start) - 1) &&
            LAMP(0, NV097_SET_BEGIN_END, NV097_SET_BEGIN_END_OP_END) &&
            LAMP(1, NV097_SET_BEGIN_END, pg->primitive_mode) &&
            LAM(2, NV097_DRAW_ARRAYS)) {
            num_words_consumed += 4;
            pg->draw_arrays_prevent_connect = true;
        }

        #undef LAM
        #undef LAP
        #undef LAMP

        num_processed = num_words_consumed;
        break;
    }
    default:
        goto unhandled;
    }

    return num_processed;

unhandled:
    trace_nv2a_pgraph_method_unhandled(subchannel, graphics_class,
                                           method, parameter);
    return num_processed;
}

DEF_METHOD(NV097, SET_OBJECT)
{
    pg->kelvin.object_instance = parameter;
}

DEF_METHOD(NV097, NO_OPERATION)
{
    /* The bios uses nop as a software method call -
     * it seems to expect a notify interrupt if the parameter isn't 0.
     * According to a nouveau guy it should still be a nop regardless
     * of the parameter. It's possible a debug register enables this,
     * but nothing obvious sticks out. Weird.
     */
    if (parameter == 0) {
        return;
    }

    unsigned channel_id =
        PG_GET_MASK(NV_PGRAPH_CTX_USER, NV_PGRAPH_CTX_USER_CHID);

    assert(!(pg->pending_interrupts & NV_PGRAPH_INTR_ERROR));

    PG_SET_MASK(NV_PGRAPH_TRAPPED_ADDR, NV_PGRAPH_TRAPPED_ADDR_CHID,
             channel_id);
    PG_SET_MASK(NV_PGRAPH_TRAPPED_ADDR, NV_PGRAPH_TRAPPED_ADDR_SUBCH,
             subchannel);
    PG_SET_MASK(NV_PGRAPH_TRAPPED_ADDR, NV_PGRAPH_TRAPPED_ADDR_MTHD,
             method);
    pgraph_reg_w(pg, NV_PGRAPH_TRAPPED_DATA_LOW, parameter);
    pgraph_reg_w(pg, NV_PGRAPH_NSOURCE,
                 NV_PGRAPH_NSOURCE_NOTIFICATION); /* TODO: check this */
    pg->pending_interrupts |= NV_PGRAPH_INTR_ERROR;
    pg->waiting_for_nop = true;

    qemu_mutex_unlock(&pg->lock);
    bql_lock();
    nv2a_update_irq(d);
    bql_unlock();
    qemu_mutex_lock(&pg->lock);
}

DEF_METHOD(NV097, WAIT_FOR_IDLE)
{
    d->pgraph.renderer->ops.surface_update(d, false, true, true);
}

DEF_METHOD(NV097, SET_FLIP_READ)
{
    PG_SET_MASK(NV_PGRAPH_SURFACE, NV_PGRAPH_SURFACE_READ_3D,
             parameter);
}

DEF_METHOD(NV097, SET_FLIP_WRITE)
{
    PG_SET_MASK(NV_PGRAPH_SURFACE, NV_PGRAPH_SURFACE_WRITE_3D,
             parameter);
}

DEF_METHOD(NV097, SET_FLIP_MODULO)
{
    PG_SET_MASK(NV_PGRAPH_SURFACE, NV_PGRAPH_SURFACE_MODULO_3D,
             parameter);
}

DEF_METHOD(NV097, FLIP_INCREMENT_WRITE)
{
    uint32_t old =
        PG_GET_MASK(NV_PGRAPH_SURFACE, NV_PGRAPH_SURFACE_WRITE_3D);

    PG_SET_MASK(NV_PGRAPH_SURFACE,
             NV_PGRAPH_SURFACE_WRITE_3D,
             (PG_GET_MASK(NV_PGRAPH_SURFACE,
                      NV_PGRAPH_SURFACE_WRITE_3D)+1)
                % PG_GET_MASK(NV_PGRAPH_SURFACE,
                           NV_PGRAPH_SURFACE_MODULO_3D) );

    uint32_t new =
        PG_GET_MASK(NV_PGRAPH_SURFACE, NV_PGRAPH_SURFACE_WRITE_3D);

    trace_nv2a_pgraph_flip_increment_write(old, new);
    pg->frame_time++;
}

DEF_METHOD(NV097, FLIP_STALL)
{
    trace_nv2a_pgraph_flip_stall();
    d->pgraph.renderer->ops.surface_update(d, false, true, true);
    d->pgraph.renderer->ops.flip_stall(d);
    nv2a_profile_flip_stall();
    pg->waiting_for_flip = true;
}

// TODO: these should be loading the dma objects from ramin here?

DEF_METHOD(NV097, SET_CONTEXT_DMA_NOTIFIES)
{
    pg->dma_notifies = parameter;
}

DEF_METHOD(NV097, SET_CONTEXT_DMA_A)
{
    pg->dma_a = parameter;
}

DEF_METHOD(NV097, SET_CONTEXT_DMA_B)
{
    pg->dma_b = parameter;
}

DEF_METHOD(NV097, SET_CONTEXT_DMA_STATE)
{
    pg->dma_state = parameter;
}

DEF_METHOD(NV097, SET_CONTEXT_DMA_COLOR)
{
    /* try to get any straggling draws in before the surface's changed :/ */
    d->pgraph.renderer->ops.surface_update(d, false, true, true);

    pg->dma_color = parameter;
    pg->surface_color.buffer_dirty = true;
}

DEF_METHOD(NV097, SET_CONTEXT_DMA_ZETA)
{
    pg->dma_zeta = parameter;
    pg->surface_zeta.buffer_dirty = true;
}

DEF_METHOD(NV097, SET_CONTEXT_DMA_VERTEX_A)
{
    pg->dma_vertex_a = parameter;
}

DEF_METHOD(NV097, SET_CONTEXT_DMA_VERTEX_B)
{
    pg->dma_vertex_b = parameter;
}

DEF_METHOD(NV097, SET_CONTEXT_DMA_SEMAPHORE)
{
    pg->dma_semaphore = parameter;
}

DEF_METHOD(NV097, SET_CONTEXT_DMA_REPORT)
{
    d->pgraph.renderer->ops.process_pending_reports(d);

    pg->dma_report = parameter;
}

DEF_METHOD(NV097, SET_SURFACE_CLIP_HORIZONTAL)
{
    d->pgraph.renderer->ops.surface_update(d, false, true, true);

    pg->surface_shape.clip_x =
        GET_MASK(parameter, NV097_SET_SURFACE_CLIP_HORIZONTAL_X);
    pg->surface_shape.clip_width =
        GET_MASK(parameter, NV097_SET_SURFACE_CLIP_HORIZONTAL_WIDTH);
}

DEF_METHOD(NV097, SET_SURFACE_CLIP_VERTICAL)
{
    d->pgraph.renderer->ops.surface_update(d, false, true, true);

    pg->surface_shape.clip_y =
        GET_MASK(parameter, NV097_SET_SURFACE_CLIP_VERTICAL_Y);
    pg->surface_shape.clip_height =
        GET_MASK(parameter, NV097_SET_SURFACE_CLIP_VERTICAL_HEIGHT);
}

DEF_METHOD(NV097, SET_SURFACE_FORMAT)
{
    d->pgraph.renderer->ops.surface_update(d, false, true, true);

    pg->surface_shape.color_format =
        GET_MASK(parameter, NV097_SET_SURFACE_FORMAT_COLOR);
    pg->surface_shape.zeta_format =
        GET_MASK(parameter, NV097_SET_SURFACE_FORMAT_ZETA);
    pg->surface_shape.anti_aliasing =
        GET_MASK(parameter, NV097_SET_SURFACE_FORMAT_ANTI_ALIASING);
    pg->surface_shape.log_width =
        GET_MASK(parameter, NV097_SET_SURFACE_FORMAT_WIDTH);
    pg->surface_shape.log_height =
        GET_MASK(parameter, NV097_SET_SURFACE_FORMAT_HEIGHT);

    int surface_type = GET_MASK(parameter, NV097_SET_SURFACE_FORMAT_TYPE);
    if (surface_type != pg->surface_type) {
        pg->surface_type = surface_type;
        pg->surface_color.buffer_dirty = true;
        pg->surface_zeta.buffer_dirty = true;
    }
}

DEF_METHOD(NV097, SET_SURFACE_PITCH)
{
    d->pgraph.renderer->ops.surface_update(d, false, true, true);
    unsigned int color_pitch = GET_MASK(parameter, NV097_SET_SURFACE_PITCH_COLOR);
    unsigned int zeta_pitch  = GET_MASK(parameter, NV097_SET_SURFACE_PITCH_ZETA);

    pg->surface_color.buffer_dirty |= (pg->surface_color.pitch != color_pitch);
    pg->surface_color.pitch = color_pitch;

    pg->surface_zeta.buffer_dirty |= (pg->surface_zeta.pitch != zeta_pitch);
    pg->surface_zeta.pitch = zeta_pitch;
}

DEF_METHOD(NV097, SET_SURFACE_COLOR_OFFSET)
{
    d->pgraph.renderer->ops.surface_update(d, false, true, true);
    pg->surface_color.buffer_dirty |= (pg->surface_color.offset != parameter);
    pg->surface_color.offset = parameter;
}

DEF_METHOD(NV097, SET_SURFACE_ZETA_OFFSET)
{
    d->pgraph.renderer->ops.surface_update(d, false, true, true);
    pg->surface_zeta.buffer_dirty |= (pg->surface_zeta.offset != parameter);
    pg->surface_zeta.offset = parameter;
}

DEF_METHOD_INC(NV097, SET_COMBINER_ALPHA_ICW)
{
    int slot = (method - NV097_SET_COMBINER_ALPHA_ICW) / 4;
    pgraph_reg_w(pg, NV_PGRAPH_COMBINEALPHAI0 + slot * 4, parameter);
}

DEF_METHOD(NV097, SET_COMBINER_SPECULAR_FOG_CW0)
{
    pgraph_reg_w(pg, NV_PGRAPH_COMBINESPECFOG0, parameter);
}

DEF_METHOD(NV097, SET_COMBINER_SPECULAR_FOG_CW1)
{
    pgraph_reg_w(pg, NV_PGRAPH_COMBINESPECFOG1, parameter);
}

DEF_METHOD(NV097, SET_TEXTURE_ADDRESS)
{
    int slot = (method - NV097_SET_TEXTURE_ADDRESS) / 64;
    pgraph_reg_w(pg, NV_PGRAPH_TEXADDRESS0 + slot * 4, parameter);
}

DEF_METHOD(NV097, SET_CONTROL0)
{
    d->pgraph.renderer->ops.surface_update(d, false, true, true);

    bool stencil_write_enable =
        parameter & NV097_SET_CONTROL0_STENCIL_WRITE_ENABLE;
    PG_SET_MASK(NV_PGRAPH_CONTROL_0,
             NV_PGRAPH_CONTROL_0_STENCIL_WRITE_ENABLE,
             stencil_write_enable);

    uint32_t z_format = GET_MASK(parameter, NV097_SET_CONTROL0_Z_FORMAT);
    PG_SET_MASK(NV_PGRAPH_SETUPRASTER,
             NV_PGRAPH_SETUPRASTER_Z_FORMAT, z_format);

    bool z_perspective =
        parameter & NV097_SET_CONTROL0_Z_PERSPECTIVE_ENABLE;
    PG_SET_MASK(NV_PGRAPH_CONTROL_0,
             NV_PGRAPH_CONTROL_0_Z_PERSPECTIVE_ENABLE,
             z_perspective);
}

DEF_METHOD(NV097, SET_LIGHT_CONTROL)
{
    PG_SET_MASK(NV_PGRAPH_CSV0_C, NV_PGRAPH_CSV0_C_SEPARATE_SPECULAR,
             (parameter & NV097_SET_LIGHT_CONTROL_SEPARATE_SPECULAR) != 0);

    PG_SET_MASK(NV_PGRAPH_CSV0_C, NV_PGRAPH_CSV0_C_LOCALEYE,
             (parameter & NV097_SET_LIGHT_CONTROL_LOCALEYE) != 0);

    PG_SET_MASK(NV_PGRAPH_CSV0_C, NV_PGRAPH_CSV0_C_ALPHA_FROM_MATERIAL_SPECULAR,
             (parameter & NV097_SET_LIGHT_CONTROL_ALPHA_FROM_MATERIAL_SPECULAR) != 0);
}

DEF_METHOD(NV097, SET_COLOR_MATERIAL)
{
    PG_SET_MASK(NV_PGRAPH_CSV0_C, NV_PGRAPH_CSV0_C_EMISSION,
             (parameter >> 0) & 3);
    PG_SET_MASK(NV_PGRAPH_CSV0_C, NV_PGRAPH_CSV0_C_AMBIENT,
             (parameter >> 2) & 3);
    PG_SET_MASK(NV_PGRAPH_CSV0_C, NV_PGRAPH_CSV0_C_DIFFUSE,
             (parameter >> 4) & 3);
    PG_SET_MASK(NV_PGRAPH_CSV0_C, NV_PGRAPH_CSV0_C_SPECULAR,
             (parameter >> 6) & 3);
}

DEF_METHOD(NV097, SET_FOG_MODE)
{
    /* FIXME: There is also NV_PGRAPH_CSV0_D_FOG_MODE */
    unsigned int mode;
    switch (parameter) {
    case NV097_SET_FOG_MODE_V_LINEAR:
        mode = NV_PGRAPH_CONTROL_3_FOG_MODE_LINEAR; break;
    case NV097_SET_FOG_MODE_V_EXP:
        mode = NV_PGRAPH_CONTROL_3_FOG_MODE_EXP; break;
    case NV097_SET_FOG_MODE_V_EXP2:
        mode = NV_PGRAPH_CONTROL_3_FOG_MODE_EXP2; break;
    case NV097_SET_FOG_MODE_V_EXP_ABS:
        mode = NV_PGRAPH_CONTROL_3_FOG_MODE_EXP_ABS; break;
    case NV097_SET_FOG_MODE_V_EXP2_ABS:
        mode = NV_PGRAPH_CONTROL_3_FOG_MODE_EXP2_ABS; break;
    case NV097_SET_FOG_MODE_V_LINEAR_ABS:
        mode = NV_PGRAPH_CONTROL_3_FOG_MODE_LINEAR_ABS; break;
    default:
        assert(false);
        break;
    }
    PG_SET_MASK(NV_PGRAPH_CONTROL_3, NV_PGRAPH_CONTROL_3_FOG_MODE,
             mode);
}

DEF_METHOD(NV097, SET_FOG_GEN_MODE)
{
    unsigned int mode;
    switch (parameter) {
    case NV097_SET_FOG_GEN_MODE_V_SPEC_ALPHA:
        mode = NV_PGRAPH_CSV0_D_FOGGENMODE_SPEC_ALPHA; break;
    case NV097_SET_FOG_GEN_MODE_V_RADIAL:
        mode = NV_PGRAPH_CSV0_D_FOGGENMODE_RADIAL; break;
    case NV097_SET_FOG_GEN_MODE_V_PLANAR:
        mode = NV_PGRAPH_CSV0_D_FOGGENMODE_PLANAR; break;
    case NV097_SET_FOG_GEN_MODE_V_ABS_PLANAR:
        mode = NV_PGRAPH_CSV0_D_FOGGENMODE_ABS_PLANAR; break;
    case NV097_SET_FOG_GEN_MODE_V_FOG_X:
        mode = NV_PGRAPH_CSV0_D_FOGGENMODE_FOG_X; break;
    default:
        assert(false);
        break;
    }
    PG_SET_MASK(NV_PGRAPH_CSV0_D, NV_PGRAPH_CSV0_D_FOGGENMODE, mode);
}

DEF_METHOD(NV097, SET_FOG_ENABLE)
{
    /*
      FIXME: There is also:
        PG_SET_MASK(NV_PGRAPH_CSV0_D, NV_PGRAPH_CSV0_D_FOGENABLE,
             parameter);
    */
    PG_SET_MASK(NV_PGRAPH_CONTROL_3, NV_PGRAPH_CONTROL_3_FOGENABLE,
         parameter);
}

DEF_METHOD(NV097, SET_FOG_COLOR)
{
    /* PGRAPH channels are ARGB, parameter channels are ABGR */
    uint8_t red = GET_MASK(parameter, NV097_SET_FOG_COLOR_RED);
    uint8_t green = GET_MASK(parameter, NV097_SET_FOG_COLOR_GREEN);
    uint8_t blue = GET_MASK(parameter, NV097_SET_FOG_COLOR_BLUE);
    uint8_t alpha = GET_MASK(parameter, NV097_SET_FOG_COLOR_ALPHA);
    PG_SET_MASK(NV_PGRAPH_FOGCOLOR, NV_PGRAPH_FOGCOLOR_RED, red);
    PG_SET_MASK(NV_PGRAPH_FOGCOLOR, NV_PGRAPH_FOGCOLOR_GREEN, green);
    PG_SET_MASK(NV_PGRAPH_FOGCOLOR, NV_PGRAPH_FOGCOLOR_BLUE, blue);
    PG_SET_MASK(NV_PGRAPH_FOGCOLOR, NV_PGRAPH_FOGCOLOR_ALPHA, alpha);
}

DEF_METHOD(NV097, SET_WINDOW_CLIP_TYPE)
{
    PG_SET_MASK(NV_PGRAPH_SETUPRASTER,
             NV_PGRAPH_SETUPRASTER_WINDOWCLIPTYPE, parameter);
}

DEF_METHOD_INC(NV097, SET_WINDOW_CLIP_HORIZONTAL)
{
    int slot = (method - NV097_SET_WINDOW_CLIP_HORIZONTAL) / 4;
    for (; slot < 8; ++slot) {
        pgraph_reg_w(pg, NV_PGRAPH_WINDOWCLIPX0 + slot * 4, parameter);
    }
}

DEF_METHOD_INC(NV097, SET_WINDOW_CLIP_VERTICAL)
{
    int slot = (method - NV097_SET_WINDOW_CLIP_VERTICAL) / 4;
    for (; slot < 8; ++slot) {
        pgraph_reg_w(pg, NV_PGRAPH_WINDOWCLIPY0 + slot * 4, parameter);
    }
}

DEF_METHOD(NV097, SET_ALPHA_TEST_ENABLE)
{
    PG_SET_MASK(NV_PGRAPH_CONTROL_0,
             NV_PGRAPH_CONTROL_0_ALPHATESTENABLE, parameter);
}

DEF_METHOD(NV097, SET_BLEND_ENABLE)
{
    PG_SET_MASK(NV_PGRAPH_BLEND, NV_PGRAPH_BLEND_EN, parameter);
}

DEF_METHOD(NV097, SET_CULL_FACE_ENABLE)
{
    PG_SET_MASK(NV_PGRAPH_SETUPRASTER,
             NV_PGRAPH_SETUPRASTER_CULLENABLE,
             parameter);
}

DEF_METHOD(NV097, SET_DEPTH_TEST_ENABLE)
{
    PG_SET_MASK(NV_PGRAPH_CONTROL_0, NV_PGRAPH_CONTROL_0_ZENABLE,
             parameter);
}

DEF_METHOD(NV097, SET_DITHER_ENABLE)
{
    PG_SET_MASK(NV_PGRAPH_CONTROL_0,
             NV_PGRAPH_CONTROL_0_DITHERENABLE, parameter);
}

DEF_METHOD(NV097, SET_LIGHTING_ENABLE)
{
    PG_SET_MASK(NV_PGRAPH_CSV0_C, NV_PGRAPH_CSV0_C_LIGHTING,
             parameter);
}

DEF_METHOD(NV097, SET_POINT_PARAMS_ENABLE)
{
    PG_SET_MASK(NV_PGRAPH_CSV0_D, NV_PGRAPH_CSV0_D_POINTPARAMSENABLE,
             parameter);
    PG_SET_MASK(NV_PGRAPH_CONTROL_3,
             NV_PGRAPH_CONTROL_3_POINTPARAMSENABLE, parameter);
}

DEF_METHOD(NV097, SET_POINT_SMOOTH_ENABLE)
{
    PG_SET_MASK(NV_PGRAPH_SETUPRASTER,
             NV_PGRAPH_SETUPRASTER_POINTSMOOTHENABLE, parameter);
}

DEF_METHOD(NV097, SET_LINE_SMOOTH_ENABLE)
{
    PG_SET_MASK(NV_PGRAPH_SETUPRASTER,
             NV_PGRAPH_SETUPRASTER_LINESMOOTHENABLE, parameter);
}

DEF_METHOD(NV097, SET_POLY_SMOOTH_ENABLE)
{
    PG_SET_MASK(NV_PGRAPH_SETUPRASTER,
             NV_PGRAPH_SETUPRASTER_POLYSMOOTHENABLE, parameter);
}

DEF_METHOD(NV097, SET_SKIN_MODE)
{
    PG_SET_MASK(NV_PGRAPH_CSV0_D, NV_PGRAPH_CSV0_D_SKIN,
             parameter);
}

DEF_METHOD(NV097, SET_STENCIL_TEST_ENABLE)
{
    PG_SET_MASK(NV_PGRAPH_CONTROL_1,
             NV_PGRAPH_CONTROL_1_STENCIL_TEST_ENABLE, parameter);
}

DEF_METHOD(NV097, SET_POLY_OFFSET_POINT_ENABLE)
{
    PG_SET_MASK(NV_PGRAPH_SETUPRASTER,
             NV_PGRAPH_SETUPRASTER_POFFSETPOINTENABLE, parameter);
}

DEF_METHOD(NV097, SET_POLY_OFFSET_LINE_ENABLE)
{
    PG_SET_MASK(NV_PGRAPH_SETUPRASTER,
             NV_PGRAPH_SETUPRASTER_POFFSETLINEENABLE, parameter);
}

DEF_METHOD(NV097, SET_POLY_OFFSET_FILL_ENABLE)
{
    PG_SET_MASK(NV_PGRAPH_SETUPRASTER,
             NV_PGRAPH_SETUPRASTER_POFFSETFILLENABLE, parameter);
}

DEF_METHOD(NV097, SET_ALPHA_FUNC)
{
    PG_SET_MASK(NV_PGRAPH_CONTROL_0,
             NV_PGRAPH_CONTROL_0_ALPHAFUNC, parameter & 0xF);
}

DEF_METHOD(NV097, SET_ALPHA_REF)
{
    PG_SET_MASK(NV_PGRAPH_CONTROL_0,
             NV_PGRAPH_CONTROL_0_ALPHAREF, parameter);
}

DEF_METHOD(NV097, SET_BLEND_FUNC_SFACTOR)
{
    unsigned int factor;
    switch (parameter) {
    case NV097_SET_BLEND_FUNC_SFACTOR_V_ZERO:
        factor = NV_PGRAPH_BLEND_SFACTOR_ZERO; break;
    case NV097_SET_BLEND_FUNC_SFACTOR_V_ONE:
        factor = NV_PGRAPH_BLEND_SFACTOR_ONE; break;
    case NV097_SET_BLEND_FUNC_SFACTOR_V_SRC_COLOR:
        factor = NV_PGRAPH_BLEND_SFACTOR_SRC_COLOR; break;
    case NV097_SET_BLEND_FUNC_SFACTOR_V_ONE_MINUS_SRC_COLOR:
        factor = NV_PGRAPH_BLEND_SFACTOR_ONE_MINUS_SRC_COLOR; break;
    case NV097_SET_BLEND_FUNC_SFACTOR_V_SRC_ALPHA:
        factor = NV_PGRAPH_BLEND_SFACTOR_SRC_ALPHA; break;
    case NV097_SET_BLEND_FUNC_SFACTOR_V_ONE_MINUS_SRC_ALPHA:
        factor = NV_PGRAPH_BLEND_SFACTOR_ONE_MINUS_SRC_ALPHA; break;
    case NV097_SET_BLEND_FUNC_SFACTOR_V_DST_ALPHA:
        factor = NV_PGRAPH_BLEND_SFACTOR_DST_ALPHA; break;
    case NV097_SET_BLEND_FUNC_SFACTOR_V_ONE_MINUS_DST_ALPHA:
        factor = NV_PGRAPH_BLEND_SFACTOR_ONE_MINUS_DST_ALPHA; break;
    case NV097_SET_BLEND_FUNC_SFACTOR_V_DST_COLOR:
        factor = NV_PGRAPH_BLEND_SFACTOR_DST_COLOR; break;
    case NV097_SET_BLEND_FUNC_SFACTOR_V_ONE_MINUS_DST_COLOR:
        factor = NV_PGRAPH_BLEND_SFACTOR_ONE_MINUS_DST_COLOR; break;
    case NV097_SET_BLEND_FUNC_SFACTOR_V_SRC_ALPHA_SATURATE:
        factor = NV_PGRAPH_BLEND_SFACTOR_SRC_ALPHA_SATURATE; break;
    case NV097_SET_BLEND_FUNC_SFACTOR_V_CONSTANT_COLOR:
        factor = NV_PGRAPH_BLEND_SFACTOR_CONSTANT_COLOR; break;
    case NV097_SET_BLEND_FUNC_SFACTOR_V_ONE_MINUS_CONSTANT_COLOR:
        factor = NV_PGRAPH_BLEND_SFACTOR_ONE_MINUS_CONSTANT_COLOR; break;
    case NV097_SET_BLEND_FUNC_SFACTOR_V_CONSTANT_ALPHA:
        factor = NV_PGRAPH_BLEND_SFACTOR_CONSTANT_ALPHA; break;
    case NV097_SET_BLEND_FUNC_SFACTOR_V_ONE_MINUS_CONSTANT_ALPHA:
        factor = NV_PGRAPH_BLEND_SFACTOR_ONE_MINUS_CONSTANT_ALPHA; break;
    default:
        NV2A_DPRINTF("Unknown blend source factor: 0x%08x\n", parameter);
        return; /* discard */
    }
    PG_SET_MASK(NV_PGRAPH_BLEND, NV_PGRAPH_BLEND_SFACTOR, factor);
}

DEF_METHOD(NV097, SET_BLEND_FUNC_DFACTOR)
{
    unsigned int factor;
    switch (parameter) {
    case NV097_SET_BLEND_FUNC_DFACTOR_V_ZERO:
        factor = NV_PGRAPH_BLEND_DFACTOR_ZERO; break;
    case NV097_SET_BLEND_FUNC_DFACTOR_V_ONE:
        factor = NV_PGRAPH_BLEND_DFACTOR_ONE; break;
    case NV097_SET_BLEND_FUNC_DFACTOR_V_SRC_COLOR:
        factor = NV_PGRAPH_BLEND_DFACTOR_SRC_COLOR; break;
    case NV097_SET_BLEND_FUNC_DFACTOR_V_ONE_MINUS_SRC_COLOR:
        factor = NV_PGRAPH_BLEND_DFACTOR_ONE_MINUS_SRC_COLOR; break;
    case NV097_SET_BLEND_FUNC_DFACTOR_V_SRC_ALPHA:
        factor = NV_PGRAPH_BLEND_DFACTOR_SRC_ALPHA; break;
    case NV097_SET_BLEND_FUNC_DFACTOR_V_ONE_MINUS_SRC_ALPHA:
        factor = NV_PGRAPH_BLEND_DFACTOR_ONE_MINUS_SRC_ALPHA; break;
    case NV097_SET_BLEND_FUNC_DFACTOR_V_DST_ALPHA:
        factor = NV_PGRAPH_BLEND_DFACTOR_DST_ALPHA; break;
    case NV097_SET_BLEND_FUNC_DFACTOR_V_ONE_MINUS_DST_ALPHA:
        factor = NV_PGRAPH_BLEND_DFACTOR_ONE_MINUS_DST_ALPHA; break;
    case NV097_SET_BLEND_FUNC_DFACTOR_V_DST_COLOR:
        factor = NV_PGRAPH_BLEND_DFACTOR_DST_COLOR; break;
    case NV097_SET_BLEND_FUNC_DFACTOR_V_ONE_MINUS_DST_COLOR:
        factor = NV_PGRAPH_BLEND_DFACTOR_ONE_MINUS_DST_COLOR; break;
    case NV097_SET_BLEND_FUNC_DFACTOR_V_SRC_ALPHA_SATURATE:
        factor = NV_PGRAPH_BLEND_DFACTOR_SRC_ALPHA_SATURATE; break;
    case NV097_SET_BLEND_FUNC_DFACTOR_V_CONSTANT_COLOR:
        factor = NV_PGRAPH_BLEND_DFACTOR_CONSTANT_COLOR; break;
    case NV097_SET_BLEND_FUNC_DFACTOR_V_ONE_MINUS_CONSTANT_COLOR:
        factor = NV_PGRAPH_BLEND_DFACTOR_ONE_MINUS_CONSTANT_COLOR; break;
    case NV097_SET_BLEND_FUNC_DFACTOR_V_CONSTANT_ALPHA:
        factor = NV_PGRAPH_BLEND_DFACTOR_CONSTANT_ALPHA; break;
    case NV097_SET_BLEND_FUNC_DFACTOR_V_ONE_MINUS_CONSTANT_ALPHA:
        factor = NV_PGRAPH_BLEND_DFACTOR_ONE_MINUS_CONSTANT_ALPHA; break;
    default:
        NV2A_DPRINTF("Unknown blend destination factor: 0x%08x\n", parameter);
        return; /* discard */
    }
    PG_SET_MASK(NV_PGRAPH_BLEND, NV_PGRAPH_BLEND_DFACTOR, factor);
}

DEF_METHOD(NV097, SET_BLEND_COLOR)
{
    pgraph_reg_w(pg, NV_PGRAPH_BLENDCOLOR, parameter);
}

DEF_METHOD(NV097, SET_BLEND_EQUATION)
{
    unsigned int equation;
    switch (parameter) {
    case NV097_SET_BLEND_EQUATION_V_FUNC_SUBTRACT:
        equation = 0; break;
    case NV097_SET_BLEND_EQUATION_V_FUNC_REVERSE_SUBTRACT:
        equation = 1; break;
    case NV097_SET_BLEND_EQUATION_V_FUNC_ADD:
        equation = 2; break;
    case NV097_SET_BLEND_EQUATION_V_MIN:
        equation = 3; break;
    case NV097_SET_BLEND_EQUATION_V_MAX:
        equation = 4; break;
    case NV097_SET_BLEND_EQUATION_V_FUNC_REVERSE_SUBTRACT_SIGNED:
        equation = 5; break;
    case NV097_SET_BLEND_EQUATION_V_FUNC_ADD_SIGNED:
        equation = 6; break;
    default:
        NV2A_DPRINTF("Unknown blend equation: 0x%08x\n", parameter);
        return; /* discard */
    }
    PG_SET_MASK(NV_PGRAPH_BLEND, NV_PGRAPH_BLEND_EQN, equation);
}

DEF_METHOD(NV097, SET_DEPTH_FUNC)
{
    if (parameter >= 0x200 && parameter <= 0x207) {
        PG_SET_MASK(NV_PGRAPH_CONTROL_0, NV_PGRAPH_CONTROL_0_ZFUNC,
                    parameter & 0xF);
    }
}

DEF_METHOD(NV097, SET_COLOR_MASK)
{
    pg->surface_color.write_enabled_cache |= pgraph_color_write_enabled(pg);

    bool alpha = parameter & NV097_SET_COLOR_MASK_ALPHA_WRITE_ENABLE;
    bool red = parameter & NV097_SET_COLOR_MASK_RED_WRITE_ENABLE;
    bool green = parameter & NV097_SET_COLOR_MASK_GREEN_WRITE_ENABLE;
    bool blue = parameter & NV097_SET_COLOR_MASK_BLUE_WRITE_ENABLE;
    PG_SET_MASK(NV_PGRAPH_CONTROL_0,
             NV_PGRAPH_CONTROL_0_ALPHA_WRITE_ENABLE, alpha);
    PG_SET_MASK(NV_PGRAPH_CONTROL_0,
             NV_PGRAPH_CONTROL_0_RED_WRITE_ENABLE, red);
    PG_SET_MASK(NV_PGRAPH_CONTROL_0,
             NV_PGRAPH_CONTROL_0_GREEN_WRITE_ENABLE, green);
    PG_SET_MASK(NV_PGRAPH_CONTROL_0,
             NV_PGRAPH_CONTROL_0_BLUE_WRITE_ENABLE, blue);
}

DEF_METHOD(NV097, SET_DEPTH_MASK)
{
    pg->surface_zeta.write_enabled_cache |= pgraph_zeta_write_enabled(pg);

    PG_SET_MASK(NV_PGRAPH_CONTROL_0,
             NV_PGRAPH_CONTROL_0_ZWRITEENABLE, parameter);
}

DEF_METHOD(NV097, SET_STENCIL_MASK)
{
    PG_SET_MASK(NV_PGRAPH_CONTROL_1,
             NV_PGRAPH_CONTROL_1_STENCIL_MASK_WRITE, parameter);
}

DEF_METHOD(NV097, SET_STENCIL_FUNC)
{
    PG_SET_MASK(NV_PGRAPH_CONTROL_1,
             NV_PGRAPH_CONTROL_1_STENCIL_FUNC, parameter & 0xF);
}

DEF_METHOD(NV097, SET_STENCIL_FUNC_REF)
{
    PG_SET_MASK(NV_PGRAPH_CONTROL_1,
             NV_PGRAPH_CONTROL_1_STENCIL_REF, parameter);
}

DEF_METHOD(NV097, SET_STENCIL_FUNC_MASK)
{
    PG_SET_MASK(NV_PGRAPH_CONTROL_1,
             NV_PGRAPH_CONTROL_1_STENCIL_MASK_READ, parameter);
}

static unsigned int kelvin_map_stencil_op(uint32_t parameter)
{
    unsigned int op;
    switch (parameter) {
    case NV097_SET_STENCIL_OP_V_KEEP:
        op = NV_PGRAPH_CONTROL_2_STENCIL_OP_V_KEEP; break;
    case NV097_SET_STENCIL_OP_V_ZERO:
        op = NV_PGRAPH_CONTROL_2_STENCIL_OP_V_ZERO; break;
    case NV097_SET_STENCIL_OP_V_REPLACE:
        op = NV_PGRAPH_CONTROL_2_STENCIL_OP_V_REPLACE; break;
    case NV097_SET_STENCIL_OP_V_INCRSAT:
        op = NV_PGRAPH_CONTROL_2_STENCIL_OP_V_INCRSAT; break;
    case NV097_SET_STENCIL_OP_V_DECRSAT:
        op = NV_PGRAPH_CONTROL_2_STENCIL_OP_V_DECRSAT; break;
    case NV097_SET_STENCIL_OP_V_INVERT:
        op = NV_PGRAPH_CONTROL_2_STENCIL_OP_V_INVERT; break;
    case NV097_SET_STENCIL_OP_V_INCR:
        op = NV_PGRAPH_CONTROL_2_STENCIL_OP_V_INCR; break;
    case NV097_SET_STENCIL_OP_V_DECR:
        op = NV_PGRAPH_CONTROL_2_STENCIL_OP_V_DECR; break;
    default:
        assert(false);
        break;
    }
    return op;
}

DEF_METHOD(NV097, SET_STENCIL_OP_FAIL)
{
    PG_SET_MASK(NV_PGRAPH_CONTROL_2,
             NV_PGRAPH_CONTROL_2_STENCIL_OP_FAIL,
             kelvin_map_stencil_op(parameter));
}

DEF_METHOD(NV097, SET_STENCIL_OP_ZFAIL)
{
    PG_SET_MASK(NV_PGRAPH_CONTROL_2,
             NV_PGRAPH_CONTROL_2_STENCIL_OP_ZFAIL,
             kelvin_map_stencil_op(parameter));
}

DEF_METHOD(NV097, SET_STENCIL_OP_ZPASS)
{
    PG_SET_MASK(NV_PGRAPH_CONTROL_2,
             NV_PGRAPH_CONTROL_2_STENCIL_OP_ZPASS,
             kelvin_map_stencil_op(parameter));
}

DEF_METHOD(NV097, SET_SHADE_MODE)
{
    switch (parameter) {
    case NV097_SET_SHADE_MODE_V_FLAT:
        PG_SET_MASK(NV_PGRAPH_CONTROL_3, NV_PGRAPH_CONTROL_3_SHADEMODE,
                 NV_PGRAPH_CONTROL_3_SHADEMODE_FLAT);
        break;
    case NV097_SET_SHADE_MODE_V_SMOOTH:
        PG_SET_MASK(NV_PGRAPH_CONTROL_3, NV_PGRAPH_CONTROL_3_SHADEMODE,
                 NV_PGRAPH_CONTROL_3_SHADEMODE_SMOOTH);
        break;
    default:
        /* Discard */
        break;
    }
}

DEF_METHOD(NV097, SET_PROVOKING_VERTEX)
{
    assert((parameter & ~1) == 0);
    PG_SET_MASK(NV_PGRAPH_CONTROL_3, NV_PGRAPH_CONTROL_3_PROVOKING_VERTEX,
             parameter);
}

DEF_METHOD(NV097, SET_POLYGON_OFFSET_SCALE_FACTOR)
{
    pgraph_reg_w(pg, NV_PGRAPH_ZOFFSETFACTOR, parameter);
}

DEF_METHOD(NV097, SET_POLYGON_OFFSET_BIAS)
{
    pgraph_reg_w(pg, NV_PGRAPH_ZOFFSETBIAS, parameter);
}

static unsigned int kelvin_map_polygon_mode(uint32_t parameter)
{
    unsigned int mode;
    switch (parameter) {
    case NV097_SET_FRONT_POLYGON_MODE_V_POINT:
        mode = NV_PGRAPH_SETUPRASTER_FRONTFACEMODE_POINT; break;
    case NV097_SET_FRONT_POLYGON_MODE_V_LINE:
        mode = NV_PGRAPH_SETUPRASTER_FRONTFACEMODE_LINE; break;
    case NV097_SET_FRONT_POLYGON_MODE_V_FILL:
        mode = NV_PGRAPH_SETUPRASTER_FRONTFACEMODE_FILL; break;
    default:
        assert(false);
        break;
    }
    return mode;
}

DEF_METHOD(NV097, SET_FRONT_POLYGON_MODE)
{
    PG_SET_MASK(NV_PGRAPH_SETUPRASTER,
             NV_PGRAPH_SETUPRASTER_FRONTFACEMODE,
             kelvin_map_polygon_mode(parameter));
}

DEF_METHOD(NV097, SET_BACK_POLYGON_MODE)
{
    PG_SET_MASK(NV_PGRAPH_SETUPRASTER,
             NV_PGRAPH_SETUPRASTER_BACKFACEMODE,
             kelvin_map_polygon_mode(parameter));
}

DEF_METHOD(NV097, SET_CLIP_MIN)
{
    pgraph_reg_w(pg, NV_PGRAPH_ZCLIPMIN, parameter);
}

DEF_METHOD(NV097, SET_CLIP_MAX)
{
    pgraph_reg_w(pg, NV_PGRAPH_ZCLIPMAX, parameter);
}

DEF_METHOD(NV097, SET_CULL_FACE)
{
    unsigned int face;
    switch (parameter) {
    case NV097_SET_CULL_FACE_V_FRONT:
        face = NV_PGRAPH_SETUPRASTER_CULLCTRL_FRONT; break;
    case NV097_SET_CULL_FACE_V_BACK:
        face = NV_PGRAPH_SETUPRASTER_CULLCTRL_BACK; break;
    case NV097_SET_CULL_FACE_V_FRONT_AND_BACK:
        face = NV_PGRAPH_SETUPRASTER_CULLCTRL_FRONT_AND_BACK; break;
    default:
        assert(false);
        break;
    }
    PG_SET_MASK(NV_PGRAPH_SETUPRASTER, NV_PGRAPH_SETUPRASTER_CULLCTRL, face);
}

DEF_METHOD(NV097, SET_FRONT_FACE)
{
    bool ccw;
    switch (parameter) {
    case NV097_SET_FRONT_FACE_V_CW:
        ccw = false; break;
    case NV097_SET_FRONT_FACE_V_CCW:
        ccw = true; break;
    default:
        NV2A_DPRINTF("Unknown front face: 0x%08x\n", parameter);
        return; /* discard */
    }
    PG_SET_MASK(NV_PGRAPH_SETUPRASTER, NV_PGRAPH_SETUPRASTER_FRONTFACE,
                ccw ? 1 : 0);
}

DEF_METHOD(NV097, SET_NORMALIZATION_ENABLE)
{
    PG_SET_MASK(NV_PGRAPH_CSV0_C, NV_PGRAPH_CSV0_C_NORMALIZATION_ENABLE,
                parameter);
}

DEF_METHOD_INC(NV097, SET_MATERIAL_EMISSION)
{
    int slot = (method - NV097_SET_MATERIAL_EMISSION) / 4;
    // FIXME: Verify NV_IGRAPH_XF_LTCTXA_CM_COL is correct
    pg->ltctxa[NV_IGRAPH_XF_LTCTXA_CM_COL][slot] = parameter;
    pg->ltctxa_dirty[NV_IGRAPH_XF_LTCTXA_CM_COL] = true;
}

DEF_METHOD(NV097, SET_MATERIAL_ALPHA)
{
    pg->material_alpha = *(float*)&parameter;
}

DEF_METHOD(NV097, SET_SPECULAR_ENABLE)
{
    PG_SET_MASK(NV_PGRAPH_CSV0_C, NV_PGRAPH_CSV0_C_SPECULAR_ENABLE, parameter);
}

DEF_METHOD(NV097, SET_LIGHT_ENABLE_MASK)
{
    PG_SET_MASK(NV_PGRAPH_CSV0_D, NV_PGRAPH_CSV0_D_LIGHTS, parameter);
}

static unsigned int kelvin_map_texgen(uint32_t parameter, unsigned int channel)
{
    assert(channel < 4);
    unsigned int texgen;
    switch (parameter) {
    case NV097_SET_TEXGEN_S_DISABLE:
        texgen = NV_PGRAPH_CSV1_A_T0_S_DISABLE; break;
    case NV097_SET_TEXGEN_S_EYE_LINEAR:
        texgen = NV_PGRAPH_CSV1_A_T0_S_EYE_LINEAR; break;
    case NV097_SET_TEXGEN_S_OBJECT_LINEAR:
        texgen = NV_PGRAPH_CSV1_A_T0_S_OBJECT_LINEAR; break;
    case NV097_SET_TEXGEN_S_SPHERE_MAP:
        assert(channel < 2);
        texgen = NV_PGRAPH_CSV1_A_T0_S_SPHERE_MAP; break;
    case NV097_SET_TEXGEN_S_REFLECTION_MAP:
        assert(channel < 3);
        texgen = NV_PGRAPH_CSV1_A_T0_S_REFLECTION_MAP; break;
    case NV097_SET_TEXGEN_S_NORMAL_MAP:
        assert(channel < 3);
        texgen = NV_PGRAPH_CSV1_A_T0_S_NORMAL_MAP; break;
    default:
        assert(false);
        break;
    }
    return texgen;
}

DEF_METHOD(NV097, SET_TEXGEN_S)
{
    int slot = (method - NV097_SET_TEXGEN_S) / 16;
    unsigned int reg = (slot < 2) ? NV_PGRAPH_CSV1_A
                                  : NV_PGRAPH_CSV1_B;
    unsigned int mask = (slot % 2) ? NV_PGRAPH_CSV1_A_T1_S
                                   : NV_PGRAPH_CSV1_A_T0_S;
    PG_SET_MASK(reg, mask, kelvin_map_texgen(parameter, 0));
}

DEF_METHOD(NV097, SET_TEXGEN_T)
{
    int slot = (method - NV097_SET_TEXGEN_T) / 16;
    unsigned int reg = (slot < 2) ? NV_PGRAPH_CSV1_A
                                  : NV_PGRAPH_CSV1_B;
    unsigned int mask = (slot % 2) ? NV_PGRAPH_CSV1_A_T1_T
                                   : NV_PGRAPH_CSV1_A_T0_T;
    PG_SET_MASK(reg, mask, kelvin_map_texgen(parameter, 1));
}

DEF_METHOD(NV097, SET_TEXGEN_R)
{
    int slot = (method - NV097_SET_TEXGEN_R) / 16;
    unsigned int reg = (slot < 2) ? NV_PGRAPH_CSV1_A
                                  : NV_PGRAPH_CSV1_B;
    unsigned int mask = (slot % 2) ? NV_PGRAPH_CSV1_A_T1_R
                                   : NV_PGRAPH_CSV1_A_T0_R;
    PG_SET_MASK(reg, mask, kelvin_map_texgen(parameter, 2));
}

DEF_METHOD(NV097, SET_TEXGEN_Q)
{
    int slot = (method - NV097_SET_TEXGEN_Q) / 16;
    unsigned int reg = (slot < 2) ? NV_PGRAPH_CSV1_A
                                  : NV_PGRAPH_CSV1_B;
    unsigned int mask = (slot % 2) ? NV_PGRAPH_CSV1_A_T1_Q
                                   : NV_PGRAPH_CSV1_A_T0_Q;
    PG_SET_MASK(reg, mask, kelvin_map_texgen(parameter, 3));
}

DEF_METHOD_INC(NV097, SET_TEXTURE_MATRIX_ENABLE)
{
    int slot = (method - NV097_SET_TEXTURE_MATRIX_ENABLE) / 4;
    pg->texture_matrix_enable[slot] = parameter;
}

DEF_METHOD(NV097, SET_POINT_SIZE)
{
    if (parameter > NV097_SET_POINT_SIZE_V_MAX) {
        return;
    }

    pgraph_reg_w(pg, NV_PGRAPH_POINTSIZE, parameter);
}

DEF_METHOD_INC(NV097, SET_PROJECTION_MATRIX)
{
    int slot = (method - NV097_SET_PROJECTION_MATRIX) / 4;
    // pg->projection_matrix[slot] = *(float*)&parameter;
    unsigned int row = NV_IGRAPH_XF_XFCTX_PMAT0 + slot/4;
    pg->vsh_constants[row][slot%4] = parameter;
    pg->vsh_constants_dirty[row] = true;
}

DEF_METHOD_INC(NV097, SET_MODEL_VIEW_MATRIX)
{
    int slot = (method - NV097_SET_MODEL_VIEW_MATRIX) / 4;
    unsigned int matnum = slot / 16;
    unsigned int entry = slot % 16;
    unsigned int row = NV_IGRAPH_XF_XFCTX_MMAT0 + matnum*8 + entry/4;
    pg->vsh_constants[row][entry % 4] = parameter;
    pg->vsh_constants_dirty[row] = true;
}

DEF_METHOD_INC(NV097, SET_INVERSE_MODEL_VIEW_MATRIX)
{
    int slot = (method - NV097_SET_INVERSE_MODEL_VIEW_MATRIX) / 4;
    unsigned int matnum = slot / 16;
    unsigned int entry = slot % 16;
    unsigned int row = NV_IGRAPH_XF_XFCTX_IMMAT0 + matnum*8 + entry/4;
    pg->vsh_constants[row][entry % 4] = parameter;
    pg->vsh_constants_dirty[row] = true;
}

DEF_METHOD_INC(NV097, SET_COMPOSITE_MATRIX)
{
    int slot = (method - NV097_SET_COMPOSITE_MATRIX) / 4;
    unsigned int row = NV_IGRAPH_XF_XFCTX_CMAT0 + slot/4;
    pg->vsh_constants[row][slot%4] = parameter;
    pg->vsh_constants_dirty[row] = true;
}

DEF_METHOD_INC(NV097, SET_TEXTURE_MATRIX)
{
    int slot = (method - NV097_SET_TEXTURE_MATRIX) / 4;
    unsigned int tex = slot / 16;
    unsigned int entry = slot % 16;
    unsigned int row = NV_IGRAPH_XF_XFCTX_T0MAT + tex*8 + entry/4;
    pg->vsh_constants[row][entry%4] = parameter;
    pg->vsh_constants_dirty[row] = true;
}

DEF_METHOD_INC(NV097, SET_FOG_PARAMS)
{
    int slot = (method - NV097_SET_FOG_PARAMS) / 4;
    if (slot < 2) {
        pgraph_reg_w(pg, NV_PGRAPH_FOGPARAM0 + slot*4, parameter);
    } else {
        /* FIXME: No idea where slot = 2 is */
    }

    pg->ltctxa[NV_IGRAPH_XF_LTCTXA_FOG_K][slot] = parameter;
    pg->ltctxa_dirty[NV_IGRAPH_XF_LTCTXA_FOG_K] = true;
}

/* Handles NV097_SET_TEXGEN_PLANE_S,T,R,Q */
DEF_METHOD_INC(NV097, SET_TEXGEN_PLANE_S)
{
    int slot = (method - NV097_SET_TEXGEN_PLANE_S) / 4;
    unsigned int tex = slot / 16;
    unsigned int entry = slot % 16;
    unsigned int row = NV_IGRAPH_XF_XFCTX_TG0MAT + tex*8 + entry/4;
    pg->vsh_constants[row][entry%4] = parameter;
    pg->vsh_constants_dirty[row] = true;
}

DEF_METHOD(NV097, SET_TEXGEN_VIEW_MODEL)
{
    PG_SET_MASK(NV_PGRAPH_CSV0_D, NV_PGRAPH_CSV0_D_TEXGEN_REF,
             parameter);
}

DEF_METHOD_INC(NV097, SET_FOG_PLANE)
{
    int slot = (method - NV097_SET_FOG_PLANE) / 4;
    pg->vsh_constants[NV_IGRAPH_XF_XFCTX_FOG][slot] = parameter;
    pg->vsh_constants_dirty[NV_IGRAPH_XF_XFCTX_FOG] = true;
}

struct CurveCoefficients {
  float a;
  float b;
  float c;
};

static const struct CurveCoefficients curve_coefficients[] = {
  {1.000108475163, -9.838607076280, 54.829089549713},
  {1.199164441703, -3.292603784852, 7.799987995214},
  {8.653441252033, 29.189473787191, 43.586027561823},
  {-531.307758450301, 117.398468683934, 113.155490738338},
  {-4.662713151292, 1.221108944572, 1.217360986939},
  {-124.435242105211, 35.401219563514, 35.408114377045},
  {10672560.259502287954, 21565843.555823743343, 10894794.336297152564},
  {-51973801.463933646679, -104199997.554352939129, -52225454.356278456748},
  {972270.324080004124, 2025882.096547174733, 1054898.052467488218},
};

static const float kCoefficient0StepPoints[] = {
  -0.022553957999, // power = 1.25
  -0.421539008617, // power = 4.00
  -0.678715527058, // power = 9.00
  -0.838916420937, // power = 20.00
  -0.961754500866, // power = 90.00
  -0.990773200989, // power = 375.00
  -0.994858562946, // power = 650.00
  -0.996561050415, // power = 1000.00
  -0.999547004700, // power = 1250.00
};

static float reconstruct_quadratic(float c0, const struct CurveCoefficients *coefficients) {
  return coefficients->a + coefficients->b * c0 + coefficients->c * c0 * c0;
}

static float reconstruct_saturation_growth_rate(float c0, const struct CurveCoefficients *coefficients) {
  return (coefficients->a * c0) / (coefficients->b + coefficients->c * c0);
}

static float (* const reconstruct_func_map[])(float, const struct CurveCoefficients *) = {
  reconstruct_quadratic, // 1.0..1.25 max error 0.01 %
  reconstruct_quadratic, // 1.25..4.0 max error 2.2 %
  reconstruct_quadratic, // 4.0..9.0 max error 2.3 %
  reconstruct_saturation_growth_rate, // 9.0..20.0 max error 1.4 %
  reconstruct_saturation_growth_rate, // 20.0..90.0 max error 2.1 %
  reconstruct_saturation_growth_rate, // 90.0..375.0 max error 2.8%
  reconstruct_quadratic, // 375..650 max error 1.0 %
  reconstruct_quadratic, // 650..1000 max error 1.7%
  reconstruct_quadratic, // 1000..1250 max error 1.0%
};

static float reconstruct_specular_power(const float *params) {
  // See https://github.com/dracc/xgu/blob/db3172d8c983629f0dc971092981846da22438ae/xgux.h#L279

  // Values < 1.0 will result in a positive c1 and (c2 - c0 * 2) will be very
  // close to the original value.
  if (params[1] > 0.0f && params[2] < 1.0f) {
    return params[2] - (params[0] * 2.0f);
  }

  float c0 = params[0];
  float c3 = params[3];
  // FIXME: This handling is not correct, but is distinct without crashing.
  // It does not appear possible for a DirectX-generated value to be positive,
  // so while this differs from hardware behavior, it may be irrelevant in
  // practice.
  if (c0 > 0.0f || c3 > 0.0f) {
    return 0.0001f;
  }

  float reconstructed_power = 0.f;
  for (uint32_t i = 0; i < sizeof(kCoefficient0StepPoints) / sizeof(kCoefficient0StepPoints[0]); ++i) {
    if (c0 > kCoefficient0StepPoints[i]) {
      reconstructed_power = reconstruct_func_map[i](c0, &curve_coefficients[i]);
      break;
    }
  }

  float reconstructed_half_power = 0.f;
  for (uint32_t i = 0; i < sizeof(kCoefficient0StepPoints) / sizeof(kCoefficient0StepPoints[0]); ++i) {
    if (c3 > kCoefficient0StepPoints[i]) {
      reconstructed_half_power = reconstruct_func_map[i](c3, &curve_coefficients[i]);
      break;
    }
  }

  // The range can be extended beyond 1250 by using the half power params. This
  // will only work for DirectX generated values, arbitrary params could
  // erroneously trigger this.
  //
  // There are some very low power (~1) values that have inverted powers, but
  // they are easily identified by comparatively high c0 parameters.
  if (reconstructed_power == 0.f || (reconstructed_half_power > reconstructed_power && c0 < -0.1f)) {
    return reconstructed_half_power * 2.f;
  }

  return reconstructed_power;
}

DEF_METHOD_INC(NV097, SET_SPECULAR_PARAMS)
{
    int slot = (method - NV097_SET_SPECULAR_PARAMS) / 4;
    pg->specular_params[slot] = *(float *)&parameter;
    if (slot == 5) {
        pg->specular_power = reconstruct_specular_power(pg->specular_params);
    }
}

DEF_METHOD_INC(NV097, SET_SCENE_AMBIENT_COLOR)
{
    int slot = (method - NV097_SET_SCENE_AMBIENT_COLOR) / 4;
    // ??
    pg->ltctxa[NV_IGRAPH_XF_LTCTXA_FR_AMB][slot] = parameter;
    pg->ltctxa_dirty[NV_IGRAPH_XF_LTCTXA_FR_AMB] = true;
}

DEF_METHOD_INC(NV097, SET_VIEWPORT_OFFSET)
{
    int slot = (method - NV097_SET_VIEWPORT_OFFSET) / 4;
    pg->vsh_constants[NV_IGRAPH_XF_XFCTX_VPOFF][slot] = parameter;
    pg->vsh_constants_dirty[NV_IGRAPH_XF_XFCTX_VPOFF] = true;
}

DEF_METHOD_INC(NV097, SET_POINT_PARAMS)
{
    int slot = (method - NV097_SET_POINT_PARAMS) / 4;
    pg->point_params[slot] = *(float *)&parameter; /* FIXME: Where? */
}

DEF_METHOD_INC(NV097, SET_EYE_POSITION)
{
    int slot = (method - NV097_SET_EYE_POSITION) / 4;
    pg->vsh_constants[NV_IGRAPH_XF_XFCTX_EYEP][slot] = parameter;
    pg->vsh_constants_dirty[NV_IGRAPH_XF_XFCTX_EYEP] = true;
}

DEF_METHOD_INC(NV097, SET_COMBINER_FACTOR0)
{
    int slot = (method - NV097_SET_COMBINER_FACTOR0) / 4;
    pgraph_reg_w(pg, NV_PGRAPH_COMBINEFACTOR0 + slot*4, parameter);
}

DEF_METHOD_INC(NV097, SET_COMBINER_FACTOR1)
{
    int slot = (method - NV097_SET_COMBINER_FACTOR1) / 4;
    pgraph_reg_w(pg, NV_PGRAPH_COMBINEFACTOR1 + slot*4, parameter);
}

DEF_METHOD_INC(NV097, SET_COMBINER_ALPHA_OCW)
{
    int slot = (method - NV097_SET_COMBINER_ALPHA_OCW) / 4;
    pgraph_reg_w(pg, NV_PGRAPH_COMBINEALPHAO0 + slot*4, parameter);
}

DEF_METHOD_INC(NV097, SET_COMBINER_COLOR_ICW)
{
    int slot = (method - NV097_SET_COMBINER_COLOR_ICW) / 4;
    pgraph_reg_w(pg, NV_PGRAPH_COMBINECOLORI0 + slot*4, parameter);
}

DEF_METHOD_INC(NV097, SET_COLOR_KEY_COLOR)
{
    int slot = (method - NV097_SET_COLOR_KEY_COLOR) / 4;
    pgraph_reg_w(pg, NV_PGRAPH_COLORKEYCOLOR0 + slot * 4, parameter);
}

DEF_METHOD_INC(NV097, SET_VIEWPORT_SCALE)
{
    int slot = (method - NV097_SET_VIEWPORT_SCALE) / 4;
    pg->vsh_constants[NV_IGRAPH_XF_XFCTX_VPSCL][slot] = parameter;
    pg->vsh_constants_dirty[NV_IGRAPH_XF_XFCTX_VPSCL] = true;
}

DEF_METHOD_INC(NV097, SET_TRANSFORM_PROGRAM)
{
    int slot = (method - NV097_SET_TRANSFORM_PROGRAM) / 4;

    int program_load = PG_GET_MASK(NV_PGRAPH_CHEOPS_OFFSET,
                                NV_PGRAPH_CHEOPS_OFFSET_PROG_LD_PTR);

    assert(program_load < NV2A_MAX_TRANSFORM_PROGRAM_LENGTH);
    pg->program_data[program_load][slot%4] = parameter;
    pg->program_data_dirty = true;

    if (slot % 4 == 3) {
        PG_SET_MASK(NV_PGRAPH_CHEOPS_OFFSET,
                 NV_PGRAPH_CHEOPS_OFFSET_PROG_LD_PTR, program_load+1);
    }
}

DEF_METHOD_INC(NV097, SET_TRANSFORM_CONSTANT)
{
    int slot = (method - NV097_SET_TRANSFORM_CONSTANT) / 4;
    int const_load = PG_GET_MASK(NV_PGRAPH_CHEOPS_OFFSET,
                              NV_PGRAPH_CHEOPS_OFFSET_CONST_LD_PTR);

    assert(const_load < NV2A_VERTEXSHADER_CONSTANTS);
    // VertexShaderConstant *constant = &pg->constants[const_load];
    pg->vsh_constants_dirty[const_load] |=
        (parameter != pg->vsh_constants[const_load][slot%4]);
    pg->vsh_constants[const_load][slot%4] = parameter;

    if (slot % 4 == 3) {
        PG_SET_MASK(NV_PGRAPH_CHEOPS_OFFSET,
                 NV_PGRAPH_CHEOPS_OFFSET_CONST_LD_PTR, const_load+1);
    }
}

DEF_METHOD_INC(NV097, SET_VERTEX3F)
{
    int slot = (method - NV097_SET_VERTEX3F) / 4;
    VertexAttribute *attribute =
        &pg->vertex_attributes[NV2A_VERTEX_ATTR_POSITION];
    pgraph_allocate_inline_buffer_vertices(pg, NV2A_VERTEX_ATTR_POSITION);
    attribute->inline_value[slot] = *(float*)&parameter;
    attribute->inline_value[3] = 1.0f;
    if (slot == 2) {
        pgraph_finish_inline_buffer_vertex(pg);
    }
}

/* Handles NV097_SET_BACK_LIGHT_* */
DEF_METHOD_INC(NV097, SET_BACK_LIGHT_AMBIENT_COLOR)
{
    int slot = (method - NV097_SET_BACK_LIGHT_AMBIENT_COLOR) / 4;
    unsigned int part = NV097_SET_BACK_LIGHT_AMBIENT_COLOR / 4 + slot % 16;
    slot /= 16; /* [Light index] */
    assert(slot < 8);
    switch(part * 4) {
    case NV097_SET_BACK_LIGHT_AMBIENT_COLOR ...
            NV097_SET_BACK_LIGHT_AMBIENT_COLOR + 8:
        part -= NV097_SET_BACK_LIGHT_AMBIENT_COLOR / 4;
        pg->ltctxb[NV_IGRAPH_XF_LTCTXB_L0_BAMB + slot*6][part] = parameter;
        pg->ltctxb_dirty[NV_IGRAPH_XF_LTCTXB_L0_BAMB + slot*6] = true;
        break;
    case NV097_SET_BACK_LIGHT_DIFFUSE_COLOR ...
            NV097_SET_BACK_LIGHT_DIFFUSE_COLOR + 8:
        part -= NV097_SET_BACK_LIGHT_DIFFUSE_COLOR / 4;
        pg->ltctxb[NV_IGRAPH_XF_LTCTXB_L0_BDIF + slot*6][part] = parameter;
        pg->ltctxb_dirty[NV_IGRAPH_XF_LTCTXB_L0_BDIF + slot*6] = true;
        break;
    case NV097_SET_BACK_LIGHT_SPECULAR_COLOR ...
            NV097_SET_BACK_LIGHT_SPECULAR_COLOR + 8:
        part -= NV097_SET_BACK_LIGHT_SPECULAR_COLOR / 4;
        pg->ltctxb[NV_IGRAPH_XF_LTCTXB_L0_BSPC + slot*6][part] = parameter;
        pg->ltctxb_dirty[NV_IGRAPH_XF_LTCTXB_L0_BSPC + slot*6] = true;
        break;
    default:
        assert(false);
        break;
    }
}

/* Handles all the light source props except for NV097_SET_BACK_LIGHT_* */
DEF_METHOD_INC(NV097, SET_LIGHT_AMBIENT_COLOR)
{
    int slot = (method - NV097_SET_LIGHT_AMBIENT_COLOR) / 4;
    unsigned int part = NV097_SET_LIGHT_AMBIENT_COLOR / 4 + slot % 32;
    slot /= 32; /* [Light index] */
    assert(slot < 8);
    switch(part * 4) {
    case NV097_SET_LIGHT_AMBIENT_COLOR ...
            NV097_SET_LIGHT_AMBIENT_COLOR + 8:
        part -= NV097_SET_LIGHT_AMBIENT_COLOR / 4;
        pg->ltctxb[NV_IGRAPH_XF_LTCTXB_L0_AMB + slot*6][part] = parameter;
        pg->ltctxb_dirty[NV_IGRAPH_XF_LTCTXB_L0_AMB + slot*6] = true;
        break;
    case NV097_SET_LIGHT_DIFFUSE_COLOR ...
           NV097_SET_LIGHT_DIFFUSE_COLOR + 8:
        part -= NV097_SET_LIGHT_DIFFUSE_COLOR / 4;
        pg->ltctxb[NV_IGRAPH_XF_LTCTXB_L0_DIF + slot*6][part] = parameter;
        pg->ltctxb_dirty[NV_IGRAPH_XF_LTCTXB_L0_DIF + slot*6] = true;
        break;
    case NV097_SET_LIGHT_SPECULAR_COLOR ...
            NV097_SET_LIGHT_SPECULAR_COLOR + 8:
        part -= NV097_SET_LIGHT_SPECULAR_COLOR / 4;
        pg->ltctxb[NV_IGRAPH_XF_LTCTXB_L0_SPC + slot*6][part] = parameter;
        pg->ltctxb_dirty[NV_IGRAPH_XF_LTCTXB_L0_SPC + slot*6] = true;
        break;
    case NV097_SET_LIGHT_LOCAL_RANGE:
        pg->ltc1[NV_IGRAPH_XF_LTC1_r0 + slot][0] = parameter;
        pg->ltc1_dirty[NV_IGRAPH_XF_LTC1_r0 + slot] = true;
        break;
    case NV097_SET_LIGHT_INFINITE_HALF_VECTOR ...
            NV097_SET_LIGHT_INFINITE_HALF_VECTOR + 8:
        part -= NV097_SET_LIGHT_INFINITE_HALF_VECTOR / 4;
        pg->light_infinite_half_vector[slot][part] = *(float*)&parameter;
        break;
    case NV097_SET_LIGHT_INFINITE_DIRECTION ...
            NV097_SET_LIGHT_INFINITE_DIRECTION + 8:
        part -= NV097_SET_LIGHT_INFINITE_DIRECTION / 4;
        pg->light_infinite_direction[slot][part] = *(float*)&parameter;
        break;
    case NV097_SET_LIGHT_SPOT_FALLOFF ...
            NV097_SET_LIGHT_SPOT_FALLOFF + 8:
        part -= NV097_SET_LIGHT_SPOT_FALLOFF / 4;
        pg->ltctxa[NV_IGRAPH_XF_LTCTXA_L0_K + slot*2][part] = parameter;
        pg->ltctxa_dirty[NV_IGRAPH_XF_LTCTXA_L0_K + slot*2] = true;
        break;
    case NV097_SET_LIGHT_SPOT_DIRECTION ...
            NV097_SET_LIGHT_SPOT_DIRECTION + 12:
        part -= NV097_SET_LIGHT_SPOT_DIRECTION / 4;
        pg->ltctxa[NV_IGRAPH_XF_LTCTXA_L0_SPT + slot*2][part] = parameter;
        pg->ltctxa_dirty[NV_IGRAPH_XF_LTCTXA_L0_SPT + slot*2] = true;
        break;
    case NV097_SET_LIGHT_LOCAL_POSITION ...
            NV097_SET_LIGHT_LOCAL_POSITION + 8:
        part -= NV097_SET_LIGHT_LOCAL_POSITION / 4;
        pg->light_local_position[slot][part] = *(float*)&parameter;
        break;
    case NV097_SET_LIGHT_LOCAL_ATTENUATION ...
            NV097_SET_LIGHT_LOCAL_ATTENUATION + 8:
        part -= NV097_SET_LIGHT_LOCAL_ATTENUATION / 4;
        pg->light_local_attenuation[slot][part] = *(float*)&parameter;
        break;
    default:
        assert(false);
        break;
    }
}

DEF_METHOD_INC(NV097, SET_VERTEX4F)
{
    int slot = (method - NV097_SET_VERTEX4F) / 4;
    VertexAttribute *attribute =
        &pg->vertex_attributes[NV2A_VERTEX_ATTR_POSITION];
    pgraph_allocate_inline_buffer_vertices(pg, NV2A_VERTEX_ATTR_POSITION);
    attribute->inline_value[slot] = *(float*)&parameter;
    if (slot == 3) {
        pgraph_finish_inline_buffer_vertex(pg);
    }
}

DEF_METHOD(NV097, SET_FOG_COORD)
{
    VertexAttribute *attribute = &pg->vertex_attributes[NV2A_VERTEX_ATTR_FOG];
    pgraph_allocate_inline_buffer_vertices(pg, NV2A_VERTEX_ATTR_FOG);
    attribute->inline_value[0] = *(float*)&parameter;
    attribute->inline_value[1] = attribute->inline_value[0];
    attribute->inline_value[2] = attribute->inline_value[0];
    attribute->inline_value[3] = attribute->inline_value[0];
}

DEF_METHOD(NV097, SET_WEIGHT1F)
{
    VertexAttribute *attribute = &pg->vertex_attributes[NV2A_VERTEX_ATTR_WEIGHT];
    pgraph_allocate_inline_buffer_vertices(pg, NV2A_VERTEX_ATTR_WEIGHT);
    attribute->inline_value[0] = *(float*)&parameter;
    attribute->inline_value[1] = 0.f;
    attribute->inline_value[2] = 0.f;
    attribute->inline_value[3] = 1.f;
}

DEF_METHOD_INC(NV097, SET_NORMAL3S)
{
    int slot = (method - NV097_SET_NORMAL3S) / 4;
    unsigned int part = slot % 2;
    VertexAttribute *attribute =
        &pg->vertex_attributes[NV2A_VERTEX_ATTR_NORMAL];
    pgraph_allocate_inline_buffer_vertices(pg, NV2A_VERTEX_ATTR_NORMAL);
    int16_t val = parameter & 0xFFFF;
    attribute->inline_value[part * 2 + 0] = MAX(-1.0f, (float)val / 32767.0f);
    val = parameter >> 16;
    attribute->inline_value[part * 2 + 1] = MAX(-1.0f, (float)val / 32767.0f);
}

#define SET_VERTEX_ATTRIBUTE_4S(command, attr_index)                     \
    do {                                                                   \
        int slot = (method - (command)) / 4;                               \
        unsigned int part = slot % 2;                                      \
        VertexAttribute *attribute = &pg->vertex_attributes[(attr_index)]; \
        pgraph_allocate_inline_buffer_vertices(pg, (attr_index));          \
        attribute->inline_value[part * 2 + 0] =                            \
            (float)(int16_t)(parameter & 0xFFFF);                          \
        attribute->inline_value[part * 2 + 1] =                            \
            (float)(int16_t)(parameter >> 16);                             \
    } while (0)

DEF_METHOD_INC(NV097, SET_TEXCOORD0_4S)
{
    SET_VERTEX_ATTRIBUTE_4S(NV097_SET_TEXCOORD0_4S, NV2A_VERTEX_ATTR_TEXTURE0);
}

DEF_METHOD_INC(NV097, SET_TEXCOORD1_4S)
{
    SET_VERTEX_ATTRIBUTE_4S(NV097_SET_TEXCOORD1_4S, NV2A_VERTEX_ATTR_TEXTURE1);
}

DEF_METHOD_INC(NV097, SET_TEXCOORD2_4S)
{
    SET_VERTEX_ATTRIBUTE_4S(NV097_SET_TEXCOORD2_4S, NV2A_VERTEX_ATTR_TEXTURE2);
}

DEF_METHOD_INC(NV097, SET_TEXCOORD3_4S)
{
    SET_VERTEX_ATTRIBUTE_4S(NV097_SET_TEXCOORD3_4S, NV2A_VERTEX_ATTR_TEXTURE3);
}

#undef SET_VERTEX_ATTRIBUTE_4S

#define SET_VERTEX_ATRIBUTE_TEX_2S(attr_index)                             \
    do {                                                                   \
        VertexAttribute *attribute = &pg->vertex_attributes[(attr_index)]; \
        pgraph_allocate_inline_buffer_vertices(pg, (attr_index));          \
        attribute->inline_value[0] = (float)(int16_t)(parameter & 0xFFFF); \
        attribute->inline_value[1] = (float)(int16_t)(parameter >> 16);    \
        attribute->inline_value[2] = 0.0f;                                 \
        attribute->inline_value[3] = 1.0f;                                 \
    } while (0)

DEF_METHOD_INC(NV097, SET_TEXCOORD0_2S)
{
    SET_VERTEX_ATRIBUTE_TEX_2S(NV2A_VERTEX_ATTR_TEXTURE0);
}

DEF_METHOD_INC(NV097, SET_TEXCOORD1_2S)
{
    SET_VERTEX_ATRIBUTE_TEX_2S(NV2A_VERTEX_ATTR_TEXTURE1);
}

DEF_METHOD_INC(NV097, SET_TEXCOORD2_2S)
{
    SET_VERTEX_ATRIBUTE_TEX_2S(NV2A_VERTEX_ATTR_TEXTURE2);
}

DEF_METHOD_INC(NV097, SET_TEXCOORD3_2S)
{
    SET_VERTEX_ATRIBUTE_TEX_2S(NV2A_VERTEX_ATTR_TEXTURE3);
}

#undef SET_VERTEX_ATRIBUTE_TEX_2S

#define SET_VERTEX_COLOR_3F(command, attr_index)                           \
    do {                                                                   \
        int slot = (method - (command)) / 4;                               \
        VertexAttribute *attribute = &pg->vertex_attributes[(attr_index)]; \
        pgraph_allocate_inline_buffer_vertices(pg, (attr_index));          \
        attribute->inline_value[slot] = *(float*)&parameter;               \
        attribute->inline_value[3] = 1.0f;                                 \
    } while (0)

DEF_METHOD_INC(NV097, SET_DIFFUSE_COLOR3F)
{
    SET_VERTEX_COLOR_3F(NV097_SET_DIFFUSE_COLOR3F, NV2A_VERTEX_ATTR_DIFFUSE);
}

DEF_METHOD_INC(NV097, SET_SPECULAR_COLOR3F)
{
    SET_VERTEX_COLOR_3F(NV097_SET_SPECULAR_COLOR3F, NV2A_VERTEX_ATTR_SPECULAR);
}

#undef SET_VERTEX_COLOR_3F

#define SET_VERTEX_ATTRIBUTE_F(command, attr_index)                        \
    do {                                                                   \
        int slot = (method - (command)) / 4;                               \
        VertexAttribute *attribute = &pg->vertex_attributes[(attr_index)]; \
        pgraph_allocate_inline_buffer_vertices(pg, (attr_index));          \
        attribute->inline_value[slot] = *(float*)&parameter;               \
    } while (0)

DEF_METHOD_INC(NV097, SET_NORMAL3F)
{
    SET_VERTEX_ATTRIBUTE_F(NV097_SET_NORMAL3F, NV2A_VERTEX_ATTR_NORMAL);
}

DEF_METHOD_INC(NV097, SET_DIFFUSE_COLOR4F)
{
    SET_VERTEX_ATTRIBUTE_F(NV097_SET_DIFFUSE_COLOR4F, NV2A_VERTEX_ATTR_DIFFUSE);
}

DEF_METHOD_INC(NV097, SET_SPECULAR_COLOR4F)
{
    SET_VERTEX_ATTRIBUTE_F(NV097_SET_SPECULAR_COLOR4F,
                           NV2A_VERTEX_ATTR_SPECULAR);
}

DEF_METHOD_INC(NV097, SET_TEXCOORD0_4F)
{
    SET_VERTEX_ATTRIBUTE_F(NV097_SET_TEXCOORD0_4F, NV2A_VERTEX_ATTR_TEXTURE0);
}

DEF_METHOD_INC(NV097, SET_TEXCOORD1_4F)
{
    SET_VERTEX_ATTRIBUTE_F(NV097_SET_TEXCOORD1_4F, NV2A_VERTEX_ATTR_TEXTURE1);
}

DEF_METHOD_INC(NV097, SET_TEXCOORD2_4F)
{
    SET_VERTEX_ATTRIBUTE_F(NV097_SET_TEXCOORD2_4F, NV2A_VERTEX_ATTR_TEXTURE2);
}

DEF_METHOD_INC(NV097, SET_TEXCOORD3_4F)
{
    SET_VERTEX_ATTRIBUTE_F(NV097_SET_TEXCOORD3_4F, NV2A_VERTEX_ATTR_TEXTURE3);
}

DEF_METHOD_INC(NV097, SET_WEIGHT4F)
{
    SET_VERTEX_ATTRIBUTE_F(NV097_SET_WEIGHT4F, NV2A_VERTEX_ATTR_WEIGHT);
}

#undef SET_VERTEX_ATTRIBUTE_F

DEF_METHOD_INC(NV097, SET_WEIGHT2F)
{
    int slot = (method - NV097_SET_WEIGHT2F) / 4;
    VertexAttribute *attribute =
        &pg->vertex_attributes[NV2A_VERTEX_ATTR_WEIGHT];
    pgraph_allocate_inline_buffer_vertices(pg, NV2A_VERTEX_ATTR_WEIGHT);
    attribute->inline_value[slot] = *(float*)&parameter;
    attribute->inline_value[2] = 0.0f;
    attribute->inline_value[3] = 1.0f;
}

DEF_METHOD_INC(NV097, SET_WEIGHT3F)
{
    int slot = (method - NV097_SET_WEIGHT3F) / 4;
    VertexAttribute *attribute =
        &pg->vertex_attributes[NV2A_VERTEX_ATTR_WEIGHT];
    pgraph_allocate_inline_buffer_vertices(pg, NV2A_VERTEX_ATTR_WEIGHT);
    attribute->inline_value[slot] = *(float*)&parameter;
    attribute->inline_value[3] = 1.0f;
}

#define SET_VERTEX_ATRIBUTE_TEX_2F(command, attr_index)                    \
    do {                                                                   \
        int slot = (method - (command)) / 4;                               \
        VertexAttribute *attribute = &pg->vertex_attributes[(attr_index)]; \
        pgraph_allocate_inline_buffer_vertices(pg, (attr_index));          \
        attribute->inline_value[slot] = *(float*)&parameter;               \
        attribute->inline_value[2] = 0.0f;                                 \
        attribute->inline_value[3] = 1.0f;                                 \
    } while (0)

DEF_METHOD_INC(NV097, SET_TEXCOORD0_2F)
{
    SET_VERTEX_ATRIBUTE_TEX_2F(NV097_SET_TEXCOORD0_2F,
                               NV2A_VERTEX_ATTR_TEXTURE0);
}

DEF_METHOD_INC(NV097, SET_TEXCOORD1_2F)
{
    SET_VERTEX_ATRIBUTE_TEX_2F(NV097_SET_TEXCOORD1_2F,
                               NV2A_VERTEX_ATTR_TEXTURE1);
}

DEF_METHOD_INC(NV097, SET_TEXCOORD2_2F)
{
    SET_VERTEX_ATRIBUTE_TEX_2F(NV097_SET_TEXCOORD2_2F,
                               NV2A_VERTEX_ATTR_TEXTURE2);
}

DEF_METHOD_INC(NV097, SET_TEXCOORD3_2F)
{
    SET_VERTEX_ATRIBUTE_TEX_2F(NV097_SET_TEXCOORD3_2F,
                               NV2A_VERTEX_ATTR_TEXTURE3);
}

#undef SET_VERTEX_ATRIBUTE_TEX_2F

#define SET_VERTEX_ATTRIBUTE_4UB(command, attr_index)                       \
    do {                                                                   \
        VertexAttribute *attribute = &pg->vertex_attributes[(attr_index)]; \
        pgraph_allocate_inline_buffer_vertices(pg, (attr_index));          \
        attribute->inline_value[0] = (parameter & 0xFF) / 255.0f;          \
        attribute->inline_value[1] = ((parameter >> 8) & 0xFF) / 255.0f;   \
        attribute->inline_value[2] = ((parameter >> 16) & 0xFF) / 255.0f;  \
        attribute->inline_value[3] = ((parameter >> 24) & 0xFF) / 255.0f;  \
    } while (0)

DEF_METHOD_INC(NV097, SET_DIFFUSE_COLOR4UB)
{
    SET_VERTEX_ATTRIBUTE_4UB(NV097_SET_DIFFUSE_COLOR4UB,
                             NV2A_VERTEX_ATTR_DIFFUSE);
}

DEF_METHOD_INC(NV097, SET_SPECULAR_COLOR4UB)
{
    SET_VERTEX_ATTRIBUTE_4UB(NV097_SET_SPECULAR_COLOR4UB,
                             NV2A_VERTEX_ATTR_SPECULAR);
}

#undef SET_VERTEX_ATTRIBUTE_4UB

DEF_METHOD_INC(NV097, SET_VERTEX_DATA_ARRAY_FORMAT)
{
    int slot = (method - NV097_SET_VERTEX_DATA_ARRAY_FORMAT) / 4;
    VertexAttribute *attr = &pg->vertex_attributes[slot];
    attr->format = GET_MASK(parameter, NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE);
    attr->count = GET_MASK(parameter, NV097_SET_VERTEX_DATA_ARRAY_FORMAT_SIZE);
    attr->stride = GET_MASK(parameter,
                            NV097_SET_VERTEX_DATA_ARRAY_FORMAT_STRIDE);

    NV2A_DPRINTF("vertex data array format=%d, count=%d, stride=%d\n",
                 attr->format, attr->count, attr->stride);

    switch (attr->format) {
    case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_UB_D3D:
        attr->size = 1;
        assert(attr->count == 4);
        break;
    case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_UB_OGL:
        attr->size = 1;
        break;
    case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_S1:
        attr->size = 2;
        break;
    case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F:
        attr->size = 4;
        break;
    case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_S32K:
        attr->size = 2;
        break;
    case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_CMP:
        /* 3 signed, normalized components packed in 32-bits. (11,11,10) */
        attr->size = 4;
        assert(attr->count == 1);
        break;
    default:
        fprintf(stderr, "Unknown vertex type: 0x%x\n", attr->format);
        assert(false);
        break;
    }

    if (attr->format == NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_CMP) {
        pg->compressed_attrs |= (1 << slot);
    } else {
        pg->compressed_attrs &= ~(1 << slot);
    }
}

DEF_METHOD_INC(NV097, SET_VERTEX_DATA_ARRAY_OFFSET)
{
    int slot = (method - NV097_SET_VERTEX_DATA_ARRAY_OFFSET) / 4;

    pg->vertex_attributes[slot].dma_select = parameter & 0x80000000;
    pg->vertex_attributes[slot].offset = parameter & 0x7fffffff;
}

DEF_METHOD(NV097, SET_LOGIC_OP_ENABLE)
{
    PG_SET_MASK(NV_PGRAPH_BLEND, NV_PGRAPH_BLEND_LOGICOP_ENABLE,
             parameter);
}

DEF_METHOD(NV097, SET_LOGIC_OP)
{
    PG_SET_MASK(NV_PGRAPH_BLEND, NV_PGRAPH_BLEND_LOGICOP,
             parameter & 0xF);
}

DEF_METHOD(NV097, CLEAR_REPORT_VALUE)
{
    d->pgraph.renderer->ops.clear_report_value(d);
}

DEF_METHOD(NV097, SET_ZPASS_PIXEL_COUNT_ENABLE)
{
    pg->zpass_pixel_count_enable = parameter;
}

DEF_METHOD(NV097, GET_REPORT)
{
    uint8_t type = GET_MASK(parameter, NV097_GET_REPORT_TYPE);
    assert(type == NV097_GET_REPORT_TYPE_ZPASS_PIXEL_CNT);

    d->pgraph.renderer->ops.get_report(d, parameter);
}

DEF_METHOD_INC(NV097, SET_EYE_DIRECTION)
{
    int slot = (method - NV097_SET_EYE_DIRECTION) / 4;
    pg->ltctxa[NV_IGRAPH_XF_LTCTXA_EYED][slot] = parameter;
    pg->ltctxa_dirty[NV_IGRAPH_XF_LTCTXA_EYED] = true;
}

DEF_METHOD(NV097, SET_BEGIN_END)
{
    if (parameter == NV097_SET_BEGIN_END_OP_END) {
        if (pg->primitive_mode == PRIM_TYPE_INVALID) {
            NV2A_DPRINTF("End without Begin!\n");
            pgraph_reset_inline_buffers(pg);
            return;
        }
        nv2a_profile_inc_counter(NV2A_PROF_BEGIN_ENDS);
        d->pgraph.renderer->ops.draw_end(d);
        pgraph_reset_inline_buffers(pg);
        pg->primitive_mode = PRIM_TYPE_INVALID;
    } else {
        if (pg->primitive_mode != PRIM_TYPE_INVALID) {
            NV2A_DPRINTF("Begin without End!\n");
            return;
        }
        assert(parameter <= NV097_SET_BEGIN_END_OP_POLYGON);
        pg->primitive_mode = parameter;
        pgraph_reset_inline_buffers(pg);
        d->pgraph.renderer->ops.draw_begin(d);
    }
}

DEF_METHOD(NV097, SET_TEXTURE_OFFSET)
{
    int slot = (method - NV097_SET_TEXTURE_OFFSET) / 64;
    pgraph_reg_w(pg, NV_PGRAPH_TEXOFFSET0 + slot * 4, parameter);
    pg->texture_dirty[slot] = true;
}

DEF_METHOD(NV097, SET_TEXTURE_FORMAT)
{
    int slot = (method - NV097_SET_TEXTURE_FORMAT) / 64;

    bool dma_select =
        GET_MASK(parameter, NV097_SET_TEXTURE_FORMAT_CONTEXT_DMA) == 2;
    bool cubemap =
        GET_MASK(parameter, NV097_SET_TEXTURE_FORMAT_CUBEMAP_ENABLE);
    unsigned int border_source =
        GET_MASK(parameter, NV097_SET_TEXTURE_FORMAT_BORDER_SOURCE);
    unsigned int dimensionality =
        GET_MASK(parameter, NV097_SET_TEXTURE_FORMAT_DIMENSIONALITY);
    unsigned int color_format =
        GET_MASK(parameter, NV097_SET_TEXTURE_FORMAT_COLOR);
    unsigned int levels =
        GET_MASK(parameter, NV097_SET_TEXTURE_FORMAT_MIPMAP_LEVELS);
    unsigned int log_width =
        GET_MASK(parameter, NV097_SET_TEXTURE_FORMAT_BASE_SIZE_U);
    unsigned int log_height =
        GET_MASK(parameter, NV097_SET_TEXTURE_FORMAT_BASE_SIZE_V);
    unsigned int log_depth =
        GET_MASK(parameter, NV097_SET_TEXTURE_FORMAT_BASE_SIZE_P);

    unsigned int reg = NV_PGRAPH_TEXFMT0 + slot * 4;
    PG_SET_MASK(reg, NV_PGRAPH_TEXFMT0_CONTEXT_DMA, dma_select);
    PG_SET_MASK(reg, NV_PGRAPH_TEXFMT0_CUBEMAPENABLE, cubemap);
    PG_SET_MASK(reg, NV_PGRAPH_TEXFMT0_BORDER_SOURCE, border_source);
    PG_SET_MASK(reg, NV_PGRAPH_TEXFMT0_DIMENSIONALITY, dimensionality);
    PG_SET_MASK(reg, NV_PGRAPH_TEXFMT0_COLOR, color_format);
    PG_SET_MASK(reg, NV_PGRAPH_TEXFMT0_MIPMAP_LEVELS, levels);
    PG_SET_MASK(reg, NV_PGRAPH_TEXFMT0_BASE_SIZE_U, log_width);
    PG_SET_MASK(reg, NV_PGRAPH_TEXFMT0_BASE_SIZE_V, log_height);
    PG_SET_MASK(reg, NV_PGRAPH_TEXFMT0_BASE_SIZE_P, log_depth);

    pg->texture_dirty[slot] = true;
}

DEF_METHOD(NV097, SET_TEXTURE_CONTROL0)
{
    int slot = (method - NV097_SET_TEXTURE_CONTROL0) / 64;
    pgraph_reg_w(pg, NV_PGRAPH_TEXCTL0_0 + slot*4, parameter);
    pg->texture_dirty[slot] = true;
}

DEF_METHOD(NV097, SET_TEXTURE_CONTROL1)
{
    int slot = (method - NV097_SET_TEXTURE_CONTROL1) / 64;
    pgraph_reg_w(pg, NV_PGRAPH_TEXCTL1_0 + slot*4, parameter);
    pg->texture_dirty[slot] = true;
}

DEF_METHOD(NV097, SET_TEXTURE_FILTER)
{
    int slot = (method - NV097_SET_TEXTURE_FILTER) / 64;
    pgraph_reg_w(pg, NV_PGRAPH_TEXFILTER0 + slot * 4, parameter);
    pg->texture_dirty[slot] = true;
}

DEF_METHOD(NV097, SET_TEXTURE_IMAGE_RECT)
{
    int slot = (method - NV097_SET_TEXTURE_IMAGE_RECT) / 64;
    pgraph_reg_w(pg, NV_PGRAPH_TEXIMAGERECT0 + slot * 4, parameter);
    pg->texture_dirty[slot] = true;
}

DEF_METHOD(NV097, SET_TEXTURE_PALETTE)
{
    int slot = (method - NV097_SET_TEXTURE_PALETTE) / 64;

    bool dma_select =
        GET_MASK(parameter, NV097_SET_TEXTURE_PALETTE_CONTEXT_DMA) == 1;
    unsigned int length =
        GET_MASK(parameter, NV097_SET_TEXTURE_PALETTE_LENGTH);
    unsigned int offset =
        GET_MASK(parameter, NV097_SET_TEXTURE_PALETTE_OFFSET);

    unsigned int reg = NV_PGRAPH_TEXPALETTE0 + slot * 4;
    PG_SET_MASK(reg, NV_PGRAPH_TEXPALETTE0_CONTEXT_DMA, dma_select);
    PG_SET_MASK(reg, NV_PGRAPH_TEXPALETTE0_LENGTH, length);
    PG_SET_MASK(reg, NV_PGRAPH_TEXPALETTE0_OFFSET, offset);

    pg->texture_dirty[slot] = true;
}

DEF_METHOD(NV097, SET_TEXTURE_BORDER_COLOR)
{
    int slot = (method - NV097_SET_TEXTURE_BORDER_COLOR) / 64;
    pgraph_reg_w(pg, NV_PGRAPH_BORDERCOLOR0 + slot * 4, parameter);
}

DEF_METHOD(NV097, SET_TEXTURE_SET_BUMP_ENV_MAT)
{
    int slot = (method - NV097_SET_TEXTURE_SET_BUMP_ENV_MAT) / 4;
    if (slot < 16) {
        /* discard */
        return;
    }

    slot -= 16;
    const int swizzle[4] = { NV_PGRAPH_BUMPMAT00, NV_PGRAPH_BUMPMAT01,
                             NV_PGRAPH_BUMPMAT11, NV_PGRAPH_BUMPMAT10 };
    pgraph_reg_w(pg, swizzle[slot % 4] + slot / 4, parameter);
}

DEF_METHOD(NV097, SET_TEXTURE_SET_BUMP_ENV_SCALE)
{
    int slot = (method - NV097_SET_TEXTURE_SET_BUMP_ENV_SCALE) / 64;
    if (slot == 0) {
        /* discard */
        return;
    }

    slot--;
    pgraph_reg_w(pg, NV_PGRAPH_BUMPSCALE1 + slot * 4, parameter);
}

DEF_METHOD(NV097, SET_TEXTURE_SET_BUMP_ENV_OFFSET)
{
    int slot = (method - NV097_SET_TEXTURE_SET_BUMP_ENV_OFFSET) / 64;
    if (slot == 0) {
        /* discard */
        return;
    }

    slot--;
    pgraph_reg_w(pg, NV_PGRAPH_BUMPOFFSET1 + slot * 4, parameter);
}

static void pgraph_expand_draw_arrays(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;
    uint32_t start = pg->draw_arrays_start[pg->draw_arrays_length - 1];
    uint32_t count = pg->draw_arrays_count[pg->draw_arrays_length - 1];

    /* Render any previously squashed DRAW_ARRAYS calls. This case would be
     * triggered if a set of BEGIN+DA+END triplets is followed by the
     * BEGIN+DA+ARRAY_ELEMENT+... chain that caused this expansion. */
    if (pg->draw_arrays_length > 1) {
        d->pgraph.renderer->ops.flush_draw(d);
        pgraph_reset_inline_buffers(pg);
    }
    assert((pg->inline_elements_length + count) < NV2A_MAX_BATCH_LENGTH);
    for (unsigned int i = 0; i < count; i++) {
        pg->inline_elements[pg->inline_elements_length++] = start + i;
    }

    pgraph_reset_draw_arrays(pg);
}

void pgraph_check_within_begin_end_block(PGRAPHState *pg)
{
    if (pg->primitive_mode == PRIM_TYPE_INVALID) {
        NV2A_DPRINTF("Vertex data being sent outside of begin/end block!\n");
    }
}

DEF_METHOD_NON_INC(NV097, ARRAY_ELEMENT16)
{
    pgraph_check_within_begin_end_block(pg);

    if (pg->draw_arrays_length) {
        pgraph_expand_draw_arrays(d);
    }

    assert(pg->inline_elements_length < NV2A_MAX_BATCH_LENGTH);
    pg->inline_elements[pg->inline_elements_length++] = parameter & 0xFFFF;
    pg->inline_elements[pg->inline_elements_length++] = parameter >> 16;
}

DEF_METHOD_NON_INC(NV097, ARRAY_ELEMENT32)
{
    pgraph_check_within_begin_end_block(pg);

    if (pg->draw_arrays_length) {
        pgraph_expand_draw_arrays(d);
    }

    assert(pg->inline_elements_length < NV2A_MAX_BATCH_LENGTH);
    pg->inline_elements[pg->inline_elements_length++] = parameter;
}

DEF_METHOD(NV097, DRAW_ARRAYS)
{
    pgraph_check_within_begin_end_block(pg);

    int32_t start = GET_MASK(parameter, NV097_DRAW_ARRAYS_START_INDEX);
    int32_t count = GET_MASK(parameter, NV097_DRAW_ARRAYS_COUNT) + 1;

    if (pg->inline_elements_length) {
        /* FIXME: HW throws an exception if the start index is > 0xFFFF. This
         * would prevent this assert from firing for any reasonable choice of
         * NV2A_MAX_BATCH_LENGTH (which must be larger to accommodate
         * NV097_INLINE_ARRAY anyway)
         */
        assert((pg->inline_elements_length + count) < NV2A_MAX_BATCH_LENGTH);
        assert(!pg->draw_arrays_prevent_connect);

        for (unsigned int i = 0; i < count; i++) {
            pg->inline_elements[pg->inline_elements_length++] = start + i;
        }
        return;
    }

    pg->draw_arrays_min_start = MIN(pg->draw_arrays_min_start, start);
    pg->draw_arrays_max_count = MAX(pg->draw_arrays_max_count, start + count);

    assert(pg->draw_arrays_length < ARRAY_SIZE(pg->draw_arrays_start));

    /* Attempt to connect contiguous primitives */
    if (!pg->draw_arrays_prevent_connect && pg->draw_arrays_length > 0) {
        unsigned int last_start =
            pg->draw_arrays_start[pg->draw_arrays_length - 1];
        int32_t *last_count =
            &pg->draw_arrays_count[pg->draw_arrays_length - 1];
        if (start == (last_start + *last_count)) {
            *last_count += count;
            return;
        }
    }

    pg->draw_arrays_start[pg->draw_arrays_length] = start;
    pg->draw_arrays_count[pg->draw_arrays_length] = count;
    pg->draw_arrays_length++;
    pg->draw_arrays_prevent_connect = false;
}

DEF_METHOD_NON_INC(NV097, INLINE_ARRAY)
{
    pgraph_check_within_begin_end_block(pg);
    assert(pg->inline_array_length < NV2A_MAX_BATCH_LENGTH);
    pg->inline_array[pg->inline_array_length++] = parameter;
}

DEF_METHOD_INC(NV097, SET_EYE_VECTOR)
{
    int slot = (method - NV097_SET_EYE_VECTOR) / 4;
    pgraph_reg_w(pg, NV_PGRAPH_EYEVEC0 + slot * 4, parameter);
}

DEF_METHOD_INC(NV097, SET_VERTEX_DATA2F_M)
{
    int slot = (method - NV097_SET_VERTEX_DATA2F_M) / 4;
    unsigned int part = slot % 2;
    slot /= 2;
    VertexAttribute *attribute = &pg->vertex_attributes[slot];
    pgraph_allocate_inline_buffer_vertices(pg, slot);
    attribute->inline_value[part] = *(float*)&parameter;
    /* FIXME: Should these really be set to 0.0 and 1.0 ? Conditions? */
    attribute->inline_value[2] = 0.0;
    attribute->inline_value[3] = 1.0;
    if ((slot == 0) && (part == 1)) {
        pgraph_finish_inline_buffer_vertex(pg);
    }
}

DEF_METHOD_INC(NV097, SET_VERTEX_DATA4F_M)
{
    int slot = (method - NV097_SET_VERTEX_DATA4F_M) / 4;
    unsigned int part = slot % 4;
    slot /= 4;
    VertexAttribute *attribute = &pg->vertex_attributes[slot];
    pgraph_allocate_inline_buffer_vertices(pg, slot);
    attribute->inline_value[part] = *(float*)&parameter;
    if ((slot == 0) && (part == 3)) {
        pgraph_finish_inline_buffer_vertex(pg);
    }
}

DEF_METHOD_INC(NV097, SET_VERTEX_DATA2S)
{
    int slot = (method - NV097_SET_VERTEX_DATA2S) / 4;
    VertexAttribute *attribute = &pg->vertex_attributes[slot];
    pgraph_allocate_inline_buffer_vertices(pg, slot);
    attribute->inline_value[0] = (float)(int16_t)(parameter & 0xFFFF);
    attribute->inline_value[1] = (float)(int16_t)(parameter >> 16);
    attribute->inline_value[2] = 0.0;
    attribute->inline_value[3] = 1.0;
    if (slot == 0) {
        pgraph_finish_inline_buffer_vertex(pg);
    }
}

DEF_METHOD_INC(NV097, SET_VERTEX_DATA4UB)
{
    int slot = (method - NV097_SET_VERTEX_DATA4UB) / 4;
    VertexAttribute *attribute = &pg->vertex_attributes[slot];
    pgraph_allocate_inline_buffer_vertices(pg, slot);
    attribute->inline_value[0] = (parameter & 0xFF) / 255.0;
    attribute->inline_value[1] = ((parameter >> 8) & 0xFF) / 255.0;
    attribute->inline_value[2] = ((parameter >> 16) & 0xFF) / 255.0;
    attribute->inline_value[3] = ((parameter >> 24) & 0xFF) / 255.0;
    if (slot == 0) {
        pgraph_finish_inline_buffer_vertex(pg);
    }
}

DEF_METHOD_INC(NV097, SET_VERTEX_DATA4S_M)
{
    int slot = (method - NV097_SET_VERTEX_DATA4S_M) / 4;
    unsigned int part = slot % 2;
    slot /= 2;
    VertexAttribute *attribute = &pg->vertex_attributes[slot];
    pgraph_allocate_inline_buffer_vertices(pg, slot);

    attribute->inline_value[part * 2 + 0] = (float)(int16_t)(parameter & 0xFFFF);
    attribute->inline_value[part * 2 + 1] = (float)(int16_t)(parameter >> 16);
    if ((slot == 0) && (part == 1)) {
        pgraph_finish_inline_buffer_vertex(pg);
    }
}

DEF_METHOD(NV097, SET_SEMAPHORE_OFFSET)
{
    pgraph_reg_w(pg, NV_PGRAPH_SEMAPHOREOFFSET, parameter);
}

DEF_METHOD(NV097, BACK_END_WRITE_SEMAPHORE_RELEASE)
{
    d->pgraph.renderer->ops.surface_update(d, false, true, true);

    //qemu_mutex_unlock(&d->pgraph.lock);
    //bql_lock();

    uint32_t semaphore_offset = pgraph_reg_r(pg, NV_PGRAPH_SEMAPHOREOFFSET);

    hwaddr semaphore_dma_len;
    uint8_t *semaphore_data = (uint8_t*)nv_dma_map(d, pg->dma_semaphore,
                                                   &semaphore_dma_len);
    assert(semaphore_offset < semaphore_dma_len);
    semaphore_data += semaphore_offset;

    stl_le_p((uint32_t*)semaphore_data, parameter);

    //qemu_mutex_lock(&d->pgraph.lock);
    //bql_unlock();
}

DEF_METHOD(NV097, SET_ZMIN_MAX_CONTROL)
{
    switch (GET_MASK(parameter, NV097_SET_ZMIN_MAX_CONTROL_ZCLAMP_EN)) {
    case NV097_SET_ZMIN_MAX_CONTROL_ZCLAMP_EN_CULL:
        PG_SET_MASK(NV_PGRAPH_ZCOMPRESSOCCLUDE,
                 NV_PGRAPH_ZCOMPRESSOCCLUDE_ZCLAMP_EN,
                 NV_PGRAPH_ZCOMPRESSOCCLUDE_ZCLAMP_EN_CULL);
        break;
    case NV097_SET_ZMIN_MAX_CONTROL_ZCLAMP_EN_CLAMP:
        PG_SET_MASK(NV_PGRAPH_ZCOMPRESSOCCLUDE,
                 NV_PGRAPH_ZCOMPRESSOCCLUDE_ZCLAMP_EN,
                 NV_PGRAPH_ZCOMPRESSOCCLUDE_ZCLAMP_EN_CLAMP);
        break;
    default:
        /* FIXME: Should raise NV_PGRAPH_NSOURCE_DATA_ERROR_PENDING */
        assert(!"Invalid zclamp value");
        break;
    }
}

DEF_METHOD(NV097, SET_ANTI_ALIASING_CONTROL)
{
    PG_SET_MASK(NV_PGRAPH_ANTIALIASING, NV_PGRAPH_ANTIALIASING_ENABLE,
             GET_MASK(parameter, NV097_SET_ANTI_ALIASING_CONTROL_ENABLE));
    // FIXME: Handle the remaining bits (observed values 0xFFFF0000, 0xFFFF0001)
}

DEF_METHOD(NV097, SET_ZSTENCIL_CLEAR_VALUE)
{
    pgraph_reg_w(pg, NV_PGRAPH_ZSTENCILCLEARVALUE, parameter);
}

DEF_METHOD(NV097, SET_COLOR_CLEAR_VALUE)
{
    pgraph_reg_w(pg, NV_PGRAPH_COLORCLEARVALUE, parameter);
}

DEF_METHOD(NV097, CLEAR_SURFACE)
{
    d->pgraph.renderer->ops.clear_surface(d, parameter);
}

DEF_METHOD(NV097, SET_CLEAR_RECT_HORIZONTAL)
{
    pgraph_reg_w(pg, NV_PGRAPH_CLEARRECTX, parameter);
}

DEF_METHOD(NV097, SET_CLEAR_RECT_VERTICAL)
{
    pgraph_reg_w(pg, NV_PGRAPH_CLEARRECTY, parameter);
}

DEF_METHOD_INC(NV097, SET_SPECULAR_FOG_FACTOR)
{
    int slot = (method - NV097_SET_SPECULAR_FOG_FACTOR) / 4;
    pgraph_reg_w(pg, NV_PGRAPH_SPECFOGFACTOR0 + slot*4, parameter);
}

DEF_METHOD_INC(NV097, SET_SPECULAR_PARAMS_BACK)
{
    int slot = (method - NV097_SET_SPECULAR_PARAMS_BACK) / 4;
    pg->specular_params_back[slot] = *(float *)&parameter;
    if (slot == 5) {
        pg->specular_power_back = reconstruct_specular_power(pg->specular_params_back);
    }
}

DEF_METHOD(NV097, SET_SHADER_CLIP_PLANE_MODE)
{
    pgraph_reg_w(pg, NV_PGRAPH_SHADERCLIPMODE, parameter);
}

DEF_METHOD_INC(NV097, SET_COMBINER_COLOR_OCW)
{
    int slot = (method - NV097_SET_COMBINER_COLOR_OCW) / 4;
    pgraph_reg_w(pg, NV_PGRAPH_COMBINECOLORO0 + slot*4, parameter);
}

DEF_METHOD(NV097, SET_COMBINER_CONTROL)
{
    pgraph_reg_w(pg, NV_PGRAPH_COMBINECTL, parameter);
}

DEF_METHOD(NV097, SET_SHADOW_ZSLOPE_THRESHOLD)
{
    pgraph_reg_w(pg, NV_PGRAPH_SHADOWZSLOPETHRESHOLD, parameter);
    assert(parameter == 0x7F800000); /* FIXME: Unimplemented */
}

DEF_METHOD(NV097, SET_SHADOW_DEPTH_FUNC)
{
    PG_SET_MASK(NV_PGRAPH_SHADOWCTL, NV_PGRAPH_SHADOWCTL_SHADOW_ZFUNC,
             parameter);
}

DEF_METHOD(NV097, SET_SHADER_STAGE_PROGRAM)
{
    pgraph_reg_w(pg, NV_PGRAPH_SHADERPROG, parameter);
}

DEF_METHOD(NV097, SET_DOT_RGBMAPPING)
{
    PG_SET_MASK(NV_PGRAPH_SHADERCTL, 0xFFF,
             GET_MASK(parameter, 0xFFF));
}

DEF_METHOD(NV097, SET_SHADER_OTHER_STAGE_INPUT)
{
    PG_SET_MASK(NV_PGRAPH_SHADERCTL, 0xFFFF000,
             GET_MASK(parameter, 0xFFFF000));
}

DEF_METHOD_INC(NV097, SET_TRANSFORM_DATA)
{
    int slot = (method - NV097_SET_TRANSFORM_DATA) / 4;
    pg->vertex_state_shader_v0[slot] = parameter;
}

DEF_METHOD(NV097, LAUNCH_TRANSFORM_PROGRAM)
{
    unsigned int program_start = parameter;
    assert(program_start < NV2A_MAX_TRANSFORM_PROGRAM_LENGTH);
    Nv2aVshProgram program;
    Nv2aVshParseResult result = nv2a_vsh_parse_program(
            &program,
            pg->program_data[program_start],
            NV2A_MAX_TRANSFORM_PROGRAM_LENGTH - program_start);
    assert(result == NV2AVPR_SUCCESS);

    Nv2aVshCPUXVSSExecutionState state_linkage;
    Nv2aVshExecutionState state = nv2a_vsh_emu_initialize_xss_execution_state(
            &state_linkage, (float*)pg->vsh_constants);
    memcpy(state_linkage.input_regs, pg->vertex_state_shader_v0, sizeof(pg->vertex_state_shader_v0));

    nv2a_vsh_emu_execute_track_context_writes(&state, &program, pg->vsh_constants_dirty);

    nv2a_vsh_program_destroy(&program);
}

DEF_METHOD(NV097, SET_TRANSFORM_EXECUTION_MODE)
{
    PG_SET_MASK(NV_PGRAPH_CSV0_D, NV_PGRAPH_CSV0_D_MODE,
             GET_MASK(parameter,
                      NV097_SET_TRANSFORM_EXECUTION_MODE_MODE));
    PG_SET_MASK(NV_PGRAPH_CSV0_D, NV_PGRAPH_CSV0_D_RANGE_MODE,
             GET_MASK(parameter,
                      NV097_SET_TRANSFORM_EXECUTION_MODE_RANGE_MODE));
}

DEF_METHOD(NV097, SET_TRANSFORM_PROGRAM_CXT_WRITE_EN)
{
    pg->enable_vertex_program_write = parameter;
}

DEF_METHOD(NV097, SET_TRANSFORM_PROGRAM_LOAD)
{
    assert(parameter < NV2A_MAX_TRANSFORM_PROGRAM_LENGTH);
    PG_SET_MASK(NV_PGRAPH_CHEOPS_OFFSET,
             NV_PGRAPH_CHEOPS_OFFSET_PROG_LD_PTR, parameter);
}

DEF_METHOD(NV097, SET_TRANSFORM_PROGRAM_START)
{
    assert(parameter < NV2A_MAX_TRANSFORM_PROGRAM_LENGTH);
    PG_SET_MASK(NV_PGRAPH_CSV0_C,
             NV_PGRAPH_CSV0_C_CHEOPS_PROGRAM_START, parameter);
}

DEF_METHOD(NV097, SET_TRANSFORM_CONSTANT_LOAD)
{
    assert(parameter < NV2A_VERTEXSHADER_CONSTANTS);
    PG_SET_MASK(NV_PGRAPH_CHEOPS_OFFSET,
             NV_PGRAPH_CHEOPS_OFFSET_CONST_LD_PTR, parameter);
}

void pgraph_get_clear_color(PGRAPHState *pg, float rgba[4])
{
    uint32_t clear_color = pgraph_reg_r(pg, NV_PGRAPH_COLORCLEARVALUE);

    float *r = &rgba[0], *g = &rgba[1], *b = &rgba[2], *a = &rgba[3];

    /* Handle RGB */
    switch(pg->surface_shape.color_format) {
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_X1R5G5B5_Z1R5G5B5:
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_X1R5G5B5_O1R5G5B5:
        *r = ((clear_color >> 10) & 0x1F) / 31.0f;
        *g = ((clear_color >> 5) & 0x1F) / 31.0f;
        *b = (clear_color & 0x1F) / 31.0f;
        break;
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_R5G6B5:
        *r = ((clear_color >> 11) & 0x1F) / 31.0f;
        *g = ((clear_color >> 5) & 0x3F) / 63.0f;
        *b = (clear_color & 0x1F) / 31.0f;
        break;
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_X8R8G8B8_Z8R8G8B8:
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_X8R8G8B8_O8R8G8B8:
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_X1A7R8G8B8_Z1A7R8G8B8:
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_X1A7R8G8B8_O1A7R8G8B8:
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_A8R8G8B8:
        *r = ((clear_color >> 16) & 0xFF) / 255.0f;
        *g = ((clear_color >> 8) & 0xFF) / 255.0f;
        *b = (clear_color & 0xFF) / 255.0f;
        break;
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_B8:
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_G8B8:
        /* Xbox D3D doesn't support clearing those */
    default:
        *r = 1.0f;
        *g = 0.0f;
        *b = 1.0f;
        fprintf(stderr, "CLEAR_SURFACE for color_format 0x%x unsupported",
                pg->surface_shape.color_format);
        assert(!"CLEAR_SURFACE not supported for selected surface format");
        break;
    }

    /* Handle alpha */
    switch(pg->surface_shape.color_format) {
    /* FIXME: CLEAR_SURFACE seems to work like memset, so maybe we
     *        also have to clear non-alpha bits with alpha value?
     *        As GL doesn't own those pixels we'd have to do this on
     *        our own in xbox memory.
     */
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_X1A7R8G8B8_Z1A7R8G8B8:
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_X1A7R8G8B8_O1A7R8G8B8:
        *a = ((clear_color >> 24) & 0x7F) / 127.0f;
        assert(!"CLEAR_SURFACE handling for LE_X1A7R8G8B8_Z1A7R8G8B8 and LE_X1A7R8G8B8_O1A7R8G8B8 is untested"); /* Untested */
        break;
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_A8R8G8B8:
        *a = ((clear_color >> 24) & 0xFF) / 255.0f;
        break;
    default:
        *a = 1.0f;
        break;
    }
}

void pgraph_get_clear_depth_stencil_value(PGRAPHState *pg, float *depth,
                                          int *stencil)
{
    uint32_t clear_zstencil =
        pgraph_reg_r(pg, NV_PGRAPH_ZSTENCILCLEARVALUE);
    *stencil = 0;
    *depth = 1.0;

    switch (pg->surface_shape.zeta_format) {
    case NV097_SET_SURFACE_FORMAT_ZETA_Z16: {
        uint16_t z = clear_zstencil & 0xFFFF;
        /* FIXME: Remove bit for stencil clear? */
        if (pg->surface_shape.z_format) {
            *depth = convert_f16_to_float(z) / f16_max;
        } else {
            *depth = z / (float)0xFFFF;
        }
        break;
    }
    case NV097_SET_SURFACE_FORMAT_ZETA_Z24S8: {
        *stencil = clear_zstencil & 0xFF;
        uint32_t z = clear_zstencil >> 8;
        if (pg->surface_shape.z_format) {
            *depth = convert_f24_to_float(z) / f24_max;
        } else {
            *depth = z / (float)0xFFFFFF;
        }
        break;
    }
    default:
        fprintf(stderr, "Unknown zeta surface format: 0x%x\n",
                pg->surface_shape.zeta_format);
        assert(false);
        break;
    }
}

void pgraph_write_zpass_pixel_cnt_report(NV2AState *d, uint32_t parameter,
                                         uint32_t result)
{
    PGRAPHState *pg = &d->pgraph;

    uint64_t timestamp = 0x0011223344556677; /* FIXME: Update timestamp?! */
    uint32_t done = 0; // FIXME: Check

    hwaddr report_dma_len;
    uint8_t *report_data =
        (uint8_t *)nv_dma_map(d, pg->dma_report, &report_dma_len);

    hwaddr offset = GET_MASK(parameter, NV097_GET_REPORT_OFFSET);
    assert(offset < report_dma_len);
    report_data += offset;

    stq_le_p((uint64_t *)&report_data[0], timestamp);
    stl_le_p((uint32_t *)&report_data[8], result);
    stl_le_p((uint32_t *)&report_data[12], done);

    NV2A_DPRINTF("Report result %d @%" HWADDR_PRIx, result, offset);
}

static void do_wait_for_renderer_switch(CPUState *cpu, run_on_cpu_data data)
{
    NV2AState *d = (NV2AState *)data.host_ptr;

    qemu_mutex_lock(&d->pfifo.lock);
    d->pgraph.renderer_switch_phase = PGRAPH_RENDERER_SWITCH_PHASE_CPU_WAITING;
    pfifo_kick(d);
    qemu_mutex_unlock(&d->pfifo.lock);
    qemu_event_wait(&d->pgraph.renderer_switch_complete);
}

void pgraph_process_pending(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;
    pg->renderer->ops.process_pending(d);

    if (g_config.display.renderer != pg->renderer->type &&
        pg->renderer_switch_phase == PGRAPH_RENDERER_SWITCH_PHASE_IDLE) {
        pg->renderer_switch_phase = PGRAPH_RENDERER_SWITCH_PHASE_STARTED;
        qemu_event_reset(&pg->renderer_switch_complete);
        async_safe_run_on_cpu(qemu_get_cpu(0), do_wait_for_renderer_switch,
                              RUN_ON_CPU_HOST_PTR(d));
    }

    if (pg->renderer_switch_phase == PGRAPH_RENDERER_SWITCH_PHASE_CPU_WAITING) {
        qemu_mutex_lock(&d->pgraph.renderer_lock);
        qemu_mutex_unlock(&d->pfifo.lock);
        qemu_mutex_lock(&d->pgraph.lock);

        if (pg->renderer) {
            qemu_event_reset(&pg->flush_complete);
            pg->flush_pending = true;

            qemu_mutex_lock(&d->pfifo.lock);
            qemu_mutex_unlock(&d->pgraph.lock);

            pg->renderer->ops.process_pending(d);

            qemu_mutex_unlock(&d->pfifo.lock);
            qemu_mutex_lock(&d->pgraph.lock);
            while (pg->framebuffer_in_use) {
                qemu_cond_wait(&d->pgraph.framebuffer_released,
                               &d->pgraph.renderer_lock);
            }

            if (pg->renderer->ops.finalize) {
                pg->renderer->ops.finalize(d);
            }
        }

        init_renderer(pg);

        qemu_mutex_unlock(&d->pgraph.renderer_lock);
        qemu_mutex_unlock(&d->pgraph.lock);
        qemu_mutex_lock(&d->pfifo.lock);

        pg->renderer_switch_phase = PGRAPH_RENDERER_SWITCH_PHASE_IDLE;
        qemu_event_set(&pg->renderer_switch_complete);
    }
}

void pgraph_process_pending_reports(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;
    pg->renderer->ops.process_pending_reports(d);
}

void pgraph_pre_savevm_trigger(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;
    pg->renderer->ops.pre_savevm_trigger(d);
}

void pgraph_pre_savevm_wait(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;
    pg->renderer->ops.pre_savevm_wait(d);
}

void pgraph_pre_shutdown_trigger(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;
    pg->renderer->ops.pre_shutdown_trigger(d);
}

void pgraph_pre_shutdown_wait(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;
    pg->renderer->ops.pre_shutdown_wait(d);
}
