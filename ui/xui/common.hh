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

#include <SDL.h>
#include <epoxy/gl.h>
#include "ui/xemu-settings.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>
#include <misc/cpp/imgui_stdlib.h>
#include <stb_image.h>

extern "C" {
#include <noc_file_dialog.h>

// Include necessary QEMU headers
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "sysemu/sysemu.h"
#include "sysemu/runstate.h"
#include "hw/xbox/mcpx/apu_debug.h"
#include "hw/xbox/nv2a/debug.h"
#include "hw/xbox/nv2a/nv2a.h"

#undef typename
#undef atomic_fetch_add
#undef atomic_fetch_and
#undef atomic_fetch_xor
#undef atomic_fetch_or
#undef atomic_fetch_sub
}

extern bool g_screenshot_pending;
extern float g_main_menu_height;
