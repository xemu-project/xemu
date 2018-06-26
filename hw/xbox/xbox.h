/*
 * QEMU Xbox System Emulator
 *
 * Copyright (c) 2013 espes
 * Copyright (c) 2018 Matt Borgerson
 *
 * Based on pc.c
 * Copyright (c) 2003-2004 Fabrice Bellard
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HW_XBOX_H
#define HW_XBOX_H

#include "hw/boards.h"

#define MAX_IDE_BUS 2

void xbox_init_common(MachineState *machine,
                      const uint8_t *eeprom,
                      PCIBus **pci_bus_out,
                      ISABus **isa_bus_out);

#define TYPE_XBOX_MACHINE MACHINE_TYPE_NAME("xbox")

#define XBOX_MACHINE(obj) \
    OBJECT_CHECK(XboxMachineState, (obj), TYPE_XBOX_MACHINE)

#define XBOX_MACHINE_CLASS(klass) \
    OBJECT_CLASS_CHECK(XboxMachineClass, (klass), TYPE_XBOX_MACHINE)

typedef struct XboxMachineState {
    /*< private >*/
    PCMachineState parent_obj;

    /*< public >*/
    char *bootrom;
    bool short_animation;
} XboxMachineState;

typedef struct XboxMachineClass {
    /*< private >*/
    PCMachineClass parent_class;

    /*< public >*/
} XboxMachineClass;

#endif
