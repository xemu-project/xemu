/*
 * xemu Guest Info Interface
 *
 * Copyright (c) 2026 xemu-project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Provides a mechanism for guests to interrogate xemu via IO.
 *
 * The guest should write the key for the information it wants to
 * the XEMU_GUEST_INFO_IO_BASE port, then it can read the 4-byte value from
 * (XEMU_GUEST_INFO_IO_BASE + 1).
 */

#ifndef HW_XBOX_XEMU_GUEST_INFO_H
#define HW_XBOX_XEMU_GUEST_INFO_H

#include "system/memory.h"

#define XEMU_GUEST_INFO_IO_BASE 0x04F4
#define XEMU_GUEST_INFO_IO_SIZE 2

#define XEMU_INFO_KEY_MAGIC 0x00
#define XEMU_INFO_KEY_VERSION_MAJOR 0x01
#define XEMU_INFO_KEY_VERSION_MINOR 0x02
#define XEMU_INFO_KEY_VERSION_PATCH 0x03

#define XEMU_MAGIC_SIGNATURE 0x554D4558 // "XEMU"

extern const MemoryRegionOps xemu_guest_info_ops;

void xemu_guest_info_init(MemoryRegion *parent_io);

#endif
