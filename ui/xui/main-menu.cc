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
#include "scene-manager.hh"
#include "widgets.hh"
#include "main-menu.hh"
#include "font-manager.hh"
#include "input-manager.hh"
#include "viewport-manager.hh"
#include "xemu-hud.h"
#include "misc.hh"
#include "gl-helpers.hh"
#include "reporting.hh"
#include "qapi/error.h"

#include "../xemu-input.h"
#include "../xemu-notifications.h"
#include "../xemu-settings.h"
#include "../xemu-monitor.h"
#include "../xemu-version.h"
#include "../xemu-net.h"
#include "../xemu-snapshots.h"
#include "../xemu-os-utils.h"
#include "../xemu-xbe.h"

MainMenuScene g_main_menu;

MainMenuTabView::~MainMenuTabView() {}
void MainMenuTabView::Draw() {}

void MainMenuGeneralView::Draw()
{
#if defined(_WIN32)
    SectionTitle("Updates");
    Toggle("Check for updates", &g_config.general.updates.check,
           "Check for updates whenever xemu is opened");
#endif

#if defined(__x86_64__)
    SectionTitle("Performance");
    Toggle("Hard FPU emulation", &g_config.perf.hard_fpu,
           "Use hardware-accelerated floating point emulation (requires restart)");
#endif

    Toggle("Cache shaders to disk", &g_config.perf.cache_shaders,
           "Reduce stutter in games by caching previously generated shaders");

    SectionTitle("Miscellaneous");
    Toggle("Skip startup animation", &g_config.general.skip_boot_anim,
           "Skip the full Xbox boot animation sequence");
    FilePicker("Screenshot output directory", &g_config.general.screenshot_dir,
               NULL, true);
    // toggle("Throttle DVD/HDD speeds", &g_config.general.throttle_io,
    //        "Limit DVD/HDD throughput to approximate Xbox load times");
}

void MainMenuInputView::Draw()
{
    SectionTitle("Controllers");
    ImGui::PushFont(g_font_mgr.m_menu_font_small);

    static int active = 0;

    // Output dimensions of texture
    float t_w = 512, t_h = 512;
    // Dimensions of (port+label)s
    float b_x = 0, b_x_stride = 100, b_y = 400;
    float b_w = 68, b_h = 81;
    // Dimensions of controller (rendered at origin)
    float controller_width  = 477.0f;
    float controller_height = 395.0f;

    // Setup rendering to fbo for controller and port images
    controller_fbo->Target();
    ImTextureID id = (ImTextureID)(intptr_t)controller_fbo->Texture();

    //
    // Render buttons with icons of the Xbox style port sockets with
    // circular numbers above them. These buttons can be activated to
    // configure the associated port, like a tabbed interface.
    //
    ImVec4 color_active(0.50, 0.86, 0.54, 0.12);
    ImVec4 color_inactive(0, 0, 0, 0);

    // Begin a 4-column layout to render the ports
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        g_viewport_mgr.Scale(ImVec2(0, 12)));
    ImGui::Columns(4, "mixed", false);

    const int port_padding = 8;
    for (int i = 0; i < 4; i++) {
        bool is_selected = (i == active);
        bool port_is_bound = (xemu_input_get_bound(i) != NULL);

        // Set an X offset to center the image button within the column
        ImGui::SetCursorPosX(
            ImGui::GetCursorPosX() +
            (int)((ImGui::GetColumnWidth() - b_w * g_viewport_mgr.m_scale -
                   2 * port_padding * g_viewport_mgr.m_scale) /
                  2));

        // We are using the same texture for all buttons, but ImageButton
        // uses the texture as a unique ID. Push a new ID now to resolve
        // the conflict.
        ImGui::PushID(i);
        float x = b_x+i*b_x_stride;
        ImGui::PushStyleColor(ImGuiCol_Button, is_selected ?
                                                   color_active :
                                                   color_inactive);
        bool activated = ImGui::ImageButton(id,
            ImVec2(b_w*g_viewport_mgr.m_scale,b_h*g_viewport_mgr.m_scale),
            ImVec2(x/t_w, (b_y+b_h)/t_h),
            ImVec2((x+b_w)/t_w, b_y/t_h),
            port_padding * g_viewport_mgr.m_scale);
        ImGui::PopStyleColor();

        if (activated) {
            active = i;
        }

        uint32_t port_color = 0xafafafff;
        bool is_hovered = ImGui::IsItemHovered();
        if (is_hovered) {
            port_color = 0xffffffff;
        } else if (is_selected || port_is_bound) {
            port_color = 0x81dc8a00;
        }

        RenderControllerPort(x, b_y, i, port_color);

        ImGui::PopID();
        ImGui::NextColumn();
    }
    ImGui::PopStyleVar(); // ItemSpacing
    ImGui::Columns(1);

    //
    // Render input device combo
    //

    // List available input devices
    const char *not_connected = "Not Connected";
    ControllerState *bound_state = xemu_input_get_bound(active);

    // Get current controller name
    const char *name;
    if (bound_state == NULL) {
        name = not_connected;
    } else {
        name = bound_state->name;
    }

    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("###InputDevices", name, ImGuiComboFlags_NoArrowButton))
    {
        // Handle "Not connected"
        bool is_selected = bound_state == NULL;
        if (ImGui::Selectable(not_connected, is_selected)) {
            xemu_input_bind(active, NULL, 1);
            bound_state = NULL;
        }
        if (is_selected) {
            ImGui::SetItemDefaultFocus();
        }

        // Handle all available input devices
        ControllerState *iter;
        QTAILQ_FOREACH(iter, &available_controllers, entry) {
            is_selected = bound_state == iter;
            ImGui::PushID(iter);
            const char *selectable_label = iter->name;
            char buf[128];
            if (iter->bound >= 0) {
                snprintf(buf, sizeof(buf), "%s (Port %d)", iter->name, iter->bound+1);
                selectable_label = buf;
            }
            if (ImGui::Selectable(selectable_label, is_selected)) {
                xemu_input_bind(active, iter, 1);
                bound_state = iter;
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
            ImGui::PopID();
        }

        ImGui::EndCombo();
    }
    DrawComboChevron();

    ImGui::Columns(1);

    //
    // Add a separator between input selection and controller graphic
    //
    ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().WindowPadding.y / 2));

    //
    // Render controller image
    //
    bool device_selected = false;

    if (bound_state) {
        device_selected = true;
        RenderController(0, 0, 0x81dc8a00, 0x0f0f0f00, bound_state);
    } else {
        static ControllerState state = { 0 };
        RenderController(0, 0, 0x1f1f1f00, 0x0f0f0f00, &state);
    }

    ImVec2 cur = ImGui::GetCursorPos();

    ImVec2 controller_display_size;
    if (ImGui::GetContentRegionMax().x < controller_width*g_viewport_mgr.m_scale) {
        controller_display_size.x = ImGui::GetContentRegionMax().x;
        controller_display_size.y =
            controller_display_size.x * controller_height / controller_width;
    } else {
        controller_display_size =
            ImVec2(controller_width * g_viewport_mgr.m_scale,
                   controller_height * g_viewport_mgr.m_scale);
    }

    ImGui::SetCursorPosX(
        ImGui::GetCursorPosX() +
        (int)((ImGui::GetColumnWidth() - controller_display_size.x) / 2.0));

    ImGui::Image(id,
        controller_display_size,
        ImVec2(0, controller_height/t_h),
        ImVec2(controller_width/t_w, 0));
    ImVec2 pos = ImGui::GetCursorPos();
    if (!device_selected) {
        const char *msg = "Please select an available input device";
        ImVec2 dim = ImGui::CalcTextSize(msg);
        ImGui::SetCursorPosX(cur.x + (controller_display_size.x-dim.x)/2);
        ImGui::SetCursorPosY(cur.y + (controller_display_size.y-dim.y)/2);
        ImGui::Text("%s", msg);
    }

    controller_fbo->Restore();

    ImGui::PopFont();
    ImGui::SetCursorPos(pos);

    SectionTitle("Options");
    Toggle("Auto-bind controllers", &g_config.input.auto_bind,
           "Bind newly connected controllers to any open port");
    Toggle("Background controller input capture",
           &g_config.input.background_input_capture,
           "Capture even if window is unfocused (requires restart)");
}

