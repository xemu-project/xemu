/*
 * RISC-V Control and Status Registers.
 *
 * Copyright (c) 2016-2017 Sagar Karandikar, sagark@eecs.berkeley.edu
 * Copyright (c) 2017-2018 SiFive, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/timer.h"
#include "cpu.h"
#include "pmu.h"
#include "time_helper.h"
#include "qemu/main-loop.h"
#include "exec/exec-all.h"
#include "sysemu/cpu-timers.h"
#include "qemu/guest-random.h"
#include "qapi/error.h"

/* CSR function table public API */
void riscv_get_csr_ops(int csrno, riscv_csr_operations *ops)
{
    *ops = csr_ops[csrno & (CSR_TABLE_SIZE - 1)];
}

void riscv_set_csr_ops(int csrno, riscv_csr_operations *ops)
{
    csr_ops[csrno & (CSR_TABLE_SIZE - 1)] = *ops;
}

/* Predicates */
static RISCVException fs(CPURISCVState *env, int csrno)
{
#if !defined(CONFIG_USER_ONLY)
    if (!env->debugger && !riscv_cpu_fp_enabled(env) &&
        !RISCV_CPU(env_cpu(env))->cfg.ext_zfinx) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
#endif
    return RISCV_EXCP_NONE;
}

static RISCVException vs(CPURISCVState *env, int csrno)
{
    CPUState *cs = env_cpu(env);
    RISCVCPU *cpu = RISCV_CPU(cs);

    if (env->misa_ext & RVV ||
        cpu->cfg.ext_zve32f || cpu->cfg.ext_zve64f) {
#if !defined(CONFIG_USER_ONLY)
        if (!env->debugger && !riscv_cpu_vector_enabled(env)) {
            return RISCV_EXCP_ILLEGAL_INST;
        }
#endif
        return RISCV_EXCP_NONE;
    }
    return RISCV_EXCP_ILLEGAL_INST;
}

