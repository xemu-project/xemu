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
#include <string>
#include "common.hh"
#include "compat.hh"
#include "widgets.hh"
#include "viewport-manager.hh"
#include "font-manager.hh"
#include "xemu-version.h"
#include "reporting.hh"
#include "../xemu-settings.h"
#include "../xemu-os-utils.h"

CompatibilityReporter::CompatibilityReporter()
{
    is_open = false;

    report.token = "";
    report.xemu_version = xemu_version;
    report.xemu_commit = xemu_commit;
    report.xemu_date = xemu_date;
    report.os_platform = SDL_GetPlatform();
    report.os_version = xemu_get_os_info();
    report.cpu = xemu_get_cpu_info();
    dirty = true;
    is_xbe_identified = false;
    did_send = send_result = false;
}

CompatibilityReporter::~CompatibilityReporter()
{
}

void CompatibilityReporter::Draw()
{
    if (!is_open) return;

    const char *playability_names[] = {
        "Broken",
        "Intro",
        "Starts",
        "Playable",
        "Perfect",
    };

    const char *playability_descriptions[] = {
        "This title crashes very soon after launching, or displays nothing at all.",
        "This title displays an intro sequence, but fails to make it to gameplay.",
        "This title starts, but may crash or have significant issues.",
        "This title is playable, but may have minor issues.",
        "This title is playable from start to finish with no noticable issues."
    };

    ImGui::SetNextWindowContentSize(ImVec2(550.0f*g_viewport_mgr.m_scale, 0.0f));
    if (!ImGui::Begin("Report Compatibility", &is_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    if (ImGui::IsWindowAppearing()) {
        report.gl_vendor = (const char *)glGetString(GL_VENDOR);
        report.gl_renderer = (const char *)glGetString(GL_RENDERER);
        report.gl_version = (const char *)glGetString(GL_VERSION);
        report.gl_shading_language_version = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
        struct xbe *xbe = xemu_get_xbe_info();
        is_xbe_identified = xbe != NULL;
        if (is_xbe_identified) {
            report.SetXbeData(xbe);
        }
        did_send = send_result = false;

        playability = 3; // Playable
        report.compat_rating = playability_names[playability];
        description[0] = '\x00';
        report.compat_comments = description;

        strncpy(token_buf, g_config.general.user_token, sizeof(token_buf)-1);
        report.token = token_buf;

        dirty = true;
    }

    if (!is_xbe_identified) {
        ImGui::TextWrapped(
            "An XBE could not be identified. Please launch an official "
            "Xbox title to submit a compatibility report.");
        ImGui::End();
        return;
    }

    ImGui::TextWrapped(
        "If you would like to help improve xemu by submitting a compatibility report for this "
        "title, please select an appropriate playability level, enter a "
        "brief description, then click 'Send'."
        "\n\n"
        "Note: By submitting a report, you acknowledge and consent to "
        "collection, archival, and publication of information as outlined "
        "in 'Privacy Disclosure' below.");

    ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().WindowPadding.y));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().WindowPadding.y));

    ImGui::Columns(2, "", false);
    ImGui::SetColumnWidth(0, ImGui::GetWindowWidth()*0.25);

    ImGui::Text("User Token");
    ImGui::SameLine();
    HelpMarker("This is a unique access token used to authorize submission of the report. To request a token, click 'Get Token'.");
    ImGui::NextColumn();
    float item_width = ImGui::GetColumnWidth()*0.75-20*g_viewport_mgr.m_scale;
    ImGui::SetNextItemWidth(item_width);
    ImGui::PushFont(g_font_mgr.m_fixed_width_font);
    if (ImGui::InputText("###UserToken", token_buf, sizeof(token_buf), 0)) {
        xemu_settings_set_string(&g_config.general.user_token, token_buf);
        report.token = token_buf;
        dirty = true;
    }
    ImGui::PopFont();
    ImGui::SameLine();
    if (ImGui::Button("Get Token")) {
        SDL_OpenURL("https://reports.xemu.app");
    }
    ImGui::NextColumn();

    ImGui::Text("Playability");
    ImGui::NextColumn();
    ImGui::SetNextItemWidth(item_width);
    if (ImGui::Combo("###PlayabilityRating", &playability,
        "Broken\0" "Intro/Menus\0" "Starts\0" "Playable\0" "Perfect\0")) {
        report.compat_rating = playability_names[playability];
        dirty = true;
    }
    ImGui::SameLine();
    HelpMarker(playability_descriptions[playability]);
    ImGui::NextColumn();

    ImGui::Columns(1);

    ImGui::Text("Description");
    if (ImGui::InputTextMultiline("###desc", description, sizeof(description), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 6), 0)) {
        report.compat_comments = description;
        dirty = true;
    }

    if (ImGui::TreeNode("Report Details")) {
        ImGui::PushFont(g_font_mgr.m_fixed_width_font);
        if (dirty) {
            serialized_report = report.GetSerializedReport();
            dirty = false;
        }
        ImGui::InputTextMultiline("##build_info", (char*)serialized_report.c_str(), strlen(serialized_report.c_str())+1, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 7), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopFont();
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Privacy Disclosure (Please read before submission!)")) {
        ImGui::TextWrapped(
            "By volunteering to submit a compatibility report, basic information about your "
            "computer is collected, including: your operating system version, CPU model, "
            "graphics card/driver information, and details about the title which are "
            "extracted from the executable in memory. The contents of this report can be "
            "seen before submission by expanding 'Report Details'."
            "\n\n"
            "Like many websites, upon submission, the public IP address of your computer is "
            "also recorded with your report. If provided, the identity associated with your "
            "token is also recorded."
            "\n\n"
            "This information will be archived and used to analyze, resolve problems with, "
            "and improve the application. This information may be made publicly visible, "
            "for example: to anyone who wishes to see the playability status of a title, as "
            "indicated by your report.");
        ImGui::TreePop();
    }

    ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().WindowPadding.y));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().WindowPadding.y));

    if (did_send) {
        if (send_result) {
            ImGui::Text("Sent! Thanks.");
        } else {
            ImGui::Text("Error: %s (%d)", report.GetResultMessage().c_str(), report.GetResultCode());
        }
        ImGui::SameLine();
    }

    ImGui::SetCursorPosX(ImGui::GetWindowWidth()-(120+10)*g_viewport_mgr.m_scale);

    ImGui::SetItemDefaultFocus();
    if (ImGui::Button("Send", ImVec2(120*g_viewport_mgr.m_scale, 0))) {
        did_send = true;
        send_result = report.Send();
        if (send_result) {
            is_open = false;
        }
    }

    ImGui::End();
}

CompatibilityReporter compatibility_reporter_window;
