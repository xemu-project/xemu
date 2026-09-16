/*
 * xemu NV2A PGRAPH D3D11 renderer registration.
 *
 * Keep the PGRAPH callback table in C: PGRAPHState and Error are QEMU types,
 * while the implementation behind it is an opaque C++ D3D11 state.  This
 * boundary is intentional and prevents the Windows C++ build from pulling
 * QEMU's GNU-C-only headers into the D3D11 translation units.
 */

#include "qemu/osdep.h"

#include "hw/xbox/nv2a/nv2a_int.h"
#include "hw/xbox/nv2a/pgraph/pgraph.h"
#include "renderer.h"

static void pgraph_d3d11_early_context_init(void)
{
}

static void pgraph_d3d11_init(NV2AState *d, Error **errp)
{
    D3D11RendererState *state = NULL;
    const char *reason = NULL;
    if (!d3d11_renderer_state_init(d, &state, &reason)) {
        error_setg(errp, "D3D11 PGRAPH renderer unavailable: %s",
                   reason != NULL ? reason : "unknown initialization error");
        d->pgraph.d3d11_renderer_state = NULL;
        return;
    }
    d->pgraph.d3d11_renderer_state = state;
    d->pgraph.surface_scale_factor =
        g_config.display.quality.surface_scale < 1 ?
            1 :
            g_config.display.quality.surface_scale;
}

static void pgraph_d3d11_finalize(NV2AState *d)
{
    D3D11RendererState *state = d->pgraph.d3d11_renderer_state;
    d->pgraph.d3d11_renderer_state = NULL;
    d3d11_renderer_state_finalize(state);
}

static void pgraph_d3d11_clear_report_value(NV2AState *d)
{
    (void)d;
}

static void pgraph_d3d11_clear_surface(NV2AState *d, uint32_t parameter)
{
    d3d11_renderer_clear_surface(d->pgraph.d3d11_renderer_state, d, parameter);
}

static void pgraph_d3d11_draw_begin(NV2AState *d)
{
    (void)d;
}

static void pgraph_d3d11_draw_end(NV2AState *d)
{
    d3d11_renderer_draw(d->pgraph.d3d11_renderer_state, d);
}

static void pgraph_d3d11_flip_stall(NV2AState *d)
{
    d3d11_renderer_flip_stall(d->pgraph.d3d11_renderer_state, d);
}

static void pgraph_d3d11_flush_draw(NV2AState *d)
{
    d3d11_renderer_draw(d->pgraph.d3d11_renderer_state, d);
}

static void pgraph_d3d11_get_report(NV2AState *d, uint32_t parameter)
{
    /* Visibility queries are not implemented by the current D3D11 executor.
     * Preserve the established null-renderer behavior until a query-backed
     * implementation exists. */
    pgraph_write_zpass_pixel_cnt_report(d, parameter, 0);
}

static void pgraph_d3d11_image_blit(NV2AState *d)
{
    (void)d;
}

static void pgraph_d3d11_pre_savevm_trigger(NV2AState *d)
{
    d3d11_renderer_flush(d->pgraph.d3d11_renderer_state, d);
}

static void pgraph_d3d11_pre_savevm_wait(NV2AState *d)
{
    (void)d;
}

static void pgraph_d3d11_pre_shutdown_trigger(NV2AState *d)
{
    d3d11_renderer_flush(d->pgraph.d3d11_renderer_state, d);
}

static void pgraph_d3d11_pre_shutdown_wait(NV2AState *d)
{
    (void)d;
}

static void pgraph_d3d11_process_pending(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;
    if (!qatomic_read(&pg->sync_pending) && !qatomic_read(&pg->flush_pending)) {
        return;
    }

    /* pgraph_process_pending() enters this callback while holding the FIFO
     * lock.  Match the GL/Vulkan lock hand-off around immediate-context work.
     */
    qemu_mutex_unlock(&d->pfifo.lock);
    qemu_mutex_lock(&pg->lock);
    if (qatomic_read(&pg->sync_pending)) {
        d3d11_renderer_flush(pg->d3d11_renderer_state, d);
        qatomic_set(&pg->sync_pending, false);
        qemu_event_set(&pg->sync_complete);
    }
    if (qatomic_read(&pg->flush_pending)) {
        d3d11_renderer_flush(pg->d3d11_renderer_state, d);
        qatomic_set(&pg->flush_pending, false);
        qemu_event_set(&pg->flush_complete);
    }
    qemu_mutex_unlock(&pg->lock);
    qemu_mutex_lock(&d->pfifo.lock);
}

