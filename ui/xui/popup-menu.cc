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
#include "misc.hh"
#include "actions.hh"
#include "font-manager.hh"
#include "viewport-manager.hh"
#include "scene-manager.hh"
#include "popup-menu.hh"
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

bool PopupMenuButton(std::string text, std::string icon = "")
{
    ImGui::PushFont(g_font_mgr.m_menu_font);
    auto button_text = string_format("%s %s", icon.c_str(), text.c_str());
    bool status = ImGui::Button(button_text.c_str(), ImVec2(-FLT_MIN, 0));
    ImGui::PopFont();
    return status;
}

bool PopupMenuCheck(std::string text, std::string icon = "", bool v = false)
{
    bool status = PopupMenuButton(text, icon);
    if (v) {
        ImGui::PushFont(g_font_mgr.m_menu_font);
        const ImVec2 p0 = ImGui::GetItemRectMin();
        const ImVec2 p1 = ImGui::GetItemRectMax();
        const char *icon = ICON_FA_CHECK;
        ImVec2 ts_icon = ImGui::CalcTextSize(icon);
        ImDrawList *draw_list = ImGui::GetWindowDrawList();
        ImGuiStyle &style = ImGui::GetStyle();
        draw_list->AddText(ImVec2(p1.x - style.FramePadding.x - ts_icon.x,
                                  p0.y + (p1.y - p0.y - ts_icon.y) / 2),
                           ImGui::GetColorU32(ImGuiCol_Text), icon);
        ImGui::PopFont();
    }
    return status;
}

bool PopupMenuSubmenuButton(std::string text, std::string icon = "")
{
    bool status = PopupMenuButton(text, icon);

    ImGui::PushFont(g_font_mgr.m_menu_font);
    const ImVec2 p0 = ImGui::GetItemRectMin();
    const ImVec2 p1 = ImGui::GetItemRectMax();
    const char *right_icon = ICON_FA_CHEVRON_RIGHT;
    ImVec2 ts_icon = ImGui::CalcTextSize(right_icon);
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    ImGuiStyle &style = ImGui::GetStyle();
    draw_list->AddText(ImVec2(p1.x - style.FramePadding.x - ts_icon.x,
                              p0.y + (p1.y - p0.y - ts_icon.y) / 2),
                       ImGui::GetColorU32(ImGuiCol_Text), right_icon);
    ImGui::PopFont();
    return status;
}

bool PopupMenuToggle(std::string text, std::string icon = "", bool *v = nullptr)
{
    bool l_v = false;
    if (v == NULL) v = &l_v;

    ImGuiStyle &style = ImGui::GetStyle();
    bool status = PopupMenuButton(text, icon);
    ImVec2 p_min = ImGui::GetItemRectMin();
    ImVec2 p_max = ImGui::GetItemRectMax();
    if (status) *v = !*v;

    ImGui::PushFont(g_font_mgr.m_menu_font);
    float title_height = ImGui::GetTextLineHeight();
    ImGui::PopFont();

    float toggle_height = title_height * 0.75;
    ImVec2 toggle_size(toggle_height * 1.75, toggle_height);
    ImVec2 toggle_pos(p_max.x - toggle_size.x - style.FramePadding.x,
                      p_min.y + (title_height - toggle_size.y)/2 + style.FramePadding.y);
    DrawToggle(*v, ImGui::IsItemHovered(), toggle_pos, toggle_size);

    return status;
}

