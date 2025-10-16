/*
 * xemu Settings Management
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

#ifndef XEMU_CONTROLLERS_H
#define XEMU_CONTROLLERS_H

#include "xemu-input.h"
#include "xemu-settings.h"
#include <SDL.h>

#ifdef __cplusplus

struct RebindingMap {
    // Returns [consume, cancel]:
    // consume: Whether the SDL_Event should not propagate to the UI
    // cancel: Whether this rebinding map should be cancelled
    virtual std::pair<bool, bool> ConsumeRebindEvent(SDL_Event *event) = 0;

    int GetTableRow() const
    {
        return table_row;
    }

    virtual ~RebindingMap()
    {
    }

protected:
    int table_row;
    RebindingMap(int table_row) : table_row{ table_row }
    {
    }
};

struct ControllerKeyboardRebindingMap : public virtual RebindingMap {
    std::pair<bool, bool> ConsumeRebindEvent(SDL_Event *event) override;

    ControllerKeyboardRebindingMap(int table_row) : RebindingMap(table_row)
    {
    }
};

class ControllerGamepadRebindingMap : public virtual RebindingMap {
    ControllerState *state;
    bool seen_key_down;

public:
    std::pair<bool, bool> ConsumeRebindEvent(SDL_Event *event) override;
    ControllerGamepadRebindingMap(int table_row, ControllerState *state)
        : RebindingMap(table_row), state{ state }, seen_key_down{ false }
    {
    }
};

extern "C" {
#endif

extern int *g_keyboard_scancode_map[25];

GamepadMappings *xemu_settings_load_gamepad_mapping(const char *guid);

#ifdef __cplusplus
}
#endif

#endif
