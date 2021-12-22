#ifndef XEMU_NV2A_DEBUGGER_H
#define XEMU_NV2A_DEBUGGER_H

extern "C" {
#include "hw/xbox/nv2a/debug.h"
}

#ifdef ENABLE_NV2A_DEBUGGER
struct ImFont;
struct ImGuiIO;

class NV2ADebugger
{
public:
    bool is_open;

    NV2ADebugger();
    void Draw(ImFont *fixed_width_font, float ui_scale, float main_menu_height);

private:
    bool initialized;
    struct decal_shader* shader;
    float texture_debugger_clear_color[3];

    void Initialize();

    void DrawDebuggerControls(ImGuiIO& io,
                              ImFont *fixed_width_font,
                              float ui_scale,
                              float main_menu_height);
    void DrawLastDrawInfoOverlay(ImGuiIO& io,
                                 ImFont *fixed_width_font,
                                 float ui_scale,
                                 float main_menu_height);
    void DrawTextureOverlay(ImGuiIO& io,
                            ImFont *fixed_width_font,
                            float ui_scale,
                            float main_menu_height);
    void DrawSavedBackbufferOverlay(ImGuiIO& io,
                                    float ui_scale,
                                    float main_menu_height);
    void DrawInstanceRamHashTableOverlay(ImGuiIO& io,
                                         ImFont *fixed_width_font,
                                         float ui_scale,
                                         float main_menu_height);

    void StoreBackbuffer(ImGuiIO& io);
};
#endif // ENABLE_NV2A_DEBUGGER

#endif // XEMU_NV2A_DEBUGGER_H
