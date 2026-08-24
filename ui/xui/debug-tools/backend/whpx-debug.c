/*
 * xemu RAW Cheat Engine - WHPX guest-debug backend
 *
 * Execute breakpoints are handled by xemu's existing WHPX INT1 patching
 * path. Data watchpoints are mirrored into the x86 hardware debug registers
 * DR0-DR3 so Read/Write debugging also works while the Windows WHPX
 * accelerator is active.
 *
 * x86 provides hardware data conditions for Write and Read/Write, but no
 * Read-only condition. A Read-only watchpoint therefore uses a pair of slots:
 *   - one Read/Write slot
 *   - one Write slot at the same address
 * DR6 tells us which slots fired. A read only trips the first slot; a write
 * trips both and is filtered without stopping the debugger.
 */

#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "qemu/atomic.h"
#include "accel/accel-cpu-ops.h"
#include "hw/core/cpu.h"
#include "cpu.h"
#include "exec/breakpoint.h"
#include "exec/watchpoint.h"
#include "gdbstub/enums.h"
#include "../../../../target/i386/whpx/whpx-internal.h"

#include "whpx-debug.h"

#define XEMU_WHPX_HW_SLOTS 4
#define XEMU_WHPX_DR7_BASE 0x00000600ULL
#define XEMU_WHPX_DR6_HIT_MASK 0x0FULL

/* Use the architectural WHP register numbers directly. This keeps the source
 * compatible with older MinGW Windows SDK headers that expose WHPX but omit
 * some of the newer enum spellings. Microsoft documents DR0..DR7 as
 * 0x21..0x26 in WHV_REGISTER_NAME. */
static const WHV_REGISTER_NAME xemu_whpx_debug_reg_names[6] = {
    (WHV_REGISTER_NAME)0x00000021, /* DR0 */
    (WHV_REGISTER_NAME)0x00000022, /* DR1 */
    (WHV_REGISTER_NAME)0x00000023, /* DR2 */
    (WHV_REGISTER_NAME)0x00000024, /* DR3 */
    (WHV_REGISTER_NAME)0x00000025, /* DR6 */
    (WHV_REGISTER_NAME)0x00000026, /* DR7 */
};

typedef enum XemuWhpxDebugEvent {
    XEMU_WHPX_DEBUG_EVENT_NONE = 0,
    XEMU_WHPX_DEBUG_EVENT_WATCHPOINT,
    XEMU_WHPX_DEBUG_EVENT_FILTERED,
} XemuWhpxDebugEvent;

typedef enum XemuWhpxSlotRole {
    XEMU_WHPX_SLOT_UNUSED = 0,
    XEMU_WHPX_SLOT_WRITE,
    XEMU_WHPX_SLOT_ACCESS,
    XEMU_WHPX_SLOT_READ_PRIMARY,
    XEMU_WHPX_SLOT_READ_WRITE_FILTER,
} XemuWhpxSlotRole;

typedef struct XemuWhpxWatchSlot {
    CPUWatchpoint *watchpoint;
    vaddr address;
    vaddr length;
    XemuWhpxSlotRole role;
    int pair_slot;
} XemuWhpxWatchSlot;

typedef struct XemuWhpxWatchState {
    XemuWhpxWatchSlot slots[XEMU_WHPX_HW_SLOTS];
    int used_slots;
    bool saved_regs_valid;
    WHV_REGISTER_VALUE saved_regs[6];
} XemuWhpxWatchState;

/* Xbox is single-vCPU in xemu, so one WHPX hardware-watch state is enough. */
static XemuWhpxWatchState xemu_whpx_watch_state;

/* One-shot Step Into request owned by the embedded debugger.  WHPX already
 * has a separate exclusive-step path used only to resume through a patched
 * execute breakpoint.  Keeping this bit independent prevents those two
 * purposes from being conflated. */
static int xemu_whpx_user_step_requested;

void xemu_whpx_set_user_step_pending(int pending)
{
    qatomic_set(&xemu_whpx_user_step_requested, pending ? 1 : 0);
}

int xemu_whpx_user_step_pending(void)
{
    return qatomic_read(&xemu_whpx_user_step_requested) != 0;
}

static int xemu_whpx_consume_user_step(void)
{
    return qatomic_xchg(&xemu_whpx_user_step_requested, 0) != 0;
}

