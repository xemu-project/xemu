/*
 * QEMU sPAPR Dynamic DMA windows support
 *
 * Copyright (c) 2015 Alexey Kardashevskiy, IBM Corporation.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License,
 *  or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "qemu/module.h"
#include "hw/ppc/spapr.h"
#include "hw/pci-host/spapr.h"
#include "trace.h"

static int spapr_phb_get_active_win_num_cb(Object *child, void *opaque)
{
    SpaprTceTable *tcet;

    tcet = (SpaprTceTable *) object_dynamic_cast(child, TYPE_SPAPR_TCE_TABLE);
    if (tcet && tcet->nb_table) {
        ++*(unsigned *)opaque;
    }
    return 0;
}

static unsigned spapr_phb_get_active_win_num(SpaprPhbState *sphb)
{
    unsigned ret = 0;

    object_child_foreach(OBJECT(sphb), spapr_phb_get_active_win_num_cb, &ret);

    return ret;
}

static int spapr_phb_get_free_liobn_cb(Object *child, void *opaque)
{
    SpaprTceTable *tcet;

    tcet = (SpaprTceTable *) object_dynamic_cast(child, TYPE_SPAPR_TCE_TABLE);
    if (tcet && !tcet->nb_table) {
        *(uint32_t *)opaque = tcet->liobn;
        return 1;
    }
    return 0;
}

static unsigned spapr_phb_get_free_liobn(SpaprPhbState *sphb)
{
    uint32_t liobn = 0;

    object_child_foreach(OBJECT(sphb), spapr_phb_get_free_liobn_cb, &liobn);

    return liobn;
}

static uint32_t spapr_page_mask_to_query_mask(uint64_t page_mask)
{
    int i;
    uint32_t mask = 0;
    const struct { int shift; uint32_t mask; } masks[] = {
        { 12, RTAS_DDW_PGSIZE_4K },
        { 16, RTAS_DDW_PGSIZE_64K },
        { 21, RTAS_DDW_PGSIZE_2M },
        { 24, RTAS_DDW_PGSIZE_16M },
        { 25, RTAS_DDW_PGSIZE_32M },
        { 26, RTAS_DDW_PGSIZE_64M },
        { 27, RTAS_DDW_PGSIZE_128M },
        { 28, RTAS_DDW_PGSIZE_256M },
        { 34, RTAS_DDW_PGSIZE_16G },
    };

    for (i = 0; i < ARRAY_SIZE(masks); ++i) {
        if (page_mask & (1ULL << masks[i].shift)) {
            mask |= masks[i].mask;
        }
    }

    return mask;
}

static void rtas_ibm_query_pe_dma_window(PowerPCCPU *cpu,
                                         SpaprMachineState *spapr,
                                         uint32_t token, uint32_t nargs,
                                         target_ulong args,
                                         uint32_t nret, target_ulong rets)
{
    SpaprPhbState *sphb;
    uint64_t buid;
    uint32_t avail, addr, pgmask = 0;

    if ((nargs != 3) || ((nret != 5) && (nret != 6))) {
        goto param_error_exit;
    }

    buid = ((uint64_t)rtas_ld(args, 1) << 32) | rtas_ld(args, 2);
    addr = rtas_ld(args, 0);
    sphb = spapr_pci_find_phb(spapr, buid);
    if (!sphb || !sphb->ddw_enabled) {
        goto param_error_exit;
    }

    /* Translate page mask to LoPAPR format */
    pgmask = spapr_page_mask_to_query_mask(sphb->page_size_mask);

    avail = SPAPR_PCI_DMA_MAX_WINDOWS - spapr_phb_get_active_win_num(sphb);

    rtas_st(rets, 0, RTAS_OUT_SUCCESS);
    rtas_st(rets, 1, avail);
    if (nret == 6) {
        /*
         * Set the Max TCE number as 1<<(58-21) = 0x20.0000.0000
         * 1<<59 is the huge window start and 21 is 2M page shift.
         */
        rtas_st(rets, 2, 0x00000020);
        rtas_st(rets, 3, 0x00000000);
        rtas_st(rets, 4, pgmask);
        rtas_st(rets, 5, 0); /* DMA migration mask, not supported */
    } else {
        rtas_st(rets, 2, 0x80000000);
        rtas_st(rets, 3, pgmask);
        rtas_st(rets, 4, 0); /* DMA migration mask, not supported */
    }

    trace_spapr_iommu_ddw_query(buid, addr, avail, 0x80000000, pgmask);
    return;

param_error_exit:
    rtas_st(rets, 0, RTAS_OUT_PARAM_ERROR);
}

