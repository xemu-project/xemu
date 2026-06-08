/*
 * Keycode Map Generator C Tests
 *
 * Copyright 2017 Pierre Ossman for Cendio AB
 *
 * This file is dual license under the terms of the GPLv2 or later
 * and 3-clause BSD licenses.
 */

#include <assert.h>
#include <string.h>

#include "osx2win32.h"
#include "osx2win32_name.h"

#include "osx2xkb.h"
#include "osx2xkb_name.h"

#include "html2win32.h"
#include "html2win32_name.h"

#include "osx.h"
#include "osx_name.h"

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))

int main(int argc, char** argv)
{
	unsigned i;

	assert(code_map_osx_to_win32_len == ARRAY_SIZE(code_map_osx_to_win32));
	assert(code_map_osx_to_win32[0x1d] == 0x30);
	assert(name_map_osx_to_win32_len == ARRAY_SIZE(name_map_osx_to_win32));
	assert(strcmp(name_map_osx_to_win32[0x1d], "VK_0") == 0);

	assert(code_map_osx_to_xkb_len == ARRAY_SIZE(code_map_osx_to_xkb));
	assert(strcmp(code_map_osx_to_xkb[0x1d], "AE10") == 0);
	assert(name_map_osx_to_xkb_len == ARRAY_SIZE(name_map_osx_to_xkb));
	assert(strcmp(name_map_osx_to_xkb[0x1d], "AE10") == 0);

	assert(code_map_html_to_win32_len == ARRAY_SIZE(code_map_html_to_win32));
	for (i = 0;i < code_map_html_to_win32_len;i++) {
		if (strcmp(code_map_html_to_win32[i].from, "ControlLeft") == 0) {
			assert(code_map_html_to_win32[i].to == 0x11);
			break;
		}
	}
	assert(i != code_map_html_to_win32_len);
	assert(name_map_html_to_win32_len == ARRAY_SIZE(name_map_html_to_win32));
	for (i = 0;i < name_map_html_to_win32_len;i++) {
		if (strcmp(name_map_html_to_win32[i].from, "ControlLeft") == 0) {
			assert(strcmp(name_map_html_to_win32[i].to, "VK_CONTROL") == 0);
			break;
		}
	}
	assert(i != name_map_html_to_win32_len);

	assert(code_table_osx_len == ARRAY_SIZE(code_table_osx));
	assert(code_table_osx[0x1d] == 0x3b);
	assert(name_table_osx_len == ARRAY_SIZE(name_table_osx));
	assert(strcmp(name_table_osx[0x1d], "Control") == 0);

	return 0;
}
