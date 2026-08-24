//
// xemu Debug Tools tab styling
//
// Copyright (C) 2026 xemu contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
#pragma once

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include <imgui.h>

namespace XemuDebugUi {

class ScopedTabStyle
{
public:
    ScopedTabStyle()
    {
        /* Keep exactly three Debug Tools tab states without changing xemu's
         * global theme: inactive light grey, hovered steel blue, selected
         * current green. Unfocused tabs reuse the same inactive/selected
         * colors so focus changes do not create a fourth visual state. */
        const ImVec4 inactive(0.58f, 0.58f, 0.58f, 0.95f); // #949494
        const ImVec4 hovered(0.36f, 0.56f, 0.73f, 1.00f);  // steel blue
        const ImVec4 active = ImGui::GetStyleColorVec4(ImGuiCol_TabActive);
        ImGui::PushStyleColor(ImGuiCol_Tab, inactive);
        ImGui::PushStyleColor(ImGuiCol_TabHovered, hovered);
        ImGui::PushStyleColor(ImGuiCol_TabActive, active);
        ImGui::PushStyleColor(ImGuiCol_TabUnfocused, inactive);
        ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, active);
    }

    ~ScopedTabStyle()
    {
        Restore();
    }

    void Restore()
    {
        if (!m_active) {
            return;
        }
        ImGui::PopStyleColor(5);
        m_active = false;
    }

    ScopedTabStyle(const ScopedTabStyle &) = delete;
    ScopedTabStyle &operator=(const ScopedTabStyle &) = delete;

private:
    bool m_active = true;
};

} // namespace XemuDebugUi
