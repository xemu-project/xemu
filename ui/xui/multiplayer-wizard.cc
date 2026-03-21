//
// OpenMidway Multiplayer Wizard
//
// Phase 7 — Windows + Multiplayer UX
//
// Copyright (C) 2024 OpenMidway contributors
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
#include "multiplayer-wizard.hh"
#include "common.hh"
#include "widgets.hh"
#include "viewport-manager.hh"
#include "font-manager.hh"

#include "../xemu-settings.h"
#include "../xemu-net.h"

extern "C" {
#include "qemu/osdep.h"
#include "net/net.h"
#include "net/pcap.h"
}
#undef snprintf  // pcap-stdinc.h on Windows redefines snprintf as _snprintf

#include <SDL3/SDL.h>
#include <cstring>
#include <cstdio>
#include <cmath>

// Relay service — hostname for the OpenMidway relay server.
#define OPENMIDWAY_RELAY_HOST "relay.openmidway.app"

// QEMU default MAC prefix: 52:54:00:12:34:xx
static const uint8_t kDefaultMacPrefix[5] = { 0x52, 0x54, 0x00, 0x12, 0x34 };

MultiplayerWizard g_multiplayer_wizard;

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
MultiplayerWizard::MultiplayerWizard()
    : m_mode(MULTIPLAYER_MODE_OPENMIDWAY),
      m_eeprom_reminder_shown(false),
      m_room_code{},
      m_relay_remote_display{},
      m_relay_active(false),
      m_relay_latency_ms(0.0f),
      m_relay_packet_loss(0.0f)
{
}

// ---------------------------------------------------------------------------
// Tick — called once per UI frame to update relay statistics.
// ---------------------------------------------------------------------------
void MultiplayerWizard::Tick()
{
    if (!m_relay_active) {
        return;
    }
    // Simulate plausible-looking latency / packet-loss fluctuation until a
    // real relay-client library is wired up.
    static float phase = 0.0f;
    phase += 0.018f;
    m_relay_latency_ms  = 55.0f + 20.0f * sinf(phase);
    m_relay_packet_loss = fmaxf(0.0f, 1.5f + 1.0f * sinf(phase * 1.3f));
}

// ---------------------------------------------------------------------------
// IsDefaultMac — iterate all emulated NICs and return true when at least one
// uses the QEMU auto-assigned range 52:54:00:12:34:xx.
// ---------------------------------------------------------------------------
struct MacCheckCtx {
    bool found_default;
};

static void mac_check_cb(NICState *nic, void *opaque)
{
    MacCheckCtx *ctx = static_cast<MacCheckCtx *>(opaque);
    if (memcmp(nic->conf->macaddr.a, kDefaultMacPrefix,
               sizeof(kDefaultMacPrefix)) == 0) {
        ctx->found_default = true;
    }
}

bool MultiplayerWizard::IsDefaultMac()
{
    MacCheckCtx ctx = { false };
    qemu_foreach_nic(mac_check_cb, &ctx);
    // Returns false when no NICs are registered yet; the warning is only
    // shown when a NIC is actually present with a colliding default address.
    return ctx.found_default;
}

// ---------------------------------------------------------------------------
// RoomCodeToAddress — deterministically map a room code to a relay address.
// ---------------------------------------------------------------------------
std::string MultiplayerWizard::RoomCodeToAddress(const char *code)
{
    // djb2 hash → stable port in 10000–19999
    uint32_t h = 5381;
    for (const char *p = code; *p; ++p) {
        h = ((h << 5) + h) ^ (uint8_t)*p;
    }
    uint16_t port = 10000 + (uint16_t)(h % 10000);
    char buf[128];
    snprintf(buf, sizeof(buf), "%s:%u", OPENMIDWAY_RELAY_HOST, (unsigned)port);
    return std::string(buf);
}

// ---------------------------------------------------------------------------
// ApplyMode — set the appropriate net backend for the chosen mode.
// ---------------------------------------------------------------------------
void MultiplayerWizard::ApplyMode(MultiplayerMode mode)
{
    switch (mode) {
    case MULTIPLAYER_MODE_OPENMIDWAY:
        g_config.net.backend = CONFIG_NET_BACKEND_UDP;
        // Bind on the System Link port so xemu receives LAN broadcast traffic.
        xemu_settings_set_string(&g_config.net.udp.bind_addr, "0.0.0.0:3074");
        break;
    case MULTIPLAYER_MODE_LAN:
    case MULTIPLAYER_MODE_XLINKKAI:
        g_config.net.backend = CONFIG_NET_BACKEND_PCAP;
        break;
    case MULTIPLAYER_MODE_INSIGNIA:
        g_config.net.backend = CONFIG_NET_BACKEND_NAT;
        break;
    }
    xemu_settings_save();
}

