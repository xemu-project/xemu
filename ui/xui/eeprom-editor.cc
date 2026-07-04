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
#include "qemu/osdep.h"
#include "eeprom-editor.hh"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <glib/gstdio.h>

#ifdef _WIN32
#include <io.h>
#include <process.h>
#else
#include <unistd.h>
#endif

#include "viewport-manager.hh"
#include "../xemu-notifications.h"
#include "../xemu-settings.h"

extern "C" {
#include "crypto/random.h"
#include "qapi/error.h"
#include "util/rc4.h"
#include "util/sha1.h"

/* Local mirror of XboxEEPROMVersion / xbox_eeprom_generate() from
 * hw/xbox/eeprom_generation.h. C linkage means no cross-TU type checking,
 * so the enumerator order below must stay in sync with that header. */
typedef enum XboxEepromEditorVersion {
    XBOX_EEPROM_EDITOR_VERSION_D,
    XBOX_EEPROM_EDITOR_VERSION_R1,
    XBOX_EEPROM_EDITOR_VERSION_R2,
    XBOX_EEPROM_EDITOR_VERSION_R3,
} XboxEepromEditorVersion;

bool xbox_eeprom_generate(const char *file, XboxEepromEditorVersion ver);
}

namespace {

/* EEPROM layout as expected by the retail kernel: a 20-byte keyed SHA1
 * digest over the security section, the RC4-encrypted security section
 * (confounder, HDD key, region), then the plaintext factory section
 * (0x34-0x5f, checksummed at 0x30) and user section (0x64-0xbf,
 * checksummed at 0x60). */
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
constexpr size_t kGameRatingOffset = 0x9c;
constexpr size_t kPasscodeOffset = 0xa0;
constexpr size_t kMovieRatingOffset = 0xa4;
constexpr size_t kLiveIpOffset = 0xa8;
constexpr size_t kLiveDnsOffset = 0xac;
constexpr size_t kLiveGatewayOffset = 0xb0;
constexpr size_t kLiveSubnetOffset = 0xb4;
constexpr size_t kMiscFlagsOffset = 0xb8;
constexpr size_t kDvdRegionOffset = 0xbc;

/* Masks of the flag bits this editor manages. Saving clears only these,
 * so any bits we don't understand survive a load/save round trip. */
constexpr uint32_t kVideoAspectMask = 0x00110000;
constexpr uint32_t kVideoKnownMask = kVideoAspectMask | 0x00080000 |
                                     0x00020000 | 0x00040000 |
                                     0x00400000 | 0x00800000;
constexpr uint32_t kAudioKnownMask = 0x00000003 | 0x00010000 | 0x00020000;
constexpr uint32_t kMiscKnownMask = 0x0000000f;
constexpr const char *kSurroundWarning =
    "xemu doesn't support surround sound at this time, do not enable this in your eeprom.";
constexpr float kEditorFieldWidth = 480.0f;
constexpr float kPasscodeFieldWidth = 96.0f;
constexpr float kButtonWidth = 120.0f;
constexpr float kRestartButtonWidth = 150.0f;
constexpr float kFormLabelWidth = 145.0f;
constexpr float kGenerateButtonWidth = 86.0f;

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

/* The video standard values embed the refresh-rate flags also used in the
 * video settings word: 0x00400000 = 60 Hz capable, 0x00800000 = 50 Hz
 * capable. VideoOutputSupportForStandard must stay consistent with them. */
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

const Choice kGameRatings[] = {
    { "Unrated", 0 },
    { "Adults Only (AO)", 1 },
    { "Mature (M)", 2 },
    { "Teen (T)", 3 },
    { "Everyone (E)", 4 },
    { "Kids to Adults (K-A)", 5 },
    { "Early Childhood (EC)", 6 },
};

const Choice kMovieRatings[] = {
    { "Unrated", 0 },
    { "Adults Only (NC-17)", 1 },
    { "Restricted (R)", 2 },
    { "Parents Strongly Cautioned (PG-13)", 4 },
    { "Parental Guidance Suggested (PG)", 5 },
    { "General Audiences (G)", 7 },
};

const Choice kPasscodeButtons[] = {
    { "None", 0x0 },
    { "Up", 0x1 },
    { "Down", 0x2 },
    { "Left", 0x3 },
    { "Right", 0x4 },
    { "X", 0x7 },
    { "Y", 0x8 },
    { "L Trigger", 0xb },
    { "R Trigger", 0xc },
};

const char *const kTabLabels[] = {
    "Security",
    "Factory",
    "User",
    "Parental",
    "Live",
};

float ScaledWidth(float width)
{
    return width * g_viewport_mgr.m_scale;
}

void SetEditorFieldWidth(float width = kEditorFieldWidth)
{
    ImGui::SetNextItemWidth(ScaledWidth(width));
}

float EepromTabBarWidth()
{
    const ImGuiStyle &style = ImGui::GetStyle();
    float width = 0.0f;

    for (size_t i = 0; i < G_N_ELEMENTS(kTabLabels); i++) {
        width += ImGui::CalcTextSize(kTabLabels[i]).x +
                 style.FramePadding.x * 2.0f;
    }

    width += style.ItemInnerSpacing.x * (G_N_ELEMENTS(kTabLabels) - 1);
    return width;
}

void CenterNextItem(float width)
{
    float avail = ImGui::GetContentRegionAvail().x;

    if (avail > width) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - width) * 0.5f);
    }
}

