/*
 * QEMU USB Xbox Live Communicator (XBLC) Device
 *
 * Copyright (c) 2025 faha223
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

#ifndef HW_XBOX_XBLC_H
#define HW_XBOX_XBLC_H

#ifdef __cplusplus
extern "C" {
#endif

void xblc_audio_stream_reinit(void *dev);

/* Outputs a value on the interval [0, 1] where 0 is muted and 1 is full volume */
float xblc_audio_stream_get_output_volume(void *dev);
float xblc_audio_stream_get_input_volume(void *dev);

/* Accepts a value on the interval [0, 1] where 0 is muted and 1 is full volume */
void xblc_audio_stream_set_output_volume(void *dev, float volume);
void xblc_audio_stream_set_input_volume(void *dev, float volume);
float xblc_audio_stream_get_current_input_volume(void *dev);

#ifdef __cplusplus
}
#endif

#endif