static RISCVException ctr(CPURISCVState *env, int csrno)
{
#if !defined(CONFIG_USER_ONLY)
    CPUState *cs = env_cpu(env);
    RISCVCPU *cpu = RISCV_CPU(cs);
    int ctr_index;
    target_ulong ctr_mask;
    int base_csrno = CSR_CYCLE;
    bool rv32 = riscv_cpu_mxl(env) == MXL_RV32 ? true : false;

    if (rv32 && csrno >= CSR_CYCLEH) {
        /* Offset for RV32 hpmcounternh counters */
        base_csrno += 0x80;
    }
    ctr_index = csrno - base_csrno;
    ctr_mask = BIT(ctr_index);

    if ((csrno >= CSR_CYCLE && csrno <= CSR_INSTRET) ||
        (csrno >= CSR_CYCLEH && csrno <= CSR_INSTRETH)) {
        goto skip_ext_pmu_check;
    }

    if (!(cpu->pmu_avail_ctrs & ctr_mask)) {
        /* No counter is enabled in PMU or the counter is out of range */
        return RISCV_EXCP_ILLEGAL_INST;
    }

skip_ext_pmu_check:

    if (env->priv < PRV_M && !get_field(env->mcounteren, ctr_mask)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    if (riscv_cpu_virt_enabled(env)) {
        if (!get_field(env->hcounteren, ctr_mask) ||
            (env->priv == PRV_U && !get_field(env->scounteren, ctr_mask))) {
            return RISCV_EXCP_VIRT_INSTRUCTION_FAULT;
        }
    }

    if (riscv_has_ext(env, RVS) && env->priv == PRV_U &&
        !get_field(env->scounteren, ctr_mask)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

#endif
    return RISCV_EXCP_NONE;
}

static RISCVException ctr32(CPURISCVState *env, int csrno)
{
    if (riscv_cpu_mxl(env) != MXL_RV32) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    return ctr(env, csrno);
}

#if !defined(CONFIG_USER_ONLY)
static RISCVException mctr(CPURISCVState *env, int csrno)
{
    CPUState *cs = env_cpu(env);
    RISCVCPU *cpu = RISCV_CPU(cs);
    int ctr_index;
    int base_csrno = CSR_MHPMCOUNTER3;

    if ((riscv_cpu_mxl(env) == MXL_RV32) && csrno >= CSR_MCYCLEH) {
        /* Offset for RV32 mhpmcounternh counters */
        base_csrno += 0x80;
    }
    ctr_index = csrno - base_csrno;
    if (!cpu->cfg.pmu_num || ctr_index >= cpu->cfg.pmu_num) {
        /* The PMU is not enabled or counter is out of range*/
        return RISCV_EXCP_ILLEGAL_INST;
    }

    return RISCV_EXCP_NONE;
}

static RISCVException mctr32(CPURISCVState *env, int csrno)
{
    if (riscv_cpu_mxl(env) != MXL_RV32) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    return mctr(env, csrno);
}

static RISCVException sscofpmf(CPURISCVState *env, int csrno)
{
    CPUState *cs = env_cpu(env);
    RISCVCPU *cpu = RISCV_CPU(cs);

    if (!cpu->cfg.ext_sscofpmf) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    return RISCV_EXCP_NONE;
}

static RISCVException any(CPURISCVState *env, int csrno)
{
    return RISCV_EXCP_NONE;
}

static RISCVException any32(CPURISCVState *env, int csrno)
{
    if (riscv_cpu_mxl(env) != MXL_RV32) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    return any(env, csrno);

}

static int aia_any(CPURISCVState *env, int csrno)
{
    RISCVCPU *cpu = env_archcpu(env);

    if (!cpu->cfg.ext_smaia) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    return any(env, csrno);
}

static int aia_any32(CPURISCVState *env, int csrno)
{
    RISCVCPU *cpu = env_archcpu(env);

    if (!cpu->cfg.ext_smaia) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    return any32(env, csrno);
}

static RISCVException smode(CPURISCVState *env, int csrno)
{
    if (riscv_has_ext(env, RVS)) {
        return RISCV_EXCP_NONE;
    }

    return RISCV_EXCP_ILLEGAL_INST;
}

static int smode32(CPURISCVState *env, int csrno)
{
    if (riscv_cpu_mxl(env) != MXL_RV32) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    return smode(env, csrno);
}

static int aia_smode(CPURISCVState *env, int csrno)
{
    RISCVCPU *cpu = env_archcpu(env);

    if (!cpu->cfg.ext_ssaia) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    return smode(env, csrno);
}

static int aia_smode32(CPURISCVState *env, int csrno)
{
    RISCVCPU *cpu = env_archcpu(env);

    if (!cpu->cfg.ext_ssaia) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    return smode32(env, csrno);
}

static RISCVException hmode(CPURISCVState *env, int csrno)
{
    if (riscv_has_ext(env, RVH)) {
        return RISCV_EXCP_NONE;
    }

    return RISCV_EXCP_ILLEGAL_INST;
}

static RISCVException hmode32(CPURISCVState *env, int csrno)
{
    if (riscv_cpu_mxl(env) != MXL_RV32) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    return hmode(env, csrno);

}

static RISCVException umode(CPURISCVState *env, int csrno)
{
    if (riscv_has_ext(env, RVU)) {
        return RISCV_EXCP_NONE;
    }

    return RISCV_EXCP_ILLEGAL_INST;
}

static RISCVException umode32(CPURISCVState *env, int csrno)
{
    if (riscv_cpu_mxl(env) != MXL_RV32) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    return umode(env, csrno);
}

/* Checks if PointerMasking registers could be accessed */
static RISCVException pointer_masking(CPURISCVState *env, int csrno)
{
    /* Check if j-ext is present */
    if (riscv_has_ext(env, RVJ)) {
        return RISCV_EXCP_NONE;
    }
    return RISCV_EXCP_ILLEGAL_INST;
}

static int aia_hmode(CPURISCVState *env, int csrno)
{
    RISCVCPU *cpu = env_archcpu(env);

    if (!cpu->cfg.ext_ssaia) {
        return RISCV_EXCP_ILLEGAL_INST;
     }

     return hmode(env, csrno);
}

static int aia_hmode32(CPURISCVState *env, int csrno)
{
    RISCVCPU *cpu = env_archcpu(env);

    if (!cpu->cfg.ext_ssaia) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    return hmode32(env, csrno);
}

static RISCVException pmp(CPURISCVState *env, int csrno)
{
    if (riscv_feature(env, RISCV_FEATURE_PMP)) {
        return RISCV_EXCP_NONE;
    }

    return RISCV_EXCP_ILLEGAL_INST;
}

static RISCVException epmp(CPURISCVState *env, int csrno)
{
    if (env->priv == PRV_M && riscv_feature(env, RISCV_FEATURE_EPMP)) {
        return RISCV_EXCP_NONE;
    }

    return RISCV_EXCP_ILLEGAL_INST;
}

static RISCVException debug(CPURISCVState *env, int csrno)
{
    if (riscv_feature(env, RISCV_FEATURE_DEBUG)) {
        return RISCV_EXCP_NONE;
    }

    return RISCV_EXCP_ILLEGAL_INST;
}
#endif

static RISCVException seed(CPURISCVState *env, int csrno)
{
    RISCVCPU *cpu = env_archcpu(env);

    if (!cpu->cfg.ext_zkr) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

#if !defined(CONFIG_USER_ONLY)
    /*
     * With a CSR read-write instruction:
     * 1) The seed CSR is always available in machine mode as normal.
     * 2) Attempted access to seed from virtual modes VS and VU always raises
     * an exception(virtual instruction exception only if mseccfg.sseed=1).
     * 3) Without the corresponding access control bit set to 1, any attempted
     * access to seed from U, S or HS modes will raise an illegal instruction
     * exception.
     */
    if (env->priv == PRV_M) {
        return RISCV_EXCP_NONE;
    } else if (riscv_cpu_virt_enabled(env)) {
        if (env->mseccfg & MSECCFG_SSEED) {
            return RISCV_EXCP_VIRT_INSTRUCTION_FAULT;
        } else {
            return RISCV_EXCP_ILLEGAL_INST;
        }
    } else {
        if (env->priv == PRV_S && (env->mseccfg & MSECCFG_SSEED)) {
            return RISCV_EXCP_NONE;
        } else if (env->priv == PRV_U && (env->mseccfg & MSECCFG_USEED)) {
            return RISCV_EXCP_NONE;
        } else {
            return RISCV_EXCP_ILLEGAL_INST;
        }
    }
#else
    return RISCV_EXCP_NONE;
#endif
}

/* User Floating-Point CSRs */
static RISCVException read_fflags(CPURISCVState *env, int csrno,
                                  target_ulong *val)
{
    *val = riscv_cpu_get_fflags(env);
    return RISCV_EXCP_NONE;
}

static RISCVException write_fflags(CPURISCVState *env, int csrno,
                                   target_ulong val)
{
#if !defined(CONFIG_USER_ONLY)
    if (riscv_has_ext(env, RVF)) {
        env->mstatus |= MSTATUS_FS;
    }
#endif
    riscv_cpu_set_fflags(env, val & (FSR_AEXC >> FSR_AEXC_SHIFT));
    return RISCV_EXCP_NONE;
}

static RISCVException read_frm(CPURISCVState *env, int csrno,
                               target_ulong *val)
{
    *val = env->frm;
    return RISCV_EXCP_NONE;
}

static RISCVException write_frm(CPURISCVState *env, int csrno,
                                target_ulong val)
{
#if !defined(CONFIG_USER_ONLY)
    if (riscv_has_ext(env, RVF)) {
        env->mstatus |= MSTATUS_FS;
    }
#endif
    env->frm = val & (FSR_RD >> FSR_RD_SHIFT);
    return RISCV_EXCP_NONE;
}

static RISCVException read_fcsr(CPURISCVState *env, int csrno,
                                target_ulong *val)
{
    *val = (riscv_cpu_get_fflags(env) << FSR_AEXC_SHIFT)
        | (env->frm << FSR_RD_SHIFT);
    return RISCV_EXCP_NONE;
}

static RISCVException write_fcsr(CPURISCVState *env, int csrno,
                                 target_ulong val)
{
#if !defined(CONFIG_USER_ONLY)
    if (riscv_has_ext(env, RVF)) {
        env->mstatus |= MSTATUS_FS;
    }
#endif
    env->frm = (val & FSR_RD) >> FSR_RD_SHIFT;
    riscv_cpu_set_fflags(env, (val & FSR_AEXC) >> FSR_AEXC_SHIFT);
    return RISCV_EXCP_NONE;
}

static RISCVException read_vtype(CPURISCVState *env, int csrno,
                                 target_ulong *val)
{
    uint64_t vill;
    switch (env->xl) {
    case MXL_RV32:
        vill = (uint32_t)env->vill << 31;
        break;
    case MXL_RV64:
        vill = (uint64_t)env->vill << 63;
        break;
    default:
        g_assert_not_reached();
    }
    *val = (target_ulong)vill | env->vtype;
    return RISCV_EXCP_NONE;
}

static RISCVException read_vl(CPURISCVState *env, int csrno,
                              target_ulong *val)
{
    *val = env->vl;
    return RISCV_EXCP_NONE;
}

static int read_vlenb(CPURISCVState *env, int csrno, target_ulong *val)
{
    *val = env_archcpu(env)->cfg.vlen >> 3;
    return RISCV_EXCP_NONE;
}

static RISCVException read_vxrm(CPURISCVState *env, int csrno,
                                target_ulong *val)
{
    *val = env->vxrm;
    return RISCV_EXCP_NONE;
}

static RISCVException write_vxrm(CPURISCVState *env, int csrno,
                                 target_ulong val)
{
#if !defined(CONFIG_USER_ONLY)
    env->mstatus |= MSTATUS_VS;
#endif
    env->vxrm = val;
    return RISCV_EXCP_NONE;
}

static RISCVException read_vxsat(CPURISCVState *env, int csrno,
                                 target_ulong *val)
{
    *val = env->vxsat;
    return RISCV_EXCP_NONE;
}

static RISCVException write_vxsat(CPURISCVState *env, int csrno,
                                  target_ulong val)
{
#if !defined(CONFIG_USER_ONLY)
    env->mstatus |= MSTATUS_VS;
#endif
    env->vxsat = val;
    return RISCV_EXCP_NONE;
}

static RISCVException read_vstart(CPURISCVState *env, int csrno,
                                  target_ulong *val)
{
    *val = env->vstart;
    return RISCV_EXCP_NONE;
}

static RISCVException write_vstart(CPURISCVState *env, int csrno,
                                   target_ulong val)
{
#if !defined(CONFIG_USER_ONLY)
    env->mstatus |= MSTATUS_VS;
#endif
    /*
     * The vstart CSR is defined to have only enough writable bits
     * to hold the largest element index, i.e. lg2(VLEN) bits.
     */
    env->vstart = val & ~(~0ULL << ctzl(env_archcpu(env)->cfg.vlen));
    return RISCV_EXCP_NONE;
}

static int read_vcsr(CPURISCVState *env, int csrno, target_ulong *val)
{
    *val = (env->vxrm << VCSR_VXRM_SHIFT) | (env->vxsat << VCSR_VXSAT_SHIFT);
    return RISCV_EXCP_NONE;
}

static int write_vcsr(CPURISCVState *env, int csrno, target_ulong val)
{
#if !defined(CONFIG_USER_ONLY)
    env->mstatus |= MSTATUS_VS;
#endif
    env->vxrm = (val & VCSR_VXRM) >> VCSR_VXRM_SHIFT;
    env->vxsat = (val & VCSR_VXSAT) >> VCSR_VXSAT_SHIFT;
    return RISCV_EXCP_NONE;
}

/* User Timers and Counters */
static target_ulong get_ticks(bool shift)
{
    int64_t val;
    target_ulong result;

#if !defined(CONFIG_USER_ONLY)
    if (icount_enabled()) {
        val = icount_get();
    } else {
        val = cpu_get_host_ticks();
    }
#else
    val = cpu_get_host_ticks();
#endif

    if (shift) {
        result = val >> 32;
    } else {
        result = val;
    }

    return result;
}

#if defined(CONFIG_USER_ONLY)
static RISCVException read_time(CPURISCVState *env, int csrno,
                                target_ulong *val)
{
    *val = cpu_get_host_ticks();
    return RISCV_EXCP_NONE;
}

static RISCVException read_timeh(CPURISCVState *env, int csrno,
                                 target_ulong *val)
{
    *val = cpu_get_host_ticks() >> 32;
    return RISCV_EXCP_NONE;
}

static int read_hpmcounter(CPURISCVState *env, int csrno, target_ulong *val)
{
    *val = get_ticks(false);
    return RISCV_EXCP_NONE;
}

static int read_hpmcounterh(CPURISCVState *env, int csrno, target_ulong *val)
{
    *val = get_ticks(true);
    return RISCV_EXCP_NONE;
}

#else /* CONFIG_USER_ONLY */

static int read_mhpmevent(CPURISCVState *env, int csrno, target_ulong *val)
{
    int evt_index = csrno - CSR_MCOUNTINHIBIT;

    *val = env->mhpmevent_val[evt_index];

    return RISCV_EXCP_NONE;
}

static int write_mhpmevent(CPURISCVState *env, int csrno, target_ulong val)
{
    int evt_index = csrno - CSR_MCOUNTINHIBIT;
    uint64_t mhpmevt_val = val;

    env->mhpmevent_val[evt_index] = val;

    if (riscv_cpu_mxl(env) == MXL_RV32) {
        mhpmevt_val = mhpmevt_val |
                      ((uint64_t)env->mhpmeventh_val[evt_index] << 32);
    }
    riscv_pmu_update_event_map(env, mhpmevt_val, evt_index);

    return RISCV_EXCP_NONE;
}

static int read_mhpmeventh(CPURISCVState *env, int csrno, target_ulong *val)
{
    int evt_index = csrno - CSR_MHPMEVENT3H + 3;

    *val = env->mhpmeventh_val[evt_index];

    return RISCV_EXCP_NONE;
}

static int write_mhpmeventh(CPURISCVState *env, int csrno, target_ulong val)
{
    int evt_index = csrno - CSR_MHPMEVENT3H + 3;
    uint64_t mhpmevth_val = val;
    uint64_t mhpmevt_val = env->mhpmevent_val[evt_index];

    mhpmevt_val = mhpmevt_val | (mhpmevth_val << 32);
    env->mhpmeventh_val[evt_index] = val;

    riscv_pmu_update_event_map(env, mhpmevt_val, evt_index);

    return RISCV_EXCP_NONE;
}

static int write_mhpmcounter(CPURISCVState *env, int csrno, target_ulong val)
{
    int ctr_idx = csrno - CSR_MCYCLE;
    PMUCTRState *counter = &env->pmu_ctrs[ctr_idx];
    uint64_t mhpmctr_val = val;

    counter->mhpmcounter_val = val;
    if (riscv_pmu_ctr_monitor_cycles(env, ctr_idx) ||
        riscv_pmu_ctr_monitor_instructions(env, ctr_idx)) {
        counter->mhpmcounter_prev = get_ticks(false);
        if (ctr_idx > 2) {
            if (riscv_cpu_mxl(env) == MXL_RV32) {
                mhpmctr_val = mhpmctr_val |
                              ((uint64_t)counter->mhpmcounterh_val << 32);
            }
            riscv_pmu_setup_timer(env, mhpmctr_val, ctr_idx);
        }
     } else {
        /* Other counters can keep incrementing from the given value */
        counter->mhpmcounter_prev = val;
    }

    return RISCV_EXCP_NONE;
}

static int write_mhpmcounterh(CPURISCVState *env, int csrno, target_ulong val)
{
    int ctr_idx = csrno - CSR_MCYCLEH;
    PMUCTRState *counter = &env->pmu_ctrs[ctr_idx];
    uint64_t mhpmctr_val = counter->mhpmcounter_val;
    uint64_t mhpmctrh_val = val;

    counter->mhpmcounterh_val = val;
    mhpmctr_val = mhpmctr_val | (mhpmctrh_val << 32);
    if (riscv_pmu_ctr_monitor_cycles(env, ctr_idx) ||
        riscv_pmu_ctr_monitor_instructions(env, ctr_idx)) {
        counter->mhpmcounterh_prev = get_ticks(true);
        if (ctr_idx > 2) {
            riscv_pmu_setup_timer(env, mhpmctr_val, ctr_idx);
        }
    } else {
        counter->mhpmcounterh_prev = val;
    }

    return RISCV_EXCP_NONE;
}

static RISCVException riscv_pmu_read_ctr(CPURISCVState *env, target_ulong *val,
                                         bool upper_half, uint32_t ctr_idx)
{
    PMUCTRState counter = env->pmu_ctrs[ctr_idx];
    target_ulong ctr_prev = upper_half ? counter.mhpmcounterh_prev :
                                         counter.mhpmcounter_prev;
    target_ulong ctr_val = upper_half ? counter.mhpmcounterh_val :
                                        counter.mhpmcounter_val;

    if (get_field(env->mcountinhibit, BIT(ctr_idx))) {
        /**
         * Counter should not increment if inhibit bit is set. We can't really
         * stop the icount counting. Just return the counter value written by
         * the supervisor to indicate that counter was not incremented.
         */
        if (!counter.started) {
            *val = ctr_val;
            return RISCV_EXCP_NONE;
        } else {
            /* Mark that the counter has been stopped */
            counter.started = false;
        }
    }

    /**
     * The kernel computes the perf delta by subtracting the current value from
     * the value it initialized previously (ctr_val).
     */
    if (riscv_pmu_ctr_monitor_cycles(env, ctr_idx) ||
        riscv_pmu_ctr_monitor_instructions(env, ctr_idx)) {
        *val = get_ticks(upper_half) - ctr_prev + ctr_val;
    } else {
        *val = ctr_val;
    }

    return RISCV_EXCP_NONE;
}

static int read_hpmcounter(CPURISCVState *env, int csrno, target_ulong *val)
{
    uint16_t ctr_index;

    if (csrno >= CSR_MCYCLE && csrno <= CSR_MHPMCOUNTER31) {
        ctr_index = csrno - CSR_MCYCLE;
    } else if (csrno >= CSR_CYCLE && csrno <= CSR_HPMCOUNTER31) {
        ctr_index = csrno - CSR_CYCLE;
    } else {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    return riscv_pmu_read_ctr(env, val, false, ctr_index);
}

static int read_hpmcounterh(CPURISCVState *env, int csrno, target_ulong *val)
{
    uint16_t ctr_index;

    if (csrno >= CSR_MCYCLEH && csrno <= CSR_MHPMCOUNTER31H) {
        ctr_index = csrno - CSR_MCYCLEH;
    } else if (csrno >= CSR_CYCLEH && csrno <= CSR_HPMCOUNTER31H) {
        ctr_index = csrno - CSR_CYCLEH;
    } else {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    return riscv_pmu_read_ctr(env, val, true, ctr_index);
}

static int read_scountovf(CPURISCVState *env, int csrno, target_ulong *val)
{
    int mhpmevt_start = CSR_MHPMEVENT3 - CSR_MCOUNTINHIBIT;
    int i;
    *val = 0;
    target_ulong *mhpm_evt_val;
    uint64_t of_bit_mask;

    if (riscv_cpu_mxl(env) == MXL_RV32) {
        mhpm_evt_val = env->mhpmeventh_val;
        of_bit_mask = MHPMEVENTH_BIT_OF;
    } else {
        mhpm_evt_val = env->mhpmevent_val;
        of_bit_mask = MHPMEVENT_BIT_OF;
    }

    for (i = mhpmevt_start; i < RV_MAX_MHPMEVENTS; i++) {
        if ((get_field(env->mcounteren, BIT(i))) &&
            (mhpm_evt_val[i] & of_bit_mask)) {
                    *val |= BIT(i);
            }
    }

    return RISCV_EXCP_NONE;
}

static RISCVException read_time(CPURISCVState *env, int csrno,
                                target_ulong *val)
{
    uint64_t delta = riscv_cpu_virt_enabled(env) ? env->htimedelta : 0;

    if (!env->rdtime_fn) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    *val = env->rdtime_fn(env->rdtime_fn_arg) + delta;
    return RISCV_EXCP_NONE;
}

static RISCVException read_timeh(CPURISCVState *env, int csrno,
                                 target_ulong *val)
{
    uint64_t delta = riscv_cpu_virt_enabled(env) ? env->htimedelta : 0;

    if (!env->rdtime_fn) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    *val = (env->rdtime_fn(env->rdtime_fn_arg) + delta) >> 32;
    return RISCV_EXCP_NONE;
}

static RISCVException sstc(CPURISCVState *env, int csrno)
{
    CPUState *cs = env_cpu(env);
    RISCVCPU *cpu = RISCV_CPU(cs);
    bool hmode_check = false;

    if (!cpu->cfg.ext_sstc || !env->rdtime_fn) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    if (env->priv == PRV_M) {
        return RISCV_EXCP_NONE;
    }

    /*
     * No need of separate function for rv32 as menvcfg stores both menvcfg
     * menvcfgh for RV32.
     */
    if (!(get_field(env->mcounteren, COUNTEREN_TM) &&
          get_field(env->menvcfg, MENVCFG_STCE))) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    if (riscv_cpu_virt_enabled(env)) {
        if (!(get_field(env->hcounteren, COUNTEREN_TM) &
              get_field(env->henvcfg, HENVCFG_STCE))) {
            return RISCV_EXCP_VIRT_INSTRUCTION_FAULT;
        }
    }

    if ((csrno == CSR_VSTIMECMP) || (csrno == CSR_VSTIMECMPH)) {
        hmode_check = true;
    }

    return hmode_check ? hmode(env, csrno) : smode(env, csrno);
}

static RISCVException sstc_32(CPURISCVState *env, int csrno)
{
    if (riscv_cpu_mxl(env) != MXL_RV32) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    return sstc(env, csrno);
}

static RISCVException read_vstimecmp(CPURISCVState *env, int csrno,
                                    target_ulong *val)
{
    *val = env->vstimecmp;

    return RISCV_EXCP_NONE;
}

static RISCVException read_vstimecmph(CPURISCVState *env, int csrno,
                                    target_ulong *val)
{
    *val = env->vstimecmp >> 32;

    return RISCV_EXCP_NONE;
}

static RISCVException write_vstimecmp(CPURISCVState *env, int csrno,
                                    target_ulong val)
{
    RISCVCPU *cpu = env_archcpu(env);

    if (riscv_cpu_mxl(env) == MXL_RV32) {
        env->vstimecmp = deposit64(env->vstimecmp, 0, 32, (uint64_t)val);
    } else {
        env->vstimecmp = val;
    }

    riscv_timer_write_timecmp(cpu, env->vstimer, env->vstimecmp,
                              env->htimedelta, MIP_VSTIP);

    return RISCV_EXCP_NONE;
}

static RISCVException write_vstimecmph(CPURISCVState *env, int csrno,
                                    target_ulong val)
{
    RISCVCPU *cpu = env_archcpu(env);

    env->vstimecmp = deposit64(env->vstimecmp, 32, 32, (uint64_t)val);
    riscv_timer_write_timecmp(cpu, env->vstimer, env->vstimecmp,
                              env->htimedelta, MIP_VSTIP);

    return RISCV_EXCP_NONE;
}

static RISCVException read_stimecmp(CPURISCVState *env, int csrno,
                                    target_ulong *val)
{
    if (riscv_cpu_virt_enabled(env)) {
        *val = env->vstimecmp;
    } else {
        *val = env->stimecmp;
    }

    return RISCV_EXCP_NONE;
}

static RISCVException read_stimecmph(CPURISCVState *env, int csrno,
                                    target_ulong *val)
{
    if (riscv_cpu_virt_enabled(env)) {
        *val = env->vstimecmp >> 32;
    } else {
        *val = env->stimecmp >> 32;
    }

    return RISCV_EXCP_NONE;
}

static RISCVException write_stimecmp(CPURISCVState *env, int csrno,
                                    target_ulong val)
{
    RISCVCPU *cpu = env_archcpu(env);

    if (riscv_cpu_virt_enabled(env)) {
        return write_vstimecmp(env, csrno, val);
    }

    if (riscv_cpu_mxl(env) == MXL_RV32) {
        env->stimecmp = deposit64(env->stimecmp, 0, 32, (uint64_t)val);
    } else {
        env->stimecmp = val;
    }

    riscv_timer_write_timecmp(cpu, env->stimer, env->stimecmp, 0, MIP_STIP);

    return RISCV_EXCP_NONE;
}

static RISCVException write_stimecmph(CPURISCVState *env, int csrno,
                                    target_ulong val)
{
    RISCVCPU *cpu = env_archcpu(env);

    if (riscv_cpu_virt_enabled(env)) {
        return write_vstimecmph(env, csrno, val);
    }

    env->stimecmp = deposit64(env->stimecmp, 32, 32, (uint64_t)val);
    riscv_timer_write_timecmp(cpu, env->stimer, env->stimecmp, 0, MIP_STIP);

    return RISCV_EXCP_NONE;
}

/* Machine constants */

#define M_MODE_INTERRUPTS  ((uint64_t)(MIP_MSIP | MIP_MTIP | MIP_MEIP))
#define S_MODE_INTERRUPTS  ((uint64_t)(MIP_SSIP | MIP_STIP | MIP_SEIP | \
                                      MIP_LCOFIP))
#define VS_MODE_INTERRUPTS ((uint64_t)(MIP_VSSIP | MIP_VSTIP | MIP_VSEIP))
#define HS_MODE_INTERRUPTS ((uint64_t)(MIP_SGEIP | VS_MODE_INTERRUPTS))

#define VSTOPI_NUM_SRCS 5

static const uint64_t delegable_ints = S_MODE_INTERRUPTS |
                                           VS_MODE_INTERRUPTS;
static const uint64_t vs_delegable_ints = VS_MODE_INTERRUPTS;
static const uint64_t all_ints = M_MODE_INTERRUPTS | S_MODE_INTERRUPTS |
                                     HS_MODE_INTERRUPTS;
#define DELEGABLE_EXCPS ((1ULL << (RISCV_EXCP_INST_ADDR_MIS)) | \
                         (1ULL << (RISCV_EXCP_INST_ACCESS_FAULT)) | \
                         (1ULL << (RISCV_EXCP_ILLEGAL_INST)) | \
                         (1ULL << (RISCV_EXCP_BREAKPOINT)) | \
                         (1ULL << (RISCV_EXCP_LOAD_ADDR_MIS)) | \
                         (1ULL << (RISCV_EXCP_LOAD_ACCESS_FAULT)) | \
                         (1ULL << (RISCV_EXCP_STORE_AMO_ADDR_MIS)) | \
                         (1ULL << (RISCV_EXCP_STORE_AMO_ACCESS_FAULT)) | \
                         (1ULL << (RISCV_EXCP_U_ECALL)) | \
                         (1ULL << (RISCV_EXCP_S_ECALL)) | \
                         (1ULL << (RISCV_EXCP_VS_ECALL)) | \
                         (1ULL << (RISCV_EXCP_M_ECALL)) | \
                         (1ULL << (RISCV_EXCP_INST_PAGE_FAULT)) | \
                         (1ULL << (RISCV_EXCP_LOAD_PAGE_FAULT)) | \
                         (1ULL << (RISCV_EXCP_STORE_PAGE_FAULT)) | \
                         (1ULL << (RISCV_EXCP_INST_GUEST_PAGE_FAULT)) | \
                         (1ULL << (RISCV_EXCP_LOAD_GUEST_ACCESS_FAULT)) | \
                         (1ULL << (RISCV_EXCP_VIRT_INSTRUCTION_FAULT)) | \
                         (1ULL << (RISCV_EXCP_STORE_GUEST_AMO_ACCESS_FAULT)))
static const target_ulong vs_delegable_excps = DELEGABLE_EXCPS &
    ~((1ULL << (RISCV_EXCP_S_ECALL)) |
      (1ULL << (RISCV_EXCP_VS_ECALL)) |
      (1ULL << (RISCV_EXCP_M_ECALL)) |
      (1ULL << (RISCV_EXCP_INST_GUEST_PAGE_FAULT)) |
      (1ULL << (RISCV_EXCP_LOAD_GUEST_ACCESS_FAULT)) |
      (1ULL << (RISCV_EXCP_VIRT_INSTRUCTION_FAULT)) |
      (1ULL << (RISCV_EXCP_STORE_GUEST_AMO_ACCESS_FAULT)));
static const target_ulong sstatus_v1_10_mask = SSTATUS_SIE | SSTATUS_SPIE |
    SSTATUS_UIE | SSTATUS_UPIE | SSTATUS_SPP | SSTATUS_FS | SSTATUS_XS |
    SSTATUS_SUM | SSTATUS_MXR | SSTATUS_VS;
static const target_ulong sip_writable_mask = SIP_SSIP | MIP_USIP | MIP_UEIP |
                                              SIP_LCOFIP;
static const target_ulong hip_writable_mask = MIP_VSSIP;
static const target_ulong hvip_writable_mask = MIP_VSSIP | MIP_VSTIP | MIP_VSEIP;
static const target_ulong vsip_writable_mask = MIP_VSSIP;

static const char valid_vm_1_10_32[16] = {
    [VM_1_10_MBARE] = 1,
    [VM_1_10_SV32] = 1
};

static const char valid_vm_1_10_64[16] = {
    [VM_1_10_MBARE] = 1,
    [VM_1_10_SV39] = 1,
    [VM_1_10_SV48] = 1,
    [VM_1_10_SV57] = 1
};

/* Machine Information Registers */
static RISCVException read_zero(CPURISCVState *env, int csrno,
                                target_ulong *val)
{
    *val = 0;
    return RISCV_EXCP_NONE;
}

static RISCVException write_ignore(CPURISCVState *env, int csrno,
                                   target_ulong val)
{
    return RISCV_EXCP_NONE;
}

static RISCVException read_mvendorid(CPURISCVState *env, int csrno,
                                     target_ulong *val)
{
    CPUState *cs = env_cpu(env);
    RISCVCPU *cpu = RISCV_CPU(cs);

    *val = cpu->cfg.mvendorid;
    return RISCV_EXCP_NONE;
}

static RISCVException read_marchid(CPURISCVState *env, int csrno,
                                   target_ulong *val)
{
    CPUState *cs = env_cpu(env);
    RISCVCPU *cpu = RISCV_CPU(cs);

    *val = cpu->cfg.marchid;
    return RISCV_EXCP_NONE;
}

static RISCVException read_mimpid(CPURISCVState *env, int csrno,
                                  target_ulong *val)
{
    CPUState *cs = env_cpu(env);
    RISCVCPU *cpu = RISCV_CPU(cs);

    *val = cpu->cfg.mimpid;
    return RISCV_EXCP_NONE;
}

static RISCVException read_mhartid(CPURISCVState *env, int csrno,
                                   target_ulong *val)
{
    *val = env->mhartid;
    return RISCV_EXCP_NONE;
}

/* Machine Trap Setup */

/* We do not store SD explicitly, only compute it on demand. */
static uint64_t add_status_sd(RISCVMXL xl, uint64_t status)
{
    if ((status & MSTATUS_FS) == MSTATUS_FS ||
        (status & MSTATUS_VS) == MSTATUS_VS ||
        (status & MSTATUS_XS) == MSTATUS_XS) {
        switch (xl) {
        case MXL_RV32:
            return status | MSTATUS32_SD;
        case MXL_RV64:
            return status | MSTATUS64_SD;
        case MXL_RV128:
            return MSTATUSH128_SD;
        default:
            g_assert_not_reached();
        }
    }
    return status;
}

static RISCVException read_mstatus(CPURISCVState *env, int csrno,
                                   target_ulong *val)
{
    *val = add_status_sd(riscv_cpu_mxl(env), env->mstatus);
    return RISCV_EXCP_NONE;
}

static int validate_vm(CPURISCVState *env, target_ulong vm)
{
    if (riscv_cpu_mxl(env) == MXL_RV32) {
        return valid_vm_1_10_32[vm & 0xf];
    } else {
        return valid_vm_1_10_64[vm & 0xf];
    }
}

static RISCVException write_mstatus(CPURISCVState *env, int csrno,
                                    target_ulong val)
{
    uint64_t mstatus = env->mstatus;
    uint64_t mask = 0;
    RISCVMXL xl = riscv_cpu_mxl(env);

    /* flush tlb on mstatus fields that affect VM */
    if ((val ^ mstatus) & (MSTATUS_MXR | MSTATUS_MPP | MSTATUS_MPV |
            MSTATUS_MPRV | MSTATUS_SUM)) {
        tlb_flush(env_cpu(env));
    }
    mask = MSTATUS_SIE | MSTATUS_SPIE | MSTATUS_MIE | MSTATUS_MPIE |
        MSTATUS_SPP | MSTATUS_MPRV | MSTATUS_SUM |
        MSTATUS_MPP | MSTATUS_MXR | MSTATUS_TVM | MSTATUS_TSR |
        MSTATUS_TW | MSTATUS_VS;

    if (riscv_has_ext(env, RVF)) {
        mask |= MSTATUS_FS;
    }

    if (xl != MXL_RV32 || env->debugger) {
        /*
         * RV32: MPV and GVA are not in mstatus. The current plan is to
         * add them to mstatush. For now, we just don't support it.
         */
        mask |= MSTATUS_MPV | MSTATUS_GVA;
        if ((val & MSTATUS64_UXL) != 0) {
            mask |= MSTATUS64_UXL;
        }
    }

    mstatus = (mstatus & ~mask) | (val & mask);

    if (xl > MXL_RV32) {
        /* SXL field is for now read only */
        mstatus = set_field(mstatus, MSTATUS64_SXL, xl);
    }
    env->mstatus = mstatus;
    env->xl = cpu_recompute_xl(env);

    return RISCV_EXCP_NONE;
}

static RISCVException read_mstatush(CPURISCVState *env, int csrno,
                                    target_ulong *val)
{
    *val = env->mstatus >> 32;
    return RISCV_EXCP_NONE;
}

static RISCVException write_mstatush(CPURISCVState *env, int csrno,
                                     target_ulong val)
{
    uint64_t valh = (uint64_t)val << 32;
    uint64_t mask = MSTATUS_MPV | MSTATUS_GVA;

    if ((valh ^ env->mstatus) & (MSTATUS_MPV)) {
        tlb_flush(env_cpu(env));
    }

    env->mstatus = (env->mstatus & ~mask) | (valh & mask);

    return RISCV_EXCP_NONE;
}

static RISCVException read_mstatus_i128(CPURISCVState *env, int csrno,
                                        Int128 *val)
{
    *val = int128_make128(env->mstatus, add_status_sd(MXL_RV128, env->mstatus));
    return RISCV_EXCP_NONE;
}

static RISCVException read_misa_i128(CPURISCVState *env, int csrno,
                                     Int128 *val)
{
    *val = int128_make128(env->misa_ext, (uint64_t)MXL_RV128 << 62);
    return RISCV_EXCP_NONE;
}

static RISCVException read_misa(CPURISCVState *env, int csrno,
                                target_ulong *val)
{
    target_ulong misa;

    switch (env->misa_mxl) {
    case MXL_RV32:
        misa = (target_ulong)MXL_RV32 << 30;
        break;
#ifdef TARGET_RISCV64
    case MXL_RV64:
        misa = (target_ulong)MXL_RV64 << 62;
        break;
#endif
    default:
        g_assert_not_reached();
    }

    *val = misa | env->misa_ext;
    return RISCV_EXCP_NONE;
}

static RISCVException write_misa(CPURISCVState *env, int csrno,
                                 target_ulong val)
{
    if (!riscv_feature(env, RISCV_FEATURE_MISA)) {
        /* drop write to misa */
        return RISCV_EXCP_NONE;
    }

    /* 'I' or 'E' must be present */
    if (!(val & (RVI | RVE))) {
        /* It is not, drop write to misa */
        return RISCV_EXCP_NONE;
    }

    /* 'E' excludes all other extensions */
    if (val & RVE) {
        /* when we support 'E' we can do "val = RVE;" however
         * for now we just drop writes if 'E' is present.
         */
        return RISCV_EXCP_NONE;
    }

    /*
     * misa.MXL writes are not supported by QEMU.
     * Drop writes to those bits.
     */

    /* Mask extensions that are not supported by this hart */
    val &= env->misa_ext_mask;

    /* Mask extensions that are not supported by QEMU */
    val &= (RVI | RVE | RVM | RVA | RVF | RVD | RVC | RVS | RVU | RVV);

    /* 'D' depends on 'F', so clear 'D' if 'F' is not present */
    if ((val & RVD) && !(val & RVF)) {
        val &= ~RVD;
    }

    /* Suppress 'C' if next instruction is not aligned
     * TODO: this should check next_pc
     */
    if ((val & RVC) && (GETPC() & ~3) != 0) {
        val &= ~RVC;
    }

    /* If nothing changed, do nothing. */
    if (val == env->misa_ext) {
        return RISCV_EXCP_NONE;
    }

    if (!(val & RVF)) {
        env->mstatus &= ~MSTATUS_FS;
    }

    /* flush translation cache */
    tb_flush(env_cpu(env));
    env->misa_ext = val;
    env->xl = riscv_cpu_mxl(env);
    return RISCV_EXCP_NONE;
}

static RISCVException read_medeleg(CPURISCVState *env, int csrno,
                                   target_ulong *val)
{
    *val = env->medeleg;
    return RISCV_EXCP_NONE;
}

static RISCVException write_medeleg(CPURISCVState *env, int csrno,
                                    target_ulong val)
{
    env->medeleg = (env->medeleg & ~DELEGABLE_EXCPS) | (val & DELEGABLE_EXCPS);
    return RISCV_EXCP_NONE;
}

static RISCVException rmw_mideleg64(CPURISCVState *env, int csrno,
                                    uint64_t *ret_val,
                                    uint64_t new_val, uint64_t wr_mask)
{
    uint64_t mask = wr_mask & delegable_ints;

    if (ret_val) {
        *ret_val = env->mideleg;
    }

    env->mideleg = (env->mideleg & ~mask) | (new_val & mask);

    if (riscv_has_ext(env, RVH)) {
        env->mideleg |= HS_MODE_INTERRUPTS;
    }

    return RISCV_EXCP_NONE;
}

static RISCVException rmw_mideleg(CPURISCVState *env, int csrno,
                                  target_ulong *ret_val,
                                  target_ulong new_val, target_ulong wr_mask)
{
    uint64_t rval;
    RISCVException ret;

    ret = rmw_mideleg64(env, csrno, &rval, new_val, wr_mask);
    if (ret_val) {
        *ret_val = rval;
    }

    return ret;
}

static RISCVException rmw_midelegh(CPURISCVState *env, int csrno,
                                   target_ulong *ret_val,
                                   target_ulong new_val,
                                   target_ulong wr_mask)
{
    uint64_t rval;
    RISCVException ret;

    ret = rmw_mideleg64(env, csrno, &rval,
        ((uint64_t)new_val) << 32, ((uint64_t)wr_mask) << 32);
    if (ret_val) {
        *ret_val = rval >> 32;
    }

    return ret;
}

static RISCVException rmw_mie64(CPURISCVState *env, int csrno,
                                uint64_t *ret_val,
                                uint64_t new_val, uint64_t wr_mask)
{
    uint64_t mask = wr_mask & all_ints;

    if (ret_val) {
        *ret_val = env->mie;
    }

    env->mie = (env->mie & ~mask) | (new_val & mask);

    if (!riscv_has_ext(env, RVH)) {
        env->mie &= ~((uint64_t)MIP_SGEIP);
    }

    return RISCV_EXCP_NONE;
}

static RISCVException rmw_mie(CPURISCVState *env, int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val, target_ulong wr_mask)
{
    uint64_t rval;
    RISCVException ret;

    ret = rmw_mie64(env, csrno, &rval, new_val, wr_mask);
    if (ret_val) {
        *ret_val = rval;
    }

    return ret;
}

static RISCVException rmw_mieh(CPURISCVState *env, int csrno,
                               target_ulong *ret_val,
                               target_ulong new_val, target_ulong wr_mask)
{
    uint64_t rval;
    RISCVException ret;

    ret = rmw_mie64(env, csrno, &rval,
        ((uint64_t)new_val) << 32, ((uint64_t)wr_mask) << 32);
    if (ret_val) {
        *ret_val = rval >> 32;
    }

    return ret;
}

static int read_mtopi(CPURISCVState *env, int csrno, target_ulong *val)
{
    int irq;
    uint8_t iprio;

    irq = riscv_cpu_mirq_pending(env);
    if (irq <= 0 || irq > 63) {
        *val = 0;
    } else {
        iprio = env->miprio[irq];
        if (!iprio) {
            if (riscv_cpu_default_priority(irq) > IPRIO_DEFAULT_M) {
                iprio = IPRIO_MMAXIPRIO;
            }
        }
        *val = (irq & TOPI_IID_MASK) << TOPI_IID_SHIFT;
        *val |= iprio;
    }

    return RISCV_EXCP_NONE;
}

static int aia_xlate_vs_csrno(CPURISCVState *env, int csrno)
{
    if (!riscv_cpu_virt_enabled(env)) {
        return csrno;
    }

    switch (csrno) {
    case CSR_SISELECT:
        return CSR_VSISELECT;
    case CSR_SIREG:
        return CSR_VSIREG;
    case CSR_STOPEI:
        return CSR_VSTOPEI;
    default:
        return csrno;
    };
}

static int rmw_xiselect(CPURISCVState *env, int csrno, target_ulong *val,
                        target_ulong new_val, target_ulong wr_mask)
{
    target_ulong *iselect;

    /* Translate CSR number for VS-mode */
    csrno = aia_xlate_vs_csrno(env, csrno);

    /* Find the iselect CSR based on CSR number */
    switch (csrno) {
    case CSR_MISELECT:
        iselect = &env->miselect;
        break;
    case CSR_SISELECT:
        iselect = &env->siselect;
        break;
    case CSR_VSISELECT:
        iselect = &env->vsiselect;
        break;
    default:
         return RISCV_EXCP_ILLEGAL_INST;
    };

    if (val) {
        *val = *iselect;
    }

    wr_mask &= ISELECT_MASK;
    if (wr_mask) {
        *iselect = (*iselect & ~wr_mask) | (new_val & wr_mask);
    }

    return RISCV_EXCP_NONE;
}

static int rmw_iprio(target_ulong xlen,
                     target_ulong iselect, uint8_t *iprio,
                     target_ulong *val, target_ulong new_val,
                     target_ulong wr_mask, int ext_irq_no)
{
    int i, firq, nirqs;
    target_ulong old_val;

    if (iselect < ISELECT_IPRIO0 || ISELECT_IPRIO15 < iselect) {
        return -EINVAL;
    }
    if (xlen != 32 && iselect & 0x1) {
        return -EINVAL;
    }

    nirqs = 4 * (xlen / 32);
    firq = ((iselect - ISELECT_IPRIO0) / (xlen / 32)) * (nirqs);

    old_val = 0;
    for (i = 0; i < nirqs; i++) {
        old_val |= ((target_ulong)iprio[firq + i]) << (IPRIO_IRQ_BITS * i);
    }

    if (val) {
        *val = old_val;
    }

    if (wr_mask) {
        new_val = (old_val & ~wr_mask) | (new_val & wr_mask);
        for (i = 0; i < nirqs; i++) {
            /*
             * M-level and S-level external IRQ priority always read-only
             * zero. This means default priority order is always preferred
             * for M-level and S-level external IRQs.
             */
            if ((firq + i) == ext_irq_no) {
                continue;
            }
            iprio[firq + i] = (new_val >> (IPRIO_IRQ_BITS * i)) & 0xff;
        }
    }

    return 0;
}

static int rmw_xireg(CPURISCVState *env, int csrno, target_ulong *val,
                     target_ulong new_val, target_ulong wr_mask)
{
    bool virt;
    uint8_t *iprio;
    int ret = -EINVAL;
    target_ulong priv, isel, vgein;

    /* Translate CSR number for VS-mode */
    csrno = aia_xlate_vs_csrno(env, csrno);

    /* Decode register details from CSR number */
    virt = false;
    switch (csrno) {
    case CSR_MIREG:
        iprio = env->miprio;
        isel = env->miselect;
        priv = PRV_M;
        break;
    case CSR_SIREG:
        iprio = env->siprio;
        isel = env->siselect;
        priv = PRV_S;
        break;
    case CSR_VSIREG:
        iprio = env->hviprio;
        isel = env->vsiselect;
        priv = PRV_S;
        virt = true;
        break;
    default:
         goto done;
    };

    /* Find the selected guest interrupt file */
    vgein = (virt) ? get_field(env->hstatus, HSTATUS_VGEIN) : 0;

    if (ISELECT_IPRIO0 <= isel && isel <= ISELECT_IPRIO15) {
        /* Local interrupt priority registers not available for VS-mode */
        if (!virt) {
            ret = rmw_iprio(riscv_cpu_mxl_bits(env),
                            isel, iprio, val, new_val, wr_mask,
                            (priv == PRV_M) ? IRQ_M_EXT : IRQ_S_EXT);
        }
    } else if (ISELECT_IMSIC_FIRST <= isel && isel <= ISELECT_IMSIC_LAST) {
        /* IMSIC registers only available when machine implements it. */
        if (env->aia_ireg_rmw_fn[priv]) {
            /* Selected guest interrupt file should not be zero */
            if (virt && (!vgein || env->geilen < vgein)) {
                goto done;
            }
            /* Call machine specific IMSIC register emulation */
            ret = env->aia_ireg_rmw_fn[priv](env->aia_ireg_rmw_fn_arg[priv],
                                    AIA_MAKE_IREG(isel, priv, virt, vgein,
                                                  riscv_cpu_mxl_bits(env)),
                                    val, new_val, wr_mask);
        }
    }

done:
    if (ret) {
        return (riscv_cpu_virt_enabled(env) && virt) ?
               RISCV_EXCP_VIRT_INSTRUCTION_FAULT : RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}

static int rmw_xtopei(CPURISCVState *env, int csrno, target_ulong *val,
                      target_ulong new_val, target_ulong wr_mask)
{
    bool virt;
    int ret = -EINVAL;
    target_ulong priv, vgein;

    /* Translate CSR number for VS-mode */
    csrno = aia_xlate_vs_csrno(env, csrno);

    /* Decode register details from CSR number */
    virt = false;
    switch (csrno) {
    case CSR_MTOPEI:
        priv = PRV_M;
        break;
    case CSR_STOPEI:
        priv = PRV_S;
        break;
    case CSR_VSTOPEI:
        priv = PRV_S;
        virt = true;
        break;
    default:
        goto done;
    };

    /* IMSIC CSRs only available when machine implements IMSIC. */
    if (!env->aia_ireg_rmw_fn[priv]) {
        goto done;
    }

    /* Find the selected guest interrupt file */
    vgein = (virt) ? get_field(env->hstatus, HSTATUS_VGEIN) : 0;

    /* Selected guest interrupt file should be valid */
    if (virt && (!vgein || env->geilen < vgein)) {
        goto done;
    }

    /* Call machine specific IMSIC register emulation for TOPEI */
    ret = env->aia_ireg_rmw_fn[priv](env->aia_ireg_rmw_fn_arg[priv],
                    AIA_MAKE_IREG(ISELECT_IMSIC_TOPEI, priv, virt, vgein,
                                  riscv_cpu_mxl_bits(env)),
                    val, new_val, wr_mask);

done:
    if (ret) {
        return (riscv_cpu_virt_enabled(env) && virt) ?
               RISCV_EXCP_VIRT_INSTRUCTION_FAULT : RISCV_EXCP_ILLEGAL_INST;
    }
    return RISCV_EXCP_NONE;
}

static RISCVException read_mtvec(CPURISCVState *env, int csrno,
                                 target_ulong *val)
{
    *val = env->mtvec;
    return RISCV_EXCP_NONE;
}

static RISCVException write_mtvec(CPURISCVState *env, int csrno,
                                  target_ulong val)
{
    /* bits [1:0] encode mode; 0 = direct, 1 = vectored, 2 >= reserved */
    if ((val & 3) < 2) {
        env->mtvec = val;
    } else {
        qemu_log_mask(LOG_UNIMP, "CSR_MTVEC: reserved mode not supported\n");
    }
    return RISCV_EXCP_NONE;
}

static RISCVException read_mcountinhibit(CPURISCVState *env, int csrno,
                                         target_ulong *val)
{
    *val = env->mcountinhibit;
    return RISCV_EXCP_NONE;
}

static RISCVException write_mcountinhibit(CPURISCVState *env, int csrno,
                                          target_ulong val)
{
    int cidx;
    PMUCTRState *counter;

    env->mcountinhibit = val;

    /* Check if any other counter is also monitoring cycles/instructions */
    for (cidx = 0; cidx < RV_MAX_MHPMCOUNTERS; cidx++) {
        if (!get_field(env->mcountinhibit, BIT(cidx))) {
            counter = &env->pmu_ctrs[cidx];
            counter->started = true;
        }
    }

    return RISCV_EXCP_NONE;
}

static RISCVException read_mcounteren(CPURISCVState *env, int csrno,
                                      target_ulong *val)
{
    *val = env->mcounteren;
    return RISCV_EXCP_NONE;
}

static RISCVException write_mcounteren(CPURISCVState *env, int csrno,
                                       target_ulong val)
{
    env->mcounteren = val;
    return RISCV_EXCP_NONE;
}

/* Machine Trap Handling */
static RISCVException read_mscratch_i128(CPURISCVState *env, int csrno,
                                         Int128 *val)
{
    *val = int128_make128(env->mscratch, env->mscratchh);
    return RISCV_EXCP_NONE;
}

static RISCVException write_mscratch_i128(CPURISCVState *env, int csrno,
                                          Int128 val)
{
    env->mscratch = int128_getlo(val);
    env->mscratchh = int128_gethi(val);
    return RISCV_EXCP_NONE;
}

static RISCVException read_mscratch(CPURISCVState *env, int csrno,
                                    target_ulong *val)
{
    *val = env->mscratch;
    return RISCV_EXCP_NONE;
}

static RISCVException write_mscratch(CPURISCVState *env, int csrno,
                                     target_ulong val)
{
    env->mscratch = val;
    return RISCV_EXCP_NONE;
}

static RISCVException read_mepc(CPURISCVState *env, int csrno,
                                     target_ulong *val)
{
    *val = env->mepc;
    return RISCV_EXCP_NONE;
}

static RISCVException write_mepc(CPURISCVState *env, int csrno,
                                     target_ulong val)
{
    env->mepc = val;
    return RISCV_EXCP_NONE;
}

static RISCVException read_mcause(CPURISCVState *env, int csrno,
                                     target_ulong *val)
{
    *val = env->mcause;
    return RISCV_EXCP_NONE;
}

static RISCVException write_mcause(CPURISCVState *env, int csrno,
                                     target_ulong val)
{
    env->mcause = val;
    return RISCV_EXCP_NONE;
}

static RISCVException read_mtval(CPURISCVState *env, int csrno,
                                 target_ulong *val)
{
    *val = env->mtval;
    return RISCV_EXCP_NONE;
}

static RISCVException write_mtval(CPURISCVState *env, int csrno,
                                  target_ulong val)
{
    env->mtval = val;
    return RISCV_EXCP_NONE;
}

/* Execution environment configuration setup */
static RISCVException read_menvcfg(CPURISCVState *env, int csrno,
                                 target_ulong *val)
{
    *val = env->menvcfg;
    return RISCV_EXCP_NONE;
}

static RISCVException write_menvcfg(CPURISCVState *env, int csrno,
                                  target_ulong val)
{
    uint64_t mask = MENVCFG_FIOM | MENVCFG_CBIE | MENVCFG_CBCFE | MENVCFG_CBZE;

    if (riscv_cpu_mxl(env) == MXL_RV64) {
        mask |= MENVCFG_PBMTE | MENVCFG_STCE;
    }
    env->menvcfg = (env->menvcfg & ~mask) | (val & mask);

    return RISCV_EXCP_NONE;
}

static RISCVException read_menvcfgh(CPURISCVState *env, int csrno,
                                 target_ulong *val)
{
    *val = env->menvcfg >> 32;
    return RISCV_EXCP_NONE;
}

static RISCVException write_menvcfgh(CPURISCVState *env, int csrno,
                                  target_ulong val)
{
    uint64_t mask = MENVCFG_PBMTE | MENVCFG_STCE;
    uint64_t valh = (uint64_t)val << 32;

    env->menvcfg = (env->menvcfg & ~mask) | (valh & mask);

    return RISCV_EXCP_NONE;
}

static RISCVException read_senvcfg(CPURISCVState *env, int csrno,
                                 target_ulong *val)
{
    *val = env->senvcfg;
    return RISCV_EXCP_NONE;
}

static RISCVException write_senvcfg(CPURISCVState *env, int csrno,
                                  target_ulong val)
{
    uint64_t mask = SENVCFG_FIOM | SENVCFG_CBIE | SENVCFG_CBCFE | SENVCFG_CBZE;

    env->senvcfg = (env->senvcfg & ~mask) | (val & mask);

    return RISCV_EXCP_NONE;
}

static RISCVException read_henvcfg(CPURISCVState *env, int csrno,
                                 target_ulong *val)
{
    *val = env->henvcfg;
    return RISCV_EXCP_NONE;
}

static RISCVException write_henvcfg(CPURISCVState *env, int csrno,
                                  target_ulong val)
{
    uint64_t mask = HENVCFG_FIOM | HENVCFG_CBIE | HENVCFG_CBCFE | HENVCFG_CBZE;

    if (riscv_cpu_mxl(env) == MXL_RV64) {
        mask |= HENVCFG_PBMTE | HENVCFG_STCE;
    }

    env->henvcfg = (env->henvcfg & ~mask) | (val & mask);

    return RISCV_EXCP_NONE;
}

static RISCVException read_henvcfgh(CPURISCVState *env, int csrno,
                                 target_ulong *val)
{
    *val = env->henvcfg >> 32;
    return RISCV_EXCP_NONE;
}

static RISCVException write_henvcfgh(CPURISCVState *env, int csrno,
                                  target_ulong val)
{
    uint64_t mask = HENVCFG_PBMTE | HENVCFG_STCE;
    uint64_t valh = (uint64_t)val << 32;

    env->henvcfg = (env->henvcfg & ~mask) | (valh & mask);

    return RISCV_EXCP_NONE;
}

static RISCVException rmw_mip64(CPURISCVState *env, int csrno,
                                uint64_t *ret_val,
                                uint64_t new_val, uint64_t wr_mask)
{
    RISCVCPU *cpu = env_archcpu(env);
    uint64_t old_mip, mask = wr_mask & delegable_ints;
    uint32_t gin;

    if (mask & MIP_SEIP) {
        env->software_seip = new_val & MIP_SEIP;
        new_val |= env->external_seip * MIP_SEIP;
    }

    if (cpu->cfg.ext_sstc && (env->priv == PRV_M) &&
        get_field(env->menvcfg, MENVCFG_STCE)) {
        /* sstc extension forbids STIP & VSTIP to be writeable in mip */
        mask = mask & ~(MIP_STIP | MIP_VSTIP);
    }

    if (mask) {
        old_mip = riscv_cpu_update_mip(cpu, mask, (new_val & mask));
    } else {
        old_mip = env->mip;
    }

    if (csrno != CSR_HVIP) {
        gin = get_field(env->hstatus, HSTATUS_VGEIN);
        old_mip |= (env->hgeip & ((target_ulong)1 << gin)) ? MIP_VSEIP : 0;
        old_mip |= env->vstime_irq ? MIP_VSTIP : 0;
    }

    if (ret_val) {
        *ret_val = old_mip;
    }

    return RISCV_EXCP_NONE;
}

static RISCVException rmw_mip(CPURISCVState *env, int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val, target_ulong wr_mask)
{
    uint64_t rval;
    RISCVException ret;

    ret = rmw_mip64(env, csrno, &rval, new_val, wr_mask);
    if (ret_val) {
        *ret_val = rval;
    }

    return ret;
}

static RISCVException rmw_miph(CPURISCVState *env, int csrno,
                               target_ulong *ret_val,
                               target_ulong new_val, target_ulong wr_mask)
{
    uint64_t rval;
    RISCVException ret;

    ret = rmw_mip64(env, csrno, &rval,
        ((uint64_t)new_val) << 32, ((uint64_t)wr_mask) << 32);
    if (ret_val) {
        *ret_val = rval >> 32;
    }

    return ret;
}

/* Supervisor Trap Setup */
static RISCVException read_sstatus_i128(CPURISCVState *env, int csrno,
                                        Int128 *val)
{
    uint64_t mask = sstatus_v1_10_mask;
    uint64_t sstatus = env->mstatus & mask;
    if (env->xl != MXL_RV32 || env->debugger) {
        mask |= SSTATUS64_UXL;
    }

    *val = int128_make128(sstatus, add_status_sd(MXL_RV128, sstatus));
    return RISCV_EXCP_NONE;
}

static RISCVException read_sstatus(CPURISCVState *env, int csrno,
                                   target_ulong *val)
{
    target_ulong mask = (sstatus_v1_10_mask);
    if (env->xl != MXL_RV32 || env->debugger) {
        mask |= SSTATUS64_UXL;
    }
    /* TODO: Use SXL not MXL. */
    *val = add_status_sd(riscv_cpu_mxl(env), env->mstatus & mask);
    return RISCV_EXCP_NONE;
}

static RISCVException write_sstatus(CPURISCVState *env, int csrno,
                                    target_ulong val)
{
    target_ulong mask = (sstatus_v1_10_mask);

    if (env->xl != MXL_RV32 || env->debugger) {
        if ((val & SSTATUS64_UXL) != 0) {
            mask |= SSTATUS64_UXL;
        }
    }
    target_ulong newval = (env->mstatus & ~mask) | (val & mask);
    return write_mstatus(env, CSR_MSTATUS, newval);
}

static RISCVException rmw_vsie64(CPURISCVState *env, int csrno,
                                 uint64_t *ret_val,
                                 uint64_t new_val, uint64_t wr_mask)
{
    RISCVException ret;
    uint64_t rval, vsbits, mask = env->hideleg & VS_MODE_INTERRUPTS;

    /* Bring VS-level bits to correct position */
    vsbits = new_val & (VS_MODE_INTERRUPTS >> 1);
    new_val &= ~(VS_MODE_INTERRUPTS >> 1);
    new_val |= vsbits << 1;
    vsbits = wr_mask & (VS_MODE_INTERRUPTS >> 1);
    wr_mask &= ~(VS_MODE_INTERRUPTS >> 1);
    wr_mask |= vsbits << 1;

    ret = rmw_mie64(env, csrno, &rval, new_val, wr_mask & mask);
    if (ret_val) {
        rval &= mask;
        vsbits = rval & VS_MODE_INTERRUPTS;
        rval &= ~VS_MODE_INTERRUPTS;
        *ret_val = rval | (vsbits >> 1);
    }

    return ret;
}

static RISCVException rmw_vsie(CPURISCVState *env, int csrno,
                               target_ulong *ret_val,
                               target_ulong new_val, target_ulong wr_mask)
{
    uint64_t rval;
    RISCVException ret;

    ret = rmw_vsie64(env, csrno, &rval, new_val, wr_mask);
    if (ret_val) {
        *ret_val = rval;
    }

    return ret;
}

static RISCVException rmw_vsieh(CPURISCVState *env, int csrno,
                                target_ulong *ret_val,
                                target_ulong new_val, target_ulong wr_mask)
{
    uint64_t rval;
    RISCVException ret;

    ret = rmw_vsie64(env, csrno, &rval,
        ((uint64_t)new_val) << 32, ((uint64_t)wr_mask) << 32);
    if (ret_val) {
        *ret_val = rval >> 32;
    }

    return ret;
}

static RISCVException rmw_sie64(CPURISCVState *env, int csrno,
                                uint64_t *ret_val,
                                uint64_t new_val, uint64_t wr_mask)
{
    RISCVException ret;
    uint64_t mask = env->mideleg & S_MODE_INTERRUPTS;

    if (riscv_cpu_virt_enabled(env)) {
        if (env->hvictl & HVICTL_VTI) {
            return RISCV_EXCP_VIRT_INSTRUCTION_FAULT;
        }
        ret = rmw_vsie64(env, CSR_VSIE, ret_val, new_val, wr_mask);
    } else {
        ret = rmw_mie64(env, csrno, ret_val, new_val, wr_mask & mask);
    }

    if (ret_val) {
        *ret_val &= mask;
    }

    return ret;
}

static RISCVException rmw_sie(CPURISCVState *env, int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val, target_ulong wr_mask)
{
    uint64_t rval;
    RISCVException ret;

    ret = rmw_sie64(env, csrno, &rval, new_val, wr_mask);
    if (ret == RISCV_EXCP_NONE && ret_val) {
        *ret_val = rval;
    }

    return ret;
}

static RISCVException rmw_sieh(CPURISCVState *env, int csrno,
                               target_ulong *ret_val,
                               target_ulong new_val, target_ulong wr_mask)
{
    uint64_t rval;
    RISCVException ret;

    ret = rmw_sie64(env, csrno, &rval,
        ((uint64_t)new_val) << 32, ((uint64_t)wr_mask) << 32);
    if (ret_val) {
        *ret_val = rval >> 32;
    }

    return ret;
}

static RISCVException read_stvec(CPURISCVState *env, int csrno,
                                 target_ulong *val)
{
    *val = env->stvec;
    return RISCV_EXCP_NONE;
}

static RISCVException write_stvec(CPURISCVState *env, int csrno,
                                  target_ulong val)
{
    /* bits [1:0] encode mode; 0 = direct, 1 = vectored, 2 >= reserved */
    if ((val & 3) < 2) {
        env->stvec = val;
    } else {
        qemu_log_mask(LOG_UNIMP, "CSR_STVEC: reserved mode not supported\n");
    }
    return RISCV_EXCP_NONE;
}

static RISCVException read_scounteren(CPURISCVState *env, int csrno,
                                      target_ulong *val)
{
    *val = env->scounteren;
    return RISCV_EXCP_NONE;
}

static RISCVException write_scounteren(CPURISCVState *env, int csrno,
                                       target_ulong val)
{
    env->scounteren = val;
    return RISCV_EXCP_NONE;
}

/* Supervisor Trap Handling */
static RISCVException read_sscratch_i128(CPURISCVState *env, int csrno,
                                         Int128 *val)
{
    *val = int128_make128(env->sscratch, env->sscratchh);
    return RISCV_EXCP_NONE;
}

static RISCVException write_sscratch_i128(CPURISCVState *env, int csrno,
                                          Int128 val)
{
    env->sscratch = int128_getlo(val);
    env->sscratchh = int128_gethi(val);
    return RISCV_EXCP_NONE;
}

static RISCVException read_sscratch(CPURISCVState *env, int csrno,
                                    target_ulong *val)
{
    *val = env->sscratch;
    return RISCV_EXCP_NONE;
}

static RISCVException write_sscratch(CPURISCVState *env, int csrno,
                                     target_ulong val)
{
    env->sscratch = val;
    return RISCV_EXCP_NONE;
}

static RISCVException read_sepc(CPURISCVState *env, int csrno,
                                target_ulong *val)
{
    *val = env->sepc;
    return RISCV_EXCP_NONE;
}

static RISCVException write_sepc(CPURISCVState *env, int csrno,
                                 target_ulong val)
{
    env->sepc = val;
    return RISCV_EXCP_NONE;
}

static RISCVException read_scause(CPURISCVState *env, int csrno,
                                  target_ulong *val)
{
    *val = env->scause;
    return RISCV_EXCP_NONE;
}

static RISCVException write_scause(CPURISCVState *env, int csrno,
                                   target_ulong val)
{
    env->scause = val;
    return RISCV_EXCP_NONE;
}

static RISCVException read_stval(CPURISCVState *env, int csrno,
                                 target_ulong *val)
{
    *val = env->stval;
    return RISCV_EXCP_NONE;
}

static RISCVException write_stval(CPURISCVState *env, int csrno,
                                  target_ulong val)
{
    env->stval = val;
    return RISCV_EXCP_NONE;
}

static RISCVException rmw_vsip64(CPURISCVState *env, int csrno,
                                 uint64_t *ret_val,
                                 uint64_t new_val, uint64_t wr_mask)
{
    RISCVException ret;
    uint64_t rval, vsbits, mask = env->hideleg & vsip_writable_mask;

    /* Bring VS-level bits to correct position */
    vsbits = new_val & (VS_MODE_INTERRUPTS >> 1);
    new_val &= ~(VS_MODE_INTERRUPTS >> 1);
    new_val |= vsbits << 1;
    vsbits = wr_mask & (VS_MODE_INTERRUPTS >> 1);
    wr_mask &= ~(VS_MODE_INTERRUPTS >> 1);
    wr_mask |= vsbits << 1;

    ret = rmw_mip64(env, csrno, &rval, new_val, wr_mask & mask);
    if (ret_val) {
        rval &= mask;
        vsbits = rval & VS_MODE_INTERRUPTS;
        rval &= ~VS_MODE_INTERRUPTS;
        *ret_val = rval | (vsbits >> 1);
    }

    return ret;
}

static RISCVException rmw_vsip(CPURISCVState *env, int csrno,
                               target_ulong *ret_val,
                               target_ulong new_val, target_ulong wr_mask)
{
    uint64_t rval;
    RISCVException ret;

    ret = rmw_vsip64(env, csrno, &rval, new_val, wr_mask);
    if (ret_val) {
        *ret_val = rval;
    }

    return ret;
}

static RISCVException rmw_vsiph(CPURISCVState *env, int csrno,
                                target_ulong *ret_val,
                                target_ulong new_val, target_ulong wr_mask)
{
    uint64_t rval;
    RISCVException ret;

    ret = rmw_vsip64(env, csrno, &rval,
        ((uint64_t)new_val) << 32, ((uint64_t)wr_mask) << 32);
    if (ret_val) {
        *ret_val = rval >> 32;
    }

    return ret;
}

static RISCVException rmw_sip64(CPURISCVState *env, int csrno,
                                uint64_t *ret_val,
                                uint64_t new_val, uint64_t wr_mask)
{
    RISCVException ret;
    uint64_t mask = env->mideleg & sip_writable_mask;

    if (riscv_cpu_virt_enabled(env)) {
        if (env->hvictl & HVICTL_VTI) {
            return RISCV_EXCP_VIRT_INSTRUCTION_FAULT;
        }
        ret = rmw_vsip64(env, CSR_VSIP, ret_val, new_val, wr_mask);
    } else {
        ret = rmw_mip64(env, csrno, ret_val, new_val, wr_mask & mask);
    }

    if (ret_val) {
        *ret_val &= env->mideleg & S_MODE_INTERRUPTS;
    }

    return ret;
}

static RISCVException rmw_sip(CPURISCVState *env, int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val, target_ulong wr_mask)
{
    uint64_t rval;
    RISCVException ret;

    ret = rmw_sip64(env, csrno, &rval, new_val, wr_mask);
    if (ret_val) {
        *ret_val = rval;
    }

    return ret;
}

static RISCVException rmw_siph(CPURISCVState *env, int csrno,
                               target_ulong *ret_val,
                               target_ulong new_val, target_ulong wr_mask)
{
    uint64_t rval;
    RISCVException ret;

    ret = rmw_sip64(env, csrno, &rval,
        ((uint64_t)new_val) << 32, ((uint64_t)wr_mask) << 32);
    if (ret_val) {
        *ret_val = rval >> 32;
    }

    return ret;
}

/* Supervisor Protection and Translation */
static RISCVException read_satp(CPURISCVState *env, int csrno,
                                target_ulong *val)
{
    if (!riscv_feature(env, RISCV_FEATURE_MMU)) {
        *val = 0;
        return RISCV_EXCP_NONE;
    }

    if (env->priv == PRV_S && get_field(env->mstatus, MSTATUS_TVM)) {
        return RISCV_EXCP_ILLEGAL_INST;
    } else {
        *val = env->satp;
    }

    return RISCV_EXCP_NONE;
}

static RISCVException write_satp(CPURISCVState *env, int csrno,
                                 target_ulong val)
{
    target_ulong vm, mask;

    if (!riscv_feature(env, RISCV_FEATURE_MMU)) {
        return RISCV_EXCP_NONE;
    }

    if (riscv_cpu_mxl(env) == MXL_RV32) {
        vm = validate_vm(env, get_field(val, SATP32_MODE));
        mask = (val ^ env->satp) & (SATP32_MODE | SATP32_ASID | SATP32_PPN);
    } else {
        vm = validate_vm(env, get_field(val, SATP64_MODE));
        mask = (val ^ env->satp) & (SATP64_MODE | SATP64_ASID | SATP64_PPN);
    }

    if (vm && mask) {
        if (env->priv == PRV_S && get_field(env->mstatus, MSTATUS_TVM)) {
            return RISCV_EXCP_ILLEGAL_INST;
        } else {
            /*
             * The ISA defines SATP.MODE=Bare as "no translation", but we still
             * pass these through QEMU's TLB emulation as it improves
             * performance.  Flushing the TLB on SATP writes with paging
             * enabled avoids leaking those invalid cached mappings.
             */
            tlb_flush(env_cpu(env));
            env->satp = val;
        }
    }
    return RISCV_EXCP_NONE;
}

static int read_vstopi(CPURISCVState *env, int csrno, target_ulong *val)
{
    int irq, ret;
    target_ulong topei;
    uint64_t vseip, vsgein;
    uint32_t iid, iprio, hviid, hviprio, gein;
    uint32_t s, scount = 0, siid[VSTOPI_NUM_SRCS], siprio[VSTOPI_NUM_SRCS];

    gein = get_field(env->hstatus, HSTATUS_VGEIN);
    hviid = get_field(env->hvictl, HVICTL_IID);
    hviprio = get_field(env->hvictl, HVICTL_IPRIO);

    if (gein) {
        vsgein = (env->hgeip & (1ULL << gein)) ? MIP_VSEIP : 0;
        vseip = env->mie & (env->mip | vsgein) & MIP_VSEIP;
        if (gein <= env->geilen && vseip) {
            siid[scount] = IRQ_S_EXT;
            siprio[scount] = IPRIO_MMAXIPRIO + 1;
            if (env->aia_ireg_rmw_fn[PRV_S]) {
                /*
                 * Call machine specific IMSIC register emulation for
                 * reading TOPEI.
                 */
                ret = env->aia_ireg_rmw_fn[PRV_S](
                        env->aia_ireg_rmw_fn_arg[PRV_S],
                        AIA_MAKE_IREG(ISELECT_IMSIC_TOPEI, PRV_S, true, gein,
                                      riscv_cpu_mxl_bits(env)),
                        &topei, 0, 0);
                if (!ret && topei) {
                    siprio[scount] = topei & IMSIC_TOPEI_IPRIO_MASK;
                }
            }
            scount++;
        }
    } else {
        if (hviid == IRQ_S_EXT && hviprio) {
            siid[scount] = IRQ_S_EXT;
            siprio[scount] = hviprio;
            scount++;
        }
    }

    if (env->hvictl & HVICTL_VTI) {
        if (hviid != IRQ_S_EXT) {
            siid[scount] = hviid;
            siprio[scount] = hviprio;
            scount++;
        }
    } else {
        irq = riscv_cpu_vsirq_pending(env);
        if (irq != IRQ_S_EXT && 0 < irq && irq <= 63) {
            siid[scount] = irq;
            siprio[scount] = env->hviprio[irq];
            scount++;
        }
    }

    iid = 0;
    iprio = UINT_MAX;
    for (s = 0; s < scount; s++) {
        if (siprio[s] < iprio) {
            iid = siid[s];
            iprio = siprio[s];
        }
    }

    if (iid) {
        if (env->hvictl & HVICTL_IPRIOM) {
            if (iprio > IPRIO_MMAXIPRIO) {
                iprio = IPRIO_MMAXIPRIO;
            }
            if (!iprio) {
                if (riscv_cpu_default_priority(iid) > IPRIO_DEFAULT_S) {
                    iprio = IPRIO_MMAXIPRIO;
                }
            }
        } else {
            iprio = 1;
        }
    } else {
        iprio = 0;
    }

    *val = (iid & TOPI_IID_MASK) << TOPI_IID_SHIFT;
    *val |= iprio;
    return RISCV_EXCP_NONE;
}

static int read_stopi(CPURISCVState *env, int csrno, target_ulong *val)
{
    int irq;
    uint8_t iprio;

    if (riscv_cpu_virt_enabled(env)) {
        return read_vstopi(env, CSR_VSTOPI, val);
    }

    irq = riscv_cpu_sirq_pending(env);
    if (irq <= 0 || irq > 63) {
        *val = 0;
    } else {
        iprio = env->siprio[irq];
        if (!iprio) {
            if (riscv_cpu_default_priority(irq) > IPRIO_DEFAULT_S) {
                iprio = IPRIO_MMAXIPRIO;
           }
        }
        *val = (irq & TOPI_IID_MASK) << TOPI_IID_SHIFT;
        *val |= iprio;
    }

    return RISCV_EXCP_NONE;
}

/* Hypervisor Extensions */
static RISCVException read_hstatus(CPURISCVState *env, int csrno,
                                   target_ulong *val)
{
    *val = env->hstatus;
    if (riscv_cpu_mxl(env) != MXL_RV32) {
        /* We only support 64-bit VSXL */
        *val = set_field(*val, HSTATUS_VSXL, 2);
    }
    /* We only support little endian */
    *val = set_field(*val, HSTATUS_VSBE, 0);
    return RISCV_EXCP_NONE;
}

static RISCVException write_hstatus(CPURISCVState *env, int csrno,
                                    target_ulong val)
{
    env->hstatus = val;
    if (riscv_cpu_mxl(env) != MXL_RV32 && get_field(val, HSTATUS_VSXL) != 2) {
        qemu_log_mask(LOG_UNIMP, "QEMU does not support mixed HSXLEN options.");
    }
    if (get_field(val, HSTATUS_VSBE) != 0) {
        qemu_log_mask(LOG_UNIMP, "QEMU does not support big endian guests.");
    }
    return RISCV_EXCP_NONE;
}

static RISCVException read_hedeleg(CPURISCVState *env, int csrno,
                                   target_ulong *val)
{
    *val = env->hedeleg;
    return RISCV_EXCP_NONE;
}

static RISCVException write_hedeleg(CPURISCVState *env, int csrno,
                                    target_ulong val)
{
    env->hedeleg = val & vs_delegable_excps;
    return RISCV_EXCP_NONE;
}

static RISCVException rmw_hideleg64(CPURISCVState *env, int csrno,
                                    uint64_t *ret_val,
                                    uint64_t new_val, uint64_t wr_mask)
{
    uint64_t mask = wr_mask & vs_delegable_ints;

    if (ret_val) {
        *ret_val = env->hideleg & vs_delegable_ints;
    }

    env->hideleg = (env->hideleg & ~mask) | (new_val & mask);
    return RISCV_EXCP_NONE;
}

static RISCVException rmw_hideleg(CPURISCVState *env, int csrno,
                                  target_ulong *ret_val,
                                  target_ulong new_val, target_ulong wr_mask)
{
    uint64_t rval;
    RISCVException ret;

    ret = rmw_hideleg64(env, csrno, &rval, new_val, wr_mask);
    if (ret_val) {
        *ret_val = rval;
    }

    return ret;
}

static RISCVException rmw_hidelegh(CPURISCVState *env, int csrno,
                                   target_ulong *ret_val,
                                   target_ulong new_val, target_ulong wr_mask)
{
    uint64_t rval;
    RISCVException ret;

    ret = rmw_hideleg64(env, csrno, &rval,
        ((uint64_t)new_val) << 32, ((uint64_t)wr_mask) << 32);
    if (ret_val) {
        *ret_val = rval >> 32;
    }

    return ret;
}

static RISCVException rmw_hvip64(CPURISCVState *env, int csrno,
                                 uint64_t *ret_val,
                                 uint64_t new_val, uint64_t wr_mask)
{
    RISCVException ret;

    ret = rmw_mip64(env, csrno, ret_val, new_val,
                    wr_mask & hvip_writable_mask);
    if (ret_val) {
        *ret_val &= VS_MODE_INTERRUPTS;
    }

    return ret;
}

static RISCVException rmw_hvip(CPURISCVState *env, int csrno,
                               target_ulong *ret_val,
                               target_ulong new_val, target_ulong wr_mask)
{
    uint64_t rval;
    RISCVException ret;

    ret = rmw_hvip64(env, csrno, &rval, new_val, wr_mask);
    if (ret_val) {
        *ret_val = rval;
    }

    return ret;
}

static RISCVException rmw_hviph(CPURISCVState *env, int csrno,
                                target_ulong *ret_val,
                                target_ulong new_val, target_ulong wr_mask)
{
    uint64_t rval;
    RISCVException ret;

    ret = rmw_hvip64(env, csrno, &rval,
        ((uint64_t)new_val) << 32, ((uint64_t)wr_mask) << 32);
    if (ret_val) {
        *ret_val = rval >> 32;
    }

    return ret;
}

static RISCVException rmw_hip(CPURISCVState *env, int csrno,
                              target_ulong *ret_value,
                              target_ulong new_value, target_ulong write_mask)
{
    int ret = rmw_mip(env, csrno, ret_value, new_value,
                      write_mask & hip_writable_mask);

    if (ret_value) {
        *ret_value &= HS_MODE_INTERRUPTS;
    }
    return ret;
}

static RISCVException rmw_hie(CPURISCVState *env, int csrno,
                              target_ulong *ret_val,
                              target_ulong new_val, target_ulong wr_mask)
{
    uint64_t rval;
    RISCVException ret;

    ret = rmw_mie64(env, csrno, &rval, new_val, wr_mask & HS_MODE_INTERRUPTS);
    if (ret_val) {
        *ret_val = rval & HS_MODE_INTERRUPTS;
    }

    return ret;
}

static RISCVException read_hcounteren(CPURISCVState *env, int csrno,
                                      target_ulong *val)
{
    *val = env->hcounteren;
    return RISCV_EXCP_NONE;
}

static RISCVException write_hcounteren(CPURISCVState *env, int csrno,
                                       target_ulong val)
{
    env->hcounteren = val;
    return RISCV_EXCP_NONE;
}

static RISCVException read_hgeie(CPURISCVState *env, int csrno,
                                 target_ulong *val)
{
    if (val) {
        *val = env->hgeie;
    }
    return RISCV_EXCP_NONE;
}

static RISCVException write_hgeie(CPURISCVState *env, int csrno,
                                  target_ulong val)
{
    /* Only GEILEN:1 bits implemented and BIT0 is never implemented */
    val &= ((((target_ulong)1) << env->geilen) - 1) << 1;
    env->hgeie = val;
    /* Update mip.SGEIP bit */
    riscv_cpu_update_mip(env_archcpu(env), MIP_SGEIP,
                         BOOL_TO_MASK(!!(env->hgeie & env->hgeip)));
    return RISCV_EXCP_NONE;
}

static RISCVException read_htval(CPURISCVState *env, int csrno,
                                 target_ulong *val)
{
    *val = env->htval;
    return RISCV_EXCP_NONE;
}

static RISCVException write_htval(CPURISCVState *env, int csrno,
                                  target_ulong val)
{
    env->htval = val;
    return RISCV_EXCP_NONE;
}

static RISCVException read_htinst(CPURISCVState *env, int csrno,
                                  target_ulong *val)
{
    *val = env->htinst;
    return RISCV_EXCP_NONE;
}

static RISCVException write_htinst(CPURISCVState *env, int csrno,
                                   target_ulong val)
{
    return RISCV_EXCP_NONE;
}

static RISCVException read_hgeip(CPURISCVState *env, int csrno,
                                 target_ulong *val)
{
    if (val) {
        *val = env->hgeip;
    }
    return RISCV_EXCP_NONE;
}

static RISCVException read_hgatp(CPURISCVState *env, int csrno,
                                 target_ulong *val)
{
    *val = env->hgatp;
    return RISCV_EXCP_NONE;
}

static RISCVException write_hgatp(CPURISCVState *env, int csrno,
                                  target_ulong val)
{
    env->hgatp = val;
    return RISCV_EXCP_NONE;
}

static RISCVException read_htimedelta(CPURISCVState *env, int csrno,
                                      target_ulong *val)
{
    if (!env->rdtime_fn) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    *val = env->htimedelta;
    return RISCV_EXCP_NONE;
}

static RISCVException write_htimedelta(CPURISCVState *env, int csrno,
                                       target_ulong val)
{
    if (!env->rdtime_fn) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    if (riscv_cpu_mxl(env) == MXL_RV32) {
        env->htimedelta = deposit64(env->htimedelta, 0, 32, (uint64_t)val);
    } else {
        env->htimedelta = val;
    }
    return RISCV_EXCP_NONE;
}

static RISCVException read_htimedeltah(CPURISCVState *env, int csrno,
                                       target_ulong *val)
{
    if (!env->rdtime_fn) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    *val = env->htimedelta >> 32;
    return RISCV_EXCP_NONE;
}

static RISCVException write_htimedeltah(CPURISCVState *env, int csrno,
                                        target_ulong val)
{
    if (!env->rdtime_fn) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    env->htimedelta = deposit64(env->htimedelta, 32, 32, (uint64_t)val);
    return RISCV_EXCP_NONE;
}

static int read_hvictl(CPURISCVState *env, int csrno, target_ulong *val)
{
    *val = env->hvictl;
    return RISCV_EXCP_NONE;
}

static int write_hvictl(CPURISCVState *env, int csrno, target_ulong val)
{
    env->hvictl = val & HVICTL_VALID_MASK;
    return RISCV_EXCP_NONE;
}

static int read_hvipriox(CPURISCVState *env, int first_index,
                         uint8_t *iprio, target_ulong *val)
{
    int i, irq, rdzero, num_irqs = 4 * (riscv_cpu_mxl_bits(env) / 32);

    /* First index has to be a multiple of number of irqs per register */
    if (first_index % num_irqs) {
        return (riscv_cpu_virt_enabled(env)) ?
               RISCV_EXCP_VIRT_INSTRUCTION_FAULT : RISCV_EXCP_ILLEGAL_INST;
    }

    /* Fill-up return value */
    *val = 0;
    for (i = 0; i < num_irqs; i++) {
        if (riscv_cpu_hviprio_index2irq(first_index + i, &irq, &rdzero)) {
            continue;
        }
        if (rdzero) {
            continue;
        }
        *val |= ((target_ulong)iprio[irq]) << (i * 8);
    }

    return RISCV_EXCP_NONE;
}

static int write_hvipriox(CPURISCVState *env, int first_index,
                          uint8_t *iprio, target_ulong val)
{
    int i, irq, rdzero, num_irqs = 4 * (riscv_cpu_mxl_bits(env) / 32);

    /* First index has to be a multiple of number of irqs per register */
    if (first_index % num_irqs) {
        return (riscv_cpu_virt_enabled(env)) ?
               RISCV_EXCP_VIRT_INSTRUCTION_FAULT : RISCV_EXCP_ILLEGAL_INST;
    }

    /* Fill-up priority arrary */
    for (i = 0; i < num_irqs; i++) {
        if (riscv_cpu_hviprio_index2irq(first_index + i, &irq, &rdzero)) {
            continue;
        }
        if (rdzero) {
            iprio[irq] = 0;
        } else {
            iprio[irq] = (val >> (i * 8)) & 0xff;
        }
    }

    return RISCV_EXCP_NONE;
}

static int read_hviprio1(CPURISCVState *env, int csrno, target_ulong *val)
{
    return read_hvipriox(env, 0, env->hviprio, val);
}

static int write_hviprio1(CPURISCVState *env, int csrno, target_ulong val)
{
    return write_hvipriox(env, 0, env->hviprio, val);
}

static int read_hviprio1h(CPURISCVState *env, int csrno, target_ulong *val)
{
    return read_hvipriox(env, 4, env->hviprio, val);
}

static int write_hviprio1h(CPURISCVState *env, int csrno, target_ulong val)
{
    return write_hvipriox(env, 4, env->hviprio, val);
}

static int read_hviprio2(CPURISCVState *env, int csrno, target_ulong *val)
{
    return read_hvipriox(env, 8, env->hviprio, val);
}

static int write_hviprio2(CPURISCVState *env, int csrno, target_ulong val)
{
    return write_hvipriox(env, 8, env->hviprio, val);
}

static int read_hviprio2h(CPURISCVState *env, int csrno, target_ulong *val)
{
    return read_hvipriox(env, 12, env->hviprio, val);
}

static int write_hviprio2h(CPURISCVState *env, int csrno, target_ulong val)
{
    return write_hvipriox(env, 12, env->hviprio, val);
}

/* Virtual CSR Registers */
static RISCVException read_vsstatus(CPURISCVState *env, int csrno,
                                    target_ulong *val)
{
    *val = env->vsstatus;
    return RISCV_EXCP_NONE;
}

static RISCVException write_vsstatus(CPURISCVState *env, int csrno,
                                     target_ulong val)
{
    uint64_t mask = (target_ulong)-1;
    if ((val & VSSTATUS64_UXL) == 0) {
        mask &= ~VSSTATUS64_UXL;
    }
    env->vsstatus = (env->vsstatus & ~mask) | (uint64_t)val;
    return RISCV_EXCP_NONE;
}

static int read_vstvec(CPURISCVState *env, int csrno, target_ulong *val)
{
    *val = env->vstvec;
    return RISCV_EXCP_NONE;
}

static RISCVException write_vstvec(CPURISCVState *env, int csrno,
                                   target_ulong val)
{
    env->vstvec = val;
    return RISCV_EXCP_NONE;
}

static RISCVException read_vsscratch(CPURISCVState *env, int csrno,
                                     target_ulong *val)
{
    *val = env->vsscratch;
    return RISCV_EXCP_NONE;
}

static RISCVException write_vsscratch(CPURISCVState *env, int csrno,
                                      target_ulong val)
{
    env->vsscratch = val;
    return RISCV_EXCP_NONE;
}

static RISCVException read_vsepc(CPURISCVState *env, int csrno,
                                 target_ulong *val)
{
    *val = env->vsepc;
    return RISCV_EXCP_NONE;
}

static RISCVException write_vsepc(CPURISCVState *env, int csrno,
                                  target_ulong val)
{
    env->vsepc = val;
    return RISCV_EXCP_NONE;
}

static RISCVException read_vscause(CPURISCVState *env, int csrno,
                                   target_ulong *val)
{
    *val = env->vscause;
    return RISCV_EXCP_NONE;
}

static RISCVException write_vscause(CPURISCVState *env, int csrno,
                                    target_ulong val)
{
    env->vscause = val;
    return RISCV_EXCP_NONE;
}

static RISCVException read_vstval(CPURISCVState *env, int csrno,
                                  target_ulong *val)
{
    *val = env->vstval;
    return RISCV_EXCP_NONE;
}

static RISCVException write_vstval(CPURISCVState *env, int csrno,
                                   target_ulong val)
{
    env->vstval = val;
    return RISCV_EXCP_NONE;
}

static RISCVException read_vsatp(CPURISCVState *env, int csrno,
                                 target_ulong *val)
{
    *val = env->vsatp;
    return RISCV_EXCP_NONE;
}

static RISCVException write_vsatp(CPURISCVState *env, int csrno,
                                  target_ulong val)
{
    env->vsatp = val;
    return RISCV_EXCP_NONE;
}

static RISCVException read_mtval2(CPURISCVState *env, int csrno,
                                  target_ulong *val)
{
    *val = env->mtval2;
    return RISCV_EXCP_NONE;
}

static RISCVException write_mtval2(CPURISCVState *env, int csrno,
                                   target_ulong val)
{
    env->mtval2 = val;
    return RISCV_EXCP_NONE;
}

static RISCVException read_mtinst(CPURISCVState *env, int csrno,
                                  target_ulong *val)
{
    *val = env->mtinst;
    return RISCV_EXCP_NONE;
}

static RISCVException write_mtinst(CPURISCVState *env, int csrno,
                                   target_ulong val)
{
    env->mtinst = val;
    return RISCV_EXCP_NONE;
}

/* Physical Memory Protection */
static RISCVException read_mseccfg(CPURISCVState *env, int csrno,
                                   target_ulong *val)
{
    *val = mseccfg_csr_read(env);
    return RISCV_EXCP_NONE;
}

static RISCVException write_mseccfg(CPURISCVState *env, int csrno,
                         target_ulong val)
{
    mseccfg_csr_write(env, val);
    return RISCV_EXCP_NONE;
}

static bool check_pmp_reg_index(CPURISCVState *env, uint32_t reg_index)
{
    /* TODO: RV128 restriction check */
    if ((reg_index & 1) && (riscv_cpu_mxl(env) == MXL_RV64)) {
        return false;
    }
    return true;
}

static RISCVException read_pmpcfg(CPURISCVState *env, int csrno,
                                  target_ulong *val)
{
    uint32_t reg_index = csrno - CSR_PMPCFG0;

    if (!check_pmp_reg_index(env, reg_index)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    *val = pmpcfg_csr_read(env, csrno - CSR_PMPCFG0);
    return RISCV_EXCP_NONE;
}

static RISCVException write_pmpcfg(CPURISCVState *env, int csrno,
                                   target_ulong val)
{
    uint32_t reg_index = csrno - CSR_PMPCFG0;

    if (!check_pmp_reg_index(env, reg_index)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    pmpcfg_csr_write(env, csrno - CSR_PMPCFG0, val);
    return RISCV_EXCP_NONE;
}

static RISCVException read_pmpaddr(CPURISCVState *env, int csrno,
                                   target_ulong *val)
{
    *val = pmpaddr_csr_read(env, csrno - CSR_PMPADDR0);
    return RISCV_EXCP_NONE;
}

static RISCVException write_pmpaddr(CPURISCVState *env, int csrno,
                                    target_ulong val)
{
    pmpaddr_csr_write(env, csrno - CSR_PMPADDR0, val);
    return RISCV_EXCP_NONE;
}

static RISCVException read_tselect(CPURISCVState *env, int csrno,
                                   target_ulong *val)
{
    *val = tselect_csr_read(env);
    return RISCV_EXCP_NONE;
}

static RISCVException write_tselect(CPURISCVState *env, int csrno,
                                    target_ulong val)
{
    tselect_csr_write(env, val);
    return RISCV_EXCP_NONE;
}

static RISCVException read_tdata(CPURISCVState *env, int csrno,
                                 target_ulong *val)
{
    /* return 0 in tdata1 to end the trigger enumeration */
    if (env->trigger_cur >= RV_MAX_TRIGGERS && csrno == CSR_TDATA1) {
        *val = 0;
        return RISCV_EXCP_NONE;
    }

    if (!tdata_available(env, csrno - CSR_TDATA1)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    *val = tdata_csr_read(env, csrno - CSR_TDATA1);
    return RISCV_EXCP_NONE;
}

static RISCVException write_tdata(CPURISCVState *env, int csrno,
                                  target_ulong val)
{
    if (!tdata_available(env, csrno - CSR_TDATA1)) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    tdata_csr_write(env, csrno - CSR_TDATA1, val);
    return RISCV_EXCP_NONE;
}

static RISCVException read_tinfo(CPURISCVState *env, int csrno,
                                 target_ulong *val)
{
    *val = tinfo_csr_read(env);
    return RISCV_EXCP_NONE;
}

/*
 * Functions to access Pointer Masking feature registers
 * We have to check if current priv lvl could modify
 * csr in given mode
 */
static bool check_pm_current_disabled(CPURISCVState *env, int csrno)
{
    int csr_priv = get_field(csrno, 0x300);
    int pm_current;

    if (env->debugger) {
        return false;
    }
    /*
     * If priv lvls differ that means we're accessing csr from higher priv lvl,
     * so allow the access
     */
    if (env->priv != csr_priv) {
        return false;
    }
    switch (env->priv) {
    case PRV_M:
        pm_current = get_field(env->mmte, M_PM_CURRENT);
        break;
    case PRV_S:
        pm_current = get_field(env->mmte, S_PM_CURRENT);
        break;
    case PRV_U:
        pm_current = get_field(env->mmte, U_PM_CURRENT);
        break;
    default:
        g_assert_not_reached();
    }
    /* It's same priv lvl, so we allow to modify csr only if pm.current==1 */
    return !pm_current;
}

static RISCVException read_mmte(CPURISCVState *env, int csrno,
                                target_ulong *val)
{
    *val = env->mmte & MMTE_MASK;
    return RISCV_EXCP_NONE;
}

static RISCVException write_mmte(CPURISCVState *env, int csrno,
                                 target_ulong val)
{
    uint64_t mstatus;
    target_ulong wpri_val = val & MMTE_MASK;

    if (val != wpri_val) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s" TARGET_FMT_lx " %s" TARGET_FMT_lx "\n",
                      "MMTE: WPRI violation written 0x", val,
                      "vs expected 0x", wpri_val);
    }
    /* for machine mode pm.current is hardwired to 1 */
    wpri_val |= MMTE_M_PM_CURRENT;

    /* hardwiring pm.instruction bit to 0, since it's not supported yet */
    wpri_val &= ~(MMTE_M_PM_INSN | MMTE_S_PM_INSN | MMTE_U_PM_INSN);
    env->mmte = wpri_val | PM_EXT_DIRTY;
    riscv_cpu_update_mask(env);

    /* Set XS and SD bits, since PM CSRs are dirty */
    mstatus = env->mstatus | MSTATUS_XS;
    write_mstatus(env, csrno, mstatus);
    return RISCV_EXCP_NONE;
}

static RISCVException read_smte(CPURISCVState *env, int csrno,
                                target_ulong *val)
{
    *val = env->mmte & SMTE_MASK;
    return RISCV_EXCP_NONE;
}

static RISCVException write_smte(CPURISCVState *env, int csrno,
                                 target_ulong val)
{
    target_ulong wpri_val = val & SMTE_MASK;

    if (val != wpri_val) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s" TARGET_FMT_lx " %s" TARGET_FMT_lx "\n",
                      "SMTE: WPRI violation written 0x", val,
                      "vs expected 0x", wpri_val);
    }

    /* if pm.current==0 we can't modify current PM CSRs */
    if (check_pm_current_disabled(env, csrno)) {
        return RISCV_EXCP_NONE;
    }

    wpri_val |= (env->mmte & ~SMTE_MASK);
    write_mmte(env, csrno, wpri_val);
    return RISCV_EXCP_NONE;
}

static RISCVException read_umte(CPURISCVState *env, int csrno,
                                target_ulong *val)
{
    *val = env->mmte & UMTE_MASK;
    return RISCV_EXCP_NONE;
}

static RISCVException write_umte(CPURISCVState *env, int csrno,
                                 target_ulong val)
{
    target_ulong wpri_val = val & UMTE_MASK;

    if (val != wpri_val) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s" TARGET_FMT_lx " %s" TARGET_FMT_lx "\n",
                      "UMTE: WPRI violation written 0x", val,
                      "vs expected 0x", wpri_val);
    }

    if (check_pm_current_disabled(env, csrno)) {
        return RISCV_EXCP_NONE;
    }

    wpri_val |= (env->mmte & ~UMTE_MASK);
    write_mmte(env, csrno, wpri_val);
    return RISCV_EXCP_NONE;
}