typedef struct XemuWhpxCr3ReloadResult {
    int ok;
} XemuWhpxCr3ReloadResult;

static void xemu_whpx_do_reload_cr3(CPUState *cpu, run_on_cpu_data data)
{
    XemuWhpxCr3ReloadResult *result = data.host_ptr;
    WHV_REGISTER_NAME name = WHvX64RegisterCr3;
    WHV_REGISTER_VALUE value;
    HRESULT hr;

    hr = whp_dispatch.WHvGetVirtualProcessorRegisters(
        whpx_global.partition, cpu->cpu_index, &name, 1, &value);
    if (FAILED(hr)) {
        error_report("WHPX debug-tools: failed to read CR3 for TLB reload, hr=%08lx",
                     hr);
        return;
    }

    hr = whp_dispatch.WHvSetVirtualProcessorRegisters(
        whpx_global.partition, cpu->cpu_index, &name, 1, &value);
    if (FAILED(hr)) {
        error_report("WHPX debug-tools: failed to reload CR3 for TLB flush, hr=%08lx",
                     hr);
        return;
    }
    result->ok = 1;
}

int xemu_whpx_reload_cr3(CPUState *cpu)
{
    XemuWhpxCr3ReloadResult result = { 0 };

    if (cpu == NULL) {
        return 0;
    }

    /* WHP register access must happen in the vCPU context, not directly from
     * the UI thread while WHPX may be executing the processor. */
    run_on_cpu(cpu, xemu_whpx_do_reload_cr3, RUN_ON_CPU_HOST_PTR(&result));
    return result.ok;
}

typedef struct XemuWhpxExtraRegisterResult {
    int ok;
    uint64_t st_low[8];
    uint16_t st_high[8];
    uint64_t mmx[8];
    uint32_t xmm[8][4];
    uint32_t fctrl;
    uint32_t fstat;
    uint32_t fop;
    uint32_t mxcsr;
    uint8_t fp_top;
} XemuWhpxExtraRegisterResult;

static void xemu_whpx_do_get_extra_registers(CPUState *cpu,
                                               run_on_cpu_data data)
{
    XemuWhpxExtraRegisterResult *result = data.host_ptr;
    WHV_REGISTER_NAME names[18];
    WHV_REGISTER_VALUE values[18];
    HRESULT hr;
    int i;

    for (i = 0; i < 8; i++) {
        names[i] = (WHV_REGISTER_NAME)(WHvX64RegisterFpMmx0 + i);
    }
    names[8] = WHvX64RegisterFpControlStatus;
    for (i = 0; i < 8; i++) {
        names[9 + i] = (WHV_REGISTER_NAME)(WHvX64RegisterXmm0 + i);
    }
    names[17] = WHvX64RegisterXmmControlStatus;

    hr = whp_dispatch.WHvGetVirtualProcessorRegisters(
        whpx_global.partition, cpu->cpu_index, names, 18, values);
    if (FAILED(hr)) {
        error_report("WHPX debug-tools: failed to read FP/SIMD registers, hr=%08lx",
                     hr);
        return;
    }

    result->fctrl = values[8].FpControlStatus.FpControl;
    result->fstat = values[8].FpControlStatus.FpStatus;
    result->fop = values[8].FpControlStatus.LastFpOp;
    result->fp_top = (result->fstat >> 11) & 7u;
    result->mxcsr = values[17].XmmControlStatus.XmmStatusControl;

    for (i = 0; i < 8; i++) {
        const int st_index = (i + result->fp_top) & 7;
        const uint64_t xmm_lo = values[9 + i].Reg128.Low64;
        const uint64_t xmm_hi = values[9 + i].Reg128.High64;

        result->st_low[i] = values[st_index].Fp.AsUINT128.Low64;
        result->st_high[i] =
            (uint16_t)(values[st_index].Fp.AsUINT128.High64 & 0xFFFFu);
        result->mmx[i] = values[i].Fp.AsUINT128.Low64;
        result->xmm[i][0] = (uint32_t)xmm_lo;
        result->xmm[i][1] = (uint32_t)(xmm_lo >> 32);
        result->xmm[i][2] = (uint32_t)xmm_hi;
        result->xmm[i][3] = (uint32_t)(xmm_hi >> 32);
    }
    result->ok = 1;
}

