/*
 * Geforce NV2A PGRAPH general debug routines
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

#include "hw/xbox/nv2a/debug.h"
#include "hw/xbox/nv2a/debug_gl.h"

#include "qemu/osdep.h"


static gchar *fatal_error_log_path = NULL;

void nv2a_set_fatal_error_log_path(const gchar *path)
{
    if (fatal_error_log_path) {
        g_free(fatal_error_log_path);
    }

    fatal_error_log_path = path ? g_strdup(path) : NULL;
}

void nv2a_log_fatal_error(const char *msg, ...)
{
    FILE *error_log = stderr;
    if (fatal_error_log_path) {
        error_log = fopen(fatal_error_log_path, "w");
        if (!error_log) {
            fprintf(stderr, "Failed to open fatal error log at '%s'\n",
                    fatal_error_log_path);
            error_log = stderr;
        }
    }

    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);

    char timestamp[32] = {
        0,
    };
    strftime(timestamp, sizeof(timestamp), "%Y%m%d at %T", local_time);

    fprintf(error_log, "%s\n", timestamp);

    va_list args;
    va_start(args, msg);
    vfprintf(error_log, msg, args);
    va_end(args);

    fprintf(error_log, "\n\n");

    if (error_log != stderr) {
        fclose(error_log);
    }
}

#ifdef XEMU_DEBUG_BUILD

static GLenum framebuffer_attachments[] = { GL_COLOR_ATTACHMENT0,
                                            GL_DEPTH_ATTACHMENT,
                                            GL_STENCIL_ATTACHMENT,
                                            GL_DEPTH_STENCIL_ATTACHMENT };

void gl_debug_assert_framebuffer_complete(const char *source_file, int line)
{
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status == GL_FRAMEBUFFER_COMPLETE) {
        return;
    }

    fprintf(stderr,
            "OpenGL framebuffer status: 0x%X (%d) != "
            "GL_FRAMEBUFFER_COMPLETE at %s:%d\n",
            status, status, source_file, line);

    GLint objectType, objectName;
    for (int i = 0; i < ARRAY_SIZE(framebuffer_attachments); ++i) {
        GLenum attachment = framebuffer_attachments[i];

        glGetFramebufferAttachmentParameteriv(
            GL_FRAMEBUFFER, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
            &objectType);

        if (objectType == GL_NONE) {
            fprintf(stderr, "\t0x%X: GL_NONE\n", attachment);
        } else {
            glGetFramebufferAttachmentParameteriv(
                GL_FRAMEBUFFER, attachment,
                GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &objectName);

            fprintf(stderr, "\t0x%X: type=%d, name=%d\n", attachment,
                    objectType, objectName);

            if (objectType == GL_TEXTURE) {
                GLint width, height, internalFormat;
                glBindTexture(GL_TEXTURE_2D, objectName);
                glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH,
                                         &width);
                glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT,
                                         &height);
                glGetTexLevelParameteriv(GL_TEXTURE_2D, 0,
                                         GL_TEXTURE_INTERNAL_FORMAT,
                                         &internalFormat);
                fprintf(stderr, "\t\tTexture: %dx%d, format=0x%X\n", width,
                        height, internalFormat);
            }
        }
    }

    assert(!"OpenGL GL_FRAMEBUFFER status != GL_FRAMEBUFFER_COMPLETE");
}

#endif // ifdef XEMU_DEBUG_BUILD
