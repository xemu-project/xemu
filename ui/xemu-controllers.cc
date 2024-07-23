/*
 * xemu Controller Binding Management
 *
 * Copyright (C) 2020-2023 Matt Borgerson
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
#include <limits>
#include <string>

std::pair<bool, bool>
ControllerKeyboardRebindingMap::ConsumeRebindEvent(SDL_Event *event)
{
    if (event->type == SDL_KEYDOWN) {
        *(g_keyboard_scancode_map[table_row]) = event->key.keysym.scancode;

        return { true, true };
    }

    return { false, false };
}

std::pair<bool, bool>
ControllerGamepadRebindingMap::ConsumeRebindEvent(SDL_Event *event)
{
    if (event->type == SDL_CONTROLLERDEVICEREMOVED) {
        if (state->sdl_joystick_id == event->cdevice.which) {
            return { false, true };
        }
    } else if (event->type == SDL_CONTROLLERBUTTONUP && table_row < 15 &&
               seen_key_down) {
        // Bind on controller up ensures the UI does not immediately respond
        // once the new binding is applied

        if (state->sdl_joystick_id != event->cbutton.which) {
            return { false, false };
        }

        int *button_map[15] = {
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

        *(button_map[table_row]) = event->cbutton.button;

        return { true, true };
    } else if (event->type == SDL_CONTROLLERBUTTONDOWN && table_row < 15) {
        // If we are rebinding with a controller, we should not consume the key
        // up event from activating the button
        seen_key_down = true;
    } else if (event->type == SDL_CONTROLLERAXISMOTION && table_row >= 15 &&
               std::abs(event->caxis.value >> 1) >
                   std::numeric_limits<Sint16>::max() >> 2) {
        // FIXME: Allow face buttons to map to axes
        if (state->sdl_joystick_id != event->caxis.which) {
            return { false, false };
        }

        int *axis_map[6] = {
            &state->controller_map->controller_mapping.axis_left_x,
            &state->controller_map->controller_mapping.axis_left_y,
            &state->controller_map->controller_mapping.axis_right_x,
            &state->controller_map->controller_mapping.axis_right_y,
            &state->controller_map->controller_mapping.axis_trigger_left,
            &state->controller_map->controller_mapping.axis_trigger_right,
        };

        *(axis_map[table_row - 15]) = event->caxis.axis;

        return { true, true };
    }

    return { false, false };
}
