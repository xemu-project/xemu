//
// xemu User Interface
//
// Copyright (C) 2026 Matt Borgerson
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
#include <vector>
#include <functional>
#include <cstdint>
#include <SDL3/SDL.h>

struct SystemActionBinding {
    std::string name;
    std::reference_wrapper<const int> scancode;
    std::reference_wrapper<const bool> ctrl;
    std::reference_wrapper<const bool> shift;
    std::reference_wrapper<const bool> alt;
    std::reference_wrapper<const bool> super;
    std::vector<std::reference_wrapper<const int>> controller_chords;
    std::function<void()> callback;
};

class SystemActionsManager {
private:
    std::vector<SystemActionBinding> m_bindings{};
    unsigned int m_prev_controller_buttons = 0;
    bool m_initialized = false;
    bool m_suppressed = false;

    static SystemActionsManager &GetInstance()
    {
        static SystemActionsManager instance;
        return instance;
    }

    SystemActionsManager() = default;

    void InitInstance();
    bool HandleKeyboardShortcuts();
    bool HandleControllerShortcuts(unsigned int buttons);
    void SetSuppressedInstance(bool suppressed)
    {
        m_suppressed = suppressed;
    }
    bool IsSuppressedInstance() const
    {
        return m_suppressed;
    }

public:
    static void Init()
    {
        GetInstance().InitInstance();
    }
    static bool HandleKeyboard()
    {
        return GetInstance().HandleKeyboardShortcuts();
    }
    static bool HandleController(unsigned int buttons)
    {
        return GetInstance().HandleControllerShortcuts(buttons);
    }
    static void SetSuppressed(bool suppressed)
    {
        GetInstance().SetSuppressedInstance(suppressed);
    }
    static bool IsSuppressed()
    {
        return GetInstance().IsSuppressedInstance();
    }
};
