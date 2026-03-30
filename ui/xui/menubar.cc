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
#include "ui/xemu-notifications.h"
#include "common.hh"
#include "main-menu.hh"
#include "menubar.hh"
#include "misc.hh"
#include "monitor.hh"
#include "menu-system.hh"
#include "actions.hh"
#include "compat.hh"
#if defined(_WIN32)
#include "update.hh"
#endif
#include "../xemu-os-utils.h"
#ifdef CONFIG_RENDERDOC
#include "hw/xbox/nv2a/debug.h"
#endif

extern float g_main_menu_height; // Set in ShowMainMenu(), consumed by overlays

#ifdef CONFIG_RENDERDOC
bool g_capture_renderdoc_frame = false;
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

    if (ImGui::IsKeyPressed(ImGuiKey_F11)) {
        xemu_toggle_fullscreen();
    }

#ifdef CONFIG_RENDERDOC
    if (ImGui::IsKeyPressed(ImGuiKey_F10) && nv2a_dbg_renderdoc_available()) {
        ImGuiIO& io = ImGui::GetIO();
        int num_frames = io.KeyShift ? 5 : 1;
        nv2a_dbg_renderdoc_capture_frames(num_frames, io.KeyCtrl);
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
            if (ImGui::MenuItem(running ? "Pause" : "Resume", XEMU_MENU_KBD_SHORTCUT(P))) ActionTogglePause();
            if (ImGui::MenuItem("Screenshot", "F12")) ActionScreenshot();

            if (ImGui::BeginMenu("Snapshot")) {
                MenuDrawSnapshotSubmenu();
                ImGui::EndMenu();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Eject Disc", XEMU_MENU_KBD_SHORTCUT(E))) ActionEjectDisc();
            if (ImGui::MenuItem("Load Disc...", XEMU_MENU_KBD_SHORTCUT(O))) ActionLoadDisc();

            ImGui::Separator();

            if (ImGui::MenuItem("Settings...")) g_main_menu.ShowSettings();

            ImGui::Separator();

            if (ImGui::MenuItem("Reset", XEMU_MENU_KBD_SHORTCUT(R))) ActionReset();
            if (ImGui::MenuItem("Exit", XEMU_MENU_KBD_SHORTCUT(Q))) ActionShutdown();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            MenuDrawViewItems(true);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Debug"))
        {
            MenuDrawDebugItems(true);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help"))
        {
            MenuDrawHelpItems(true);
            ImGui::EndMenu();
        }

        g_main_menu_height = ImGui::GetWindowHeight();
        ImGui::EndMainMenuBar();
    }
}
