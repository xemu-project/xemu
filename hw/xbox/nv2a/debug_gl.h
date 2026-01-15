/*
 * Geforce NV2A PGRAPH OpenGL Renderer debug routines
 *
 * Copyright (c) 2025 Matt Borgerson
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

#ifndef XEMU_HW_XBOX_NV2A_DEBUG_GL_H_
#define XEMU_HW_XBOX_NV2A_DEBUG_GL_H_

#include <epoxy/gl.h>

#define ASSERT_NO_GL_ERROR()                                                 \
    do {                                                                     \
        GLenum error = glGetError();                                         \
        if (error != GL_NO_ERROR) {                                          \
            fprintf(stderr, "OpenGL error: 0x%X (%d) at %s:%d\n", error,     \
                    error, __FILE__, __LINE__);                              \
            nv2a_log_fatal_error("OpenGL error 0x%X (%d) detected at %s:%d", \
                                 error, error, __FILE__, __LINE__);          \
        }                                                                    \
    } while (0)


#ifdef XEMU_DEBUG_BUILD

#define ASSERT_FRAMEBUFFER_COMPLETE() \
    gl_debug_assert_framebuffer_complete(__FILE__, __LINE__)

void gl_debug_assert_framebuffer_complete(const char *source_file, int line);

#else

#define ASSERT_FRAMEBUFFER_COMPLETE()                             \
    do {                                                          \
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER); \
        if (status != GL_FRAMEBUFFER_COMPLETE) {                  \
            fprintf(stderr,                                       \
                    "OpenGL framebuffer status 0x%X (%d) != "     \
                    "GL_FRAMEBUFFER_COMPLETE at %s:%d\n",         \
                    status, status, __FILE__, __LINE__);          \
            nv2a_log_fatal_error(                                 \
                "OpenGL framebuffer status not complete: 0x%X "   \
                "(%d)\nat %s:%d\n",                               \
                status, status, __FILE__, __LINE__);              \
        }                                                         \
    } while (0)

#endif // XEMU_DEBUG_BUILD

#endif // XEMU_HW_XBOX_NV2A_DEBUG_GL_H_
