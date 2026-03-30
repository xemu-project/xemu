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
#include "ui/xui/viewport-manager.hh"
#include "common.hh"
#include "imgui.h"
#include "viewport-manager.hh"
#include "welcome.hh"
#include "widgets.hh"
#include "misc.hh"
#include "gl-helpers.hh"
#include "xemu-version.h"
#include "main-menu.hh"
#include "font-manager.hh"
#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <string>

namespace {

constexpr uintmax_t kMcpxBootRomSize = 512;
constexpr uintmax_t kFlashRomSmallSize = 256 * 1024;
constexpr uintmax_t kFlashRomLargeSize = 1024 * 1024;

static bool IsConfiguredFileReady(const char *path)
{
    return path && path[0] && g_file_test(path, G_FILE_TEST_EXISTS);
}

static std::string ToLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return value;
}

static std::string GetFirstBootSuggestedFolder()
{
    const std::array<const char *, 4> paths = {
        g_config.sys.files.hdd_path,
        g_config.sys.files.flashrom_path,
        g_config.sys.files.bootrom_path,
        g_config.general.games_dir,
    };

    for (const char *path_str : paths) {
        if (!path_str || !path_str[0]) {
            continue;
        }

        std::error_code ec;
        std::filesystem::path path(path_str);
        if (std::filesystem::is_directory(path, ec)) {
            return (path / "").string();
        }

        auto parent = path.parent_path();
        if (!parent.empty() && std::filesystem::exists(parent, ec)) {
            return (parent / "").string();
        }
    }

    return {};
}

static void DrawWizardPanel(const char *id, const char *title,
                            const char *body, ImVec4 bg, ImVec4 border,
                            const char *icon = nullptr)
{
    float scale = g_viewport_mgr.m_scale;
    float padding = 14.0f * scale;
    float content_width = ImGui::GetContentRegionAvail().x - padding * 2;

    ImGui::PushFont(g_font_mgr.m_menu_font_medium);
    float title_height = ImGui::GetTextLineHeight();
    ImGui::PopFont();

    ImGui::PushFont(g_font_mgr.m_menu_font_small);
    float body_height = ImGui::CalcTextSize(body, nullptr, false, content_width).y;
    ImGui::PopFont();
    float height = padding * 2 + title_height + body_height + 10.0f * scale;

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f * scale);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
    ImGui::PushStyleColor(ImGuiCol_Border, border);
    if (ImGui::BeginChild(id, ImVec2(0, height), true,
                          ImGuiWindowFlags_AlwaysUseWindowPadding)) {
        ImGui::PushFont(g_font_mgr.m_menu_font_medium);
        if (icon) {
            ImGui::Text("%s  %s", icon, title);
        } else {
            ImGui::Text("%s", title);
        }
        ImGui::PopFont();

        ImGui::PushFont(g_font_mgr.m_menu_font_small);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.94f, 0.94f, 0.78f));
        ImGui::TextWrapped("%s", body);
        ImGui::PopStyleColor();
        ImGui::PopFont();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

}

FirstBootWindow::FirstBootWindow()
{
    is_open = false;
    m_step = 0;
    m_did_open_file_picker = false;
    m_files_step_success = false;
    m_settings_step_success = false;
}

bool FirstBootWindow::AreRequiredFilesConfigured()
{
    bool bootrom = IsConfiguredFileReady(g_config.sys.files.bootrom_path);
    bool flashrom = IsConfiguredFileReady(g_config.sys.files.flashrom_path);
    bool hdd = IsConfiguredFileReady(g_config.sys.files.hdd_path);
    return bootrom && flashrom && hdd;
}

int FirstBootWindow::GetConfiguredRequiredFileCount()
{
    int count = 0;
    count += IsConfiguredFileReady(g_config.sys.files.bootrom_path) ? 1 : 0;
    count += IsConfiguredFileReady(g_config.sys.files.flashrom_path) ? 1 : 0;
    count += IsConfiguredFileReady(g_config.sys.files.hdd_path) ? 1 : 0;
    return count;
}

