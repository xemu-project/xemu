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

#include <hw/xbox/nv2a/nv2a_int.h>
#include "renderer.h"

static void process_pending_report(NV2AState *d, QueryReport *report)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHGLState *r = pg->gl_renderer_state;

    if (report->clear) {
        r->zpass_pixel_count_result = 0;
        return;
    }

    uint8_t type = GET_MASK(report->parameter, NV097_GET_REPORT_TYPE);
    assert(type == NV097_GET_REPORT_TYPE_ZPASS_PIXEL_CNT);

    /* FIXME: Multisampling affects this (both: OGL and Xbox GPU),
     *        not sure if CLEARs also count
     */
    /* FIXME: What about clipping regions etc? */
    for (int i = 0; i < report->query_count; i++) {
        GLuint gl_query_result = 0;
        glGetQueryObjectuiv(report->queries[i], GL_QUERY_RESULT, &gl_query_result);
        gl_query_result /= pg->surface_scale_factor * pg->surface_scale_factor;
        r->zpass_pixel_count_result += gl_query_result;
    }

    if (report->query_count) {
        glDeleteQueries(report->query_count, report->queries);
        g_free(report->queries);
    }

    pgraph_write_zpass_pixel_cnt_report(d, report->parameter, r->zpass_pixel_count_result);
}

void pgraph_gl_process_pending_reports(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHGLState *r = pg->gl_renderer_state;
    QueryReport *report, *next;

    QSIMPLEQ_FOREACH_SAFE(report, &r->report_queue, entry, next) {
        process_pending_report(d, report);
        QSIMPLEQ_REMOVE_HEAD(&r->report_queue, entry);
        g_free(report);
    }
}

void pgraph_gl_clear_report_value(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHGLState *r = pg->gl_renderer_state;

    /* FIXME: Does this have a value in parameter? Also does this (also?) modify
     *        the report memory block?
     */
    if (r->gl_zpass_pixel_count_query_count) {
        glDeleteQueries(r->gl_zpass_pixel_count_query_count,
                        r->gl_zpass_pixel_count_queries);
        r->gl_zpass_pixel_count_query_count = 0;
    }

    QueryReport *report = g_malloc(sizeof(QueryReport));
    report->clear = true;
    QSIMPLEQ_INSERT_TAIL(&r->report_queue, report, entry);
}

void pgraph_gl_init_reports(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHGLState *r = pg->gl_renderer_state;

    QSIMPLEQ_INIT(&r->report_queue);
}

void pgraph_gl_get_report(NV2AState *d, uint32_t parameter)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHGLState *r = pg->gl_renderer_state;

    QueryReport *report = g_malloc(sizeof(QueryReport));
    report->clear = false;
    report->parameter = parameter;
    report->query_count = r->gl_zpass_pixel_count_query_count;
    report->queries = r->gl_zpass_pixel_count_queries;
    QSIMPLEQ_INSERT_TAIL(&r->report_queue, report, entry);

    r->gl_zpass_pixel_count_query_count = 0;
    r->gl_zpass_pixel_count_queries = NULL;
}

void pgraph_gl_finalize_reports(PGRAPHState *pg)
{
    PGRAPHGLState *r = pg->gl_renderer_state;

    QueryReport *report, *next;
    QSIMPLEQ_FOREACH_SAFE (report, &r->report_queue, entry, next) {
        if (report->query_count) {
            glDeleteQueries(report->query_count, report->queries);
        }
        QSIMPLEQ_REMOVE_HEAD(&r->report_queue, entry);
        g_free(report);
    }

    if (r->gl_zpass_pixel_count_query_count) {
        glDeleteQueries(r->gl_zpass_pixel_count_query_count,
                        r->gl_zpass_pixel_count_queries);
        r->gl_zpass_pixel_count_query_count = 0;
    }
}
