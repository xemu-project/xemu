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
#include "common.hh"
#include "scene-manager.hh"
#include "widgets.hh"
#include "main-menu.hh"
#include "font-manager.hh"
#include "input-manager.hh"
#include "snapshot-manager.hh"
#include "viewport-manager.hh"
#include "xemu-hud.h"
#include "misc.hh"
#include "gl-helpers.hh"
#include "reporting.hh"
#include "qapi/error.h"
#include "actions.hh"

#include "../xemu-input.h"
#include "../xemu-notifications.h"
#include "../xemu-settings.h"
#include "../xemu-monitor.h"
#include "../xemu-version.h"
#include "../xemu-net.h"
#include "../xemu-os-utils.h"
#include "../xemu-xbe.h"

#include "../thirdparty/fatx/fatx.h"

#define DEFAULT_XMU_SIZE 8388608

MainMenuScene g_main_menu;

MainMenuTabView::~MainMenuTabView() {}
void MainMenuTabView::Draw()
{
}

void MainMenuGeneralView::Draw()
{
#if defined(_WIN32)
    SectionTitle("Updates");
    Toggle("Check for updates", &g_config.general.updates.check,
           "Check for updates whenever xemu is opened");
#endif

#if defined(__x86_64__)
    SectionTitle("Performance");
    Toggle("Hard FPU emulation", &g_config.perf.hard_fpu,
           "Use hardware-accelerated floating point emulation (requires restart)");
#endif

    Toggle("Cache shaders to disk", &g_config.perf.cache_shaders,
           "Reduce stutter in games by caching previously generated shaders");

    SectionTitle("Miscellaneous");
    Toggle("Skip startup animation", &g_config.general.skip_boot_anim,
           "Skip the full Xbox boot animation sequence");
    FilePicker("Screenshot output directory", g_config.general.screenshot_dir,
               nullptr, 0, true, [](const char *path) {
                   xemu_settings_set_string(&g_config.general.screenshot_dir, path);
               });
    FilePicker("Games directory", g_config.general.games_dir, nullptr, 0, true,
               [](const char *path) {
                   xemu_settings_set_string(&g_config.general.games_dir, path);
               });
    // toggle("Throttle DVD/HDD speeds", &g_config.general.throttle_io,
    //        "Limit DVD/HDD throughput to approximate Xbox load times");
}

bool MainMenuInputView::ConsumeRebindEvent(SDL_Event *event)
{
    if (!m_rebinding) {
        return false;
    }

    RebindEventResult rebind_result = m_rebinding->ConsumeRebindEvent(event);
    if (rebind_result == RebindEventResult::Complete) {
        m_rebinding = nullptr;
    }

    return rebind_result == RebindEventResult::Ignore;
}

bool MainMenuInputView::IsInputRebinding()
{
    return m_rebinding != nullptr;
}

