/*
 * xemu RAW Cheat Engine - WHPX guest-debug registration bridge
 *
 * Keep the WHPX debugger implementation inside ui/xui/debug-tools.  The
 * upstream WHPX accelerator files only call the small hooks declared here.
 */
#pragma once

#include "qemu/typedefs.h"

#ifdef __cplusplus
extern "C" {
#endif

void xemu_debug_register_whpx_ops(AccelOpsClass *ops);

/* WHPX data-watchpoint helpers. Watchpoints themselves remain QEMU
 * CPUWatchpoint objects; these helpers mirror them into x86 DR0-DR3. */
int xemu_whpx_watchpoints_active(CPUState *cpu);
int xemu_whpx_sync_watchpoints(CPUState *cpu);

/* Explicit one-instruction debugger Step Into state.  Keep this separate
 * from WHPX's internal execute-breakpoint step-over so a user-requested step
 * can never be mistaken for the invisible resume helper. */
void xemu_whpx_set_user_step_pending(int pending);
int xemu_whpx_user_step_pending(void);
/* Resolve a WHPX #DB/#BP debug exit into the QEMU exception index expected by
 * the existing guest-debug path.  is_execute_breakpoint is true only when the
 * exit instruction byte is WHPX's patched execute-breakpoint opcode. */
int xemu_whpx_debug_exception_index(CPUState *cpu, int is_execute_breakpoint);

/* Reload CR3 through WHPX after debug-tools edits a guest page-table entry.
 * Writing CR3 back to itself invalidates stale guest translations. */
int xemu_whpx_reload_cr3(CPUState *cpu);

/* Read the complete WHPX floating-point/SIMD register state directly from
 * WHP. QEMU's normal WHPX synchronize path intentionally treats FP/MMX
 * register payloads as 64-bit MMX values, so this helper preserves the upper
 * 16 bits required to display architectural 80-bit x87 ST registers. */
int xemu_whpx_get_extra_registers(CPUState *cpu,
                                  uint64_t st_low[8], uint16_t st_high[8],
                                  uint64_t mmx[8], uint32_t xmm[8][4],
                                  uint32_t *fctrl, uint32_t *fstat,
                                  uint32_t *fop, uint32_t *mxcsr,
                                  uint8_t *fp_top);

#ifdef __cplusplus
}
#endif