float LabeledItemWidth(const char *label, float item_width)
{
    const ImGuiStyle &style = ImGui::GetStyle();
    const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);
    float width = ScaledWidth(item_width);

    if (label_size.x > 0.0f) {
        width += style.ItemInnerSpacing.x + label_size.x;
    }

    return width;
}

void CenterLabeledItem(const char *label, float item_width,
                       float extra_width = 0.0f)
{
    CenterNextItem(LabeledItemWidth(label, item_width) + extra_width);
}

float FormRowWidth()
{
    const ImGuiStyle &style = ImGui::GetStyle();
    return ScaledWidth(kEditorFieldWidth) + style.ItemInnerSpacing.x +
           ScaledWidth(kFormLabelWidth) + style.ItemSpacing.x +
           ScaledWidth(kGenerateButtonWidth);
}

float BeginFormRow()
{
    CenterNextItem(FormRowWidth());
    return ImGui::GetCursorPosX();
}

void DrawFormLabel()
{
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::AlignTextToFramePadding();
}

bool DrawFormButton(float row_start, const char *label)
{
    const ImGuiStyle &style = ImGui::GetStyle();
    float button_x = row_start + ScaledWidth(kEditorFieldWidth) +
                     style.ItemInnerSpacing.x + ScaledWidth(kFormLabelWidth) +
                     style.ItemSpacing.x;

    ImGui::SameLine(button_x, 0.0f);
    return ImGui::Button(label, ImVec2(ScaledWidth(kGenerateButtonWidth), 0));
}

void DrawFullWidthTabLine(float y)
{
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 region_min = ImGui::GetWindowContentRegionMin();
    ImVec2 region_max = ImGui::GetWindowContentRegionMax();

    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(window_pos.x + region_min.x, y),
        ImVec2(window_pos.x + region_max.x, y),
        ImGui::GetColorU32(ImGuiCol_Tab), g_viewport_mgr.m_scale);
}

