/*
 * xemu User Interface
 *
 * Copyright (C) 2020 Matt Borgerson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <SDL.h>
#include <epoxy/gl.h>
#include <stdio.h>
#include <deque>

#include "xemu-hud.h"
#include "xemu-input.h"
#include "xemu-notifications.h"
#include "xemu-settings.h"
#include "xemu-shaders.h"
#include "xemu-custom-widgets.h"
#include "xemu-monitor.h"
#include "xemu-version.h"
#include "xemu-data.h"
#include "xemu-net.h"
#include "xemu-os-utils.h"
#include "xemu-xbe.h"
#include "xemu-reporting.h"

#include "imgui/imgui.h"
#include "imgui/examples/imgui_impl_sdl.h"
#include "imgui/examples/imgui_impl_opengl3.h"

extern "C" {
#include "noc_file_dialog.h"

// Include necessary QEMU headers
#include "qemu/osdep.h"
#include "qemu-common.h"
#include "sysemu/sysemu.h"
#include "sysemu/runstate.h"

#undef typename
#undef atomic_fetch_add
#undef atomic_fetch_and
#undef atomic_fetch_xor
#undef atomic_fetch_or
#undef atomic_fetch_sub
}

ImFont *g_fixed_width_font;
float g_main_menu_height;
float g_ui_scale = 1.0;
bool g_trigger_style_update = true;

class NotificationManager
{
private:
    const int kNotificationDuration = 4000;
    std::deque<const char *> notification_queue;
    bool active;
    uint32_t notification_end_ts;
    const char *msg;

public:
    NotificationManager()
    {
        active = false;
    }

    ~NotificationManager()
    {

    }

    void QueueNotification(const char *msg)
    {
        notification_queue.push_back(strdup(msg));
    }

    void Draw()
    {
        uint32_t now = SDL_GetTicks();

        if (active) {
            // Currently displaying a notification
            float t = (notification_end_ts - now)/(float)kNotificationDuration;
            if (t > 1.0) {
                // Notification delivered, free it
                free((void*)msg);
                active = false;
            } else {
                // Notification should be displayed
                DrawNotification(t, msg);
            }
        } else {
            // Check to see if a notification is pending
            if (notification_queue.size() > 0) {
                msg = notification_queue[0];
                active = true;
                notification_end_ts = now + kNotificationDuration;
                notification_queue.pop_front();
            }
        }
    }

private:
    void DrawNotification(float t, const char *msg)
    {
        const float DISTANCE = 10.0f;
        static int corner = 1;
        ImGuiIO& io = ImGui::GetIO();
        if (corner != -1)
        {
            ImVec2 window_pos = ImVec2((corner & 1) ? io.DisplaySize.x - DISTANCE : DISTANCE, (corner & 2) ? io.DisplaySize.y - DISTANCE : DISTANCE);
            window_pos.y = g_main_menu_height + DISTANCE;
            ImVec2 window_pos_pivot = ImVec2((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);
            ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        }

        const float fade_in  = 0.1;
        const float fade_out = 0.9;
        float fade = 0;
        
        if (t < fade_in) {
            // Linear fade in
            fade = t/fade_in;
        } else if (t >= fade_out) {
            // Linear fade out
            fade = 1-(t-fade_out)/(1-fade_out);
        } else {
            // Constant
            fade = 1.0;
        }

        ImVec4 color = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
        color.w *= fade;
        ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0,0,0,fade*0.9f));
        ImGui::PushStyleColor(ImGuiCol_Border, color);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::SetNextWindowBgAlpha(0.90f * fade);
        if (ImGui::Begin("Notification", NULL,
            ImGuiWindowFlags_Tooltip |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoInputs
            ))
        {
            ImGui::Text("%s", msg);
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleColor();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::End();
    }  
};

static void HelpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

class MonitorWindow
{
public:
    bool is_open;

private:
    char                  InputBuf[256];
    ImVector<char*>       Items;
    ImVector<const char*> Commands;
    ImVector<char*>       History;
    int                   HistoryPos;    // -1: new line, 0..History.Size-1 browsing history.
    ImGuiTextFilter       Filter;
    bool                  AutoScroll;
    bool                  ScrollToBottom;

public:
    MonitorWindow()
    {
        is_open = false;
        memset(InputBuf, 0, sizeof(InputBuf));
        HistoryPos = -1;
        AutoScroll = true;
        ScrollToBottom = false;
    }
    ~MonitorWindow()
    {
    }

    // Portable helpers
    static int   Stricmp(const char* str1, const char* str2)         { int d; while ((d = toupper(*str2) - toupper(*str1)) == 0 && *str1) { str1++; str2++; } return d; }
    static char* Strdup(const char *str)                             { size_t len = strlen(str) + 1; void* buf = malloc(len); IM_ASSERT(buf); return (char*)memcpy(buf, (const void*)str, len); }
    static void  Strtrim(char* str)                                  { char* str_end = str + strlen(str); while (str_end > str && str_end[-1] == ' ') str_end--; *str_end = 0; }

    void Draw()
    {
        if (!is_open) return;

        ImGui::SetNextWindowSize(ImVec2(520*g_ui_scale, 600*g_ui_scale), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Monitor", &is_open))
        {
            ImGui::End();
            return;
        }

        const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing(); // 1 separator, 1 input text
        ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar); // Leave room for 1 separator + 1 InputText
 
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4,1)); // Tighten spacing
        ImGui::PushFont(g_fixed_width_font);
        ImGui::TextUnformatted(xemu_get_monitor_buffer());
        ImGui::PopFont();

        if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
            ImGui::SetScrollHereY(1.0f);
        ScrollToBottom = false;

        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::Separator();

        // Command-line
        bool reclaim_focus = false;
        ImGui::SetNextItemWidth(-1);
        ImGui::PushFont(g_fixed_width_font);
        if (ImGui::InputText("", InputBuf, IM_ARRAYSIZE(InputBuf), ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_CallbackCompletion|ImGuiInputTextFlags_CallbackHistory, &TextEditCallbackStub, (void*)this))
        {
            char* s = InputBuf;
            Strtrim(s);
            if (s[0])
                ExecCommand(s);
            strcpy(s, "");
            reclaim_focus = true;
        }
        ImGui::PopFont();

        // Auto-focus on window apparition
        ImGui::SetItemDefaultFocus();
        if (reclaim_focus)
            ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget

        ImGui::End();
    }

private:
    void ExecCommand(const char* command_line)
    {
        xemu_run_monitor_command(command_line);

        // Insert into history. First find match and delete it so it can be pushed to the back. This isn't trying to be smart or optimal.
        HistoryPos = -1;
        for (int i = History.Size-1; i >= 0; i--)
            if (Stricmp(History[i], command_line) == 0)
            {
                free(History[i]);
                History.erase(History.begin() + i);
                break;
            }
        History.push_back(Strdup(command_line));

        // On commad input, we scroll to bottom even if AutoScroll==false
        ScrollToBottom = true;
    }

    static int TextEditCallbackStub(ImGuiInputTextCallbackData* data) // In C++11 you are better off using lambdas for this sort of forwarding callbacks
    {
        MonitorWindow* console = (MonitorWindow*)data->UserData;
        return console->TextEditCallback(data);
    }

    int TextEditCallback(ImGuiInputTextCallbackData* data)
    {
        switch (data->EventFlag)
        {
        case ImGuiInputTextFlags_CallbackHistory:
            {
                // Example of HISTORY
                const int prev_history_pos = HistoryPos;
                if (data->EventKey == ImGuiKey_UpArrow)
                {
                    if (HistoryPos == -1)
                        HistoryPos = History.Size - 1;
                    else if (HistoryPos > 0)
                        HistoryPos--;
                }
                else if (data->EventKey == ImGuiKey_DownArrow)
                {
                    if (HistoryPos != -1)
                        if (++HistoryPos >= History.Size)
                            HistoryPos = -1;
                }

                // A better implementation would preserve the data on the current input line along with cursor position.
                if (prev_history_pos != HistoryPos)
                {
                    const char* history_str = (HistoryPos >= 0) ? History[HistoryPos] : "";
                    data->DeleteChars(0, data->BufTextLen);
                    data->InsertChars(0, history_str);
                }
            }
        }
        return 0;
    }
};

class InputWindow
{
public:
    bool is_open;

    InputWindow()
    {
        is_open = false;
    }

    ~InputWindow()
    {
    }

    void Draw()
    {
        if (!is_open) return;

        ImGui::SetNextWindowContentSize(ImVec2(500.0f*g_ui_scale, 0.0f));
        // Remove window X padding for this window to easily center stuff
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,ImGui::GetStyle().WindowPadding.y));
        if (!ImGui::Begin("Input", &is_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::End();
            ImGui::PopStyleVar();
            return;
        }

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
        ImTextureID id = (ImTextureID)(intptr_t)render_to_fbo(controller_fbo);

        //
        // Render buttons with icons of the Xbox style port sockets with
        // circular numbers above them. These buttons can be activated to
        // configure the associated port, like a tabbed interface.
        //
        ImVec4 color_active(0.50, 0.86, 0.54, 0.12);
        ImVec4 color_inactive(0, 0, 0, 0);

        // Begin a 4-column layout to render the ports
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0,12));
        ImGui::Columns(4, "mixed", false);

        const int port_padding = 8;
        for (int i = 0; i < 4; i++) {
            bool is_currently_selected = (i == active);
            bool port_is_bound = (xemu_input_get_bound(i) != NULL);

            // Set an X offset to center the image button within the column
            ImGui::SetCursorPosX(ImGui::GetCursorPosX()+(int)((ImGui::GetColumnWidth()-b_w*g_ui_scale-2*port_padding*g_ui_scale)/2));

            // We are using the same texture for all buttons, but ImageButton
            // uses the texture as a unique ID. Push a new ID now to resolve
            // the conflict.
            ImGui::PushID(i);
            float x = b_x+i*b_x_stride;
            ImGui::PushStyleColor(ImGuiCol_Button, is_currently_selected ? color_active : color_inactive);
            bool activated = ImGui::ImageButton(id,
                ImVec2(b_w*g_ui_scale,b_h*g_ui_scale),
                ImVec2(x/t_w, (b_y+b_h)/t_h),
                ImVec2((x+b_w)/t_w, b_y/t_h),
                port_padding);
            ImGui::PopStyleColor();

            if (activated) {
                active = i;
            }

            uint32_t port_color = 0xafafafff;
            bool is_hovered = ImGui::IsItemHovered();
            if (is_currently_selected || port_is_bound) {
                port_color = 0x81dc8a00;
            } else if (is_hovered) {
                port_color = 0x000000ff;
            }

            render_controller_port(x, b_y, i, port_color);

            ImGui::PopID();
            ImGui::NextColumn();
        }
        ImGui::PopStyleVar(); // ItemSpacing
        ImGui::Columns(1);

        //
        // Render input device combo
        //

        // Center the combo above the controller with the same width
        ImGui::SetCursorPosX(ImGui::GetCursorPosX()+(int)((ImGui::GetColumnWidth()-controller_width*g_ui_scale)/2.0));

        // Note: SetNextItemWidth applies only to the combo element, but not the
        // associated label which follows, so scale back a bit to make space for
        // the label.
        ImGui::SetNextItemWidth(controller_width*0.75*g_ui_scale);

        // List available input devices
        const char *not_connected = "Not Connected";
        struct controller_state *bound_state = xemu_input_get_bound(active);

        // Get current controller name
        const char *name;
        if (bound_state == NULL) {
            name = not_connected;
        } else {
            name = bound_state->name;
        }

        if (ImGui::BeginCombo("Input Devices", name))
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
            struct controller_state *iter;
            for (iter=available_controllers; iter != NULL; iter=iter->next) {
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

        ImGui::Columns(1);

        //
        // Add a separator between input selection and controller graphic
        //
        ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().WindowPadding.y));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().WindowPadding.y));

        //
        // Render controller image
        //
        bool device_selected = false;

        if (bound_state) {
            device_selected = true;
            render_controller(0, 0, 0x81dc8a00, 0x0f0f0f00, bound_state);
        } else {
            static struct controller_state state = { 0 };
            render_controller(0, 0, 0x1f1f1f00, 0x0f0f0f00, &state);
        }

        // update_sdl_controller_state(&state);
        // update_sdl_kbd_controller_state(&state);
        ImVec2 cur = ImGui::GetCursorPos();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX()+(int)((ImGui::GetColumnWidth()-controller_width*g_ui_scale)/2.0));
        ImGui::Image(id,
            ImVec2(controller_width*g_ui_scale, controller_height*g_ui_scale),
            ImVec2(0, controller_height/t_h),
            ImVec2(controller_width/t_w, 0));

        if (!device_selected) {
            // ImGui::SameLine();
            const char *msg = "Please select an available input device";
            ImVec2 dim = ImGui::CalcTextSize(msg);
            ImGui::SetCursorPosX(cur.x + (controller_width*g_ui_scale-dim.x)/2);
            ImGui::SetCursorPosY(cur.y + (controller_height*g_ui_scale-dim.y)/2);
            ImGui::Text("%s", msg);
            ImGui::SameLine();
        }

        ImGui::End();
        ImGui::PopStyleVar(); // Window padding

        // Restore original framebuffer target
        render_to_default_fb();
    }
};


#define MAX_STRING_LEN 2048 // FIXME: Completely arbitrary and only used here
                            // to give a buffer to ImGui for each field

class SettingsWindow
{
public:
    bool is_open;

private:
    bool dirty;
    bool pending_restart;

    char flash_path[MAX_STRING_LEN];
    char bootrom_path[MAX_STRING_LEN];
    char hdd_path[MAX_STRING_LEN];
    char eeprom_path[MAX_STRING_LEN];
    int  memory_idx;
    bool short_animation;

public:
    SettingsWindow()
    {
        is_open = false;
        dirty = false;
        pending_restart = false;
    
        flash_path[0] = '\0';
        bootrom_path[0] = '\0';
        hdd_path[0] = '\0';
        eeprom_path[0] = '\0';
        memory_idx = 0;
        short_animation = false;
    }
    
    ~SettingsWindow()
    {
    }

    void Load()
    {
        const char *tmp;
        int tmp_int;
        size_t len;

        xemu_settings_get_string(XEMU_SETTINGS_SYSTEM_FLASH_PATH, &tmp);
        len = strlen(tmp);
        assert(len < MAX_STRING_LEN);
        strncpy(flash_path, tmp, sizeof(flash_path));

        xemu_settings_get_string(XEMU_SETTINGS_SYSTEM_BOOTROM_PATH, &tmp);
        len = strlen(tmp);
        assert(len < MAX_STRING_LEN);
        strncpy(bootrom_path, tmp, sizeof(bootrom_path));

        xemu_settings_get_string(XEMU_SETTINGS_SYSTEM_HDD_PATH, &tmp);
        len = strlen(tmp);
        assert(len < MAX_STRING_LEN);
        strncpy(hdd_path, tmp, sizeof(hdd_path));

        xemu_settings_get_string(XEMU_SETTINGS_SYSTEM_EEPROM_PATH, &tmp);
        len = strlen(tmp);
        assert(len < MAX_STRING_LEN);
        strncpy(eeprom_path, tmp, sizeof(eeprom_path));
        
        xemu_settings_get_int(XEMU_SETTINGS_SYSTEM_MEMORY, &tmp_int);
        memory_idx = (tmp_int-64)/64;

        xemu_settings_get_bool(XEMU_SETTINGS_SYSTEM_SHORTANIM, &tmp_int);
        short_animation = !!tmp_int;

        dirty = false;
    }

    void Save()
    {
        xemu_settings_set_string(XEMU_SETTINGS_SYSTEM_FLASH_PATH, flash_path);
        xemu_settings_set_string(XEMU_SETTINGS_SYSTEM_BOOTROM_PATH, bootrom_path);
        xemu_settings_set_string(XEMU_SETTINGS_SYSTEM_HDD_PATH, hdd_path);
        xemu_settings_set_string(XEMU_SETTINGS_SYSTEM_EEPROM_PATH, eeprom_path);
        xemu_settings_set_int(XEMU_SETTINGS_SYSTEM_MEMORY, 64+memory_idx*64);
        xemu_settings_set_bool(XEMU_SETTINGS_SYSTEM_SHORTANIM, short_animation);
        xemu_settings_save();
        xemu_queue_notification("Settings saved! Restart to apply updates.");
        pending_restart = true;
    }

    void FilePicker(const char *name, char *buf, size_t len, const char *filters)
    {
        ImGui::PushID(name);
        if (ImGui::InputText("", buf, len)) {
            dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Browse...", ImVec2(100*g_ui_scale, 0))) {
            const char *selected = noc_file_dialog_open(NOC_FILE_DIALOG_OPEN, filters, buf, NULL);
            if ((selected != NULL) && (strcmp(buf, selected) != 0)) {
                strncpy(buf, selected, len-1);
                dirty = true;
            }
        }
        ImGui::PopID();
    }

    void Draw()
    {
        if (!is_open) return;

        ImGui::SetNextWindowContentSize(ImVec2(550.0f*g_ui_scale, 0.0f));
        if (!ImGui::Begin("Settings", &is_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::End();
            return;
        }

        if (ImGui::IsWindowAppearing()) {
            Load();
        }

        const char *rom_file_filters = ".bin Files\0*.bin\0.rom Files\0*.rom\0All Files\0*.*\0";
        const char *qcow_file_filters = ".qcow2 Files\0*.qcow2\0All Files\0*.*\0";

        ImGui::Columns(2, "", false);
        ImGui::SetColumnWidth(0, ImGui::GetWindowWidth()*0.25);

        ImGui::Text("Flash (BIOS) File");
        ImGui::NextColumn();
        float picker_width = ImGui::GetColumnWidth()-120*g_ui_scale;
        ImGui::SetNextItemWidth(picker_width);
        FilePicker("###Flash", flash_path, sizeof(flash_path), rom_file_filters);
        ImGui::NextColumn();

        ImGui::Text("MCPX Boot ROM File");
        ImGui::NextColumn();
        ImGui::SetNextItemWidth(picker_width);
        FilePicker("###BootROM", bootrom_path, sizeof(bootrom_path), rom_file_filters);
        ImGui::NextColumn();

        ImGui::Text("Hard Disk Image File");
        ImGui::NextColumn();
        ImGui::SetNextItemWidth(picker_width);
        FilePicker("###HDD", hdd_path, sizeof(hdd_path), qcow_file_filters);
        ImGui::NextColumn();

        ImGui::Text("EEPROM File");
        ImGui::NextColumn();
        ImGui::SetNextItemWidth(picker_width);
        FilePicker("###EEPROM", eeprom_path, sizeof(eeprom_path), rom_file_filters);
        ImGui::NextColumn();

        ImGui::Text("System Memory");
        ImGui::NextColumn();
        ImGui::SetNextItemWidth(ImGui::GetColumnWidth()*0.5);
        if (ImGui::Combo("###mem", &memory_idx, "64 MiB\0" "128 MiB\0")) {
            dirty = true;
        }
        ImGui::NextColumn();

        ImGui::Dummy(ImVec2(0,0));
        ImGui::NextColumn();
        if (ImGui::Checkbox("Skip startup animation", &short_animation)) {
            dirty = true;
        }
        ImGui::NextColumn();

        ImGui::Columns(1);

        ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().WindowPadding.y));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().WindowPadding.y));

        if (dirty) {
            ImGui::Text("Warning: Unsaved changes!");
            ImGui::SameLine();
        } else if (pending_restart) {
            ImGui::Text("Restart to apply updates");
            ImGui::SameLine();
        }

        ImGui::SetCursorPosX(ImGui::GetWindowWidth()-(120+10)*g_ui_scale);
        ImGui::SetItemDefaultFocus();
        if (ImGui::Button("Save", ImVec2(120*g_ui_scale, 0))) {
            Save();
            dirty = false;
            pending_restart = true;
        }

        ImGui::End();
    }
};

class AboutWindow
{
public:
    bool is_open;

private:
    char build_info_text[256];

public:
    AboutWindow()
    {
        snprintf(build_info_text, sizeof(build_info_text),
            "Verson: %s\n" "Branch: %s\n" "Commit: %s\n" "Date:   %s",
            xemu_version,  xemu_branch,   xemu_commit,   xemu_date);
        // FIXME: Show platform
        // FIXME: Show driver
        // FIXME: Show BIOS/BootROM hash
    }
    
    ~AboutWindow()
    {
    }

    void Draw()
    {
        if (!is_open) return;

        ImGui::SetNextWindowContentSize(ImVec2(400.0f*g_ui_scale, 0.0f));
        if (!ImGui::Begin("About", &is_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::End();
            return;
        }

        static uint32_t time_start = 0;
        if (ImGui::IsWindowAppearing()) {
            time_start = SDL_GetTicks();
        }
        uint32_t now = SDL_GetTicks() - time_start;

        ImGui::SetCursorPosY(ImGui::GetCursorPosY()-50*g_ui_scale);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth()-256*g_ui_scale)/2);

        ImTextureID id = (ImTextureID)(intptr_t)render_to_fbo(logo_fbo);
        float t_w = 256.0;
        float t_h = 256.0;
        float x_off = 0;
        ImGui::Image(id,
            ImVec2((t_w-x_off)*g_ui_scale, t_h*g_ui_scale),
            ImVec2(x_off/t_w, t_h/t_h),
            ImVec2(t_w/t_w, 0));
        if (ImGui::IsItemClicked()) {
            time_start = SDL_GetTicks();
        }
        render_logo(now, 0x42e335ff, 0x42e335ff, 0x00000000);
        render_to_default_fb();
        ImGui::SetCursorPosX(10*g_ui_scale);

        ImGui::SetCursorPosY(ImGui::GetCursorPosY()-100*g_ui_scale);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth()-ImGui::CalcTextSize(xemu_version).x)/2);
        ImGui::Text("%s", xemu_version);

        ImGui::SetCursorPosX(10*g_ui_scale);
        ImGui::Dummy(ImVec2(0,20*g_ui_scale));
        
        const char *msg = "Visit https://xemu.app for more information";
        ImGui::SetCursorPosX((ImGui::GetWindowWidth()-ImGui::CalcTextSize(msg).x)/2);
        ImGui::Text("%s", msg);
        if (ImGui::IsItemClicked()) {
            xemu_open_web_browser("https://xemu.app");
        }

        ImGui::Dummy(ImVec2(0,40*g_ui_scale));

        ImGui::PushFont(g_fixed_width_font);
        ImGui::InputTextMultiline("##build_info", build_info_text, sizeof(build_info_text), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 6), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopFont();

        ImGui::End();
    }
};

class NetworkWindow
{
public:
    bool is_open;
    int  backend;
    char remote_addr[64];
    char local_addr[64];

    NetworkWindow()
    {
        is_open = false;
    }

    ~NetworkWindow()
    {
    }

    void Draw()
    {
        if (!is_open) return;

        ImGui::SetNextWindowContentSize(ImVec2(400.0f*g_ui_scale, 0.0f));
        if (!ImGui::Begin("Network", &is_open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::End();
            return;
        }

        if (ImGui::IsWindowAppearing()) {
            const char *tmp;
            xemu_settings_get_string(XEMU_SETTINGS_NETWORK_REMOTE_ADDR, &tmp);
            strncpy(remote_addr, tmp, sizeof(remote_addr)-1);
            xemu_settings_get_string(XEMU_SETTINGS_NETWORK_LOCAL_ADDR, &tmp);
            strncpy(local_addr, tmp, sizeof(local_addr)-1);
            xemu_settings_get_enum(XEMU_SETTINGS_NETWORK_BACKEND, &backend);
        }

        ImGuiInputTextFlags flg = 0;
        bool is_enabled = xemu_net_is_enabled();
        if (is_enabled) {
            flg |= ImGuiInputTextFlags_ReadOnly;
        }

        ImGui::Columns(2, "", false);
        ImGui::SetColumnWidth(0, ImGui::GetWindowWidth()*0.33);

        ImGui::Text("Attached To");
        ImGui::SameLine(); HelpMarker("The network backend which the emulated NIC interacts with");
        ImGui::NextColumn();
        if (is_enabled) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.6f);
        int temp_backend = backend; // Temporary to make backend combo read-only (FIXME: surely there's a nicer way)
        if (ImGui::Combo("##backend", is_enabled ? &temp_backend : &backend, "User (NAT)\0Socket\0") && !is_enabled) {
            xemu_settings_set_enum(XEMU_SETTINGS_NETWORK_BACKEND, backend);
            xemu_settings_save();
        }
        if (is_enabled) ImGui::PopStyleVar();
        ImGui::SameLine();
        if (backend == XEMU_NET_BACKEND_USER) {
            HelpMarker("User-mode TCP/IP stack with a NAT'd network");
        } else if (backend == XEMU_NET_BACKEND_SOCKET_UDP) {
            HelpMarker("Encapsulates link-layer traffic in UDP packets");
        }
        ImGui::NextColumn();

        if (backend == XEMU_NET_BACKEND_SOCKET_UDP) {
            ImGui::Text("Remote Host");
            ImGui::SameLine(); HelpMarker("The remote <IP address>:<Port> to forward packets to (e.g. 1.2.3.4:9368)");
            ImGui::NextColumn();
            float w = ImGui::GetColumnWidth()-10*g_ui_scale;
            ImGui::SetNextItemWidth(w);
            if (is_enabled) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.6f);
            ImGui::InputText("###remote_host", remote_addr, sizeof(remote_addr), flg);
            if (is_enabled) ImGui::PopStyleVar();
            ImGui::NextColumn();

            ImGui::Text("Local Host");
            ImGui::SameLine(); HelpMarker("The local <IP address>:<Port> to receive packets on (e.g. 0.0.0.0:9368)");
            ImGui::NextColumn();
            ImGui::SetNextItemWidth(w);
            if (is_enabled) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.6f);
            ImGui::InputText("###local_host", local_addr, sizeof(local_addr), flg);
            if (is_enabled) ImGui::PopStyleVar();
            ImGui::NextColumn();
        }

        ImGui::Columns(1);

        ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().WindowPadding.y));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().WindowPadding.y));

        ImGui::Text("Status: %sEnabled", is_enabled ? "" : "Not ");
        ImGui::SameLine();

        ImGui::SetCursorPosX(ImGui::GetWindowWidth()-(120+10)*g_ui_scale);
        ImGui::SetItemDefaultFocus();
        if (ImGui::Button(is_enabled ? "Disable" : "Enable", ImVec2(120*g_ui_scale, 0))) {
            if (!is_enabled) {
                xemu_settings_set_string(XEMU_SETTINGS_NETWORK_REMOTE_ADDR, remote_addr);
                xemu_settings_set_string(XEMU_SETTINGS_NETWORK_LOCAL_ADDR, local_addr);
                xemu_net_enable();
            } else {
                xemu_net_disable();
            }
            xemu_settings_set_bool(XEMU_SETTINGS_NETWORK_ENABLED, xemu_net_is_enabled());
            xemu_settings_save();
        }
        
        ImGui::End();
    }
};

#ifdef CONFIG_CPUID_H
#include <cpuid.h>
#endif

const char *get_cpu_info(void)
{
    const char *cpu_info = "";
#ifdef CONFIG_CPUID_H
    static uint32_t brand[12];
    if (__get_cpuid_max(0x80000004, NULL)) {
        __get_cpuid(0x80000002, brand+0x0, brand+0x1, brand+0x2, brand+0x3);
        __get_cpuid(0x80000003, brand+0x4, brand+0x5, brand+0x6, brand+0x7);
        __get_cpuid(0x80000004, brand+0x8, brand+0x9, brand+0xa, brand+0xb);
    }
    cpu_info = (const char *)brand;
#endif
    // FIXME: Support other architectures (e.g. ARM)
    return cpu_info;
}

class CompatibilityReporter
{
public:
    CompatibilityReport report;
    bool dirty;
    bool is_open;
    bool is_xbe_identified;
    bool did_send, send_result;
    char token_buf[512];
    int playability;
    char description[1024];
    std::string serialized_report;

    CompatibilityReporter()
    {
        is_open = false;

        report.token = "";
        report.xemu_version = xemu_version;
        report.xemu_branch = xemu_branch;
        report.xemu_commit = xemu_commit;
        report.xemu_date = xemu_date;
#if defined(__linux__)
        report.os_platform = "Linux";
#elif defined(_WIN32)
        report.os_platform = "Windows";
#elif defined(__APPLE__)
        report.os_platform = "macOS";
#else
        report.os_platform = "Unknown";
#endif
        report.os_version = xemu_get_os_info();
        report.cpu = get_cpu_info();
        dirty = true;
        is_xbe_identified = false;
        did_send = send_result = false;
    }

    ~CompatibilityReporter()
    {
    }

    void Draw()
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
            "This title is playable from start to finish, with only minor issues.",
            "This title is playable from start to finish with no noticable issues."
        };

        ImGui::SetNextWindowContentSize(ImVec2(550.0f*g_ui_scale, 0.0f));
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

            const char *tmp;
            xemu_settings_get_string(XEMU_SETTINGS_MISC_USER_TOKEN, &tmp);
            assert(strlen(tmp) < sizeof(token_buf));
            strncpy(token_buf, tmp, sizeof(token_buf));
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
        float item_width = ImGui::GetColumnWidth()*0.75-20*g_ui_scale;
        ImGui::SetNextItemWidth(item_width);
        ImGui::PushFont(g_fixed_width_font);
        if (ImGui::InputText("###UserToken", token_buf, sizeof(token_buf), 0)) {
            report.token = token_buf;
            dirty = true;
        }
        ImGui::PopFont();
        ImGui::SameLine();
        if (ImGui::Button("Get Token")) {
            xemu_open_web_browser("https://reports.xemu.app");
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
            ImGui::PushFont(g_fixed_width_font);
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

        ImGui::SetCursorPosX(ImGui::GetWindowWidth()-(120+10)*g_ui_scale);

        ImGui::SetItemDefaultFocus();
        if (ImGui::Button("Send", ImVec2(120*g_ui_scale, 0))) {
            did_send = true;
            send_result = report.Send();
            if (send_result) {
                // Close window on success
                is_open = false;

                // Save user token if it was used
                xemu_settings_set_string(XEMU_SETTINGS_MISC_USER_TOKEN, token_buf);
                xemu_settings_save();
            }
        }
        
        ImGui::End();
    }
};

static MonitorWindow monitor_window;
static InputWindow input_window;
static NetworkWindow network_window;
static AboutWindow about_window;
static SettingsWindow settings_window;
static CompatibilityReporter compatibility_reporter_window;
static NotificationManager notification_manager;
static std::deque<const char *> g_errors;

class FirstBootWindow
{
public:
    bool is_open;

    FirstBootWindow()
    {
        is_open = false;
    }
    
    ~FirstBootWindow()
    {
    }

    void Draw()
    {
        if (!is_open) return;

        ImVec2 size(400*g_ui_scale, 300*g_ui_scale);
        ImGuiIO& io = ImGui::GetIO();

        ImVec2 window_pos = ImVec2((io.DisplaySize.x - size.x)/2, (io.DisplaySize.y - size.y)/2);
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always);

        ImGui::SetNextWindowSize(size, ImGuiCond_Appearing);
        if (!ImGui::Begin("First Boot", &is_open, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration)) {
            ImGui::End();
            return;
        }

        static uint32_t time_start = 0;
        if (ImGui::IsWindowAppearing()) {
            time_start = SDL_GetTicks();
        }
        uint32_t now = SDL_GetTicks() - time_start;

        ImGui::SetCursorPosY(ImGui::GetCursorPosY()-50*g_ui_scale);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth()-256*g_ui_scale)/2);

        ImTextureID id = (ImTextureID)(intptr_t)render_to_fbo(logo_fbo);
        float t_w = 256.0;
        float t_h = 256.0;
        float x_off = 0;
        ImGui::Image(id,
            ImVec2((t_w-x_off)*g_ui_scale, t_h*g_ui_scale),
            ImVec2(x_off/t_w, t_h/t_h),
            ImVec2(t_w/t_w, 0));
        if (ImGui::IsItemClicked()) {
            time_start = SDL_GetTicks();
        }
        render_logo(now, 0x42e335ff, 0x42e335ff, 0x00000000);
        render_to_default_fb();

        ImGui::SetCursorPosY(ImGui::GetCursorPosY()-100*g_ui_scale);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth()-ImGui::CalcTextSize(xemu_version).x)/2);
        ImGui::Text("%s", xemu_version);

        ImGui::SetCursorPosX(10*g_ui_scale);
        ImGui::Dummy(ImVec2(0,20*g_ui_scale));

        const char *msg = "To get started, please configure machine settings.";
        ImGui::SetCursorPosX((ImGui::GetWindowWidth()-ImGui::CalcTextSize(msg).x)/2);
        ImGui::Text("%s", msg);

        ImGui::Dummy(ImVec2(0,20*g_ui_scale));
        ImGui::SetCursorPosX((ImGui::GetWindowWidth()-120*g_ui_scale)/2);
        if (ImGui::Button("Settings", ImVec2(120*g_ui_scale, 0))) {
            settings_window.is_open = true; // FIXME
        }
        ImGui::Dummy(ImVec2(0,20*g_ui_scale));

        msg = "Visit https://xemu.app for more information";
        ImGui::SetCursorPosX((ImGui::GetWindowWidth()-ImGui::CalcTextSize(msg).x)/2);
        ImGui::Text("%s", msg);
        if (ImGui::IsItemClicked()) {
            xemu_open_web_browser("https://xemu.app");
        }
        
        ImGui::End();
    }
};

static bool is_shortcut_key_pressed(int scancode)
{
    ImGuiIO& io = ImGui::GetIO();
    const bool is_osx = io.ConfigMacOSXBehaviors;
    const bool is_shortcut_key = (is_osx ? (io.KeySuper && !io.KeyCtrl) : (io.KeyCtrl && !io.KeySuper)) && !io.KeyAlt && !io.KeyShift; // OS X style: Shortcuts using Cmd/Super instead of Ctrl
    return is_shortcut_key && io.KeysDown[scancode] && (io.KeysDownDuration[scancode] == 0.0);
}

static void action_eject_disc(void)
{
    xemu_settings_set_string(XEMU_SETTINGS_SYSTEM_DVD_PATH, "");
    xemu_settings_save();
    xemu_eject_disc();
}

static void action_load_disc(void)
{
    const char *iso_file_filters = ".iso Files\0*.iso\0All Files\0*.*\0";
    const char *current_disc_path;
    xemu_settings_get_string(XEMU_SETTINGS_SYSTEM_DVD_PATH, &current_disc_path);
    const char *new_disc_path = noc_file_dialog_open(NOC_FILE_DIALOG_OPEN, iso_file_filters, current_disc_path, NULL);
    if (new_disc_path == NULL) {
        /* Cancelled */
        return;
    }
    xemu_settings_set_string(XEMU_SETTINGS_SYSTEM_DVD_PATH, new_disc_path);
    xemu_settings_save();
    xemu_load_disc(new_disc_path);
}