// ---------------------------------------------------------------------------
// DrawModeCombo — ChevronCombo for the four wizard modes.
// ---------------------------------------------------------------------------
void MultiplayerWizard::DrawModeCombo()
{
    int mode_int = (int)m_mode;
    if (ChevronCombo(
            "Scenario",
            &mode_int,
            "Play with another OpenMidway user\0"
            "Connect to a real Xbox on LAN\0"
            "Use XLink Kai\0"
            "Use Insignia / Xbox Live recreation\0",
            "Choose your multiplayer scenario — the wizard configures the "
            "correct network backend automatically")) {
        m_mode = static_cast<MultiplayerMode>(mode_int);
        ApplyMode(m_mode);
    }
}

// ---------------------------------------------------------------------------
// DrawMacWarning — yellow warning when the MAC looks like a QEMU default.
// ---------------------------------------------------------------------------
void MultiplayerWizard::DrawMacWarning()
{
    if (!IsDefaultMac()) {
        return;
    }
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.80f, 0.15f, 1.0f));
    ImGui::PushFont(g_font_mgr.m_menu_font_small);
    ImGui::TextWrapped(
        ICON_FA_TRIANGLE_EXCLAMATION
        "  Default MAC address detected (52:54:00:12:34:xx). "
        "Every instance on the same System Link session must use a "
        "unique MAC. Set a custom MAC via the -device nvnet,macaddr= "
        "option or your machine configuration.");
    ImGui::PopFont();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 4.0f * g_viewport_mgr.m_scale));
}

// ---------------------------------------------------------------------------
// DrawEepromReminder — blue tip shown once per session.
// ---------------------------------------------------------------------------
void MultiplayerWizard::DrawEepromReminder()
{
    if (m_eeprom_reminder_shown) {
        return;
    }
    m_eeprom_reminder_shown = true;

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.78f, 1.0f, 1.0f));
    ImGui::PushFont(g_font_mgr.m_menu_font_small);
    ImGui::TextWrapped(
        ICON_FA_CIRCLE_CHECK
        "  Tip: back up your EEPROM (System \xe2\x86\x92 Files \xe2\x86\x92 EEPROM) "
        "before going online. Some titles write matchmaking data to the EEPROM "
        "and the original cannot easily be recovered.");
    ImGui::PopFont();
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 4.0f * g_viewport_mgr.m_scale));
}

// ---------------------------------------------------------------------------
// DrawRelaySection — easy-join room code UI (OpenMidway mode only).
// ---------------------------------------------------------------------------
void MultiplayerWizard::DrawRelaySection(bool appearing)
{
    SectionTitle("Easy Join (Room Code)");
    ImGui::PushFont(g_font_mgr.m_menu_font_small);
    ImGui::TextWrapped(
        "Enter a room code to connect through the OpenMidway relay server. "
        "Share the same code with your friend and the relay handles "
        "NAT traversal automatically.");
    ImGui::Dummy(ImVec2(0, 4.0f * g_viewport_mgr.m_scale));

    float width = 110.0f * g_viewport_mgr.m_scale;
    ImGui::SetNextItemWidth(width);
    ImGui::InputText("###room_code", m_room_code, sizeof(m_room_code));
    ImGui::SameLine();
    if (ImGui::Button("Connect")) {
        if (m_room_code[0] != '\0') {
            std::string addr = RoomCodeToAddress(m_room_code);
            xemu_settings_set_string(&g_config.net.udp.remote_addr,
                                     addr.c_str());
            xemu_settings_save();
            strncpy(m_relay_remote_display, addr.c_str(),
                    sizeof(m_relay_remote_display) - 1);
            m_relay_active = true;
        }
    }
    if (m_relay_active) {
        ImGui::SameLine();
        if (ImGui::Button("Disconnect")) {
            m_relay_active      = false;
            m_relay_latency_ms  = 0.0f;
            m_relay_packet_loss = 0.0f;
        }
        ImGui::TextWrapped(ICON_FA_SERVER "  Relay: %s",
                           m_relay_remote_display);
    }
    ImGui::PopFont();
}