int FirstBootWindow::AutoDetectRequiredFilesFromFolder(const char *folder_path)
{
    if (!folder_path || !folder_path[0]) {
        return 0;
    }

    std::filesystem::path root(folder_path);
    std::error_code ec;
    if (!std::filesystem::is_directory(root, ec)) {
        return 0;
    }

    std::filesystem::path bootrom_path;
    std::filesystem::path flashrom_path;
    std::filesystem::path hdd_path;

    for (const auto &entry : std::filesystem::directory_iterator(
             root, std::filesystem::directory_options::skip_permission_denied,
             ec)) {
        if (ec) {
            break;
        }

        if (!entry.is_regular_file(ec) || ec) {
            continue;
        }

        auto extension = ToLowerAscii(entry.path().extension().string());
        auto file_size = entry.file_size(ec);
        if (ec) {
            continue;
        }

        if (bootrom_path.empty() &&
            (extension == ".bin" || extension == ".rom") &&
            file_size == kMcpxBootRomSize) {
            bootrom_path = entry.path();
            continue;
        }

        if (flashrom_path.empty() &&
            (extension == ".bin" || extension == ".rom" || extension == ".img") &&
            (file_size == kFlashRomSmallSize ||
             file_size == kFlashRomLargeSize)) {
            flashrom_path = entry.path();
            continue;
        }

        if (hdd_path.empty() && extension == ".qcow2") {
            hdd_path = entry.path();
        }
    }

    int detected = 0;
    int updated = 0;
    if (!bootrom_path.empty()) {
        detected++;
    }
    if (!flashrom_path.empty()) {
        detected++;
    }
    if (!hdd_path.empty()) {
        detected++;
    }

    if (!bootrom_path.empty() &&
        !IsConfiguredFileReady(g_config.sys.files.bootrom_path)) {
        xemu_settings_set_string(&g_config.sys.files.bootrom_path,
                                 bootrom_path.string().c_str());
        updated++;
    }
    if (!flashrom_path.empty() &&
        !IsConfiguredFileReady(g_config.sys.files.flashrom_path)) {
        xemu_settings_set_string(&g_config.sys.files.flashrom_path,
                                 flashrom_path.string().c_str());
        updated++;
    }
    if (!hdd_path.empty() &&
        !IsConfiguredFileReady(g_config.sys.files.hdd_path)) {
        xemu_settings_set_string(&g_config.sys.files.hdd_path,
                                 hdd_path.string().c_str());
        updated++;
    }

    if (updated > 0) {
        g_main_menu.UpdateAboutViewConfigInfo();
    }

    return detected;
}

void FirstBootWindow::ApplyWindowsRecommendedSettings()
{
#ifdef CONFIG_VULKAN
    g_config.display.renderer = CONFIG_DISPLAY_RENDERER_VULKAN;
#else
    g_config.display.renderer = CONFIG_DISPLAY_RENDERER_OPENGL;
#endif
    g_config.display.window.fullscreen_on_startup = false;
    g_config.display.window.vsync = true;
    g_config.display.ui.auto_scale = true;
    g_config.display.ui.show_notifications = true;
    g_config.display.ui.use_animations = true;
    g_config.perf.cache_shaders = true;
    g_config.input.auto_bind = true;
#if defined(__x86_64__)
    g_config.perf.hard_fpu = true;
#endif
    nv2a_set_surface_scale_factor(2);
}

void FirstBootWindow::DrawStepIndicator(int total_steps, int current_step)
{
    ImDrawList *draw_list = ImGui::GetWindowDrawList();
    float scale = g_viewport_mgr.m_scale;

    float dot_radius = 5.0f * scale;
    float dot_spacing = 24.0f * scale;
    float total_width = (total_steps - 1) * dot_spacing;
    float start_x = (ImGui::GetWindowWidth() - total_width) / 2.0f;

    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    float y = window_pos.y + dot_radius;

    for (int i = 0; i < total_steps; i++) {
        float x = start_x + i * dot_spacing + ImGui::GetWindowPos().x;
        ImU32 color;
        if (i < current_step) {
            color = IM_COL32(66, 170, 60, 255);  // completed - green
        } else if (i == current_step) {
            color = IM_COL32(100, 200, 90, 255);  // current - bright green
        } else {
            color = IM_COL32(80, 80, 80, 255);   // future - dim
        }

        if (i == current_step) {
            draw_list->AddCircleFilled(ImVec2(x, y), dot_radius + 1.0f * scale, color);
        } else {
            draw_list->AddCircleFilled(ImVec2(x, y), dot_radius, color);
        }

        // Draw connecting line between dots
        if (i < total_steps - 1) {
            ImU32 line_color = (i < current_step) ? IM_COL32(66, 170, 60, 180)
                                                   : IM_COL32(60, 60, 60, 180);
            draw_list->AddLine(
                ImVec2(x + dot_radius + 2 * scale, y),
                ImVec2(x + dot_spacing - dot_radius - 2 * scale, y),
                line_color, 2.0f * scale);
        }
    }

    ImGui::Dummy(ImVec2(0, dot_radius * 2 + 8 * scale));
}