static RISCVException read_mpmmask(CPURISCVState *env, int csrno,
                                   target_ulong *val)
{
    *val = env->mpmmask;
    return RISCV_EXCP_NONE;
}

static RISCVException write_mpmmask(CPURISCVState *env, int csrno,
                                    target_ulong val)
{
    uint64_t mstatus;

    env->mpmmask = val;
    if ((env->priv == PRV_M) && (env->mmte & M_PM_ENABLE)) {
        env->cur_pmmask = val;
    }
    env->mmte |= PM_EXT_DIRTY;

    /* Set XS and SD bits, since PM CSRs are dirty */
    mstatus = env->mstatus | MSTATUS_XS;
    write_mstatus(env, csrno, mstatus);
    return RISCV_EXCP_NONE;
}

static RISCVException read_spmmask(CPURISCVState *env, int csrno,
                                   target_ulong *val)
{
    *val = env->spmmask;
    return RISCV_EXCP_NONE;
}

static RISCVException write_spmmask(CPURISCVState *env, int csrno,
                                    target_ulong val)
{
    uint64_t mstatus;

    /* if pm.current==0 we can't modify current PM CSRs */
    if (check_pm_current_disabled(env, csrno)) {
        return RISCV_EXCP_NONE;
    }
    env->spmmask = val;
    if ((env->priv == PRV_S) && (env->mmte & S_PM_ENABLE)) {
        env->cur_pmmask = val;
    }
    env->mmte |= PM_EXT_DIRTY;

    /* Set XS and SD bits, since PM CSRs are dirty */
    mstatus = env->mstatus | MSTATUS_XS;
    write_mstatus(env, csrno, mstatus);
    return RISCV_EXCP_NONE;
}