void MainMenuInputView::Draw()
{
    SectionTitle("Controllers");
    ImGui::PushFont(g_font_mgr.m_menu_font_small);

    static int active = 0;

    // Output dimensions of texture
    float t_w = 512, t_h = 512;
    // Dimensions of (port+label)s
    float b_x = 0, b_x_stride = 100, b_y = 400;
    float b_w = 68, b_h = 81;
    // Dimensions of controller (rendered at origin)
    float controller_width  = 477.0f;
    float controller_height = 395.0f;
    // Dimensions of XMU
    float xmu_x = 0, xmu_x_stride = 256, xmu_y = 0;
    float xmu_w = 256, xmu_h = 256;

    // Setup rendering to fbo for controller and port images
    controller_fbo->Target();
    ImTextureID id = (ImTextureID)(intptr_t)controller_fbo->Texture();

    //
    // Render buttons with icons of the Xbox style port sockets with
    // circular numbers above them. These buttons can be activated to
    // configure the associated port, like a tabbed interface.
    //
    ImVec4 color_active(0.50, 0.86, 0.54, 0.12);
    ImVec4 color_inactive(0, 0, 0, 0);

    // Begin a 4-column layout to render the ports
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        g_viewport_mgr.Scale(ImVec2(0, 12)));
    ImGui::Columns(4, "mixed", false);

    const int port_padding = 8;
    for (int i = 0; i < 4; i++) {
        bool is_selected = (i == active);
        bool port_is_bound = (xemu_input_get_bound(i) != NULL);

        // Set an X offset to center the image button within the column
        ImGui::SetCursorPosX(
            ImGui::GetCursorPosX() +
            (int)((ImGui::GetColumnWidth() - b_w * g_viewport_mgr.m_scale -
                   2 * port_padding * g_viewport_mgr.m_scale) /
                  2));

        // We are using the same texture for all buttons, but ImageButton
        // uses the texture as a unique ID. Push a new ID now to resolve
        // the conflict.
        ImGui::PushID(i);
        float x = b_x + i * b_x_stride;
        ImGui::PushStyleColor(ImGuiCol_Button,
                              is_selected ? color_active : color_inactive);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                            g_viewport_mgr.Scale(ImVec2(port_padding, port_padding)));
        bool activated = ImGui::ImageButton(
            "port_image_button",
            id,
            ImVec2(b_w * g_viewport_mgr.m_scale, b_h * g_viewport_mgr.m_scale),
            ImVec2(x / t_w, (b_y + b_h) / t_h),
            ImVec2((x + b_w) / t_w, b_y / t_h));
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        if (activated) {
            active = i;
            m_rebinding = nullptr;
        }

        uint32_t port_color = 0xafafafff;
        bool is_hovered = ImGui::IsItemHovered();
        if (is_hovered) {
            port_color = 0xffffffff;
        } else if (is_selected || port_is_bound) {
            port_color = 0x81dc8a00;
        }

        RenderControllerPort(x, b_y, i, port_color);

        ImGui::PopID();
        ImGui::NextColumn();
    }
    ImGui::PopStyleVar(); // ItemSpacing
    ImGui::Columns(1);

    //
    // Render device driver combo
    //

    // List available device drivers
    const char *driver = bound_drivers[active];

    if (strcmp(driver, DRIVER_DUKE) == 0)
        driver = DRIVER_DUKE_DISPLAY_NAME;
    else if (strcmp(driver, DRIVER_S) == 0)
        driver = DRIVER_S_DISPLAY_NAME;

    ImGui::Columns(2, "", false);
    ImGui::SetColumnWidth(0, ImGui::GetWindowWidth()*0.25);

    ImGui::Text("Emulated Device");
    ImGui::SameLine(0, 0);
    ImGui::NextColumn();

    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("###InputDrivers", driver,
                          ImGuiComboFlags_NoArrowButton)) {
        const char *available_drivers[] = { DRIVER_DUKE, DRIVER_S };
        const char *driver_display_names[] = { DRIVER_DUKE_DISPLAY_NAME,
                                               DRIVER_S_DISPLAY_NAME };
        bool is_selected = false;
        int num_drivers = sizeof(driver_display_names) / sizeof(driver_display_names[0]);
        for (int i = 0; i < num_drivers; i++) {
            const char *iter = driver_display_names[i];
            is_selected = strcmp(driver, iter) == 0;
            ImGui::PushID(iter);
            if (ImGui::Selectable(iter, is_selected)) {
                for (int j = 0; j < num_drivers; j++) {
                    if (iter == driver_display_names[j])
                        bound_drivers[active] = available_drivers[j];
                }
                xemu_input_bind(active, bound_controllers[active], 1);
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
            ImGui::PopID();
        }

        ImGui::EndCombo();
    }
    DrawComboChevron();

    ImGui::NextColumn();

    //
    // Render input device combo
    //

    ImGui::Text("Input Device");
    ImGui::SameLine(0, 0);
    ImGui::NextColumn();

    // List available input devices
    const char *not_connected = "Not Connected";
    ControllerState *bound_state = xemu_input_get_bound(active);

    // Get current controller name
    const char *name;
    if (bound_state == NULL) {
        name = not_connected;
    } else {
        name = bound_state->name;
    }

    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("###InputDevices", name, ImGuiComboFlags_NoArrowButton))
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
        ControllerState *iter;
        QTAILQ_FOREACH(iter, &available_controllers, entry) {
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

                // FIXME: We want to bind the XMU here, but we can't because we
                // just unbound it and we need to wait for Qemu to release the
                // file

                // If we previously had no controller connected, we can rebind
                // the XMU
                if (bound_state == NULL)
                    xemu_input_rebind_xmu(active);

                bound_state = iter;
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
            ImGui::PopID();
        }

        ImGui::EndCombo();
    }
    DrawComboChevron();

    ImGui::Columns(1);

    //
    // Add a separator between input selection and controller graphic
    //
    ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().WindowPadding.y / 2));

    //
    // Render controller image
    //
    bool device_selected = false;

    if (bound_state) {
        device_selected = true;
        RenderController(0, 0, 0x81dc8a00, 0x0f0f0f00, bound_state);
    } else {
        static ControllerState state{};
        RenderController(0, 0, 0x1f1f1f00, 0x0f0f0f00, &state);
    }

    ImVec2 cur = ImGui::GetCursorPos();

    ImVec2 controller_display_size;
    if (ImGui::GetContentRegionMax().x < controller_width*g_viewport_mgr.m_scale) {
        controller_display_size.x = ImGui::GetContentRegionMax().x;
        controller_display_size.y =
            controller_display_size.x * controller_height / controller_width;
    } else {
        controller_display_size =
            ImVec2(controller_width * g_viewport_mgr.m_scale,
                   controller_height * g_viewport_mgr.m_scale);
    }

    ImGui::SetCursorPosX(
        ImGui::GetCursorPosX() +
        (int)((ImGui::GetColumnWidth() - controller_display_size.x) / 2.0));

    ImGui::Image(id,
        controller_display_size,
        ImVec2(0, controller_height/t_h),
        ImVec2(controller_width/t_w, 0));
    ImVec2 pos = ImGui::GetCursorPos();
    if (!device_selected) {
        const char *msg = "Please select an available input device";
        ImVec2 dim = ImGui::CalcTextSize(msg);
        ImGui::SetCursorPosX(cur.x + (controller_display_size.x-dim.x)/2);
        ImGui::SetCursorPosY(cur.y + (controller_display_size.y-dim.y)/2);
        ImGui::Text("%s", msg);
    }

    controller_fbo->Restore();

    ImGui::PopFont();
    ImGui::SetCursorPos(pos);

    if (bound_state) {
        ImGui::PushID(active);

        SectionTitle("Expansion Slots");
        // Begin a 2-column layout to render the expansion slots
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                            g_viewport_mgr.Scale(ImVec2(0, 12)));
        ImGui::Columns(2, "mixed", false);

        xmu_fbo->Target();
        id = (ImTextureID)(intptr_t)xmu_fbo->Texture();

        static const SDL_DialogFileFilter img_file_filters[] = {
            { ".img Files", "img" },
            { "All Files", "*" }
        };
        const char *comboLabels[2] = { "###ExpansionSlotA",
                                       "###ExpansionSlotB" };
        for (int i = 0; i < 2; i++) {
            // Display a combo box to allow the user to choose the type of
            // peripheral they want to use
            enum peripheral_type selected_type =
                bound_state->peripheral_types[i];
            const char *peripheral_type_names[2] = { "None", "Memory Unit" };
            const char *selected_peripheral_type =
                peripheral_type_names[selected_type];
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo(comboLabels[i], selected_peripheral_type,
                                  ImGuiComboFlags_NoArrowButton)) {
                // Handle all available peripheral types
                for (int j = 0; j < 2; j++) {
                    bool is_selected = selected_type == j;
                    ImGui::PushID(j);
                    const char *selectable_label = peripheral_type_names[j];

                    if (ImGui::Selectable(selectable_label, is_selected)) {
                        // Free any existing peripheral
                        if (bound_state->peripherals[i] != NULL) {
                            if (bound_state->peripheral_types[i] ==
                                PERIPHERAL_XMU) {
                                // Another peripheral was already bound.
                                // Unplugging
                                xemu_input_unbind_xmu(active, i);
                            }

                            // Free the existing state
                            g_free((void *)bound_state->peripherals[i]);
                            bound_state->peripherals[i] = NULL;
                        }

                        // Change the peripheral type to the newly selected type
                        bound_state->peripheral_types[i] =
                            (enum peripheral_type)j;

                        // Allocate state for the new peripheral
                        if (j == PERIPHERAL_XMU) {
                            bound_state->peripherals[i] =
                                g_malloc(sizeof(XmuState));
                            memset(bound_state->peripherals[i], 0,
                                   sizeof(XmuState));
                        }

                        xemu_save_peripheral_settings(
                            active, i, bound_state->peripheral_types[i], NULL);
                    }

                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }

                    ImGui::PopID();
                }

                ImGui::EndCombo();
            }
            DrawComboChevron();

            // Set an X offset to center the image button within the column
            ImGui::SetCursorPosX(
                ImGui::GetCursorPosX() +
                (int)((ImGui::GetColumnWidth() -
                       xmu_w * g_viewport_mgr.m_scale -
                       2 * port_padding * g_viewport_mgr.m_scale) /
                      2));

            selected_type = bound_state->peripheral_types[i];
            if (selected_type == PERIPHERAL_XMU) {
                float x = xmu_x + i * xmu_x_stride;
                float y = xmu_y;

                XmuState *xmu = (XmuState *)bound_state->peripherals[i];
                if (xmu->filename != NULL && strlen(xmu->filename) > 0) {
                    RenderXmu(x, y, 0x81dc8a00, 0x0f0f0f00);

                } else {
                    RenderXmu(x, y, 0x1f1f1f00, 0x0f0f0f00);
                }

                ImVec2 xmu_display_size;
                if (ImGui::GetContentRegionMax().x <
                    xmu_h * g_viewport_mgr.m_scale) {
                    xmu_display_size.x = ImGui::GetContentRegionMax().x / 2;
                    xmu_display_size.y = xmu_display_size.x * xmu_h / xmu_w;
                } else {
                    xmu_display_size = ImVec2(xmu_w * g_viewport_mgr.m_scale,
                                              xmu_h * g_viewport_mgr.m_scale);
                }

                ImGui::SetCursorPosX(
                    ImGui::GetCursorPosX() +
                    (int)((ImGui::GetColumnWidth() - xmu_display_size.x) /
                          2.0));

                ImGui::Image(id, xmu_display_size, ImVec2(0.5f * i, 1),
                             ImVec2(0.5f * (i + 1), 0));

                // Button to generate a new XMU
                ImGui::PushID(i);
                if (ImGui::Button("New Image", ImVec2(250, 0))) {
                    int port = active;
                    int slot = i;
                    ShowSaveFileDialog(img_file_filters, 2, nullptr, [port, slot](const char *new_path) {
                        if (create_fatx_image(new_path, DEFAULT_XMU_SIZE)) {
                            // XMU was created successfully. Bind it
                            xemu_input_bind_xmu(port, slot, new_path, false);
                        } else {
                            // Show alert message
                            char *msg = g_strdup_printf(
                                "Unable to create XMU image at %s", new_path);
                            xemu_queue_error_message(msg);
                            g_free(msg);
                        }
                    });
                }

                int port = active;
                int slot = i;
                FilePicker("Image", xmu->filename, img_file_filters, 2, false,
                           [port, slot](const char *path) {
                               if (strlen(path) > 0) {
                                   xemu_input_bind_xmu(port, slot, path, false);
                               } else {
                                   xemu_input_unbind_xmu(port, slot);
                               }
                           });

                ImGui::PopID();
            }

            ImGui::NextColumn();
        }

        xmu_fbo->Restore();

        ImGui::PopStyleVar(); // ItemSpacing
        ImGui::Columns(1);

        SectionTitle("Mapping");
        ImVec4 tc = ImGui::GetStyle().Colors[ImGuiCol_Header];
        tc.w = 0.0f;
        ImGui::PushStyleColor(ImGuiCol_Header, tc);

        if (ImGui::CollapsingHeader("Input Mapping")) {
            float p = ImGui::GetFrameHeight() * 0.3;
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(p, p));
            if (ImGui::BeginTable("input_remap_tbl", 2,
                                  ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_Borders)) {
                ImGui::TableSetupColumn("Emulated Input");
                ImGui::TableSetupColumn("Host Input");
                ImGui::TableHeadersRow();

                PopulateTableController(bound_state);

                ImGui::EndTable();
            }
            ImGui::PopStyleVar();
        }

        if (bound_state->type == INPUT_DEVICE_SDL_GAMEPAD) {
            Toggle("Enable Rumble",
                   &bound_state->controller_map->enable_rumble);
            Toggle("Invert Left X Axis",
                   &bound_state->controller_map->controller_mapping
                        .invert_axis_left_x);
            Toggle("Invert Left Y Axis",
                   &bound_state->controller_map->controller_mapping
                        .invert_axis_left_y);
            Toggle("Invert Right X Axis",
                   &bound_state->controller_map->controller_mapping
                        .invert_axis_right_x);
            Toggle("Invert Right Y Axis",
                   &bound_state->controller_map->controller_mapping
                        .invert_axis_right_y);
        }

        if (ImGui::Button("Reset to Default")) {
            xemu_input_reset_input_mapping(bound_state);
        }

        ImGui::PopStyleColor();
        ImGui::PopID();
    }

    SectionTitle("Options");
    Toggle("Auto-bind controllers", &g_config.input.auto_bind,
           "Bind newly connected controllers to any open port");
    Toggle("Background controller input capture",
           &g_config.input.background_input_capture,
           "Capture even if window is unfocused (requires restart)");
}

