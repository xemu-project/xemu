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
#include "ui/xemu-notifications.h"
#include <string>
#include <vector>
#include <filesystem>
#include <map>
#include "misc.hh"
#include "actions.hh"
#include "font-manager.hh"
#include "viewport-manager.hh"
#include "scene-manager.hh"
#include "popup-menu.hh"
#include "popup-menu-draw.hh"
#include "menu-system.hh"
#include "input-manager.hh"
#include "xemu-hud.h"
#include "IconsFontAwesome6.h"
#include "../xemu-snapshots.h"
#include "main-menu.hh"

PopupMenuItemDelegate::~PopupMenuItemDelegate() {}
void PopupMenuItemDelegate::PushMenu(PopupMenu &menu) {}
void PopupMenuItemDelegate::PopMenu() {}
void PopupMenuItemDelegate::ClearMenuStack() {}
void PopupMenuItemDelegate::LostFocus() {}
void PopupMenuItemDelegate::PushFocus() {}
void PopupMenuItemDelegate::PopFocus() {}
bool PopupMenuItemDelegate::DidPop() { return false; }

PopupMenu::PopupMenu() : m_animation(0.12, 0.12), m_ease_direction(0, 0)
{
    m_focus = false;
    m_pop_focus = false;
}

void PopupMenu::InitFocus()
{
    m_pop_focus = true;
}

PopupMenu::~PopupMenu()
{

}

void PopupMenu::Show(const ImVec2 &direction)
{
    m_animation.EaseIn();
    m_ease_direction = direction;
    m_focus = true;
}

void PopupMenu::Hide(const ImVec2 &direction)
{
    m_animation.EaseOut();
    m_ease_direction = direction;
}

bool PopupMenu::IsAnimating()
{
    return m_animation.IsAnimating();
}

void PopupMenu::Draw(PopupMenuItemDelegate &nav)
{
    m_animation.Step();

    ImGuiIO &io = ImGui::GetIO();
    float t = m_animation.GetSinInterpolatedValue();
    float window_alpha = t;
    ImVec2 window_pos = ImVec2(io.DisplaySize.x / 2 + (1-t) * m_ease_direction.x,
                               io.DisplaySize.y / 2 + (1-t) * m_ease_direction.y);

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, window_alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        g_viewport_mgr.Scale(ImVec2(10, 5)));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0.5));
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_WindowBg));
    ImGui::PushStyleColor(ImGuiCol_NavHighlight, IM_COL32_BLACK_TRANS);

    if (m_focus) ImGui::SetNextWindowFocus();
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, ImVec2(0.5, 0.5));
    ImGui::SetNextWindowSize(ImVec2(400*g_viewport_mgr.m_scale, 0), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0);

    ImGui::Begin("###PopupMenu", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
    if (DrawItems(nav)) nav.PopMenu();
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) nav.LostFocus();
    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 sz = ImGui::GetWindowSize();
    ImGui::End();

    if (!g_input_mgr.IsNavigatingWithController()) {
        ImGui::PushFont(g_font_mgr.m_menu_font);
        pos.y -= ImGui::GetFrameHeight();
        ImGui::SetNextWindowPos(pos);
        ImGui::SetNextWindowSize(ImVec2(sz.x, ImGui::GetFrameHeight()));
        ImGui::SetNextWindowBgAlpha(0);
        ImGui::Begin("###PopupMenuNav", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 200));
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32_BLACK_TRANS);
        if (ImGui::Button(ICON_FA_ARROW_LEFT)) {
            nav.PopMenu();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Back");
        }
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - ImGui::GetStyle().FramePadding.x * 2.0f - ImGui::GetTextLineHeight());
        if (ImGui::Button(ICON_FA_XMARK)) {
            nav.ClearMenuStack();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Close");
        }
        ImGui::PopStyleColor(2);
        ImGui::End();
        ImGui::PopFont();
    }

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(7);
    m_pop_focus = false;
    m_focus = false;
}

bool PopupMenu::DrawItems(PopupMenuItemDelegate &nav)
{
    return false;
}

extern MainMenuScene g_main_menu;

class SettingsPopupMenu : public virtual PopupMenu {
public:
    bool DrawItems(PopupMenuItemDelegate &nav) override
    {
        bool pop = false;

        if (m_focus && !m_pop_focus) {
            ImGui::SetKeyboardFocusHere();
        }
        MenuDrawViewItems(false);
        if (PopupMenuButton("Snapshots...", ICON_FA_CLOCK_ROTATE_LEFT)) {
            nav.ClearMenuStack();
            g_scene_mgr.PushScene(g_main_menu);
            g_main_menu.ShowSnapshots();
        }
        if (PopupMenuButton("All settings...", ICON_FA_SLIDERS)) {
            nav.ClearMenuStack();
            g_scene_mgr.PushScene(g_main_menu);
        }
        if (m_pop_focus) {
            nav.PopFocus();
        }
        return pop;
    }
};

