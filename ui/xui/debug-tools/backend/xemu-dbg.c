/*
 * xemu RAW Cheat Engine - target-specific debugger backend
 *
 * The breakpoint/watchpoint path is based on the implementation in Josh's
 * xemu fork: use QEMU's existing CPU breakpoint/watchpoint lists directly
 * instead of maintaining a second debugger mechanism in the UI.
 *
 * This source is deliberately built through specific_ss. It may include the
 * i386 target's cpu.h without exposing those definitions to C++ XUI code.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "qemu/osdep.h"
#include "qemu/accel.h"
#include "accel/accel-cpu-ops.h"
#include "hw/core/cpu.h"
#include "system/hw_accel.h"
#include "system/tcg.h"
#include "system/runstate.h"
#include "system/cpus.h"
#include "cpu.h"
#include "exec/watchpoint.h"
#include "exec/breakpoint.h"
#include "exec/gdbstub.h"
#include "gdbstub/enums.h"
#include "exec/cputlb.h"
#include "exec/tb-flush.h"
#include "qapi/error.h"

#include "xemu-dbg.h"

#ifdef CONFIG_WHPX
#include "system/whpx.h"
#include "whpx-debug.h"
#endif

static CPUState *debug_cpu(void)
{
    return qemu_get_cpu(0);
}

static bool xemu_dbg_whpx_active(void)
{
#ifdef CONFIG_WHPX
    return whpx_enabled();
#else
    return false;
#endif
}

static int xemu_dbg_whpx_reload_cr3_if_active(CPUState *cpu)
{
#ifdef CONFIG_WHPX
    if (whpx_enabled()) {
        return xemu_whpx_reload_cr3(cpu);
    }
#else
    (void)cpu;
#endif
    return 0;
}

static int xemu_dbg_whpx_sync_watchpoints_if_active(CPUState *cpu)
{
#ifdef CONFIG_WHPX
    if (whpx_enabled()) {
        return xemu_whpx_sync_watchpoints(cpu);
    }
#else
    (void)cpu;
#endif
    return 0;
}

static bool gdb_stub_ready;

static bool xemu_dbg_generic_is_kvm(void)
{
    const char *name = current_accel_name();
    return name != NULL && strcmp(name, "kvm") == 0;
}

/*
 * Non-WHPX hardware accelerators should use QEMU's own guest-debug
 * interface instead of reaching into accelerator-private state.  KVM already
 * wires these callbacks to its DR0-DR3 guest-debug machinery;
 * TCG has equivalent callbacks, while x86 HVF currently reports that guest
 * debugging is unsupported.
 *
 * Keep WHPX on the existing xemu-specific path.  That backend has additional
 * read-only filtering and native breakpoint-resume behavior which must not be
 * changed by the cross-platform work.
 */
static const AccelOpsClass *xemu_dbg_generic_guest_debug_ops(void)
{
    const AccelOpsClass *ops = cpus_get_accel();

    if (ops == NULL || ops->supports_guest_debug == NULL ||
        !ops->supports_guest_debug() || ops->update_guest_debug == NULL ||
        ops->insert_breakpoint == NULL || ops->remove_breakpoint == NULL) {
        return NULL;
    }
    return ops;
}

static bool xemu_dbg_pause_for_accel_update(bool *was_running)
{
    *was_running = runstate_is_running();
    return !*was_running || vm_stop(RUN_STATE_PAUSED) == 0;
}

static void xemu_dbg_restore_run_state(bool was_running)
{
    if (was_running) {
        vm_start();
    }
}

int xemu_dbg_guest_debug_supported(void)
{
    /* cpus_get_accel() asserts until vCPU initialization is complete. */
    if (debug_cpu() == NULL) {
        return 0;
    }
    if (tcg_enabled() || xemu_dbg_whpx_active()) {
        return 1;
    }
    return xemu_dbg_generic_guest_debug_ops() != NULL ? 1 : 0;
}

