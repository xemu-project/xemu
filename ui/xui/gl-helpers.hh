//
// xemu User Interface
//
// Copyright (C) 2020-2022 Matt Borgerson
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#pragma once
#include "common.hh"
#include "../xemu-input.h"

class Fbo
{
public:
    static GLint vp[4];
    static GLint original_fbo;
    static bool blend;

    int w, h;
    GLuint fbo, tex;

    Fbo(int width, int height);
    ~Fbo();
    inline GLuint Texture() { return tex; }
    void Target();
    void Restore();
};

extern Fbo *controller_fbo, *logo_fbo;

void InitCustomRendering(void);
void RenderLogo(uint32_t time, uint32_t primary_color, uint32_t secondary_color,
                uint32_t fill_color);
void RenderController(float frame_x, float frame_y, uint32_t primary_color,
                      uint32_t secondary_color, ControllerState *state);
void RenderControllerPort(float frame_x, float frame_y, int i,
                          uint32_t port_color);
void RenderFramebuffer(GLint tex, int width, int height, bool flip);
void SaveScreenshot(GLuint tex, bool flip);