static void pgraph_d3d11_process_pending_reports(NV2AState *d)
{
    (void)d;
}

static void pgraph_d3d11_surface_update(NV2AState *d, bool upload,
                                        bool color_write, bool zeta_write)
{
    d3d11_renderer_surface_update(d->pgraph.d3d11_renderer_state, d, upload,
                                  color_write, zeta_write);
}

static void pgraph_d3d11_set_surface_scale_factor(NV2AState *d,
                                                  unsigned int scale)
{
    /* The common PGRAPH state is the source consumed by both the clear and
     * draw adapters.  Keep the existing configuration/UI contract, while
     * flushing outstanding D3D11 work before changing the dimensions. */
    d3d11_renderer_flush(d->pgraph.d3d11_renderer_state, d);
    g_config.display.quality.surface_scale = scale < 1 ? 1 : scale;
    d->pgraph.surface_scale_factor = g_config.display.quality.surface_scale;
}

static unsigned int pgraph_d3d11_get_surface_scale_factor(NV2AState *d)
{
    return d->pgraph.surface_scale_factor;
}

static bool
pgraph_d3d11_get_framebuffer_surface(NV2AState *d,
                                     NV2AFramebufferSurface *surface)
{
    (void)d;
    (void)surface;
    /* The existing framebuffer ABI exports an OpenGL texture handle only.
     * D3D11/CoreWindow interop has no compatible handoff yet; returning false
     * makes the caller retain its normal safe fallback instead of exposing a
     * foreign or stale native handle. */
    return false;
}

static GPUProperties *pgraph_d3d11_get_gpu_properties(void)
{
    static GPUProperties properties;
    return &properties;
}

static PGRAPHRenderer pgraph_d3d11_renderer = {
    .type = CONFIG_DISPLAY_RENDERER_D3D11,
    .name = "Direct3D 11",
    .ops = {
        .early_context_init = pgraph_d3d11_early_context_init,
        .init = pgraph_d3d11_init,
        .finalize = pgraph_d3d11_finalize,
        .clear_report_value = pgraph_d3d11_clear_report_value,
        .clear_surface = pgraph_d3d11_clear_surface,
        .draw_begin = pgraph_d3d11_draw_begin,
        .draw_end = pgraph_d3d11_draw_end,
        .flip_stall = pgraph_d3d11_flip_stall,
        .flush_draw = pgraph_d3d11_flush_draw,
        .get_report = pgraph_d3d11_get_report,
        .image_blit = pgraph_d3d11_image_blit,
        .pre_savevm_trigger = pgraph_d3d11_pre_savevm_trigger,
        .pre_savevm_wait = pgraph_d3d11_pre_savevm_wait,
        .pre_shutdown_trigger = pgraph_d3d11_pre_shutdown_trigger,
        .pre_shutdown_wait = pgraph_d3d11_pre_shutdown_wait,
        .process_pending = pgraph_d3d11_process_pending,
        .process_pending_reports = pgraph_d3d11_process_pending_reports,
        .surface_update = pgraph_d3d11_surface_update,
        .set_surface_scale_factor = pgraph_d3d11_set_surface_scale_factor,
        .get_surface_scale_factor = pgraph_d3d11_get_surface_scale_factor,
        .get_framebuffer_surface = pgraph_d3d11_get_framebuffer_surface,
        .get_gpu_properties = pgraph_d3d11_get_gpu_properties,
    },
};

static void __attribute__((constructor)) register_renderer(void)
{
    pgraph_renderer_register(&pgraph_d3d11_renderer);
}