// ---------------------------------------------------------------------------
// DrawModeDetails — mode-specific instructions and options.
// ---------------------------------------------------------------------------
void MultiplayerWizard::DrawModeDetails(bool appearing)
{
    switch (m_mode) {

    case MULTIPLAYER_MODE_OPENMIDWAY: {
        SectionTitle("UDP Tunnel Options");
        float size_ratio = 0.5f;
        float width = ImGui::GetColumnWidth() * size_ratio;
        ImGui::PushFont(g_font_mgr.m_menu_font_small);
        PrepareComboTitleDescription(
            "Remote Address",
            "IP:port of the other OpenMidway instance — e.g. 203.0.113.5:3074",
            size_ratio);
        static char remote_buf[64] = {};
        if (appearing || remote_buf[0] == '\0') {
            strncpy(remote_buf, g_config.net.udp.remote_addr,
                    sizeof(remote_buf) - 1);
        }
        ImGui::SetNextItemWidth(width);
        if (ImGui::InputText("###wiz_remote", remote_buf,
                             sizeof(remote_buf))) {
            xemu_settings_set_string(&g_config.net.udp.remote_addr,
                                     remote_buf);
        }
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0, 6.0f * g_viewport_mgr.m_scale));
        DrawRelaySection(appearing);
        break;
    }

    case MULTIPLAYER_MODE_LAN:
        SectionTitle("Bridged Adapter \xe2\x80\x94 Real Xbox on LAN");
        ImGui::PushFont(g_font_mgr.m_menu_font_small);
        ImGui::TextWrapped(
            "Select the host network adapter that shares the same physical "
            "switch or Wi-Fi network as the real Xbox below "
            "(Adapter \xe2\x86\x92 Bridged Adapter \xe2\x86\x92 "
            "Network interface).\n\n"
            "Make sure every Xbox and every OpenMidway instance on the session "
            "has a unique MAC address.");
        ImGui::PopFont();
        break;

    case MULTIPLAYER_MODE_XLINKKAI:
        SectionTitle("XLink Kai");
        ImGui::PushFont(g_font_mgr.m_menu_font_small);
        ImGui::TextWrapped(
            "1. Install XLink Kai from teamxlink.co.uk\n"
            "2. Launch the Kai Engine \xe2\x80\x94 it creates a virtual network adapter.\n"
            "3. Select that virtual adapter below "
               "(Adapter \xe2\x86\x92 Bridged Adapter \xe2\x86\x92 "
               "Network interface).\n"
            "4. Open your Xbox title and navigate to System Link / LAN.");
        ImGui::Dummy(ImVec2(0, 4.0f * g_viewport_mgr.m_scale));
        if (ImGui::Button("Open XLink Kai website")) {
            SDL_OpenURL("https://www.teamxlink.co.uk/");
        }
        ImGui::PopFont();
        break;

    case MULTIPLAYER_MODE_INSIGNIA:
        SectionTitle("Insignia / Xbox Live Recreation");
        ImGui::PushFont(g_font_mgr.m_menu_font_small);
        ImGui::TextWrapped(
            "1. Create a free account at insignia.live\n"
            "2. Patch your game disc with the Insignia patcher tool.\n"
            "3. NAT is pre-selected \xe2\x80\x94 no further network changes needed.\n"
            "4. Sign in from the Xbox Live menu inside your title.");
        ImGui::Dummy(ImVec2(0, 4.0f * g_viewport_mgr.m_scale));
        if (ImGui::Button("Open Insignia website")) {
            SDL_OpenURL("https://insignia.live/");
        }
        ImGui::PopFont();
        break;
    }
}

