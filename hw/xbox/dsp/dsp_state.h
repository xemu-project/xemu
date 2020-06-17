/*
 * MCPX DSP emulator

 * Copyright (c) 2015 espes

 * Adapted from Hatari DSP M56001 emulation
 * (C) 2001-2008 ARAnyM developer team
 * Adaption to Hatari (C) 2008 by Thomas Huth

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef DSP_STATE_H
#define DSP_STATE_H

#include "dsp_cpu.h"
#include "dsp_dma.h"

struct DSPState {
    dsp_core_t core;
    DSPDMAState dma;
    int save_cycles;

    uint32_t interrupts;
};

#endif /* DSP_STATE_H */