int xemu_whpx_get_extra_registers(CPUState *cpu,
                                  uint64_t st_low[8], uint16_t st_high[8],
                                  uint64_t mmx[8], uint32_t xmm[8][4],
                                  uint32_t *fctrl, uint32_t *fstat,
                                  uint32_t *fop, uint32_t *mxcsr,
                                  uint8_t *fp_top)
{
    XemuWhpxExtraRegisterResult result = { 0 };

    if (cpu == NULL || st_low == NULL || st_high == NULL || mmx == NULL ||
        xmm == NULL || fctrl == NULL || fstat == NULL || fop == NULL ||
        mxcsr == NULL || fp_top == NULL) {
        return 0;
    }

    run_on_cpu(cpu, xemu_whpx_do_get_extra_registers,
               RUN_ON_CPU_HOST_PTR(&result));
    if (!result.ok) {
        return 0;
    }

    memcpy(st_low, result.st_low, sizeof(result.st_low));
    memcpy(st_high, result.st_high, sizeof(result.st_high));
    memcpy(mmx, result.mmx, sizeof(result.mmx));
    memcpy(xmm, result.xmm, sizeof(result.xmm));
    *fctrl = result.fctrl;
    *fstat = result.fstat;
    *fop = result.fop;
    *mxcsr = result.mxcsr;
    *fp_top = result.fp_top;
    return 1;
}

static bool xemu_whpx_supports_guest_debug(void)
{
    return true;
}

static int xemu_whpx_len_code(vaddr length)
{
    switch (length) {
    case 1:
        return 0x0;
    case 2:
        return 0x1;
    case 4:
        return 0x3;
    default:
        return -1;
    }
}

static int xemu_whpx_add_slot(XemuWhpxWatchSlot *slots, int *used,
                              CPUWatchpoint *wp, vaddr address, vaddr length,
                              XemuWhpxSlotRole role, int pair_slot)
{
    if (*used >= XEMU_WHPX_HW_SLOTS || xemu_whpx_len_code(length) < 0) {
        return -ENOSPC;
    }

    slots[*used].watchpoint = wp;
    slots[*used].address = address;
    slots[*used].length = length;
    slots[*used].role = role;
    slots[*used].pair_slot = pair_slot;
    return (*used)++;
}

/* Split an arbitrary watched range into naturally aligned 1/2/4-byte x86
 * hardware ranges. Read-only ranges consume twice as many slots. */
static int xemu_whpx_add_watchpoint_slots(XemuWhpxWatchSlot *slots, int *used,
                                          CPUWatchpoint *wp)
{
    vaddr address = wp->vaddr;
    vaddr remaining = wp->len;
    int access = wp->flags & BP_MEM_ACCESS;

    if (access != BP_MEM_READ && access != BP_MEM_WRITE &&
        access != BP_MEM_ACCESS) {
        return -EINVAL;
    }

    while (remaining != 0) {
        vaddr chunk;

        if (remaining >= 4 && (address & 3) == 0) {
            chunk = 4;
        } else if (remaining >= 2 && (address & 1) == 0) {
            chunk = 2;
        } else {
            chunk = 1;
        }

        if (access == BP_MEM_READ) {
            int primary;
            int filter;

            primary = xemu_whpx_add_slot(slots, used, wp, address, chunk,
                                         XEMU_WHPX_SLOT_READ_PRIMARY, -1);
            if (primary < 0) {
                return primary;
            }
            filter = xemu_whpx_add_slot(slots, used, wp, address, chunk,
                                        XEMU_WHPX_SLOT_READ_WRITE_FILTER,
                                        primary);
            if (filter < 0) {
                return filter;
            }
            slots[primary].pair_slot = filter;
        } else if (access == BP_MEM_WRITE) {
            if (xemu_whpx_add_slot(slots, used, wp, address, chunk,
                                   XEMU_WHPX_SLOT_WRITE, -1) < 0) {
                return -ENOSPC;
            }
        } else {
            if (xemu_whpx_add_slot(slots, used, wp, address, chunk,
                                   XEMU_WHPX_SLOT_ACCESS, -1) < 0) {
                return -ENOSPC;
            }
        }

        address += chunk;
        remaining -= chunk;
    }

    return 0;
}