static RISCVException read_upmmask(CPURISCVState *env, int csrno,
                                   target_ulong *val)
{
    *val = env->upmmask;
    return RISCV_EXCP_NONE;
}

static RISCVException write_upmmask(CPURISCVState *env, int csrno,
                                    target_ulong val)
{
    uint64_t mstatus;

    /* if pm.current==0 we can't modify current PM CSRs */
    if (check_pm_current_disabled(env, csrno)) {
        return RISCV_EXCP_NONE;
    }
    env->upmmask = val;
    if ((env->priv == PRV_U) && (env->mmte & U_PM_ENABLE)) {
        env->cur_pmmask = val;
    }
    env->mmte |= PM_EXT_DIRTY;

    /* Set XS and SD bits, since PM CSRs are dirty */
    mstatus = env->mstatus | MSTATUS_XS;
    write_mstatus(env, csrno, mstatus);
    return RISCV_EXCP_NONE;
}

static RISCVException read_mpmbase(CPURISCVState *env, int csrno,
                                   target_ulong *val)
{
    *val = env->mpmbase;
    return RISCV_EXCP_NONE;
}

static RISCVException write_mpmbase(CPURISCVState *env, int csrno,
                                    target_ulong val)
{
    uint64_t mstatus;

    env->mpmbase = val;
    if ((env->priv == PRV_M) && (env->mmte & M_PM_ENABLE)) {
        env->cur_pmbase = val;
    }
    env->mmte |= PM_EXT_DIRTY;

    /* Set XS and SD bits, since PM CSRs are dirty */
    mstatus = env->mstatus | MSTATUS_XS;
    write_mstatus(env, csrno, mstatus);
    return RISCV_EXCP_NONE;
}

