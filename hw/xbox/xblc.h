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
int xblc_audio_stream_get_output_volume(void *dev);
int xblc_audio_stream_get_input_volume(void *dev);
void xblc_audio_stream_set_output_volume(void *dev, int volume);
void xblc_audio_stream_set_input_volume(void *dev, int volume);
int xblc_audio_stream_get_average_input_volume(void *dev);

#ifdef __cplusplus
}
#endif

#endif