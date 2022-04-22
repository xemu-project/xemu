/*
 * xemu Settings Management
 *
 * Primary storage for non-volatile user configuration. Basic key-value storage
 * that gets saved to an INI file. All entries should be accessed through the
 * appropriate getter/setter functions.
 *
 * Copyright (C) 2020-2021 Matt Borgerson
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

#ifndef XEMU_SETTINGS_H
#define XEMU_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "xemu-config.h"

extern struct config g_config;

// Determine whether settings were loaded or not
bool xemu_settings_did_fail_to_load(void);

// Override the default config file paths
void xemu_settings_set_path(const char *path);

// Get path of the config file on disk
const char *xemu_settings_get_path(void);

// Get path of the default generated eeprom file on disk
const char *xemu_settings_get_default_eeprom_path(void);

// Load config file from disk, or load defaults
void xemu_settings_load(void);

// Save config file to disk
void xemu_settings_save(void);

#include <stdlib.h>
#include <string.h>

static inline void xemu_settings_set_string(const char **str, const char *new_str)
{
	free((char*)*str);
	*str = strdup(new_str);
}

#ifdef __cplusplus
}
#endif

#endif