void MainMenuDisplayView::Draw()
{
    SectionTitle("Quality");
    int rendering_scale = nv2a_get_surface_scale_factor() - 1;
    if (ChevronCombo("Internal resolution scale", &rendering_scale,
                     "1x\0"
                     "2x\0"
                     "3x\0"
                     "4x\0"
                     "5x\0"
                     "6x\0"
                     "7x\0"
                     "8x\0"
                     "9x\0"
                     "10x\0",
                     "Increase surface scaling factor for higher quality")) {
        nv2a_set_surface_scale_factor(rendering_scale+1);
    }

    SectionTitle("Window");
    bool fs = xemu_is_fullscreen();
    if (Toggle("Fullscreen", &fs, "Enable fullscreen now")) {
        xemu_toggle_fullscreen();
    }
    Toggle("Fullscreen on startup",
           &g_config.display.window.fullscreen_on_startup,
           "Start xemu in fullscreen when opened");
    if (ChevronCombo("Window size", &g_config.display.window.startup_size,
                     "Last Used\0"
                     "640x480\0"
                     "1280x720\0"
                     "1280x800\0"
                     "1280x960\0"
                     "1920x1080\0"
                     "2560x1440\0"
                     "2560x1600\0"
                     "2560x1920\0"
                     "3840x2160\0",
                     "Select preferred startup window size")) {
    }
    Toggle("Vertical refresh sync", &g_config.display.window.vsync,
           "Sync to screen vertical refresh to reduce tearing artifacts");

    SectionTitle("Interface");
    Toggle("Show main menu bar", &g_config.display.ui.show_menubar,
           "Show main menu bar when mouse is activated");

    int ui_scale_idx;
    if (g_config.display.ui.auto_scale) {
        ui_scale_idx = 0;
    } else {
        ui_scale_idx = g_config.display.ui.scale;
        if (ui_scale_idx < 0) ui_scale_idx = 0;
        else if (ui_scale_idx > 2) ui_scale_idx = 2;
    }
    if (ChevronCombo("UI scale", &ui_scale_idx,
                     "Auto\0"
                     "1x\0"
                     "2x\0",
                     "Interface element scale")) {
        if (ui_scale_idx == 0) {
            g_config.display.ui.auto_scale = true;
        } else {
            g_config.display.ui.auto_scale = false;
            g_config.display.ui.scale = ui_scale_idx;
        }
    }
    Toggle("Animations", &g_config.display.ui.use_animations,
           "Enable xemu user interface animations");
    ChevronCombo("Display mode", &g_config.display.ui.fit,
                 "Center\0"
                 "Scale\0"
                 "Scale (Widescreen 16:9)\0"
                 "Scale (4:3)\0"
                 "Stretch\0",
                 "Select how the framebuffer should fit or scale into the window");
}

void MainMenuAudioView::Draw()
{
    SectionTitle("Volume");
    char buf[32];
    snprintf(buf, sizeof(buf), "Limit output volume (%d%%)",
             (int)(g_config.audio.volume_limit * 100));
    Slider("Output volume limit", &g_config.audio.volume_limit, buf);

    SectionTitle("Quality");
    Toggle("Real-time DSP processing", &g_config.audio.use_dsp,
           "Enable improved audio accuracy (experimental)");

}

NetworkInterface::NetworkInterface(pcap_if_t *pcap_desc, char *_friendlyname)
{
    m_pcap_name = pcap_desc->name;
    m_description = pcap_desc->description ?: pcap_desc->name;
    if (_friendlyname) {
        char *tmp =
            g_strdup_printf("%s (%s)", _friendlyname, m_description.c_str());
        m_friendly_name = tmp;
        g_free((gpointer)tmp);
    } else {
        m_friendly_name = m_description;
    }
}

