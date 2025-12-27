# Keycode Map Generator Python Tests
#
# Copyright 2017 Pierre Ossman for Cendio AB
#
# This file is dual license under the terms of the GPLv2 or later
# and 3-clause BSD licenses.

import osx2win32
import osx2win32_name

import osx2xkb
import osx2xkb_name

import html2win32
import html2win32_name

import osx
import osx_name

assert osx2win32.code_map_osx_to_win32[0x1d] == 0x30
assert osx2win32_name.name_map_osx_to_win32[0x1d] == "VK_0"

assert osx2xkb.code_map_osx_to_xkb[0x1d] == "AE10"
assert osx2xkb_name.name_map_osx_to_xkb[0x1d] == "AE10"

assert html2win32.code_map_html_to_win32["ControlLeft"] == 0x11
assert html2win32_name.name_map_html_to_win32["ControlLeft"] == "VK_CONTROL"

assert osx.code_table_osx[0x1d] == 0x3b;
assert osx_name.name_table_osx[0x1d] == "Control";