void MainMenuInputView::Hide()
{
    m_rebinding = nullptr;
}

void MainMenuInputView::PopulateTableController(ControllerState *state)
{
    if (!state)
        return;

    // Must match g_keyboard_scancode_map and the controller
    // button map below.
    static constexpr const char *face_button_index_to_name_map[15] = {
        "A",
        "B",
        "X",
        "Y",
        "Back",
        "Guide",
        "Start",
        "Left Stick Button",
        "Right Stick Button",
        "White",
        "Black",
        "DPad Up",
        "DPad Down",
        "DPad Left",
        "DPad Right",
    };

    // Must match g_keyboard_scancode_map[15:]. Each axis requires
    // two keys for the positive and negative direction with the
    // exception of the triggers, which only require one each.
    static constexpr const char *keyboard_stick_index_to_name_map[10] = {
        "Left Stick Up",
        "Left Stick Left",
        "Left Stick Right",
        "Left Stick Down",
        "Left Trigger",
        "Right Stick Up",
        "Right Stick Left",
        "Right Stick Right",
        "Right Stick Down",
        "Right Trigger",
    };

    // Must match controller axis map below.
    static constexpr const char *gamepad_axis_index_to_name_map[6] = {
        "Left Stick Axis X",
        "Left Stick Axis Y",
        "Right Stick Axis X",
        "Right Stick Axis Y",
        "Left Trigger Axis",
        "Right Trigger Axis",
    };

    bool is_keyboard = state->type == INPUT_DEVICE_SDL_KEYBOARD;

    int num_axis_mappings;
    const char *const *axis_index_to_name_map;
    if (is_keyboard) {
      num_axis_mappings = std::size(keyboard_stick_index_to_name_map);
      axis_index_to_name_map = keyboard_stick_index_to_name_map;
    } else {
      num_axis_mappings = std::size(gamepad_axis_index_to_name_map);
      axis_index_to_name_map = gamepad_axis_index_to_name_map;
    }

    constexpr int num_face_buttons = std::size(face_button_index_to_name_map);
    const int table_rows = num_axis_mappings + num_face_buttons;
    for (int i = 0; i < table_rows; ++i) {
        ImGui::TableNextRow();

        // Button/Axis Name Column
        ImGui::TableSetColumnIndex(0);

        if (i < num_face_buttons) {
          ImGui::Text("%s", face_button_index_to_name_map[i]);
        } else {
          ImGui::Text("%s", axis_index_to_name_map[i - num_face_buttons]);
        }

        // Button Binding Column
        ImGui::TableSetColumnIndex(1);

        if (m_rebinding && m_rebinding->GetTableRow() == i) {
            ImGui::Text("Press a key to rebind");
            continue;
        }

        const char *remap_button_text = "Invalid";
        if (is_keyboard) {
          // g_keyboard_scancode_map includes both face buttons and axis buttons.
            int keycode = *(g_keyboard_scancode_map[i]);
            if (keycode != SDL_SCANCODE_UNKNOWN) {
                remap_button_text =
                    SDL_GetScancodeName(static_cast<SDL_Scancode>(keycode));
            }
        } else if (i < num_face_buttons) {
                int *button_map[num_face_buttons] = {
                    &state->controller_map->controller_mapping.a,
                    &state->controller_map->controller_mapping.b,
                    &state->controller_map->controller_mapping.x,
                    &state->controller_map->controller_mapping.y,
                    &state->controller_map->controller_mapping.back,
                    &state->controller_map->controller_mapping.guide,
                    &state->controller_map->controller_mapping.start,
                    &state->controller_map->controller_mapping.lstick_btn,
                    &state->controller_map->controller_mapping.rstick_btn,
                    &state->controller_map->controller_mapping.lshoulder,
                    &state->controller_map->controller_mapping.rshoulder,
                    &state->controller_map->controller_mapping.dpad_up,
                    &state->controller_map->controller_mapping.dpad_down,
                    &state->controller_map->controller_mapping.dpad_left,
                    &state->controller_map->controller_mapping.dpad_right,
                };

                int button = *(button_map[i]);
                if (button != SDL_GAMEPAD_BUTTON_INVALID) {
                    remap_button_text = SDL_GetGamepadStringForButton(
                        static_cast<SDL_GamepadButton>(button));
                }
        } else {
          int *axis_map[6] = {
            &state->controller_map->controller_mapping.axis_left_x,
            &state->controller_map->controller_mapping.axis_left_y,
            &state->controller_map->controller_mapping.axis_right_x,
            &state->controller_map->controller_mapping.axis_right_y,
            &state->controller_map->controller_mapping
              .axis_trigger_left,
            &state->controller_map->controller_mapping
              .axis_trigger_right,
          };
          int axis = *(axis_map[i - num_face_buttons]);
          if (axis != SDL_GAMEPAD_AXIS_INVALID) {
            remap_button_text = SDL_GetGamepadStringForAxis(
                static_cast<SDL_GamepadAxis>(axis));
          }
        }

        ImGui::PushID(i);
        float tw = ImGui::CalcTextSize(remap_button_text).x;
        auto &style = ImGui::GetStyle();
        float max_button_width =
          tw + g_viewport_mgr.m_scale * 2 * style.FramePadding.x;

        float min_button_width = ImGui::GetColumnWidth(1) / 2;
        float button_width = std::max(min_button_width, max_button_width);

        if (ImGui::Button(remap_button_text, ImVec2(button_width, 0))) {
          if (is_keyboard) {
            m_rebinding =
              std::make_unique<ControllerKeyboardRebindingMap>(i);
          } else {
            m_rebinding =
              std::make_unique<ControllerGamepadRebindingMap>(i,
                  state);
          }
        }
        ImGui::PopID();
    }
}

