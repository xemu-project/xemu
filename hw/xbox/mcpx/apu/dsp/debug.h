/*
 * MCPX DSP emulator

 * Copyright (c) 2025 Matt Borgerson

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

#ifndef HW_XBOX_MCPX_DSP_DEBUG_H
#define HW_XBOX_MCPX_DSP_DEBUG_H

#ifndef DEBUG_DSP
#define DEBUG_DSP 0
#endif

#define TRACE_DSP_DISASM 0
#define TRACE_DSP_DISASM_REG 0
#define TRACE_DSP_DISASM_MEM 0

#define DPRINTF(fmt, ...) \
    do { \
        if (DEBUG_DSP) fprintf(stderr, fmt, ## __VA_ARGS__); \
    } while (0)

#endif