static int xemu_whpx_build_slots(CPUState *cpu, XemuWhpxWatchSlot *slots,
                                 int *used)
{
    CPUWatchpoint *wp;

    memset(slots, 0, sizeof(XemuWhpxWatchSlot) * XEMU_WHPX_HW_SLOTS);
    *used = 0;

    QTAILQ_FOREACH(wp, &cpu->watchpoints, entry) {
        int rc;

        if (!(wp->flags & BP_GDB)) {
            continue;
        }

        rc = xemu_whpx_add_watchpoint_slots(slots, used, wp);
        if (rc != 0) {
            return rc;
        }
    }

    return 0;
}

int xemu_whpx_watchpoints_active(CPUState *cpu)
{
    CPUWatchpoint *wp;

    if (cpu == NULL) {
        return 0;
    }

    QTAILQ_FOREACH(wp, &cpu->watchpoints, entry) {
        if ((wp->flags & BP_GDB) && (wp->flags & BP_MEM_ACCESS)) {
            return 1;
        }
    }
    return 0;
}

static HRESULT xemu_whpx_get_debug_regs(CPUState *cpu,
                                        WHV_REGISTER_VALUE values[6])
{
    return whp_dispatch.WHvGetVirtualProcessorRegisters(
        whpx_global.partition, cpu->cpu_index,
        xemu_whpx_debug_reg_names, 6, values);
}

static HRESULT xemu_whpx_set_debug_regs(CPUState *cpu,
                                        const WHV_REGISTER_VALUE values[6])
{
    return whp_dispatch.WHvSetVirtualProcessorRegisters(
        whpx_global.partition, cpu->cpu_index,
        xemu_whpx_debug_reg_names, 6, values);
}

int xemu_whpx_sync_watchpoints(CPUState *cpu)
{
    XemuWhpxWatchSlot new_slots[XEMU_WHPX_HW_SLOTS];
    WHV_REGISTER_VALUE values[6];
    WHV_REGISTER_VALUE current_regs[6];
    uint64_t dr7 = XEMU_WHPX_DR7_BASE;
    int used = 0;
    int rc;
    int i;
    HRESULT hr;

    if (cpu == NULL) {
        return -EINVAL;
    }

    rc = xemu_whpx_build_slots(cpu, new_slots, &used);
    if (rc != 0) {
        return rc;
    }

    if (used == 0) {
        if (xemu_whpx_watch_state.saved_regs_valid) {
            hr = xemu_whpx_set_debug_regs(cpu,
                                          xemu_whpx_watch_state.saved_regs);
            if (FAILED(hr)) {
                error_report("WHPX debug-tools: failed to restore DR registers, hr=%08lx",
                             hr);
                return -EIO;
            }
            xemu_whpx_watch_state.saved_regs_valid = false;
        }
        memset(xemu_whpx_watch_state.slots, 0,
               sizeof(xemu_whpx_watch_state.slots));
        xemu_whpx_watch_state.used_slots = 0;
        return 0;
    }

    hr = xemu_whpx_get_debug_regs(cpu, current_regs);
    if (FAILED(hr)) {
        error_report("WHPX debug-tools: failed to read DR registers, hr=%08lx",
                     hr);
        return -EIO;
    }

    if (!xemu_whpx_watch_state.saved_regs_valid) {
        memcpy(xemu_whpx_watch_state.saved_regs, current_regs,
               sizeof(current_regs));
        xemu_whpx_watch_state.saved_regs_valid = true;
    }

    memset(values, 0, sizeof(values));

    for (i = 0; i < used; i++) {
        int type_code;
        int len_code = xemu_whpx_len_code(new_slots[i].length);

        values[i].Reg64 = new_slots[i].address;

        switch (new_slots[i].role) {
        case XEMU_WHPX_SLOT_WRITE:
        case XEMU_WHPX_SLOT_READ_WRITE_FILTER:
            type_code = 0x1; /* data writes */
            break;
        case XEMU_WHPX_SLOT_ACCESS:
        case XEMU_WHPX_SLOT_READ_PRIMARY:
            type_code = 0x3; /* data reads or writes */
            break;
        default:
            return -EINVAL;
        }

        /* Use global enable bits, matching QEMU/KVM's x86 hardware-debug
         * programming. Each slot has RW/LEN fields four bits apart. */
        dr7 |= (2ULL << (i * 2));
        dr7 |= ((uint64_t)type_code << (16 + i * 4));
        dr7 |= ((uint64_t)len_code << (18 + i * 4));
    }

    /* Preserve architectural DR6 reserved/fixed state but clear old B0-B3
     * hit flags. */
    values[4] = current_regs[4];
    values[4].Reg64 &= ~XEMU_WHPX_DR6_HIT_MASK;
    values[5].Reg64 = dr7;

    hr = xemu_whpx_set_debug_regs(cpu, values);
    if (FAILED(hr)) {
        error_report("WHPX debug-tools: failed to program hardware watchpoints, hr=%08lx",
                     hr);
        return -EIO;
    }

    memcpy(xemu_whpx_watch_state.slots, new_slots, sizeof(new_slots));
    xemu_whpx_watch_state.used_slots = used;
    return 0;
}