/* Reloading CR3 to itself invalidates stale page-table translations after
 * Type-F edits guest paging structures.  WHPX needs its dedicated register
 * access helper and TCG only needs a software TLB flush.  KVM/HVF can use the
 * synchronized architectural CPU state: synchronize_state() makes that state
 * authoritative/dirty, cpu_x86_update_cr3() performs the architectural
 * reload, and the accelerator pushes the dirty state on the next run. */
static int xemu_dbg_reload_cr3_generic(CPUState *cpu)
{
    const char *name = current_accel_name();
    CPUX86State *env;
    target_ulong cr3;
    bool was_running = false;

    if (name == NULL || (strcmp(name, "kvm") != 0 && strcmp(name, "hvf") != 0)) {
        return 0;
    }
    if (!xemu_dbg_pause_for_accel_update(&was_running)) {
        return 0;
    }

    cpu_synchronize_state(cpu);
    env = &X86_CPU(cpu)->env;
    cr3 = env->cr[3];
    cpu_x86_update_cr3(env, cr3);

    xemu_dbg_restore_run_state(was_running);
    return 1;
}

int xemu_dbg_flush_guest_translation(void)
{
    CPUState *cpu = debug_cpu();

    if (cpu == NULL) {
        return 0;
    }

    if (xemu_dbg_whpx_active()) {
        return xemu_dbg_whpx_reload_cr3_if_active(cpu);
    }

    if (tcg_enabled()) {
        bool was_running = false;

        /* Type-F caves live in xemu-owned RAM and are written directly through
         * the backing MemoryRegion. memory_region_set_dirty() is sufficient for
         * RAM coherency, but it does not invalidate host code that TCG already
         * translated for the old bytes at the same guest virtual address.
         *
         * Keep both sides coherent before the guest can execute again:
         *   - tlb_flush() drops stale guest virtual->physical translations.
         *   - tb_flush__exclusive_or_serial() drops stale translated code.
         *
         * The TB API requires an exclusive/serial context. Most Type-F callers
         * already hold TypeFGuestPauseGuard, but make this helper safe for the
         * other executable-patch callers too by stopping a running VM here and
         * only resuming it when this helper performed the stop. */
        if (!xemu_dbg_pause_for_accel_update(&was_running)) {
            return 0;
        }
        tlb_flush(cpu);
        tb_flush__exclusive_or_serial();
        xemu_dbg_restore_run_state(was_running);
        return 1;
    }

    return xemu_dbg_reload_cr3_generic(cpu);
}

/*
 * BP_GDB breakpoints enter QEMU's normal guest-debug path when they fire.
 * That path calls gdb_set_stop_cpu(), which expects the GDB process table to
 * exist even when no external GDB client is connected.  xemu normally does
 * not start the gdbstub, so initialize its internal state once using the
 * special "none" transport.  This creates no socket/listener and leaves the
 * gdbstub in RS_INACTIVE; it only supplies the process/debug state required by
 * QEMU's built-in BP_GDB handling.
 */
static int ensure_gdb_stub_ready(void)
{
    Error *err = NULL;

    if (gdb_stub_ready) {
        return 1;
    }

    if (!gdbserver_start("none", &err)) {
        fprintf(stderr, "xemu debug-tools: failed to initialize QEMU gdbstub: %s\n",
                err != NULL ? error_get_pretty(err) : "unknown error");
        error_free(err);
        return 0;
    }

    gdb_stub_ready = true;
    return 1;
}

int xemu_dbg_bp_insert(uint32_t address)
{
    CPUState *cpu = debug_cpu();
    const AccelOpsClass *ops;
    bool was_running = false;
    int rc;

    if (cpu == NULL || !ensure_gdb_stub_ready()) {
        return 0;
    }

    /* Preserve the proven v1.67 behavior exactly for WHPX and TCG. */
    if (xemu_dbg_whpx_active() || tcg_enabled()) {
        return cpu_breakpoint_insert(cpu, (vaddr)address, BP_GDB, NULL) == 0;
    }

    /* KVM (and any future accelerator that advertises QEMU guest debug) must
     * install the breakpoint through AccelOps.  Prefer a hardware execute
     * breakpoint for KVM: it preserves the debugger's read-only guarantee by
     * avoiding an INT3 patch in guest memory.  x86 KVM exposes four DR slots
     * shared with data watchpoints. */
    ops = xemu_dbg_generic_guest_debug_ops();
    if (ops == NULL || !xemu_dbg_pause_for_accel_update(&was_running)) {
        return 0;
    }
    rc = ops->insert_breakpoint(cpu,
                                xemu_dbg_generic_is_kvm() ? GDB_BREAKPOINT_HW
                                                          : GDB_BREAKPOINT_SW,
                                (vaddr)address, 1);
    xemu_dbg_restore_run_state(was_running);
    return rc == 0;
}