NetworkInterfaceManager::NetworkInterfaceManager()
{
    m_current_iface = NULL;
    m_failed_to_load_lib = false;
}

void NetworkInterfaceManager::Refresh(void)
{
    pcap_if_t *alldevs, *iter;
    char err[PCAP_ERRBUF_SIZE];

    if (xemu_net_is_enabled()) {
        return;
    }

#if defined(_WIN32)
    if (pcap_load_library()) {
        m_failed_to_load_lib = true;
        return;
    }
#endif

    m_ifaces.clear();
    m_current_iface = NULL;

    if (pcap_findalldevs(&alldevs, err)) {
        return;
    }

    for (iter=alldevs; iter != NULL; iter=iter->next) {
#if defined(_WIN32)
        char *friendly_name = get_windows_interface_friendly_name(iter->name);
        m_ifaces.emplace_back(new NetworkInterface(iter, friendly_name));
        if (friendly_name) {
            g_free((gpointer)friendly_name);
        }
#else
        m_ifaces.emplace_back(new NetworkInterface(iter));
#endif
        if (!strcmp(g_config.net.pcap.netif, iter->name)) {
            m_current_iface = m_ifaces.back().get();
        }
    }

    pcap_freealldevs(alldevs);
}

void NetworkInterfaceManager::Select(NetworkInterface &iface)
{
    m_current_iface = &iface;
    xemu_settings_set_string(&g_config.net.pcap.netif,
                             iface.m_pcap_name.c_str());
}

bool NetworkInterfaceManager::IsCurrent(NetworkInterface &iface)
{
    return &iface == m_current_iface;
}

MainMenuNetworkView::MainMenuNetworkView()
{
    should_refresh = true;
}

void MainMenuNetworkView::Draw()
{
    SectionTitle("Adapter");
    bool enabled = xemu_net_is_enabled();
    g_config.net.enable = enabled;
    if (Toggle("Enable", &g_config.net.enable,
               enabled ? "Virtual network connected (disable to change network "
                         "settings)" :
                         "Connect virtual network cable to machine")) {
        if (enabled) {
            xemu_net_disable();
        } else {
            xemu_net_enable();
        }
    }

    bool appearing = ImGui::IsWindowAppearing();
    if (enabled) ImGui::BeginDisabled();
    if (ChevronCombo(
            "Attached to", &g_config.net.backend,
            "NAT\0"
            "UDP Tunnel\0"
            "Bridged Adapter\0",
            "Controls what the virtual network controller interfaces with")) {
        appearing = true;
    }
    SectionTitle("Options");
    switch (g_config.net.backend) {
    case CONFIG_NET_BACKEND_PCAP:
        DrawPcapOptions(appearing);
        break;
    case CONFIG_NET_BACKEND_NAT:
        DrawNatOptions(appearing);
        break;
    case CONFIG_NET_BACKEND_UDP:
        DrawUdpOptions(appearing);
        break;
    default: break;
    }
    if (enabled) ImGui::EndDisabled();
}

void MainMenuNetworkView::DrawPcapOptions(bool appearing)
{
    if (iface_mgr.get() == nullptr) {
        iface_mgr.reset(new NetworkInterfaceManager());
        iface_mgr->Refresh();
    }

    if (iface_mgr->m_failed_to_load_lib) {
#if defined(_WIN32)
        const char *msg = "npcap library could not be loaded.\n"
                          "To use this backend, please install npcap.";
        ImGui::Text("%s", msg);
        ImGui::Dummy(ImVec2(0,10*g_viewport_mgr.m_scale));
        ImGui::SetCursorPosX((ImGui::GetWindowWidth()-120*g_viewport_mgr.m_scale)/2);
        if (ImGui::Button("Install npcap", ImVec2(120*g_viewport_mgr.m_scale, 0))) {
            xemu_open_web_browser("https://nmap.org/npcap/");
        }
#endif
    } else {
        const char *selected_display_name =
            (iface_mgr->m_current_iface ?
                 iface_mgr->m_current_iface->m_friendly_name.c_str() :
                 g_config.net.pcap.netif);
        float combo_width = ImGui::GetColumnWidth();
        float combo_size_ratio = 0.5;
        combo_width *= combo_size_ratio;
        PrepareComboTitleDescription("Network interface",
                                     "Host network interface to bridge with",
                                     combo_size_ratio);
        ImGui::SetNextItemWidth(combo_width);
        ImGui::PushFont(g_font_mgr.m_menu_font_small);
        if (ImGui::BeginCombo("###network_iface", selected_display_name,
                              ImGuiComboFlags_NoArrowButton)) {
            if (should_refresh) {
                iface_mgr->Refresh();
                should_refresh = false;
            }

            int i = 0;
            for (auto &iface : iface_mgr->m_ifaces) {
                bool is_selected = iface_mgr->IsCurrent((*iface));
                ImGui::PushID(i++);
                if (ImGui::Selectable(iface->m_friendly_name.c_str(),
                                      is_selected)) {
                    iface_mgr->Select((*iface));
                }
                if (is_selected) ImGui::SetItemDefaultFocus();
                ImGui::PopID();
            }
            ImGui::EndCombo();
        } else {
            should_refresh = true;
        }
        ImGui::PopFont();
        DrawComboChevron();
    }
}