void FirstBootWindow::DrawWelcomeStep()
{
    float scale = g_viewport_mgr.m_scale;

    Logo();

    ImGui::PushFont(g_font_mgr.m_menu_font_medium);
    const char *title = "Welcome to OpenMidway";
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(title).x) / 2);
    ImGui::Text("%s", title);
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, 8 * scale));

    ImGui::PushFont(g_font_mgr.m_menu_font_small);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.94f, 0.94f, 0.70f));

    const char *desc = "This wizard will guide you through the initial setup.\n"
                       "You will need three files to get started:";
    ImVec2 desc_size = ImGui::CalcTextSize(desc);
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - desc_size.x) / 2);
    ImGui::TextWrapped("%s", desc);

    ImGui::Dummy(ImVec2(0, 12 * scale));

    DrawWizardPanel(
        "##welcome_windows_panel",
        "Built for Windows 10 and 11 players",
        "Keep your Boot ROM, BIOS, and HDD image in one folder, then let "
        "OpenMidway scan that folder for you. The wizard also includes a "
        "Windows-friendly defaults pass so first boot is smoother on desktops "
        "and laptops.",
        ImVec4(0.10f, 0.16f, 0.11f, 0.96f),
        ImVec4(0.22f, 0.48f, 0.24f, 0.90f),
        ICON_FA_DESKTOP);

    ImGui::Dummy(ImVec2(0, 12 * scale));

    // Show required files list with icons
    float indent = 60 * scale;
    ImGui::SetCursorPosX(indent);
    ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.50f, 1.0f), ICON_FA_MICROCHIP);
    ImGui::SameLine();
    ImGui::Text("MCPX Boot ROM");

    ImGui::SetCursorPosX(indent);
    ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.50f, 1.0f), ICON_FA_MEMORY);
    ImGui::SameLine();
    ImGui::Text("Flash ROM (BIOS)");

    ImGui::SetCursorPosX(indent);
    ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.50f, 1.0f), ICON_FA_HARD_DRIVE);
    ImGui::SameLine();
    ImGui::Text("Hard Disk Image (.qcow2)");

    ImGui::PopStyleColor();
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, 16 * scale));

    // Centered "Get Started" button
    float btn_width = 160 * scale;
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - btn_width) / 2);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.55f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.66f, 0.23f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.75f, 0.28f, 1.0f));
    if (ImGui::Button("Get Started " ICON_FA_ARROW_RIGHT, ImVec2(btn_width, 36 * scale))) {
        m_step = 1;
    }
    ImGui::PopStyleColor(3);

    ImGui::Dummy(ImVec2(0, 12 * scale));

    // Help link
    const char *help_msg = "Need help? Visit the setup guide";
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(help_msg).x) / 2);
    ImGui::PushFont(g_font_mgr.m_menu_font_small);
    Hyperlink(help_msg, "https://github.com/awest813/OpenMidway");
    ImGui::PopFont();
}