void MainMenuDisplayView::Draw()
{
    SectionTitle("Renderer");
    ChevronCombo("Backend", &g_config.display.renderer,
                 "Null\0"
                 "OpenGL\0"
#ifdef CONFIG_VULKAN
                 "Vulkan\0"
#endif
                 ,
                 "Select desired renderer implementation");
    int rendering_scale = nv2a_get_surface_scale_factor() - 1;
    if (ChevronCombo("Internal resolution scale", &rendering_scale,
                     "1x\0"
                     "2x\0"
                     "3x\0"
                     "4x\0"
                     "5x\0"
                     "6x\0"
                     "7x\0"
                     "8x\0"
                     "9x\0"
                     "10x\0",
                     "Increase surface scaling factor for higher quality")) {
        nv2a_set_surface_scale_factor(rendering_scale+1);
    }

    SectionTitle("Window");
    bool fs = xemu_is_fullscreen();
    if (Toggle("Fullscreen", &fs, "Enable fullscreen now")) {
        xemu_toggle_fullscreen();
    }
    Toggle("Fullscreen on startup",
           &g_config.display.window.fullscreen_on_startup,
           "Start xemu in fullscreen when opened");
    Toggle("Exclusive fullscreen",
           &g_config.display.window.fullscreen_exclusive,
           "May improve responsiveness, but slows window switching");
    if (ChevronCombo("Window size", &g_config.display.window.startup_size,
                     "Last Used\0"
                     "640x480\0"
                     "720x480\0"
                     "1280x720\0"
                     "1280x800\0"
                     "1280x960\0"
                     "1920x1080\0"
                     "2560x1440\0"
                     "2560x1600\0"
                     "2560x1920\0"
                     "3840x2160\0",
                     "Select preferred startup window size")) {
    }
    Toggle("Vertical refresh sync", &g_config.display.window.vsync,
           "Sync to screen vertical refresh to reduce tearing artifacts");

    SectionTitle("Interface");
    Toggle("Show main menu bar", &g_config.display.ui.show_menubar,
           "Show main menu bar when mouse is activated");
    Toggle("Show notifications", &g_config.display.ui.show_notifications,
           "Display notifications in upper-right corner");
    Toggle("Hide mouse cursor", &g_config.display.ui.hide_cursor,
           "Hide the mouse cursor when it is not moving");

    int ui_scale_idx;
    if (g_config.display.ui.auto_scale) {
        ui_scale_idx = 0;
    } else {
        ui_scale_idx = g_config.display.ui.scale;
        if (ui_scale_idx < 0) ui_scale_idx = 0;
        else if (ui_scale_idx > 2) ui_scale_idx = 2;
    }
    if (ChevronCombo("UI scale", &ui_scale_idx,
                     "Auto\0"
                     "1x\0"
                     "2x\0",
                     "Interface element scale")) {
        if (ui_scale_idx == 0) {
            g_config.display.ui.auto_scale = true;
        } else {
            g_config.display.ui.auto_scale = false;
            g_config.display.ui.scale = ui_scale_idx;
        }
    }
    Toggle("Animations", &g_config.display.ui.use_animations,
           "Enable xemu user interface animations");
    ChevronCombo("Display mode", &g_config.display.ui.fit,
                 "Center\0"
                 "Scale\0"
                 "Stretch\0",
                 "Select how the framebuffer should fit or scale into the window");
    ChevronCombo("Aspect ratio", &g_config.display.ui.aspect_ratio,
                 "Native\0"
                 "Auto (Default)\0"
                 "4:3\0"
                 "16:9\0",
                 "Select the displayed aspect ratio");
}

void MainMenuAudioView::Draw()
{
    SectionTitle("Volume");
    char buf[32];
    snprintf(buf, sizeof(buf), "Limit output volume (%d%%)",
             (int)(g_config.audio.volume_limit * 100));
    Slider("Output volume limit", &g_config.audio.volume_limit, buf);

    SectionTitle("Quality");
    Toggle("Real-time DSP processing", &g_config.audio.use_dsp,
           "Enable improved audio accuracy (experimental)");

}

NetworkInterface::NetworkInterface(pcap_if_t *pcap_desc, char *_friendlyname)
{
    m_pcap_name = pcap_desc->name;
    m_description = pcap_desc->description ?: pcap_desc->name;
    if (_friendlyname) {
        char *tmp =
            g_strdup_printf("%s (%s)", _friendlyname, m_description.c_str());
        m_friendly_name = tmp;
        g_free((gpointer)tmp);
    } else {
        m_friendly_name = m_description;
    }
}

NetworkInterfaceManager::NetworkInterfaceManager()
{
    m_current_iface = NULL;
    m_failed_to_load_lib = false;
}

void NetworkInterfaceManager::Refresh(void)
{
    pcap_if_t *alldevs, *iter;
    char err[PCAP_ERRBUF_SIZE];

    if (xemu_net_is_enabled()) {
        return;
    }

#if defined(_WIN32)
    if (pcap_load_library()) {
        m_failed_to_load_lib = true;
        return;
    }
#endif

    m_ifaces.clear();
    m_current_iface = NULL;

    if (pcap_findalldevs(&alldevs, err)) {
        return;
    }

    for (iter=alldevs; iter != NULL; iter=iter->next) {
#if defined(_WIN32)
        char *friendly_name = get_windows_interface_friendly_name(iter->name);
        m_ifaces.emplace_back(new NetworkInterface(iter, friendly_name));
        if (friendly_name) {
            g_free((gpointer)friendly_name);
        }
#else
        m_ifaces.emplace_back(new NetworkInterface(iter));
#endif
        if (!strcmp(g_config.net.pcap.netif, iter->name)) {
            m_current_iface = m_ifaces.back().get();
        }
    }

    pcap_freealldevs(alldevs);
}

void NetworkInterfaceManager::Select(NetworkInterface &iface)
{
    m_current_iface = &iface;
    xemu_settings_set_string(&g_config.net.pcap.netif,
                             iface.m_pcap_name.c_str());
}

bool NetworkInterfaceManager::IsCurrent(NetworkInterface &iface)
{
    return &iface == m_current_iface;
}

MainMenuNetworkView::MainMenuNetworkView()
{
    should_refresh = true;
}

void MainMenuNetworkView::Draw()
{
    SectionTitle("Adapter");
    bool enabled = xemu_net_is_enabled();
    g_config.net.enable = enabled;
    if (Toggle("Enable", &g_config.net.enable,
               enabled ? "Virtual network connected (disable to change network "
                         "settings)" :
                         "Connect virtual network cable to machine")) {
        if (enabled) {
            xemu_net_disable();
        } else {
            xemu_net_enable();
        }
    }

    bool appearing = ImGui::IsWindowAppearing();
    if (enabled) ImGui::BeginDisabled();
    if (ChevronCombo(
            "Attached to", &g_config.net.backend,
            "NAT\0"
            "UDP Tunnel\0"
            "Bridged Adapter\0",
            "Controls what the virtual network controller interfaces with")) {
        appearing = true;
    }
    SectionTitle("Options");
    switch (g_config.net.backend) {
    case CONFIG_NET_BACKEND_PCAP:
        DrawPcapOptions(appearing);
        break;
    case CONFIG_NET_BACKEND_NAT:
        DrawNatOptions(appearing);
        break;
    case CONFIG_NET_BACKEND_UDP:
        DrawUdpOptions(appearing);
        break;
    default: break;
    }
    if (enabled) ImGui::EndDisabled();
}

void MainMenuNetworkView::DrawPcapOptions(bool appearing)
{
    if (iface_mgr.get() == nullptr) {
        iface_mgr.reset(new NetworkInterfaceManager());
        iface_mgr->Refresh();
    }

    if (iface_mgr->m_failed_to_load_lib) {
#if defined(_WIN32)
        const char *msg = "npcap library could not be loaded.\n"
                          "To use this backend, please install npcap.";
        ImGui::Text("%s", msg);
        ImGui::Dummy(ImVec2(0,10*g_viewport_mgr.m_scale));
        ImGui::SetCursorPosX((ImGui::GetWindowWidth()-120*g_viewport_mgr.m_scale)/2);
        if (ImGui::Button("Install npcap", ImVec2(120*g_viewport_mgr.m_scale, 0))) {
            SDL_OpenURL("https://nmap.org/npcap/");
        }
#endif
    } else {
        const char *selected_display_name =
            (iface_mgr->m_current_iface ?
                 iface_mgr->m_current_iface->m_friendly_name.c_str() :
                 g_config.net.pcap.netif);
        float combo_width = ImGui::GetColumnWidth();
        float combo_size_ratio = 0.5;
        combo_width *= combo_size_ratio;
        PrepareComboTitleDescription("Network interface",
                                     "Host network interface to bridge with",
                                     combo_size_ratio);
        ImGui::SetNextItemWidth(combo_width);
        ImGui::PushFont(g_font_mgr.m_menu_font_small);
        if (ImGui::BeginCombo("###network_iface", selected_display_name,
                              ImGuiComboFlags_NoArrowButton)) {
            if (should_refresh) {
                iface_mgr->Refresh();
                should_refresh = false;
            }

            int i = 0;
            for (auto &iface : iface_mgr->m_ifaces) {
                bool is_selected = iface_mgr->IsCurrent((*iface));
                ImGui::PushID(i++);
                if (ImGui::Selectable(iface->m_friendly_name.c_str(),
                                      is_selected)) {
                    iface_mgr->Select((*iface));
                }
                if (is_selected) ImGui::SetItemDefaultFocus();
                ImGui::PopID();
            }
            ImGui::EndCombo();
        } else {
            should_refresh = true;
        }
        ImGui::PopFont();
        DrawComboChevron();
    }
}

