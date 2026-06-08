/*
 * Geforce NV2A PGRAPH Null Renderer
 *
 * Copyright (c) 2024 Matt Borgerson
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

#include "qemu/osdep.h"
#include "qemu/thread.h"
#include "hw/hw.h"
#include "hw/xbox/nv2a/nv2a_int.h"

static void pgraph_null_sync(NV2AState *d)
{
    qatomic_set(&d->pgraph.sync_pending, false);
    qemu_event_set(&d->pgraph.sync_complete);
}

static void pgraph_null_flush(NV2AState *d)
{
    qatomic_set(&d->pgraph.flush_pending, false);
    qemu_event_set(&d->pgraph.flush_complete);
}

static void pgraph_null_process_pending(NV2AState *d)
{
    if (
        qatomic_read(&d->pgraph.sync_pending) ||
        qatomic_read(&d->pgraph.flush_pending)
        ) {
        qemu_mutex_unlock(&d->pfifo.lock);
        qemu_mutex_lock(&d->pgraph.lock);
        if (qatomic_read(&d->pgraph.sync_pending)) {
            pgraph_null_sync(d);
        }
        if (qatomic_read(&d->pgraph.flush_pending)) {
            pgraph_null_flush(d);
        }
        qemu_mutex_unlock(&d->pgraph.lock);
        qemu_mutex_lock(&d->pfifo.lock);
    }
}

static void pgraph_null_clear_report_value(NV2AState *d)
{
}

static void pgraph_null_clear_surface(NV2AState *d, uint32_t parameter)
{
}

static void pgraph_null_draw_begin(NV2AState *d)
{
}

static void pgraph_null_draw_end(NV2AState *d)
{
}

static void pgraph_null_flip_stall(NV2AState *d)
{
}

static void pgraph_null_flush_draw(NV2AState *d)
{
}

static void pgraph_null_get_report(NV2AState *d, uint32_t parameter)
{
    pgraph_write_zpass_pixel_cnt_report(d, parameter, 0);
}

static void pgraph_null_image_blit(NV2AState *d)
{
}

static void pgraph_null_pre_savevm_trigger(NV2AState *d)
{
}

static void pgraph_null_pre_savevm_wait(NV2AState *d)
{
}

static void pgraph_null_pre_shutdown_trigger(NV2AState *d)
{
}

static void pgraph_null_pre_shutdown_wait(NV2AState *d)
{
}

static void pgraph_null_process_pending_reports(NV2AState *d)
{
}

static void pgraph_null_surface_update(NV2AState *d, bool upload,
                                       bool color_write, bool zeta_write)
{
}

static void pgraph_null_init(NV2AState *d, Error **errp)
{
    PGRAPHState *pg = &d->pgraph;
    pg->null_renderer_state = NULL;
}

static PGRAPHRenderer pgraph_null_renderer = {
    .type = CONFIG_DISPLAY_RENDERER_NULL,
    .name = "Null",
    .ops = {
        .init = pgraph_null_init,
        .clear_report_value = pgraph_null_clear_report_value,
        .clear_surface = pgraph_null_clear_surface,
        .draw_begin = pgraph_null_draw_begin,
        .draw_end = pgraph_null_draw_end,
        .flip_stall = pgraph_null_flip_stall,
        .flush_draw = pgraph_null_flush_draw,
        .get_report = pgraph_null_get_report,
        .image_blit = pgraph_null_image_blit,
        .pre_savevm_trigger = pgraph_null_pre_savevm_trigger,
        .pre_savevm_wait = pgraph_null_pre_savevm_wait,
        .pre_shutdown_trigger = pgraph_null_pre_shutdown_trigger,
        .pre_shutdown_wait = pgraph_null_pre_shutdown_wait,
        .process_pending = pgraph_null_process_pending,
        .process_pending_reports = pgraph_null_process_pending_reports,
        .surface_update = pgraph_null_surface_update,
    }
};

static void __attribute__((constructor)) register_renderer(void)
{
    pgraph_renderer_register(&pgraph_null_renderer);
}