void MainMenuNetworkView::DrawNatOptions(bool appearing)
{
    static ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
    WidgetTitleDescriptionItem(
        "Port Forwarding",
        "Configure xemu to forward connections to guest on these ports");
    float p = ImGui::GetFrameHeight() * 0.3;
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(p, p));
    if (ImGui::BeginTable("port_forward_tbl", 4, flags))
    {
        ImGui::TableSetupColumn("Host Port");
        ImGui::TableSetupColumn("Guest Port");
        ImGui::TableSetupColumn("Protocol");
        ImGui::TableSetupColumn("Action");
        ImGui::TableHeadersRow();

        for (unsigned int row = 0; row < g_config.net.nat.forward_ports_count; row++)
        {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", g_config.net.nat.forward_ports[row].host);

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%d", g_config.net.nat.forward_ports[row].guest);

            ImGui::TableSetColumnIndex(2);
            switch (g_config.net.nat.forward_ports[row].protocol) {
            case CONFIG_NET_NAT_FORWARD_PORTS_PROTOCOL_TCP:
                ImGui::TextUnformatted("TCP"); break;
            case CONFIG_NET_NAT_FORWARD_PORTS_PROTOCOL_UDP:
                ImGui::TextUnformatted("UDP"); break;
            default: assert(0);
            }

            ImGui::TableSetColumnIndex(3);
            ImGui::PushID(row);
            if (ImGui::Button("Remove")) {
                remove_net_nat_forward_ports(row);
            }
            ImGui::PopID();
        }

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        static char buf[8] = {"1234"};
        ImGui::SetNextItemWidth(ImGui::GetColumnWidth());
        ImGui::InputText("###hostport", buf, sizeof(buf));

        ImGui::TableSetColumnIndex(1);
        static char buf2[8] = {"1234"};
        ImGui::SetNextItemWidth(ImGui::GetColumnWidth());
        ImGui::InputText("###guestport", buf2, sizeof(buf2));

        ImGui::TableSetColumnIndex(2);
        static CONFIG_NET_NAT_FORWARD_PORTS_PROTOCOL protocol =
            CONFIG_NET_NAT_FORWARD_PORTS_PROTOCOL_TCP;
        assert(sizeof(protocol) >= sizeof(int));
        ImGui::SetNextItemWidth(ImGui::GetColumnWidth());
        ImGui::Combo("###protocol", &protocol, "TCP\0UDP\0");

        ImGui::TableSetColumnIndex(3);
        if (ImGui::Button("Add")) {
            int host, guest;
            if (sscanf(buf, "%d", &host) == 1 &&
                sscanf(buf2, "%d", &guest) == 1) {
                add_net_nat_forward_ports(host, guest, protocol);
            }
        }

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

void MainMenuNetworkView::DrawUdpOptions(bool appearing)
{
    if (appearing) {
        strncpy(remote_addr, g_config.net.udp.remote_addr, sizeof(remote_addr)-1);
        strncpy(local_addr, g_config.net.udp.bind_addr, sizeof(local_addr)-1);
    }

    float size_ratio = 0.5;
    float width = ImGui::GetColumnWidth() * size_ratio;
    ImGui::PushFont(g_font_mgr.m_menu_font_small);
    PrepareComboTitleDescription(
        "Remote Address",
        "Destination addr:port to forward packets to (1.2.3.4:9968)",
        size_ratio);
    ImGui::SetNextItemWidth(width);
    if (ImGui::InputText("###remote_host", remote_addr, sizeof(remote_addr))) {
        xemu_settings_set_string(&g_config.net.udp.remote_addr, remote_addr);
    }
    PrepareComboTitleDescription(
        "Bind Address", "Local addr:port to receive packets on (0.0.0.0:9968)",
        size_ratio);
    ImGui::SetNextItemWidth(width);
    if (ImGui::InputText("###local_host", local_addr, sizeof(local_addr))) {
        xemu_settings_set_string(&g_config.net.udp.bind_addr, local_addr);
    }
    ImGui::PopFont();
}

void MainMenuSnapshotsView::Load()
{
    Error *err = NULL;

    if (!m_load_failed) {
        m_snapshots_len = xemu_snapshots_list(&m_snapshots, &m_extra_data, &err);
    }

    if (err) {
        m_load_failed = true;
        xemu_queue_error_message(error_get_pretty(err));
        error_free(err);
        m_snapshots_len = 0;
    }

    struct xbe *xbe = xemu_get_xbe_info();
    if (xbe && xbe->cert->m_titleid != m_current_title_id) {
        g_free(m_current_title_name);
        m_current_title_name = g_utf16_to_utf8(xbe->cert->m_title_name, 40, NULL, NULL, NULL);
        m_current_title_id = xbe->cert->m_titleid;
    }
}

MainMenuSnapshotsView::MainMenuSnapshotsView(): MainMenuTabView()
{
    xemu_snapshots_mark_dirty();
    m_load_failed = false;

    m_search_regex = NULL;
    m_current_title_name = NULL;
    m_current_title_id = 0;

}

MainMenuSnapshotsView::~MainMenuSnapshotsView()
{
    g_free(m_snapshots);
    g_free(m_extra_data);
    xemu_snapshots_mark_dirty();

    g_free(m_current_title_name);
    g_free(m_search_regex);
}

void MainMenuSnapshotsView::SnapshotBigButton(QEMUSnapshotInfo *snapshot, const char *title_name, GLuint thumbnail)
{
    Error *err = NULL;
    int current_snapshot_binding = -1;
    for (int i = 0; i < 4; ++i) {
        if (g_strcmp0(*(g_snapshot_shortcut_index_key_map[i]), snapshot->name) == 0) {
            assert(current_snapshot_binding == -1);
            current_snapshot_binding = i;
        }
    }

    ImGuiStyle &style = ImGui::GetStyle();
    ImVec2 pos = ImGui::GetCursorPos();
    ImDrawList *draw_list = ImGui::GetWindowDrawList();

    ImGui::PushFont(g_font_mgr.m_menu_font_small);
    ImVec2 ts_sub = ImGui::CalcTextSize(snapshot->name);
    ImGui::PopFont();

    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, g_viewport_mgr.Scale(ImVec2(5, 5)));
    ImGui::PushFont(g_font_mgr.m_menu_font_medium);

    ImVec2 ts_title = ImGui::CalcTextSize(snapshot->name);
    ImVec2 thumbnail_size = g_viewport_mgr.Scale(ImVec2(XEMU_SNAPSHOT_THUMBNAIL_WIDTH, XEMU_SNAPSHOT_THUMBNAIL_HEIGHT));
    ImVec2 thumbnail_pos(style.FramePadding.x, style.FramePadding.y);
    ImVec2 name_pos(thumbnail_pos.x + thumbnail_size.x + style.FramePadding.x * 2, thumbnail_pos.y);
    ImVec2 title_pos(name_pos.x, name_pos.y + ts_title.y + style.FramePadding.x);
    ImVec2 date_pos(name_pos.x, title_pos.y + ts_title.y + style.FramePadding.x);
    ImVec2 binding_pos(name_pos.x, date_pos.y + ts_title.y + style.FramePadding.x);

    bool load = ImGui::Button("###button", ImVec2(-FLT_MIN, fmax(thumbnail_size.y + style.FramePadding.y * 2, ts_title.y + ts_sub.y + style.FramePadding.y * 3)));
    if (load) {
        xemu_snapshots_load(snapshot->name, &err);
        if (err) {
            xemu_queue_error_message(error_get_pretty(err));
            error_free(err);
        }
    }
    bool options_key_pressed = ImGui::IsKeyPressed(ImGuiKey_GamepadFaceLeft);
    bool show_snapshot_context_menu = ImGui::IsItemHovered() &&
        (ImGui::IsMouseReleased(ImGuiMouseButton_Right) ||
         options_key_pressed);

    ImVec2 next_pos = ImGui::GetCursorPos();
    const ImVec2 p0 = ImGui::GetItemRectMin();
    const ImVec2 p1 = ImGui::GetItemRectMax();
    ImGui::PopFont();

    draw_list->PushClipRect(p0, p1, true);

    // Snapshot thumbnail
    ImGui::SetItemAllowOverlap();
    ImGui::SetCursorPos(ImVec2(pos.x + thumbnail_pos.x, pos.y + thumbnail_pos.y));

    if (!thumbnail) {
        thumbnail = g_icon_tex;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, thumbnail);
    int thumbnail_width, thumbnail_height;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &thumbnail_width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &thumbnail_height);

    // Draw black background behind thumbnail
    draw_list->AddRectFilled(ImVec2(p0.x + thumbnail_pos.x, p0.y + thumbnail_pos.y),
                             ImVec2(p0.x + thumbnail_pos.x + thumbnail_size.x, p0.y + thumbnail_pos.y + thumbnail_size.y),
                             IM_COL32_BLACK);

    // Draw centered thumbnail
    int scaled_width, scaled_height;
    ScaleDimensions(thumbnail_width, thumbnail_height, thumbnail_size.x, thumbnail_size.y, &scaled_width, &scaled_height);
    ImVec2 img_pos = ImGui::GetCursorPos();
    img_pos.x += (thumbnail_size.x - scaled_width) / 2;
    img_pos.y += (thumbnail_size.y - scaled_height) / 2;
    ImGui::SetCursorPos(img_pos);
    ImGui::Image((ImTextureID)(uint64_t)thumbnail, ImVec2(scaled_width, scaled_height));

    // Snapshot title
    ImGui::PushFont(g_font_mgr.m_menu_font_medium);
    draw_list->AddText(ImVec2(p0.x + name_pos.x, p0.y + name_pos.y), IM_COL32(255, 255, 255, 255), snapshot->name);
    ImGui::PopFont();

    // Snapshot XBE title name
    ImGui::PushFont(g_font_mgr.m_menu_font_small);
    draw_list->AddText(ImVec2(p0.x + title_pos.x, p0.y + title_pos.y), IM_COL32(255, 255, 255, 200), title_name);

    // Snapshot date
    g_autoptr(GDateTime) date = g_date_time_new_from_unix_local(snapshot->date_sec);
    char *date_buf = g_date_time_format(date, "%Y-%m-%d %H:%M:%S");
    draw_list->AddText(ImVec2(p0.x + date_pos.x, p0.y + date_pos.y), IM_COL32(255, 255, 255, 200), date_buf);
    g_free(date_buf);

    // Snapshot keyboard binding
    if (current_snapshot_binding != -1) {
        char *binding_text = g_strdup_printf("Bound to F%d", current_snapshot_binding + 5);
        draw_list->AddText(ImVec2(p0.x + binding_pos.x, p0.y + binding_pos.y), IM_COL32(255, 255, 255, 200), binding_text);
        g_free(binding_text);
    }

    ImGui::PopFont();
    draw_list->PopClipRect();

    if (show_snapshot_context_menu) {
        if (options_key_pressed) {
            ImGui::SetNextWindowPos(p0);
        }
        ImGui::OpenPopup("Snapshot Options");
    }

    if (ImGui::BeginPopupContextItem("Snapshot Options")) {
        if (ImGui::MenuItem("Load")) {
            xemu_snapshots_load(snapshot->name, &err);
            if (err) {
                xemu_queue_error_message(error_get_pretty(err));
                error_free(err);
            }
        }

        if (ImGui::BeginMenu("Keybinding")) {
            for (int i = 0; i < 4; ++i) {
                char *item_name = g_strdup_printf("Bind to F%d", i + 5);

                if (ImGui::MenuItem(item_name)) {
                    if (current_snapshot_binding >= 0) {
                        xemu_settings_set_string(g_snapshot_shortcut_index_key_map[current_snapshot_binding], "");
                    }
                    xemu_settings_set_string(g_snapshot_shortcut_index_key_map[i], snapshot->name);
                    current_snapshot_binding = i;

                    ImGui::CloseCurrentPopup();
                }

                g_free(item_name);
            }

            if (current_snapshot_binding >= 0) {
                if (ImGui::MenuItem("Unbind")) {
                    xemu_settings_set_string(g_snapshot_shortcut_index_key_map[current_snapshot_binding], "");
                    current_snapshot_binding = -1;
                }
            }
            ImGui::EndMenu();
        }
        
        ImGui::Separator();

        if (ImGui::MenuItem("Replace")) {
            xemu_snapshots_save(snapshot->name, &err);
            if (err) {
                xemu_queue_error_message(error_get_pretty(err));
                error_free(err);
            }
        }
        if (ImGui::MenuItem("Delete")) {
            xemu_snapshots_delete(snapshot->name, &err);
            if (err) {
                xemu_queue_error_message(error_get_pretty(err));
                error_free(err);
            }
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(2);
    ImGui::SetCursorPos(next_pos);
}

static int MainMenuSnapshotsViewUpdateSearchBox(ImGuiInputTextCallbackData *data)
{
    GError *gerr = NULL;
    MainMenuSnapshotsView *win = (MainMenuSnapshotsView*)data->UserData;

    if (win->m_search_regex) g_free(win->m_search_regex);
    if (data->BufTextLen == 0) {
        win->m_search_regex = NULL;
        return 0;
    }

    char *buf = g_strdup_printf("(.*)%s(.*)", data->Buf);

    win->m_search_regex = g_regex_new(buf, (GRegexCompileFlags)0, (GRegexMatchFlags)0, &gerr);
    g_free(buf);
    if (gerr) {
        win->m_search_regex = NULL;
        return 1;
    }

    return 0;
}

void MainMenuSnapshotsView::Draw()
{
    Load();
    SectionTitle("Snapshots");

    Toggle("Filter by current title", &g_config.general.snapshots.filter_current_game);

    ImGui::SetNextItemWidth(ImGui::GetColumnWidth() * 0.8);
    ImGui::InputTextWithHint("##search", "Search...", &m_search_buf, ImGuiInputTextFlags_CallbackEdit,
                             &MainMenuSnapshotsViewUpdateSearchBox, this);

    bool snapshot_with_create_name_exists = false;
    for (int i = 0; i < m_snapshots_len; ++i) {
        if (g_strcmp0(m_search_buf.c_str(), m_snapshots[i].name) == 0) {
            snapshot_with_create_name_exists = true;
            break;
        }
    }

    ImGui::SameLine();
    if (ImGui::Button(snapshot_with_create_name_exists ? "Replace" : "Create", ImVec2(-FLT_MIN, 0))) {
        xemu_snapshots_save(m_search_buf.empty() ? NULL : m_search_buf.c_str(), NULL);
        m_search_buf.clear();
    }

    if (snapshot_with_create_name_exists && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("A snapshot with the name \"%s\" already exists. This button will overwrite the existing snapshot.", m_search_buf.c_str());
    }

    for (int i = m_snapshots_len - 1; i >= 0; i--) {
        if (g_config.general.snapshots.filter_current_game && m_extra_data[i].xbe_title_name && 
            (strcmp(m_current_title_name, m_extra_data[i].xbe_title_name) != 0)) {
            continue;
        }

        if (m_search_regex) {
            GMatchInfo *match;
            bool keep_entry = false;
        
            g_regex_match(m_search_regex, m_snapshots[i].name, (GRegexMatchFlags)0, &match);
            keep_entry |= g_match_info_matches(match);
            g_match_info_free(match);

            if (m_extra_data[i].xbe_title_name) {
                g_regex_match(m_search_regex, m_extra_data[i].xbe_title_name, (GRegexMatchFlags)0, &match);
                keep_entry |= g_match_info_matches(match);
                g_free(match);
            }

            if (!keep_entry) {
                continue;
            }
        }

        ImGui::PushID(i);
        SnapshotBigButton(
            m_snapshots + i,
            m_extra_data[i].xbe_title_name ? m_extra_data[i].xbe_title_name : "Unknown",
            m_extra_data[i].gl_thumbnail
        );
        ImGui::PopID();
    }
}

MainMenuSystemView::MainMenuSystemView() : m_dirty(false)
{
}

void MainMenuSystemView::Draw()
{
    const char *rom_file_filters = ".bin Files\0*.bin\0.rom Files\0*.rom\0All Files\0*.*\0";
    const char *qcow_file_filters = ".qcow2 Files\0*.qcow2\0All Files\0*.*\0";

    if (m_dirty) {
        ImGui::TextColored(ImVec4(1,0,0,1), "Application restart required to apply settings");
    }

    SectionTitle("System Configuration");

    if (ChevronCombo(
            "System Memory", &g_config.sys.mem_limit,
            "64 MiB (Default)\0""128 MiB\0",
            "Increase to 128 MiB for debug or homebrew applications")) {
        m_dirty = true;
    }

    if (ChevronCombo(
            "AV Pack", &g_config.sys.avpack,
            "SCART\0HDTV (Default)\0VGA\0RFU\0S-Video\0Composite\0None\0",
            "Select the attached AV pack")) {
        m_dirty = true;
    }

    SectionTitle("Files");
    if (FilePicker("MCPX Boot ROM", &g_config.sys.files.bootrom_path,
                   rom_file_filters)) {
        m_dirty = true;
        g_main_menu.UpdateAboutViewConfigInfo();
    }
    if (FilePicker("Flash ROM (BIOS)", &g_config.sys.files.flashrom_path,
                   rom_file_filters)) {
        m_dirty = true;
        g_main_menu.UpdateAboutViewConfigInfo();
    }
    if (FilePicker("Hard Disk", &g_config.sys.files.hdd_path,
                   qcow_file_filters)) {
        m_dirty = true;
    }
    if (FilePicker("EEPROM", &g_config.sys.files.eeprom_path,
                   rom_file_filters)) {
        m_dirty = true;
    }
}

MainMenuAboutView::MainMenuAboutView(): m_config_info_text{NULL}
{}

void MainMenuAboutView::UpdateConfigInfoText()
{
    if (m_config_info_text) {
        g_free(m_config_info_text);
    }

    gchar *bootrom_checksum = GetFileMD5Checksum(g_config.sys.files.bootrom_path);
    if (!bootrom_checksum) {
        bootrom_checksum = g_strdup("None");
    }

    gchar *flash_rom_checksum = GetFileMD5Checksum(g_config.sys.files.flashrom_path);
    if (!flash_rom_checksum) {
        flash_rom_checksum = g_strdup("None");
    }

    m_config_info_text = g_strdup_printf(
            "MCPX Boot ROM MD5 Hash:        %s\n"
            "Flash ROM (BIOS) MD5 Hash:     %s",
            bootrom_checksum, flash_rom_checksum);
    g_free(bootrom_checksum);
    g_free(flash_rom_checksum);
}

void MainMenuAboutView::Draw()
{
    static const char *build_info_text = NULL;
    if (build_info_text == NULL) {
        build_info_text = g_strdup_printf(
            "Version:      %s\nBranch:       %s\nCommit:       %s\nDate:         %s",
            xemu_version, xemu_branch, xemu_commit, xemu_date);
    }

    static const char *sys_info_text = NULL;
    if (sys_info_text == NULL) {
        const char *gl_shader_version = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
        const char *gl_version = (const char*)glGetString(GL_VERSION);
        const char *gl_renderer = (const char*)glGetString(GL_RENDERER);
        const char *gl_vendor = (const char*)glGetString(GL_VENDOR);
        sys_info_text = g_strdup_printf(
            "CPU:          %s\nOS Platform:  %s\nOS Version:   %s\nManufacturer: %s\n"
            "GPU Model:    %s\nDriver:       %s\nShader:       %s",
             xemu_get_cpu_info(), xemu_get_os_platform(), xemu_get_os_info(), gl_vendor,
             gl_renderer, gl_version, gl_shader_version);
    }

    if (m_config_info_text == NULL) {
        UpdateConfigInfoText();
    }

    Logo();

    SectionTitle("Build Information");
    ImGui::PushFont(g_font_mgr.m_fixed_width_font);
    ImGui::InputTextMultiline("##build_info", (char *)build_info_text,
                              strlen(build_info_text),
                              ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 5),
                              ImGuiInputTextFlags_ReadOnly);
    ImGui::PopFont();

    SectionTitle("System Information");
    ImGui::PushFont(g_font_mgr.m_fixed_width_font);
    ImGui::InputTextMultiline("###systeminformation", (char *)sys_info_text,
                              strlen(sys_info_text),
                              ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 8),
                              ImGuiInputTextFlags_ReadOnly);
    ImGui::PopFont();

    SectionTitle("Config Information");
    ImGui::PushFont(g_font_mgr.m_fixed_width_font);
    ImGui::InputTextMultiline("##config_info", (char *)m_config_info_text,
                              strlen(build_info_text),
                              ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 3),
                              ImGuiInputTextFlags_ReadOnly);
    ImGui::PopFont();

    SectionTitle("Community");

    ImGui::Text("Visit");
    ImGui::SameLine();
    if (ImGui::SmallButton("https://xemu.app")) {
        xemu_open_web_browser("https://xemu.app");
    }
    ImGui::SameLine();
    ImGui::Text("for more information");
}

