//
// xemu User Interface
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
#include "eeprom-editor.hh"

#include <algorithm>

#include "viewport-manager.hh"
#include "../xemu-notifications.h"

extern "C" {
#include "crypto/random.h"
#include "qapi/error.h"
#include "util/rc4.h"
#include "util/sha1.h"

typedef enum XboxEepromEditorVersion {
    XBOX_EEPROM_EDITOR_VERSION_D,
    XBOX_EEPROM_EDITOR_VERSION_R1,
    XBOX_EEPROM_EDITOR_VERSION_R2,
    XBOX_EEPROM_EDITOR_VERSION_R3,
} XboxEepromEditorVersion;

bool xbox_eeprom_generate(const char *file, XboxEepromEditorVersion ver);
}

namespace {

constexpr size_t kHashOffset = 0x00;
constexpr size_t kHashSize = 20;
constexpr size_t kSecurityOffset = 0x14;
constexpr size_t kSecuritySize = 0x1c;
constexpr size_t kXboxRegionOffset = 0x2c;
constexpr size_t kFactoryChecksumOffset = 0x30;
constexpr size_t kSerialOffset = 0x34;
constexpr size_t kMacOffset = 0x40;
constexpr size_t kOnlineKeyOffset = 0x48;
constexpr size_t kVideoStandardOffset = 0x58;
constexpr size_t kUserChecksumOffset = 0x60;
constexpr size_t kLanguageOffset = 0x90;
constexpr size_t kVideoSettingsOffset = 0x94;
constexpr size_t kAudioSettingsOffset = 0x98;
constexpr size_t kMiscFlagsOffset = 0xb8;
constexpr size_t kDvdRegionOffset = 0xbc;

constexpr uint32_t kVideoAspectMask = 0x00110000;
constexpr uint32_t kVideoKnownMask = kVideoAspectMask | 0x00080000 |
                                     0x00020000 | 0x00040000 |
                                     0x00400000 | 0x00800000;
constexpr uint32_t kAudioKnownMask = 0x00000003 | 0x00010000 | 0x00020000;
constexpr uint32_t kMiscKnownMask = 0x0000000f;
constexpr const char *kSurroundWarning =
    "xemu doesn't support surround sound at this time, do not enable this in your eeprom.";

struct Choice {
    const char *label;
    uint32_t value;
};

struct VideoOutputSupport {
    bool hd_modes;
    bool hz60;
    bool hz50;
};

const Choice kHardwareRevisions[] = {
    { "Debug", XBOX_EEPROM_EDITOR_VERSION_D },
    { "1.0", XBOX_EEPROM_EDITOR_VERSION_R1 },
    { "1.1-1.4", XBOX_EEPROM_EDITOR_VERSION_R2 },
    { "1.6", XBOX_EEPROM_EDITOR_VERSION_R3 },
};

const Choice kXboxRegions[] = {
    { "North America", 0x00000001 },
    { "Japan", 0x00000002 },
    { "Europe / Australia", 0x00000004 },
    { "Manufacturing / Debug", 0x80000000 },
};

const Choice kVideoStandards[] = {
    { "NTSC", 0x00400100 },
    { "NTSC Japan", 0x00400200 },
    { "PAL", 0x00800300 },
    { "PAL Brazil", 0x00400400 },
};

const Choice kLanguages[] = {
    { "Not set", 0 },
    { "English", 1 },
    { "Japanese", 2 },
    { "German", 3 },
    { "French", 4 },
    { "Spanish", 5 },
    { "Italian", 6 },
    { "Korean", 7 },
    { "Chinese", 8 },
    { "Portuguese", 9 },
};

const Choice kVideoAspects[] = {
    { "Normal", 0x00000000 },
    { "Widescreen", 0x00010000 },
    { "Letterbox", 0x00100000 },
};

const Choice kAudioOutputs[] = {
    { "Stereo", 0x00000000 },
    { "Mono", 0x00000001 },
    { "Surround", 0x00000002 },
};

const Choice kDvdRegions[] = {
    { "None", 0 },
    { "1 USA / Canada", 1 },
    { "2 Europe / Japan / Middle East", 2 },
    { "3 Southeast Asia / South Korea", 3 },
    { "4 Latin America / Australia", 4 },
    { "5 Eastern Europe / Russia / Africa", 5 },
    { "6 China", 6 },
};

VideoOutputSupport VideoOutputSupportForStandard(int video_standard)
{
    if (video_standard < 0 ||
        video_standard >=
            (int)(sizeof(kVideoStandards) / sizeof(kVideoStandards[0]))) {
        video_standard = 0;
    }

    switch (kVideoStandards[video_standard].value) {
    case 0x00400100: /* NTSC-M */
    case 0x00400200: /* NTSC-J */
        return { true, true, false };
    case 0x00800300: /* PAL-I */
    case 0x00400400: /* PAL-M */
    default:
        return { false, true, true };
    }
}

template <size_t N>
int ChoiceIndexForValue(const Choice (&choices)[N], uint32_t value,
                        int fallback = 0)
{
    for (size_t i = 0; i < N; i++) {
        if (choices[i].value == value) {
            return (int)i;
        }
    }
    return fallback;
}

template <size_t N>
uint32_t ChoiceValue(const Choice (&choices)[N], int index)
{
    if (index < 0 || index >= (int)N) {
        index = 0;
    }
    return choices[index].value;
}

template <size_t N>
bool ChoiceCombo(const char *label, int *index, const Choice (&choices)[N])
{
    bool changed = false;
    if (*index < 0 || *index >= (int)N) {
        *index = 0;
    }

    if (ImGui::BeginCombo(label, choices[*index].label)) {
        for (size_t i = 0; i < N; i++) {
            const bool selected = *index == (int)i;
            if (ImGui::Selectable(choices[i].label, selected)) {
                *index = (int)i;
                changed = true;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool SupportedCheckbox(const char *label, bool *value, bool supported)
{
    ImVec4 color = supported ? ImVec4(0.25f, 0.55f, 1.0f, 1.0f)
                             : ImVec4(1.0f, 0.25f, 0.25f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_CheckMark, color);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,
                          ImVec4(color.x, color.y, color.z, 0.25f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,
                          ImVec4(color.x, color.y, color.z, 0.45f));
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    bool changed = ImGui::Checkbox(label, value);
    ImGui::PopStyleColor(4);

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
                          supported ?
                              "Supported by the selected video standard." :
                              "Not supported by the selected video standard.");
    }

    return changed;
}

uint32_t ReadLe32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

void WriteLe32(uint8_t *data, uint32_t value)
{
    data[0] = value & 0xff;
    data[1] = (value >> 8) & 0xff;
    data[2] = (value >> 16) & 0xff;
    data[3] = (value >> 24) & 0xff;
}

uint32_t EepromCrc(const uint8_t *data, size_t len)
{
    uint32_t high = 0;
    uint32_t low = 0;

    for (size_t i = 0; i < len; i += 4) {
        uint32_t val = ReadLe32(data + i);
        uint64_t sum = ((uint64_t)high << 32) | low;
        high = (uint32_t)((sum + val) >> 32);
        low += val;
    }

    return ~(high + low);
}

void XboxSha1Reset(SHA1Context *ctx, XboxEepromEditorVersion ver, bool first)
{
    ctx->msg_blk_index = 0;
    ctx->computed = false;
    ctx->length = 512;

    switch (ver) {
    case XBOX_EEPROM_EDITOR_VERSION_D:
        if (first) {
            sha1_fill(ctx, 0x85F9E51A, 0xE04613D2, 0x6D86A50C,
                      0x77C32E3C, 0x4BD717A4);
        } else {
            sha1_fill(ctx, 0x5D7A9C6B, 0xE1922BEB, 0xB82CCDBC,
                      0x3137AB34, 0x486B52B3);
        }
        break;
    case XBOX_EEPROM_EDITOR_VERSION_R2:
        if (first) {
            sha1_fill(ctx, 0x39B06E79, 0xC9BD25E8, 0xDBC6B498,
                      0x40B4389D, 0x86BBD7ED);
        } else {
            sha1_fill(ctx, 0x9B49BED3, 0x84B430FC, 0x6B8749CD,
                      0xEBFE5FE5, 0xD96E7393);
        }
        break;
    case XBOX_EEPROM_EDITOR_VERSION_R3:
        if (first) {
            sha1_fill(ctx, 0x8058763A, 0xF97D4E0E, 0x865A9762,
                      0x8A3D920D, 0x08995B2C);
        } else {
            sha1_fill(ctx, 0x01075307, 0xA2F1E037, 0x1186EEEA,
                      0x88DA9992, 0x168A5609);
        }
        break;
    case XBOX_EEPROM_EDITOR_VERSION_R1:
    default:
        if (first) {
            sha1_fill(ctx, 0x72127625, 0x336472B9, 0xBE609BEA,
                      0xF55E226B, 0x99958DAC);
        } else {
            sha1_fill(ctx, 0x76441D41, 0x4DE82659, 0x2E8EF85E,
                      0xB256FACA, 0xC4FE2DE8);
        }
    }
}

void XboxSha1Compute(XboxEepromEditorVersion ver, const uint8_t *data,
                     size_t len, uint8_t *hash)
{
    SHA1Context ctx;
    XboxSha1Reset(&ctx, ver, true);
    sha1_input(&ctx, const_cast<uint8_t *>(data), len);
    sha1_result(&ctx, ctx.msg_blk);
    XboxSha1Reset(&ctx, ver, false);
    sha1_input(&ctx, ctx.msg_blk, kHashSize);
    sha1_result(&ctx, hash);
}

void BytesToHex(const uint8_t *data, size_t len, char *out, size_t out_len)
{
    static const char *digits = "0123456789ABCDEF";
    size_t needed = len * 2 + 1;
    if (out_len < needed) {
        return;
    }

    for (size_t i = 0; i < len; i++) {
        out[i * 2] = digits[data[i] >> 4];
        out[i * 2 + 1] = digits[data[i] & 0xf];
    }
    out[len * 2] = 0;
}

int HexValue(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

bool ParseHex(const char *text, size_t byte_count, uint8_t *out,
              const char *label, std::string &error)
{
    if (strlen(text) != byte_count * 2) {
        error = std::string(label) + " must be " +
                std::to_string(byte_count * 2) + " hex characters.";
        return false;
    }

    for (size_t i = 0; i < byte_count; i++) {
        int high = HexValue(text[i * 2]);
        int low = HexValue(text[i * 2 + 1]);
        if (high < 0 || low < 0) {
            error = std::string(label) + " contains non-hex characters.";
            return false;
        }
        out[i] = (uint8_t)((high << 4) | low);
    }

    return true;
}

bool ParseSerial(const char *text, uint8_t *out, std::string &error)
{
    if (strlen(text) != 12) {
        error = "Serial number must be 12 digits.";
        return false;
    }

    for (size_t i = 0; i < 12; i++) {
        if (!g_ascii_isdigit(text[i])) {
            error = "Serial number must contain digits only.";
            return false;
        }
        out[i] = (uint8_t)text[i];
    }

    return true;
}

void DrawHexInput(const char *label, char *buffer, size_t buffer_size)
{
    ImGui::InputText(label, buffer, buffer_size,
                     ImGuiInputTextFlags_CharsHexadecimal |
                         ImGuiInputTextFlags_CharsUppercase |
                         ImGuiInputTextFlags_CharsNoBlank);
}

extern "C" {
extern char **gArgv;
}

void RestartXemu()
{
    xemu_settings_save();

#ifdef _WIN32
    _execv(gArgv[0], gArgv);
#else
    execv(gArgv[0], gArgv);
#endif

    xemu_queue_error_message("Failed to restart xemu.");
}

} // namespace

void MainMenuEepromEditor::SetStatus(const char *status, bool error)
{
    m_status = status ? status : "";
    m_status_error = error;
}

void MainMenuEepromEditor::Open(const char *configured_path)
{
    const char *path = configured_path;
    if (!path || !path[0]) {
        path = xemu_settings_get_default_eeprom_path();
        xemu_settings_set_string(&g_config.sys.files.eeprom_path, path);
    }

    m_path = path;
    m_saved = false;
    Load();
    m_open_requested = true;
}

bool MainMenuEepromEditor::Load()
{
    m_loaded = false;

    if (m_path.empty()) {
        SetStatus("No EEPROM path selected.", true);
        return false;
    }

    if (qemu_access(m_path.c_str(), F_OK) == -1) {
        if (!xbox_eeprom_generate(m_path.c_str(), XBOX_EEPROM_EDITOR_VERSION_R1)) {
            SetStatus("Failed to generate EEPROM file.", true);
            return false;
        }
    }

    FILE *fd = qemu_fopen(m_path.c_str(), "rb");
    if (!fd) {
        SetStatus("Failed to open EEPROM file.", true);
        return false;
    }

    size_t read = fread(m_eeprom.data(), 1, m_eeprom.size(), fd);
    bool eof = fgetc(fd) == EOF;
    fclose(fd);

    if (read != m_eeprom.size() || !eof) {
        SetStatus("Invalid EEPROM file size. Expected exactly 256 bytes.",
                  true);
        return false;
    }

    if (!PopulateFromEeprom()) {
        return false;
    }

    m_loaded = true;
    SetStatus("EEPROM loaded.");
    return true;
}

bool MainMenuEepromEditor::PopulateFromEeprom()
{
    const uint8_t *stored_hash = m_eeprom.data() + kHashOffset;
    uint8_t encrypted[kSecuritySize];
    memcpy(encrypted, m_eeprom.data() + kSecurityOffset, sizeof(encrypted));

    uint8_t decrypted[kSecuritySize];
    bool decrypted_ok = false;

    for (int i = 0; i < (int)G_N_ELEMENTS(kHardwareRevisions); i++) {
        auto version = (XboxEepromEditorVersion)kHardwareRevisions[i].value;
        uint8_t seed[kHashSize];
        uint8_t test_hash[kHashSize];
        memcpy(decrypted, encrypted, sizeof(decrypted));

        XboxSha1Compute(version, stored_hash, kHashSize, seed);

        RC4Context rc4;
        rc4_init(&rc4, seed, sizeof(seed));
        rc4_crypt(&rc4, decrypted, sizeof(decrypted));

        XboxSha1Compute(version, decrypted, sizeof(decrypted), test_hash);
        if (!memcmp(test_hash, stored_hash, kHashSize)) {
            m_hardware_revision = i;
            decrypted_ok = true;
            break;
        }
    }

    if (!decrypted_ok) {
        SetStatus("Could not decrypt EEPROM security section.", true);
        return false;
    }

    BytesToHex(decrypted, 8, m_confounder, sizeof(m_confounder));
    BytesToHex(decrypted + 8, 16, m_hdd_key, sizeof(m_hdd_key));
    m_xbox_region = ChoiceIndexForValue(
        kXboxRegions, ReadLe32(decrypted + kXboxRegionOffset - kSecurityOffset));

    for (size_t i = 0; i < 12; i++) {
        uint8_t c = m_eeprom[kSerialOffset + i];
        m_serial[i] = g_ascii_isdigit(c) ? (char)c : '0';
    }
    m_serial[12] = 0;

    BytesToHex(m_eeprom.data() + kMacOffset, 6, m_mac, sizeof(m_mac));
    BytesToHex(m_eeprom.data() + kOnlineKeyOffset, 16, m_online_key,
               sizeof(m_online_key));

    m_video_standard =
        ChoiceIndexForValue(kVideoStandards,
                            ReadLe32(m_eeprom.data() + kVideoStandardOffset));
    m_language = ChoiceIndexForValue(
        kLanguages, ReadLe32(m_eeprom.data() + kLanguageOffset), 1);
    m_dvd_region = ChoiceIndexForValue(
        kDvdRegions, ReadLe32(m_eeprom.data() + kDvdRegionOffset));

    uint32_t video = ReadLe32(m_eeprom.data() + kVideoSettingsOffset);
    m_video_aspect = ChoiceIndexForValue(kVideoAspects,
                                         video & kVideoAspectMask);
    m_video_480p = video & 0x00080000;
    m_video_720p = video & 0x00020000;
    m_video_1080i = video & 0x00040000;
    m_video_60hz = video & 0x00400000;
    m_video_50hz = video & 0x00800000;

    uint32_t audio = ReadLe32(m_eeprom.data() + kAudioSettingsOffset);
    m_audio_output = ChoiceIndexForValue(kAudioOutputs, audio & 0x00000003);
    m_audio_ac3 = audio & 0x00010000;
    m_audio_dts = audio & 0x00020000;

    uint32_t misc = ReadLe32(m_eeprom.data() + kMiscFlagsOffset);
    m_misc_auto_off = misc & 0x00000001;
    m_misc_disable_dst = misc & 0x00000002;
    m_misc_disable_live_signin = misc & 0x00000004;
    m_misc_disable_live_policy = misc & 0x00000008;

    return true;
}

bool MainMenuEepromEditor::ApplyToEeprom(std::string &error)
{
    uint8_t decrypted[kSecuritySize];
    if (!ParseHex(m_confounder, 8, decrypted, "Confounder", error) ||
        !ParseHex(m_hdd_key, 16, decrypted + 8, "Hard drive key", error)) {
        return false;
    }
    WriteLe32(decrypted + kXboxRegionOffset - kSecurityOffset,
              ChoiceValue(kXboxRegions, m_xbox_region));

    if (!ParseSerial(m_serial, m_eeprom.data() + kSerialOffset, error) ||
        !ParseHex(m_mac, 6, m_eeprom.data() + kMacOffset, "MAC address",
                  error) ||
        !ParseHex(m_online_key, 16, m_eeprom.data() + kOnlineKeyOffset,
                  "Online key", error)) {
        return false;
    }

    WriteLe32(m_eeprom.data() + kVideoStandardOffset,
              ChoiceValue(kVideoStandards, m_video_standard));
    WriteLe32(m_eeprom.data() + kLanguageOffset,
              ChoiceValue(kLanguages, m_language));
    WriteLe32(m_eeprom.data() + kDvdRegionOffset,
              ChoiceValue(kDvdRegions, m_dvd_region));

    uint32_t video = ReadLe32(m_eeprom.data() + kVideoSettingsOffset);
    video &= ~kVideoKnownMask;
    video |= ChoiceValue(kVideoAspects, m_video_aspect);
    if (m_video_480p) {
        video |= 0x00080000;
    }
    if (m_video_720p) {
        video |= 0x00020000;
    }
    if (m_video_1080i) {
        video |= 0x00040000;
    }
    if (m_video_60hz) {
        video |= 0x00400000;
    }
    if (m_video_50hz) {
        video |= 0x00800000;
    }
    WriteLe32(m_eeprom.data() + kVideoSettingsOffset, video);

    uint32_t audio = ReadLe32(m_eeprom.data() + kAudioSettingsOffset);
    audio &= ~kAudioKnownMask;
    audio |= ChoiceValue(kAudioOutputs, m_audio_output);
    if (m_audio_ac3) {
        audio |= 0x00010000;
    }
    if (m_audio_dts) {
        audio |= 0x00020000;
    }
    WriteLe32(m_eeprom.data() + kAudioSettingsOffset, audio);

    uint32_t misc = ReadLe32(m_eeprom.data() + kMiscFlagsOffset);
    misc &= ~kMiscKnownMask;
    if (m_misc_auto_off) {
        misc |= 0x00000001;
    }
    if (m_misc_disable_dst) {
        misc |= 0x00000002;
    }
    if (m_misc_disable_live_signin) {
        misc |= 0x00000004;
    }
    if (m_misc_disable_live_policy) {
        misc |= 0x00000008;
    }
    WriteLe32(m_eeprom.data() + kMiscFlagsOffset, misc);

    WriteLe32(m_eeprom.data() + kFactoryChecksumOffset,
              EepromCrc(m_eeprom.data() + kSerialOffset, 0x2c));
    WriteLe32(m_eeprom.data() + kUserChecksumOffset,
              EepromCrc(m_eeprom.data() + 0x64, 0x5c));

    auto version = (XboxEepromEditorVersion)ChoiceValue(
        kHardwareRevisions, m_hardware_revision);
    uint8_t hash[kHashSize];
    uint8_t seed[kHashSize];
    uint8_t encrypted[kSecuritySize];

    XboxSha1Compute(version, decrypted, sizeof(decrypted), hash);
    XboxSha1Compute(version, hash, sizeof(hash), seed);
    memcpy(encrypted, decrypted, sizeof(encrypted));

    RC4Context rc4;
    rc4_init(&rc4, seed, sizeof(seed));
    rc4_crypt(&rc4, encrypted, sizeof(encrypted));

    memcpy(m_eeprom.data() + kHashOffset, hash, sizeof(hash));
    memcpy(m_eeprom.data() + kSecurityOffset, encrypted, sizeof(encrypted));

    return true;
}

bool MainMenuEepromEditor::Save(std::string &error)
{
    if (!ApplyToEeprom(error)) {
        return false;
    }

    FILE *fd = qemu_fopen(m_path.c_str(), "wb");
    if (!fd) {
        error = "Failed to open EEPROM file for writing.";
        return false;
    }

    size_t written = fwrite(m_eeprom.data(), 1, m_eeprom.size(), fd);
    bool close_ok = fclose(fd) == 0;
    if (written != m_eeprom.size() || !close_ok) {
        error = "Failed to write EEPROM file.";
        return false;
    }

    return true;
}

void MainMenuEepromEditor::GenerateHex(char *buffer, size_t byte_count,
                                       const uint8_t *prefix,
                                       size_t prefix_len)
{
    uint8_t bytes[32] = {};
    assert(byte_count <= sizeof(bytes));
    if (prefix && prefix_len) {
        memcpy(bytes, prefix, std::min(prefix_len, byte_count));
    }

    Error *err = nullptr;
    if (prefix_len < byte_count &&
        qcrypto_random_bytes(bytes + prefix_len, byte_count - prefix_len,
                             &err) < 0) {
        std::string msg = "Failed to generate random bytes.";
        if (err) {
            msg += " ";
            msg += error_get_pretty(err);
            error_free(err);
        }
        SetStatus(msg.c_str(), true);
        return;
    }

    BytesToHex(bytes, byte_count, buffer, byte_count * 2 + 1);
}

void MainMenuEepromEditor::GenerateSerial()
{
    uint8_t bytes[12];
    Error *err = nullptr;
    if (qcrypto_random_bytes(bytes, sizeof(bytes), &err) < 0) {
        std::string msg = "Failed to generate random serial.";
        if (err) {
            msg += " ";
            msg += error_get_pretty(err);
            error_free(err);
        }
        SetStatus(msg.c_str(), true);
        return;
    }

    for (size_t i = 0; i < sizeof(bytes); i++) {
        m_serial[i] = '0' + (bytes[i] % 10);
    }
    m_serial[sizeof(bytes)] = 0;
}

void MainMenuEepromEditor::Draw(const char *configured_path,
                                bool *restart_dirty)
{
    ImGui::PushID("EEPROMEditor");
    if (ImGui::Button("Edit EEPROM...", ImVec2(220 * g_viewport_mgr.m_scale, 0))) {
        Open(configured_path);
    }

    if (m_open_requested) {
        ImGui::OpenPopup("EEPROM Editor");
        m_open_requested = false;
    }

    DrawModal(restart_dirty);
    ImGui::PopID();
}

void MainMenuEepromEditor::DrawModal(bool *restart_dirty)
{
    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.72f,
                                    io.DisplaySize.y * 0.72f),
                             ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f,
                                   io.DisplaySize.y * 0.5f),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    bool open = true;
    if (!ImGui::BeginPopupModal("EEPROM Editor", &open,
                                ImGuiWindowFlags_NoSavedSettings)) {
        return;
    }

    ImGui::TextWrapped("%s", m_path.c_str());
    if (!m_status.empty()) {
        ImVec4 color = m_status_error ? ImVec4(1, 0.35f, 0.35f, 1)
                                      : ImVec4(0.55f, 1, 0.55f, 1);
        ImGui::TextColored(color, "%s", m_status.c_str());
    }
    ImGui::Separator();

    if (!m_loaded) {
        ImGui::BeginDisabled();
    }

    if (ImGui::BeginTabBar("##eeprom_tabs")) {
        if (ImGui::BeginTabItem("Security")) {
            ChoiceCombo("Hardware revision", &m_hardware_revision,
                        kHardwareRevisions);

            DrawHexInput("Confounder", m_confounder, sizeof(m_confounder));
            ImGui::SameLine();
            if (ImGui::Button("Generate##confounder")) {
                GenerateHex(m_confounder, 8);
            }

            DrawHexInput("Hard drive key", m_hdd_key, sizeof(m_hdd_key));
            ImGui::SameLine();
            if (ImGui::Button("Generate##hdd_key")) {
                GenerateHex(m_hdd_key, 16);
            }

            ChoiceCombo("Xbox region", &m_xbox_region, kXboxRegions);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Factory")) {
            ImGui::InputText("Serial", m_serial, sizeof(m_serial),
                             ImGuiInputTextFlags_CharsDecimal |
                                 ImGuiInputTextFlags_CharsNoBlank);
            ImGui::SameLine();
            if (ImGui::Button("Generate##serial")) {
                GenerateSerial();
            }

            DrawHexInput("MAC address", m_mac, sizeof(m_mac));
            ImGui::SameLine();
            if (ImGui::Button("Generate##mac")) {
                const uint8_t prefix[3] = { 0x00, 0x50, 0xf2 };
                GenerateHex(m_mac, 6, prefix, sizeof(prefix));
            }

            DrawHexInput("Online key", m_online_key, sizeof(m_online_key));
            ImGui::SameLine();
            if (ImGui::Button("Generate##online_key")) {
                GenerateHex(m_online_key, 16);
            }

            ChoiceCombo("Video standard", &m_video_standard,
                        kVideoStandards);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("User")) {
            ChoiceCombo("Language", &m_language, kLanguages);
            ChoiceCombo("DVD region", &m_dvd_region, kDvdRegions);
            ChoiceCombo("Video output", &m_video_standard,
                        kVideoStandards);
            ChoiceCombo("Video aspect", &m_video_aspect, kVideoAspects);

            const VideoOutputSupport video_support =
                VideoOutputSupportForStandard(m_video_standard);
            bool unsupported_video_selected =
                (!video_support.hd_modes &&
                 (m_video_480p || m_video_720p || m_video_1080i)) ||
                (!video_support.hz60 && m_video_60hz) ||
                (!video_support.hz50 && m_video_50hz);

            ImGui::Text("Supported modes for %s",
                        kVideoStandards[m_video_standard].label);
            SupportedCheckbox("480p", &m_video_480p,
                              video_support.hd_modes);
            ImGui::SameLine();
            SupportedCheckbox("720p", &m_video_720p,
                              video_support.hd_modes);
            ImGui::SameLine();
            SupportedCheckbox("1080i", &m_video_1080i,
                              video_support.hd_modes);
            SupportedCheckbox("60 Hz", &m_video_60hz,
                              video_support.hz60);
            ImGui::SameLine();
            SupportedCheckbox("50 Hz", &m_video_50hz,
                              video_support.hz50);
            if (unsupported_video_selected) {
                ImGui::TextColored(
                    ImVec4(1, 0.35f, 0.35f, 1),
                    "Red video modes are not supported by the selected video standard.");
            }

            if (ChoiceCombo("Audio output", &m_audio_output, kAudioOutputs) &&
                ChoiceValue(kAudioOutputs, m_audio_output) == 0x00000002) {
                SetStatus(kSurroundWarning, true);
            }
            if (ChoiceValue(kAudioOutputs, m_audio_output) == 0x00000002) {
                ImGui::TextColored(ImVec4(1, 0.35f, 0.35f, 1), "%s",
                                   kSurroundWarning);
            }
            ImGui::Checkbox("AC3", &m_audio_ac3);
            ImGui::SameLine();
            ImGui::Checkbox("DTS", &m_audio_dts);

            ImGui::Separator();
            ImGui::Checkbox("6 hour automatic shutdown",
                            &m_misc_auto_off);
            ImGui::Checkbox("Disable automatic daylight savings adjustment",
                            &m_misc_disable_dst);
            ImGui::Checkbox("Disable automatic Xbox Live sign-in",
                            &m_misc_disable_live_signin);
            ImGui::Checkbox("Disable Xbox Live policy on next boot",
                            &m_misc_disable_live_policy);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    if (!m_loaded) {
        ImGui::EndDisabled();
    }

    ImGui::Separator();
    if (ImGui::Button("Reload", ImVec2(120 * g_viewport_mgr.m_scale, 0))) {
        Load();
    }
    ImGui::SameLine();

    if (!m_loaded) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Save", ImVec2(120 * g_viewport_mgr.m_scale, 0))) {
        std::string error;
        if (Save(error)) {
            SetStatus("EEPROM saved. Restart required to use changes.");
            m_saved = true;
            xemu_queue_notification("EEPROM saved");
            if (restart_dirty) {
                *restart_dirty = true;
            }
        } else {
            SetStatus(error.c_str(), true);
        }
    }
    if (!m_loaded) {
        ImGui::EndDisabled();
    }

    if (m_saved) {
        ImGui::SameLine();
        if (ImGui::Button("Restart xemu",
                          ImVec2(150 * g_viewport_mgr.m_scale, 0))) {
            RestartXemu();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Close", ImVec2(120 * g_viewport_mgr.m_scale, 0))) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}