static RISCVException read_spmbase(CPURISCVState *env, int csrno,
                                   target_ulong *val)
{
    *val = env->spmbase;
    return RISCV_EXCP_NONE;
}

static RISCVException write_spmbase(CPURISCVState *env, int csrno,
                                    target_ulong val)
{
    uint64_t mstatus;

    /* if pm.current==0 we can't modify current PM CSRs */
    if (check_pm_current_disabled(env, csrno)) {
        return RISCV_EXCP_NONE;
    }
    env->spmbase = val;
    if ((env->priv == PRV_S) && (env->mmte & S_PM_ENABLE)) {
        env->cur_pmbase = val;
    }
    env->mmte |= PM_EXT_DIRTY;

    /* Set XS and SD bits, since PM CSRs are dirty */
    mstatus = env->mstatus | MSTATUS_XS;
    write_mstatus(env, csrno, mstatus);
    return RISCV_EXCP_NONE;
}

static RISCVException read_upmbase(CPURISCVState *env, int csrno,
                                   target_ulong *val)
{
    *val = env->upmbase;
    return RISCV_EXCP_NONE;
}

static RISCVException write_upmbase(CPURISCVState *env, int csrno,
                                    target_ulong val)
{
    uint64_t mstatus;

    /* if pm.current==0 we can't modify current PM CSRs */
    if (check_pm_current_disabled(env, csrno)) {
        return RISCV_EXCP_NONE;
    }
    env->upmbase = val;
    if ((env->priv == PRV_U) && (env->mmte & U_PM_ENABLE)) {
        env->cur_pmbase = val;
    }
    env->mmte |= PM_EXT_DIRTY;

    /* Set XS and SD bits, since PM CSRs are dirty */
    mstatus = env->mstatus | MSTATUS_XS;
    write_mstatus(env, csrno, mstatus);
    return RISCV_EXCP_NONE;
}