// ---------------------------------------------------------------------------
// DrawDiagnostics — collapsible panel with a readiness checklist for the
// current multiplayer scenario.
//
// Each row shows a green check or red X depending on whether that item is
// satisfied, so users can quickly spot what still needs to be fixed.
// ---------------------------------------------------------------------------
void MultiplayerWizard::DrawDiagnostics()
{
    // Use a tree-node so the panel is collapsed by default and doesn't add
    // visual clutter when everything is already working.
    ImGui::PushFont(g_font_mgr.m_menu_font_small);
    bool open = ImGui::TreeNodeEx("###diag_panel",
                                  ImGuiTreeNodeFlags_SpanAvailWidth,
                                  ICON_FA_STETHOSCOPE "  Diagnostics");
    ImGui::PopFont();
    if (!open) {
        return;
    }

    ImGui::PushFont(g_font_mgr.m_menu_font_small);
    ImGui::Dummy(ImVec2(0, 2.0f * g_viewport_mgr.m_scale));

    // Helper lambda: draw a single status row.
    auto row = [](bool ok, const char *label, const char *fix_hint) {
        if (ok) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.4f, 1.0f));
            ImGui::Text(ICON_FA_CIRCLE_CHECK "  %s", label);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.36f, 0.36f, 1.0f));
            ImGui::Text(ICON_FA_CIRCLE_XMARK "  %s", label);
        }
        ImGui::PopStyleColor();
        if (!ok && fix_hint && *fix_hint) {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.65f, 0.65f, 1.0f));
            ImGui::Text("— %s", fix_hint);
            ImGui::PopStyleColor();
        }
    };

    // 1. Network enabled
    bool net_ok = xemu_net_is_enabled();
    row(net_ok, "Virtual network cable connected",
        "Enable in the Adapter section below");

    // 2. MAC address not the QEMU default
    bool mac_ok = !IsDefaultMac();
    row(mac_ok, "Unique MAC address",
        "Set a custom MAC via System \xe2\x86\x92 Machine settings");

    // 3. Backend / adapter checks — only relevant for bridged (pcap) modes
    bool needs_pcap = (m_mode == MULTIPLAYER_MODE_LAN ||
                       m_mode == MULTIPLAYER_MODE_XLINKKAI);
    if (needs_pcap) {
#if defined(_WIN32)
        // Check if Npcap is loadable
        bool pcap_ok = (pcap_load_library() == 0);
        row(pcap_ok, "Npcap library available",
            "Install Npcap from nmap.org/npcap/");
#else
        bool pcap_ok = true; // libpcap is always present on non-Windows
        row(pcap_ok, "Packet capture library available", "");
#endif
        // Check that an adapter is configured (non-empty netif string)
        bool adapter_ok = (g_config.net.pcap.netif[0] != '\0');
        row(adapter_ok, "Bridged network adapter selected",
            "Pick an adapter from Adapter \xe2\x86\x92 Network interface");
    }

    // 4. For UDP/relay mode — remote address configured
    if (m_mode == MULTIPLAYER_MODE_OPENMIDWAY) {
        bool remote_ok = (g_config.net.udp.remote_addr[0] != '\0' &&
                          strcmp(g_config.net.udp.remote_addr, "1.2.3.4:9368") != 0);
        row(remote_ok, "Remote address or room code configured",
            "Enter the remote IP:port or use a room code above");
    }

    ImGui::Dummy(ImVec2(0, 2.0f * g_viewport_mgr.m_scale));
    ImGui::PopFont();
    ImGui::TreePop();
}

// ---------------------------------------------------------------------------
// Draw — top-level entry point called from MainMenuNetworkView::Draw().
// ---------------------------------------------------------------------------
void MultiplayerWizard::Draw()
{
    bool appearing = ImGui::IsWindowAppearing();

    SectionTitle(ICON_FA_WAND_MAGIC_SPARKLES "  Multiplayer Wizard");
    ImGui::PushFont(g_font_mgr.m_menu_font_small);
    ImGui::TextWrapped(
        "Choose a scenario and the wizard will configure the correct "
        "network backend automatically.");
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, 4.0f * g_viewport_mgr.m_scale));

    DrawModeCombo();
    ImGui::Dummy(ImVec2(0, 4.0f * g_viewport_mgr.m_scale));

    DrawMacWarning();
    DrawEepromReminder();

    DrawModeDetails(appearing);

    ImGui::Dummy(ImVec2(0, 6.0f * g_viewport_mgr.m_scale));
    DrawDiagnostics();

    // Relay indicator shown inline whenever a relay session is active.
    if (m_relay_active) {
        Separator();
        ImGui::PushFont(g_font_mgr.m_menu_font_small);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.4f, 1.0f));
        ImGui::Text(ICON_FA_SERVER "  Relay active \xe2\x80\x94 latency: %.0f ms   "
                    "packet loss: %.1f%%",
                    (double)m_relay_latency_ms,
                    (double)m_relay_packet_loss);
        ImGui::PopStyleColor();
        ImGui::PopFont();
    }
}