bool PopupMenuSlider(std::string text, std::string icon = "", float *v = NULL)
{
    bool status = PopupMenuButton(text, icon);
    ImVec2 p_min = ImGui::GetItemRectMin();
    ImVec2 p_max = ImGui::GetItemRectMax();

    ImGuiStyle &style = ImGui::GetStyle();

    float new_v = *v;

    if (ImGui::IsItemHovered()) {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) ||
            ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft) ||
            ImGui::IsKeyPressed(ImGuiKey_GamepadLStickLeft) ||
            ImGui::IsKeyPressed(ImGuiKey_GamepadRStickLeft)) new_v -= 0.05;
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) ||
            ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight) ||
            ImGui::IsKeyPressed(ImGuiKey_GamepadLStickRight) ||
            ImGui::IsKeyPressed(ImGuiKey_GamepadRStickRight)) new_v += 0.05;
    }

    ImGui::PushFont(g_font_mgr.m_menu_font);
    float title_height = ImGui::GetTextLineHeight();
    ImGui::PopFont();

    float toggle_height = title_height * 0.75;
    ImVec2 slider_size(toggle_height * 3.75, toggle_height);
    ImVec2 slider_pos(p_max.x - slider_size.x - style.FramePadding.x,
                      p_min.y + (title_height - slider_size.y)/2 + style.FramePadding.y);

    if (ImGui::IsItemActive()) {
        ImVec2 mouse = ImGui::GetMousePos();
        new_v = GetSliderValueForMousePos(mouse, slider_pos, slider_size);
    }

    DrawSlider(*v, ImGui::IsItemActive() || ImGui::IsItemHovered(), slider_pos,
               slider_size);

    *v = fmin(fmax(0, new_v), 1.0);

    return status;
}

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
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - ImGui::GetStyle().FramePadding.x * 2.0f - ImGui::GetTextLineHeight());
        if (ImGui::Button(ICON_FA_XMARK)) {
            nav.ClearMenuStack();
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

class DisplayModePopupMenu : public virtual PopupMenu {
public:
    bool DrawItems(PopupMenuItemDelegate &nav) override
    {
        const char *values[] = {
            "Center", "Scale", "Stretch"
        };

        for (int i = 0; i < CONFIG_DISPLAY_UI_FIT__COUNT; i++) {
            bool selected = g_config.display.ui.fit == i;
            if (m_focus && selected) ImGui::SetKeyboardFocusHere();
            if (PopupMenuCheck(values[i], "", selected))
                g_config.display.ui.fit = i;
        }

        return false;
    }
};

class AspectRatioPopupMenu : public virtual PopupMenu {
public:
    bool DrawItems(PopupMenuItemDelegate &nav) override
    {
        const char *values[] = {
            "Native",
            "Auto (Default)",
            "4:3",
            "16:9"
        };

        for (int i = 0; i < CONFIG_DISPLAY_UI_ASPECT_RATIO__COUNT; i++) {
            bool selected = g_config.display.ui.aspect_ratio == i;
            if (m_focus && selected) ImGui::SetKeyboardFocusHere();
            if (PopupMenuCheck(values[i], "", selected))
                g_config.display.ui.aspect_ratio = i;
        }

        return false;
    }
};

extern MainMenuScene g_main_menu;

class SettingsPopupMenu : public virtual PopupMenu {
protected:
    DisplayModePopupMenu display_mode;
    AspectRatioPopupMenu aspect_ratio;

public:
    bool DrawItems(PopupMenuItemDelegate &nav) override
    {
        bool pop = false;

        if (m_focus && !m_pop_focus) {
            ImGui::SetKeyboardFocusHere();
        }
        PopupMenuSlider("Volume", ICON_FA_VOLUME_HIGH, &g_config.audio.volume_limit);
        bool fs = xemu_is_fullscreen();
        if (PopupMenuToggle("Fullscreen", ICON_FA_WINDOW_MAXIMIZE, &fs)) {
            xemu_toggle_fullscreen();
        }
        if (PopupMenuSubmenuButton("Display Mode", ICON_FA_EXPAND)) {
            nav.PushFocus();
            nav.PushMenu(display_mode);
        }
        if (PopupMenuSubmenuButton("Aspect Ratio", ICON_FA_EXPAND)) {
            nav.PushFocus();
            nav.PushMenu(aspect_ratio);
        }
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

class RootPopupMenu : public virtual PopupMenu {
protected:
    SettingsPopupMenu settings;
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
