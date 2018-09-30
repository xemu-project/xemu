/*
 * QEMU Hypervisor.framework (HVF) support
 *
 * Copyright Google Inc., 2017
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

/* header to be included in HVF-specific code */
#ifndef _HVF_INT_H
#define _HVF_INT_H

#include "sysemu/hvf.h"

#ifdef CONFIG_HVF
#include "target/i386/cpu.h"
#include <Hypervisor/hv.h>
#include <Hypervisor/hv_vmx.h>
#include <Hypervisor/hv_error.h>
#endif

/* hvf_slot flags */
#define HVF_SLOT_LOG (1 << 0)

typedef struct hvf_slot {
    uint64_t start;
    uint64_t size;
    uint8_t *mem;
    int slot_id;
    uint32_t flags;
    MemoryRegion *region;
} hvf_slot;

typedef struct hvf_vcpu_caps {
    uint64_t vmx_cap_pinbased;
    uint64_t vmx_cap_procbased;
    uint64_t vmx_cap_procbased2;
    uint64_t vmx_cap_entry;
    uint64_t vmx_cap_exit;
    uint64_t vmx_cap_preemption_timer;
} hvf_vcpu_caps;

typedef struct HVFState {
    AccelState parent;
    hvf_slot slots[32];
    int num_slots;

    hvf_vcpu_caps *hvf_caps;
} HVFState;
extern HVFState *hvf_state;

void hvf_set_phys_mem(MemoryRegionSection *, bool);
void hvf_handle_io(CPUArchState *, uint16_t, void *,
                  int, int, int);
hvf_slot *hvf_find_overlap_slot(uint64_t, uint64_t);

void _hvf_cpu_synchronize_post_init(CPUState *, run_on_cpu_data);

void vmx_update_tpr(CPUState *);
void update_apic_tpr(CPUState *);
void vmx_clear_int_window_exiting(CPUState *cpu);

#define TYPE_HVF_ACCEL ACCEL_CLASS_NAME("hvf")

#define HVF_STATE(obj) \
    OBJECT_CHECK(HVFState, (obj), TYPE_HVF_ACCEL)

#endif