void MainMenuNetworkView::DrawNatOptions(bool appearing)
{
    static ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
    WidgetTitleDescriptionItem(
        "Port Forwarding",
        "Configure xemu to forward connections to guest on these ports");
    float p = ImGui::GetFrameHeight() * 0.3;
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(p, p));
    if (ImGui::BeginTable("port_forward_tbl", 4, flags))
    {
        ImGui::TableSetupColumn("Host Port");
        ImGui::TableSetupColumn("Guest Port");
        ImGui::TableSetupColumn("Protocol");
        ImGui::TableSetupColumn("Action");
        ImGui::TableHeadersRow();

        for (unsigned int row = 0; row < g_config.net.nat.forward_ports_count; row++)
        {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", g_config.net.nat.forward_ports[row].host);

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%d", g_config.net.nat.forward_ports[row].guest);

            ImGui::TableSetColumnIndex(2);
            switch (g_config.net.nat.forward_ports[row].protocol) {
            case CONFIG_NET_NAT_FORWARD_PORTS_PROTOCOL_TCP:
                ImGui::TextUnformatted("TCP"); break;
            case CONFIG_NET_NAT_FORWARD_PORTS_PROTOCOL_UDP:
                ImGui::TextUnformatted("UDP"); break;
            default: assert(0);
            }

            ImGui::TableSetColumnIndex(3);
            ImGui::PushID(row);
            if (ImGui::Button("Remove")) {
                remove_net_nat_forward_ports(row);
            }
            ImGui::PopID();
        }

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        static char buf[8] = {"1234"};
        ImGui::SetNextItemWidth(ImGui::GetColumnWidth());
        ImGui::InputText("###hostport", buf, sizeof(buf));

        ImGui::TableSetColumnIndex(1);
        static char buf2[8] = {"1234"};
        ImGui::SetNextItemWidth(ImGui::GetColumnWidth());
        ImGui::InputText("###guestport", buf2, sizeof(buf2));

        ImGui::TableSetColumnIndex(2);
        static CONFIG_NET_NAT_FORWARD_PORTS_PROTOCOL protocol =
            CONFIG_NET_NAT_FORWARD_PORTS_PROTOCOL_TCP;
        assert(sizeof(protocol) >= sizeof(int));
        ImGui::SetNextItemWidth(ImGui::GetColumnWidth());
        ImGui::Combo("###protocol", &protocol, "TCP\0UDP\0");

        ImGui::TableSetColumnIndex(3);
        if (ImGui::Button("Add")) {
            int host, guest;
            if (sscanf(buf, "%d", &host) == 1 &&
                sscanf(buf2, "%d", &guest) == 1) {
                add_net_nat_forward_ports(host, guest, protocol);
            }
        }

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

void MainMenuNetworkView::DrawUdpOptions(bool appearing)
{
    if (appearing) {
        strncpy(remote_addr, g_config.net.udp.remote_addr,
                sizeof(remote_addr) - 1);
        strncpy(local_addr, g_config.net.udp.bind_addr, sizeof(local_addr) - 1);
    }

    float size_ratio = 0.5;
    float width = ImGui::GetColumnWidth() * size_ratio;
    ImGui::PushFont(g_font_mgr.m_menu_font_small);
    PrepareComboTitleDescription(
        "Remote Address",
        "Destination addr:port to forward packets to (1.2.3.4:9968)",
        size_ratio);
    ImGui::SetNextItemWidth(width);
    if (ImGui::InputText("###remote_host", remote_addr, sizeof(remote_addr))) {
        xemu_settings_set_string(&g_config.net.udp.remote_addr, remote_addr);
    }
    PrepareComboTitleDescription(
        "Bind Address", "Local addr:port to receive packets on (0.0.0.0:9968)",
        size_ratio);
    ImGui::SetNextItemWidth(width);
    if (ImGui::InputText("###local_host", local_addr, sizeof(local_addr))) {
        xemu_settings_set_string(&g_config.net.udp.bind_addr, local_addr);
    }
    ImGui::PopFont();
}

MainMenuSnapshotsView::MainMenuSnapshotsView() : MainMenuTabView()
{
    xemu_snapshots_mark_dirty();

    m_search_regex = NULL;
    m_current_title_id = 0;
}

MainMenuSnapshotsView::~MainMenuSnapshotsView()
{
    g_free(m_search_regex);
}

bool MainMenuSnapshotsView::BigSnapshotButton(QEMUSnapshotInfo *snapshot,
                                              XemuSnapshotData *data,
                                              int current_snapshot_binding)
{
    ImGuiStyle &style = ImGui::GetStyle();
    ImDrawList *draw_list = ImGui::GetWindowDrawList();

    ImGui::PushFont(g_font_mgr.m_menu_font_small);
    ImVec2 ts_sub = ImGui::CalcTextSize(snapshot->name);
    ImGui::PopFont();

    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        g_viewport_mgr.Scale(ImVec2(5, 5)));

    ImGui::PushFont(g_font_mgr.m_menu_font_medium);

    ImVec2 ts_title = ImGui::CalcTextSize(snapshot->name);
    ImVec2 thumbnail_size = g_viewport_mgr.Scale(
        ImVec2(XEMU_SNAPSHOT_THUMBNAIL_WIDTH, XEMU_SNAPSHOT_THUMBNAIL_HEIGHT));
    ImVec2 thumbnail_pos(style.FramePadding.x, style.FramePadding.y);
    ImVec2 name_pos(thumbnail_pos.x + thumbnail_size.x +
                        style.FramePadding.x * 2,
                    thumbnail_pos.y);
    ImVec2 title_pos(name_pos.x,
                     name_pos.y + ts_title.y + style.FramePadding.x);
    ImVec2 date_pos(name_pos.x,
                    title_pos.y + ts_title.y + style.FramePadding.x);
    ImVec2 binding_pos(name_pos.x,
                       date_pos.y + ts_title.y + style.FramePadding.x);
    ImVec2 button_size(-FLT_MIN,
                       fmax(thumbnail_size.y + style.FramePadding.y * 2,
                            ts_title.y + ts_sub.y + style.FramePadding.y * 3));

    bool load = ImGui::Button("###button", button_size);

    ImGui::PopFont();

    const ImVec2 p0 = ImGui::GetItemRectMin();
    const ImVec2 p1 = ImGui::GetItemRectMax();
    draw_list->PushClipRect(p0, p1, true);

    // Snapshot thumbnail
    GLuint thumbnail = data->gl_thumbnail ? data->gl_thumbnail : g_icon_tex;
    int thumbnail_width, thumbnail_height;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, thumbnail);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH,
                             &thumbnail_width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT,
                             &thumbnail_height);

    // Draw black background behind thumbnail
    ImVec2 thumbnail_min(p0.x + thumbnail_pos.x, p0.y + thumbnail_pos.y);
    ImVec2 thumbnail_max(thumbnail_min.x + thumbnail_size.x,
                         thumbnail_min.y + thumbnail_size.y);
    draw_list->AddRectFilled(thumbnail_min, thumbnail_max, IM_COL32_BLACK);

    // Draw centered thumbnail image
    int scaled_width, scaled_height;
    ScaleDimensions(thumbnail_width, thumbnail_height, thumbnail_size.x,
                    thumbnail_size.y, &scaled_width, &scaled_height);
    ImVec2 img_min =
        ImVec2(thumbnail_min.x + (thumbnail_size.x - scaled_width) / 2,
               thumbnail_min.y + (thumbnail_size.y - scaled_height) / 2);
    ImVec2 img_max =
        ImVec2(img_min.x + scaled_width, img_min.y + scaled_height);
    draw_list->AddImage((ImTextureID)(uint64_t)thumbnail, img_min, img_max);

    // Snapshot title
    ImGui::PushFont(g_font_mgr.m_menu_font_medium);
    draw_list->AddText(ImVec2(p0.x + name_pos.x, p0.y + name_pos.y),
                       IM_COL32(255, 255, 255, 255), snapshot->name);
    ImGui::PopFont();

    // Snapshot XBE title name
    ImGui::PushFont(g_font_mgr.m_menu_font_small);
    const char *title_name = data->xbe_title_name ? data->xbe_title_name :
                                                    "(Unknown XBE Title Name)";
    draw_list->AddText(ImVec2(p0.x + title_pos.x, p0.y + title_pos.y),
                       IM_COL32(255, 255, 255, 200), title_name);

    // Snapshot date
    g_autoptr(GDateTime) date =
        g_date_time_new_from_unix_local(snapshot->date_sec);
    char *date_buf = g_date_time_format(date, "%Y-%m-%d %H:%M:%S");
    draw_list->AddText(ImVec2(p0.x + date_pos.x, p0.y + date_pos.y),
                       IM_COL32(255, 255, 255, 200), date_buf);
    g_free(date_buf);

    // Snapshot keyboard binding
    if (current_snapshot_binding != -1) {
        char *binding_text =
            g_strdup_printf("Bound to F%d", current_snapshot_binding + 5);
        draw_list->AddText(ImVec2(p0.x + binding_pos.x, p0.y + binding_pos.y),
                           IM_COL32(255, 255, 255, 200), binding_text);
        g_free(binding_text);
    }

    ImGui::PopFont();
    draw_list->PopClipRect();
    ImGui::PopStyleVar(2);

    return load;
}

