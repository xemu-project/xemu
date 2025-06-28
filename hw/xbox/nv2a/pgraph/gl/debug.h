/*
 * Geforce NV2A PGRAPH OpenGL Renderer
 *
 * Copyright (c) 2015 Jannik Vogel
 * Copyright (c) 2012 espes
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

#ifndef HW_XBOX_NV2A_PGRAPH_GL_DEBUG_H
#define HW_XBOX_NV2A_PGRAPH_GL_DEBUG_H

#ifndef DEBUG_NV2A_GL
# define DEBUG_NV2A_GL 0
#endif

#if DEBUG_NV2A_GL

#include <stdbool.h>
#include "gloffscreen.h"
#include "config-host.h"

void gl_debug_initialize(void);
void gl_debug_message(bool cc, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
void gl_debug_group_begin(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
void gl_debug_group_end(void);
void gl_debug_label(GLenum target, GLuint name, const char *fmt, ...) __attribute__ ((format (printf, 3, 4)));
void gl_debug_frame_terminator(void);

# define NV2A_GL_DPRINTF(cc, format, ...) \
    gl_debug_message(cc, "nv2a: " format, ## __VA_ARGS__)
# define NV2A_GL_DGROUP_BEGIN(format, ...) \
    gl_debug_group_begin("nv2a: " format, ## __VA_ARGS__)
# define NV2A_GL_DGROUP_END() \
    gl_debug_group_end()
# define NV2A_GL_DLABEL(target, name, format, ...)  \
    gl_debug_label(target, name, "nv2a: { " format " }", ## __VA_ARGS__)
#define NV2A_GL_DFRAME_TERMINATOR() \
    gl_debug_frame_terminator()

#else

# define NV2A_GL_DPRINTF(cc, format, ...)          do { \
        if (cc) NV2A_DPRINTF(format "\n", ##__VA_ARGS__ ); \
    } while (0)
# define NV2A_GL_DGROUP_BEGIN(format, ...)         do { } while (0)
# define NV2A_GL_DGROUP_END()                      do { } while (0)
# define NV2A_GL_DLABEL(target, name, format, ...) do { } while (0)
# define NV2A_GL_DFRAME_TERMINATOR()               do { } while (0)
#endif

#endif
