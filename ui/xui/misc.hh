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
#include <memory>
#include <stdexcept>
#include <cstdio>
#include "common.hh"
#include "xemu-hud.h"

extern "C" {
#include <noc_file_dialog.h>
}

static inline const char *PausedFileOpen(int flags, const char *filters,
                                         const char *default_path,
                                         const char *default_name)
{
    bool is_running = runstate_is_running();
    if (is_running) {
        vm_stop(RUN_STATE_PAUSED);
    }
    const char *r = noc_file_dialog_open(flags, filters, default_path, default_name);
    if (is_running) {
        vm_start();
    }

    return r;
}

template<typename ... Args>
std::string string_format( const std::string& format, Args ... args )
{
    int size_s = std::snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
    if( size_s <= 0 ){ throw std::runtime_error( "Error during formatting." ); }
    auto size = static_cast<size_t>( size_s );
    std::unique_ptr<char[]> buf( new char[ size ] );
    std::snprintf( buf.get(), size, format.c_str(), args ... );
    return std::string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
}

static inline bool IsShortcutKeyPressed(int scancode)
{
    ImGuiIO& io = ImGui::GetIO();
    const bool is_osx = io.ConfigMacOSXBehaviors;
    const bool is_shortcut_key = (is_osx ? (io.KeySuper && !io.KeyCtrl) : (io.KeyCtrl && !io.KeySuper)) && !io.KeyAlt && !io.KeyShift; // OS X style: Shortcuts using Cmd/Super instead of Ctrl
    return is_shortcut_key && ImGui::IsKeyPressed((enum ImGuiKey)scancode);
}

static inline float mix(float a, float b, float t)
{
    return a*(1.0-t) + (b-a)*t;
}

static inline
int PushWindowTransparencySettings(bool transparent, float alpha_transparent = 0.4, float alpha_opaque = 1.0)
{
        float alpha = transparent ? alpha_transparent : alpha_opaque;

        ImVec4 c;

        c = ImGui::GetStyle().Colors[transparent ? ImGuiCol_WindowBg : ImGuiCol_TitleBg];
        c.w *= alpha;
        ImGui::PushStyleColor(ImGuiCol_TitleBg, c);

        c = ImGui::GetStyle().Colors[transparent ? ImGuiCol_WindowBg : ImGuiCol_TitleBgActive];
        c.w *= alpha;
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, c);

        c = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
        c.w *= alpha;
        ImGui::PushStyleColor(ImGuiCol_WindowBg, c);

        c = ImGui::GetStyle().Colors[ImGuiCol_Border];
        c.w *= alpha;
        ImGui::PushStyleColor(ImGuiCol_Border, c);

        c = ImGui::GetStyle().Colors[ImGuiCol_FrameBg];
        c.w *= alpha;
        ImGui::PushStyleColor(ImGuiCol_FrameBg, c);

        return 5;
}

static inline gchar *GetFileMD5Checksum(const char *path)
{
    auto *checksum = g_checksum_new(G_CHECKSUM_MD5);

    auto *file = qemu_fopen(path, "rb");
    if (!file) return nullptr;

    guchar buf[512];
    size_t nread;
    while ((nread = fread(buf, 1, sizeof(buf), file))) {
        g_checksum_update(checksum, buf, nread);
    }

    gchar *checksum_str = g_strdup(g_checksum_get_string(checksum));
    fclose(file);
    g_checksum_free(checksum);
    return checksum_str;
}

