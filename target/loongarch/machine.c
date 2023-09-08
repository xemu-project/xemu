/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * QEMU LoongArch Machine State
 *
 * Copyright (c) 2021 Loongson Technology Corporation Limited
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "migration/cpu.h"
#include "internals.h"

/* TLB state */
const VMStateDescription vmstate_tlb = {
    .name = "cpu/tlb",
    .version_id = 0,
    .minimum_version_id = 0,
    .fields = (VMStateField[]) {
        VMSTATE_UINT64(tlb_misc, LoongArchTLB),
        VMSTATE_UINT64(tlb_entry0, LoongArchTLB),
        VMSTATE_UINT64(tlb_entry1, LoongArchTLB),
        VMSTATE_END_OF_LIST()
    }
};

/* LoongArch CPU state */

const VMStateDescription vmstate_loongarch_cpu = {
    .name = "cpu",
    .version_id = 0,
    .minimum_version_id = 0,
    .fields = (VMStateField[]) {

        VMSTATE_UINTTL_ARRAY(env.gpr, LoongArchCPU, 32),
        VMSTATE_UINTTL(env.pc, LoongArchCPU),
        VMSTATE_UINT64_ARRAY(env.fpr, LoongArchCPU, 32),
        VMSTATE_UINT32(env.fcsr0, LoongArchCPU),
        VMSTATE_BOOL_ARRAY(env.cf, LoongArchCPU, 8),

        /* Remaining CSRs */
        VMSTATE_UINT64(env.CSR_CRMD, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_PRMD, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_EUEN, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_MISC, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_ECFG, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_ESTAT, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_ERA, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_BADV, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_BADI, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_EENTRY, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_TLBIDX, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_TLBEHI, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_TLBELO0, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_TLBELO1, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_ASID, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_PGDL, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_PGDH, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_PGD, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_PWCL, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_PWCH, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_STLBPS, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_RVACFG, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_PRCFG1, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_PRCFG2, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_PRCFG3, LoongArchCPU),
        VMSTATE_UINT64_ARRAY(env.CSR_SAVE, LoongArchCPU, 16),
        VMSTATE_UINT64(env.CSR_TID, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_TCFG, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_TVAL, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_CNTC, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_TICLR, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_LLBCTL, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_IMPCTL1, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_IMPCTL2, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_TLBRENTRY, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_TLBRBADV, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_TLBRERA, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_TLBRSAVE, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_TLBRELO0, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_TLBRELO1, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_TLBREHI, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_TLBRPRMD, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_MERRCTL, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_MERRINFO1, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_MERRINFO2, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_MERRENTRY, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_MERRERA, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_MERRSAVE, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_CTAG, LoongArchCPU),
        VMSTATE_UINT64_ARRAY(env.CSR_DMW, LoongArchCPU, 4),

        /* Debug CSRs */
        VMSTATE_UINT64(env.CSR_DBG, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_DERA, LoongArchCPU),
        VMSTATE_UINT64(env.CSR_DSAVE, LoongArchCPU),
        /* TLB */
        VMSTATE_STRUCT_ARRAY(env.tlb, LoongArchCPU, LOONGARCH_TLB_MAX,
                             0, vmstate_tlb, LoongArchTLB),

        VMSTATE_END_OF_LIST()
    },
};