void MainMenuSnapshotsView::ClearSearch()
{
    m_search_buf.clear();

    if (m_search_regex) {
        g_free(m_search_regex);
        m_search_regex = NULL;
    }
}

int MainMenuSnapshotsView::OnSearchTextUpdate(ImGuiInputTextCallbackData *data)
{
    GError *gerr = NULL;
    MainMenuSnapshotsView *win = (MainMenuSnapshotsView *)data->UserData;

    if (win->m_search_regex) {
        g_free(win->m_search_regex);
        win->m_search_regex = NULL;
    }

    if (data->BufTextLen == 0) {
        return 0;
    }

    char *buf = g_strdup_printf("(.*)%s(.*)", data->Buf);
    win->m_search_regex =
        g_regex_new(buf, (GRegexCompileFlags)0, (GRegexMatchFlags)0, &gerr);
    g_free(buf);
    if (gerr) {
        win->m_search_regex = NULL;
        return 1;
    }

    return 0;
}

void MainMenuSnapshotsView::Draw()
{
    g_snapshot_mgr.Refresh();

    SectionTitle("Snapshots");
    Toggle("Filter by current title",
           &g_config.general.snapshots.filter_current_game,
           "Only display snapshots created while running the currently running "
           "XBE");

    if (g_config.general.snapshots.filter_current_game) {
        struct xbe *xbe = xemu_get_xbe_info();
        if (xbe && xbe->cert) {
            if (xbe->cert->m_titleid != m_current_title_id) {
                char *title_name = g_utf16_to_utf8(xbe->cert->m_title_name, 40,
                                                   NULL, NULL, NULL);
                if (title_name) {
                    m_current_title_name = title_name;
                    g_free(title_name);
                } else {
                    m_current_title_name.clear();
                }

                m_current_title_id = xbe->cert->m_titleid;
            }
        } else {
            m_current_title_name.clear();
            m_current_title_id = 0;
        }
    }

    ImGui::SetNextItemWidth(ImGui::GetColumnWidth() * 0.8);
    ImGui::PushFont(g_font_mgr.m_menu_font_small);
    ImGui::InputTextWithHint("##search", "Search or name new snapshot...",
                             &m_search_buf, ImGuiInputTextFlags_CallbackEdit,
                             &OnSearchTextUpdate, this);

    bool snapshot_with_create_name_exists = false;
    for (int i = 0; i < g_snapshot_mgr.m_snapshots_len; ++i) {
        if (g_strcmp0(m_search_buf.c_str(),
                      g_snapshot_mgr.m_snapshots[i].name) == 0) {
            snapshot_with_create_name_exists = true;
            break;
        }
    }

    ImGui::SameLine();
    if (snapshot_with_create_name_exists) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8, 0, 0, 1));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 0, 0, 1));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 0, 0, 1));
    }
    if (ImGui::Button(snapshot_with_create_name_exists ? "Replace" : "Create",
                      ImVec2(-FLT_MIN, 0))) {
        xemu_snapshots_save(m_search_buf.empty() ? NULL : m_search_buf.c_str(),
                            NULL);
        ClearSearch();
    }
    if (snapshot_with_create_name_exists) {
        ImGui::PopStyleColor(3);
    }

    if (snapshot_with_create_name_exists && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("A snapshot with the name \"%s\" already exists. "
                          "This button will overwrite the existing snapshot.",
                          m_search_buf.c_str());
    }
    ImGui::PopFont();

    bool at_least_one_snapshot_displayed = false;

    for (int i = g_snapshot_mgr.m_snapshots_len - 1; i >= 0; i--) {
        if (g_config.general.snapshots.filter_current_game &&
            g_snapshot_mgr.m_extra_data[i].xbe_title_name &&
            m_current_title_name.size() &&
            strcmp(m_current_title_name.c_str(),
                   g_snapshot_mgr.m_extra_data[i].xbe_title_name)) {
            continue;
        }

        if (m_search_regex) {
            GMatchInfo *match;
            bool keep_entry = false;

            g_regex_match(m_search_regex, g_snapshot_mgr.m_snapshots[i].name,
                          (GRegexMatchFlags)0, &match);
            keep_entry |= g_match_info_matches(match);
            g_match_info_free(match);

            if (g_snapshot_mgr.m_extra_data[i].xbe_title_name) {
                g_regex_match(m_search_regex,
                              g_snapshot_mgr.m_extra_data[i].xbe_title_name,
                              (GRegexMatchFlags)0, &match);
                keep_entry |= g_match_info_matches(match);
                g_free(match);
            }

            if (!keep_entry) {
                continue;
            }
        }

        QEMUSnapshotInfo *snapshot = &g_snapshot_mgr.m_snapshots[i];
        XemuSnapshotData *data = &g_snapshot_mgr.m_extra_data[i];

        int current_snapshot_binding = -1;
        for (int j = 0; j < 4; ++j) {
            if (g_strcmp0(*(g_snapshot_shortcut_index_key_map[j]),
                          snapshot->name) == 0) {
                assert(current_snapshot_binding == -1);
                current_snapshot_binding = j;
            }
        }

        ImGui::PushID(i);

        ImVec2 pos = ImGui::GetCursorScreenPos();
        bool load = BigSnapshotButton(snapshot, data, current_snapshot_binding);

        // FIXME: Provide context menu control annotation
        if (ImGui::IsItemHovered() &&
            ImGui::IsKeyPressed(ImGuiKey_GamepadFaceLeft)) {
            ImGui::SetNextWindowPos(pos);
            ImGui::OpenPopup("Snapshot Options");
        }

        DrawSnapshotContextMenu(snapshot, data, current_snapshot_binding);

        ImGui::PopID();

        if (load) {
            ActionLoadSnapshotChecked(snapshot->name);
        }

        at_least_one_snapshot_displayed = true;
    }

    if (!at_least_one_snapshot_displayed) {
        ImGui::Dummy(g_viewport_mgr.Scale(ImVec2(0, 16)));
        const char *msg;
        if (g_snapshot_mgr.m_snapshots_len) {
            if (!m_search_buf.empty()) {
                msg = "Press Create to create new snapshot";
            } else {
                msg = "No snapshots match filter criteria";
            }
        } else {
            msg = "No snapshots to display";
        }
        ImVec2 dim = ImGui::CalcTextSize(msg);
        ImVec2 cur = ImGui::GetCursorPos();
        ImGui::SetCursorPosX(cur.x + (ImGui::GetColumnWidth() - dim.x) / 2);
        ImGui::TextColored(ImVec4(0.94f, 0.94f, 0.94f, 0.70f), "%s", msg);
    }
}