static void action_toggle_pause(void)
{
    if (runstate_is_running()) {
        vm_stop(RUN_STATE_PAUSED);
    } else {
        vm_start();
    }
}

static void action_reset(void)
{
    qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
}

static void action_shutdown(void)
{
    qemu_system_shutdown_request(SHUTDOWN_CAUSE_HOST_UI);
}

static void process_keyboard_shortcuts(void)
{
    if (is_shortcut_key_pressed(SDL_SCANCODE_E)) {
        action_eject_disc();
    }

    if (is_shortcut_key_pressed(SDL_SCANCODE_O)) {
        action_load_disc();
    }

    if (is_shortcut_key_pressed(SDL_SCANCODE_P)) {
        action_toggle_pause();
    }

    if (is_shortcut_key_pressed(SDL_SCANCODE_R)) {
        action_reset();
    }

    if (is_shortcut_key_pressed(SDL_SCANCODE_Q)) {
        action_shutdown();
    }
}

#if defined(__APPLE__)
#define SHORTCUT_MENU_TEXT(c) "Cmd+" #c
#else
#define SHORTCUT_MENU_TEXT(c) "Ctrl+" #c
#endif

static void ShowMainMenu()
{
    bool running = runstate_is_running();

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Machine"))
        {
            if (ImGui::MenuItem("Eject Disc", SHORTCUT_MENU_TEXT(E))) {
                action_eject_disc();
            }
            if (ImGui::MenuItem("Load Disc...", SHORTCUT_MENU_TEXT(O))) {
                action_load_disc();
            }

            ImGui::Separator();

            ImGui::MenuItem("Input",    NULL, &input_window.is_open);
            ImGui::MenuItem("Network",  NULL, &network_window.is_open);
            ImGui::MenuItem("Settings", NULL, &settings_window.is_open);

            ImGui::Separator();

            if (ImGui::MenuItem(running ? "Pause" : "Run", SHORTCUT_MENU_TEXT(P))) {
                action_toggle_pause();
            }
            if (ImGui::MenuItem("Reset", SHORTCUT_MENU_TEXT(R))) {
                action_reset();
            }
            if (ImGui::MenuItem("Shutdown", SHORTCUT_MENU_TEXT(Q))) {
                action_shutdown();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            int ui_scale_combo = g_ui_scale - 1.0;
            if (ui_scale_combo < 0) ui_scale_combo = 0;
            if (ui_scale_combo > 1) ui_scale_combo = 1;
            if (ImGui::Combo("UI Scale", &ui_scale_combo, "1x\0" "2x\0")) {
                g_ui_scale = ui_scale_combo + 1;
                xemu_settings_set_int(XEMU_SETTINGS_DISPLAY_UI_SCALE, g_ui_scale);
                xemu_settings_save();
                g_trigger_style_update = true;
            }

            if (ImGui::Combo("Scaling Mode", &scaling_mode, "Center\0Scale\0Stretch\0")) {
                xemu_settings_set_enum(XEMU_SETTINGS_DISPLAY_SCALE, scaling_mode);
                xemu_settings_save();
            }
            ImGui::SameLine(); HelpMarker("Controls how the rendered content should be scaled into the window");
            if (ImGui::MenuItem("Fullscreen", NULL, xemu_is_fullscreen(), true)) {
                xemu_toggle_fullscreen();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Debug"))
        {
            ImGui::MenuItem("Monitor", NULL, &monitor_window.is_open);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help"))
        {
            ImGui::MenuItem("Report Compatibility", NULL, &compatibility_reporter_window.is_open);
            ImGui::Separator();
            ImGui::MenuItem("About", NULL, &about_window.is_open);
            ImGui::EndMenu();
        }

        g_main_menu_height = ImGui::GetWindowHeight();
        ImGui::EndMainMenuBar();
    }
}

static void InitializeStyle()
{
    ImGuiIO& io = ImGui::GetIO();

    io.Fonts->Clear();
    io.Fonts->AddFontFromFileTTF(xemu_get_resource_path("Roboto-Medium.ttf"), 16*g_ui_scale);
    ImFontConfig font_cfg = ImFontConfig();
    font_cfg.OversampleH = font_cfg.OversampleV = 1;
    font_cfg.PixelSnapH = true;
    font_cfg.SizePixels = 13.0f*g_ui_scale;
    g_fixed_width_font = io.Fonts->AddFontDefault(&font_cfg);
    ImGui_ImplOpenGL3_CreateFontsTexture();

    ImGuiStyle style;
    style.FrameRounding = 8.0;
    style.GrabRounding = 12.0;
    style.PopupRounding = 5.0;
    style.ScrollbarRounding = 12.0;
    style.FramePadding.x = 10;
    style.FramePadding.y = 4;
    style.WindowBorderSize = 0;
    style.PopupBorderSize = 0;
    style.FrameBorderSize = 0;
    style.TabBorderSize = 0;
    ImGui::GetStyle() = style;
    ImGui::GetStyle().ScaleAllSizes(g_ui_scale);

    // Set default theme, override
    ImGui::StyleColorsDark();

    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Text]                   = ImVec4(0.86f, 0.93f, 0.89f, 0.78f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.86f, 0.93f, 0.89f, 0.28f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.06f, 0.06f, 0.06f, 0.98f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.16f, 0.16f, 0.16f, 0.58f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.16f, 0.16f, 0.16f, 0.90f);
    colors[ImGuiCol_Border]                 = ImVec4(0.11f, 0.11f, 0.11f, 0.60f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.16f, 0.16f, 0.16f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.28f, 0.71f, 0.25f, 0.78f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.28f, 0.71f, 0.25f, 1.00f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.20f, 0.51f, 0.18f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.26f, 0.66f, 0.23f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.16f, 0.16f, 0.16f, 0.75f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 0.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.20f, 0.51f, 0.18f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.28f, 0.71f, 0.25f, 0.78f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.28f, 0.71f, 0.25f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.26f, 0.66f, 0.23f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.26f, 0.66f, 0.23f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.28f, 0.71f, 0.25f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.26f, 0.66f, 0.23f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.28f, 0.71f, 0.25f, 0.76f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.28f, 0.71f, 0.25f, 0.86f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.26f, 0.66f, 0.23f, 1.00f);
    colors[ImGuiCol_Separator]              = ImVec4(0.11f, 0.11f, 0.11f, 0.60f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.13f, 0.87f, 0.16f, 0.78f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.25f, 0.75f, 0.10f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.47f, 0.83f, 0.49f, 0.04f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.28f, 0.71f, 0.25f, 0.78f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.28f, 0.71f, 0.25f, 1.00f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.26f, 0.67f, 0.23f, 0.95f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.28f, 0.71f, 0.25f, 0.86f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.26f, 0.66f, 0.23f, 1.00f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.21f, 0.54f, 0.19f, 0.99f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.24f, 0.60f, 0.21f, 1.00f);
    colors[ImGuiCol_PlotLines]              = ImVec4(0.86f, 0.93f, 0.89f, 0.63f);
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(0.28f, 0.71f, 0.25f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.86f, 0.93f, 0.89f, 0.63f);
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(0.28f, 0.71f, 0.25f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.28f, 0.71f, 0.25f, 0.43f);
    colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.16f, 0.16f, 0.16f, 0.73f);
}

/* External interface, called from ui/xemu.c which handles SDL main loop */
static FirstBootWindow first_boot_window;
static SDL_Window *g_sdl_window;

void xemu_hud_init(SDL_Window* window, void* sdl_gl_context)
{
    xemu_monitor_init();

    initialize_custom_ui_rendering();

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = NULL;

    // Setup Platform/Renderer bindings
    ImGui_ImplSDL2_InitForOpenGL(window, sdl_gl_context);
    ImGui_ImplOpenGL3_Init("#version 150");

    first_boot_window.is_open = xemu_settings_did_fail_to_load();

    int ui_scale_int = 1;
    xemu_settings_get_int(XEMU_SETTINGS_DISPLAY_UI_SCALE, &ui_scale_int);
    if (ui_scale_int < 1) ui_scale_int = 1;
    g_ui_scale = ui_scale_int;

    g_sdl_window = window;
}

void xemu_hud_cleanup(void)
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

void xemu_hud_process_sdl_events(SDL_Event *event)
{
    ImGui_ImplSDL2_ProcessEvent(event);
}

void xemu_hud_should_capture_kbd_mouse(int *kbd, int *mouse)
{
    ImGuiIO& io = ImGui::GetIO();
    if (kbd) *kbd = io.WantCaptureKeyboard;
    if (mouse) *mouse = io.WantCaptureMouse; 
}

void xemu_hud_render(void)
{
    uint32_t now = SDL_GetTicks();
    bool ui_wakeup = false;
    struct controller_state *iter;

    // Combine all controller states to allow any controller to navigate
    uint32_t buttons = 0;
    int16_t axis[CONTROLLER_AXIS__COUNT] = {0};
    for (iter=available_controllers; iter != NULL; iter=iter->next) {
        if (iter->type != INPUT_DEVICE_SDL_GAMECONTROLLER) continue;
        buttons |= iter->buttons;
        // We simply take any axis that is >10 % activation
        for (int i = 0; i < CONTROLLER_AXIS__COUNT; i++) {
            if ((iter->axis[i] > 3276) || (iter->axis[i] < -3276)) {
                axis[i] = iter->axis[i];
            }
        }
    }

    // If the guide button is pressed, wake the up
    bool menu_button = false;
    if (buttons & CONTROLLER_BUTTON_GUIDE) {
        ui_wakeup = true;
        menu_button = true;
    }

    // Allow controllers without a guide button to also work
    if ((buttons & CONTROLLER_BUTTON_BACK) &&
        (buttons & CONTROLLER_BUTTON_START)) {
        ui_wakeup = true;
        menu_button = true;
    }

    // If the mouse is moved, wake the ui
    static ImVec2 last_mouse_pos = ImVec2();
    ImVec2 current_mouse_pos = ImGui::GetMousePos();
    if ((current_mouse_pos.x != last_mouse_pos.x) ||
        (current_mouse_pos.y != last_mouse_pos.y)) {
        last_mouse_pos = current_mouse_pos;
        ui_wakeup = true;
    }

    // If mouse capturing is enabled (we are in a dialog), ensure the UI is alive
    bool controller_focus_capture = false;
    ImGuiIO& io = ImGui::GetIO();
    if (io.NavActive) {
        ui_wakeup = true;
        controller_focus_capture = true;
    }

    // Prevent controller events from going to the guest if they are being used
    // to navigate the HUD
    xemu_input_set_test_mode(controller_focus_capture);

    if (g_trigger_style_update) {
        InitializeStyle();
        g_trigger_style_update = false;
    }

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();

    // Override SDL2 implementation gamecontroller interface
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    ImGui_ImplSDL2_NewFrame(g_sdl_window);
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;

    // Update gamepad inputs (from imgui_impl_sdl.cpp)
    memset(io.NavInputs, 0, sizeof(io.NavInputs));
    #define MAP_BUTTON(NAV_NO, BUTTON_NO)       { io.NavInputs[NAV_NO] = (buttons & BUTTON_NO) ? 1.0f : 0.0f; }
    #define MAP_ANALOG(NAV_NO, AXIS_NO, V0, V1) { float vn = (float)(axis[AXIS_NO] - V0) / (float)(V1 - V0); if (vn > 1.0f) vn = 1.0f; if (vn > 0.0f && io.NavInputs[NAV_NO] < vn) io.NavInputs[NAV_NO] = vn; }
    const int thumb_dead_zone = 8000;           // SDL_gamecontroller.h suggests using this value.
    MAP_BUTTON(ImGuiNavInput_Activate,      CONTROLLER_BUTTON_A);               // Cross / A
    MAP_BUTTON(ImGuiNavInput_Cancel,        CONTROLLER_BUTTON_B);               // Circle / B
    MAP_BUTTON(ImGuiNavInput_Menu,          CONTROLLER_BUTTON_X);               // Square / X
    MAP_BUTTON(ImGuiNavInput_Input,         CONTROLLER_BUTTON_Y);               // Triangle / Y
    MAP_BUTTON(ImGuiNavInput_DpadLeft,      CONTROLLER_BUTTON_DPAD_LEFT);       // D-Pad Left
    MAP_BUTTON(ImGuiNavInput_DpadRight,     CONTROLLER_BUTTON_DPAD_RIGHT);      // D-Pad Right
    MAP_BUTTON(ImGuiNavInput_DpadUp,        CONTROLLER_BUTTON_DPAD_UP);         // D-Pad Up
    MAP_BUTTON(ImGuiNavInput_DpadDown,      CONTROLLER_BUTTON_DPAD_DOWN);       // D-Pad Down
    MAP_BUTTON(ImGuiNavInput_FocusPrev,     CONTROLLER_BUTTON_WHITE);           // L1 / LB
    MAP_BUTTON(ImGuiNavInput_FocusNext,     CONTROLLER_BUTTON_BLACK);           // R1 / RB
    MAP_BUTTON(ImGuiNavInput_TweakSlow,     CONTROLLER_BUTTON_WHITE);           // L1 / LB
    MAP_BUTTON(ImGuiNavInput_TweakFast,     CONTROLLER_BUTTON_BLACK);           // R1 / RB

    // Allow Guide and "Back+Start" buttons to also act as Menu buttons
    if (menu_button) {
        io.NavInputs[ImGuiNavInput_Menu] = 1.0;
    }

    MAP_ANALOG(ImGuiNavInput_LStickLeft,    CONTROLLER_AXIS_LSTICK_X, -thumb_dead_zone, -32768);
    MAP_ANALOG(ImGuiNavInput_LStickRight,   CONTROLLER_AXIS_LSTICK_X, +thumb_dead_zone, +32767);
    MAP_ANALOG(ImGuiNavInput_LStickUp,      CONTROLLER_AXIS_LSTICK_Y, +thumb_dead_zone, +32767);
    MAP_ANALOG(ImGuiNavInput_LStickDown,    CONTROLLER_AXIS_LSTICK_Y, -thumb_dead_zone, -32767);

    ImGui::NewFrame();
    process_keyboard_shortcuts();

    bool show_main_menu = true;

    if (first_boot_window.is_open) {
        show_main_menu = false;
    }

    if (show_main_menu) {
        // Auto-hide main menu after 5s of inactivity
        static uint32_t last_check = 0;
        float alpha = 1.0;
        const uint32_t timeout = 5000;
        const float fade_duration = 1000.0;
        if (ui_wakeup) {
            last_check = now;
        }
        if ((now-last_check) > timeout) {
            float t = fmin((float)((now-last_check)-timeout)/fade_duration, 1.0);
            alpha = 1.0-t;
            if (t >= 1.0) {
                alpha = 0.0;
            }
        }
        if (alpha > 0.0) {
            ImVec4 tc = ImGui::GetStyle().Colors[ImGuiCol_Text];
            tc.w = alpha;
            ImGui::PushStyleColor(ImGuiCol_Text, tc);
            ImGui::SetNextWindowBgAlpha(alpha);
            ShowMainMenu();
            ImGui::PopStyleColor();
        } else {
            g_main_menu_height = 0;
        }
    }

    first_boot_window.Draw();
    input_window.Draw();
    settings_window.Draw();
    monitor_window.Draw();
    about_window.Draw();
    network_window.Draw();
    compatibility_reporter_window.Draw();
    notification_manager.Draw();

    // Very rudimentary error notification API
    if (g_errors.size() > 0) {
        ImGui::OpenPopup("Error");
    }
    if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("%s", g_errors[0]);
        ImGui::Dummy(ImVec2(0,16));
        ImGui::SetItemDefaultFocus();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth()-(120+10));
        if (ImGui::Button("Ok", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
            free((void*)g_errors[0]);
            g_errors.pop_front();
        }
        ImGui::EndPopup();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

/* External interface, exposed via xemu-notifications.h */

void xemu_queue_notification(const char *msg)
{
    notification_manager.QueueNotification(msg);
}

void xemu_queue_error_message(const char *msg)
{
    g_errors.push_back(strdup(msg));
}
