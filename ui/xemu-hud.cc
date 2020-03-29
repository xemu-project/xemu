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

#include "xemu-hud.h"
#include "xemu-input.h"
#include "xemu-notifications.h"
#include "xemu-settings.h"
#include "xemu-shaders.h"
#include "xemu-custom-widgets.h"
#include "xemu-monitor.h"
#include "xemu-version.h"
#include "xemu-data.h"

#include "imgui/imgui.h"
#include "imgui/examples/imgui_impl_sdl.h"
#include "imgui/examples/imgui_impl_opengl3.h"

extern "C" {
#include "noc_file_dialog.h"

// Include necessary QEMU headers
#include "qemu/osdep.h"
#include "qemu-common.h"
#include "ui/input.h"
#include "qapi/qapi-commands-misc.h"
#include "qapi/qapi-types-run-state.h"
#include "qom/object.h"
#include "sysemu/sysemu.h"
#include "sysemu/runstate.h"
#define typename c_typename
#define typeof decltype
#include "hw/qdev-core.h"
#include "hw/qdev-properties.h"
#include "qapi/error.h"
#include "monitor/qdev.h"
#include "qapi/qmp/qdict.h"
#include "qemu/option.h"
#include "qemu/config-file.h"
#undef typename
#undef atomic_fetch_add
#undef atomic_fetch_and
#undef atomic_fetch_xor
#undef atomic_fetch_or
#undef atomic_fetch_sub
}

uint32_t c = 0x81dc8a21; // FIXME: Use existing theme colors here
#define COL(color, c) (float)(((color)>>((c)*8)) & 0xff)/255.0
ImVec4 color_active = ImVec4(COL(c, 3), COL(c, 2), COL(c, 1), COL(c, 0));
#undef COL

using namespace std;

static bool show_notifications = true;
const int notification_duration = 4000;
static void render_notification(bool* p_open, float t, const char *msg);

ImFont *fixed_width_font;
bool show_main_menu = true;
float main_menu_height;
static void ShowMainMenu();

bool show_first_boot_window = false;
static void ShowFirstBootWindow(bool* p_open);

bool show_monitor_window = false;
static void ShowMonitorConsole(bool* p_open);

bool show_input_window = false;
static void ShowInputWindow(bool* p_open);

bool show_settings_window = false;
static void ShowSettingsWindow(bool* p_open);

bool show_about_window = false;
static void ShowAboutWindow(bool* p_open);

bool show_demo_window = false;

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

    // Load fonts
    io.Fonts->AddFontFromFileTTF(xemu_get_resource_path("Roboto-Medium.ttf"), 16);
    fixed_width_font = io.Fonts->AddFontDefault();

    // Setup Platform/Renderer bindings
    ImGui_ImplSDL2_InitForOpenGL(window, sdl_gl_context);
    const char *glsl_version = "#version 150";
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Set default theme, override
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
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

    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Text]                   = ImVec4(0.86f, 0.93f, 0.89f, 0.78f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.86f, 0.93f, 0.89f, 0.28f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.06f, 0.06f, 0.06f, 250.0/255.0);
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

    show_first_boot_window = xemu_settings_did_fail_to_load();
    if (show_first_boot_window) {
        show_main_menu = false;
    }
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