void FirstBootWindow::DrawFilesStep()
{
    float scale = g_viewport_mgr.m_scale;
    int configured_files = GetConfiguredRequiredFileCount();

    ImGui::PushFont(g_font_mgr.m_menu_font_medium);
    const char *title = ICON_FA_FOLDER_OPEN "  Select Required Files";
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(title).x) / 2);
    ImGui::Text("%s", title);
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, 4 * scale));

    ImGui::PushFont(g_font_mgr.m_menu_font_small);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.94f, 0.94f, 0.60f));
    const char *subtitle = "Click each row to browse for the file";
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(subtitle).x) / 2);
    ImGui::Text("%s", subtitle);
    ImGui::PopStyleColor();
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, 12 * scale));

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f * scale);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.09f, 0.09f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.20f, 0.20f, 0.90f));
    float files_panel_height = 120.0f * scale;
    if (!m_files_step_message.empty()) {
        files_panel_height += 20.0f * scale;
    }
    if (ImGui::BeginChild("##files_progress_panel",
                          ImVec2(0, files_panel_height), true,
                          ImGuiWindowFlags_AlwaysUseWindowPadding)) {
        ImGui::PushFont(g_font_mgr.m_menu_font_medium);
        if (configured_files == 3) {
            ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f),
                               ICON_FA_CIRCLE_CHECK "  Required files ready");
        } else {
            ImGui::TextColored(ImVec4(0.95f, 0.72f, 0.35f, 1.0f),
                               ICON_FA_FOLDER_TREE "  %d of 3 required files ready",
                               configured_files);
        }
        ImGui::PopFont();

        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.23f, 0.70f, 0.24f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
        ImGui::ProgressBar(configured_files / 3.0f, ImVec2(-FLT_MIN, 8 * scale), "");
        ImGui::PopStyleColor(2);

        ImGui::Dummy(ImVec2(0, 8 * scale));

        ImGui::PushFont(g_font_mgr.m_menu_font_small);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.94f, 0.94f, 0.75f));
        ImGui::TextWrapped(
            "Windows tip: if your Boot ROM, BIOS, and HDD image already live "
            "in one folder, scan that folder first and let the wizard fill in "
            "the matching files automatically.");
        ImGui::PopStyleColor();
        ImGui::PopFont();

        std::string suggested_folder = GetFirstBootSuggestedFolder();
        float button_width = 220 * scale;
        if (ImGui::Button(ICON_FA_WAND_MAGIC_SPARKLES "  Scan folder for required files",
                          ImVec2(button_width, 0))) {
            ShowOpenFolderDialog(
                suggested_folder.empty() ? nullptr : suggested_folder.c_str(),
                [this](const char *folder_path) {
                    int configured_before = GetConfiguredRequiredFileCount();
                    int found = AutoDetectRequiredFilesFromFolder(folder_path);
                    int configured_after = GetConfiguredRequiredFileCount();
                    std::string folder_name = std::filesystem::path(folder_path)
                                                  .filename()
                                                  .string();
                    if (folder_name.empty()) {
                        folder_name = folder_path;
                    }

                    m_files_step_success = found > 0;
                    if (found == 0) {
                        m_files_step_message =
                            "No matching Boot ROM, BIOS, or .qcow2 HDD image "
                            "was found in that folder.";
                    } else if (configured_after > configured_before &&
                               configured_after == 3) {
                        m_files_step_message = string_format(
                            "Found the remaining required files in %s and filled every setup field.",
                            folder_name.c_str());
                    } else if (configured_after > configured_before) {
                        m_files_step_message = string_format(
                            "Found %d matching file%s in %s. Review the remaining field%s below.",
                            found, found == 1 ? "" : "s",
                            folder_name.c_str(),
                            configured_after == 2 ? "" : "s");
                    } else {
                        m_files_step_message =
                            "Matching files were found, but your current selections were already filled so nothing was replaced.";
                    }
                });
        }
        ImGui::SameLine();
        Hyperlink(ICON_FA_BOOK "  Open setup guide",
                  "https://github.com/awest813/OpenMidway/blob/main/docs/getting-started.md");

        if (!m_files_step_message.empty()) {
            ImGui::Dummy(ImVec2(0, 8 * scale));
            ImVec4 text_color = m_files_step_success
                ? ImVec4(0.50f, 0.88f, 0.55f, 1.0f)
                : ImVec4(0.95f, 0.72f, 0.35f, 1.0f);
            ImGui::PushFont(g_font_mgr.m_menu_font_small);
            ImGui::TextColored(text_color, "%s", m_files_step_message.c_str());
            ImGui::PopFont();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    ImGui::Dummy(ImVec2(0, 12 * scale));

    Separator();

    static const SDL_DialogFileFilter rom_file_filters[] = {
        { ".bin Files", "bin" },
        { ".rom Files", "rom" },
        { "All Files", "*" }
    };
    static const SDL_DialogFileFilter qcow_file_filters[] = {
        { ".qcow2 Files", "qcow2" },
        { "All Files", "*" }
    };

    // Helper lambda to draw a file row with status icon
    auto DrawFileRow = [&](const char *label, const char *path,
                           const SDL_DialogFileFilter *filters, int nfilters,
                           const char *hint, char **config_path) {
        bool ready = path && path[0] && g_file_test(path, G_FILE_TEST_EXISTS);

        ImGui::PushFont(g_font_mgr.m_menu_font_small);

        // Status icon
        if (ready) {
            ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f), ICON_FA_CIRCLE_CHECK);
        } else {
            ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.35f, 1.0f), ICON_FA_CIRCLE);
        }
        ImGui::SameLine();

        ImGui::PopFont();

        // File picker widget
        FilePicker(label, path, filters, nfilters, false,
                   [config_path](const char *new_path) {
                       xemu_settings_set_string(config_path, new_path);
                   });

        if (!ready) {
            ImGui::PushFont(g_font_mgr.m_menu_font_small);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 0.5f));
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 28 * scale);
            ImGui::Text("%s", hint);
            ImGui::PopStyleColor();
            ImGui::PopFont();
        }
    };

    DrawFileRow("MCPX Boot ROM",
                g_config.sys.files.bootrom_path,
                rom_file_filters, 3,
                "Required - 512 byte Xbox boot ROM dump",
                &g_config.sys.files.bootrom_path);

    ImGui::Dummy(ImVec2(0, 4 * scale));

    DrawFileRow("Flash ROM (BIOS)",
                g_config.sys.files.flashrom_path,
                rom_file_filters, 3,
                "Required - 256 KB or 1 MB Xbox BIOS image",
                &g_config.sys.files.flashrom_path);

    ImGui::Dummy(ImVec2(0, 4 * scale));

    DrawFileRow("Hard Disk Image",
                g_config.sys.files.hdd_path,
                qcow_file_filters, 2,
                "Required - Xbox HDD image in .qcow2 format",
                &g_config.sys.files.hdd_path);

    ImGui::Dummy(ImVec2(0, 16 * scale));
    Separator();

    // Navigation buttons
    float btn_width = 120 * scale;
    float btn_height = 32 * scale;
    float spacing = 12 * scale;
    float total_btn_width = btn_width * 2 + spacing;
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - total_btn_width) / 2);

    if (ImGui::Button(ICON_FA_ARROW_LEFT " Back", ImVec2(btn_width, btn_height))) {
        m_step = 0;
    }
    ImGui::SameLine(0, spacing);

    bool files_ready = AreRequiredFilesConfigured();
    if (!files_ready) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.30f, 0.30f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.55f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.66f, 0.23f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    }
    if (ImGui::Button("Next " ICON_FA_ARROW_RIGHT, ImVec2(btn_width, btn_height))) {
        if (files_ready) {
            m_step = 2;
        }
    }
    ImGui::PopStyleColor(3);

    if (!files_ready) {
        ImGui::PushFont(g_font_mgr.m_menu_font_small);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.65f, 0.35f, 0.9f));
        const char *warn = "Select all three files above to continue";
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(warn).x) / 2);
        ImGui::Text("%s", warn);
        ImGui::PopStyleColor();
        ImGui::PopFont();
    }
}

