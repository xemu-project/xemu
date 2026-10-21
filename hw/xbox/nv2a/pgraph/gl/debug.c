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

#include "renderer.h"
#include "debug.h"

#if DEBUG_NV2A_GL

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <assert.h>

#include <SDL3/SDL.h>

#ifdef CONFIG_RENDERDOC
#include "trace/control.h"

#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include "thirdparty/renderdoc_app.h"
#endif

#include "hw/xbox/nv2a/debug_gl.h"

#define DEFINE_GL_FUNC(type, name) static type _##name = NULL

/* On macOS, epoxy's stub function sometimes loses extension context so
 * functions are manually resolved during initialization. */
#if defined(__APPLE__)
#define LOAD_GL_FUNC(name) \
    _##name = (typeof(_##name))SDL_GL_GetProcAddress(#name)
#else
#define LOAD_GL_FUNC(name) _##name = name
#endif // defined(__APPLE__)

DEFINE_GL_FUNC(PFNGLPUSHDEBUGGROUPPROC, glPushDebugGroup);
DEFINE_GL_FUNC(PFNGLPOPDEBUGGROUPPROC, glPopDebugGroup);
DEFINE_GL_FUNC(PFNGLFRAMETERMINATORGREMEDYPROC, glFrameTerminatorGREMEDY);
DEFINE_GL_FUNC(PFNGLDEBUGMESSAGEINSERTPROC, glDebugMessageInsert);
DEFINE_GL_FUNC(PFNGLOBJECTLABELPROC, glObjectLabel);

void gl_debug_initialize(void)
{
    if (glo_check_extension("GL_KHR_debug")) {
        LOAD_GL_FUNC(glPushDebugGroup);
        LOAD_GL_FUNC(glPopDebugGroup);
        LOAD_GL_FUNC(glDebugMessageInsert);
        LOAD_GL_FUNC(glObjectLabel);

#if defined(__APPLE__)
        /* On macOS, calling glEnable(GL_DEBUG_OUTPUT) will result in error
         * GL_INVALID_ENUM.
         *
         * According to GL_KHR_debug this should work, therefore probably
         * not a bug in our code.
         *
         * It appears however that we can safely ignore this error, and the
         * debug functions which we depend on will still work as expected,
         * so skip the call for this platform.
         */
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#else
        glEnable(GL_DEBUG_OUTPUT);
#endif // defined(__APPLE__)
    }

    if (glo_check_extension("GL_GREMEDY_frame_terminator")) {
        LOAD_GL_FUNC(glFrameTerminatorGREMEDY);
    }

    ASSERT_NO_GL_ERROR();

#ifdef CONFIG_RENDERDOC
    nv2a_dbg_renderdoc_init();
    ASSERT_NO_GL_ERROR();
#endif
}

void gl_debug_message(bool cc, const char *fmt, ...)
{
    if (!_glDebugMessageInsert) {
        return;
    }

    size_t n;
    char buffer[1024];
    va_list ap;
    va_start(ap, fmt);
    n = vsnprintf(buffer, sizeof(buffer), fmt, ap);
    assert(n <= sizeof(buffer));
    va_end(ap);

    _glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0,
                          GL_DEBUG_SEVERITY_NOTIFICATION, n, buffer);
    if (cc) {
        fwrite(buffer, sizeof(char), n, stdout);
        fputc('\n', stdout);
    }
}

void gl_debug_group_begin(const char *fmt, ...)
{
    /* Debug group begin */
    if (_glPushDebugGroup) {
        size_t n;
        char buffer[1024];
        va_list ap;
        va_start(ap, fmt);
        n = vsnprintf(buffer, sizeof(buffer), fmt, ap);
        assert(n <= sizeof(buffer));
        va_end(ap);

        _glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, n, buffer);
    }

    /* Check for errors before starting real commands in group */
    ASSERT_NO_GL_ERROR();
}

void gl_debug_group_end(void)
{
    /* Check for errors when leaving group */
    ASSERT_NO_GL_ERROR();

    /* Debug group end */
    if (_glPopDebugGroup) {
        _glPopDebugGroup();
    }
}

void gl_debug_label(GLenum target, GLuint name, const char *fmt, ...)
{
    if (!_glObjectLabel) {
        return;
    }

    size_t n;
    char buffer[1024];
    va_list ap;
    va_start(ap, fmt);
    n = vsnprintf(buffer, sizeof(buffer), fmt, ap);
    assert(n <= sizeof(buffer));
    va_end(ap);

    _glObjectLabel(target, name, n, buffer);

    ASSERT_NO_GL_ERROR();
}

void gl_debug_frame_terminator(void)
{
    ASSERT_NO_GL_ERROR();

#ifdef CONFIG_RENDERDOC
    if (nv2a_dbg_renderdoc_available()) {

        RENDERDOC_API_1_6_0 *rdoc_api = nv2a_dbg_renderdoc_get_api();

        if (rdoc_api->IsTargetControlConnected()) {
            bool capturing = rdoc_api->IsFrameCapturing();
            if (capturing && renderdoc_capture_frames == 0) {
                rdoc_api->EndFrameCapture(NULL, NULL);
                GLenum error = glGetError();
                if (error != GL_NO_ERROR) {
                    fprintf(stderr,
                            "Renderdoc EndFrameCapture triggered GL error 0x%X - ignoring\n",
                            error);
                }
                if (renderdoc_trace_frames) {
                    trace_enable_events("-nv2a_pgraph_*");
                    renderdoc_trace_frames = false;
                }
            }
            if (renderdoc_capture_frames > 0) {
                if (!capturing) {
                    if (renderdoc_trace_frames) {
                        trace_enable_events("nv2a_pgraph_*");
                    }
                    rdoc_api->StartFrameCapture(NULL, NULL);
                    GLenum error = glGetError();
                    if (error != GL_NO_ERROR) {
                        fprintf(stderr,
                                "Renderdoc StartFrameCapture triggered GL error 0x%X - ignoring\n",
                                error);
                    }
                }
                --renderdoc_capture_frames;
            }
        }
    }
#endif
    if (_glFrameTerminatorGREMEDY) {
        _glFrameTerminatorGREMEDY();
        ASSERT_NO_GL_ERROR();
    }
}

#endif // DEBUG_NV2A_GL