MainMenuTabButton::MainMenuTabButton(std::string text, std::string icon)
: m_icon(icon), m_text(text)
{
}

bool MainMenuTabButton::Draw(bool selected)
{
    ImGuiStyle &style = ImGui::GetStyle();

    ImU32 col = selected ?
                    ImGui::GetColorU32(style.Colors[ImGuiCol_ButtonHovered]) :
                    IM_COL32(0, 0, 0, 0);

    ImGui::PushStyleColor(ImGuiCol_Button, col);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, selected ? col : IM_COL32(32, 32, 32, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, selected ? col : IM_COL32(32, 32, 32, 255));
    int p = ImGui::GetTextLineHeight() * 0.5;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(p, p));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0.5));
    ImGui::PushFont(g_font_mgr.m_menu_font);

    ImVec2 button_size = ImVec2(-FLT_MIN, 0);
    auto text = string_format("%s %s", m_icon.c_str(), m_text.c_str());
    ImGui::PushID(this);
    bool status = ImGui::Button(text.c_str(), button_size);
    ImGui::PopID();
    ImGui::PopFont();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(3);
    return status;
}

MainMenuScene::MainMenuScene()
: m_animation(0.12, 0.12),
  m_general_button("General",     ICON_FA_GEARS),
  m_input_button("Input",         ICON_FA_GAMEPAD),
  m_display_button("Display",     ICON_FA_TV),
  m_audio_button("Audio",         ICON_FA_VOLUME_HIGH),
  m_network_button("Network",     ICON_FA_NETWORK_WIRED),
  m_snapshots_button("Snapshots", ICON_FA_CLOCK_ROTATE_LEFT),
  m_system_button("System",       ICON_FA_MICROCHIP),
  m_about_button("About",         ICON_FA_CIRCLE_INFO)
{
    m_had_focus_last_frame = false;
    m_focus_view = false;
    m_tabs.push_back(&m_general_button);
    m_tabs.push_back(&m_input_button);
    m_tabs.push_back(&m_display_button);
    m_tabs.push_back(&m_audio_button);
    m_tabs.push_back(&m_network_button);
    m_tabs.push_back(&m_snapshots_button);
    m_tabs.push_back(&m_system_button);
    m_tabs.push_back(&m_about_button);

    m_views.push_back(&m_general_view);
    m_views.push_back(&m_input_view);
    m_views.push_back(&m_display_view);
    m_views.push_back(&m_audio_view);
    m_views.push_back(&m_network_view);
    m_views.push_back(&m_snapshots_view);
    m_views.push_back(&m_system_view);
    m_views.push_back(&m_about_view);

    m_current_view_index = 0;
    m_next_view_index = m_current_view_index;
}

