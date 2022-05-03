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
#include "viewport-manager.hh"

ViewportManager g_viewport_mgr;

ViewportManager::ViewportManager() {
    m_scale = 1;
    m_extents.x = 25 * m_scale; // Distance from Left
    m_extents.y = 25 * m_scale; // '' Top
    m_extents.z = 25 * m_scale; // '' Right
    m_extents.w = 25 * m_scale; // '' Bottom
}

ImVec4 ViewportManager::GetExtents()
{
    return m_extents;
}

#if 0
void ViewportManager::DrawExtents()
{
    ImGuiIO &io = ImGui::GetIO();
    ImVec2 tl(m_extents.x, m_extents.y);
    ImVec2 tr(io.DisplaySize.x - m_extents.z, m_extents.y);
    ImVec2 br(io.DisplaySize.x - m_extents.z, io.DisplaySize.y - m_extents.w);
    ImVec2 bl(m_extents.x, io.DisplaySize.y - m_extents.w);

    auto dl = ImGui::GetForegroundDrawList();
    ImU32 color = 0xffff00ff;
    dl->AddLine(tl, tr, color, 2.0);
    dl->AddLine(tr, br, color, 2.0);
    dl->AddLine(br, bl, color, 2.0);
    dl->AddLine(bl, tl, color, 2.0);
    dl->AddLine(tl, br, color, 2.0);
    dl->AddLine(bl, tr, color, 2.0);
}
#endif

ImVec2 ViewportManager::Scale(const ImVec2 vec2)
{
    return ImVec2(vec2.x * m_scale, vec2.y * m_scale);
}

void ViewportManager::Update()
{
    ImGuiIO &io = ImGui::GetIO();

    if (g_config.display.ui.auto_scale) {
        if (io.DisplaySize.x > 1920) {
            g_config.display.ui.scale = 2;
        } else {
            g_config.display.ui.scale = 1;
        }
    }

    m_scale = g_config.display.ui.scale;

    if (m_scale < 1) {
        m_scale = 1;
    } else if (m_scale > 2) {
        m_scale = 2;
    }

    if (io.DisplaySize.x > 640*m_scale) {
        m_extents.x = 25 * m_scale; // Distance from Left
        m_extents.y = 25 * m_scale; // '' Top
        m_extents.z = 25 * m_scale; // '' Right
        m_extents.w = 25 * m_scale; // '' Bottom
    } else {
        m_extents = ImVec4(0,0,0,0);
    }
}
