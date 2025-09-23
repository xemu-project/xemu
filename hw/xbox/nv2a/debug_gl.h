#ifndef XEMU_HW_XBOX_NV2A_DEBUG_GL_H_
#define XEMU_HW_XBOX_NV2A_DEBUG_GL_H_

#ifdef XEMU_DEBUG_BUILD

#define ASSERT_NO_GL_ERROR()                                                 \
    do {                                                                 \
        GLenum error = glGetError();                                     \
        if (error != GL_NO_ERROR) {                                      \
            fprintf(stderr, "OpenGL error: 0x%X (%d) at %s:%d\n", error, \
                    error, __FILE__, __LINE__);                          \
            assert(!"OpenGL error detected");                            \
        }                                                                \
    } while (0)

#define ASSERT_FRAMEBUFFER_COMPLETE()                                         \
    do {                                                                     \
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);            \
        if (status != GL_FRAMEBUFFER_COMPLETE) {                             \
            fprintf(stderr,                                                  \
                    "OpenGL framebuffer status: 0x%X (%d) != "               \
                    "GL_FRAMEBUFFER_COMPLETE at %s:%d\n",                    \
                    status, status, __FILE__, __LINE__);                     \
            assert(                                                          \
                !"OpenGL GL_FRAMEBUFFER status != GL_FRAMEBUFFER_COMPLETE"); \
        }                                                                    \
    } while (0)

#else

#define ASSERT_NO_GL_ERROR() assert(glGetError() == GL_NO_ERROR)

#define ASSERT_FRAMEBUFFER_COMPLETE() \
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)

#endif // XEMU_DEBUG_BUILD

#endif // XEMU_HW_XBOX_NV2A_DEBUG_GL_H_
