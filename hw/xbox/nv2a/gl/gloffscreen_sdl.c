/*
 *  Offscreen OpenGL abstraction layer -- SDL based
 *
 *  Copyright (c) 2018 Matt Borgerson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "gloffscreen.h"

#include <SDL.h>
#include <SDL_syswm.h>

struct _GloContext {
    SDL_Window    *window;
    SDL_GLContext gl_context;
};

/* Create an OpenGL context */
GloContext *glo_context_create(void)
{
    GloContext *context = (GloContext *)malloc(sizeof(GloContext));
    assert(context != NULL);

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    // Initialize rendering context
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_PROFILE_MASK,
        SDL_GL_CONTEXT_PROFILE_CORE);

    // Create main window
    context->window = SDL_CreateWindow(
        "SDL Offscreen Window",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        640, 480,
        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (context->window == NULL) {
        fprintf(stderr, "%s: Failed to create window\n", __func__);
        SDL_Quit();
        exit(1);
    }

    context->gl_context = SDL_GL_CreateContext(context->window);
    if (context->gl_context == NULL) {
        fprintf(stderr, "%s: Failed to create GL context\n", __func__);
        SDL_DestroyWindow(context->window);
        SDL_Quit();
        exit(1);
    }

    glo_set_current(context);

    return context;
}

/* Set current context */
void glo_set_current(GloContext *context)
{
    if (context == NULL) {
        SDL_GL_MakeCurrent(NULL, NULL);
    } else {
        SDL_GL_MakeCurrent(context->window, context->gl_context);
    }
}

/* Destroy a previously created OpenGL context */
void glo_context_destroy(GloContext *context)
{
    if (!context) return;
    glo_set_current(NULL);
    free(context);
}