static void xemu_whpx_mark_watch_hit(CPUState *cpu, XemuWhpxWatchSlot *slot,
                                     int access)
{
    CPUWatchpoint *wp = slot->watchpoint;

    if (wp == NULL) {
        return;
    }

    wp->hitaddr = slot->address;
    wp->flags &= ~BP_WATCHPOINT_HIT;
    if (access == BP_MEM_READ) {
        wp->flags |= BP_WATCHPOINT_HIT_READ;
    } else if (access == BP_MEM_WRITE) {
        wp->flags |= BP_WATCHPOINT_HIT_WRITE;
    } else {
        wp->flags |= BP_WATCHPOINT_HIT;
    }
    cpu->watchpoint_hit = wp;
}

static int xemu_whpx_handle_debug_exception(CPUState *cpu)
{
    WHV_REGISTER_VALUE dr6_value;
    WHV_REGISTER_VALUE clear_value;
    WHV_REGISTER_NAME dr6_name = (WHV_REGISTER_NAME)0x00000025;
    uint64_t hits;
    int i;
    bool filtered = false;
    HRESULT hr;

    if (cpu == NULL || xemu_whpx_watch_state.used_slots == 0) {
        return XEMU_WHPX_DEBUG_EVENT_NONE;
    }

    hr = whp_dispatch.WHvGetVirtualProcessorRegisters(
        whpx_global.partition, cpu->cpu_index,
        &dr6_name, 1, &dr6_value);
    if (FAILED(hr)) {
        return XEMU_WHPX_DEBUG_EVENT_NONE;
    }

    hits = dr6_value.Reg64 & XEMU_WHPX_DR6_HIT_MASK;
    if (hits == 0) {
        return XEMU_WHPX_DEBUG_EVENT_NONE;
    }

    /* Clear only B0-B3. Preserve the other guest/debug status bits. */
    clear_value = dr6_value;
    clear_value.Reg64 &= ~XEMU_WHPX_DR6_HIT_MASK;
    whp_dispatch.WHvSetVirtualProcessorRegisters(
        whpx_global.partition, cpu->cpu_index,
        &dr6_name, 1, &clear_value);

    /* First handle real user-visible hits. */
    for (i = 0; i < xemu_whpx_watch_state.used_slots; i++) {
        XemuWhpxWatchSlot *slot = &xemu_whpx_watch_state.slots[i];

        if (!(hits & (1ULL << i)) || slot->watchpoint == NULL) {
            continue;
        }

        switch (slot->role) {
        case XEMU_WHPX_SLOT_WRITE:
            xemu_whpx_mark_watch_hit(cpu, slot, BP_MEM_WRITE);
            return XEMU_WHPX_DEBUG_EVENT_WATCHPOINT;

        case XEMU_WHPX_SLOT_ACCESS:
            xemu_whpx_mark_watch_hit(cpu, slot, BP_MEM_ACCESS);
            return XEMU_WHPX_DEBUG_EVENT_WATCHPOINT;

        case XEMU_WHPX_SLOT_READ_PRIMARY:
            if (slot->pair_slot >= 0 &&
                (hits & (1ULL << slot->pair_slot))) {
                /* Both the R/W and Write-only slots fired: this was a write,
                 * so suppress it for a Read-only user watchpoint. */
                filtered = true;
            } else {
                xemu_whpx_mark_watch_hit(cpu, slot, BP_MEM_READ);
                return XEMU_WHPX_DEBUG_EVENT_WATCHPOINT;
            }
            break;

        case XEMU_WHPX_SLOT_READ_WRITE_FILTER:
            filtered = true;
            break;

        default:
            break;
        }
    }

    return filtered ? XEMU_WHPX_DEBUG_EVENT_FILTERED
                    : XEMU_WHPX_DEBUG_EVENT_NONE;
}

