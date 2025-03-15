/*
 *  Offscreen OpenGL abstraction layer -- SDL based
 *
 *  Copyright (c) 2018-2024 Matt Borgerson
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

#include <SDL3/SDL.h>

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
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_PROFILE_MASK,
        SDL_GL_CONTEXT_PROFILE_CORE);

    // Create properties for the window
    SDL_PropertiesID props = SDL_CreateProperties();
    const char* title = "SDL Offscreen Window";
    int x = SDL_WINDOWPOS_CENTERED;
    int y = SDL_WINDOWPOS_CENTERED;
    int width = 640;
    int height = 480;
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN;

    // Set window properties
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, x);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, y);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, width);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, height);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, flags);

    // Create the window with properties
    context->window = SDL_CreateWindowWithProperties(props);
    if (context->window == NULL) {
        fprintf(stderr, "%s: Failed to create window: %s\n", __func__);
        free(context); // Free allocated memory
        SDL_Quit();
        exit(1);
    }

    // Create OpenGL context
    context->gl_context = SDL_GL_CreateContext(context->window);
    if (context->gl_context == NULL) {
        fprintf(stderr, "%s: Failed to create GL context: %s\n", __func__);
        SDL_DestroyWindow(context->window);
        free(context); // Free allocated memory
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
