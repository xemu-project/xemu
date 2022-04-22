/*
 * xemu Settings Management
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

#include "qemu/osdep.h"
#include <stdlib.h>
#include <SDL_filesystem.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include <iostream>
#include <toml.hpp>
#include <cnode.h>

#include "xemu-settings.h"
#define DEFINE_CONFIG_TREE
#include "xemu-config.h"

struct config g_config;
static bool settings_failed_to_load = true;
static const char *filename = "xemu.toml";
static const char *settings_path;

static bool xemu_settings_detect_portable_mode(void)
{
	bool val = false;
	char *portable_path = g_strdup_printf("%s%s", SDL_GetBasePath(), filename);
	FILE *tmpfile;
	if ((tmpfile = qemu_fopen(portable_path, "r"))) {
		fclose(tmpfile);
		val = true;
	}

	free(portable_path);
	return val;
}

void xemu_settings_set_path(const char *path)
{
	assert(path != NULL);
	assert(settings_path == NULL);
	settings_path = path;
	fprintf(stderr, "%s: config path: %s\n", __func__, settings_path);
}

const char *xemu_settings_get_path(void)
{
	if (settings_path != NULL) {
		return settings_path;
	}

	char *base = xemu_settings_detect_portable_mode()
	             ? SDL_GetBasePath()
	             : SDL_GetPrefPath("xemu", "xemu");
	assert(base != NULL);
	settings_path = g_strdup_printf("%s%s", base, filename);
	SDL_free(base);
	fprintf(stderr, "%s: config path: %s\n", __func__, settings_path);
	return settings_path;
}

const char *xemu_settings_get_default_eeprom_path(void)
{
	static char *eeprom_path = NULL;
	if (eeprom_path != NULL) {
		return eeprom_path;
	}

	char *base = xemu_settings_detect_portable_mode()
	             ? SDL_GetBasePath()
	             : SDL_GetPrefPath("xemu", "xemu");
	assert(base != NULL);
	eeprom_path = g_strdup_printf("%s%s", base, "eeprom.bin");
	SDL_free(base);
	return eeprom_path;
}

void xemu_settings_load(void)
{
	FILE *fd;
	size_t file_size = 0;
	char *file_buf = NULL;
	const char *settings_path = xemu_settings_get_path();

	fd = qemu_fopen(settings_path, "rb");
	if (!fd) {
		fprintf(stderr, "Failed to open settings path %s for reading\n", settings_path);
		goto out;
	}

	fseek(fd, 0, SEEK_END);
	file_size = ftell(fd);
	fseek(fd, 0, SEEK_SET);
	file_buf = (char *)malloc(file_size + 1);
	if (fread(file_buf, file_size, 1, fd) != 1) {
		fprintf(stderr, "Failed to read settings\n");
		fclose(fd);
		fd = NULL;
		free(file_buf);
		file_buf = NULL;
		goto out;
	}
	file_buf[file_size] = '\x00';

	try
	{
		auto tbl = toml::parse(file_buf);
		config_tree.update_from_table(tbl);
		settings_failed_to_load = false;
	}
	catch (const toml::parse_error& err)
	{
	   std::cerr
	       << "Error parsing file '" << *err.source().path
	       << "':\n" << err.description()
	       << "\n  (" << err.source().begin << ")\n";
	}

out:
	config_tree.store_to_struct(&g_config);
}

bool xemu_settings_did_fail_to_load(void)
{
	return settings_failed_to_load;
}

void xemu_settings_save(void)
{
	FILE *fd = qemu_fopen(xemu_settings_get_path(), "wb");
	assert(fd != NULL);
	config_tree.update_from_struct(&g_config);
	fprintf(fd, "%s", config_tree.generate_delta_toml().c_str());
	fclose(fd);
}