/*
 * QEMU BIOS e820 routines
 *
 * Copyright (c) 2003-2004 Fabrice Bellard
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef HW_I386_E820_MEMORY_LAYOUT_H
#define HW_I386_E820_MEMORY_LAYOUT_H

/* e820 types */
#define E820_RAM        1
#define E820_RESERVED   2
#define E820_ACPI       3
#define E820_NVS        4
#define E820_UNUSABLE   5

struct e820_entry {
    uint64_t address;
    uint64_t length;
    uint32_t type;
} QEMU_PACKED __attribute((__aligned__(4)));

extern struct e820_entry *e820_table;

int e820_add_entry(uint64_t address, uint64_t length, uint32_t type);
int e820_get_num_entries(void);
bool e820_get_entry(int index, uint32_t type,
                    uint64_t *address, uint64_t *length);



#endif
