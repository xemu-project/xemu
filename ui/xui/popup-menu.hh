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
#include "scene.hh"
#include "scene-components.hh"
#include "animation.hh"
#include "widgets.hh"

class PopupMenu;

class PopupMenuItemDelegate
{
public:
    PopupMenuItemDelegate() = default;
    virtual ~PopupMenuItemDelegate();
    virtual void PushMenu(PopupMenu &menu);
    virtual void PopMenu();
    virtual void ClearMenuStack();
    virtual void LostFocus();
    virtual void PushFocus();
    virtual void PopFocus();
    virtual bool DidPop();
};

class PopupMenu
{
protected:
    EasingAnimation m_animation;
    ImVec2 m_ease_direction;
    bool m_focus;
    bool m_pop_focus;

public:
    PopupMenu();
    void InitFocus();
    virtual ~PopupMenu();
    void Show(const ImVec2 &direction);
    void Hide(const ImVec2 &direction);
    bool IsAnimating();
    void Draw(PopupMenuItemDelegate &nav);
    virtual bool DrawItems(PopupMenuItemDelegate &nav);
};

class PopupMenuScene : virtual public PopupMenuItemDelegate, public Scene {
protected:
    std::vector<PopupMenu *> m_view_stack;
    std::vector<PopupMenu *> m_menus_in_transition;
    std::vector<std::pair<ImGuiID, ImRect>> m_focus_stack;
    BackgroundGradient m_background;
    NavControlAnnotation m_nav_control_view;
    // BigStateIcon m_big_state_icon;
    // TitleInfo m_title_info;

public:
    void PushMenu(PopupMenu &menu) override;
    void PopMenu() override;
    void PushFocus() override;
    void PopFocus() override;
    void ClearMenuStack() override;
    void HandleInput();
    void Show() override;
    void Hide() override;
    bool IsAnimating() override;
    bool Draw() override;
    void LostFocus() override;
};

extern PopupMenuScene g_popup_menu;
