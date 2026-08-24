// v2.87 current regression ownership.
#include <cassert>

#include "tab-style.hh"

int ImGui::g_style_color_depth = 0;

int main()
{
    {
        XemuDebugUi::ScopedTabStyle style;
        assert(ImGui::g_style_color_depth == 5);
        style.Restore();
        assert(ImGui::g_style_color_depth == 0);
        style.Restore();
        assert(ImGui::g_style_color_depth == 0);
    }
    assert(ImGui::g_style_color_depth == 0);

    {
        XemuDebugUi::ScopedTabStyle style;
        assert(ImGui::g_style_color_depth == 5);
    }
    assert(ImGui::g_style_color_depth == 0);
    return 0;
}