void FirstBootWindow::DrawSettingsStep()
{
    float scale = g_viewport_mgr.m_scale;

    ImGui::PushFont(g_font_mgr.m_menu_font_medium);
    const char *title = ICON_FA_SLIDERS "  Quick Settings";
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(title).x) / 2);
    ImGui::Text("%s", title);
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, 4 * scale));

    ImGui::PushFont(g_font_mgr.m_menu_font_small);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.94f, 0.94f, 0.60f));
    const char *subtitle = "Common options - all can be changed later in Settings";
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(subtitle).x) / 2);
    ImGui::Text("%s", subtitle);
    ImGui::PopStyleColor();
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, 12 * scale));

    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f * scale);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.14f, 0.18f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.38f, 0.52f, 0.90f));
    float preset_panel_height = 108.0f * scale;
    if (!m_settings_step_message.empty()) {
        preset_panel_height += 20.0f * scale;
    }
    if (ImGui::BeginChild("##settings_preset_panel",
                          ImVec2(0, preset_panel_height), true,
                          ImGuiWindowFlags_AlwaysUseWindowPadding)) {
        ImGui::PushFont(g_font_mgr.m_menu_font_medium);
        ImGui::Text("%s  Recommended for Windows 10/11", ICON_FA_WAND_MAGIC_SPARKLES);
        ImGui::PopFont();

        ImGui::PushFont(g_font_mgr.m_menu_font_small);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.94f, 0.94f, 0.78f));
        ImGui::TextWrapped(
            "Applies a balanced first-run preset: best available renderer, "
            "2x internal resolution, shader caching, VSync, automatic "
            "controller binding, and adaptive UI scaling.");
        ImGui::PopStyleColor();
        ImGui::PopFont();

        if (ImGui::Button(
                ICON_FA_DESKTOP "  Apply recommended Windows defaults",
                ImVec2(0, 0))) {
            ApplyWindowsRecommendedSettings();
            m_settings_step_success = true;
            m_settings_step_message =
                "Recommended Windows defaults applied. You can still adjust any setting below.";
        }

        if (!m_settings_step_message.empty()) {
            ImGui::Dummy(ImVec2(0, 8 * scale));
            ImGui::PushFont(g_font_mgr.m_menu_font_small);
            ImGui::TextColored(ImVec4(0.56f, 0.84f, 0.96f, 1.0f),
                               "%s", m_settings_step_message.c_str());
            ImGui::PopFont();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    ImGui::Dummy(ImVec2(0, 12 * scale));

    SectionTitle("Display");
    Toggle("Fullscreen on startup",
           &g_config.display.window.fullscreen_on_startup,
           "Launch in fullscreen mode each time you open OpenMidway");
    Toggle("Vertical refresh sync", &g_config.display.window.vsync,
           "Reduce tearing on most Windows displays by syncing to monitor refresh");

    int rendering_scale = nv2a_get_surface_scale_factor() - 1;
    if (ChevronCombo("Resolution scale", &rendering_scale,
                     "1x (Original)\0"
                     "2x (Recommended)\0"
                     "3x\0"
                     "4x\0",
                     "Higher values look sharper but need more GPU power")) {
        nv2a_set_surface_scale_factor(rendering_scale + 1);
    }

    SectionTitle("Performance");
    Toggle("Skip startup animation", &g_config.general.skip_boot_anim,
           "Skip the Xbox boot animation for faster startup");
#if defined(__x86_64__)
    Toggle("Hard FPU emulation", &g_config.perf.hard_fpu,
           "Hardware-accelerated floating point (recommended, requires restart)");
#endif
    Toggle("Cache shaders to disk", &g_config.perf.cache_shaders,
           "Reduce stutter by caching compiled shaders");

    SectionTitle("Input");
    Toggle("Auto-bind controllers", &g_config.input.auto_bind,
           "Automatically assign new controllers to open ports");

    ImGui::Dummy(ImVec2(0, 16 * scale));
    Separator();

    // Navigation buttons
    float btn_width = 120 * scale;
    float btn_height = 32 * scale;
    float spacing = 12 * scale;
    float total_btn_width = btn_width * 2 + spacing;
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - total_btn_width) / 2);

    if (ImGui::Button(ICON_FA_ARROW_LEFT " Back", ImVec2(btn_width, btn_height))) {
        m_step = 1;
    }
    ImGui::SameLine(0, spacing);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.55f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.66f, 0.23f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.75f, 0.28f, 1.0f));
    if (ImGui::Button("Next " ICON_FA_ARROW_RIGHT, ImVec2(btn_width, btn_height))) {
        m_step = 3;
    }
    ImGui::PopStyleColor(3);
}