class GamesPopupMenu : public virtual PopupMenu {
protected:
    std::multimap<std::string, std::string> sorted_file_names;

public:
    void Show(const ImVec2 &direction) override
    {
        PopupMenu::Show(direction);
        PopulateGameList();
    }

    bool DrawItems(PopupMenuItemDelegate &nav) override
    {
        bool pop = false;

        if (m_focus && !m_pop_focus) {
            ImGui::SetKeyboardFocusHere();
        }

        for (const auto &[label, file_path] : sorted_file_names) {
            if (PopupMenuButton(label, ICON_FA_COMPACT_DISC)) {
                ActionLoadDiscFile(file_path.c_str());
                nav.ClearMenuStack();
                pop = true;
            }
        }

        if (sorted_file_names.size() == 0) {
            if (PopupMenuButton("No games found", ICON_FA_SLIDERS)) {
                nav.ClearMenuStack();
                g_scene_mgr.PushScene(g_main_menu);
            }
        }

        if (m_pop_focus) {
            nav.PopFocus();
        }
        return pop;
    }

    void PopulateGameList() {
        sorted_file_names.clear();
        std::filesystem::path directory(g_config.general.games_dir);
        std::error_code ec;
        if (std::filesystem::is_directory(directory, ec)) {
            for (const auto &file :
                 std::filesystem::directory_iterator(directory)) {
                const auto &file_path = file.path();
                if (std::filesystem::is_regular_file(file_path) &&
                    (file_path.extension() == ".iso" ||
                     file_path.extension() == ".xiso")) {
                    sorted_file_names.insert(
                        { file_path.stem().string(), file_path.string() });
                }
            }
        }
    }
};

class ViewPopupMenu : public virtual PopupMenu {
public:
    bool DrawItems(PopupMenuItemDelegate &nav) override
    {
        if (m_focus && !m_pop_focus) {
            ImGui::SetKeyboardFocusHere();
        }
        MenuDrawViewItems(false);
        if (m_pop_focus) {
            nav.PopFocus();
        }
        return false;
    }
};

class DebugPopupMenu : public virtual PopupMenu {
public:
    bool DrawItems(PopupMenuItemDelegate &nav) override
    {
        if (m_focus && !m_pop_focus) {
            ImGui::SetKeyboardFocusHere();
        }
        MenuDrawDebugItems(false);
        if (m_pop_focus) {
            nav.PopFocus();
        }
        return false;
    }
};

class HelpPopupMenu : public virtual PopupMenu {
public:
    bool DrawItems(PopupMenuItemDelegate &nav) override
    {
        if (m_focus && !m_pop_focus) {
            ImGui::SetKeyboardFocusHere();
        }
        MenuDrawHelpItems(false, &nav);
        if (m_pop_focus) {
            nav.PopFocus();
        }
        return false;
    }
};

class RootPopupMenu : public virtual PopupMenu {
protected:
    SettingsPopupMenu settings;
    GamesPopupMenu games;
    ViewPopupMenu view_menu;
    DebugPopupMenu debug_menu;
    HelpPopupMenu help_menu;
    bool refocus_first_item;

public:
    RootPopupMenu() {
        refocus_first_item = false;
    }

    bool DrawItems(PopupMenuItemDelegate &nav) override
    {
        bool pop = false;

        if (refocus_first_item || (m_focus && !m_pop_focus)) {
            ImGui::SetKeyboardFocusHere();
            refocus_first_item = false;
        }

        bool running = runstate_is_running();
        if (running) {
            if (PopupMenuButton("Pause", ICON_FA_CIRCLE_PAUSE)) {
                ActionTogglePause();
                refocus_first_item = true;
            }
        } else {
            if (PopupMenuButton("Resume", ICON_FA_CIRCLE_PLAY)) {
                ActionTogglePause();
                refocus_first_item = true;
            }
        }
        if (PopupMenuButton("Screenshot", ICON_FA_CAMERA)) {
            ActionScreenshot();
            pop = true;
        }
        if (PopupMenuButton("Save Snapshot", ICON_FA_DOWNLOAD)) {
            xemu_snapshots_save(NULL, NULL);
            xemu_queue_notification("Created new snapshot");
            pop = true;
        }
        if (PopupMenuSubmenuButton("Games", ICON_FA_GAMEPAD)) {
            nav.PushFocus();
            nav.PushMenu(games);
        }
        if (PopupMenuButton("Eject Disc", ICON_FA_EJECT)) {
            ActionEjectDisc();
            pop = true;
        }
        if (PopupMenuButton("Load Disc...", ICON_FA_COMPACT_DISC)) {
            ActionLoadDisc();
            pop = true;
        }
        if (PopupMenuSubmenuButton("Settings", ICON_FA_GEARS)) {
            nav.PushFocus();
            nav.PushMenu(settings);
        }
        if (PopupMenuSubmenuButton("View", ICON_FA_TV)) {
            nav.PushFocus();
            nav.PushMenu(view_menu);
        }
        if (PopupMenuSubmenuButton("Debug", ICON_FA_BUG)) {
            nav.PushFocus();
            nav.PushMenu(debug_menu);
        }
        if (PopupMenuSubmenuButton("Help", ICON_FA_CIRCLE_QUESTION)) {
            nav.PushFocus();
            nav.PushMenu(help_menu);
        }
        if (PopupMenuButton("Restart", ICON_FA_ARROWS_ROTATE)) {
            ActionReset();
            pop = true;
        }
        if (PopupMenuButton("Exit", ICON_FA_POWER_OFF)) {
            ActionShutdown();
            pop = true;
        }

        if (m_pop_focus) {
            nav.PopFocus();
        }

        return pop;
    }
};

