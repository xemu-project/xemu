/*
 * MCPX DSP memory map
 *
 * Copyright (c) 2015 espes
 * Copyright (c) 2020-2026 Matt Borgerson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef HW_XBOX_MCPX_DSP_DSP_MEM_H
#define HW_XBOX_MCPX_DSP_DSP_MEM_H

/* Memory spaces, as the DMA engine hands them to the core (the values are
 * Dsp56300MemSpace's). */
#define DSP_SPACE_X 0x00
#define DSP_SPACE_Y 0x01
#define DSP_SPACE_P 0x02

/* Allocation sizes for the backing arrays: the max across cores (the GP's). */
#define DSP_XRAM_SIZE 4096
#define DSP_YRAM_SIZE 2048
#define DSP_PRAM_SIZE 4096

/*
 * Per-core populated data RAM (measured on hardware). The EP trades most of
 * its Y RAM for the encode ROM below and has slightly less X RAM. Reads
 * above a core's RAM (and below the EP ROM) return 0; writes are ignored.
 */
#define DSP_GP_XRAM_SIZE 4096   /* GP X: $0000-$0FFF */
#define DSP_GP_YRAM_SIZE 2048   /* GP Y: $0000-$07FF */
#define DSP_EP_XRAM_SIZE 3072   /* EP X: $0000-$0BFF */
#define DSP_EP_YRAM_SIZE 256    /* EP Y: $0000-$00FF */

/*
 * The EP core carries a 2048-word on-chip Y data ROM at Y:$0800-$0FFF
 * holding its Dolby Digital / AC3 encode tables (ep_yrom.h). The GP has no
 * such ROM; reads there return 0.
 */
#define DSP_YROM_BASE 0x0800
#define DSP_YROM_SIZE 0x0800

#define DSP_MIXBUFFER_BASE 0x001400
#define DSP_MIXBUFFER_SIZE 1024

#define DSP_PERIPH_BASE 0xFFFF80
#define DSP_PERIPH_SIZE 128

#endif