void FirstBootWindow::DrawReadyStep()
{
    float scale = g_viewport_mgr.m_scale;

    ImGui::Dummy(ImVec2(0, 16 * scale));

    // Big green check
    ImGui::PushFont(g_font_mgr.m_menu_font);
    const char *check_icon = ICON_FA_CIRCLE_CHECK;
    ImVec2 icon_size = ImGui::CalcTextSize(check_icon);
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - icon_size.x * 2) / 2);
    ImGui::SetWindowFontScale(2.0f);
    ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f), "%s", check_icon);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, 8 * scale));

    ImGui::PushFont(g_font_mgr.m_menu_font_medium);
    const char *title = "You're all set!";
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(title).x) / 2);
    ImGui::Text("%s", title);
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, 8 * scale));

    ImGui::PushFont(g_font_mgr.m_menu_font_small);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.94f, 0.94f, 0.70f));

    const char *msg1 = "OpenMidway is configured and ready to use.";
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(msg1).x) / 2);
    ImGui::Text("%s", msg1);

    ImGui::Dummy(ImVec2(0, 4 * scale));

    const char *msg2 = "Load a game disc image to start playing, or press";
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(msg2).x) / 2);
    ImGui::Text("%s", msg2);
    const char *msg3 = "F1 at any time to open the settings menu.";
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(msg3).x) / 2);
    ImGui::Text("%s", msg3);

    ImGui::PopStyleColor();
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, 20 * scale));

    DrawWizardPanel(
        "##ready_panel",
        "What to do next",
        "Load a game disc image and confirm audio, video, and controller "
        "input on one title before changing advanced graphics or networking "
        "settings. Press F1 any time to come back to Settings.",
        ImVec4(0.09f, 0.10f, 0.12f, 0.96f),
        ImVec4(0.22f, 0.30f, 0.36f, 0.90f),
        ICON_FA_ROCKET);

    ImGui::Dummy(ImVec2(0, 12 * scale));

    // Games directory picker
    ImGui::PushFont(g_font_mgr.m_menu_font_small);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.94f, 0.94f, 0.50f));
    const char *optional_label = "Optional: set a default games folder";
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(optional_label).x) / 2);
    ImGui::Text("%s", optional_label);
    ImGui::PopStyleColor();
    ImGui::PopFont();
    FilePicker("Games directory", g_config.general.games_dir, nullptr, 0, true,
               [](const char *path) {
                   xemu_settings_set_string(&g_config.general.games_dir, path);
               });

    ImGui::Dummy(ImVec2(0, 20 * scale));

    // Finish button
    float btn_width = 180 * scale;
    float btn_height = 40 * scale;
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - btn_width) / 2);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.55f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.66f, 0.23f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.75f, 0.28f, 1.0f));
    if (ImGui::Button(ICON_FA_PLAY "  Start Using OpenMidway", ImVec2(btn_width, btn_height))) {
        g_config.general.show_welcome = false;
        xemu_settings_save();
        is_open = false;
    }
    ImGui::PopStyleColor(3);

    ImGui::Dummy(ImVec2(0, 8 * scale));

    // Back button (smaller, below)
    float back_width = 80 * scale;
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - back_width) / 2);
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32_BLACK_TRANS);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    if (ImGui::Button(ICON_FA_ARROW_LEFT " Back", ImVec2(back_width, 0))) {
        m_step = 2;
    }
    ImGui::PopStyleColor(2);
}

