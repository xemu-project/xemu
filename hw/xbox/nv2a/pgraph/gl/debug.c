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

#ifdef CONFIG_RENDERDOC
#include "trace/control.h"

#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include "thirdparty/renderdoc_app.h"
#endif

#define CHECK_GL_ERROR() do { \
  GLenum error = glGetError(); \
  if (error != GL_NO_ERROR) {  \
      fprintf(stderr, "OpenGL error: 0x%X (%d) at %s:%d\n", error, error, __FILE__, __LINE__); \
      assert(!"OpenGL error detected");                                                        \
  } \
} while(0)

static bool has_GL_GREMEDY_frame_terminator = false;
static bool has_GL_KHR_debug = false;

void gl_debug_initialize(void)
{
    has_GL_KHR_debug = glo_check_extension("GL_KHR_debug");
    has_GL_GREMEDY_frame_terminator = glo_check_extension("GL_GREMEDY_frame_terminator");

    if (has_GL_KHR_debug) {
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
#else
       glEnable(GL_DEBUG_OUTPUT);
       assert(glGetError() == GL_NO_ERROR);
#endif
    }

#ifdef CONFIG_RENDERDOC
    nv2a_dbg_renderdoc_init();
#endif
}

void gl_debug_message(bool cc, const char *fmt, ...)
{
    if (!has_GL_KHR_debug) {
        return;
    }

    size_t n;
    char buffer[1024];
    va_list ap;
    va_start(ap, fmt);
    n = vsnprintf(buffer, sizeof(buffer), fmt, ap);
    assert(n <= sizeof(buffer));
    va_end(ap);

    glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER,
                         0, GL_DEBUG_SEVERITY_NOTIFICATION, n, buffer);
    if (cc) {
        fwrite(buffer, sizeof(char), n, stdout);
        fputc('\n', stdout);
    }
}

void gl_debug_group_begin(const char *fmt, ...)
{
    /* Debug group begin */
    if (has_GL_KHR_debug) {
        size_t n;
        char buffer[1024];
        va_list ap;
        va_start(ap, fmt);
        n = vsnprintf(buffer, sizeof(buffer), fmt, ap);
        assert(n <= sizeof(buffer));
        va_end(ap);

        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, n, buffer);
    }

    /* Check for errors before starting real commands in group */
    assert(glGetError() == GL_NO_ERROR);
}

void gl_debug_group_end(void)
{
    /* Check for errors when leaving group */
    assert(glGetError() == GL_NO_ERROR);

    /* Debug group end */
    if (has_GL_KHR_debug) {
        glPopDebugGroup();
    }
}

void gl_debug_label(GLenum target, GLuint name, const char *fmt, ...)
{
    if (!has_GL_KHR_debug) {
        return;
    }

    size_t n;
    char buffer[1024];
    va_list ap;
    va_start(ap, fmt);
    n = vsnprintf(buffer, sizeof(buffer), fmt, ap);
    assert(n <= sizeof(buffer));
    va_end(ap);

    glObjectLabel(target, name, n, buffer);

    GLenum err = glGetError();
    assert(err == GL_NO_ERROR);
}

void gl_debug_frame_terminator(void)
{
    CHECK_GL_ERROR();

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
    if (has_GL_GREMEDY_frame_terminator) {
        glFrameTerminatorGREMEDY();
        CHECK_GL_ERROR();
    }
}

#endif // DEBUG_NV2A_GL