void MainMenuScene::ShowGeneral()
{
    SetNextViewIndexWithFocus(0);
}
void MainMenuScene::ShowInput()
{
    SetNextViewIndexWithFocus(1);
}
void MainMenuScene::ShowDisplay()
{
    SetNextViewIndexWithFocus(2);
}
void MainMenuScene::ShowAudio()
{
    SetNextViewIndexWithFocus(3);
}
void MainMenuScene::ShowNetwork()
{
    SetNextViewIndexWithFocus(4);
}
void MainMenuScene::ShowSnapshots() 
{
    SetNextViewIndexWithFocus(5);
}
void MainMenuScene::ShowSystem()
{
    SetNextViewIndexWithFocus(6);
}
void MainMenuScene::ShowAbout()
{
    SetNextViewIndexWithFocus(7);
}

void MainMenuScene::SetNextViewIndexWithFocus(int i)
{
    m_focus_view = true;
    SetNextViewIndex(i);

    if (!g_scene_mgr.IsDisplayingScene()) {
        g_scene_mgr.PushScene(*this);
    }
}

void MainMenuScene::Show()
{
    m_background.Show();
    m_nav_control_view.Show();
    m_animation.EaseIn();
}

void MainMenuScene::Hide()
{
    m_background.Hide();
    m_nav_control_view.Hide();
    m_animation.EaseOut();
}

