#pragma once

struct ImVec4
{
    float x, y, z, w;
    ImVec4(float x_, float y_, float z_, float w_)
        : x(x_), y(y_), z(z_), w(w_)
    {
    }
};

typedef int ImGuiCol;
enum ImGuiCol_
{
    ImGuiCol_Tab,
    ImGuiCol_TabHovered,
    ImGuiCol_TabActive,
    ImGuiCol_TabUnfocused,
    ImGuiCol_TabUnfocusedActive,
};

namespace ImGui {
extern int g_style_color_depth;

inline const ImVec4 &GetStyleColorVec4(ImGuiCol)
{
    static const ImVec4 value(0.1f, 0.2f, 0.3f, 1.0f);
    return value;
}

inline void PushStyleColor(ImGuiCol, const ImVec4 &)
{
    ++g_style_color_depth;
}

inline void PopStyleColor(int count = 1)
{
    g_style_color_depth -= count;
}
} // namespace ImGui