int xemu_dbg_bp_remove(uint32_t address)
{
    CPUState *cpu = debug_cpu();
    const AccelOpsClass *ops;
    bool was_running = false;
    int rc;

    if (cpu == NULL) {
        return 0;
    }

    if (xemu_dbg_whpx_active() || tcg_enabled()) {
        return cpu_breakpoint_remove(cpu, (vaddr)address, BP_GDB) == 0;
    }

    ops = xemu_dbg_generic_guest_debug_ops();
    if (ops == NULL || !xemu_dbg_pause_for_accel_update(&was_running)) {
        return 0;
    }
    rc = ops->remove_breakpoint(cpu,
                                xemu_dbg_generic_is_kvm() ? GDB_BREAKPOINT_HW
                                                          : GDB_BREAKPOINT_SW,
                                (vaddr)address, 1);
    xemu_dbg_restore_run_state(was_running);
    return rc == 0;
}

int xemu_dbg_get_extra_registers(uint64_t st_low[8], uint16_t st_high[8],
                                 uint64_t mmx[8], uint32_t xmm[8][4],
                                 uint32_t *fctrl, uint32_t *fstat,
                                 uint32_t *fop, uint32_t *mxcsr,
                                 uint8_t *fp_top)
{
    CPUState *cpu = debug_cpu();
    X86CPU *x86_cpu;
    CPUX86State *env;
    int i;

    if (cpu == NULL || st_low == NULL || st_high == NULL || mmx == NULL ||
        xmm == NULL || fctrl == NULL || fstat == NULL || fop == NULL ||
        mxcsr == NULL || fp_top == NULL) {
        return 0;
    }

#ifdef CONFIG_WHPX
    if (xemu_dbg_whpx_active()) {
        return xemu_whpx_get_extra_registers(cpu, st_low, st_high, mmx, xmm,
                                             fctrl, fstat, fop, mxcsr, fp_top);
    }
#endif

    cpu_synchronize_state(cpu);
    x86_cpu = X86_CPU(cpu);
    env = &x86_cpu->env;

    for (i = 0; i < 8; i++) {
        const int st_index = (i + env->fpstt) & 7;
        const floatx80 *fp = &env->fpregs[st_index].d;
        st_low[i] = fp->low;
        st_high[i] = fp->high;
        mmx[i] = env->fpregs[i].mmx.MMX_Q(0);
        xmm[i][0] = env->xmm_regs[i].ZMM_L(0);
        xmm[i][1] = env->xmm_regs[i].ZMM_L(1);
        xmm[i][2] = env->xmm_regs[i].ZMM_L(2);
        xmm[i][3] = env->xmm_regs[i].ZMM_L(3);
    }

    *fctrl = env->fpuc;
    *fstat = (env->fpus & ~0x3800u) | ((env->fpstt & 7u) << 11);
    *fop = env->fpop;
    update_mxcsr_from_sse_status(env);
    *mxcsr = env->mxcsr;
    *fp_top = env->fpstt & 7u;
    return 1;
}

int xemu_dbg_wp_supported(void)
{
    return xemu_dbg_guest_debug_supported();
}