void MainMenuSnapshotsView::DrawSnapshotContextMenu(
    QEMUSnapshotInfo *snapshot, XemuSnapshotData *data,
    int current_snapshot_binding)
{
    if (!ImGui::BeginPopupContextItem("Snapshot Options")) {
        return;
    }

    if (ImGui::MenuItem("Load")) {
        ActionLoadSnapshotChecked(snapshot->name);
    }

    if (ImGui::BeginMenu("Keybinding")) {
        for (int i = 0; i < 4; ++i) {
            char *item_name = g_strdup_printf("Bind to F%d", i + 5);

            if (ImGui::MenuItem(item_name)) {
                if (current_snapshot_binding >= 0) {
                    xemu_settings_set_string(g_snapshot_shortcut_index_key_map
                                                 [current_snapshot_binding],
                                             "");
                }
                xemu_settings_set_string(g_snapshot_shortcut_index_key_map[i],
                                         snapshot->name);
                current_snapshot_binding = i;

                ImGui::CloseCurrentPopup();
            }

            g_free(item_name);
        }

        if (current_snapshot_binding >= 0) {
            if (ImGui::MenuItem("Unbind")) {
                xemu_settings_set_string(
                    g_snapshot_shortcut_index_key_map[current_snapshot_binding],
                    "");
                current_snapshot_binding = -1;
            }
        }
        ImGui::EndMenu();
    }

    ImGui::Separator();

    Error *err = NULL;

    if (ImGui::MenuItem("Replace")) {
        xemu_snapshots_save(snapshot->name, &err);
    }

    if (ImGui::MenuItem("Delete")) {
        xemu_snapshots_delete(snapshot->name, &err);
    }

    if (err) {
        xemu_queue_error_message(error_get_pretty(err));
        error_free(err);
    }

    ImGui::EndPopup();
}

MainMenuSystemView::MainMenuSystemView() : m_dirty(false)
{
}

void MainMenuSystemView::Draw()
{
    static const SDL_DialogFileFilter rom_file_filters[] = {
        { ".bin Files", "bin" },
        { ".rom Files", "rom" },
        { "All Files", "*" }
    };
    static const SDL_DialogFileFilter qcow_file_filters[] = {
        { ".qcow2 Files", "qcow2" },
        { "All Files", "*" }
    };

    if (m_dirty) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1),
                           "Application restart required to apply settings");
    }

    if ((int)g_config.sys.avpack == CONFIG_SYS_AVPACK_NONE) {
        ImGui::TextColored(ImVec4(1,0,0,1), "Setting AV Pack to NONE disables video output.");
    }

    SectionTitle("System Configuration");

    if (ChevronCombo(
            "System Memory", &g_config.sys.mem_limit,
            "64 MiB (Default)\0"
            "128 MiB\0",
            "Increase to 128 MiB for debug or homebrew applications")) {
        m_dirty = true;
    }

    if (ChevronCombo(
            "AV Pack", &g_config.sys.avpack,
            "SCART\0HDTV (Default)\0VGA\0RFU\0S-Video\0Composite\0None\0",
            "Select the attached AV pack")) {
        m_dirty = true;
    }

    SectionTitle("Files");
    FilePicker("MCPX Boot ROM", g_config.sys.files.bootrom_path,
               rom_file_filters, 3, false, [this](const char *path) {
                   xemu_settings_set_string(&g_config.sys.files.bootrom_path, path);
                   m_dirty = true;
                   g_main_menu.UpdateAboutViewConfigInfo();
               });
    FilePicker("Flash ROM (BIOS)", g_config.sys.files.flashrom_path,
               rom_file_filters, 3, false, [this](const char *path) {
                   xemu_settings_set_string(&g_config.sys.files.flashrom_path, path);
                   m_dirty = true;
                   g_main_menu.UpdateAboutViewConfigInfo();
               });
    FilePicker("Hard Disk", g_config.sys.files.hdd_path,
               qcow_file_filters, 2, false, [this](const char *path) {
                   xemu_settings_set_string(&g_config.sys.files.hdd_path, path);
                   m_dirty = true;
               });
    FilePicker("EEPROM", g_config.sys.files.eeprom_path,
               rom_file_filters, 3, false, [this](const char *path) {
                   xemu_settings_set_string(&g_config.sys.files.eeprom_path, path);
                   m_dirty = true;
               });
}

MainMenuAboutView::MainMenuAboutView() : m_config_info_text{ NULL }
{
}

void MainMenuAboutView::UpdateConfigInfoText()
{
    if (m_config_info_text) {
        g_free(m_config_info_text);
    }

    gchar *bootrom_checksum =
        GetFileMD5Checksum(g_config.sys.files.bootrom_path);
    if (!bootrom_checksum) {
        bootrom_checksum = g_strdup("None");
    }

    gchar *flash_rom_checksum =
        GetFileMD5Checksum(g_config.sys.files.flashrom_path);
    if (!flash_rom_checksum) {
        flash_rom_checksum = g_strdup("None");
    }

    m_config_info_text = g_strdup_printf("MCPX Boot ROM MD5 Hash:        %s\n"
                                         "Flash ROM (BIOS) MD5 Hash:     %s",
                                         bootrom_checksum, flash_rom_checksum);
    g_free(bootrom_checksum);
    g_free(flash_rom_checksum);
}

void MainMenuAboutView::Draw()
{
    static const char *build_info_text = NULL;
    if (build_info_text == NULL) {
        build_info_text =
            g_strdup_printf("Version:      %s\n"
                            "Commit:       %s\n"
                            "Date:         %s",
                            xemu_version, xemu_commit, xemu_date);
    }

    static const char *sys_info_text = NULL;
    if (sys_info_text == NULL) {
        const char *gl_shader_version =
            (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
        const char *gl_version = (const char *)glGetString(GL_VERSION);
        const char *gl_renderer = (const char *)glGetString(GL_RENDERER);
        const char *gl_vendor = (const char *)glGetString(GL_VENDOR);
        sys_info_text = g_strdup_printf(
            "CPU:          %s\nOS Platform:  %s\nOS Version:   "
            "%s\nManufacturer: %s\n"
            "GPU Model:    %s\nDriver:       %s\nShader:       %s",
            xemu_get_cpu_info(), SDL_GetPlatform(), xemu_get_os_info(),
            gl_vendor, gl_renderer, gl_version, gl_shader_version);
    }

    if (m_config_info_text == NULL) {
        UpdateConfigInfoText();
    }

    Logo();

    SectionTitle("Build Information");
    ImGui::PushFont(g_font_mgr.m_fixed_width_font);
    ImGui::InputTextMultiline("##build_info", (char *)build_info_text,
                              strlen(build_info_text) + 1,
                              ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 5),
                              ImGuiInputTextFlags_ReadOnly);
    ImGui::PopFont();

    SectionTitle("System Information");
    ImGui::PushFont(g_font_mgr.m_fixed_width_font);
    ImGui::InputTextMultiline("###systeminformation", (char *)sys_info_text,
                              strlen(sys_info_text) + 1,
                              ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 8),
                              ImGuiInputTextFlags_ReadOnly);
    ImGui::PopFont();

    SectionTitle("Config Information");
    ImGui::PushFont(g_font_mgr.m_fixed_width_font);
    ImGui::InputTextMultiline("##config_info", (char *)m_config_info_text,
                              strlen(m_config_info_text) + 1,
                              ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 3),
                              ImGuiInputTextFlags_ReadOnly);
    ImGui::PopFont();

    SectionTitle("Community");

    ImGui::Text("Visit");
    ImGui::SameLine();
    if (ImGui::SmallButton("https://xemu.app")) {
        SDL_OpenURL("https://xemu.app");
    }
    ImGui::SameLine();
    ImGui::Text("for more information");
}

MainMenuTabButton::MainMenuTabButton(std::string text, std::string icon)
    : m_icon(icon), m_text(text)
{
}

bool MainMenuTabButton::Draw(bool selected)
{
    ImGuiStyle &style = ImGui::GetStyle();

    ImU32 col = selected ?
                    ImGui::GetColorU32(style.Colors[ImGuiCol_ButtonHovered]) :
                    IM_COL32(0, 0, 0, 0);

    ImGui::PushStyleColor(ImGuiCol_Button, col);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          selected ? col : IM_COL32(32, 32, 32, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          selected ? col : IM_COL32(32, 32, 32, 255));
    int p = ImGui::GetTextLineHeight() * 0.5;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(p, p));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0.5));
    ImGui::PushFont(g_font_mgr.m_menu_font);

    ImVec2 button_size = ImVec2(-FLT_MIN, 0);
    auto text = string_format("%s %s", m_icon.c_str(), m_text.c_str());
    ImGui::PushID(this);
    bool status = ImGui::Button(text.c_str(), button_size);
    ImGui::PopID();
    ImGui::PopFont();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(3);
    return status;
}