bool MainMenuScene::IsAnimating()
{
    return m_animation.IsAnimating();
}

void MainMenuScene::SetNextViewIndex(int i)
{
    m_next_view_index = i % m_tabs.size();
    g_config.general.last_viewed_menu_index = i;
}

void MainMenuScene::HandleInput()
{
    bool nofocus = !ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow);
    bool focus = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows |
                                        ImGuiFocusedFlags_NoPopupHierarchy);

    // XXX: Ensure we have focus for two frames. If a user cancels a popup window, we do not want to cancel main
    //      window as well.
    if (nofocus || (focus && m_had_focus_last_frame &&
                    ImGui::IsNavInputTest(ImGuiNavInput_Cancel,
                                          ImGuiInputReadMode_Pressed))) {
        Hide();
        return;
    }

    if (focus && m_had_focus_last_frame) {
        if (ImGui::IsKeyPressed(ImGuiKey_GamepadL1)) {
            SetNextViewIndex((m_current_view_index + m_tabs.size() - 1) %
                             m_tabs.size());
        }

        if (ImGui::IsKeyPressed(ImGuiKey_GamepadR1)) {
            SetNextViewIndex((m_current_view_index + 1) % m_tabs.size());
        }
    }

    m_had_focus_last_frame = focus;
}