#endif

/* Crypto Extension */
static RISCVException rmw_seed(CPURISCVState *env, int csrno,
                               target_ulong *ret_value,
                               target_ulong new_value,
                               target_ulong write_mask)
{
    uint16_t random_v;
    Error *random_e = NULL;
    int random_r;
    target_ulong rval;

    random_r = qemu_guest_getrandom(&random_v, 2, &random_e);
    if (unlikely(random_r < 0)) {
        /*
         * Failed, for unknown reasons in the crypto subsystem.
         * The best we can do is log the reason and return a
         * failure indication to the guest.  There is no reason
         * we know to expect the failure to be transitory, so
         * indicate DEAD to avoid having the guest spin on WAIT.
         */
        qemu_log_mask(LOG_UNIMP, "%s: Crypto failure: %s",
                      __func__, error_get_pretty(random_e));
        error_free(random_e);
        rval = SEED_OPST_DEAD;
    } else {
        rval = random_v | SEED_OPST_ES16;
    }

    if (ret_value) {
        *ret_value = rval;
    }

    return RISCV_EXCP_NONE;
}

/*
 * riscv_csrrw - read and/or update control and status register
 *
 * csrr   <->  riscv_csrrw(env, csrno, ret_value, 0, 0);
 * csrrw  <->  riscv_csrrw(env, csrno, ret_value, value, -1);
 * csrrs  <->  riscv_csrrw(env, csrno, ret_value, -1, value);
 * csrrc  <->  riscv_csrrw(env, csrno, ret_value, 0, value);
 */

static inline RISCVException riscv_csrrw_check(CPURISCVState *env,
                                               int csrno,
                                               bool write_mask,
                                               RISCVCPU *cpu)
{
    /* check privileges and return RISCV_EXCP_ILLEGAL_INST if check fails */
    int read_only = get_field(csrno, 0xC00) == 3;
    int csr_min_priv = csr_ops[csrno].min_priv_ver;

    /* ensure the CSR extension is enabled. */
    if (!cpu->cfg.ext_icsr) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    if (env->priv_ver < csr_min_priv) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    /* check predicate */
    if (!csr_ops[csrno].predicate) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    if (write_mask && read_only) {
        return RISCV_EXCP_ILLEGAL_INST;
    }

    RISCVException ret = csr_ops[csrno].predicate(env, csrno);
    if (ret != RISCV_EXCP_NONE) {
        return ret;
    }

#if !defined(CONFIG_USER_ONLY)
    int csr_priv, effective_priv = env->priv;

    if (riscv_has_ext(env, RVH) && env->priv == PRV_S &&
        !riscv_cpu_virt_enabled(env)) {
        /*
         * We are in HS mode. Add 1 to the effective privledge level to
         * allow us to access the Hypervisor CSRs.
         */
        effective_priv++;
    }

    csr_priv = get_field(csrno, 0x300);
    if (!env->debugger && (effective_priv < csr_priv)) {
        if (csr_priv == (PRV_S + 1) && riscv_cpu_virt_enabled(env)) {
            return RISCV_EXCP_VIRT_INSTRUCTION_FAULT;
        }
        return RISCV_EXCP_ILLEGAL_INST;
    }
#endif
    return RISCV_EXCP_NONE;
}

static RISCVException riscv_csrrw_do64(CPURISCVState *env, int csrno,
                                       target_ulong *ret_value,
                                       target_ulong new_value,
                                       target_ulong write_mask)
{
    RISCVException ret;
    target_ulong old_value;

    /* execute combined read/write operation if it exists */
    if (csr_ops[csrno].op) {
        return csr_ops[csrno].op(env, csrno, ret_value, new_value, write_mask);
    }

    /* if no accessor exists then return failure */
    if (!csr_ops[csrno].read) {
        return RISCV_EXCP_ILLEGAL_INST;
    }
    /* read old value */
    ret = csr_ops[csrno].read(env, csrno, &old_value);
    if (ret != RISCV_EXCP_NONE) {
        return ret;
    }

    /* write value if writable and write mask set, otherwise drop writes */
    if (write_mask) {
        new_value = (old_value & ~write_mask) | (new_value & write_mask);
        if (csr_ops[csrno].write) {
            ret = csr_ops[csrno].write(env, csrno, new_value);
            if (ret != RISCV_EXCP_NONE) {
                return ret;
            }
        }
    }

    /* return old value */
    if (ret_value) {
        *ret_value = old_value;
    }

    return RISCV_EXCP_NONE;
}

RISCVException riscv_csrrw(CPURISCVState *env, int csrno,
                           target_ulong *ret_value,
                           target_ulong new_value, target_ulong write_mask)
{
    RISCVCPU *cpu = env_archcpu(env);

    RISCVException ret = riscv_csrrw_check(env, csrno, write_mask, cpu);
    if (ret != RISCV_EXCP_NONE) {
        return ret;
    }

    return riscv_csrrw_do64(env, csrno, ret_value, new_value, write_mask);
}

static RISCVException riscv_csrrw_do128(CPURISCVState *env, int csrno,
                                        Int128 *ret_value,
                                        Int128 new_value,
                                        Int128 write_mask)
{
    RISCVException ret;
    Int128 old_value;

    /* read old value */
    ret = csr_ops[csrno].read128(env, csrno, &old_value);
    if (ret != RISCV_EXCP_NONE) {
        return ret;
    }

    /* write value if writable and write mask set, otherwise drop writes */
    if (int128_nz(write_mask)) {
        new_value = int128_or(int128_and(old_value, int128_not(write_mask)),
                              int128_and(new_value, write_mask));
        if (csr_ops[csrno].write128) {
            ret = csr_ops[csrno].write128(env, csrno, new_value);
            if (ret != RISCV_EXCP_NONE) {
                return ret;
            }
        } else if (csr_ops[csrno].write) {
            /* avoids having to write wrappers for all registers */
            ret = csr_ops[csrno].write(env, csrno, int128_getlo(new_value));
            if (ret != RISCV_EXCP_NONE) {
                return ret;
            }
        }
    }

    /* return old value */
    if (ret_value) {
        *ret_value = old_value;
    }

    return RISCV_EXCP_NONE;
}

RISCVException riscv_csrrw_i128(CPURISCVState *env, int csrno,
                                Int128 *ret_value,
                                Int128 new_value, Int128 write_mask)
{
    RISCVException ret;
    RISCVCPU *cpu = env_archcpu(env);

    ret = riscv_csrrw_check(env, csrno, int128_nz(write_mask), cpu);
    if (ret != RISCV_EXCP_NONE) {
        return ret;
    }

    if (csr_ops[csrno].read128) {
        return riscv_csrrw_do128(env, csrno, ret_value, new_value, write_mask);
    }

    /*
     * Fall back to 64-bit version for now, if the 128-bit alternative isn't
     * at all defined.
     * Note, some CSRs don't need to extend to MXLEN (64 upper bits non
     * significant), for those, this fallback is correctly handling the accesses
     */
    target_ulong old_value;
    ret = riscv_csrrw_do64(env, csrno, &old_value,
                           int128_getlo(new_value),
                           int128_getlo(write_mask));
    if (ret == RISCV_EXCP_NONE && ret_value) {
        *ret_value = int128_make64(old_value);
    }
    return ret;
}

/*
 * Debugger support.  If not in user mode, set env->debugger before the
 * riscv_csrrw call and clear it after the call.
 */
RISCVException riscv_csrrw_debug(CPURISCVState *env, int csrno,
                                 target_ulong *ret_value,
                                 target_ulong new_value,
                                 target_ulong write_mask)
{
    RISCVException ret;
#if !defined(CONFIG_USER_ONLY)
    env->debugger = true;
#endif
    ret = riscv_csrrw(env, csrno, ret_value, new_value, write_mask);
#if !defined(CONFIG_USER_ONLY)
    env->debugger = false;
#endif
    return ret;
}

