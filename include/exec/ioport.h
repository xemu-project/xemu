/*
 * defines ioport related functions
 *
 *  Copyright (c) 2003 Fabrice Bellard
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

/**************************************************************************
 * IO ports API
 */

#ifndef IOPORT_H
#define IOPORT_H

#include "exec/memory.h"

#define MAX_IOPORTS     (64 * 1024)
#define IOPORTS_MASK    (MAX_IOPORTS - 1)

typedef struct MemoryRegionPortio {
    uint32_t offset;
    uint32_t len;
    unsigned size;
    uint32_t (*read)(void *opaque, uint32_t address);
    void (*write)(void *opaque, uint32_t address, uint32_t data);
} MemoryRegionPortio;

#define PORTIO_END_OF_LIST() { }

#ifndef CONFIG_USER_ONLY
extern const MemoryRegionOps unassigned_io_ops;
#endif

void cpu_outb(uint32_t addr, uint8_t val);
void cpu_outw(uint32_t addr, uint16_t val);
void cpu_outl(uint32_t addr, uint32_t val);
uint8_t cpu_inb(uint32_t addr);
uint16_t cpu_inw(uint32_t addr);
uint32_t cpu_inl(uint32_t addr);

typedef struct PortioList {
    const struct MemoryRegionPortio *ports;
    Object *owner;
    struct MemoryRegion *address_space;
    uint32_t addr;
    unsigned nr;
    struct MemoryRegion **regions;
    void *opaque;
    const char *name;
    bool flush_coalesced_mmio;
} PortioList;

void portio_list_init(PortioList *piolist, Object *owner,
                      const struct MemoryRegionPortio *callbacks,
                      void *opaque, const char *name);
void portio_list_set_flush_coalesced(PortioList *piolist);
void portio_list_destroy(PortioList *piolist);
void portio_list_add(PortioList *piolist,
                     struct MemoryRegion *address_space,
                     uint32_t addr);
void portio_list_del(PortioList *piolist);
void portio_list_set_enabled(PortioList *piolist, bool enabled);
void portio_list_set_address(PortioList *piolist, uint32_t addr);

#endif /* IOPORT_H */