MainMenuScene::MainMenuScene()
    : m_animation(0.12, 0.12), m_general_button("General", ICON_FA_GEARS),
      m_input_button("Input", ICON_FA_GAMEPAD),
      m_display_button("Display", ICON_FA_TV),
      m_audio_button("Audio", ICON_FA_VOLUME_HIGH),
      m_network_button("Network", ICON_FA_NETWORK_WIRED),
      m_snapshots_button("Snapshots", ICON_FA_CLOCK_ROTATE_LEFT),
      m_system_button("System", ICON_FA_MICROCHIP),
      m_about_button("About", ICON_FA_CIRCLE_INFO)
{
    m_had_focus_last_frame = false;
    m_focus_view = false;
    m_tabs.push_back(&m_general_button);
    m_tabs.push_back(&m_input_button);
    m_tabs.push_back(&m_display_button);
    m_tabs.push_back(&m_audio_button);
    m_tabs.push_back(&m_network_button);
    m_tabs.push_back(&m_snapshots_button);
    m_tabs.push_back(&m_system_button);
    m_tabs.push_back(&m_about_button);

    m_views.push_back(&m_general_view);
    m_views.push_back(&m_input_view);
    m_views.push_back(&m_display_view);
    m_views.push_back(&m_audio_view);
    m_views.push_back(&m_network_view);
    m_views.push_back(&m_snapshots_view);
    m_views.push_back(&m_system_view);
    m_views.push_back(&m_about_view);

    m_current_view_index = 0;
    m_next_view_index = m_current_view_index;
}

void MainMenuScene::ShowSettings()
{
    SetNextViewIndexWithFocus(g_config.general.last_viewed_menu_index);
}

void MainMenuScene::ShowSnapshots()
{
    SetNextViewIndexWithFocus(5);
}

void MainMenuScene::ShowSystem()
{
    SetNextViewIndexWithFocus(6);
}

void MainMenuScene::ShowAbout()
{
    SetNextViewIndexWithFocus(7);
}

void MainMenuScene::SetNextViewIndexWithFocus(int i)
{
    m_focus_view = true;
    SetNextViewIndex(i);

    if (!g_scene_mgr.IsDisplayingScene()) {
        g_scene_mgr.PushScene(*this);
    }
}

void MainMenuScene::Show()
{
    m_background.Show();
    m_nav_control_view.Show();
    m_animation.EaseIn();
}

void MainMenuScene::Hide()
{
    m_views[m_current_view_index]->Hide();
    m_background.Hide();
    m_nav_control_view.Hide();
    m_animation.EaseOut();
}

bool MainMenuScene::IsAnimating()
{
    return m_animation.IsAnimating();
}

void MainMenuScene::SetNextViewIndex(int i)
{
    m_views[m_current_view_index]->Hide();
    m_next_view_index = i % m_tabs.size();
    g_config.general.last_viewed_menu_index = i;
}

void MainMenuScene::HandleInput()
{
    bool nofocus = !ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow);
    bool focus = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows |
                                        ImGuiFocusedFlags_NoPopupHierarchy);

    // XXX: Ensure we have focus for two frames. If a user cancels a popup
    // window, we do not want to cancel main
    //      window as well.
    if (nofocus || (focus && m_had_focus_last_frame &&
                    (ImGui::IsKeyDown(ImGuiKey_GamepadFaceRight) ||
                     ImGui::IsKeyDown(ImGuiKey_Escape)))) {
        Hide();
        return;
    }

    if (focus && m_had_focus_last_frame) {
        if (ImGui::IsKeyPressed(ImGuiKey_GamepadL1)) {
            SetNextViewIndex((m_current_view_index + m_tabs.size() - 1) %
                             m_tabs.size());
        }

        if (ImGui::IsKeyPressed(ImGuiKey_GamepadR1)) {
            SetNextViewIndex((m_current_view_index + 1) % m_tabs.size());
        }
    }

    m_had_focus_last_frame = focus;
}

void MainMenuScene::UpdateAboutViewConfigInfo()
{
    m_about_view.UpdateConfigInfoText();
}

bool MainMenuScene::ConsumeRebindEvent(SDL_Event *event)
{
    return m_input_view.ConsumeRebindEvent(event);
}

bool MainMenuScene::IsInputRebinding()
{
    return m_input_view.IsInputRebinding();
}

bool MainMenuScene::Draw()
{
    m_animation.Step();
    m_background.Draw();
    m_nav_control_view.Draw();

    ImGuiIO &io = ImGui::GetIO();
    float t = m_animation.GetSinInterpolatedValue();
    float window_alpha = t;

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, window_alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

    ImVec4 extents = g_viewport_mgr.GetExtents();
    ImVec2 window_pos = ImVec2(io.DisplaySize.x / 2, extents.y);
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, ImVec2(0.5, 0));

    ImVec2 max_size = g_viewport_mgr.Scale(ImVec2(800, 0));
    float x = fmin(io.DisplaySize.x - extents.x - extents.z, max_size.x);
    float y = io.DisplaySize.y - extents.y - extents.w;
    ImGui::SetNextWindowSize(ImVec2(x, y));

    if (ImGui::Begin("###MainWindow", NULL,
                     ImGuiWindowFlags_NoDecoration |
                         ImGuiWindowFlags_NoSavedSettings)) {
        //
        // Nav menu
        //

        float width = ImGui::GetWindowWidth();
        float nav_width = width * 0.3;
        float content_width = width - nav_width;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(26, 26, 26, 255));

        ImGui::BeginChild("###MainWindowNav", ImVec2(nav_width, -1), true,
                          ImGuiWindowFlags_NavFlattened);

        bool move_focus_to_tab = false;
        if (m_current_view_index != m_next_view_index) {
            m_current_view_index = m_next_view_index;
            if (!m_focus_view) {
                move_focus_to_tab = true;
            }
        }

        int i = 0;
        for (auto &button : m_tabs) {
            if (move_focus_to_tab && i == m_current_view_index) {
                ImGui::SetKeyboardFocusHere();
                move_focus_to_tab = false;
            }
            if (button->Draw(i == m_current_view_index)) {
                SetNextViewIndex(i);
            }
            if (i == m_current_view_index) {
                ImGui::SetItemDefaultFocus();
            }
            i++;
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        //
        // Content
        //
        ImGui::SameLine();
        int s = ImGui::GetTextLineHeight() * 0.75;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(s, s));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(s, s));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,
                            6 * g_viewport_mgr.m_scale);

        ImGui::PushID(m_current_view_index);
        ImGui::BeginChild("###MainWindowContent", ImVec2(content_width, -1),
                          true,
                          ImGuiWindowFlags_AlwaysUseWindowPadding |
                              ImGuiWindowFlags_NavFlattened);

        if (!g_input_mgr.IsNavigatingWithController()) {
            // Close button
            ImGui::PushFont(g_font_mgr.m_menu_font);
            ImGuiStyle &style = ImGui::GetStyle();
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 128));
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32_BLACK_TRANS);
            ImVec2 pos = ImGui::GetCursorPos();
            ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x -
                                 style.FramePadding.x * 2.0f -
                                 ImGui::GetTextLineHeight());
            if (ImGui::Button(ICON_FA_XMARK)) {
                Hide();
            }
            ImGui::SetCursorPos(pos);
            ImGui::PopStyleColor(2);
            ImGui::PopFont();
        }

        ImGui::PushFont(g_font_mgr.m_default_font);
        if (m_focus_view) {
            ImGui::SetKeyboardFocusHere();
            m_focus_view = false;
        }
        m_views[m_current_view_index]->Draw();

        ImGui::PopFont();
        ImGui::EndChild();
        ImGui::PopID();
        ImGui::PopStyleVar(3);

        HandleInput();
    }
    ImGui::End();
    ImGui::PopStyleVar(5);

    return !m_animation.IsComplete();
}
