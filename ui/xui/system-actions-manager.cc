//
// xemu User Interface
//
// Copyright (C) 2026 Matt Borgerson
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
#include "system-actions-manager.hh"
#include "actions.hh"
#include "main-menu.hh"
#include "popup-menu.hh"
#include "monitor.hh"
#include "scene-manager.hh"
#include "xemu-hud.h"
#include "../xemu-settings.h"
#include "../xemu-input.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>

IMGUI_IMPL_API ImGuiKey
ImGui_ImplSDL3_KeyEventToImGuiKey(SDL_Keycode keycode, SDL_Scancode scancode);

void SystemActionsManager::InitInstance()
{
    m_bindings.clear();

    auto add_binding =
        [this](const char *name, const int &scancode, const bool &ctrl,
               const bool &shift, const bool &alt, const bool &super,
               std::vector<std::reference_wrapper<const int>> chords,
               std::function<void()> cb) {
            m_bindings.push_back({ name, std::cref(scancode), std::cref(ctrl),
                                   std::cref(shift), std::cref(alt),
                                   std::cref(super), std::move(chords), cb });
        };

#define ADD_ACTION(NAME, CALLBACK)                                            \
    add_binding(                                                              \
        #NAME, g_config.input.system_actions.NAME.scancode,                   \
        g_config.input.system_actions.NAME.ctrl,                              \
        g_config.input.system_actions.NAME.shift,                             \
        g_config.input.system_actions.NAME.alt,                               \
        g_config.input.system_actions.NAME.super,                             \
        { std::cref(g_config.input.system_actions.NAME.controller_chord_1),   \
          std::cref(g_config.input.system_actions.NAME.controller_chord_2) }, \
        CALLBACK)

    ADD_ACTION(toggle_pause, ActionTogglePause);
    ADD_ACTION(reset, ActionReset);
    ADD_ACTION(power_off, ActionShutdown);
    ADD_ACTION(eject_disc, ActionEjectDisc);
    ADD_ACTION(load_disc, ActionLoadDisc);
    ADD_ACTION(screenshot, ActionScreenshot);
    ADD_ACTION(toggle_fullscreen, xemu_toggle_fullscreen);
    ADD_ACTION(toggle_monitor, [] { monitor_window.ToggleOpen(); });
    auto is_ui_busy = [] {
        return ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow) ||
               g_scene_mgr.IsDisplayingScene();
    };

    ADD_ACTION(toggle_main_menu, [is_ui_busy] {
        if (!is_ui_busy()) {
            g_scene_mgr.PushScene(g_main_menu);
        }
    });
    ADD_ACTION(toggle_popup_menu, [is_ui_busy] {
        if (!is_ui_busy()) {
            g_scene_mgr.PushScene(g_popup_menu);
        }
    });
    ADD_ACTION(snapshot_load_1, [is_ui_busy] {
        if (!is_ui_busy()) {
            ActionActivateBoundSnapshot(0, false);
        }
    });
    ADD_ACTION(snapshot_load_2, [is_ui_busy] {
        if (!is_ui_busy()) {
            ActionActivateBoundSnapshot(1, false);
        }
    });
    ADD_ACTION(snapshot_load_3, [is_ui_busy] {
        if (!is_ui_busy()) {
            ActionActivateBoundSnapshot(2, false);
        }
    });
    ADD_ACTION(snapshot_load_4, [is_ui_busy] {
        if (!is_ui_busy()) {
            ActionActivateBoundSnapshot(3, false);
        }
    });
    ADD_ACTION(snapshot_save_1, [is_ui_busy] {
        if (!is_ui_busy()) {
            ActionActivateBoundSnapshot(0, true);
        }
    });
    ADD_ACTION(snapshot_save_2, [is_ui_busy] {
        if (!is_ui_busy()) {
            ActionActivateBoundSnapshot(1, true);
        }
    });
    ADD_ACTION(snapshot_save_3, [is_ui_busy] {
        if (!is_ui_busy()) {
            ActionActivateBoundSnapshot(2, true);
        }
    });
    ADD_ACTION(snapshot_save_4, [is_ui_busy] {
        if (!is_ui_busy()) {
            ActionActivateBoundSnapshot(3, true);
        }
    });

#undef ADD_ACTION

    m_prev_controller_buttons = 0;
    m_suppressed = false;
    m_initialized = true;
}

bool SystemActionsManager::HandleKeyboardShortcuts()
{
    if (!m_initialized || m_suppressed) {
        return false;
    }

    ImGuiIO &io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) {
        return false;
    }

    bool action_invoked = false;

    for (const auto &b : m_bindings) {
        if (b.scancode.get() == 0) {
            continue;
        }

        SDL_Keycode keycode = SDL_GetKeyFromScancode(
            (SDL_Scancode)b.scancode.get(), SDL_KMOD_NONE, false);
        ImGuiKey key = ImGui_ImplSDL3_KeyEventToImGuiKey(
            keycode, (SDL_Scancode)b.scancode.get());

        if (key == ImGuiKey_None || !ImGui::IsKeyPressed(key)) {
            continue;
        }

        if (b.ctrl.get() != io.KeyCtrl || b.shift.get() != io.KeyShift ||
            b.alt.get() != io.KeyAlt || b.super.get() != io.KeySuper) {
            continue;
        }

        if (b.callback) {
            b.callback();
            action_invoked = true;
        }
    }

    return action_invoked;
}

bool SystemActionsManager::HandleControllerShortcuts(unsigned int buttons)
{
    if (!m_initialized || m_suppressed) {
        return false;
    }

    bool action_invoked = false;

    for (const auto &b : m_bindings) {
        for (const auto &chord_ref : b.controller_chords) {
            auto chord_mask = static_cast<unsigned int>(chord_ref.get());

            if (chord_mask == 0) {
                continue;
            }

            bool chord_active = (buttons & chord_mask) == chord_mask;
            bool chord_was_active =
                (m_prev_controller_buttons & chord_mask) == chord_mask;

            if (chord_active && !chord_was_active) {
                if (b.callback) {
                    b.callback();
                    action_invoked = true;
                }
            }
        }
    }

    m_prev_controller_buttons = buttons;
    return action_invoked;
}
