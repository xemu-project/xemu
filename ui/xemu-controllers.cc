/*
 * xemu Controller Binding Management
 *
 * Copyright (C) 2025 Matt Borgerson
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

#include "xemu-controllers.h"
#include "xemu-settings.h"
#include <assert.h>
#include <cmath>
#include <limits>

constexpr int controller_button_count = 15;
constexpr int controller_axes_count = 6;

RebindEventResult
ControllerKeyboardRebindingMap::ConsumeRebindEvent(SDL_Event *event)
{
    // Bind on key up
    // This ensures the UI does not immediately respond once the new binding is
    // applied
    if (event->type == SDL_EVENT_KEY_UP) {
        *(g_keyboard_scancode_map[m_table_row]) = event->key.scancode;
        return RebindEventResult::Complete;
    }

    return RebindEventResult::Ignore;
}

RebindEventResult ControllerGamepadRebindingMap::HandleButtonEvent(
    SDL_GamepadButtonEvent *event)
{
    if (m_state->sdl_joystick_id != event->which) {
        return RebindEventResult::Ignore;
    }

    int *button_map[controller_button_count] = {
        &m_state->controller_map->controller_mapping.a,
        &m_state->controller_map->controller_mapping.b,
        &m_state->controller_map->controller_mapping.x,
        &m_state->controller_map->controller_mapping.y,
        &m_state->controller_map->controller_mapping.back,
        &m_state->controller_map->controller_mapping.guide,
        &m_state->controller_map->controller_mapping.start,
        &m_state->controller_map->controller_mapping.lstick_btn,
        &m_state->controller_map->controller_mapping.rstick_btn,
        &m_state->controller_map->controller_mapping.lshoulder,
        &m_state->controller_map->controller_mapping.rshoulder,
        &m_state->controller_map->controller_mapping.dpad_up,
        &m_state->controller_map->controller_mapping.dpad_down,
        &m_state->controller_map->controller_mapping.dpad_left,
        &m_state->controller_map->controller_mapping.dpad_right,
    };

    // FIXME: Allow face buttons to map to axes
    if (m_table_row >= controller_button_count) {
        return RebindEventResult::Ignore;
    }

    // If we only track up events, then we might rebind to a button
    // that was already held down when the rebinding event began
    if (event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        m_seen_key_down = true;
        return RebindEventResult::Ignore;
    }

    // Bind on controller button up
    // This ensures the UI does not immediately respond once the new binding is
    // applied
    if (event->type != SDL_EVENT_GAMEPAD_BUTTON_UP || !m_seen_key_down) {
        return RebindEventResult::Ignore;
    }

    *(button_map[m_table_row]) = event->button;

    return RebindEventResult::Complete;
}

RebindEventResult
ControllerGamepadRebindingMap::HandleAxisEvent(SDL_GamepadAxisEvent *event)
{
    if (m_state->sdl_joystick_id != event->which) {
        return RebindEventResult::Ignore;
    }

    // Axis inputs cannot be bound to controller buttons
    if (m_table_row < controller_button_count) {
        return RebindEventResult::Ignore;
    }

    // Requre that the input be sufficiently outside of any deadzone range
    // before using it for rebinding
    if (std::abs(event->value >> 1) <=
        (std::numeric_limits<Sint16>::max() >> 2)) {
        return RebindEventResult::Ignore;
    }

    int *axis_map[controller_axes_count] = {
        &m_state->controller_map->controller_mapping.axis_left_x,
        &m_state->controller_map->controller_mapping.axis_left_y,
        &m_state->controller_map->controller_mapping.axis_right_x,
        &m_state->controller_map->controller_mapping.axis_right_y,
        &m_state->controller_map->controller_mapping.axis_trigger_left,
        &m_state->controller_map->controller_mapping.axis_trigger_right,
    };

    *(axis_map[m_table_row - controller_button_count]) = event->axis;

    return RebindEventResult::Complete;
}

RebindEventResult
ControllerGamepadRebindingMap::ConsumeRebindEvent(SDL_Event *event)
{
    switch (event->type) {
    case SDL_EVENT_GAMEPAD_REMOVED:
        return (m_state->sdl_joystick_id == event->gdevice.which) ?
                   RebindEventResult::Complete :
                   RebindEventResult::Ignore;
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        return HandleButtonEvent(&event->gbutton);
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        return HandleAxisEvent(&event->gaxis);
    default:
        return RebindEventResult::Ignore;
    }
}