/* Which output options the dashboard actually offers per video standard:
 * HD modes and 60 Hz are NTSC features, PAL-I consoles can additionally
 * toggle PAL-60, and PAL-M uses NTSC timing (60 Hz, no 50 Hz mode). */
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
    case 0x00400400: /* PAL-M, 60 Hz NTSC timing */
        return { false, true, false };
    case 0x00800300: /* PAL-I */
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
bool ChoiceComboWidget(const char *label, int *index,
                       const Choice (&choices)[N],
                       float width = kEditorFieldWidth)
{
    bool changed = false;
    if (*index < 0 || *index >= (int)N) {
        *index = 0;
    }

    SetEditorFieldWidth(width);
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

template <size_t N>
bool ChoiceCombo(const char *label, int *index, const Choice (&choices)[N],
                 float width = kEditorFieldWidth, bool center = true)
{
    if (center) {
        CenterLabeledItem(label, width);
    }
    return ChoiceComboWidget(label, index, choices, width);
}

template <size_t N>
bool ChoiceComboRow(const char *id, const char *label, int *index,
                    const Choice (&choices)[N])
{
    BeginFormRow();
    bool changed = ChoiceComboWidget(id, index, choices);

    DrawFormLabel();
    ImGui::TextUnformatted(label);

    return changed;
}

bool SupportedCheckbox(const char *label, bool *value, bool supported)
{
    ImVec4 color = supported ? ImVec4(0.35f, 0.9f, 0.35f, 1.0f)
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

/* Checksum the kernel uses for the factory and user sections: a 64-bit
 * carry-folding sum over little-endian 32-bit words, inverted. Must match
 * xbox_eeprom_crc() in hw/xbox/eeprom_generation.c. */
uint32_t EepromCrc(const uint8_t *data, size_t len)
{
    assert(len % 4 == 0);
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

/* The kernel keys its EEPROM hash by starting SHA1 from a precomputed
 * internal state instead of the standard initialisation vector; the state
 * pairs differ per kernel version. Each state is the result of hashing one
 * secret 64-byte block, hence the message bit count starts at 512. The
 * constants are documented in "The Middle Message" paper:
 * https://web.archive.org/web/20040618164907/http://www.xbox-linux.org/down/The%20Middle%20Message-1a.pdf
 */
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

/* Keyed hash over the security section: SHA1 with the version's first
 * state, then the 20-byte digest is hashed again with the second state.
 * ctx.msg_blk (64 bytes) doubles as scratch for the intermediate digest;
 * XboxSha1Reset clears the block index before it is read again. */
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

bool DrawHexInputRow(const char *id, const char *label, char *buffer,
                     size_t buffer_size, const char *button_label = nullptr)
{
    bool button_column = button_label != nullptr;
    float row_start = BeginFormRow();
    SetEditorFieldWidth();
    ImGui::InputText(id, buffer, buffer_size,
                     ImGuiInputTextFlags_CharsHexadecimal |
                         ImGuiInputTextFlags_CharsUppercase |
                         ImGuiInputTextFlags_CharsNoBlank);

    DrawFormLabel();
    ImGui::TextUnformatted(label);

    if (button_column) {
        return DrawFormButton(row_start, button_label);
    }

    return false;
}

int DigitsOnlyFilter(ImGuiInputTextCallbackData *data)
{
    return (data->EventChar >= '0' && data->EventChar <= '9') ? 0 : 1;
}

bool DrawTextInputRow(const char *id, const char *label, char *buffer,
                      size_t buffer_size, ImGuiInputTextFlags flags = 0,
                      const char *button_label = nullptr,
                      ImGuiInputTextCallback callback = nullptr)
{
    bool button_column = button_label != nullptr;
    float row_start = BeginFormRow();
    SetEditorFieldWidth();
    ImGui::InputText(id, buffer, buffer_size, flags, callback);

    DrawFormLabel();
    ImGui::TextUnformatted(label);

    if (button_column) {
        return DrawFormButton(row_start, button_label);
    }

    return false;
}

void AlignToFormField()
{
    float form_start = (ImGui::GetContentRegionAvail().x - FormRowWidth()) * 0.5f;

    if (form_start > 0.0f) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + form_start);
    }
}

void FormatIpv4(const uint8_t *data, char *buffer, size_t buffer_size)
{
    snprintf(buffer, buffer_size, "%u.%u.%u.%u", data[0], data[1], data[2],
             data[3]);
}

bool ParseIpv4(const char *text, uint8_t *out, const char *label,
               std::string &error)
{
    unsigned int octets[4];
    char tail;

    if (sscanf(text, "%u.%u.%u.%u %c", &octets[0], &octets[1], &octets[2],
               &octets[3], &tail) != 4) {
        error = std::string(label) + " must be an IPv4 address.";
        return false;
    }

    for (size_t i = 0; i < 4; i++) {
        if (octets[i] > 255) {
            error = std::string(label) + " contains an invalid IPv4 octet.";
            return false;
        }
        out[i] = (uint8_t)octets[i];
    }

    return true;
}

extern "C" {
extern int gArgc;
extern char **gArgv;
}

void RestartXemu()
{
    xemu_settings_save();

    if (!gArgv || gArgc < 1 || !gArgv[0]) {
        xemu_queue_error_message("Failed to restart xemu: command line arguments are missing.");
        return;
    }

    /* Startup strips -config_path and its value out of gArgv by nulling the
     * entries in place, so rebuild the argument list by index instead of
     * stopping at the first null, and restore the effective config path. */
    GPtrArray *argv = g_ptr_array_new();
    bool stripped = false;
    for (int i = 0; i < gArgc; i++) {
        if (gArgv[i]) {
            g_ptr_array_add(argv, gArgv[i]);
        } else {
            stripped = true;
        }
    }
    if (stripped) {
        g_ptr_array_add(argv, (gpointer)"-config_path");
        g_ptr_array_add(argv, (gpointer)xemu_settings_get_path());
    }
    g_ptr_array_add(argv, nullptr);

#ifdef _WIN32
    /* The narrow CRT exec functions interpret their arguments in the ANSI
     * code page, which breaks UTF-8 paths, so convert everything to UTF-16
     * and use _wexecvp. It joins argv into a command line without quoting,
     * so arguments containing spaces must be quoted here to survive the
     * round-trip through CommandLineToArgv in the new process. */
    GPtrArray *args = g_ptr_array_new_with_free_func(g_free);
    bool converted = true;
    for (guint n = 0; converted && n + 1 < argv->len; n++) {
        const char *arg = (const char *)g_ptr_array_index(argv, n);
        gchar *quoted_arg;
        if (strpbrk(arg, " \t\"")) {
            GString *quoted = g_string_new("\"");
            for (const char *c = arg;; c++) {
                size_t backslashes = 0;
                while (*c == '\\') {
                    backslashes++;
                    c++;
                }
                if (*c == '\0') {
                    /* Backslashes before the closing quote must be doubled
                     * so they don't escape it. */
                    backslashes *= 2;
                } else if (*c == '"') {
                    /* Backslashes before a quote must be doubled, and the
                     * quote itself escaped. */
                    backslashes = backslashes * 2 + 1;
                }
                for (size_t i = 0; i < backslashes; i++) {
                    g_string_append_c(quoted, '\\');
                }
                if (*c == '\0') {
                    break;
                }
                g_string_append_c(quoted, *c);
            }
            g_string_append_c(quoted, '"');
            quoted_arg = g_string_free(quoted, FALSE);
        } else {
            quoted_arg = g_strdup(arg);
        }

        gunichar2 *warg = g_utf8_to_utf16(quoted_arg, -1, NULL, NULL, NULL);
        g_free(quoted_arg);
        if (warg) {
            g_ptr_array_add(args, warg);
        } else {
            converted = false;
        }
    }

    gunichar2 *wfile = g_utf8_to_utf16(gArgv[0], -1, NULL, NULL, NULL);
    if (converted && wfile) {
        g_ptr_array_add(args, nullptr);
        _wexecvp((const wchar_t *)wfile,
                 (const wchar_t *const *)args->pdata);
    }
    g_free(wfile);
    g_ptr_array_free(args, TRUE);
#else
    execvp(gArgv[0], (char *const *)argv->pdata);
#endif

    g_ptr_array_free(argv, TRUE);
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
        if (path) {
            xemu_settings_set_string(&g_config.sys.files.eeprom_path, path);
        }
    }

    /* Load() reports a clean error for an empty path. */
    m_path = path ? path : "";
    m_saved = false;
    Load();
    m_open_requested = true;
}

