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
#include "scene-components.hh"
#include "common.hh"
#include "misc.hh"
#include "font-manager.hh"
#include "input-manager.hh"
#include "viewport-manager.hh"

BackgroundGradient::BackgroundGradient()
: m_animation(0.2, 0.2) {}

void BackgroundGradient::Show()
{
    m_animation.EaseIn();
}

void BackgroundGradient::Hide()
{
    m_animation.EaseOut();
}

bool BackgroundGradient::IsAnimating()
{
    return m_animation.IsAnimating();
}

void BackgroundGradient::Draw()
{
    m_animation.Step();

    float a = m_animation.GetSinInterpolatedValue();
    ImU32 top_color = ImGui::GetColorU32(ImVec4(0,0,0,a));
    ImU32 bottom_color = ImGui::GetColorU32(ImVec4(0,0,0,fmax(0, fmin(a-0.125, 0.125))));

    ImGuiIO &io = ImGui::GetIO();
    auto dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilledMultiColor(ImVec2(0, 0), io.DisplaySize, top_color, top_color, bottom_color, bottom_color);
}

NavControlItem::NavControlItem(std::string icon, std::string text)
: m_icon(icon), m_text(text) {}

void NavControlItem::Draw()
{
    ImGui::PushFont(g_font_mgr.m_menu_font_small);
    auto text = string_format("%s %s", m_icon.c_str(), m_text.c_str());
    ImGui::Text("%s", text.c_str());
    ImGui::PopFont();
}

NavControlAnnotation::NavControlAnnotation()
: m_animation(0.12,0.12)
{
    m_show = false;
    m_visible = false;

    // FIXME: Based on controller input type, display different icons. Currently
    // only showing Xbox scheme
    // FIXME: Support configuration of displayed items
    m_items.push_back(NavControlItem(ICON_BUTTON_A, "SELECT"));
    m_items.push_back(NavControlItem(ICON_BUTTON_B, "BACK"));
}

void NavControlAnnotation::Show()
{
    m_show = true;
}

void NavControlAnnotation::Hide()
{
    m_show = false;
}

bool NavControlAnnotation::IsAnimating()
{
    return m_animation.IsAnimating();
}

void NavControlAnnotation::Draw()
{
    if (g_input_mgr.IsNavigatingWithController() && m_show && !m_visible) {
        m_animation.EaseIn();
        m_visible = true;
    } else if ((!g_input_mgr.IsNavigatingWithController() || !m_show) &&
               m_visible) {
        m_animation.EaseOut();
        m_visible = false;
    }

    m_animation.Step();
    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowBgAlpha(0);
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x - g_viewport_mgr.GetExtents().z,
               io.DisplaySize.y - g_viewport_mgr.GetExtents().w),
        ImGuiCond_Always, ImVec2(1, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                        m_animation.GetSinInterpolatedValue());
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(30, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0.5));
    if (ImGui::Begin("###NavControlAnnotation", NULL,
                     ImGuiWindowFlags_NoDecoration |
                         ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoFocusOnAppearing |
                         ImGuiWindowFlags_NoInputs)) {
        int i = 0;
        for (auto &button : m_items) {
            if (i++) ImGui::SameLine();
            button.Draw();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(6);
}

#if 0
class BigStateIcon {
protected:
    EasingAnimation m_animation;

public:
    BigStateIcon()
    : m_animation(0.5, 0.15)
    {
    }

    void Show() {
        m_animation.easeIn();
    }

    void Hide() {
        m_animation.easeOut();
    }

    bool IsAnimating()
    {
        return m_animation.IsAnimating();
    }

    void Draw()
    {
        m_animation.step();
        ImGuiIO &io = ImGui::GetIO();
        ImGui::SetNextWindowBgAlpha(0);
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - g_viewport_mgr.getExtents().z, g_viewport_mgr.getExtents().y),
                                ImGuiCond_Always, ImVec2(1, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_animation.getSinInterpolatedValue());
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10*g_viewport_mgr.m_scale, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        if (ImGui::Begin("###BigStateIcon", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::PushFont(g_font_mgr.m_bigStateIconFont);
            ImGui::Text("%s", ICON_FA_PAUSE);
            ImGui::PopFont();
        }
        ImGui::End();
        ImGui::PopStyleVar(4);
    }
};

class TitleInfo
{
protected:
    GLuint screenshot;
    ImVec2 size;
    EasingAnimation m_animation;

public:
    TitleInfo()
    : m_animation(0.2, 0.2)
    {
        screenshot = 0;
    }

    void Show()
    {
        m_animation.easeIn();
    }

    void Hide()
    {
        m_animation.easeOut();
    }

    bool IsAnimating()
    {
        return m_animation.IsAnimating();
    }

    void initScreenshot()
    {
        if (screenshot == 0) {
            glGenTextures(1, &screenshot);
            int w, h, n;
            stbi_set_flip_vertically_on_load(0);
            unsigned char *data = stbi_load("./data/cover_front.jpg", &w, &h, &n, 4);
            assert(data);
            assert(n == 4 || n == 3);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, screenshot);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL,  0);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);

            // Fix width
            float width = 100;
            float height = width*h/w;
            size = ImVec2(width, height);
        }
    }

    void Draw()
    {
        initScreenshot();
        m_animation.step();

        ImGui::SetNextWindowSize(g_viewport_mgr.scale(ImVec2(600, 600)));
        ImGui::SetNextWindowBgAlpha(0);
        ImGui::SetNextWindowPos(ImVec2(g_viewport_mgr.getExtents().x,
                                       g_viewport_mgr.getExtents().y),
                                ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, m_animation.getSinInterpolatedValue());
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0.5));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, g_viewport_mgr.m_scale*6);
        if (ImGui::Begin("###TitleInfo", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::Columns(2, NULL, false);
            ImGuiStyle &style = ImGui::GetStyle();
            ImVec2 scaled_size = g_viewport_mgr.scale(size);
            ImGui::SetColumnWidth(0, scaled_size.x + style.ItemSpacing.x);
            ImGui::Dummy(scaled_size);
            ImVec2 p0 = ImGui::GetItemRectMin();
            ImVec2 p1 = ImGui::GetItemRectMax();
            ImDrawList *draw_list = ImGui::GetWindowDrawList();
            draw_list->AddImageRounded((ImTextureID)screenshot, p0, p1, ImVec2(0, 0), ImVec2(1, 1), ImGui::GetColorU32(ImVec4(1,1,1,m_animation.getSinInterpolatedValue())), 3*g_viewport_mgr.m_scale);

            ImGui::NextColumn();

            ImGui::PushFont(g_font_mgr.m_menuFont);
            ImGui::Text("Halo: Combat Evolved");
            ImGui::PopFont();
            ImGui::PushFont(g_font_mgr.m_menuFontSmall);
            ImGui::Text("NTSC MS-004");
            ImGui::PopFont();
            ImGui::Columns(1);
        }
        ImGui::End();
        ImGui::PopStyleVar(6);
    }
};
#endif
