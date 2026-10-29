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

#include "qemu/osdep.h"
#include "hw/xbox/xemu_guest_info.h"
#include "xemu-version.h"

typedef struct XemuGuestInfoState {
    MemoryRegion io;
    uint8_t selected_index;
} XemuGuestInfoState;

static uint64_t xemu_guest_info_read(void *opaque, hwaddr addr,
                                     unsigned int size)
{
    XemuGuestInfoState *s = opaque;

    if (addr == 0) {
        return s->selected_index;
    }

    if (addr == 1) {
        switch (s->selected_index) {
        case XEMU_INFO_KEY_MAGIC:
            return XEMU_MAGIC_SIGNATURE;

        case XEMU_INFO_KEY_VERSION_MAJOR:
            return xemu_version_major;

        case XEMU_INFO_KEY_VERSION_MINOR:
            return xemu_version_minor;

        case XEMU_INFO_KEY_VERSION_PATCH:
            return xemu_version_patch;

        default:
            return 0x00;
        }
    }

    return 0x00;
}

static void xemu_guest_info_write(void *opaque, hwaddr addr, uint64_t val,
                                  unsigned int size)
{
    XemuGuestInfoState *s = opaque;

    if (addr == 0) {
        s->selected_index = val & 0xFF;
    }
    /* Offset 1 (Data Port) is read-only; writes are ignored */
}

const MemoryRegionOps xemu_guest_info_ops = {
    .read = xemu_guest_info_read,
    .write = xemu_guest_info_write,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4,
        .unaligned = true,
    },
    .impl = {
        .min_access_size = 1,
        .max_access_size = 4,
        .unaligned = true,
    },
    .endianness = DEVICE_LITTLE_ENDIAN,
};

void xemu_guest_info_init(MemoryRegion *parent_io)
{
    XemuGuestInfoState *s = g_new0(XemuGuestInfoState, 1);

    memory_region_init_io(&s->io, NULL, &xemu_guest_info_ops, s,
                          "xemu-guest-info", XEMU_GUEST_INFO_IO_SIZE);
    memory_region_add_subregion(parent_io, XEMU_GUEST_INFO_IO_BASE, &s->io);
}
