/*
 * QEMU Xbox System Emulator
 *
 * Copyright (c) 2013 espes
 * Copyright (c) 2018-2021 Matt Borgerson
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

#ifndef HW_XBOX_H
#define HW_XBOX_H

#include "hw/boards.h"

#define MAX_IDE_BUS 2

void xbox_init_common(MachineState *machine,
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
    char *avpack;
    bool short_animation;
    char *smc_version;
    char *video_encoder;
} XboxMachineState;

typedef struct XboxMachineClass {
    /*< private >*/
    PCMachineClass parent_class;

    /*< public >*/
} XboxMachineClass;

#endif
