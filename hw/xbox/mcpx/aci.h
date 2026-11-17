/*
 * QEMU MCPX Audio Codec Interface
 *
 * Copyright (c) 2026 Matt Borgerson
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

#ifndef HW_XBOX_MCPX_ACI_H
#define HW_XBOX_MCPX_ACI_H

#include <stdbool.h>
#include <stdint.h>

/* Clock the ACI's bus masters by `frames` stereo frames (one sound engine
 * frame is 32): PCM out lands in `pcm` (`frames` stereo S16 frames at the
 * codec's output volume, silence where the engine is not running), S/PDIF
 * out in `spdif` (`frames` * 4 bytes of the IEC 61937 stream). False when
 * there is no ACI. */
bool mcpx_aci_frame(int frames, int16_t *pcm, uint8_t *spdif);

#endif
