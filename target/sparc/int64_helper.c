/*
 * Sparc64 interrupt helpers
 *
 *  Copyright (c) 2003-2005 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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
#include "qemu/main-loop.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "exec/log.h"
#include "trace.h"

#define DEBUG_PCALL

#ifdef DEBUG_PCALL
static const char * const excp_names[0x80] = {
    [TT_TFAULT] = "Instruction Access Fault",
    [TT_TMISS] = "Instruction Access MMU Miss",
    [TT_CODE_ACCESS] = "Instruction Access Error",
    [TT_ILL_INSN] = "Illegal Instruction",
    [TT_PRIV_INSN] = "Privileged Instruction",
    [TT_NFPU_INSN] = "FPU Disabled",
    [TT_FP_EXCP] = "FPU Exception",
    [TT_TOVF] = "Tag Overflow",
    [TT_CLRWIN] = "Clean Windows",
    [TT_DIV_ZERO] = "Division By Zero",
    [TT_DFAULT] = "Data Access Fault",
    [TT_DMISS] = "Data Access MMU Miss",
    [TT_DATA_ACCESS] = "Data Access Error",
    [TT_DPROT] = "Data Protection Error",
    [TT_UNALIGNED] = "Unaligned Memory Access",
    [TT_PRIV_ACT] = "Privileged Action",
    [TT_EXTINT | 0x1] = "External Interrupt 1",
    [TT_EXTINT | 0x2] = "External Interrupt 2",
    [TT_EXTINT | 0x3] = "External Interrupt 3",
    [TT_EXTINT | 0x4] = "External Interrupt 4",
    [TT_EXTINT | 0x5] = "External Interrupt 5",
    [TT_EXTINT | 0x6] = "External Interrupt 6",
    [TT_EXTINT | 0x7] = "External Interrupt 7",
    [TT_EXTINT | 0x8] = "External Interrupt 8",
    [TT_EXTINT | 0x9] = "External Interrupt 9",
    [TT_EXTINT | 0xa] = "External Interrupt 10",
    [TT_EXTINT | 0xb] = "External Interrupt 11",
    [TT_EXTINT | 0xc] = "External Interrupt 12",
    [TT_EXTINT | 0xd] = "External Interrupt 13",
    [TT_EXTINT | 0xe] = "External Interrupt 14",
    [TT_EXTINT | 0xf] = "External Interrupt 15",
};
#endif

void cpu_check_irqs(CPUSPARCState *env)
{
    CPUState *cs;
    uint32_t pil = env->pil_in |
                  (env->softint & ~(SOFTINT_TIMER | SOFTINT_STIMER));

    /* We should be holding the BQL before we mess with IRQs */
    g_assert(bql_locked());

    /* TT_IVEC has a higher priority (16) than TT_EXTINT (31..17) */
    if (env->ivec_status & 0x20) {
        return;
    }
    cs = env_cpu(env);
    /*
     * check if TM or SM in SOFTINT are set
     * setting these also causes interrupt 14
     */
    if (env->softint & (SOFTINT_TIMER | SOFTINT_STIMER)) {
        pil |= 1 << 14;
    }

    /*
     * The bit corresponding to psrpil is (1<< psrpil),
     * the next bit is (2 << psrpil).
     */
    if (pil < (2 << env->psrpil)) {
        if (cs->interrupt_request & CPU_INTERRUPT_HARD) {
            trace_sparc64_cpu_check_irqs_reset_irq(env->interrupt_index);
            env->interrupt_index = 0;
            cpu_reset_interrupt(cs, CPU_INTERRUPT_HARD);
        }
        return;
    }

    if (cpu_interrupts_enabled(env)) {

        unsigned int i;

        for (i = 15; i > env->psrpil; i--) {
            if (pil & (1 << i)) {
                int old_interrupt = env->interrupt_index;
                int new_interrupt = TT_EXTINT | i;

                if (unlikely(env->tl > 0 && cpu_tsptr(env)->tt > new_interrupt
                  && ((cpu_tsptr(env)->tt & 0x1f0) == TT_EXTINT))) {
                    trace_sparc64_cpu_check_irqs_noset_irq(env->tl,
                                                      cpu_tsptr(env)->tt,
                                                      new_interrupt);
                } else if (old_interrupt != new_interrupt) {
                    env->interrupt_index = new_interrupt;
                    trace_sparc64_cpu_check_irqs_set_irq(i, old_interrupt,
                                                         new_interrupt);
                    cpu_interrupt(cs, CPU_INTERRUPT_HARD);
                }
                break;
            }
        }
    } else if (cs->interrupt_request & CPU_INTERRUPT_HARD) {
        trace_sparc64_cpu_check_irqs_disabled(pil, env->pil_in, env->softint,
                                              env->interrupt_index);
        env->interrupt_index = 0;
        cpu_reset_interrupt(cs, CPU_INTERRUPT_HARD);
    }
}

