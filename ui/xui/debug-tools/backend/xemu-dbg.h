/*
 * xemu RAW Cheat Engine - target-specific debugger backend
 *
 * Breakpoint/watchpoint design adapted from Josh's xemu debugger backend.
 * Keep this interface plain C so the ImGui C++ frontend never includes
 * target/i386/cpu.h or QEMU target-specific internals directly.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    XEMU_DBG_WATCH_READ = 0x01,
    XEMU_DBG_WATCH_WRITE = 0x02,
    XEMU_DBG_WATCH_ACCESS = XEMU_DBG_WATCH_READ | XEMU_DBG_WATCH_WRITE,
};

enum {
    XEMU_DBG_WATCH_HIT_NONE = 0,
    XEMU_DBG_WATCH_HIT_REPORTED = 1,
};

typedef struct XemuDbgWatchpointHit {
    uint32_t watch_address;
    uint32_t hit_address;
    uint32_t length;
    int access_flags;
} XemuDbgWatchpointHit;

/* Execute breakpoints use guest virtual addresses. */
int xemu_dbg_guest_debug_supported(void);
int xemu_dbg_bp_insert(uint32_t address);
int xemu_dbg_bp_remove(uint32_t address);

/* Snapshot floating-point/SIMD state without exposing CPUX86State to the
 * common C/C++ debugger frontend. */
int xemu_dbg_get_extra_registers(uint64_t st_low[8], uint16_t st_high[8],
                                 uint64_t mmx[8], uint32_t xmm[8][4],
                                 uint32_t *fctrl, uint32_t *fstat,
                                 uint32_t *fop, uint32_t *mxcsr,
                                 uint8_t *fp_top);

/* Data watchpoints use QEMU's native watchpoint list. TCG consumes it
 * directly; WHPX mirrors it into x86 DR0-DR3 hardware watchpoints, and
 * KVM uses QEMU AccelOps guest-debug hardware break/watchpoints. Stock x86
 * KVM supports Write and Read/Write hardware data breakpoints, but not a
 * reliable Read-only hardware breakpoint. */
int xemu_dbg_wp_supported(void);
int xemu_dbg_wp_access_supported(int access_flags);
int xemu_dbg_wp_insert(uint32_t address, uint32_t length, int access_flags);
int xemu_dbg_wp_remove(uint32_t address, uint32_t length, int access_flags);
int xemu_dbg_wp_get_hit(XemuDbgWatchpointHit *hit);

/* Flush guest translations after Type-F edits the Xbox page table.
 * WHPX reloads CR3 through WHP; TCG flushes its software TLB; KVM/HVF
 * synchronize architectural state and reload CR3 generically. */
int xemu_dbg_flush_guest_translation(void);

#ifdef __cplusplus
}
#endif
