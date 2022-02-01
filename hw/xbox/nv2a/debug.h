/*
 * QEMU Geforce NV2A debug helpers
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

#ifndef HW_NV2A_DEBUG_H
#define HW_NV2A_DEBUG_H

#include <stdint.h>

#define NV2A_XPRINTF(x, ...) do { \
    if (x) { \
        fprintf(stderr, "nv2a: " __VA_ARGS__); \
    } \
} while (0)

// #define DEBUG_NV2A
#ifdef DEBUG_NV2A
# define NV2A_DPRINTF(format, ...)       printf("nv2a: " format, ## __VA_ARGS__)
#else
# define NV2A_DPRINTF(format, ...)       do { } while (0)
#endif

// #define DEBUG_NV2A_GL
#ifdef DEBUG_NV2A_GL

#include <stdbool.h>
#include "gl/gloffscreen.h"
#include "config-host.h"

void gl_debug_initialize(void);
void gl_debug_message(bool cc, const char *fmt, ...);
void gl_debug_group_begin(const char *fmt, ...);
void gl_debug_group_end(void);
void gl_debug_label(GLenum target, GLuint name, const char *fmt, ...);
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

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_RENDERDOC
bool nv2a_dbg_renderdoc_available(void);
void nv2a_dbg_renderdoc_capture_frames(uint32_t num_frames);
#endif

#ifdef __cplusplus
}
#endif

#else
# define NV2A_GL_DPRINTF(cc, format, ...)          do { \
        if (cc) NV2A_DPRINTF(format "\n", ##__VA_ARGS__ ); \
    } while (0)
# define NV2A_GL_DGROUP_BEGIN(format, ...)         do { } while (0)
# define NV2A_GL_DGROUP_END()                      do { } while (0)
# define NV2A_GL_DLABEL(target, name, format, ...) do { } while (0)
# define NV2A_GL_DFRAME_TERMINATOR()               do { } while (0)
#endif

/* Debug prints to identify when unimplemented or unconfirmed features
 * are being exercised. These cases likely result in graphical problems of
 * varying degree, but should otherwise not crash the system. Enable this
 * macro for debugging.
 */
// #define DEBUG_NV2A_FEATURES 1

#ifdef DEBUG_NV2A_FEATURES

/* Feature which has not yet been confirmed */
#define NV2A_UNCONFIRMED(format, ...) do { \
    fprintf(stderr, "nv2a: Warning unconfirmed feature: " format "\n", ## __VA_ARGS__); \
} while (0)

/* Feature which is not implemented */
#define NV2A_UNIMPLEMENTED(format, ...) do { \
    fprintf(stderr, "nv2a: Warning unimplemented feature: " format "\n", ## __VA_ARGS__); \
} while (0)

#else

#define NV2A_UNCONFIRMED(...) do {} while (0)
#define NV2A_UNIMPLEMENTED(...) do {} while (0)

#endif

#define NV2A_PROF_COUNTERS_XMAC \
    _X(NV2A_PROF_BEGIN_ENDS) \
    _X(NV2A_PROF_DRAW_ARRAYS) \
    _X(NV2A_PROF_INLINE_BUFFERS) \
    _X(NV2A_PROF_INLINE_ARRAYS) \
    _X(NV2A_PROF_INLINE_ELEMENTS) \
    _X(NV2A_PROF_QUERY) \
    _X(NV2A_PROF_SHADER_GEN) \
    _X(NV2A_PROF_SHADER_BIND) \
    _X(NV2A_PROF_SHADER_BIND_NOTDIRTY) \
    _X(NV2A_PROF_ATTR_BIND) \
    _X(NV2A_PROF_TEX_UPLOAD) \
    _X(NV2A_PROF_TEX_BIND) \
    _X(NV2A_PROF_GEOM_BUFFER_UPDATE_1) \
    _X(NV2A_PROF_GEOM_BUFFER_UPDATE_2) \
    _X(NV2A_PROF_GEOM_BUFFER_UPDATE_3) \
    _X(NV2A_PROF_GEOM_BUFFER_UPDATE_4) \
    _X(NV2A_PROF_GEOM_BUFFER_UPDATE_4_NOTDIRTY) \
    _X(NV2A_PROF_SURF_DOWNLOAD) \
    _X(NV2A_PROF_SURF_UPLOAD) \
    _X(NV2A_PROF_SURF_TO_TEX) \
    _X(NV2A_PROF_SURF_TO_TEX_FALLBACK) \

enum NV2A_PROF_COUNTERS_ENUM {
    #define _X(x) x,
    NV2A_PROF_COUNTERS_XMAC
    #undef _X
    NV2A_PROF__COUNT
};

#define NV2A_PROF_NUM_FRAMES 300

typedef struct NV2AStats {
    int64_t last_flip_time;
    unsigned int frame_count;
    unsigned int increment_fps;
    struct {
        int mspf;
        int counters[NV2A_PROF__COUNT];
    } frame_working, frame_history[NV2A_PROF_NUM_FRAMES];
    unsigned int frame_ptr;
} NV2AStats;

#ifdef __cplusplus
extern "C" {
#endif

extern NV2AStats g_nv2a_stats;

const char *nv2a_profile_get_counter_name(unsigned int cnt);
int nv2a_profile_get_counter_value(unsigned int cnt);

#ifdef __cplusplus
}
#endif

#endif
