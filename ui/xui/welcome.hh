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

class FirstBootWindow
{
public:
    bool is_open;
    int m_step;       // 0=welcome, 1=files, 2=settings, 3=ready
    bool m_did_open_file_picker;
    FirstBootWindow();
    void Draw();
    void DrawStepIndicator(int total_steps, int current_step);
    void DrawWelcomeStep();
    void DrawFilesStep();
    void DrawSettingsStep();
    void DrawReadyStep();
    bool AreRequiredFilesConfigured();
};

extern FirstBootWindow first_boot_window;
