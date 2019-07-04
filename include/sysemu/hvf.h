/*
 * QEMU Hypervisor.framework (HVF) support
 *
 * Copyright Google Inc., 2017
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

/* header to be included in non-HVF-specific code */
#ifndef _HVF_H
#define _HVF_H

#include "qemu-common.h"
#include "qemu/bitops.h"
#include "exec/memory.h"
#include "sysemu/accel.h"

extern bool hvf_allowed;
#ifdef CONFIG_HVF
uint32_t hvf_get_supported_cpuid(uint32_t func, uint32_t idx,
                                 int reg);
#define hvf_enabled() (hvf_allowed)
#else
#define hvf_enabled() 0
#define hvf_get_supported_cpuid(func, idx, reg) 0
#endif

/* Disable HVF if |disable| is 1, otherwise, enable it iff it is supported by
 * the host CPU. Use hvf_enabled() after this to get the result. */
void hvf_disable(int disable);

/* Returns non-0 if the host CPU supports the VMX "unrestricted guest" feature
 * which allows the virtual CPU to directly run in "real mode". If true, this
 * allows QEMU to run several vCPU threads in parallel (see cpus.c). Otherwise,
 * only a a single TCG thread can run, and it will call HVF to run the current
 * instructions, except in case of "real mode" (paging disabled, typically at
 * boot time), or MMIO operations. */

int hvf_sync_vcpus(void);

int hvf_init_vcpu(CPUState *);
int hvf_vcpu_exec(CPUState *);
int hvf_smp_cpu_exec(CPUState *);
void hvf_cpu_synchronize_state(CPUState *);
void hvf_cpu_synchronize_post_reset(CPUState *);
void hvf_cpu_synchronize_post_init(CPUState *);

void hvf_vcpu_destroy(CPUState *);
void hvf_reset_vcpu(CPUState *);
int hvf_put_registers(CPUState *);

#endif
