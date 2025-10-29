/*
 * QEMU RISCV Host Target Interface (HTIF) Emulation
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

#ifndef HW_RISCV_HTIF_H
#define HW_RISCV_HTIF_H

#include "chardev/char.h"
#include "chardev/char-fe.h"
#include "exec/memory.h"

#define TYPE_HTIF_UART "riscv.htif.uart"

typedef struct HTIFState {
    int allow_tohost;
    int fromhost_inprogress;

    uint64_t tohost;
    uint64_t fromhost;
    hwaddr tohost_offset;
    hwaddr fromhost_offset;
    MemoryRegion mmio;

    CharBackend chr;
    uint64_t pending_read;
} HTIFState;

extern const char *sig_file;
extern uint8_t line_size;

/* HTIF symbol callback */
void htif_symbol_callback(const char *st_name, int st_info, uint64_t st_value,
    uint64_t st_size);

/* legacy pre qom */
HTIFState *htif_mm_init(MemoryRegion *address_space, Chardev *chr,
                        uint64_t nonelf_base, bool custom_base);

#endif
