/*
 * Geforce NV2A PGRAPH Vulkan Renderer
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

#include "renderer.h"

void pgraph_vk_init_reports(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    QSIMPLEQ_INIT(&r->report_queue);
    r->num_queries_in_flight = 0;
    r->max_queries_in_flight = 1024;
    r->new_query_needed = false;
    r->query_in_flight = false;
    r->zpass_pixel_count_result = 0;

    VkQueryPoolCreateInfo pool_create_info = (VkQueryPoolCreateInfo){
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_OCCLUSION,
        .queryCount = r->max_queries_in_flight,
    };
    VK_CHECK(
        vkCreateQueryPool(r->device, &pool_create_info, NULL, &r->query_pool));
}

void pgraph_vk_finalize_reports(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    QueryReport *report;
    while ((report = QSIMPLEQ_FIRST(&r->report_queue)) != NULL) {
        QSIMPLEQ_REMOVE_HEAD(&r->report_queue, entry);
        g_free(report);
    }

    vkDestroyQueryPool(r->device, r->query_pool, NULL);
}

void pgraph_vk_clear_report_value(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHVkState *r = pg->vk_renderer_state;

    QueryReport *report = g_malloc(sizeof(QueryReport)); // FIXME: Pre-allocate
    report->clear = true;
    report->parameter = 0;
    report->query_count = r->num_queries_in_flight;
    QSIMPLEQ_INSERT_TAIL(&r->report_queue, report, entry);

    r->new_query_needed = true;
}

void pgraph_vk_get_report(NV2AState *d, uint32_t parameter)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHVkState *r = pg->vk_renderer_state;

    uint8_t type = GET_MASK(parameter, NV097_GET_REPORT_TYPE);
    assert(type == NV097_GET_REPORT_TYPE_ZPASS_PIXEL_CNT);

    QueryReport *report = g_malloc(sizeof(QueryReport)); // FIXME: Pre-allocate
    report->clear = false;
    report->parameter = parameter;
    report->query_count = r->num_queries_in_flight;
    QSIMPLEQ_INSERT_TAIL(&r->report_queue, report, entry);

    r->new_query_needed = true;
}

void pgraph_vk_process_pending_reports_internal(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHVkState *r = pg->vk_renderer_state;

    NV2A_VK_DGROUP_BEGIN("Processing queries");

    assert(!r->in_command_buffer);

    // Fetch all query results
    g_autofree uint64_t *query_results = NULL;

    if (r->num_queries_in_flight > 0) {
        size_t size_of_results = r->num_queries_in_flight * sizeof(uint64_t);
        query_results = g_malloc_n(r->num_queries_in_flight,
                                   sizeof(uint64_t)); // FIXME: Pre-allocate
        VkResult result;
        do {
            result = vkGetQueryPoolResults(
                r->device, r->query_pool, 0, r->num_queries_in_flight,
                size_of_results, query_results, sizeof(uint64_t),
                VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
        } while (result == VK_NOT_READY);
    }

    // Write out queries
    int num_results_counted = 0;
    const int result_divisor =
        pg->surface_scale_factor * pg->surface_scale_factor;

    QueryReport *report;
    while ((report = QSIMPLEQ_FIRST(&r->report_queue)) != NULL) {
        assert(report->query_count >= num_results_counted);
        assert(report->query_count <= r->num_queries_in_flight);

        while (num_results_counted < report->query_count) {
            r->zpass_pixel_count_result +=
                query_results[num_results_counted++];
        }

        if (report->clear) {
            NV2A_VK_DPRINTF("Cleared");
            r->zpass_pixel_count_result = 0;
        } else {
            pgraph_write_zpass_pixel_cnt_report(
                d, report->parameter,
                r->zpass_pixel_count_result / result_divisor);
        }

        QSIMPLEQ_REMOVE_HEAD(&r->report_queue, entry);
        g_free(report);
    }

    // Add remaining results
    while (num_results_counted < r->num_queries_in_flight) {
        r->zpass_pixel_count_result += query_results[num_results_counted++];
    }

    r->num_queries_in_flight = 0;
    NV2A_VK_DGROUP_END();
}

void pgraph_vk_process_pending_reports(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHVkState *r = pg->vk_renderer_state;

    uint32_t *dma_get = &d->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET];
    uint32_t *dma_put = &d->pfifo.regs[NV_PFIFO_CACHE1_DMA_PUT];

    if (*dma_get == *dma_put && r->in_command_buffer) {
        pgraph_vk_finish(pg, VK_FINISH_REASON_STALLED);
    }
}
