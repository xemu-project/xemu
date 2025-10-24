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
#include <string>
#include <vector>
#include "animation.hh"

class BackgroundGradient
{
protected:
    EasingAnimation m_animation;

public:
    BackgroundGradient();
    void Show();
    void Hide();
    bool IsAnimating();
    void Draw();
};

class NavControlItem
{
protected:
    std::string m_icon;
    std::string m_text;

public:
    NavControlItem(std::string icon, std::string text);
    void Draw();
};

class NavControlAnnotation
{
protected:
    EasingAnimation m_animation;
    std::vector<NavControlItem> m_items;
    bool m_show, m_visible;

public:
    NavControlAnnotation();
    void Show();
    void Hide();
    bool IsAnimating();
    void Draw();
};

#if 0
class BigStateIcon {
protected:
    EasingAnimation m_animation;

public:
    BigStateIcon();
    void Show();
    void Hide();
    bool IsAnimating();
    void Draw();
};

class TitleInfo
{
protected:
    GLuint screenshot;
    ImVec2 size;
    EasingAnimation m_animation;

public:
    TitleInfo();
    void Show();
    void Hide();
    bool IsAnimating();
    void initScreenshot();
    void Draw();
};
#endif
