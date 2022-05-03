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
#include "ui/xui/viewport-manager.hh"
#include "common.hh"
#include "imgui.h"
#include "viewport-manager.hh"
#include "welcome.hh"
#include "widgets.hh"
#include "misc.hh"
#include "gl-helpers.hh"
#include "xemu-version.h"
#include "main-menu.hh"

FirstBootWindow::FirstBootWindow()
{
    is_open = false;
}

void FirstBootWindow::Draw()
{
    if (!is_open) return;

    ImVec2 size(400*g_viewport_mgr.m_scale, 300*g_viewport_mgr.m_scale);
    ImGuiIO& io = ImGui::GetIO();

    ImVec2 window_pos = ImVec2((io.DisplaySize.x - size.x)/2, (io.DisplaySize.y - size.y)/2);
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always);

    ImGui::SetNextWindowSize(size, ImGuiCond_Appearing);
    if (!ImGui::Begin("First Boot", &is_open, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration)) {
        ImGui::End();
        return;
    }

    static uint32_t time_start = 0;
    if (ImGui::IsWindowAppearing()) {
        time_start = SDL_GetTicks();
    }
    uint32_t now = SDL_GetTicks() - time_start;

    ImGui::SetCursorPosY(ImGui::GetCursorPosY()-50*g_viewport_mgr.m_scale);
    ImGui::SetCursorPosX((ImGui::GetWindowWidth()-256*g_viewport_mgr.m_scale)/2);

    logo_fbo->Target();
    ImTextureID id = (ImTextureID)(intptr_t)logo_fbo->Texture();
    float t_w = 256.0;
    float t_h = 256.0;
    float x_off = 0;
    ImGui::Image(id,
        ImVec2((t_w-x_off)*g_viewport_mgr.m_scale, t_h*g_viewport_mgr.m_scale),
        ImVec2(x_off/t_w, t_h/t_h),
        ImVec2(t_w/t_w, 0));
    if (ImGui::IsItemClicked()) {
        time_start = SDL_GetTicks();
    }
    RenderLogo(now, 0x42e335ff, 0x42e335ff, 0x00000000);
    logo_fbo->Restore();

    ImGui::SetCursorPosY(ImGui::GetCursorPosY()-100*g_viewport_mgr.m_scale);
    ImGui::SetCursorPosX(10*g_viewport_mgr.m_scale);
    ImGui::Dummy(ImVec2(0,20*g_viewport_mgr.m_scale));

    const char *msg = "Configure machine settings to get started";
    ImGui::SetCursorPosX((ImGui::GetWindowWidth()-ImGui::CalcTextSize(msg).x)/2);
    ImGui::Text("%s", msg);

    ImGui::Dummy(ImVec2(0,20*g_viewport_mgr.m_scale));
    ImGui::SetCursorPosX((ImGui::GetWindowWidth()-120*g_viewport_mgr.m_scale)/2);
    ImGui::SetItemDefaultFocus();
    if (ImGui::Button("Settings", ImVec2(120*g_viewport_mgr.m_scale, 0))) {
        g_main_menu.ShowSystem();
        g_config.general.show_welcome = false;
    }
    ImGui::Dummy(ImVec2(0,20*g_viewport_mgr.m_scale));

    msg = "Visit https://xemu.app for more information";
    ImGui::SetCursorPosX((ImGui::GetWindowWidth()-ImGui::CalcTextSize(msg).x)/2);
    Hyperlink(msg, "https://xemu.app");

    ImGui::End();
}

FirstBootWindow first_boot_window;
