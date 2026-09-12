/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 *
 * Contributions after 2012-01-13 are licensed under the terms of the
 * GNU GPL, version 2 or (at your option) any later version.
 */
#ifndef HW_AC97_INT_H
#define HW_AC97_INT_H

#include "qemu/osdep.h"
#include "hw/hw.h"
#include "qemu/audio.h"
#include "hw/pci/pci.h"
#include "system/dma.h"

enum {
    PI_INDEX = 0,   /* PCM in */
    PO_INDEX,       /* PCM out */
    MC_INDEX,       /* Mic in */
    SO_INDEX = 7,   /* SPDIF out */
    LAST_INDEX
};

typedef struct BD {
    uint32_t addr;
    uint32_t ctl_len;
} BD;

typedef struct AC97BusMasterRegs {
    uint32_t bdbar;             /* rw 0, buffer descriptor list base address register */
    uint8_t civ;                /* ro 0, current index value */
    uint8_t lvi;                /* rw 0, last valid index */
    uint16_t sr;                /* rw 1, status register */
    uint16_t picb;              /* ro 0, position in current buffer */
    uint8_t piv;                /* ro 0, prefetched index value */
    uint8_t cr;                 /* rw 0, control register */
    unsigned int bd_valid;
    BD bd;
} AC97BusMasterRegs;

#define TYPE_AC97 "AC97"
OBJECT_DECLARE_SIMPLE_TYPE(AC97LinkState, AC97)

struct AC97LinkState {
    PCIDevice *pci_dev;
    AddressSpace *as;
    AudioBackend *audio_be;
    uint32_t glob_cnt;
    uint32_t glob_sta;
    uint32_t cas; /* Codec Access Semaphore Register */
    uint32_t last_samp;
    AC97BusMasterRegs bm_regs[LAST_INDEX];
    uint8_t mixer_data[256];
    SWVoiceIn *voice_pi;
    SWVoiceOut *voice_po;
    SWVoiceIn *voice_mc;
    int invalid_freq[LAST_INDEX];
#ifdef XBOX
    /* The bus masters are run by the owner's clock (ac97_xbox_run) and
     * deliver what they fetch into a sink rather than a voice. */
    uint8_t *sink;
    int sink_len, sink_pos;
    /* The interrupt line, set through the owner (from any thread). */
    void (*irq_update)(void *opaque, int level);
    void *irq_opaque;
    /* The codec's combined output volume for the owner to apply. */
    int out_mute;
    uint8_t out_vol_l, out_vol_r;
#endif
    uint8_t silence[128];
    int bup_flag;
};

void ac97_common_init (AC97LinkState *s,
                       PCIDevice *pci_dev,
                       AddressSpace *as);

extern const MemoryRegionOps ac97_io_nam_ops;
extern const MemoryRegionOps ac97_io_nabm_ops;

#ifdef XBOX
/* Run bus master `index` for `bytes` of its stream into `out`: what it
 * fetched, and silence where it is not running or halted. */
void ac97_xbox_run(AC97LinkState *s, int index, uint8_t *out, int bytes);
#endif

#endif

