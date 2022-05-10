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
#include "common.hh"
#include "main-menu.hh"
#include "menubar.hh"
#include "misc.hh"
#include "widgets.hh"
#include "monitor.hh"
#include "debug.hh"
#include "actions.hh"
#include "compat.hh"
#include "update.hh"
#include "../xemu-os-utils.h"

extern float g_main_menu_height; // FIXME

#ifdef CONFIG_RENDERDOC
bool g_capture_renderdoc_frame = false;
#endif

#if defined(__APPLE__)
#define SHORTCUT_MENU_TEXT(c) "Cmd+" #c
#else
#define SHORTCUT_MENU_TEXT(c) "Ctrl+" #c
#endif

void ProcessKeyboardShortcuts(void)
{
    if (IsShortcutKeyPressed(ImGuiKey_E)) {
        ActionEjectDisc();
    }

    if (IsShortcutKeyPressed(ImGuiKey_O)) {
        ActionLoadDisc();
    }

    if (IsShortcutKeyPressed(ImGuiKey_P)) {
        ActionTogglePause();
    }

    if (IsShortcutKeyPressed(ImGuiKey_R)) {
        ActionReset();
    }

    if (IsShortcutKeyPressed(ImGuiKey_Q)) {
        ActionShutdown();
    }

    if (ImGui::IsKeyPressed(ImGuiKey_GraveAccent)) {
        monitor_window.ToggleOpen();
    }

    if (ImGui::IsKeyPressed(ImGuiKey_F12)) {
        ActionScreenshot();
    }

#if defined(DEBUG_NV2A_GL) && defined(CONFIG_RENDERDOC)
    if (ImGui::IsKeyPressed(ImGuiKey_F10)) {
        nv2a_dbg_renderdoc_capture_frames(1);
    }
#endif
}

void ShowMainMenu()
{
    bool running = runstate_is_running();

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Machine"))
        {
            if (ImGui::MenuItem(running ? "Pause" : "Resume", SHORTCUT_MENU_TEXT(P))) ActionTogglePause();
            if (ImGui::MenuItem("Screenshot", "F12")) ActionScreenshot();

            ImGui::Separator();

            if (ImGui::MenuItem("Eject Disc", SHORTCUT_MENU_TEXT(E))) ActionEjectDisc();
            if (ImGui::MenuItem("Load Disc...", SHORTCUT_MENU_TEXT(O))) ActionLoadDisc();

            ImGui::Separator();

            ImGui::MenuItem("Settings", NULL, false, false);
            if (ImGui::MenuItem(" General")) g_main_menu.ShowGeneral();
            if (ImGui::MenuItem(" Input")) g_main_menu.ShowInput();
            if (ImGui::MenuItem(" Display")) g_main_menu.ShowDisplay();
            if (ImGui::MenuItem(" Audio")) g_main_menu.ShowAudio();
            if (ImGui::MenuItem(" Network")) g_main_menu.ShowNetwork();
            if (ImGui::MenuItem(" System")) g_main_menu.ShowSystem();

            ImGui::Separator();

            if (ImGui::MenuItem("Reset", SHORTCUT_MENU_TEXT(R))) ActionReset();
            if (ImGui::MenuItem("Exit", SHORTCUT_MENU_TEXT(Q))) ActionShutdown();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            int ui_scale_idx;
            if (g_config.display.ui.auto_scale) {
                ui_scale_idx = 0;
            } else {
                ui_scale_idx = g_config.display.ui.scale;
                if (ui_scale_idx < 0) ui_scale_idx = 0;
                else if (ui_scale_idx > 2) ui_scale_idx = 2;
            }
            if (ImGui::Combo("UI Scale", &ui_scale_idx,
                             "Auto\0"
                             "1x\0"
                             "2x\0")) {
                if (ui_scale_idx == 0) {
                    g_config.display.ui.auto_scale = true;
                } else {
                    g_config.display.ui.auto_scale = false;
                    g_config.display.ui.scale = ui_scale_idx;
                }
            }
            int rendering_scale = nv2a_get_surface_scale_factor() - 1;
            if (ImGui::Combo("Int. Resolution Scale", &rendering_scale,
                             "1x\0"
                             "2x\0"
                             "3x\0"
                             "4x\0"
                             "5x\0"
                             "6x\0"
                             "7x\0"
                             "8x\0"
                             "9x\0"
                             "10x\0")) {
                nv2a_set_surface_scale_factor(rendering_scale + 1);
            }

            ImGui::Combo("Display Mode", &g_config.display.ui.fit,
                         "Center\0Scale\0Scale (Widescreen 16:9)\0Scale "
                         "(4:3)\0Stretch\0");
            ImGui::SameLine();
            HelpMarker("Controls how the rendered content should be scaled "
                       "into the window");
            if (ImGui::MenuItem("Fullscreen", SHORTCUT_MENU_TEXT(Alt + F),
                                xemu_is_fullscreen(), true)) {
                xemu_toggle_fullscreen();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Debug"))
        {
            ImGui::MenuItem("Monitor", "~", &monitor_window.is_open);
            ImGui::MenuItem("Audio", NULL, &apu_window.m_is_open);
            ImGui::MenuItem("Video", NULL, &video_window.m_is_open);
#if defined(DEBUG_NV2A_GL) && defined(CONFIG_RENDERDOC)
            if (nv2a_dbg_renderdoc_available()) {
                ImGui::MenuItem("RenderDoc: Capture", NULL, &g_capture_renderdoc_frame);
            }
#endif
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("Help", NULL)) {
                xemu_open_web_browser("https://xemu.app/docs/getting-started/");
            }

            ImGui::MenuItem("Report Compatibility...", NULL,
                            &compatibility_reporter_window.is_open);
#if defined(_WIN32)
            ImGui::MenuItem("Check for Updates...", NULL, &update_window.is_open);
#endif

            ImGui::Separator();
            if (ImGui::MenuItem("About")) g_main_menu.ShowAbout();
            ImGui::EndMenu();
        }

        g_main_menu_height = ImGui::GetWindowHeight();
        ImGui::EndMainMenuBar();
    }
}
