//
// xemu Debug Tools mixed-selection checkbox helper
//
// Copyright (C) 2026 xemu contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
#pragma once

#include "../common.hh"

namespace XemuDebugUi {

inline bool MixedCheckbox(const char *label, bool mixed, bool *value)
{
    const bool changed = ImGui::Checkbox(label, value);
    if (mixed) {
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const float height = max.y - min.y;
        const float y = min.y + height * 0.5f;
        const float pad = height * 0.28f;
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(min.x + pad, y), ImVec2(min.x + height - pad, y),
            ImGui::GetColorU32(ImGuiCol_CheckMark), 2.0f);
    }
    return changed;
}

} // namespace XemuDebugUi
