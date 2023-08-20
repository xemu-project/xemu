#include "common.hh"
#include "input-manager.hh"
#include "../xemu-input.h"

InputManager g_input_mgr;

InputManager::InputManager()
{
    m_last_mouse_pos = ImVec2(0, 0);
    m_navigating_with_controller = false;
}

void InputManager::Update()
{
    ImGuiIO& io = ImGui::GetIO();

    // Combine all controller states to allow any controller to navigate
    m_buttons = 0;
    int16_t axis[CONTROLLER_AXIS__COUNT] = {0};

    ControllerState *iter;
    QTAILQ_FOREACH(iter, &available_controllers, entry) {
        if (iter->type != INPUT_DEVICE_SDL_GAMECONTROLLER) continue;
        m_buttons |= iter->buttons;
        // We simply take any axis that is >10 % activation
        for (int i = 0; i < CONTROLLER_AXIS__COUNT; i++) {
            if ((iter->axis[i] > 3276) || (iter->axis[i] < -3276)) {
                axis[i] = iter->axis[i];
            }
        }
    }

    // If the mouse is moved, wake the ui
    ImVec2 current_mouse_pos = ImGui::GetMousePos();
    m_mouse_moved = false;
    if ((current_mouse_pos.x != m_last_mouse_pos.x) ||
        (current_mouse_pos.y != m_last_mouse_pos.y) ||
        ImGui::IsMouseDown(0) || ImGui::IsMouseDown(1) || ImGui::IsMouseDown(2)) {
        m_mouse_moved = true;
        m_last_mouse_pos = current_mouse_pos;
        m_navigating_with_controller = false;
    }

    // If mouse capturing is enabled (we are in a dialog), ensure the UI is alive
    bool controller_focus_capture = false;
    if (io.NavActive) {
        controller_focus_capture = true;
        m_navigating_with_controller |= !!m_buttons;
    }


    // Prevent controller events from going to the guest if they are being used
    // to navigate the HUD
    xemu_input_set_test_mode(controller_focus_capture); // FIXME: Rename 'test mode'

    // Update gamepad inputs
    #define IM_SATURATE(V)                      (V < 0.0f ? 0.0f : V > 1.0f ? 1.0f : V)
    #define MAP_BUTTON(KEY_NO, BUTTON_NO)       { io.AddKeyEvent(KEY_NO, !!(m_buttons & BUTTON_NO)); }
    #define MAP_ANALOG(KEY_NO, AXIS_NO, V0, V1) { float vn = (float)(axis[AXIS_NO] - V0) / (float)(V1 - V0); vn = IM_SATURATE(vn); io.AddKeyAnalogEvent(KEY_NO, vn > 0.1f, vn); }
    const int thumb_dead_zone = 8000;           // SDL_gamecontroller.h suggests using this value.
    MAP_BUTTON(ImGuiKey_GamepadStart,           CONTROLLER_BUTTON_START);
    MAP_BUTTON(ImGuiKey_GamepadBack,            CONTROLLER_BUTTON_BACK);
    MAP_BUTTON(ImGuiKey_GamepadFaceDown,        CONTROLLER_BUTTON_A);              // Xbox A, PS Cross
    MAP_BUTTON(ImGuiKey_GamepadFaceRight,       CONTROLLER_BUTTON_B);              // Xbox B, PS Circle
    MAP_BUTTON(ImGuiKey_GamepadFaceLeft,        CONTROLLER_BUTTON_X);              // Xbox X, PS Square
    MAP_BUTTON(ImGuiKey_GamepadFaceUp,          CONTROLLER_BUTTON_Y);              // Xbox Y, PS Triangle
    MAP_BUTTON(ImGuiKey_GamepadDpadLeft,        CONTROLLER_BUTTON_DPAD_LEFT);
    MAP_BUTTON(ImGuiKey_GamepadDpadRight,       CONTROLLER_BUTTON_DPAD_RIGHT);
    MAP_BUTTON(ImGuiKey_GamepadDpadUp,          CONTROLLER_BUTTON_DPAD_UP);
    MAP_BUTTON(ImGuiKey_GamepadDpadDown,        CONTROLLER_BUTTON_DPAD_DOWN);
    MAP_BUTTON(ImGuiKey_GamepadL1,              CONTROLLER_BUTTON_WHITE);
    MAP_BUTTON(ImGuiKey_GamepadR1,              CONTROLLER_BUTTON_BLACK);
    //MAP_ANALOG(ImGuiKey_GamepadL2,              SDL_CONTROLLER_AXIS_TRIGGERLEFT,  0.0f, 32767);
    //MAP_ANALOG(ImGuiKey_GamepadR2,              SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 0.0f, 32767);
    //MAP_BUTTON(ImGuiKey_GamepadL3,              SDL_CONTROLLER_BUTTON_LEFTSTICK);
    //MAP_BUTTON(ImGuiKey_GamepadR3,              SDL_CONTROLLER_BUTTON_RIGHTSTICK);
    MAP_ANALOG(ImGuiKey_GamepadLStickLeft,  CONTROLLER_AXIS_LSTICK_X, -thumb_dead_zone, -32768);
    MAP_ANALOG(ImGuiKey_GamepadLStickRight, CONTROLLER_AXIS_LSTICK_X, +thumb_dead_zone, +32767);
    MAP_ANALOG(ImGuiKey_GamepadLStickUp,    CONTROLLER_AXIS_LSTICK_Y, +thumb_dead_zone, +32768);
    MAP_ANALOG(ImGuiKey_GamepadLStickDown,  CONTROLLER_AXIS_LSTICK_Y, -thumb_dead_zone, -32767);
    MAP_ANALOG(ImGuiKey_GamepadRStickLeft,  CONTROLLER_AXIS_RSTICK_X, -thumb_dead_zone, -32768);
    MAP_ANALOG(ImGuiKey_GamepadRStickRight, CONTROLLER_AXIS_RSTICK_X, +thumb_dead_zone, +32767);
    MAP_ANALOG(ImGuiKey_GamepadRStickUp,    CONTROLLER_AXIS_RSTICK_Y, +thumb_dead_zone, +32768);
    MAP_ANALOG(ImGuiKey_GamepadRStickDown,  CONTROLLER_AXIS_RSTICK_Y, -thumb_dead_zone, -32767);
    #undef MAP_BUTTON
    #undef MAP_ANALOG
    #undef IM_SATURATE
}
