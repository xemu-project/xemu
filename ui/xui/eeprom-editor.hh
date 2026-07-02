//
// UI EEPROM EDITOR
//
// Copyright (C) 2026 JBW89
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

#include <array>
#include <string>

#include "common.hh"

class MainMenuEepromEditor
{
public:
    void Draw(const char *configured_path, bool *restart_dirty);

private:
    static constexpr size_t kEepromSize = 256;

    std::array<uint8_t, kEepromSize> m_eeprom = {};
    std::string m_path;
    std::string m_status;
    bool m_status_error = false;
    bool m_loaded = false;
    bool m_open_requested = false;
    bool m_saved = false;

    int m_hardware_revision = 1;
    int m_xbox_region = 0;
    int m_video_standard = 0;
    int m_language = 1;
    int m_video_aspect = 0;
    int m_audio_output = 0;
    int m_dvd_region = 0;

    bool m_video_480p = false;
    bool m_video_720p = false;
    bool m_video_1080i = false;
    bool m_video_60hz = false;
    bool m_video_50hz = false;
    bool m_audio_ac3 = false;
    bool m_audio_dts = false;
    bool m_misc_auto_off = false;
    bool m_misc_disable_dst = false;
    bool m_misc_disable_live_signin = false;
    bool m_misc_disable_live_policy = false;

    char m_confounder[17] = {};
    char m_hdd_key[33] = {};
    char m_serial[13] = {};
    char m_mac[13] = {};
    char m_online_key[33] = {};

    void Open(const char *configured_path);
    void DrawModal(bool *restart_dirty);
    bool Load();
    bool Save(std::string &error);
    bool PopulateFromEeprom();
    bool ApplyToEeprom(std::string &error);
    void SetStatus(const char *status, bool error = false);
    void GenerateHex(char *buffer, size_t byte_count,
                     const uint8_t *prefix = nullptr, size_t prefix_len = 0);
    void GenerateSerial();
};