/* Control and Status Register function table */
riscv_csr_operations csr_ops[CSR_TABLE_SIZE] = {
    /* User Floating-Point CSRs */
    [CSR_FFLAGS]   = { "fflags",   fs,     read_fflags,  write_fflags },
    [CSR_FRM]      = { "frm",      fs,     read_frm,     write_frm    },
    [CSR_FCSR]     = { "fcsr",     fs,     read_fcsr,    write_fcsr   },
    /* Vector CSRs */
    [CSR_VSTART]   = { "vstart",   vs,     read_vstart,  write_vstart,
                       .min_priv_ver = PRIV_VERSION_1_12_0            },
    [CSR_VXSAT]    = { "vxsat",    vs,     read_vxsat,   write_vxsat,
                       .min_priv_ver = PRIV_VERSION_1_12_0            },
    [CSR_VXRM]     = { "vxrm",     vs,     read_vxrm,    write_vxrm,
                       .min_priv_ver = PRIV_VERSION_1_12_0            },
    [CSR_VCSR]     = { "vcsr",     vs,     read_vcsr,    write_vcsr,
                       .min_priv_ver = PRIV_VERSION_1_12_0            },
    [CSR_VL]       = { "vl",       vs,     read_vl,
                       .min_priv_ver = PRIV_VERSION_1_12_0            },
    [CSR_VTYPE]    = { "vtype",    vs,     read_vtype,
                       .min_priv_ver = PRIV_VERSION_1_12_0            },
    [CSR_VLENB]    = { "vlenb",    vs,     read_vlenb,
                       .min_priv_ver = PRIV_VERSION_1_12_0            },
    /* User Timers and Counters */
    [CSR_CYCLE]    = { "cycle",    ctr,    read_hpmcounter  },
    [CSR_INSTRET]  = { "instret",  ctr,    read_hpmcounter  },
    [CSR_CYCLEH]   = { "cycleh",   ctr32,  read_hpmcounterh },
    [CSR_INSTRETH] = { "instreth", ctr32,  read_hpmcounterh },

    /*
     * In privileged mode, the monitor will have to emulate TIME CSRs only if
     * rdtime callback is not provided by machine/platform emulation.
     */
    [CSR_TIME]  = { "time",  ctr,   read_time  },
    [CSR_TIMEH] = { "timeh", ctr32, read_timeh },

    /* Crypto Extension */
    [CSR_SEED] = { "seed", seed, NULL, NULL, rmw_seed },

#if !defined(CONFIG_USER_ONLY)
    /* Machine Timers and Counters */
    [CSR_MCYCLE]    = { "mcycle",    any,   read_hpmcounter,
                        write_mhpmcounter                    },
    [CSR_MINSTRET]  = { "minstret",  any,   read_hpmcounter,
                        write_mhpmcounter                    },
    [CSR_MCYCLEH]   = { "mcycleh",   any32, read_hpmcounterh,
                        write_mhpmcounterh                   },
    [CSR_MINSTRETH] = { "minstreth", any32, read_hpmcounterh,
                        write_mhpmcounterh                   },

    /* Machine Information Registers */
    [CSR_MVENDORID] = { "mvendorid", any,   read_mvendorid },
    [CSR_MARCHID]   = { "marchid",   any,   read_marchid   },
    [CSR_MIMPID]    = { "mimpid",    any,   read_mimpid    },
    [CSR_MHARTID]   = { "mhartid",   any,   read_mhartid   },

    [CSR_MCONFIGPTR]  = { "mconfigptr", any,   read_zero,
                          .min_priv_ver = PRIV_VERSION_1_12_0 },
    /* Machine Trap Setup */
    [CSR_MSTATUS]     = { "mstatus",    any,   read_mstatus, write_mstatus,
                          NULL,                read_mstatus_i128           },
    [CSR_MISA]        = { "misa",       any,   read_misa,    write_misa,
                          NULL,                read_misa_i128              },
    [CSR_MIDELEG]     = { "mideleg",    any,   NULL, NULL,   rmw_mideleg   },
    [CSR_MEDELEG]     = { "medeleg",    any,   read_medeleg, write_medeleg },
    [CSR_MIE]         = { "mie",        any,   NULL, NULL,   rmw_mie       },
    [CSR_MTVEC]       = { "mtvec",      any,   read_mtvec,   write_mtvec   },
    [CSR_MCOUNTEREN]  = { "mcounteren", umode, read_mcounteren,
                          write_mcounteren                                 },

    [CSR_MSTATUSH]    = { "mstatush",   any32, read_mstatush,
                          write_mstatush                                   },

    /* Machine Trap Handling */
    [CSR_MSCRATCH] = { "mscratch", any,  read_mscratch, write_mscratch,
                       NULL, read_mscratch_i128, write_mscratch_i128   },
    [CSR_MEPC]     = { "mepc",     any,  read_mepc,     write_mepc     },
    [CSR_MCAUSE]   = { "mcause",   any,  read_mcause,   write_mcause   },
    [CSR_MTVAL]    = { "mtval",    any,  read_mtval,    write_mtval    },
    [CSR_MIP]      = { "mip",      any,  NULL,    NULL, rmw_mip        },

    /* Machine-Level Window to Indirectly Accessed Registers (AIA) */
    [CSR_MISELECT] = { "miselect", aia_any,   NULL, NULL,    rmw_xiselect },
    [CSR_MIREG]    = { "mireg",    aia_any,   NULL, NULL,    rmw_xireg },

    /* Machine-Level Interrupts (AIA) */
    [CSR_MTOPEI]   = { "mtopei",   aia_any, NULL, NULL, rmw_xtopei },
    [CSR_MTOPI]    = { "mtopi",    aia_any, read_mtopi },

    /* Virtual Interrupts for Supervisor Level (AIA) */
    [CSR_MVIEN]    = { "mvien",    aia_any, read_zero, write_ignore },
    [CSR_MVIP]     = { "mvip",     aia_any, read_zero, write_ignore },

    /* Machine-Level High-Half CSRs (AIA) */
    [CSR_MIDELEGH] = { "midelegh", aia_any32, NULL, NULL, rmw_midelegh },
    [CSR_MIEH]     = { "mieh",     aia_any32, NULL, NULL, rmw_mieh     },
    [CSR_MVIENH]   = { "mvienh",   aia_any32, read_zero,  write_ignore },
    [CSR_MVIPH]    = { "mviph",    aia_any32, read_zero,  write_ignore },
    [CSR_MIPH]     = { "miph",     aia_any32, NULL, NULL, rmw_miph     },

    /* Execution environment configuration */
    [CSR_MENVCFG]  = { "menvcfg",  umode, read_menvcfg,  write_menvcfg,
                       .min_priv_ver = PRIV_VERSION_1_12_0              },
    [CSR_MENVCFGH] = { "menvcfgh", umode32, read_menvcfgh, write_menvcfgh,
                       .min_priv_ver = PRIV_VERSION_1_12_0              },
    [CSR_SENVCFG]  = { "senvcfg",  smode, read_senvcfg,  write_senvcfg,
                       .min_priv_ver = PRIV_VERSION_1_12_0              },
    [CSR_HENVCFG]  = { "henvcfg",  hmode, read_henvcfg, write_henvcfg,
                       .min_priv_ver = PRIV_VERSION_1_12_0              },
    [CSR_HENVCFGH] = { "henvcfgh", hmode32, read_henvcfgh, write_henvcfgh,
                       .min_priv_ver = PRIV_VERSION_1_12_0              },

    /* Supervisor Trap Setup */
    [CSR_SSTATUS]    = { "sstatus",    smode, read_sstatus,    write_sstatus,
                         NULL,                read_sstatus_i128               },
    [CSR_SIE]        = { "sie",        smode, NULL,   NULL,    rmw_sie        },
    [CSR_STVEC]      = { "stvec",      smode, read_stvec,      write_stvec    },
    [CSR_SCOUNTEREN] = { "scounteren", smode, read_scounteren,
                         write_scounteren                                     },

    /* Supervisor Trap Handling */
    [CSR_SSCRATCH] = { "sscratch", smode, read_sscratch, write_sscratch,
                       NULL, read_sscratch_i128, write_sscratch_i128    },
    [CSR_SEPC]     = { "sepc",     smode, read_sepc,     write_sepc     },
    [CSR_SCAUSE]   = { "scause",   smode, read_scause,   write_scause   },
    [CSR_STVAL]    = { "stval",    smode, read_stval,    write_stval    },
    [CSR_SIP]      = { "sip",      smode, NULL,    NULL, rmw_sip        },
    [CSR_STIMECMP] = { "stimecmp", sstc, read_stimecmp, write_stimecmp,
                       .min_priv_ver = PRIV_VERSION_1_12_0 },
    [CSR_STIMECMPH] = { "stimecmph", sstc_32, read_stimecmph, write_stimecmph,
                        .min_priv_ver = PRIV_VERSION_1_12_0 },
    [CSR_VSTIMECMP] = { "vstimecmp", sstc, read_vstimecmp,
                        write_vstimecmp,
                        .min_priv_ver = PRIV_VERSION_1_12_0 },
    [CSR_VSTIMECMPH] = { "vstimecmph", sstc_32, read_vstimecmph,
                         write_vstimecmph,
                         .min_priv_ver = PRIV_VERSION_1_12_0 },

    /* Supervisor Protection and Translation */
    [CSR_SATP]     = { "satp",     smode, read_satp,     write_satp     },

    /* Supervisor-Level Window to Indirectly Accessed Registers (AIA) */
    [CSR_SISELECT]   = { "siselect",   aia_smode, NULL, NULL, rmw_xiselect },
    [CSR_SIREG]      = { "sireg",      aia_smode, NULL, NULL, rmw_xireg },

    /* Supervisor-Level Interrupts (AIA) */
    [CSR_STOPEI]     = { "stopei",     aia_smode, NULL, NULL, rmw_xtopei },
    [CSR_STOPI]      = { "stopi",      aia_smode, read_stopi },

    /* Supervisor-Level High-Half CSRs (AIA) */
    [CSR_SIEH]       = { "sieh",   aia_smode32, NULL, NULL, rmw_sieh },
    [CSR_SIPH]       = { "siph",   aia_smode32, NULL, NULL, rmw_siph },

    [CSR_HSTATUS]     = { "hstatus",     hmode,   read_hstatus, write_hstatus,
                          .min_priv_ver = PRIV_VERSION_1_12_0                },
    [CSR_HEDELEG]     = { "hedeleg",     hmode,   read_hedeleg, write_hedeleg,
                          .min_priv_ver = PRIV_VERSION_1_12_0                },
    [CSR_HIDELEG]     = { "hideleg",     hmode,   NULL,   NULL, rmw_hideleg,
                          .min_priv_ver = PRIV_VERSION_1_12_0                },
    [CSR_HVIP]        = { "hvip",        hmode,   NULL,   NULL, rmw_hvip,
                          .min_priv_ver = PRIV_VERSION_1_12_0                },
    [CSR_HIP]         = { "hip",         hmode,   NULL,   NULL, rmw_hip,
                          .min_priv_ver = PRIV_VERSION_1_12_0                },
    [CSR_HIE]         = { "hie",         hmode,   NULL,   NULL, rmw_hie,
                          .min_priv_ver = PRIV_VERSION_1_12_0                },
    [CSR_HCOUNTEREN]  = { "hcounteren",  hmode,   read_hcounteren,
                          write_hcounteren,
                          .min_priv_ver = PRIV_VERSION_1_12_0                },
    [CSR_HGEIE]       = { "hgeie",       hmode,   read_hgeie,   write_hgeie,
                          .min_priv_ver = PRIV_VERSION_1_12_0                },
    [CSR_HTVAL]       = { "htval",       hmode,   read_htval,   write_htval,
                          .min_priv_ver = PRIV_VERSION_1_12_0                },
    [CSR_HTINST]      = { "htinst",      hmode,   read_htinst,  write_htinst,
                          .min_priv_ver = PRIV_VERSION_1_12_0                },
    [CSR_HGEIP]       = { "hgeip",       hmode,   read_hgeip,
                          .min_priv_ver = PRIV_VERSION_1_12_0                },
    [CSR_HGATP]       = { "hgatp",       hmode,   read_hgatp,   write_hgatp,
                          .min_priv_ver = PRIV_VERSION_1_12_0                },
    [CSR_HTIMEDELTA]  = { "htimedelta",  hmode,   read_htimedelta,
                          write_htimedelta,
                          .min_priv_ver = PRIV_VERSION_1_12_0                },
    [CSR_HTIMEDELTAH] = { "htimedeltah", hmode32, read_htimedeltah,
                          write_htimedeltah,
                          .min_priv_ver = PRIV_VERSION_1_12_0                },

    [CSR_VSSTATUS]    = { "vsstatus",    hmode,   read_vsstatus,
                          write_vsstatus,
                          .min_priv_ver = PRIV_VERSION_1_12_0                },
    [CSR_VSIP]        = { "vsip",        hmode,   NULL,    NULL, rmw_vsip,
                          .min_priv_ver = PRIV_VERSION_1_12_0                },
    [CSR_VSIE]        = { "vsie",        hmode,   NULL,    NULL, rmw_vsie ,
                          .min_priv_ver = PRIV_VERSION_1_12_0                },
    [CSR_VSTVEC]      = { "vstvec",      hmode,   read_vstvec,   write_vstvec,
                          .min_priv_ver = PRIV_VERSION_1_12_0                },
    [CSR_VSSCRATCH]   = { "vsscratch",   hmode,   read_vsscratch,
                          write_vsscratch,
                          .min_priv_ver = PRIV_VERSION_1_12_0                },
    [CSR_VSEPC]       = { "vsepc",       hmode,   read_vsepc,    write_vsepc,
                          .min_priv_ver = PRIV_VERSION_1_12_0                },
    [CSR_VSCAUSE]     = { "vscause",     hmode,   read_vscause,  write_vscause,
                          .min_priv_ver = PRIV_VERSION_1_12_0                },
    [CSR_VSTVAL]      = { "vstval",      hmode,   read_vstval,   write_vstval,
                          .min_priv_ver = PRIV_VERSION_1_12_0                },
    [CSR_VSATP]       = { "vsatp",       hmode,   read_vsatp,    write_vsatp,
                          .min_priv_ver = PRIV_VERSION_1_12_0                },

    [CSR_MTVAL2]      = { "mtval2",      hmode,   read_mtval2,   write_mtval2,
                          .min_priv_ver = PRIV_VERSION_1_12_0                },
    [CSR_MTINST]      = { "mtinst",      hmode,   read_mtinst,   write_mtinst,
                          .min_priv_ver = PRIV_VERSION_1_12_0                },

    /* Virtual Interrupts and Interrupt Priorities (H-extension with AIA) */
    [CSR_HVIEN]       = { "hvien",       aia_hmode, read_zero, write_ignore },
    [CSR_HVICTL]      = { "hvictl",      aia_hmode, read_hvictl,
                          write_hvictl                                      },
    [CSR_HVIPRIO1]    = { "hviprio1",    aia_hmode, read_hviprio1,
                          write_hviprio1                                    },
    [CSR_HVIPRIO2]    = { "hviprio2",    aia_hmode, read_hviprio2,
                          write_hviprio2                                    },

    /*
     * VS-Level Window to Indirectly Accessed Registers (H-extension with AIA)
     */
    [CSR_VSISELECT]   = { "vsiselect",   aia_hmode, NULL, NULL,
                          rmw_xiselect                                     },
    [CSR_VSIREG]      = { "vsireg",      aia_hmode, NULL, NULL, rmw_xireg  },

    /* VS-Level Interrupts (H-extension with AIA) */
    [CSR_VSTOPEI]     = { "vstopei",     aia_hmode, NULL, NULL, rmw_xtopei },
    [CSR_VSTOPI]      = { "vstopi",      aia_hmode, read_vstopi },

    /* Hypervisor and VS-Level High-Half CSRs (H-extension with AIA) */
    [CSR_HIDELEGH]    = { "hidelegh",    aia_hmode32, NULL, NULL,
                          rmw_hidelegh                                      },
    [CSR_HVIENH]      = { "hvienh",      aia_hmode32, read_zero,
                          write_ignore                                      },
    [CSR_HVIPH]       = { "hviph",       aia_hmode32, NULL, NULL, rmw_hviph },
    [CSR_HVIPRIO1H]   = { "hviprio1h",   aia_hmode32, read_hviprio1h,
                          write_hviprio1h                                   },
    [CSR_HVIPRIO2H]   = { "hviprio2h",   aia_hmode32, read_hviprio2h,
                          write_hviprio2h                                   },
    [CSR_VSIEH]       = { "vsieh",       aia_hmode32, NULL, NULL, rmw_vsieh },
    [CSR_VSIPH]       = { "vsiph",       aia_hmode32, NULL, NULL, rmw_vsiph },

    /* Physical Memory Protection */
    [CSR_MSECCFG]    = { "mseccfg",  epmp, read_mseccfg, write_mseccfg,
                         .min_priv_ver = PRIV_VERSION_1_11_0           },
    [CSR_PMPCFG0]    = { "pmpcfg0",   pmp, read_pmpcfg,  write_pmpcfg  },
    [CSR_PMPCFG1]    = { "pmpcfg1",   pmp, read_pmpcfg,  write_pmpcfg  },
    [CSR_PMPCFG2]    = { "pmpcfg2",   pmp, read_pmpcfg,  write_pmpcfg  },
    [CSR_PMPCFG3]    = { "pmpcfg3",   pmp, read_pmpcfg,  write_pmpcfg  },
    [CSR_PMPADDR0]   = { "pmpaddr0",  pmp, read_pmpaddr, write_pmpaddr },
    [CSR_PMPADDR1]   = { "pmpaddr1",  pmp, read_pmpaddr, write_pmpaddr },
    [CSR_PMPADDR2]   = { "pmpaddr2",  pmp, read_pmpaddr, write_pmpaddr },
    [CSR_PMPADDR3]   = { "pmpaddr3",  pmp, read_pmpaddr, write_pmpaddr },
    [CSR_PMPADDR4]   = { "pmpaddr4",  pmp, read_pmpaddr, write_pmpaddr },
    [CSR_PMPADDR5]   = { "pmpaddr5",  pmp, read_pmpaddr, write_pmpaddr },
    [CSR_PMPADDR6]   = { "pmpaddr6",  pmp, read_pmpaddr, write_pmpaddr },
    [CSR_PMPADDR7]   = { "pmpaddr7",  pmp, read_pmpaddr, write_pmpaddr },
    [CSR_PMPADDR8]   = { "pmpaddr8",  pmp, read_pmpaddr, write_pmpaddr },
    [CSR_PMPADDR9]   = { "pmpaddr9",  pmp, read_pmpaddr, write_pmpaddr },
    [CSR_PMPADDR10]  = { "pmpaddr10", pmp, read_pmpaddr, write_pmpaddr },
    [CSR_PMPADDR11]  = { "pmpaddr11", pmp, read_pmpaddr, write_pmpaddr },
    [CSR_PMPADDR12]  = { "pmpaddr12", pmp, read_pmpaddr, write_pmpaddr },
    [CSR_PMPADDR13]  = { "pmpaddr13", pmp, read_pmpaddr, write_pmpaddr },
    [CSR_PMPADDR14] =  { "pmpaddr14", pmp, read_pmpaddr, write_pmpaddr },
    [CSR_PMPADDR15] =  { "pmpaddr15", pmp, read_pmpaddr, write_pmpaddr },

    /* Debug CSRs */
    [CSR_TSELECT]   =  { "tselect", debug, read_tselect, write_tselect },
    [CSR_TDATA1]    =  { "tdata1",  debug, read_tdata,   write_tdata   },
    [CSR_TDATA2]    =  { "tdata2",  debug, read_tdata,   write_tdata   },
    [CSR_TDATA3]    =  { "tdata3",  debug, read_tdata,   write_tdata   },
    [CSR_TINFO]     =  { "tinfo",   debug, read_tinfo,   write_ignore  },

    /* User Pointer Masking */
    [CSR_UMTE]    =    { "umte",    pointer_masking, read_umte,  write_umte },
    [CSR_UPMMASK] =    { "upmmask", pointer_masking, read_upmmask,
                         write_upmmask                                      },
    [CSR_UPMBASE] =    { "upmbase", pointer_masking, read_upmbase,
                         write_upmbase                                      },
    /* Machine Pointer Masking */
    [CSR_MMTE]    =    { "mmte",    pointer_masking, read_mmte,  write_mmte },
    [CSR_MPMMASK] =    { "mpmmask", pointer_masking, read_mpmmask,
                         write_mpmmask                                      },
    [CSR_MPMBASE] =    { "mpmbase", pointer_masking, read_mpmbase,
                         write_mpmbase                                      },
    /* Supervisor Pointer Masking */
    [CSR_SMTE]    =    { "smte",    pointer_masking, read_smte,  write_smte },
    [CSR_SPMMASK] =    { "spmmask", pointer_masking, read_spmmask,
                         write_spmmask                                      },
    [CSR_SPMBASE] =    { "spmbase", pointer_masking, read_spmbase,
                         write_spmbase                                      },

    /* Performance Counters */
    [CSR_HPMCOUNTER3]    = { "hpmcounter3",    ctr,    read_hpmcounter },
    [CSR_HPMCOUNTER4]    = { "hpmcounter4",    ctr,    read_hpmcounter },
    [CSR_HPMCOUNTER5]    = { "hpmcounter5",    ctr,    read_hpmcounter },
    [CSR_HPMCOUNTER6]    = { "hpmcounter6",    ctr,    read_hpmcounter },
    [CSR_HPMCOUNTER7]    = { "hpmcounter7",    ctr,    read_hpmcounter },
    [CSR_HPMCOUNTER8]    = { "hpmcounter8",    ctr,    read_hpmcounter },
    [CSR_HPMCOUNTER9]    = { "hpmcounter9",    ctr,    read_hpmcounter },
    [CSR_HPMCOUNTER10]   = { "hpmcounter10",   ctr,    read_hpmcounter },
    [CSR_HPMCOUNTER11]   = { "hpmcounter11",   ctr,    read_hpmcounter },
    [CSR_HPMCOUNTER12]   = { "hpmcounter12",   ctr,    read_hpmcounter },
    [CSR_HPMCOUNTER13]   = { "hpmcounter13",   ctr,    read_hpmcounter },
    [CSR_HPMCOUNTER14]   = { "hpmcounter14",   ctr,    read_hpmcounter },
    [CSR_HPMCOUNTER15]   = { "hpmcounter15",   ctr,    read_hpmcounter },
    [CSR_HPMCOUNTER16]   = { "hpmcounter16",   ctr,    read_hpmcounter },
    [CSR_HPMCOUNTER17]   = { "hpmcounter17",   ctr,    read_hpmcounter },
    [CSR_HPMCOUNTER18]   = { "hpmcounter18",   ctr,    read_hpmcounter },
    [CSR_HPMCOUNTER19]   = { "hpmcounter19",   ctr,    read_hpmcounter },
    [CSR_HPMCOUNTER20]   = { "hpmcounter20",   ctr,    read_hpmcounter },
    [CSR_HPMCOUNTER21]   = { "hpmcounter21",   ctr,    read_hpmcounter },
    [CSR_HPMCOUNTER22]   = { "hpmcounter22",   ctr,    read_hpmcounter },
    [CSR_HPMCOUNTER23]   = { "hpmcounter23",   ctr,    read_hpmcounter },
    [CSR_HPMCOUNTER24]   = { "hpmcounter24",   ctr,    read_hpmcounter },
    [CSR_HPMCOUNTER25]   = { "hpmcounter25",   ctr,    read_hpmcounter },
    [CSR_HPMCOUNTER26]   = { "hpmcounter26",   ctr,    read_hpmcounter },
    [CSR_HPMCOUNTER27]   = { "hpmcounter27",   ctr,    read_hpmcounter },
    [CSR_HPMCOUNTER28]   = { "hpmcounter28",   ctr,    read_hpmcounter },
    [CSR_HPMCOUNTER29]   = { "hpmcounter29",   ctr,    read_hpmcounter },
    [CSR_HPMCOUNTER30]   = { "hpmcounter30",   ctr,    read_hpmcounter },
    [CSR_HPMCOUNTER31]   = { "hpmcounter31",   ctr,    read_hpmcounter },

    [CSR_MHPMCOUNTER3]   = { "mhpmcounter3",   mctr,    read_hpmcounter,
                             write_mhpmcounter                         },
    [CSR_MHPMCOUNTER4]   = { "mhpmcounter4",   mctr,    read_hpmcounter,
                             write_mhpmcounter                         },
    [CSR_MHPMCOUNTER5]   = { "mhpmcounter5",   mctr,    read_hpmcounter,
                             write_mhpmcounter                         },
    [CSR_MHPMCOUNTER6]   = { "mhpmcounter6",   mctr,    read_hpmcounter,
                             write_mhpmcounter                         },
    [CSR_MHPMCOUNTER7]   = { "mhpmcounter7",   mctr,    read_hpmcounter,
                             write_mhpmcounter                         },
    [CSR_MHPMCOUNTER8]   = { "mhpmcounter8",   mctr,    read_hpmcounter,
                             write_mhpmcounter                         },
    [CSR_MHPMCOUNTER9]   = { "mhpmcounter9",   mctr,    read_hpmcounter,
                             write_mhpmcounter                         },
    [CSR_MHPMCOUNTER10]  = { "mhpmcounter10",  mctr,    read_hpmcounter,
                             write_mhpmcounter                         },
    [CSR_MHPMCOUNTER11]  = { "mhpmcounter11",  mctr,    read_hpmcounter,
                             write_mhpmcounter                         },
    [CSR_MHPMCOUNTER12]  = { "mhpmcounter12",  mctr,    read_hpmcounter,
                             write_mhpmcounter                         },
    [CSR_MHPMCOUNTER13]  = { "mhpmcounter13",  mctr,    read_hpmcounter,
                             write_mhpmcounter                         },
    [CSR_MHPMCOUNTER14]  = { "mhpmcounter14",  mctr,    read_hpmcounter,
                             write_mhpmcounter                         },
    [CSR_MHPMCOUNTER15]  = { "mhpmcounter15",  mctr,    read_hpmcounter,
                             write_mhpmcounter                         },
    [CSR_MHPMCOUNTER16]  = { "mhpmcounter16",  mctr,    read_hpmcounter,
                             write_mhpmcounter                         },
    [CSR_MHPMCOUNTER17]  = { "mhpmcounter17",  mctr,    read_hpmcounter,
                             write_mhpmcounter                         },
    [CSR_MHPMCOUNTER18]  = { "mhpmcounter18",  mctr,    read_hpmcounter,
                             write_mhpmcounter                         },
    [CSR_MHPMCOUNTER19]  = { "mhpmcounter19",  mctr,    read_hpmcounter,
                             write_mhpmcounter                         },
    [CSR_MHPMCOUNTER20]  = { "mhpmcounter20",  mctr,    read_hpmcounter,
                             write_mhpmcounter                         },
    [CSR_MHPMCOUNTER21]  = { "mhpmcounter21",  mctr,    read_hpmcounter,
                             write_mhpmcounter                         },
    [CSR_MHPMCOUNTER22]  = { "mhpmcounter22",  mctr,    read_hpmcounter,
                             write_mhpmcounter                         },
    [CSR_MHPMCOUNTER23]  = { "mhpmcounter23",  mctr,    read_hpmcounter,
                             write_mhpmcounter                         },
    [CSR_MHPMCOUNTER24]  = { "mhpmcounter24",  mctr,    read_hpmcounter,
                             write_mhpmcounter                         },
    [CSR_MHPMCOUNTER25]  = { "mhpmcounter25",  mctr,    read_hpmcounter,
                             write_mhpmcounter                         },
    [CSR_MHPMCOUNTER26]  = { "mhpmcounter26",  mctr,    read_hpmcounter,
                             write_mhpmcounter                         },
    [CSR_MHPMCOUNTER27]  = { "mhpmcounter27",  mctr,    read_hpmcounter,
                             write_mhpmcounter                         },
    [CSR_MHPMCOUNTER28]  = { "mhpmcounter28",  mctr,    read_hpmcounter,
                             write_mhpmcounter                         },
    [CSR_MHPMCOUNTER29]  = { "mhpmcounter29",  mctr,    read_hpmcounter,
                             write_mhpmcounter                         },
    [CSR_MHPMCOUNTER30]  = { "mhpmcounter30",  mctr,    read_hpmcounter,
                             write_mhpmcounter                         },
    [CSR_MHPMCOUNTER31]  = { "mhpmcounter31",  mctr,    read_hpmcounter,
                             write_mhpmcounter                         },

    [CSR_MCOUNTINHIBIT]  = { "mcountinhibit",  any, read_mcountinhibit,
                             write_mcountinhibit,
                             .min_priv_ver = PRIV_VERSION_1_11_0       },

    [CSR_MHPMEVENT3]     = { "mhpmevent3",     any,    read_mhpmevent,
                             write_mhpmevent                           },
    [CSR_MHPMEVENT4]     = { "mhpmevent4",     any,    read_mhpmevent,
                             write_mhpmevent                           },
    [CSR_MHPMEVENT5]     = { "mhpmevent5",     any,    read_mhpmevent,
                             write_mhpmevent                           },
    [CSR_MHPMEVENT6]     = { "mhpmevent6",     any,    read_mhpmevent,
                             write_mhpmevent                           },
    [CSR_MHPMEVENT7]     = { "mhpmevent7",     any,    read_mhpmevent,
                             write_mhpmevent                           },
    [CSR_MHPMEVENT8]     = { "mhpmevent8",     any,    read_mhpmevent,
                             write_mhpmevent                           },
    [CSR_MHPMEVENT9]     = { "mhpmevent9",     any,    read_mhpmevent,
                             write_mhpmevent                           },
    [CSR_MHPMEVENT10]    = { "mhpmevent10",    any,    read_mhpmevent,
                             write_mhpmevent                           },
    [CSR_MHPMEVENT11]    = { "mhpmevent11",    any,    read_mhpmevent,
                             write_mhpmevent                           },
    [CSR_MHPMEVENT12]    = { "mhpmevent12",    any,    read_mhpmevent,
                             write_mhpmevent                           },
    [CSR_MHPMEVENT13]    = { "mhpmevent13",    any,    read_mhpmevent,
                             write_mhpmevent                           },
    [CSR_MHPMEVENT14]    = { "mhpmevent14",    any,    read_mhpmevent,
                             write_mhpmevent                           },
    [CSR_MHPMEVENT15]    = { "mhpmevent15",    any,    read_mhpmevent,
                             write_mhpmevent                           },
    [CSR_MHPMEVENT16]    = { "mhpmevent16",    any,    read_mhpmevent,
                             write_mhpmevent                           },
    [CSR_MHPMEVENT17]    = { "mhpmevent17",    any,    read_mhpmevent,
                             write_mhpmevent                           },
    [CSR_MHPMEVENT18]    = { "mhpmevent18",    any,    read_mhpmevent,
                             write_mhpmevent                           },
    [CSR_MHPMEVENT19]    = { "mhpmevent19",    any,    read_mhpmevent,
                             write_mhpmevent                           },
    [CSR_MHPMEVENT20]    = { "mhpmevent20",    any,    read_mhpmevent,
                             write_mhpmevent                           },
    [CSR_MHPMEVENT21]    = { "mhpmevent21",    any,    read_mhpmevent,
                             write_mhpmevent                           },
    [CSR_MHPMEVENT22]    = { "mhpmevent22",    any,    read_mhpmevent,
                             write_mhpmevent                           },
    [CSR_MHPMEVENT23]    = { "mhpmevent23",    any,    read_mhpmevent,
                             write_mhpmevent                           },
    [CSR_MHPMEVENT24]    = { "mhpmevent24",    any,    read_mhpmevent,
                             write_mhpmevent                           },
    [CSR_MHPMEVENT25]    = { "mhpmevent25",    any,    read_mhpmevent,
                             write_mhpmevent                           },
    [CSR_MHPMEVENT26]    = { "mhpmevent26",    any,    read_mhpmevent,
                             write_mhpmevent                           },
    [CSR_MHPMEVENT27]    = { "mhpmevent27",    any,    read_mhpmevent,
                             write_mhpmevent                           },
    [CSR_MHPMEVENT28]    = { "mhpmevent28",    any,    read_mhpmevent,
                             write_mhpmevent                           },
    [CSR_MHPMEVENT29]    = { "mhpmevent29",    any,    read_mhpmevent,
                             write_mhpmevent                           },
    [CSR_MHPMEVENT30]    = { "mhpmevent30",    any,    read_mhpmevent,
                             write_mhpmevent                           },
    [CSR_MHPMEVENT31]    = { "mhpmevent31",    any,    read_mhpmevent,
                             write_mhpmevent                           },

    [CSR_MHPMEVENT3H]    = { "mhpmevent3h",    sscofpmf,  read_mhpmeventh,
                             write_mhpmeventh,
                             .min_priv_ver = PRIV_VERSION_1_12_0        },
    [CSR_MHPMEVENT4H]    = { "mhpmevent4h",    sscofpmf,  read_mhpmeventh,
                             write_mhpmeventh,
                             .min_priv_ver = PRIV_VERSION_1_12_0        },
    [CSR_MHPMEVENT5H]    = { "mhpmevent5h",    sscofpmf,  read_mhpmeventh,
                             write_mhpmeventh,
                             .min_priv_ver = PRIV_VERSION_1_12_0        },
    [CSR_MHPMEVENT6H]    = { "mhpmevent6h",    sscofpmf,  read_mhpmeventh,
                             write_mhpmeventh,
                             .min_priv_ver = PRIV_VERSION_1_12_0        },
    [CSR_MHPMEVENT7H]    = { "mhpmevent7h",    sscofpmf,  read_mhpmeventh,
                             write_mhpmeventh,
                             .min_priv_ver = PRIV_VERSION_1_12_0        },
    [CSR_MHPMEVENT8H]    = { "mhpmevent8h",    sscofpmf,  read_mhpmeventh,
                             write_mhpmeventh,
                             .min_priv_ver = PRIV_VERSION_1_12_0        },
    [CSR_MHPMEVENT9H]    = { "mhpmevent9h",    sscofpmf,  read_mhpmeventh,
                             write_mhpmeventh,
                             .min_priv_ver = PRIV_VERSION_1_12_0        },
    [CSR_MHPMEVENT10H]   = { "mhpmevent10h",    sscofpmf,  read_mhpmeventh,
                             write_mhpmeventh,
                             .min_priv_ver = PRIV_VERSION_1_12_0        },
    [CSR_MHPMEVENT11H]   = { "mhpmevent11h",    sscofpmf,  read_mhpmeventh,
                             write_mhpmeventh,
                             .min_priv_ver = PRIV_VERSION_1_12_0        },
    [CSR_MHPMEVENT12H]   = { "mhpmevent12h",    sscofpmf,  read_mhpmeventh,
                             write_mhpmeventh,
                             .min_priv_ver = PRIV_VERSION_1_12_0        },
    [CSR_MHPMEVENT13H]   = { "mhpmevent13h",    sscofpmf,  read_mhpmeventh,
                             write_mhpmeventh,
                             .min_priv_ver = PRIV_VERSION_1_12_0        },
    [CSR_MHPMEVENT14H]   = { "mhpmevent14h",    sscofpmf,  read_mhpmeventh,
                             write_mhpmeventh,
                             .min_priv_ver = PRIV_VERSION_1_12_0        },
    [CSR_MHPMEVENT15H]   = { "mhpmevent15h",    sscofpmf,  read_mhpmeventh,
                             write_mhpmeventh,
                             .min_priv_ver = PRIV_VERSION_1_12_0        },
    [CSR_MHPMEVENT16H]   = { "mhpmevent16h",    sscofpmf,  read_mhpmeventh,
                             write_mhpmeventh,
                             .min_priv_ver = PRIV_VERSION_1_12_0        },
    [CSR_MHPMEVENT17H]   = { "mhpmevent17h",    sscofpmf,  read_mhpmeventh,
                             write_mhpmeventh,
                             .min_priv_ver = PRIV_VERSION_1_12_0        },
    [CSR_MHPMEVENT18H]   = { "mhpmevent18h",    sscofpmf,  read_mhpmeventh,
                             write_mhpmeventh,
                             .min_priv_ver = PRIV_VERSION_1_12_0        },
    [CSR_MHPMEVENT19H]   = { "mhpmevent19h",    sscofpmf,  read_mhpmeventh,
                             write_mhpmeventh,
                             .min_priv_ver = PRIV_VERSION_1_12_0        },
    [CSR_MHPMEVENT20H]   = { "mhpmevent20h",    sscofpmf,  read_mhpmeventh,
                             write_mhpmeventh,
                             .min_priv_ver = PRIV_VERSION_1_12_0        },
    [CSR_MHPMEVENT21H]   = { "mhpmevent21h",    sscofpmf,  read_mhpmeventh,
                             write_mhpmeventh,
                             .min_priv_ver = PRIV_VERSION_1_12_0        },
    [CSR_MHPMEVENT22H]   = { "mhpmevent22h",    sscofpmf,  read_mhpmeventh,
                             write_mhpmeventh,
                             .min_priv_ver = PRIV_VERSION_1_12_0        },
    [CSR_MHPMEVENT23H]   = { "mhpmevent23h",    sscofpmf,  read_mhpmeventh,
                             write_mhpmeventh,
                             .min_priv_ver = PRIV_VERSION_1_12_0        },
    [CSR_MHPMEVENT24H]   = { "mhpmevent24h",    sscofpmf,  read_mhpmeventh,
                             write_mhpmeventh,
                             .min_priv_ver = PRIV_VERSION_1_12_0        },
    [CSR_MHPMEVENT25H]   = { "mhpmevent25h",    sscofpmf,  read_mhpmeventh,
                             write_mhpmeventh,
                             .min_priv_ver = PRIV_VERSION_1_12_0        },
    [CSR_MHPMEVENT26H]   = { "mhpmevent26h",    sscofpmf,  read_mhpmeventh,
                             write_mhpmeventh,
                             .min_priv_ver = PRIV_VERSION_1_12_0        },
    [CSR_MHPMEVENT27H]   = { "mhpmevent27h",    sscofpmf,  read_mhpmeventh,
                             write_mhpmeventh,
                             .min_priv_ver = PRIV_VERSION_1_12_0        },
    [CSR_MHPMEVENT28H]   = { "mhpmevent28h",    sscofpmf,  read_mhpmeventh,
                             write_mhpmeventh,
                             .min_priv_ver = PRIV_VERSION_1_12_0        },
    [CSR_MHPMEVENT29H]   = { "mhpmevent29h",    sscofpmf,  read_mhpmeventh,
                             write_mhpmeventh,
                             .min_priv_ver = PRIV_VERSION_1_12_0        },
    [CSR_MHPMEVENT30H]   = { "mhpmevent30h",    sscofpmf,  read_mhpmeventh,
                             write_mhpmeventh,
                             .min_priv_ver = PRIV_VERSION_1_12_0        },
    [CSR_MHPMEVENT31H]   = { "mhpmevent31h",    sscofpmf,  read_mhpmeventh,
                             write_mhpmeventh,
                             .min_priv_ver = PRIV_VERSION_1_12_0        },

    [CSR_HPMCOUNTER3H]   = { "hpmcounter3h",   ctr32,  read_hpmcounterh },
    [CSR_HPMCOUNTER4H]   = { "hpmcounter4h",   ctr32,  read_hpmcounterh },
    [CSR_HPMCOUNTER5H]   = { "hpmcounter5h",   ctr32,  read_hpmcounterh },
    [CSR_HPMCOUNTER6H]   = { "hpmcounter6h",   ctr32,  read_hpmcounterh },
    [CSR_HPMCOUNTER7H]   = { "hpmcounter7h",   ctr32,  read_hpmcounterh },
    [CSR_HPMCOUNTER8H]   = { "hpmcounter8h",   ctr32,  read_hpmcounterh },
    [CSR_HPMCOUNTER9H]   = { "hpmcounter9h",   ctr32,  read_hpmcounterh },
    [CSR_HPMCOUNTER10H]  = { "hpmcounter10h",  ctr32,  read_hpmcounterh },
    [CSR_HPMCOUNTER11H]  = { "hpmcounter11h",  ctr32,  read_hpmcounterh },
    [CSR_HPMCOUNTER12H]  = { "hpmcounter12h",  ctr32,  read_hpmcounterh },
    [CSR_HPMCOUNTER13H]  = { "hpmcounter13h",  ctr32,  read_hpmcounterh },
    [CSR_HPMCOUNTER14H]  = { "hpmcounter14h",  ctr32,  read_hpmcounterh },
    [CSR_HPMCOUNTER15H]  = { "hpmcounter15h",  ctr32,  read_hpmcounterh },
    [CSR_HPMCOUNTER16H]  = { "hpmcounter16h",  ctr32,  read_hpmcounterh },
    [CSR_HPMCOUNTER17H]  = { "hpmcounter17h",  ctr32,  read_hpmcounterh },
    [CSR_HPMCOUNTER18H]  = { "hpmcounter18h",  ctr32,  read_hpmcounterh },
    [CSR_HPMCOUNTER19H]  = { "hpmcounter19h",  ctr32,  read_hpmcounterh },
    [CSR_HPMCOUNTER20H]  = { "hpmcounter20h",  ctr32,  read_hpmcounterh },
    [CSR_HPMCOUNTER21H]  = { "hpmcounter21h",  ctr32,  read_hpmcounterh },
    [CSR_HPMCOUNTER22H]  = { "hpmcounter22h",  ctr32,  read_hpmcounterh },
    [CSR_HPMCOUNTER23H]  = { "hpmcounter23h",  ctr32,  read_hpmcounterh },
    [CSR_HPMCOUNTER24H]  = { "hpmcounter24h",  ctr32,  read_hpmcounterh },
    [CSR_HPMCOUNTER25H]  = { "hpmcounter25h",  ctr32,  read_hpmcounterh },
    [CSR_HPMCOUNTER26H]  = { "hpmcounter26h",  ctr32,  read_hpmcounterh },
    [CSR_HPMCOUNTER27H]  = { "hpmcounter27h",  ctr32,  read_hpmcounterh },
    [CSR_HPMCOUNTER28H]  = { "hpmcounter28h",  ctr32,  read_hpmcounterh },
    [CSR_HPMCOUNTER29H]  = { "hpmcounter29h",  ctr32,  read_hpmcounterh },
    [CSR_HPMCOUNTER30H]  = { "hpmcounter30h",  ctr32,  read_hpmcounterh },
    [CSR_HPMCOUNTER31H]  = { "hpmcounter31h",  ctr32,  read_hpmcounterh },

    [CSR_MHPMCOUNTER3H]  = { "mhpmcounter3h",  mctr32,  read_hpmcounterh,
                             write_mhpmcounterh                         },
    [CSR_MHPMCOUNTER4H]  = { "mhpmcounter4h",  mctr32,  read_hpmcounterh,
                             write_mhpmcounterh                         },
    [CSR_MHPMCOUNTER5H]  = { "mhpmcounter5h",  mctr32,  read_hpmcounterh,
                             write_mhpmcounterh                         },
    [CSR_MHPMCOUNTER6H]  = { "mhpmcounter6h",  mctr32,  read_hpmcounterh,
                             write_mhpmcounterh                         },
    [CSR_MHPMCOUNTER7H]  = { "mhpmcounter7h",  mctr32,  read_hpmcounterh,
                             write_mhpmcounterh                         },
    [CSR_MHPMCOUNTER8H]  = { "mhpmcounter8h",  mctr32,  read_hpmcounterh,
                             write_mhpmcounterh                         },
    [CSR_MHPMCOUNTER9H]  = { "mhpmcounter9h",  mctr32,  read_hpmcounterh,
                             write_mhpmcounterh                         },
    [CSR_MHPMCOUNTER10H] = { "mhpmcounter10h", mctr32,  read_hpmcounterh,
                             write_mhpmcounterh                         },
    [CSR_MHPMCOUNTER11H] = { "mhpmcounter11h", mctr32,  read_hpmcounterh,
                             write_mhpmcounterh                         },
    [CSR_MHPMCOUNTER12H] = { "mhpmcounter12h", mctr32,  read_hpmcounterh,
                             write_mhpmcounterh                         },
    [CSR_MHPMCOUNTER13H] = { "mhpmcounter13h", mctr32,  read_hpmcounterh,
                             write_mhpmcounterh                         },
    [CSR_MHPMCOUNTER14H] = { "mhpmcounter14h", mctr32,  read_hpmcounterh,
                             write_mhpmcounterh                         },
    [CSR_MHPMCOUNTER15H] = { "mhpmcounter15h", mctr32,  read_hpmcounterh,
                             write_mhpmcounterh                         },
    [CSR_MHPMCOUNTER16H] = { "mhpmcounter16h", mctr32,  read_hpmcounterh,
                             write_mhpmcounterh                         },
    [CSR_MHPMCOUNTER17H] = { "mhpmcounter17h", mctr32,  read_hpmcounterh,
                             write_mhpmcounterh                         },
    [CSR_MHPMCOUNTER18H] = { "mhpmcounter18h", mctr32,  read_hpmcounterh,
                             write_mhpmcounterh                         },
    [CSR_MHPMCOUNTER19H] = { "mhpmcounter19h", mctr32,  read_hpmcounterh,
                             write_mhpmcounterh                         },
    [CSR_MHPMCOUNTER20H] = { "mhpmcounter20h", mctr32,  read_hpmcounterh,
                             write_mhpmcounterh                         },
    [CSR_MHPMCOUNTER21H] = { "mhpmcounter21h", mctr32,  read_hpmcounterh,
                             write_mhpmcounterh                         },
    [CSR_MHPMCOUNTER22H] = { "mhpmcounter22h", mctr32,  read_hpmcounterh,
                             write_mhpmcounterh                         },
    [CSR_MHPMCOUNTER23H] = { "mhpmcounter23h", mctr32,  read_hpmcounterh,
                             write_mhpmcounterh                         },
    [CSR_MHPMCOUNTER24H] = { "mhpmcounter24h", mctr32,  read_hpmcounterh,
                             write_mhpmcounterh                         },
    [CSR_MHPMCOUNTER25H] = { "mhpmcounter25h", mctr32,  read_hpmcounterh,
                             write_mhpmcounterh                         },
    [CSR_MHPMCOUNTER26H] = { "mhpmcounter26h", mctr32,  read_hpmcounterh,
                             write_mhpmcounterh                         },
    [CSR_MHPMCOUNTER27H] = { "mhpmcounter27h", mctr32,  read_hpmcounterh,
                             write_mhpmcounterh                         },
    [CSR_MHPMCOUNTER28H] = { "mhpmcounter28h", mctr32,  read_hpmcounterh,
                             write_mhpmcounterh                         },
    [CSR_MHPMCOUNTER29H] = { "mhpmcounter29h", mctr32,  read_hpmcounterh,
                             write_mhpmcounterh                         },
    [CSR_MHPMCOUNTER30H] = { "mhpmcounter30h", mctr32,  read_hpmcounterh,
                             write_mhpmcounterh                         },
    [CSR_MHPMCOUNTER31H] = { "mhpmcounter31h", mctr32,  read_hpmcounterh,
                             write_mhpmcounterh                         },
    [CSR_SCOUNTOVF]      = { "scountovf", sscofpmf,  read_scountovf,
                             .min_priv_ver = PRIV_VERSION_1_12_0 },

#endif /* !CONFIG_USER_ONLY */
};
