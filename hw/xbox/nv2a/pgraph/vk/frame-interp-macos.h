/*
 * Frame Interpolation for macOS via VideoToolbox VTFrameProcessor
 *
 * Copyright (c) 2025 xemu contributors
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

#ifndef FRAME_INTERP_MACOS_H
#define FRAME_INTERP_MACOS_H

#include <stdbool.h>
#include <IOSurface/IOSurfaceRef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Check if VTFrameRateConversion is supported on this system */
bool frame_interp_is_available(void);

/* Initialize frame interpolation for given dimensions.
 * Returns true on success. */
bool frame_interp_init(int width, int height);

/* Tear down frame interpolation resources */
void frame_interp_finalize(void);

/* Push a new real frame's IOSurface into the ring buffer */
void frame_interp_push_frame(IOSurfaceRef surface);

/* Generate an interpolated frame between the two most recent pushed frames.
 * Returns the IOSurface of the interpolated frame, or NULL if not enough
 * frames have been pushed or interpolation fails. The returned IOSurface
 * is owned by the interpolator and valid until the next call. */
IOSurfaceRef frame_interp_get_interpolated(void);

#ifdef __cplusplus
}
#endif

#endif /* FRAME_INTERP_MACOS_H */