int xemu_dbg_wp_access_supported(int access_flags)
{
    int flags = access_flags & BP_MEM_ACCESS;

    if (flags != BP_MEM_READ && flags != BP_MEM_WRITE &&
        flags != BP_MEM_ACCESS) {
        return 0;
    }
    if (!xemu_dbg_wp_supported()) {
        return 0;
    }

    /* x86 KVM maps hardware data breakpoints onto DR7, whose native types are
     * Write and Access (Read/Write). It has no true Read-only type. An earlier
     * ACCESS+WRITE pairing experiment depended on KVM's private slot order;
     * removing another hardware breakpoint can reorder those slots, so it is
     * not safe enough for debugger semantics. Use TCG when a true Read-only
     * watchpoint is required on Linux. */
    if (xemu_dbg_generic_is_kvm() && flags == BP_MEM_READ) {
        return 0;
    }
    return 1;
}

/* Generic accelerator watchpoint bookkeeping. KVM exposes four x86 debug
 * slots. Split arbitrary ranges into naturally aligned 1/2/4-byte chunks,
 * matching the existing WHPX strategy. */
#define XEMU_ACCEL_WP_MAX_RECORDS 16
#define XEMU_ACCEL_WP_MAX_SLOTS 8

typedef struct XemuAccelWatchSlot {
    vaddr address;
    vaddr length;
    int gdb_type;
} XemuAccelWatchSlot;

typedef struct XemuAccelWatchRecord {
    bool used;
    vaddr address;
    vaddr length;
    int access_flags;
    int slot_count;
    XemuAccelWatchSlot slots[XEMU_ACCEL_WP_MAX_SLOTS];
} XemuAccelWatchRecord;

static XemuAccelWatchRecord xemu_accel_watch_records[XEMU_ACCEL_WP_MAX_RECORDS];

static vaddr xemu_dbg_watch_chunk(vaddr address, vaddr remaining)
{
    if (remaining >= 4 && (address & 3) == 0) {
        return 4;
    }
    if (remaining >= 2 && (address & 1) == 0) {
        return 2;
    }
    return 1;
}

static XemuAccelWatchRecord *xemu_dbg_alloc_watch_record(void)
{
    int i;
    for (i = 0; i < XEMU_ACCEL_WP_MAX_RECORDS; i++) {
        if (!xemu_accel_watch_records[i].used) {
            memset(&xemu_accel_watch_records[i], 0,
                   sizeof(xemu_accel_watch_records[i]));
            return &xemu_accel_watch_records[i];
        }
    }
    return NULL;
}

static XemuAccelWatchRecord *xemu_dbg_find_watch_record(vaddr address,
                                                         vaddr length,
                                                         int access_flags)
{
    int i;
    for (i = 0; i < XEMU_ACCEL_WP_MAX_RECORDS; i++) {
        XemuAccelWatchRecord *record = &xemu_accel_watch_records[i];
        if (record->used && record->address == address &&
            record->length == length &&
            record->access_flags == (access_flags & BP_MEM_ACCESS)) {
            return record;
        }
    }
    return NULL;
}

static int xemu_dbg_record_slot(XemuAccelWatchRecord *record, vaddr address,
                                vaddr length, int gdb_type)
{
    XemuAccelWatchSlot *slot;
    if (record->slot_count >= XEMU_ACCEL_WP_MAX_SLOTS) {
        return -ENOSPC;
    }
    slot = &record->slots[record->slot_count++];
    slot->address = address;
    slot->length = length;
    slot->gdb_type = gdb_type;
    return 0;
}

static void xemu_dbg_remove_record_slots(CPUState *cpu,
                                         const AccelOpsClass *ops,
                                         XemuAccelWatchRecord *record)
{
    int i;
    for (i = record->slot_count - 1; i >= 0; i--) {
        XemuAccelWatchSlot *slot = &record->slots[i];
        ops->remove_breakpoint(cpu, slot->gdb_type,
                               slot->address, slot->length);
    }
    memset(record, 0, sizeof(*record));
}

