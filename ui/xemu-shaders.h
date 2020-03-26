/*
 * xemu User Interface Rendering Helpers
 *
 * Copyright (C) 2020 Matt Borgerson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef XEMU_SHADERS_H
#define XEMU_SHADERS_H

#include <SDL2/SDL.h>
#include <epoxy/gl.h>

#include "stb_image.h"

enum SHADER_TYPE {
    SHADER_TYPE_BLIT,
    SHADER_TYPE_MASK,
    SHADER_TYPE_LOGO,
};

struct decal_shader
{
    int flip;
    float scale;
    float smoothing;
    float outline_dist;
    uint32_t time;

    // GL object handles
    GLuint prog, vao, vbo, ebo;

    // Uniform locations
    GLint Mat_loc;
    GLint FlipY_loc;
    GLint tex_loc;
    GLint ScaleOffset_loc;
    GLint TexScaleOffset_loc;

    GLint ColorPrimary_loc;
    GLint ColorSecondary_loc;
    GLint ColorFill_loc;
	GLint time_loc;
	GLint scale_loc;
};

struct fbo {
    GLuint fbo;
    GLuint tex;
    int w, h;
};

#ifdef __cplusplus
extern "C" {
#endif

extern GLuint main_fb;
extern GLint vp[4];

GLuint compile_shader(GLenum type, const char *src);

struct decal_shader *create_decal_shader(enum SHADER_TYPE type);
void delete_decal_shader(struct decal_shader *s);

GLuint load_texture_from_file(const char *name);

struct fbo *create_fbo(int width, int height);
void render_to_default_fb(void);
GLuint render_to_fbo(struct fbo *fbo);

void render_decal( 
    struct decal_shader *s,
    float x,     float y,     float w,     float h,
    float tex_x, float tex_y, float tex_w, float tex_h,
    uint32_t primary, uint32_t secondary, uint32_t fill
    );

void render_decal_image( 
    struct decal_shader *s,
    float x,     float y,     float w,     float h,
    float tex_x, float tex_y, float tex_w, float tex_h
    );

#ifdef __cplusplus
}
#endif   

#endif