void sparc_cpu_do_interrupt(CPUState *cs)
{
    CPUSPARCState *env = cpu_env(cs);
    int intno = cs->exception_index;
    trap_state *tsptr;

#ifdef DEBUG_PCALL
    if (qemu_loglevel_mask(CPU_LOG_INT)) {
        static int count;
        const char *name;

        if (intno < 0 || intno >= 0x1ff) {
            name = "Unknown";
        } else if (intno >= 0x180) {
            name = "Hyperprivileged Trap Instruction";
        } else if (intno >= 0x100) {
            name = "Trap Instruction";
        } else if (intno >= 0xc0) {
            name = "Window Fill";
        } else if (intno >= 0x80) {
            name = "Window Spill";
        } else {
            name = excp_names[intno];
            if (!name) {
                name = "Unknown";
            }
        }

        qemu_log("%6d: %s (v=%04x)\n", count, name, intno);
        log_cpu_state(cs, 0);
#if 0
        {
            int i;
            uint8_t *ptr;

            qemu_log("       code=");
            ptr = (uint8_t *)env->pc;
            for (i = 0; i < 16; i++) {
                qemu_log(" %02x", ldub(ptr + i));
            }
            qemu_log("\n");
        }
#endif
        count++;
    }
#endif
#if !defined(CONFIG_USER_ONLY)
    if (env->tl >= env->maxtl) {
        cpu_abort(cs, "Trap 0x%04x while trap level (%d) >= MAXTL (%d),"
                  " Error state", cs->exception_index, env->tl, env->maxtl);
        return;
    }
#endif
    if (env->tl < env->maxtl - 1) {
        env->tl++;
    } else {
        env->pstate |= PS_RED;
        if (env->tl < env->maxtl) {
            env->tl++;
        }
    }
    tsptr = cpu_tsptr(env);

    tsptr->tstate = sparc64_tstate(env);
    tsptr->tpc = env->pc;
    tsptr->tnpc = env->npc;
    tsptr->tt = intno;

    if (cpu_has_hypervisor(env)) {
        env->htstate[env->tl] = env->hpstate;
        /* XXX OpenSPARC T1 - UltraSPARC T3 have MAXPTL=2
           but this may change in the future */
        if (env->tl > 2) {
            env->hpstate |= HS_PRIV;
        }
    }

    if (env->def.features & CPU_FEATURE_GL) {
        cpu_gl_switch_gregs(env, env->gl + 1);
        env->gl++;
    }

    switch (intno) {
    case TT_IVEC:
        if (!cpu_has_hypervisor(env)) {
            cpu_change_pstate(env, PS_PEF | PS_PRIV | PS_IG);
        }
        break;
    case TT_TFAULT:
    case TT_DFAULT:
    case TT_TMISS ... TT_TMISS + 3:
    case TT_DMISS ... TT_DMISS + 3:
    case TT_DPROT ... TT_DPROT + 3:
        if (cpu_has_hypervisor(env)) {
            env->hpstate |= HS_PRIV;
            env->pstate = PS_PEF | PS_PRIV;
        } else {
            cpu_change_pstate(env, PS_PEF | PS_PRIV | PS_MG);
        }
        break;
    case TT_INSN_REAL_TRANSLATION_MISS ... TT_DATA_REAL_TRANSLATION_MISS:
    case TT_HTRAP ... TT_HTRAP + 127:
        env->hpstate |= HS_PRIV;
        break;
    default:
        cpu_change_pstate(env, PS_PEF | PS_PRIV | PS_AG);
        break;
    }

    if (intno == TT_CLRWIN) {
        cpu_set_cwp(env, cpu_cwp_dec(env, env->cwp - 1));
    } else if ((intno & 0x1c0) == TT_SPILL) {
        cpu_set_cwp(env, cpu_cwp_dec(env, env->cwp - env->cansave - 2));
    } else if ((intno & 0x1c0) == TT_FILL) {
        cpu_set_cwp(env, cpu_cwp_inc(env, env->cwp + 1));
    }

    if (cpu_hypervisor_mode(env)) {
        env->pc = (env->htba & ~0x3fffULL) | (intno << 5);
    } else {
        env->pc = env->tbr  & ~0x7fffULL;
        env->pc |= ((env->tl > 1) ? 1 << 14 : 0) | (intno << 5);
    }
    env->npc = env->pc + 4;
    cs->exception_index = -1;
}

trap_state *cpu_tsptr(CPUSPARCState* env)
{
    return &env->ts[env->tl & MAXTL_MASK];
}

static bool do_modify_softint(CPUSPARCState *env, uint32_t value)
{
    if (env->softint != value) {
        env->softint = value;
#if !defined(CONFIG_USER_ONLY)
        if (cpu_interrupts_enabled(env)) {
            bql_lock();
            cpu_check_irqs(env);
            bql_unlock();
        }
#endif
        return true;
    }
    return false;
}

void helper_set_softint(CPUSPARCState *env, uint64_t value)
{
    if (do_modify_softint(env, env->softint | (uint32_t)value)) {
        trace_int_helper_set_softint(env->softint);
    }
}

void helper_clear_softint(CPUSPARCState *env, uint64_t value)
{
    if (do_modify_softint(env, env->softint & (uint32_t)~value)) {
        trace_int_helper_clear_softint(env->softint);
    }
}

void helper_write_softint(CPUSPARCState *env, uint64_t value)
{
    if (do_modify_softint(env, (uint32_t)value)) {
        trace_int_helper_write_softint(env->softint);
    }
}
