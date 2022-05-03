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

#include "IconsFontAwesome6.h"
#define ICON_BUTTON_A "\xef\xa4\x80"
#define ICON_BUTTON_B "\xef\xa4\x81"
#define ICON_BUTTON_X "\xef\xa4\x82"
#define ICON_BUTTON_Y "\xef\xa4\x83"

class FontManager
{
public:
    ImFont *m_default_font;
    ImFont *m_fixed_width_font;
    ImFont *m_menu_font;
    ImFont *m_menu_font_small;
    ImFont *m_menu_font_medium;
    // ImFont *m_big_state_icon_font;
    float m_last_viewport_scale;
    float m_font_scale;

    FontManager();
    void Rebuild();
    void Update();
};

extern FontManager g_font_mgr;