void MainMenuScene::UpdateAboutViewConfigInfo()
{
    m_about_view.UpdateConfigInfoText();
}

bool MainMenuScene::Draw()
{
    m_animation.Step();
    m_background.Draw();
    m_nav_control_view.Draw();

    ImGuiIO &io = ImGui::GetIO();
    float t = m_animation.GetSinInterpolatedValue();
    float window_alpha = t;

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, window_alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

    ImVec4 extents = g_viewport_mgr.GetExtents();
    ImVec2 window_pos = ImVec2(io.DisplaySize.x / 2, extents.y);
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, ImVec2(0.5, 0));

    ImVec2 max_size = g_viewport_mgr.Scale(ImVec2(800, 0));
    float x = fmin(io.DisplaySize.x - extents.x - extents.z, max_size.x);
    float y = io.DisplaySize.y - extents.y - extents.w;
    ImGui::SetNextWindowSize(ImVec2(x, y));

    if (ImGui::Begin("###MainWindow", NULL,
                     ImGuiWindowFlags_NoDecoration |
                         ImGuiWindowFlags_NoSavedSettings)) {
        //
        // Nav menu
        //

        float width = ImGui::GetWindowWidth();
        float nav_width = width * 0.3;
        float content_width = width - nav_width;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(26,26,26,255));

        ImGui::BeginChild("###MainWindowNav", ImVec2(nav_width, -1), true, ImGuiWindowFlags_NavFlattened);

        bool move_focus_to_tab = false;
        if (m_current_view_index != m_next_view_index) {
            m_current_view_index = m_next_view_index;
            if (!m_focus_view) {
                move_focus_to_tab = true;
            }
        }

        int i = 0;
        for (auto &button : m_tabs) {
            if (move_focus_to_tab && i == m_current_view_index) {
                ImGui::SetKeyboardFocusHere();
                move_focus_to_tab = false;
            }
            if (button->Draw(i == m_current_view_index)) {
                SetNextViewIndex(i);
            }
            if (i == m_current_view_index) {
                ImGui::SetItemDefaultFocus();
            }
            i++;
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        //
        // Content
        //
        ImGui::SameLine();
        int s = ImGui::GetTextLineHeight() * 0.75;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(s, s));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(s, s));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6*g_viewport_mgr.m_scale);

        ImGui::PushID(m_current_view_index);
        ImGui::BeginChild("###MainWindowContent", ImVec2(content_width, -1),
                          true,
                          ImGuiWindowFlags_AlwaysUseWindowPadding |
                              ImGuiWindowFlags_NavFlattened);

        if (!g_input_mgr.IsNavigatingWithController()) {
            // Close button
            ImGui::PushFont(g_font_mgr.m_menu_font);
            ImGuiStyle &style = ImGui::GetStyle();
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 128));
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32_BLACK_TRANS);
            ImVec2 pos = ImGui::GetCursorPos();
            ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - style.FramePadding.x * 2.0f - ImGui::GetTextLineHeight());
            if (ImGui::Button(ICON_FA_XMARK)) {
                Hide();
            }
            ImGui::SetCursorPos(pos);
            ImGui::PopStyleColor(2);
            ImGui::PopFont();
        }

        ImGui::PushFont(g_font_mgr.m_default_font);
        if (m_focus_view) {
            ImGui::SetKeyboardFocusHere();
            m_focus_view = false;
        }
        m_views[m_current_view_index]->Draw();

        ImGui::PopFont();
        ImGui::EndChild();
        ImGui::PopID();
        ImGui::PopStyleVar(3);

        HandleInput();
    }
    ImGui::End();
    ImGui::PopStyleVar(5);

    return !m_animation.IsComplete();
}
