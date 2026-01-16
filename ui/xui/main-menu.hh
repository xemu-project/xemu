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
#pragma once
#include <string>
#include <vector>
#include <memory>
#include "common.hh"
#include "widgets.hh"
#include "scene.hh"
#include "scene-components.hh"
#include "../xemu-snapshots.h"
#include "../xemu-controllers.h"
#include "ui/xemu-input.h"

extern "C" {
#include "net/pcap.h"
#undef snprintf  // FIXME
}

class MainMenuTabView
{
public:
    virtual ~MainMenuTabView();
    virtual void Draw();
    virtual void Hide()
    {
    }
};

class MainMenuGeneralView : public virtual MainMenuTabView
{
public:
    void Draw() override;
};

class MainMenuInputView : public virtual MainMenuTabView
{
public:
    std::unique_ptr<RebindingMap> m_rebinding;

    MainMenuInputView() : m_rebinding{ nullptr }
    {
    }
    bool ConsumeRebindEvent(SDL_Event *event);
    bool IsInputRebinding();
    void Draw() override;
    void Hide() override;
    void PopulateTableController(ControllerState *state);
};

class MainMenuDisplayView : public virtual MainMenuTabView
{
public:
    void Draw() override;
};

class MainMenuAudioView : public virtual MainMenuTabView
{
public:
    void Draw() override;
};

class NetworkInterface
{
public:
    std::string m_pcap_name;
    std::string m_description;
    std::string m_friendly_name;

    NetworkInterface(pcap_if_t *pcap_desc, char *_friendlyname = NULL);
};

class NetworkInterfaceManager
{
public:
    std::vector<std::unique_ptr<NetworkInterface>> m_ifaces;
    NetworkInterface *m_current_iface;
    bool m_failed_to_load_lib;

    NetworkInterfaceManager();
    void Refresh(void);
    void Select(NetworkInterface &iface);
    bool IsCurrent(NetworkInterface &iface);
};

class MainMenuNetworkView : public virtual MainMenuTabView
{
protected:
    char remote_addr[64];
    char local_addr[64];
    bool should_refresh;
    std::unique_ptr<NetworkInterfaceManager> iface_mgr;

public:
    MainMenuNetworkView();
    void Draw() override;
    void DrawPcapOptions(bool appearing);
    void DrawNatOptions(bool appearing);
    void DrawUdpOptions(bool appearing);
};

class MainMenuSnapshotsView : public virtual MainMenuTabView
{
protected:
    GRegex *m_search_regex;
    uint32_t m_current_title_id;
    std::string m_current_title_name;
    std::string m_search_buf;

    void ClearSearch();
    void DrawSnapshotContextMenu(QEMUSnapshotInfo *snapshot, XemuSnapshotData *data, int current_snapshot_binding);
    bool BigSnapshotButton(QEMUSnapshotInfo *snapshot, XemuSnapshotData *data, int current_snapshot_binding);
    static int OnSearchTextUpdate(ImGuiInputTextCallbackData *data);

public:
    MainMenuSnapshotsView();
    ~MainMenuSnapshotsView();
    void Draw() override;

};

class MainMenuSystemView : public virtual MainMenuTabView
{
protected:
    bool m_dirty;

public:
    MainMenuSystemView();
    void Draw() override;
};

class MainMenuAboutView : public virtual MainMenuTabView
{
protected:
    char *m_config_info_text;
public:
    MainMenuAboutView();
    void UpdateConfigInfoText();
    void Draw() override;
};

class MainMenuTabButton
{
protected:
    std::string m_icon, m_text;

public:
    MainMenuTabButton(std::string text, std::string icon = "");
    bool Draw(bool selected);
};

class MainMenuScene : public virtual Scene {
protected:
    EasingAnimation                 m_animation;
    bool                            m_focus_view;
    bool                            m_had_focus_last_frame;
    int                             m_current_view_index;
    int                             m_next_view_index;
    BackgroundGradient              m_background;
    NavControlAnnotation            m_nav_control_view;
    std::vector<MainMenuTabButton*> m_tabs;
    MainMenuTabButton               m_general_button,
                                    m_input_button,
                                    m_display_button,
                                    m_audio_button,
                                    m_network_button,
                                    m_snapshots_button,
                                    m_system_button,
                                    m_about_button;
    std::vector<MainMenuTabView*>   m_views;
    MainMenuGeneralView             m_general_view;
    MainMenuInputView               m_input_view;
    MainMenuDisplayView             m_display_view;
    MainMenuAudioView               m_audio_view;
    MainMenuNetworkView             m_network_view;
    MainMenuSnapshotsView           m_snapshots_view;
    MainMenuSystemView              m_system_view;
    MainMenuAboutView               m_about_view;


public:
    MainMenuScene();
    void ShowSettings();
    void ShowSystem();
    void ShowAbout();
    void ShowSnapshots();
    void SetNextViewIndexWithFocus(int i);
    void Show() override;
    void Hide() override;
    bool IsAnimating() override;
    void SetNextViewIndex(int i);
    void HandleInput();
    void UpdateAboutViewConfigInfo();
    bool ConsumeRebindEvent(SDL_Event *event);
    bool IsInputRebinding();
    bool Draw() override;
};

extern MainMenuScene g_main_menu;