static void ShowMainMenu()
{
    bool running = runstate_is_running();

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Machine"))
        {
            ImGui::MenuItem("Input", NULL, &show_input_window);
            ImGui::MenuItem("Settings", NULL, &show_settings_window);
            ImGui::Separator();
            if (ImGui::MenuItem(running ? "Pause" : "Run")) {
                if (running) {
                    vm_stop(RUN_STATE_PAUSED);
                } else {
                    vm_start();
                }
            }
            // FIXME: Disabled for now because nv2a crashes during resets. This
            // will be fixed shortly.
            #if 0
            if (ImGui::MenuItem("Restart")) {
                qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
            }
            #endif
            if (ImGui::MenuItem("Shutdown")) {
                qemu_system_shutdown_request(SHUTDOWN_CAUSE_HOST_UI);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            if (ImGui::Combo("Scaling Mode", &scaling_mode, "Center\0Scale\0Stretch\0\0")) {
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
            ImGui::MenuItem("Monitor", NULL, &show_monitor_window);
            ImGui::Separator();
            ImGui::MenuItem("ImGUI Demo", NULL, &show_demo_window);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help"))
        {
            ImGui::MenuItem("About", NULL, &show_about_window);
            ImGui::EndMenu();
        }

        ImGui::SetCursorPosX(ImGui::GetWindowWidth()-100.0);
        extern float fps;
        ImGui::Text("%.3f", fps);
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth()-300.0);
        ImGui::Text("%.3f", 1000.0/fps);

        main_menu_height = ImGui::GetWindowHeight();
        ImGui::EndMainMenuBar();
    }
}

// Tracker for current notification
struct notification_display_state
{
    bool active;
    uint32_t notification_end_ts;
    const char *msg;
} notification;

#include <deque>
std::deque<const char *> notifications;
std::deque<const char *> errors;

void xemu_queue_notification(const char *msg)
{
    notifications.push_back(strdup(msg));
}

void xemu_queue_error_message(const char *msg)
{
    errors.push_back(strdup(msg));
}

static void render_notification(bool* p_open, float t, const char *msg)
{
    const float DISTANCE = 10.0f;
    static int corner = 1;
    ImGuiIO& io = ImGui::GetIO();
    if (corner != -1)
    {
        ImVec2 window_pos = ImVec2((corner & 1) ? io.DisplaySize.x - DISTANCE : DISTANCE, (corner & 2) ? io.DisplaySize.y - DISTANCE : DISTANCE);
        window_pos.y = main_menu_height + DISTANCE;
        ImVec2 window_pos_pivot = ImVec2((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);
        // window_pos_pivot = ImVec2(0.5f, 1.0f);
        // window_pos = ImVec2(io.DisplaySize.x/2, io.DisplaySize.y - DISTANCE);
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
    if (ImGui::Begin("Notification", p_open,
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

void xemu_hud_render(SDL_Window *window)
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

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();

    // Override SDL2 implementation gamecontroller interface
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    ImGui_ImplSDL2_NewFrame(window);
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
            main_menu_height = 0;
        }
    }

    if (show_first_boot_window) ShowFirstBootWindow(&show_first_boot_window);
    if (show_input_window)      ShowInputWindow(&show_input_window);
    if (show_settings_window)   ShowSettingsWindow(&show_settings_window);
    if (show_monitor_window)    ShowMonitorConsole(&show_monitor_window);
    if (show_about_window)      ShowAboutWindow(&show_about_window);
    if (show_demo_window)       ImGui::ShowDemoWindow(&show_demo_window);
    
    if (notification.active) {
        // Currently displaying a notification
        float t = (notification.notification_end_ts - now)/(float)notification_duration;
        if (t > 1.0) {
            // Notification delivered, free it
            free((void*)notification.msg);
            notification.active = 0;
        } else {
            // Notification should be displayed
            render_notification(&show_notifications, t, notification.msg);
        }
    } else {
        // Check to see if a notification is pending
        if (notifications.size() > 0) {
            notification.msg = notifications[0];
            notification.active = 1;
            notification.notification_end_ts = now+notification_duration;
            notifications.pop_front();
        }
    }

    // Very rudimentary error notification API
    if (errors.size() > 0) {
        ImGui::OpenPopup("Error");
    }
    if (ImGui::BeginPopupModal("Error", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("%s", errors[0]);
        ImGui::Dummy(ImVec2(0,16));
        ImGui::SetItemDefaultFocus();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth()-(120+10));
        if (ImGui::Button("Ok", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
            free((void*)errors[0]);
            errors.pop_front();
        }
        ImGui::EndPopup();
    }

    // Rendering
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void xemu_hud_cleanup(void)
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

struct MonitorConsole
{
    char                  InputBuf[256];
    ImVector<char*>       Items;
    ImVector<const char*> Commands;
    ImVector<char*>       History;
    int                   HistoryPos;    // -1: new line, 0..History.Size-1 browsing history.
    ImGuiTextFilter       Filter;
    bool                  AutoScroll;
    bool                  ScrollToBottom;

    MonitorConsole()
    {
        memset(InputBuf, 0, sizeof(InputBuf));
        HistoryPos = -1;
        AutoScroll = true;
        ScrollToBottom = false;
    }
    ~MonitorConsole()
    {
    }

    // Portable helpers
    static int   Stricmp(const char* str1, const char* str2)         { int d; while ((d = toupper(*str2) - toupper(*str1)) == 0 && *str1) { str1++; str2++; } return d; }
    static int   Strnicmp(const char* str1, const char* str2, int n) { int d = 0; while (n > 0 && (d = toupper(*str2) - toupper(*str1)) == 0 && *str1) { str1++; str2++; n--; } return d; }
    static char* Strdup(const char *str)                             { size_t len = strlen(str) + 1; void* buf = malloc(len); IM_ASSERT(buf); return (char*)memcpy(buf, (const void*)str, len); }
    static void  Strtrim(char* str)                                  { char* str_end = str + strlen(str); while (str_end > str && str_end[-1] == ' ') str_end--; *str_end = 0; }

    void Draw(const char* title, bool* p_open)
    {
        ImGui::SetNextWindowSize(ImVec2(520,600), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin(title, p_open))
        {
            ImGui::End();
            return;
        }

        const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing(); // 1 separator, 1 input text
        ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar); // Leave room for 1 separator + 1 InputText
 
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4,1)); // Tighten spacing
        ImGui::PushFont(fixed_width_font);
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
        ImGui::PushFont(fixed_width_font);
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
        MonitorConsole* console = (MonitorConsole*)data->UserData;
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

static void ShowMonitorConsole(bool* p_open)
{
    static MonitorConsole console;
    console.Draw("Monitor", p_open);
}

struct InputWindow
{
    InputWindow()
    {
    }
    ~InputWindow()
    {
    }

    void Draw(const char* title, bool* p_open)
    {
        ImGui::SetNextWindowSize(ImVec2(500,620), ImGuiCond_Appearing);

        // Remove window X padding for this window to easily center stuff
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,ImGui::GetStyle().WindowPadding.y));
        if (!ImGui::Begin(title, p_open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
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
        ImTextureID id = (ImTextureID)render_to_fbo(controller_fbo);

        //
        // Render buttons with icons of the Xbox style port sockets with
        // circular numbers above them. These buttons can be activated to
        // configure the associated port, like a tabbed interface.
        //
        ImVec4 color_inactive = ImVec4(0,0,0,0);

        // Begin a 4-column layout to render the ports
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0,12));
        ImGui::Columns(4, "mixed", false);

        const int port_padding = 8;
        for (int i = 0; i < 4; i++) {
            bool is_currently_selected = (i == active);
            bool port_is_bound = (xemu_input_get_bound(i) != NULL);

            // Set an X offset to center the image button within the column
            ImGui::SetCursorPosX(ImGui::GetCursorPosX()+(int)((ImGui::GetColumnWidth()-b_w-2*port_padding)/2));

            // We are using the same texture for all buttons, but ImageButton
            // uses the texture as a unique ID. Push a new ID now to resolve
            // the conflict.
            ImGui::PushID(i);
            float x = b_x+i*b_x_stride;
            ImGui::PushStyleColor(ImGuiCol_Button, is_currently_selected ? color_active : color_inactive);
            bool activated = ImGui::ImageButton(id,
                ImVec2(b_w,b_h),
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
        ImGui::SetCursorPosX(ImGui::GetCursorPosX()+(int)((ImGui::GetColumnWidth()-controller_width)/2.0));

        // Note: SetNextItemWidth applies only to the combo element, but not the
        // associated label which follows, so scale back a bit to make space for
        // the label.
        ImGui::SetNextItemWidth(controller_width*0.75);

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

        //
        // Add a separator between input selection and controller graphic
        //
        ImGui::Columns(1);
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
        ImGui::SetCursorPosX(ImGui::GetCursorPosX()+(int)((ImGui::GetColumnWidth()-controller_width)/2.0));
        ImGui::Image(id,
            ImVec2(controller_width, controller_height),
            ImVec2(0, controller_height/t_h),
            ImVec2(controller_width/t_w, 0));

        if (!device_selected) {
            // ImGui::SameLine();
            const char *msg = "Please select an available input device";
            ImVec2 dim = ImGui::CalcTextSize(msg);
            ImGui::SetCursorPosX(cur.x + (controller_width-dim.x)/2);
            ImGui::SetCursorPosY(cur.y +(controller_height-dim.y)/2);
            ImGui::Text("%s", msg);
            ImGui::SameLine();
        }

        ImGui::End();
        ImGui::PopStyleVar(); // Window padding

        // Restore original framebuffer target
        render_to_default_fb();
    }
};

static void ShowInputWindow(bool* p_open)
{
    static InputWindow console;
    console.Draw("Input", p_open);
}

#define MAX_STRING_LEN 2048 // FIXME: Completely arbitrary and only used here
                            // to give a buffer to ImGui for each field

struct SettingsWindow
{
    char flash_path[MAX_STRING_LEN];
    char bootrom_path[MAX_STRING_LEN];
    char hdd_path[MAX_STRING_LEN];
    char dvd_path[MAX_STRING_LEN];
    char eeprom_path[MAX_STRING_LEN];
    int  memory_idx;
    bool short_animation;
    bool dirty;
    bool pending_restart;

    SettingsWindow()
    {
        Load(); // Note: This does not catch updates made elsewhere! That isn't
                // a problem yet, but in the future might need to be changed to
                // get most recent data. I put it here so we don't need to sync
                // settings back to these temporary buffers on every frame.
                // Please don't do this. If you need it, consider adding an "on
                // settings updated" callback to sync the updates, if necessary.
        pending_restart = false;
    }
    
    ~SettingsWindow()
    {
    }

    void Load(void)
    {
        const char *tmp;
        int tmp_int;
        size_t len;

        xemu_settings_get_string(XEMU_SETTINGS_SYSTEM_FLASH_PATH, &tmp);
        len = strlen(tmp);
        assert(len < (MAX_STRING_LEN-1));
        strncpy(flash_path, tmp, sizeof(flash_path));

        xemu_settings_get_string(XEMU_SETTINGS_SYSTEM_BOOTROM_PATH, &tmp);
        len = strlen(tmp);
        assert(len < (MAX_STRING_LEN-1));
        strncpy(bootrom_path, tmp, sizeof(bootrom_path));

        xemu_settings_get_string(XEMU_SETTINGS_SYSTEM_HDD_PATH, &tmp);
        len = strlen(tmp);
        assert(len < (MAX_STRING_LEN-1));
        strncpy(hdd_path, tmp, sizeof(hdd_path));

        xemu_settings_get_string(XEMU_SETTINGS_SYSTEM_DVD_PATH, &tmp);
        len = strlen(tmp);
        assert(len < (MAX_STRING_LEN-1));
        strncpy(dvd_path, tmp, sizeof(dvd_path));

        xemu_settings_get_string(XEMU_SETTINGS_SYSTEM_EEPROM_PATH, &tmp);
        len = strlen(tmp);
        assert(len < (MAX_STRING_LEN-1));
        strncpy(eeprom_path, tmp, sizeof(eeprom_path));
        
        xemu_settings_get_int(XEMU_SETTINGS_SYSTEM_MEMORY, &tmp_int);
        memory_idx = (tmp_int-64)/64;

        xemu_settings_get_bool(XEMU_SETTINGS_SYSTEM_SHORTANIM, &tmp_int);
        short_animation = !!tmp_int;

        dirty = false;
    }

    void Save(void)
    {
        xemu_settings_set_string(XEMU_SETTINGS_SYSTEM_FLASH_PATH, flash_path);
        xemu_settings_set_string(XEMU_SETTINGS_SYSTEM_BOOTROM_PATH, bootrom_path);
        xemu_settings_set_string(XEMU_SETTINGS_SYSTEM_HDD_PATH, hdd_path);
        xemu_settings_set_string(XEMU_SETTINGS_SYSTEM_DVD_PATH, dvd_path);
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
        if (ImGui::Button("Browse...", ImVec2(100, 0))) {
            const char *selected = noc_file_dialog_open(NOC_FILE_DIALOG_OPEN, filters, buf, NULL);
            if ((selected != NULL) && (strcmp(buf, selected) != 0)) {
                strncpy(buf, selected, len-1);
                dirty = true;
            }
        }
        ImGui::PopID();
    }

    void Draw(const char* title, bool* p_open)
    {
        ImGui::SetNextWindowSize(ImVec2(550, 300), ImGuiCond_Appearing);
        if (!ImGui::Begin(title, p_open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
        {
            ImGui::End();
            return;
        }

        if (ImGui::IsWindowAppearing()) {
            Load();
        }

        const char *rom_file_filters = ".bin Files\0*.bin\0.rom Files\0*.rom\0All Files\0*.*\0";
        const char *iso_file_filters = ".iso Files\0*.iso\0All Files\0*.*\0";
        const char *qcow_file_filters = ".qcow2 Files\0*.qcow2\0All Files\0*.*\0";

        ImGui::Columns(2, "", false);
        ImGui::SetColumnWidth(0, ImGui::GetWindowWidth()*0.25);

        ImGui::Text("Flash (BIOS) File");
        ImGui::NextColumn();
        float picker_width = ImGui::GetColumnWidth()-120;
        ImGui::SetNextItemWidth(picker_width);
        FilePicker("###Flash", flash_path, MAX_STRING_LEN, rom_file_filters);
        ImGui::NextColumn();

        ImGui::Text("BootROM File");
        ImGui::NextColumn();
        ImGui::SetNextItemWidth(picker_width);
        FilePicker("###BootROM", bootrom_path, MAX_STRING_LEN, rom_file_filters);
        ImGui::NextColumn();

        ImGui::Text("Hard Disk Image File");
        ImGui::NextColumn();
        ImGui::SetNextItemWidth(picker_width);
        FilePicker("###HDD", hdd_path, MAX_STRING_LEN, qcow_file_filters);
        ImGui::NextColumn();

        ImGui::Text("DVD Image File");
        ImGui::NextColumn();
        ImGui::SetNextItemWidth(picker_width);
        FilePicker("###DVD", dvd_path, MAX_STRING_LEN, iso_file_filters);
        ImGui::NextColumn();

        ImGui::Text("EEPROM File");
        ImGui::NextColumn();
        ImGui::SetNextItemWidth(picker_width);
        FilePicker("###EEPROM", eeprom_path, MAX_STRING_LEN, rom_file_filters);
        ImGui::NextColumn();

        ImGui::Text("System Memory");
        ImGui::NextColumn();
        ImGui::SetNextItemWidth(ImGui::GetColumnWidth()*0.5);
        if (ImGui::Combo("###mem", &memory_idx, "64 MiB\0" "128 MiB\0" "\0")) {
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

        ImGui::SetCursorPosY(ImGui::GetWindowHeight()-(10+20));
        if (dirty) {
            ImGui::Text("Warning: Unsaved changes!");
            ImGui::SameLine();
        } else if (pending_restart) {
            ImGui::Text("Restart to apply updates");
            ImGui::SameLine();
        }

        ImGui::SetCursorPosY(ImGui::GetWindowHeight()-(10+25));
        ImGui::SetCursorPosX(ImGui::GetWindowWidth()-(120+10));

        ImGui::SetItemDefaultFocus();
        if (ImGui::Button("Save", ImVec2(120, 0))) {
            Save();
            dirty = false;
            pending_restart = true;
        }

        ImGui::End();
    }
};

static void ShowSettingsWindow(bool* p_open)
{
    static SettingsWindow console;
    console.Draw("Settings", p_open);
}

struct AboutWindow
{
    char build_info_text[256];

    AboutWindow()
    {
        snprintf(build_info_text, sizeof(build_info_text),
            "Verson: %s\n" "Branch: %s\n" "Commit: %s\n" "Date:   %s\n",
            xemu_version,  xemu_branch,   xemu_commit,   xemu_date);
        // FIXME: Show platform
        // FIXME: Show driver
        // FIXME: Show BIOS/BootROM hash
    }
    
    ~AboutWindow()
    {
    }

    void Draw(const char* title, bool* p_open)
    {
        ImGui::SetNextWindowSize(ImVec2(400, 350), ImGuiCond_Appearing);
        if (!ImGui::Begin(title, p_open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
        {
            ImGui::End();
            return;
        }

        static uint32_t time_start = 0;
        if (ImGui::IsWindowAppearing()) {
            time_start = SDL_GetTicks();
        }
        uint32_t now = SDL_GetTicks() - time_start;

        ImGui::SetCursorPosY(ImGui::GetCursorPosY()-50);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth()-256)/2);

        ImTextureID id = (ImTextureID)render_to_fbo(logo_fbo);
        float t_w = 256.0;
        float t_h = 256.0;
        float x_off = 0;
        ImGui::Image(id,
            ImVec2(t_w-x_off, t_h),
            ImVec2(x_off/t_w, t_h/t_h),
            ImVec2(t_w/t_w, 0));
        if (ImGui::IsItemClicked()) {
            time_start = SDL_GetTicks();
        }
        render_logo(now, 0x42e335ff, 0x42e335ff, 0x00000000);
        render_to_default_fb();
        ImGui::SetCursorPosX(10);

        ImGui::SetCursorPosY(ImGui::GetCursorPosY()-100);

        ImGui::SetCursorPosX((ImGui::GetWindowWidth()-ImGui::CalcTextSize(xemu_version).x)/2);
        ImGui::Text("%s", xemu_version);

        ImGui::SetCursorPosX(10);
        ImGui::Dummy(ImVec2(0,35));

        const char *msg = "Visit https://xemu.app for more information";
        ImGui::SetCursorPosX((ImGui::GetWindowWidth()-ImGui::CalcTextSize(msg).x)/2);
        ImGui::Text("%s", msg);

        ImGui::Dummy(ImVec2(0,35));

        ImGui::PushFont(fixed_width_font);
        ImGui::InputTextMultiline("##build_info", build_info_text, IM_ARRAYSIZE(build_info_text), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 6), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopFont();
        if (ImGui::BeginPopupContextItem("##build_info_context", 1))
        {
            if (ImGui::MenuItem("Copy to Clipboard")) {
                SDL_SetClipboardText(build_info_text);
            }
            ImGui::EndPopup();
        }
        
        ImGui::End();
    }
};

static void ShowAboutWindow(bool* p_open)
{
    static AboutWindow console;
    console.Draw("About", p_open);
}

struct FirstBootWindow
{
    FirstBootWindow()
    {
    }
    
    ~FirstBootWindow()
    {
    }

    void Draw(const char* title, bool* p_open)
    {
        ImVec2 size(400, 300);
        ImGuiIO& io = ImGui::GetIO();

        ImVec2 window_pos = ImVec2((io.DisplaySize.x - size.x)/2, (io.DisplaySize.y - size.y)/2);
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always);

        ImGui::SetNextWindowSize(size, ImGuiCond_Appearing);
        if (!ImGui::Begin(title, p_open, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration)) {
            ImGui::End();
            return;
        }

        static uint32_t time_start = 0;
        if (ImGui::IsWindowAppearing()) {
            time_start = SDL_GetTicks();
        }
        uint32_t now = SDL_GetTicks() - time_start;

        ImGui::SetCursorPosY(ImGui::GetCursorPosY()-50);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth()-256)/2);

        ImTextureID id = (ImTextureID)render_to_fbo(logo_fbo);
        float t_w = 256.0;
        float t_h = 256.0;
        float x_off = 0;
        ImGui::Image(id,
            ImVec2(t_w-x_off, t_h),
            ImVec2(x_off/t_w, t_h/t_h),
            ImVec2(t_w/t_w, 0));
        render_logo(now, 0x42e335ff, 0x42e335ff, 0x00000000);
        render_to_default_fb();
        ImGui::SetCursorPosX(10);

        ImGui::SetCursorPosY(ImGui::GetCursorPosY()-75);

        const char *msg = "To get started, please configure machine settings.";
        ImGui::SetCursorPosX((ImGui::GetWindowWidth()-ImGui::CalcTextSize(msg).x)/2);
        ImGui::Text("%s", msg);

        ImGui::Dummy(ImVec2(0,20));
        ImGui::SetCursorPosX((ImGui::GetWindowWidth()-120)/2);
        if (ImGui::Button("Settings", ImVec2(120, 0))) {
            show_settings_window = true;
        }
        ImGui::Dummy(ImVec2(0,20));

        ImGui::SetCursorPosX(10);

        msg = "Visit https://xemu.app for more information";
        ImGui::SetCursorPosY(ImGui::GetWindowHeight()-ImGui::CalcTextSize(msg).y-10);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth()-ImGui::CalcTextSize(msg).x)/2);
        ImGui::Text("%s", msg);
        
        ImGui::End();
    }
};

static void ShowFirstBootWindow(bool* p_open)
{
    static FirstBootWindow console;
    console.Draw("First Boot", p_open);
}