static int xemu_dbg_insert_generic_watchpoint(CPUState *cpu,
                                               const AccelOpsClass *ops,
                                               uint32_t address,
                                               uint32_t length,
                                               int access_flags)
{
    XemuAccelWatchRecord *record;
    vaddr cursor = address;
    vaddr remaining = length;
    int access = access_flags & BP_MEM_ACCESS;
    int rc = 0;

    if (xemu_dbg_find_watch_record(address, length, access_flags) != NULL) {
        return -EEXIST;
    }

    record = xemu_dbg_alloc_watch_record();
    if (record == NULL) {
        return -ENOSPC;
    }
    record->address = address;
    record->length = length;
    record->access_flags = access;

    while (remaining != 0) {
        vaddr chunk = xemu_dbg_watch_chunk(cursor, remaining);
        int gdb_type = access == BP_MEM_READ ? GDB_WATCHPOINT_READ
                       : access == BP_MEM_WRITE ? GDB_WATCHPOINT_WRITE
                                                : GDB_WATCHPOINT_ACCESS;

        rc = ops->insert_breakpoint(cpu, gdb_type, cursor, chunk);
        if (rc != 0 || xemu_dbg_record_slot(record, cursor, chunk,
                                            gdb_type) != 0) {
            if (rc == 0) {
                ops->remove_breakpoint(cpu, gdb_type, cursor, chunk);
            }
            rc = rc != 0 ? rc : -ENOSPC;
            break;
        }

        cursor += chunk;
        remaining -= chunk;
    }

    if (rc != 0) {
        xemu_dbg_remove_record_slots(cpu, ops, record);
        return rc;
    }

    record->used = true;
    return 0;
}

static int watch_flags(int access_flags)
{
    int flags = access_flags & BP_MEM_ACCESS;

    if (flags == 0) {
        return 0;
    }

    /* Keep Josh's exact watchpoint semantics: BP_GDB plus Read/Write bits.
     * In particular, do not add BP_STOP_BEFORE_ACCESS here. Stopping after
     * the access lets a normal Resume make forward progress without an
     * additional watchpoint step-over state machine. */
    return flags | BP_GDB;
}

int xemu_dbg_wp_insert(uint32_t address, uint32_t length, int access_flags)
{
    CPUState *cpu = debug_cpu();
    const AccelOpsClass *ops;
    bool was_running = false;
    int flags;
    int rc;

    if (cpu == NULL || length == 0 ||
        !xemu_dbg_wp_access_supported(access_flags) ||
        !ensure_gdb_stub_ready()) {
        return 0;
    }

    flags = watch_flags(access_flags);
    if (flags == 0) {
        return 0;
    }

    if (xemu_dbg_whpx_active()) {
        /* WHvSetVirtualProcessorRegisters requires the VP to be stopped.
         * Pause only long enough to mutate/synchronize the hardware slots,
         * then preserve the caller's original run state. */
        was_running = runstate_is_running();
        if (was_running && vm_stop(RUN_STATE_PAUSED) != 0) {
            return 0;
        }
    }

    if (tcg_enabled() || xemu_dbg_whpx_active()) {
        rc = cpu_watchpoint_insert(cpu, (vaddr)address, (vaddr)length,
                                   flags, NULL);
        if (rc == 0 && xemu_dbg_whpx_active()) {
            rc = xemu_dbg_whpx_sync_watchpoints_if_active(cpu);
            if (rc != 0) {
                cpu_watchpoint_remove(cpu, (vaddr)address,
                                      (vaddr)length, flags);
                xemu_dbg_whpx_sync_watchpoints_if_active(cpu);
            }
        }

        if (was_running) {
            vm_start();
        }
        return rc == 0;
    }

    ops = xemu_dbg_generic_guest_debug_ops();
    if (ops == NULL || !xemu_dbg_pause_for_accel_update(&was_running)) {
        return 0;
    }
    rc = xemu_dbg_insert_generic_watchpoint(cpu, ops, address, length,
                                            access_flags);
    xemu_dbg_restore_run_state(was_running);
    return rc == 0;
}

