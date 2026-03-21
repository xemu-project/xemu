//
// OpenMidway Multiplayer Wizard
//
// Phase 7 — Windows + Multiplayer UX
//
// Provides a guided panel for configuring System Link / LAN / XLink Kai /
// Insignia multiplayer sessions, plus a relay-backed "easy join" room code
// flow.
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
#pragma once
#include <string>
#include "common.hh"

// ---------------------------------------------------------------------------
// Wizard mode — one of the four System Link scenarios supported.
// ---------------------------------------------------------------------------
enum MultiplayerMode {
    MULTIPLAYER_MODE_OPENMIDWAY = 0, // UDP tunnel — peer-to-peer or relay
    MULTIPLAYER_MODE_LAN        = 1, // Bridged/pcap — real Xbox on LAN
    MULTIPLAYER_MODE_XLINKKAI   = 2, // Bridged/pcap + XLink Kai
    MULTIPLAYER_MODE_INSIGNIA   = 3, // NAT + Insignia / Xbox Live recreation
};

// ---------------------------------------------------------------------------
// MultiplayerWizard
//
// Draws a self-contained wizard panel that:
//   • lets the user choose one of four modes
//   • automatically pre-fills the matching network backend settings
//   • warns about default QEMU MAC addresses that collide on LAN
//   • shows a one-time reminder to back up the EEPROM before going online
//   • offers a relay "easy join" room code flow
// ---------------------------------------------------------------------------
class MultiplayerWizard {
public:
    MultiplayerWizard();

    // Draw the full wizard panel.  Call from inside an ImGui scroll region.
    void Draw();

    // Returns true when a relay session is active (used by HUD overlay).
    bool IsRelayActive() const { return m_relay_active; }

    // Estimated one-way latency and packet loss for the current relay session.
    // Only meaningful when IsRelayActive() is true.
    float GetRelayLatencyMs() const  { return m_relay_latency_ms; }
    float GetRelayPacketLoss() const { return m_relay_packet_loss; }

    // Advance relay statistics simulation.  Call once per UI frame.
    void Tick();

private:
    // -----------------------------------------------------------------------
    // Wizard helpers
    // -----------------------------------------------------------------------
    void DrawModeCombo();
    void DrawMacWarning();
    void DrawEepromReminder();
    void DrawModeDetails(bool appearing);
    void DrawDiagnostics();

    // Apply g_config.net.backend (and related settings) for the chosen mode.
    void ApplyMode(MultiplayerMode mode);

    // Returns true when the emulated NIC MAC looks like the default QEMU
    // allocation range (52:54:00:12:34:xx), which would collide with other
    // instances on a System Link session.
    static bool IsDefaultMac();

    // -----------------------------------------------------------------------
    // Relay / room code helpers
    // -----------------------------------------------------------------------
    void DrawRelaySection(bool appearing);

    // Derive a stable UDP remote address from a room code.
    static std::string RoomCodeToAddress(const char *code);

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------
    MultiplayerMode m_mode;
    bool            m_eeprom_reminder_shown;

    // Relay / room code
    char  m_room_code[16];
    char  m_relay_remote_display[128]; // display copy of applied relay address
    bool  m_relay_active;
    float m_relay_latency_ms;
    float m_relay_packet_loss;
};

extern MultiplayerWizard g_multiplayer_wizard;
