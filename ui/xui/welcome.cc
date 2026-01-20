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
    if (!ImGui::Begin("First Boot", &is_open, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    Logo();

    const char *msg = "Configure machine settings to get started";
    ImGui::SetCursorPosX((ImGui::GetWindowWidth()-ImGui::CalcTextSize(msg).x)/2);
    ImGui::Text("%s", msg);

    ImGui::Dummy(ImVec2(0,20*g_viewport_mgr.m_scale));

    ImGui::SetCursorPosX((ImGui::GetWindowWidth()-120*g_viewport_mgr.m_scale)/2);
    if (ImGui::Button("Settings", ImVec2(120*g_viewport_mgr.m_scale, 0))) {
        g_main_menu.ShowSystem();
        g_config.general.show_welcome = false;
    }

    ImGui::Dummy(ImVec2(0,50*g_viewport_mgr.m_scale));

    msg = "Visit https://xemu.app for more information";
    ImGui::SetCursorPosX((ImGui::GetWindowWidth()-ImGui::CalcTextSize(msg).x)/2);
    Hyperlink(msg, "https://xemu.app");

    ImGui::Dummy(ImVec2(400*g_viewport_mgr.m_scale,20*g_viewport_mgr.m_scale));

    ImGui::End();
}

FirstBootWindow first_boot_window;