int xemu_dbg_wp_remove(uint32_t address, uint32_t length, int access_flags)
{
    CPUState *cpu = debug_cpu();
    const AccelOpsClass *ops;
    XemuAccelWatchRecord *record;
    bool was_running = false;
    int flags;
    int rc;

    if (cpu == NULL || length == 0 || !xemu_dbg_wp_supported()) {
        return 0;
    }

    flags = watch_flags(access_flags);
    if (flags == 0) {
        return 0;
    }

    if (xemu_dbg_whpx_active()) {
        was_running = runstate_is_running();
        if (was_running && vm_stop(RUN_STATE_PAUSED) != 0) {
            return 0;
        }
    }

    if (tcg_enabled() || xemu_dbg_whpx_active()) {
        rc = cpu_watchpoint_remove(cpu, (vaddr)address, (vaddr)length, flags);
        if (rc == 0 && xemu_dbg_whpx_active()) {
            rc = xemu_dbg_whpx_sync_watchpoints_if_active(cpu);
        }
        if (was_running) {
            vm_start();
        }
        return rc == 0;
    }

    ops = xemu_dbg_generic_guest_debug_ops();
    record = xemu_dbg_find_watch_record(address, length, access_flags);
    if (ops == NULL || record == NULL ||
        !xemu_dbg_pause_for_accel_update(&was_running)) {
        return 0;
    }
    xemu_dbg_remove_record_slots(cpu, ops, record);
    xemu_dbg_restore_run_state(was_running);
    return 1;
}

static XemuAccelWatchRecord *xemu_dbg_find_hit_record(vaddr hit_address,
                                                       int hit_access)
{
    int i;
    int j;

    for (i = 0; i < XEMU_ACCEL_WP_MAX_RECORDS; i++) {
        XemuAccelWatchRecord *record = &xemu_accel_watch_records[i];
        if (!record->used) {
            continue;
        }
        for (j = 0; j < record->slot_count; j++) {
            XemuAccelWatchSlot *slot = &record->slots[j];
            int slot_access = slot->gdb_type == GDB_WATCHPOINT_WRITE
                                  ? BP_MEM_WRITE
                                  : (slot->gdb_type == GDB_WATCHPOINT_READ
                                         ? BP_MEM_READ
                                         : BP_MEM_ACCESS);
            if (slot->address == hit_address && hit_access == slot_access) {
                return record;
            }
        }
    }
    return NULL;
}

int xemu_dbg_wp_get_hit(XemuDbgWatchpointHit *hit)
{
    CPUState *cpu = debug_cpu();
    CPUWatchpoint *wp;
    int access;

    if (cpu == NULL || hit == NULL) {
        return XEMU_DBG_WATCH_HIT_NONE;
    }

    wp = cpu->watchpoint_hit;
    if (wp == NULL) {
        return XEMU_DBG_WATCH_HIT_NONE;
    }

    access = (wp->flags & BP_WATCHPOINT_HIT) >> BP_HIT_SHIFT;
    if (access == 0) {
        access = wp->flags & BP_MEM_ACCESS;
    }

    if (!tcg_enabled() && !xemu_dbg_whpx_active()) {
        XemuAccelWatchRecord *record =
            xemu_dbg_find_hit_record(wp->vaddr, access & BP_MEM_ACCESS);

        if (record != NULL) {
            hit->watch_address = (uint32_t)record->address;
            /* KVM exposes the hardware slot address, not the exact byte inside
             * a multi-byte range. This is still the correct watched address
             * and is the best location available from KVM's debug exit. */
            hit->hit_address = (uint32_t)wp->vaddr;
            hit->length = (uint32_t)record->length;
            hit->access_flags = record->access_flags == BP_MEM_READ
                                    ? BP_MEM_READ
                                    : (access & BP_MEM_ACCESS);
            cpu->watchpoint_hit = NULL;
            return XEMU_DBG_WATCH_HIT_REPORTED;
        }
    }

    /* Existing TCG/WHPX path: these backends preserve the original QEMU
     * CPUWatchpoint object and exact hitaddr semantics. */
    hit->watch_address = (uint32_t)wp->vaddr;
    hit->hit_address = (uint32_t)wp->hitaddr;
    hit->length = (uint32_t)wp->len;
    hit->access_flags = access & BP_MEM_ACCESS;

    wp->flags &= ~BP_WATCHPOINT_HIT;
    cpu->watchpoint_hit = NULL;
    return XEMU_DBG_WATCH_HIT_REPORTED;
}