int xemu_whpx_debug_exception_index(CPUState *cpu, int is_execute_breakpoint)
{
    int debug_event;
    int user_step_complete;

    if (is_execute_breakpoint) {
        /* If a user Step Into lands directly on another execute breakpoint,
         * that breakpoint is also the completion point for the requested
         * one-shot. */
        xemu_whpx_consume_user_step();
        return EXCP_DEBUG;
    }

    debug_event = xemu_whpx_handle_debug_exception(cpu);
    user_step_complete = xemu_whpx_consume_user_step();

    if (debug_event == XEMU_WHPX_DEBUG_EVENT_WATCHPOINT) {
        /* DR0-DR3 data watchpoint owned by debug-tools. */
        return EXCP_DEBUG;
    }
    if (user_step_complete) {
        /* A debugger Step Into is user-visible even if the same #DB also
         * matched an internal Read-only filter slot. Exactly one guest
         * instruction has completed, so stop. */
        return EXCP_DEBUG;
    }
    if (debug_event == XEMU_WHPX_DEBUG_EVENT_FILTERED) {
        /* Internal paired-slot hit used to implement Read-only. The access
         * was a write, so continue without exposing a debugger stop. */
        return EXCP_INTERRUPT;
    }
    if (!cpu->singlestep_enabled) {
        /* Just finished stepping over an execute breakpoint, but GDB does not
         * expect us to single-step. */
        return EXCP_INTERRUPT;
    }
    return EXCP_DEBUG;
}

static int xemu_whpx_gdb_watch_flags(int type)
{
    switch (type) {
    case GDB_WATCHPOINT_READ:
        return BP_MEM_READ;
    case GDB_WATCHPOINT_WRITE:
        return BP_MEM_WRITE;
    case GDB_WATCHPOINT_ACCESS:
        return BP_MEM_ACCESS;
    default:
        return 0;
    }
}

static int xemu_whpx_insert_breakpoint(CPUState *cpu, int type, vaddr addr,
                                       vaddr len)
{
    int flags;
    int rc;

    if (type == GDB_BREAKPOINT_SW || type == GDB_BREAKPOINT_HW) {
        return cpu_breakpoint_insert(cpu, addr, BP_GDB, NULL);
    }

    flags = xemu_whpx_gdb_watch_flags(type);
    if (flags == 0) {
        return -ENOSYS;
    }

    rc = cpu_watchpoint_insert(cpu, addr, len, flags | BP_GDB, NULL);
    if (rc != 0) {
        return rc;
    }

    rc = xemu_whpx_sync_watchpoints(cpu);
    if (rc != 0) {
        cpu_watchpoint_remove(cpu, addr, len, flags | BP_GDB);
        xemu_whpx_sync_watchpoints(cpu);
    }
    return rc;
}

static int xemu_whpx_remove_breakpoint(CPUState *cpu, int type, vaddr addr,
                                       vaddr len)
{
    int flags;
    int rc;

    if (type == GDB_BREAKPOINT_SW || type == GDB_BREAKPOINT_HW) {
        return cpu_breakpoint_remove(cpu, addr, BP_GDB);
    }

    flags = xemu_whpx_gdb_watch_flags(type);
    if (flags == 0) {
        return -ENOSYS;
    }

    rc = cpu_watchpoint_remove(cpu, addr, len, flags | BP_GDB);
    if (rc == 0) {
        rc = xemu_whpx_sync_watchpoints(cpu);
    }
    return rc;
}

static void xemu_whpx_remove_all_breakpoints(CPUState *cpu)
{
    cpu_breakpoint_remove_all(cpu, BP_GDB);
    cpu_watchpoint_remove_all(cpu, BP_GDB);
    xemu_whpx_sync_watchpoints(cpu);
}

void xemu_debug_register_whpx_ops(AccelOpsClass *ops)
{
    if (ops == NULL) {
        return;
    }

    ops->supports_guest_debug = xemu_whpx_supports_guest_debug;
    ops->insert_breakpoint = xemu_whpx_insert_breakpoint;
    ops->remove_breakpoint = xemu_whpx_remove_breakpoint;
    ops->remove_all_breakpoints = xemu_whpx_remove_all_breakpoints;
}
