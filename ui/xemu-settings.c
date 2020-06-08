/*
 * xemu Settings Management
 *
 * Copyright (C) 2020 Matt Borgerson
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

#include <stdlib.h>
#include <SDL_filesystem.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>

#include "xemu-settings.h"
#include "inih/ini.c" // FIXME

enum config_types {
	CONFIG_TYPE_STRING,
	CONFIG_TYPE_INT,
	CONFIG_TYPE_BOOL,
	CONFIG_TYPE_ENUM,
	CONFIG_TYPE__MAX
};

struct xemu_settings {
	// [system]
	char *flash_path;
	char *bootrom_path;
	char *hdd_path;
	char *dvd_path;
	char *eeprom_path;
	int   memory;
	int   short_animation; // Boolean

	// [display]
	int scale;
	int ui_scale;

	// [input]
	char *controller_1_guid;
	char *controller_2_guid;
	char *controller_3_guid;
	char *controller_4_guid;

	// [network]
	int   net_enabled; // Boolean
	int   net_backend;
	char *net_local_addr;
	char *net_remote_addr;

	// [misc]
	char *user_token;
};

struct enum_str_map {
	int         value;
	const char *str;
};

static const struct enum_str_map display_scale_map[DISPLAY_SCALE__COUNT+1] = {
	{ DISPLAY_SCALE_CENTER,  "center"  },
	{ DISPLAY_SCALE_SCALE,   "scale"   },
	{ DISPLAY_SCALE_STRETCH, "stretch" },
	{ 0,                     NULL      },
};

static const struct enum_str_map net_backend_map[XEMU_NET_BACKEND__COUNT+1] = {
	{ XEMU_NET_BACKEND_USER,       "user" },
	{ XEMU_NET_BACKEND_SOCKET_UDP, "udp"  },
	{ 0,                           NULL   },
};

struct config_offset_table {
	enum config_types type;
	const char *section;
	const char *name;
	ptrdiff_t offset;
	union {
		const char *default_str;
		int default_int;
		int default_bool;
	};
	const struct enum_str_map *enum_map;
} config_items[XEMU_SETTINGS__COUNT] = {
	// Please keep organized by section
	[XEMU_SETTINGS_SYSTEM_FLASH_PATH]   = { CONFIG_TYPE_STRING, "system", "flash_path",   offsetof(struct xemu_settings, flash_path),      { .default_str  = "" } },
	[XEMU_SETTINGS_SYSTEM_BOOTROM_PATH] = { CONFIG_TYPE_STRING, "system", "bootrom_path", offsetof(struct xemu_settings, bootrom_path),    { .default_str  = "" } },
	[XEMU_SETTINGS_SYSTEM_HDD_PATH]     = { CONFIG_TYPE_STRING, "system", "hdd_path",     offsetof(struct xemu_settings, hdd_path),        { .default_str  = "" } },
	[XEMU_SETTINGS_SYSTEM_DVD_PATH]     = { CONFIG_TYPE_STRING, "system", "dvd_path",     offsetof(struct xemu_settings, dvd_path),        { .default_str  = "" } },
	[XEMU_SETTINGS_SYSTEM_EEPROM_PATH]  = { CONFIG_TYPE_STRING, "system", "eeprom_path",  offsetof(struct xemu_settings, eeprom_path),     { .default_str  = "" } },
	[XEMU_SETTINGS_SYSTEM_MEMORY]       = { CONFIG_TYPE_INT,    "system", "memory",       offsetof(struct xemu_settings, memory),          { .default_int  = 64 } },
	[XEMU_SETTINGS_SYSTEM_SHORTANIM]    = { CONFIG_TYPE_BOOL,   "system", "shortanim",    offsetof(struct xemu_settings, short_animation), { .default_bool = 0  } },

	[XEMU_SETTINGS_DISPLAY_SCALE] =    { CONFIG_TYPE_ENUM, "display", "scale",    offsetof(struct xemu_settings, scale),    { .default_int = DISPLAY_SCALE_SCALE }, display_scale_map },
	[XEMU_SETTINGS_DISPLAY_UI_SCALE] = { CONFIG_TYPE_INT,  "display", "ui_scale", offsetof(struct xemu_settings, ui_scale), { .default_int = 1                   }                    },

	[XEMU_SETTINGS_INPUT_CONTROLLER_1_GUID] = { CONFIG_TYPE_STRING,   "input", "controller_1_guid", offsetof(struct xemu_settings, controller_1_guid), { .default_str = "" } },
	[XEMU_SETTINGS_INPUT_CONTROLLER_2_GUID] = { CONFIG_TYPE_STRING,   "input", "controller_2_guid", offsetof(struct xemu_settings, controller_2_guid), { .default_str = "" } },
	[XEMU_SETTINGS_INPUT_CONTROLLER_3_GUID] = { CONFIG_TYPE_STRING,   "input", "controller_3_guid", offsetof(struct xemu_settings, controller_3_guid), { .default_str = "" } },
	[XEMU_SETTINGS_INPUT_CONTROLLER_4_GUID] = { CONFIG_TYPE_STRING,   "input", "controller_4_guid", offsetof(struct xemu_settings, controller_4_guid), { .default_str = "" } },

	[XEMU_SETTINGS_NETWORK_ENABLED]     = { CONFIG_TYPE_BOOL,   "network", "enabled",     offsetof(struct xemu_settings, net_enabled),     { .default_bool = 0              } },
	[XEMU_SETTINGS_NETWORK_BACKEND]     = { CONFIG_TYPE_ENUM,   "network", "backend",     offsetof(struct xemu_settings, net_backend),     { .default_int = XEMU_NET_BACKEND_USER }, net_backend_map },
	[XEMU_SETTINGS_NETWORK_LOCAL_ADDR]  = { CONFIG_TYPE_STRING, "network", "local_addr",  offsetof(struct xemu_settings, net_local_addr),  { .default_str  = "0.0.0.0:9368" } },
	[XEMU_SETTINGS_NETWORK_REMOTE_ADDR] = { CONFIG_TYPE_STRING, "network", "remote_addr", offsetof(struct xemu_settings, net_remote_addr), { .default_str  = "1.2.3.4:9368" } },

	[XEMU_SETTINGS_MISC_USER_TOKEN] = { CONFIG_TYPE_STRING, "misc", "user_token", offsetof(struct xemu_settings, user_token), { .default_str  = "" } },
};

static const char *settings_path;
static const char *filename = "xemu.ini";
static struct xemu_settings *g_settings;
static int settings_failed_to_load = 0;

static void *xemu_settings_get_field(enum xemu_settings_keys key, enum config_types type)
{
	assert(key < XEMU_SETTINGS__COUNT);
	assert(config_items[key].type == type);
	return (void *)((char*)g_settings + config_items[key].offset);
}

int xemu_settings_set_string(enum xemu_settings_keys key, const char *str)
{
	char **field_str = (char **)xemu_settings_get_field(key, CONFIG_TYPE_STRING);
	free(*field_str);
	*field_str = strdup(str);
	return 0;
}

int xemu_settings_get_string(enum xemu_settings_keys key, const char **str)
{
	*str = *(const char **)xemu_settings_get_field(key, CONFIG_TYPE_STRING);
	return 0;
}

int xemu_settings_set_int(enum xemu_settings_keys key, int val)
{
	int *field_int = (int *)xemu_settings_get_field(key, CONFIG_TYPE_INT);
	*field_int = val;
	return 0;
}

int xemu_settings_get_int(enum xemu_settings_keys key, int *val)
{
	*val = *(int *)xemu_settings_get_field(key, CONFIG_TYPE_INT);
	return 0;
}

int xemu_settings_set_bool(enum xemu_settings_keys key, int val)
{
	int *field_int = (int *)xemu_settings_get_field(key, CONFIG_TYPE_BOOL);
	*field_int = val;
	return 0;
}

int xemu_settings_get_bool(enum xemu_settings_keys key, int *val)
{
	*val = *(int *)xemu_settings_get_field(key, CONFIG_TYPE_BOOL);
	return 0;
}

int xemu_settings_set_enum(enum xemu_settings_keys key, int val)
{
	int *field_int = (int *)xemu_settings_get_field(key, CONFIG_TYPE_ENUM);
	*field_int = val;
	return 0;
}

int xemu_settings_get_enum(enum xemu_settings_keys key, int *val)
{
	*val = *(int *)xemu_settings_get_field(key, CONFIG_TYPE_ENUM);
	return 0;
}

const char *xemu_settings_get_path(void)
{
	if (settings_path != NULL) {
		return settings_path;
	}

	char *base = SDL_GetPrefPath("xemu", "xemu");
	assert(base != NULL);
	size_t base_len = strlen(base);

	size_t filename_len = strlen(filename);
	size_t final_len = base_len + filename_len;
	final_len += 1; // Terminating null byte

	char *path = malloc(final_len);
	assert(path != NULL);

	// Copy base part
	memcpy(path, base, base_len);
	free(base);

	// Copy filename part
	memcpy(path+base_len, filename, strlen(filename));
	path[final_len-1] = '\0';

	settings_path = path;

	fprintf(stderr, "%s: config path: %s\n", __func__, settings_path);

	return settings_path;
}

static int xemu_enum_str_to_int(const struct enum_str_map *map, const char *str, int *value)
{
	for (int i = 0; map[i].str != NULL; i++) {
		if (strcmp(map[i].str, str) == 0) {
			*value = map[i].value;
			return 0;
		}
	}

	return -1;
}

static int xemu_enum_int_to_str(const struct enum_str_map *map, int value, const char **str)
{
	for (int i = 0; map[i].str != NULL; i++) {
		if (map[i].value == value) {
			*str = map[i].str;
			return 0;
		}
	}

	return -1;
}


static enum xemu_settings_keys xemu_key_from_name(const char *section, const char *name)
{
	for (int i = 0; i < XEMU_SETTINGS__COUNT; i++) {
		if ((strcmp(section, config_items[i].section) == 0) &&
			(strcmp(name, config_items[i].name) == 0)) {
			return i; // Found
		}
	}

	return XEMU_SETTINGS_INVALID;
}

static int config_parse_callback(void *user, const char *section, const char *name, const char *value)
{
	// struct xemu_settings *settings = (struct xemu_settings *)user;
	fprintf(stderr, "%s: [%s] %s = %s\n", __func__, section, name, value);

	enum xemu_settings_keys key = xemu_key_from_name(section, name);

	if (key == XEMU_SETTINGS_INVALID) {
		fprintf(stderr, "Ignoring unknown key %s.%s\n", section, name);
		return 1;
	}

	if (config_items[key].type == CONFIG_TYPE_STRING) {
		xemu_settings_set_string(key, value);
	} else if (config_items[key].type == CONFIG_TYPE_INT) {
		int int_val;
		int converted = sscanf(value, "%d", &int_val);
		if (converted != 1) {
			fprintf(stderr, "Error parsing %s.%s as integer. Got '%s'\n", section, name, value);
			return 0;
		}
		xemu_settings_set_int(key, int_val);
	} else if (config_items[key].type == CONFIG_TYPE_BOOL) {
		int int_val;
		if (strcmp(value, "true") == 0) {
			int_val = 1;
		} else if (strcmp(value, "false") == 0) {
			int_val = 0;
		} else {
			fprintf(stderr, "Error parsing %s.%s as boolean. Got '%s'\n", section, name, value);
			return 0;
		}
		xemu_settings_set_bool(key, int_val);
	} else if (config_items[key].type == CONFIG_TYPE_ENUM) {
		int int_val;
		int status = xemu_enum_str_to_int(config_items[key].enum_map, value, &int_val);
		if (status != 0) {
			fprintf(stderr, "Error parsing %s.%s as enum. Got '%s'\n", section, name, value);
			return 0;
		}
		xemu_settings_set_enum(key, int_val);
	} else {
		// Unimplemented
		assert(0);
	}

	// Success
	return 1;
}

static void xemu_settings_init_default(struct xemu_settings *settings)
{
	memset(settings, 0, sizeof(struct xemu_settings));
	for (int i = 0; i < XEMU_SETTINGS__COUNT; i++) {
		if (config_items[i].type == CONFIG_TYPE_STRING) {
			xemu_settings_set_string(i, config_items[i].default_str);
		} else if (config_items[i].type == CONFIG_TYPE_INT) {
			xemu_settings_set_int(i, config_items[i].default_int);
		} else if (config_items[i].type == CONFIG_TYPE_BOOL) {
			xemu_settings_set_bool(i, config_items[i].default_bool);
		} else if (config_items[i].type == CONFIG_TYPE_ENUM) {
			xemu_settings_set_enum(i, config_items[i].default_int);
		} else {
			// Unimplemented
			assert(0);
		}
	}
}

int xemu_settings_did_fail_to_load(void)
{
	return settings_failed_to_load;
}

void xemu_settings_load(void)
{
	// Should only call this once, at startup
	assert(g_settings == NULL);

	g_settings = malloc(sizeof(struct xemu_settings));
	assert(g_settings != NULL);
	xemu_settings_init_default(g_settings);

	// Parse configuration file
	int status = ini_parse(xemu_settings_get_path(),
		                   config_parse_callback,
		                   g_settings);
	if (status < 0) {
		// fprintf(stderr, "Failed to load config! Using defaults\n");
		settings_failed_to_load = 1;
	}
}

int xemu_settings_save(void)
{
	FILE *fd = fopen(xemu_settings_get_path(), "wb");
	assert(fd != NULL);

	const char *last_section = "";
	for (int i = 0; i < XEMU_SETTINGS__COUNT; i++) {
		if (strcmp(last_section, config_items[i].section)) {
			fprintf(fd, "[%s]\n", config_items[i].section);
			last_section = config_items[i].section;
		}

		fprintf(fd, "%s = ", config_items[i].name);
		if (config_items[i].type == CONFIG_TYPE_STRING) {
			const char *v;
			xemu_settings_get_string(i, &v);
			fprintf(fd, "%s\n", v);
		} else if (config_items[i].type == CONFIG_TYPE_INT) {
			int v;
			xemu_settings_get_int(i, &v);
			fprintf(fd, "%d\n", v);
		} else if (config_items[i].type == CONFIG_TYPE_BOOL) {
			int v;
			xemu_settings_get_bool(i, &v);
			fprintf(fd, "%s\n", !!(v) ? "true" : "false");
		} else if (config_items[i].type == CONFIG_TYPE_ENUM) {
			int v;
			xemu_settings_get_enum(i, &v);
			const char *str = "";
			xemu_enum_int_to_str(config_items[i].enum_map, v, &str);
			fprintf(fd, "%s\n", str);
		} else {
			// Unimplemented
			assert(0);
		}
	}

	fclose(fd);
	return 0;
}
