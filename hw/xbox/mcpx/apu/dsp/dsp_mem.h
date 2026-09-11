/*
 * MCPX DSP memory map
 *
 * Copyright (c) 2015 espes
 * Copyright (c) 2020-2025 Matt Borgerson
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

#define DSP_MIXBUFFER_BASE 0x001400
#define DSP_MIXBUFFER_SIZE 1024

#define DSP_PERIPH_BASE 0xFFFF80
#define DSP_PERIPH_SIZE 128

#endif
