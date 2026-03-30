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
#include "menu-system.hh"
#include "actions.hh"
#include "common.hh"
#include "compat.hh"
#include "debug.hh"
#include "main-menu.hh"
#include "monitor.hh"
#include "popup-menu.hh"
#include "popup-menu-draw.hh"
#include "scene-manager.hh"
#include "widgets.hh"
#include "../xemu-os-utils.h"
#include "../xemu-snapshots.h"
#include "IconsFontAwesome6.h"

#ifdef CONFIG_RENDERDOC
#include "hw/xbox/nv2a/debug.h"
#include "menubar.hh"
#endif

#if defined(_WIN32)
#include "update.hh"
#endif

extern MainMenuScene g_main_menu;

void MenuDrawSnapshotSubmenu()
{
    if (PopupMenuButton("Create Snapshot", ICON_FA_DOWNLOAD)) {
        xemu_snapshots_save(NULL, NULL);
        xemu_queue_notification("Created new snapshot");
    }

    for (int i = 0; i < 4; ++i) {
        char *hotkey = g_strdup_printf("Shift+F%d", i + 5);

        char *load_name;
        char *save_name;

        assert(g_snapshot_shortcut_index_key_map[i]);
        bool bound = *(g_snapshot_shortcut_index_key_map[i]) &&
                (**(g_snapshot_shortcut_index_key_map[i]) != 0);

        if (bound) {
            load_name = g_strdup_printf("Load '%s'", *(g_snapshot_shortcut_index_key_map[i]));
            save_name = g_strdup_printf("Save '%s'", *(g_snapshot_shortcut_index_key_map[i]));
        } else {
            load_name = g_strdup_printf("Load F%d (Unbound)", i + 5);
            save_name = g_strdup_printf("Save F%d (Unbound)", i + 5);
        }

        ImGui::Separator();

        if (PopupMenuButton(load_name, ICON_FA_FOLDER_OPEN)) {
            if (bound) {
                ActionActivateBoundSnapshot(i, false);
            }
        }
        if (PopupMenuButton(save_name, ICON_FA_FLOPPY_DISK)) {
            if (bound) {
                ActionActivateBoundSnapshot(i, true);
            }
        }

        g_free(hotkey);
        g_free(load_name);
        g_free(save_name);
    }
}

void MenuDrawViewItems(bool for_menubar)
{
    if (!for_menubar) {
        PopupMenuSlider("Volume", ICON_FA_VOLUME_HIGH, &g_config.audio.volume_limit);
    }

    int ui_scale_idx;
    if (g_config.display.ui.auto_scale) {
        ui_scale_idx = 0;
    } else {
        ui_scale_idx = g_config.display.ui.scale;
        if (ui_scale_idx < 0) ui_scale_idx = 0;
        else if (ui_scale_idx > 2) ui_scale_idx = 2;
    }
    const char *ui_scale_id = for_menubar ? "UI Scale" : "##ui_scale";
    if (ImGui::Combo(ui_scale_id, &ui_scale_idx,
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

    const char *backend_id = for_menubar ? "Backend" : "##backend";
    ImGui::Combo(backend_id, &g_config.display.renderer,
         "Null\0"
         "OpenGL\0"
#ifdef CONFIG_VULKAN
         "Vulkan\0"
#endif
        );

    int rendering_scale = nv2a_get_surface_scale_factor() - 1;
    const char *ires_id = for_menubar ? "Int. Resolution Scale" : "##int_res_scale";
    if (ImGui::Combo(ires_id, &rendering_scale,
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

    const char *fit_id = for_menubar ? "Display Mode" : "##display_mode";
    ImGui::Combo(fit_id, &g_config.display.ui.fit,
                 "Center\0Scale\0Stretch\0");
    if (for_menubar) {
        ImGui::SameLine();
        HelpMarker("Controls how the rendered content should be scaled "
                   "into the window");
    }
    const char *filter_id = for_menubar ? "Filter Method" : "##filter_method";
    ImGui::Combo(filter_id, &g_config.display.filtering,
                 "Linear\0Nearest\0");
    const char *ar_id = for_menubar ? "Aspect Ratio" : "##aspect_ratio";
    ImGui::Combo(ar_id, &g_config.display.ui.aspect_ratio,
                 "Native\0Auto\0""4:3\0""16:9\0");
    if (for_menubar) {
        if (ImGui::MenuItem("Fullscreen", "F11",
                            xemu_is_fullscreen(), true)) {
            xemu_toggle_fullscreen();
        }
    } else {
        bool fs = xemu_is_fullscreen();
        if (PopupMenuToggle("Fullscreen", ICON_FA_WINDOW_MAXIMIZE, &fs)) {
            xemu_toggle_fullscreen();
        }
    }
}

void MenuDrawDebugItems(bool for_menubar)
{
    const char *mon_id = for_menubar ? "Monitor" : "##debug_monitor";
    ImGui::MenuItem(mon_id, "~", &monitor_window.is_open);
    const char *apu_id = for_menubar ? "Audio" : "##debug_audio";
    ImGui::MenuItem(apu_id, NULL, &apu_window.m_is_open);
    const char *vid_id = for_menubar ? "Video" : "##debug_video";
    ImGui::MenuItem(vid_id, NULL, &video_window.m_is_open);
#ifdef CONFIG_RENDERDOC
    if (nv2a_dbg_renderdoc_available()) {
        const char *rd_id = for_menubar ? "RenderDoc: Capture" : "##renderdoc_capture";
        ImGui::MenuItem(rd_id, NULL, &g_capture_renderdoc_frame);
    }
#endif
}

void MenuDrawHelpItems(bool for_menubar, PopupMenuItemDelegate *nav)
{
    if (for_menubar) {
        if (ImGui::MenuItem("Help", NULL)) {
            SDL_OpenURL("https://xemu.app/docs/getting-started/");
        }
    } else if (PopupMenuButton("Help", ICON_FA_CIRCLE_QUESTION)) {
        if (nav) {
            nav->ClearMenuStack();
        }
        SDL_OpenURL("https://xemu.app/docs/getting-started/");
    }

    const char *compat_id = for_menubar ? "Report Compatibility..." : "##compat_report";
    ImGui::MenuItem(compat_id, NULL,
                    &compatibility_reporter_window.is_open);
#if defined(_WIN32)
    const char *upd_id = for_menubar ? "Check for Updates..." : "##check_updates";
    ImGui::MenuItem(upd_id, NULL, &update_window.is_open);
#endif

    ImGui::Separator();
    if (for_menubar) {
        if (ImGui::MenuItem("About")) {
            g_main_menu.ShowAbout();
        }
    } else if (PopupMenuButton("About", ICON_FA_CIRCLE_INFO)) {
        if (nav) {
            nav->ClearMenuStack();
        }
        g_scene_mgr.PushScene(g_main_menu);
        g_main_menu.ShowAbout();
    }
}