void FirstBootWindow::Draw()
{
    if (!is_open) return;

    float scale = g_viewport_mgr.m_scale;
    ImVec2 size(560 * scale, 0);  // auto-height
    ImGuiIO& io = ImGui::GetIO();

    ImVec2 window_pos = ImVec2((io.DisplaySize.x - size.x) / 2,
                               io.DisplaySize.y * 0.08f);
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(size.x, 0), ImVec2(size.x, io.DisplaySize.y * 0.88f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24 * scale, 16 * scale));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.08f, 0.97f));

    if (!ImGui::Begin("First Boot", &is_open,
                       ImGuiWindowFlags_NoMove |
                       ImGuiWindowFlags_NoResize |
                       ImGuiWindowFlags_NoCollapse |
                       ImGuiWindowFlags_NoDecoration |
                       ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        return;
    }

    // Step indicator at top (except on welcome page where we have the logo)
    if (m_step > 0) {
        DrawStepIndicator(4, m_step);
        ImGui::Dummy(ImVec2(0, 4 * scale));
    }

    switch (m_step) {
    case 0: DrawWelcomeStep(); break;
    case 1: DrawFilesStep(); break;
    case 2: DrawSettingsStep(); break;
    case 3: DrawReadyStep(); break;
    default: m_step = 0; break;
    }

    ImGui::Dummy(ImVec2(560 * scale, 8 * scale));

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

FirstBootWindow first_boot_window;
