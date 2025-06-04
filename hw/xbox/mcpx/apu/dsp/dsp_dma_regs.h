/*
 * MCPX DSP DMA
 *
 * Copyright (c) 2015 espes
 * Copyright (c) 2020-2025 Matt Borgerson
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

#ifndef HW_XBOX_MCPX_DSP_DSP_DMA_REGS_H
#define HW_XBOX_MCPX_DSP_DSP_DMA_REGS_H

#define DMA_CONFIGURATION_AUTOSTART (1 << 0)
#define DMA_CONFIGURATION_AUTOREADY (1 << 1)
#define DMA_CONFIGURATION_IOC_CLEAR (1 << 2)
#define DMA_CONFIGURATION_EOL_CLEAR (1 << 3)
#define DMA_CONFIGURATION_ERR_CLEAR (1 << 4)

#define DMA_CONTROL_ACTION 0x7
#define DMA_CONTROL_ACTION_NOP 0
#define DMA_CONTROL_ACTION_START 1
#define DMA_CONTROL_ACTION_STOP 2
#define DMA_CONTROL_ACTION_FREEZE 3
#define DMA_CONTROL_ACTION_UNFREEZE 4
#define DMA_CONTROL_ACTION_ABORT 5
#define DMA_CONTROL_FROZEN (1 << 3)
#define DMA_CONTROL_RUNNING (1 << 4)
#define DMA_CONTROL_STOPPED (1 << 5)

#define NODE_POINTER_VAL 0x3fff
#define NODE_POINTER_EOL (1 << 14)

#define NODE_CONTROL_DIRECTION (1 << 1)

#endif
