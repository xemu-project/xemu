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
#include <string>
#include "reporting.hh"

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

    CompatibilityReporter();
    ~CompatibilityReporter();
    void Draw();
};

extern CompatibilityReporter compatibility_reporter_window;
