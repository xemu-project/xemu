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
#include "popup-menu-draw.hh"
#include "font-manager.hh"
#include "misc.hh"
#include "widgets.hh"
#include "IconsFontAwesome6.h"

bool PopupMenuButton(std::string text, std::string icon)
{
    ImGui::PushFont(g_font_mgr.m_menu_font);
    auto button_text = string_format("%s %s", icon.c_str(), text.c_str());
    bool status = ImGui::Button(button_text.c_str(), ImVec2(-FLT_MIN, 0));
    ImGui::PopFont();
    return status;
}

bool PopupMenuCheck(std::string text, std::string icon, bool v)
{
    bool status = PopupMenuButton(text, icon);
    if (v) {
        ImGui::PushFont(g_font_mgr.m_menu_font);
        const ImVec2 p0 = ImGui::GetItemRectMin();
        const ImVec2 p1 = ImGui::GetItemRectMax();
        const char *check_icon = ICON_FA_CHECK;
        ImVec2 ts_icon = ImGui::CalcTextSize(check_icon);
        ImDrawList *draw_list = ImGui::GetWindowDrawList();
        ImGuiStyle &style = ImGui::GetStyle();
        draw_list->AddText(ImVec2(p1.x - style.FramePadding.x - ts_icon.x,
                                  p0.y + (p1.y - p0.y - ts_icon.y) / 2),
                           ImGui::GetColorU32(ImGuiCol_Text), check_icon);
        ImGui::PopFont();
    }
    return status;
}

bool PopupMenuSubmenuButton(std::string text, std::string icon)
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

bool PopupMenuToggle(std::string text, std::string icon, bool *v)
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

bool PopupMenuSlider(std::string text, std::string icon, float *v)
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