bool MainMenuEepromEditor::Load()
{
    m_loaded = false;
    m_saved = false;

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
    /* One extra read to reject files larger than the expected 256 bytes. */
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

    /* The hardware revision is not stored in the EEPROM; detect it by
     * trial: decrypt the security section with each version's keys (the
     * RC4 key is the keyed SHA1 of the stored hash) and accept the version
     * whose decrypted data hashes back to the stored value. */
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

    m_game_rating = ChoiceIndexForValue(
        kGameRatings, ReadLe32(m_eeprom.data() + kGameRatingOffset));
    m_movie_rating = ChoiceIndexForValue(
        kMovieRatings, ReadLe32(m_eeprom.data() + kMovieRatingOffset));

    /* The passcode occupies the low 16 bits, one button nibble per digit,
     * most significant nibble first. */
    uint32_t passcode = ReadLe32(m_eeprom.data() + kPasscodeOffset) & 0xffff;
    for (size_t i = 0; i < G_N_ELEMENTS(m_passcode); i++) {
        uint32_t nibble = (passcode >> ((3 - i) * 4)) & 0xf;
        m_passcode[i] = ChoiceIndexForValue(kPasscodeButtons, nibble);
    }

    FormatIpv4(m_eeprom.data() + kLiveIpOffset, m_live_ip,
               sizeof(m_live_ip));
    FormatIpv4(m_eeprom.data() + kLiveDnsOffset, m_live_dns,
               sizeof(m_live_dns));
    FormatIpv4(m_eeprom.data() + kLiveGatewayOffset, m_live_gateway,
               sizeof(m_live_gateway));
    FormatIpv4(m_eeprom.data() + kLiveSubnetOffset, m_live_subnet,
               sizeof(m_live_subnet));

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

    WriteLe32(m_eeprom.data() + kGameRatingOffset,
              ChoiceValue(kGameRatings, m_game_rating));
    WriteLe32(m_eeprom.data() + kMovieRatingOffset,
              ChoiceValue(kMovieRatings, m_movie_rating));

    /* Only the passcode nibbles in the low 16 bits are edited; keep
     * whatever the upper half of the word contains. */
    uint32_t passcode =
        ReadLe32(m_eeprom.data() + kPasscodeOffset) & ~0xffffu;
    for (size_t i = 0; i < G_N_ELEMENTS(m_passcode); i++) {
        passcode |= ChoiceValue(kPasscodeButtons, m_passcode[i])
                    << ((3 - i) * 4);
    }
    WriteLe32(m_eeprom.data() + kPasscodeOffset, passcode);

    if (!ParseIpv4(m_live_ip, m_eeprom.data() + kLiveIpOffset, "IP address",
                   error) ||
        !ParseIpv4(m_live_dns, m_eeprom.data() + kLiveDnsOffset,
                   "DNS server", error) ||
        !ParseIpv4(m_live_gateway, m_eeprom.data() + kLiveGatewayOffset,
                   "Gateway", error) ||
        !ParseIpv4(m_live_subnet, m_eeprom.data() + kLiveSubnetOffset,
                   "Subnet mask", error)) {
        return false;
    }

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

    /* Checksum ranges the kernel verifies: factory section 0x34-0x5f and
     * user section 0x64-0xbf. Both must be recomputed after any field
     * change, so this has to run after all writes above. */
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

    /* Write to a temporary file, flush and sync it, then rename it over
     * the original so an interrupted save can't corrupt the EEPROM.
     * qemu_fopen and g_rename handle UTF-8 paths on Windows, and g_rename
     * also replaces an existing destination there. */
    std::string temp_path = m_path + ".tmp";
    FILE *fd = qemu_fopen(temp_path.c_str(), "wb");
    if (!fd) {
        error = "Failed to open EEPROM file for writing.";
        return false;
    }

    size_t written = fwrite(m_eeprom.data(), 1, m_eeprom.size(), fd);
    bool flush_ok = fflush(fd) == 0;
#ifdef _WIN32
    bool sync_ok = _commit(_fileno(fd)) == 0;
#else
    bool sync_ok = fsync(fileno(fd)) == 0;
#endif
    bool close_ok = fclose(fd) == 0;
    if (written != m_eeprom.size() || !flush_ok || !sync_ok || !close_ok) {
        g_remove(temp_path.c_str());
        error = "Failed to write EEPROM file.";
        return false;
    }

    if (g_rename(temp_path.c_str(), m_path.c_str()) != 0) {
        g_remove(temp_path.c_str());
        error = "Failed to replace EEPROM file.";
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

    if (!open) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
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

    ImGui::PushStyleColor(ImGuiCol_TabActive, ImVec4(0.14f, 0.45f, 0.10f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive,
                          ImVec4(0.14f, 0.45f, 0.10f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TabHovered,
                          ImVec4(0.20f, 0.55f, 0.12f, 1.0f));

    CenterNextItem(EepromTabBarWidth());
    float tab_line_y = ImGui::GetCursorScreenPos().y +
                       ImGui::GetFrameHeight() - 1.0f;
    if (ImGui::BeginTabBar("##eeprom_tabs")) {
        DrawFullWidthTabLine(tab_line_y);

        if (ImGui::BeginTabItem(kTabLabels[0])) {
            ChoiceComboRow("##hardware_revision", "Hardware revision",
                           &m_hardware_revision, kHardwareRevisions);

            if (DrawHexInputRow("##confounder", "Confounder", m_confounder,
                                sizeof(m_confounder),
                                "Generate##confounder")) {
                GenerateHex(m_confounder, 8);
            }

            if (DrawHexInputRow("##hdd_key", "Hard drive key", m_hdd_key,
                                sizeof(m_hdd_key), "Generate##hdd_key")) {
                GenerateHex(m_hdd_key, 16);
            }

            ChoiceComboRow("##xbox_region", "Xbox region", &m_xbox_region,
                           kXboxRegions);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(kTabLabels[1])) {
            if (DrawTextInputRow("##serial", "Serial", m_serial,
                                 sizeof(m_serial),
                                 ImGuiInputTextFlags_CallbackCharFilter,
                                 "Generate##serial", DigitsOnlyFilter)) {
                GenerateSerial();
            }

            if (DrawHexInputRow("##mac", "MAC address", m_mac, sizeof(m_mac),
                                "Generate##mac")) {
                /* 00:50:F2 is a Microsoft OUI, as used on retail consoles. */
                const uint8_t prefix[3] = { 0x00, 0x50, 0xf2 };
                GenerateHex(m_mac, 6, prefix, sizeof(prefix));
            }

            if (DrawHexInputRow("##online_key", "Online key", m_online_key,
                                sizeof(m_online_key),
                                "Generate##online_key")) {
                GenerateHex(m_online_key, 16);
            }

            ChoiceComboRow("##factory_video_standard", "Video standard",
                           &m_video_standard, kVideoStandards);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(kTabLabels[2])) {
            ChoiceComboRow("##language", "Language", &m_language, kLanguages);
            ChoiceComboRow("##dvd_region", "DVD region", &m_dvd_region,
                           kDvdRegions);
            ChoiceComboRow("##video_output", "Video output", &m_video_standard,
                           kVideoStandards);
            ChoiceComboRow("##video_aspect", "Video aspect", &m_video_aspect,
                           kVideoAspects);

            const VideoOutputSupport video_support =
                VideoOutputSupportForStandard(m_video_standard);
            bool unsupported_video_selected =
                (!video_support.hd_modes &&
                 (m_video_480p || m_video_720p || m_video_1080i)) ||
                (!video_support.hz60 && m_video_60hz) ||
                (!video_support.hz50 && m_video_50hz);

            AlignToFormField();
            ImGui::Text("Supported modes for %s",
                        kVideoStandards[m_video_standard].label);
            AlignToFormField();
            SupportedCheckbox("480p", &m_video_480p,
                              video_support.hd_modes);
            ImGui::SameLine();
            SupportedCheckbox("720p", &m_video_720p,
                              video_support.hd_modes);
            ImGui::SameLine();
            SupportedCheckbox("1080i", &m_video_1080i,
                              video_support.hd_modes);
            AlignToFormField();
            SupportedCheckbox("60 Hz", &m_video_60hz,
                              video_support.hz60);
            ImGui::SameLine();
            SupportedCheckbox("50 Hz", &m_video_50hz,
                              video_support.hz50);
            if (unsupported_video_selected) {
                AlignToFormField();
                ImGui::TextColored(
                    ImVec4(1, 0.35f, 0.35f, 1),
                    "Red video modes are not supported by the selected video standard.");
            }

            if (ChoiceComboRow("##audio_output", "Audio output",
                               &m_audio_output, kAudioOutputs)) {
                if (ChoiceValue(kAudioOutputs, m_audio_output) == 0x00000002) {
                    SetStatus(kSurroundWarning, true);
                } else if (m_status == kSurroundWarning) {
                    SetStatus(nullptr);
                }
            }
            if (ChoiceValue(kAudioOutputs, m_audio_output) == 0x00000002) {
                AlignToFormField();
                ImGui::TextColored(ImVec4(1, 0.35f, 0.35f, 1), "%s",
                                   kSurroundWarning);
            }
            AlignToFormField();
            ImGui::Checkbox("AC3", &m_audio_ac3);
            ImGui::SameLine();
            ImGui::Checkbox("DTS", &m_audio_dts);

            ImGui::Separator();
            AlignToFormField();
            ImGui::Checkbox("6 hour automatic shutdown",
                            &m_misc_auto_off);
            AlignToFormField();
            ImGui::Checkbox("Disable automatic daylight savings adjustment",
                            &m_misc_disable_dst);
            AlignToFormField();
            ImGui::Checkbox("Disable automatic Xbox Live sign-in",
                            &m_misc_disable_live_signin);
            AlignToFormField();
            ImGui::Checkbox("Disable Xbox Live policy on next boot",
                            &m_misc_disable_live_policy);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(kTabLabels[3])) {
            ChoiceComboRow("##game_rating", "Max game rating",
                           &m_game_rating, kGameRatings);
            ChoiceComboRow("##movie_rating", "Max movie rating",
                           &m_movie_rating, kMovieRatings);

            float passcode_row_width =
                ImGui::CalcTextSize("Passcode").x +
                ImGui::GetStyle().ItemSpacing.x * 4.0f +
                ScaledWidth(kPasscodeFieldWidth) * 4.0f;
            CenterNextItem(passcode_row_width);
            ImGui::Text("Passcode");
            ImGui::SameLine();
            ChoiceCombo("##passcode0", &m_passcode[0], kPasscodeButtons,
                        kPasscodeFieldWidth, false);
            ImGui::SameLine();
            ChoiceCombo("##passcode1", &m_passcode[1], kPasscodeButtons,
                        kPasscodeFieldWidth, false);
            ImGui::SameLine();
            ChoiceCombo("##passcode2", &m_passcode[2], kPasscodeButtons,
                        kPasscodeFieldWidth, false);
            ImGui::SameLine();
            ChoiceCombo("##passcode3", &m_passcode[3], kPasscodeButtons,
                        kPasscodeFieldWidth, false);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(kTabLabels[4])) {
            AlignToFormField();
            ImGui::Text("Xbox Live Settings (Deprecated)");
            DrawTextInputRow("##live_ip", "IP address", m_live_ip,
                             sizeof(m_live_ip));
            DrawTextInputRow("##live_dns", "DNS server", m_live_dns,
                             sizeof(m_live_dns));
            DrawTextInputRow("##live_gateway", "Gateway", m_live_gateway,
                             sizeof(m_live_gateway));
            DrawTextInputRow("##live_subnet", "Subnet mask", m_live_subnet,
                             sizeof(m_live_subnet));
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
    ImGui::PopStyleColor(3);

    if (!m_loaded) {
        ImGui::EndDisabled();
    }

    ImGui::Separator();

    int button_count = m_saved ? 4 : 3;
    float button_row_width = ScaledWidth(kButtonWidth) * 3.0f +
                             ImGui::GetStyle().ItemSpacing.x *
                                 (button_count - 1);
    if (m_saved) {
        button_row_width += ScaledWidth(kRestartButtonWidth);
    }
    CenterNextItem(button_row_width);

    if (ImGui::Button("Reload", ImVec2(ScaledWidth(kButtonWidth), 0))) {
        Load();
    }
    ImGui::SameLine();

    if (!m_loaded) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Save", ImVec2(ScaledWidth(kButtonWidth), 0))) {
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
                          ImVec2(ScaledWidth(kRestartButtonWidth), 0))) {
            RestartXemu();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Close", ImVec2(ScaledWidth(kButtonWidth), 0))) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}