RootPopupMenu root_menu;

void PopupMenuScene::PushMenu(PopupMenu &menu)
{
    menu.Show(m_view_stack.size() ? EASE_VECTOR_LEFT : EASE_VECTOR_DOWN);
    m_menus_in_transition.push_back(&menu);

    if (m_view_stack.size()) {
        auto current = m_view_stack.back();
        m_menus_in_transition.push_back(current);
        current->Hide(EASE_VECTOR_RIGHT);
    }

    m_view_stack.push_back(&menu);
}

void PopupMenuScene::PopMenu()
{
    if (!m_view_stack.size()) {
        return;
    }

    if (m_view_stack.size() > 1) {
        auto previous = m_view_stack[m_view_stack.size() - 2];
        previous->Show(EASE_VECTOR_RIGHT);
        previous->InitFocus();
        m_menus_in_transition.push_back(previous);
    }

    auto current = m_view_stack.back();
    m_view_stack.pop_back();
    current->Hide(m_view_stack.size() ? EASE_VECTOR_LEFT : EASE_VECTOR_DOWN);
    m_menus_in_transition.push_back(current);

    if (!m_view_stack.size()) {
        Hide();
    }
}

void PopupMenuScene::PushFocus()
{
    ImGuiContext *g = ImGui::GetCurrentContext();
    m_focus_stack.push_back(std::pair<ImGuiID, ImRect>(g->LastItemData.ID,
                                                       g->LastItemData.Rect));
}

void PopupMenuScene::PopFocus()
{
    auto next_focus = m_focus_stack.back();
    m_focus_stack.pop_back();
    ImGuiContext *g = ImGui::GetCurrentContext();
    g->NavInitRequest = false;
    g->NavInitResult.ID = next_focus.first;
    g->NavInitResult.RectRel = ImGui::WindowRectAbsToRel(g->CurrentWindow,
                                                         next_focus.second);
    // ImGui::NavUpdateAnyRequestFlag();
    g->NavAnyRequest = g->NavMoveScoringItems || g->NavInitRequest;// || (IMGUI_DEBUG_NAV_SCORING && g->NavWindow != NULL);
}

void PopupMenuScene::ClearMenuStack()
{
    if (m_view_stack.size()) {
        auto current = m_view_stack.back();
        current->Hide(EASE_VECTOR_DOWN);
        m_menus_in_transition.push_back(current);
    }
    m_view_stack.clear();
    m_focus_stack.clear();
    Hide();
}

void PopupMenuScene::HandleInput()
{
    if (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)
        || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        PopMenu();
    }
}

void PopupMenuScene::Show()
{
    m_background.Show();
    m_nav_control_view.Show();
    // m_big_state_icon.Show();
    // m_title_info.Show();

    if (m_view_stack.size() == 0) {
        PushMenu(root_menu);
    }
}

void PopupMenuScene::Hide()
{
    m_background.Hide();
    m_nav_control_view.Hide();
    // m_big_state_icon.Hide();
    // m_title_info.Hide();
}

bool PopupMenuScene::IsAnimating()
{
    return m_menus_in_transition.size() > 0 ||
           m_background.IsAnimating() ||
           m_nav_control_view.IsAnimating();
    // m_big_state_icon.IsAnimating() ||
    // m_title_info.IsAnimating();
}

bool PopupMenuScene::Draw()
{
    m_background.Draw();
    // m_big_state_icon.Draw();
    // m_title_info.Draw();

    bool displayed = false;
    while (m_menus_in_transition.size()) {
        auto current = m_menus_in_transition.back();
        if (current->IsAnimating()) {
            current->Draw(*this);
            displayed = true;
            break;
        }
        m_menus_in_transition.pop_back();
    }

    if (!displayed) {
        if (m_view_stack.size()) {
            m_view_stack.back()->Draw(*this);
            HandleInput();
            displayed = true;
        }
    }

    m_nav_control_view.Draw();
    return displayed || IsAnimating();
}

void PopupMenuScene::LostFocus()
{
    ClearMenuStack();
}

PopupMenuScene g_popup_menu;