static void rtas_ibm_create_pe_dma_window(PowerPCCPU *cpu,
                                          SpaprMachineState *spapr,
                                          uint32_t token, uint32_t nargs,
                                          target_ulong args,
                                          uint32_t nret, target_ulong rets)
{
    SpaprPhbState *sphb;
    SpaprTceTable *tcet = NULL;
    uint32_t addr, page_shift, window_shift, liobn;
    uint64_t buid, win_addr;
    int windows;

    if ((nargs != 5) || (nret != 4)) {
        goto param_error_exit;
    }

    buid = ((uint64_t)rtas_ld(args, 1) << 32) | rtas_ld(args, 2);
    addr = rtas_ld(args, 0);
    sphb = spapr_pci_find_phb(spapr, buid);
    if (!sphb || !sphb->ddw_enabled) {
        goto param_error_exit;
    }

    page_shift = rtas_ld(args, 3);
    window_shift = rtas_ld(args, 4);
    liobn = spapr_phb_get_free_liobn(sphb);
    windows = spapr_phb_get_active_win_num(sphb);

    if (!(sphb->page_size_mask & (1ULL << page_shift)) ||
        (window_shift < page_shift)) {
        goto param_error_exit;
    }

    if (!liobn || !sphb->ddw_enabled || windows == SPAPR_PCI_DMA_MAX_WINDOWS) {
        goto hw_error_exit;
    }

    tcet = spapr_tce_find_by_liobn(liobn);
    if (!tcet) {
        goto hw_error_exit;
    }

    win_addr = (windows == 0) ? sphb->dma_win_addr : sphb->dma64_win_addr;
    /*
     * We have just created a window, we know for the fact that it is empty,
     * use a hack to avoid iterating over the table as it is quite possible
     * to have billions of TCEs, all empty.
     * Note that we cannot delay this to the first H_PUT_TCE as this hcall is
     * mostly likely to be handled in KVM so QEMU just does not know if it
     * happened.
     */
    tcet->skipping_replay = true;
    spapr_tce_table_enable(tcet, page_shift, win_addr,
                           1ULL << (window_shift - page_shift));
    tcet->skipping_replay = false;
    if (!tcet->nb_table) {
        goto hw_error_exit;
    }

    trace_spapr_iommu_ddw_create(buid, addr, 1ULL << page_shift,
                                 1ULL << window_shift, tcet->bus_offset, liobn);

    rtas_st(rets, 0, RTAS_OUT_SUCCESS);
    rtas_st(rets, 1, liobn);
    rtas_st(rets, 2, tcet->bus_offset >> 32);
    rtas_st(rets, 3, tcet->bus_offset & ((uint32_t) -1));

    return;

hw_error_exit:
    rtas_st(rets, 0, RTAS_OUT_HW_ERROR);
    return;

param_error_exit:
    rtas_st(rets, 0, RTAS_OUT_PARAM_ERROR);
}

static void rtas_ibm_remove_pe_dma_window(PowerPCCPU *cpu,
                                          SpaprMachineState *spapr,
                                          uint32_t token, uint32_t nargs,
                                          target_ulong args,
                                          uint32_t nret, target_ulong rets)
{
    SpaprPhbState *sphb;
    SpaprTceTable *tcet;
    uint32_t liobn;
    bool def_win_removed;

    if ((nargs != 1) || (nret != 1)) {
        goto param_error_exit;
    }

    liobn = rtas_ld(args, 0);
    tcet = spapr_tce_find_by_liobn(liobn);
    if (!tcet) {
        goto param_error_exit;
    }

    sphb = SPAPR_PCI_HOST_BRIDGE(OBJECT(tcet)->parent);
    if (!sphb || !sphb->ddw_enabled || !tcet->nb_table) {
        goto param_error_exit;
    }

    def_win_removed = tcet->def_win;
    spapr_tce_table_disable(tcet);
    trace_spapr_iommu_ddw_remove(liobn);

    /*
     * PAPR+/LoPAPR says:
     * The platform must restore the default DMA window for the PE on a call
     * to the ibm,remove-pe-dma-window RTAS call when all of the following
     * are true:
     * a. The call removes the last DMA window remaining for the PE.
     * b. The DMA window being removed is not the default window
     */
    if (spapr_phb_get_active_win_num(sphb) == 0 && !def_win_removed) {
        spapr_phb_dma_reset(sphb);
        trace_spapr_iommu_ddw_reset(sphb->buid, 0);
    }

    rtas_st(rets, 0, RTAS_OUT_SUCCESS);
    return;

param_error_exit:
    rtas_st(rets, 0, RTAS_OUT_PARAM_ERROR);
}

static void rtas_ibm_reset_pe_dma_window(PowerPCCPU *cpu,
                                         SpaprMachineState *spapr,
                                         uint32_t token, uint32_t nargs,
                                         target_ulong args,
                                         uint32_t nret, target_ulong rets)
{
    SpaprPhbState *sphb;
    uint64_t buid;
    uint32_t addr;

    if ((nargs != 3) || (nret != 1)) {
        goto param_error_exit;
    }

    buid = ((uint64_t)rtas_ld(args, 1) << 32) | rtas_ld(args, 2);
    addr = rtas_ld(args, 0);
    sphb = spapr_pci_find_phb(spapr, buid);
    if (!sphb || !sphb->ddw_enabled) {
        goto param_error_exit;
    }

    spapr_phb_dma_reset(sphb);
    trace_spapr_iommu_ddw_reset(buid, addr);

    rtas_st(rets, 0, RTAS_OUT_SUCCESS);

    return;

param_error_exit:
    rtas_st(rets, 0, RTAS_OUT_PARAM_ERROR);
}

static void spapr_rtas_ddw_init(void)
{
    spapr_rtas_register(RTAS_IBM_QUERY_PE_DMA_WINDOW,
                        "ibm,query-pe-dma-window",
                        rtas_ibm_query_pe_dma_window);
    spapr_rtas_register(RTAS_IBM_CREATE_PE_DMA_WINDOW,
                        "ibm,create-pe-dma-window",
                        rtas_ibm_create_pe_dma_window);
    spapr_rtas_register(RTAS_IBM_REMOVE_PE_DMA_WINDOW,
                        "ibm,remove-pe-dma-window",
                        rtas_ibm_remove_pe_dma_window);
    spapr_rtas_register(RTAS_IBM_RESET_PE_DMA_WINDOW,
                        "ibm,reset-pe-dma-window",
                        rtas_ibm_reset_pe_dma_window);
}

type_init(spapr_rtas_ddw_init)
