/*
 * xemu Settings Management
 *
 * Copyright (C) 2020-2022 Matt Borgerson
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
#include <toml++/toml.h>
#include <cnode.h>
#include <sstream>
#include <iostream>
#include <locale.h>

#include "xemu-settings.h"

#define DEFINE_CONFIG_TREE
#include "xemu-config.h"

struct config g_config;

static const char *filename = "xemu.toml";
static const char *settings_path;
static std::string error_msg;

const char *xemu_settings_get_error_message(void)
{
    return error_msg.length() ? error_msg.c_str() : NULL;
}

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

const char *xemu_settings_get_base_path(void)
{
    static const char *base_path = NULL;
    if (base_path != NULL) {
        return base_path;
    }

    char *base = xemu_settings_detect_portable_mode()
                 ? SDL_GetBasePath()
                 : SDL_GetPrefPath("xemu", "xemu");
    assert(base != NULL);
    base_path = g_strdup(base);
    SDL_free(base);
    fprintf(stderr, "%s: base path: %s\n", __func__, base_path);
    return base_path;
}

const char *xemu_settings_get_path(void)
{
    if (settings_path != NULL) {
        return settings_path;
    }

    const char *base = xemu_settings_get_base_path();
    assert(base != NULL);
    settings_path = g_strdup_printf("%s%s", base, filename);
    fprintf(stderr, "%s: config path: %s\n", __func__, settings_path);
    return settings_path;
}

const char *xemu_settings_get_default_eeprom_path(void)
{
    static char *eeprom_path = NULL;
    if (eeprom_path != NULL) {
        return eeprom_path;
    }

    const char *base = xemu_settings_get_base_path();
    assert(base != NULL);
    eeprom_path = g_strdup_printf("%s%s", base, "eeprom.bin");
    return eeprom_path;
}

static ssize_t get_file_size(FILE *fd)
{
    if (fseek(fd, 0, SEEK_END)) {
        return -1;
    }

    size_t file_size = ftell(fd);

    if (fseek(fd, 0, SEEK_SET)) {
        return -1;
    }

    return file_size;
}

static const char *read_file(FILE *fd)
{
    ssize_t size = get_file_size(fd);
    if (size < 0) {
        return NULL;
    }

    char *buf = (char *)malloc(size + 1);
    if (!buf) {
        return NULL;
    }

    if (size > 0) {
        if (fread(buf, size, 1, fd) != 1) {
            free(buf);
            return NULL;
        }
    }

    buf[size] = '\x00';
    return buf;
}

bool xemu_settings_load(void)
{
    const char *settings_path = xemu_settings_get_path();
    bool success = false;

    if (qemu_access(settings_path, F_OK) == -1) {
        fprintf(stderr, "Config file not found, starting with default settings.\n");
        success = true;
    } else {
        FILE *fd = qemu_fopen(settings_path, "rb");
        if (fd) {
            const char *buf = read_file(fd);
            if (buf) {
                char *previous_numeric_locale = setlocale(LC_NUMERIC, NULL);
                if (previous_numeric_locale) {
                    previous_numeric_locale = g_strdup(previous_numeric_locale);
                }

                /* Ensure numeric values are scanned with '.' radix, no grouping */
                setlocale(LC_NUMERIC, "C");

                try {
                    config_tree.update_from_table(toml::parse(buf));
                    success = true;
                } catch (const toml::parse_error& err) {
                   std::ostringstream oss;
                   oss << "Error parsing config file at " << err.source().begin << ":\n"
                       << "    " << err.description() << "\n"
                       << "Please fix the error or delete the file to continue.\n";
                   error_msg = oss.str();
                }
                free((char*)buf);

                if (previous_numeric_locale) {
                    setlocale(LC_NUMERIC, previous_numeric_locale);
                    g_free(previous_numeric_locale);
                }
            } else {
                error_msg = "Failed to read config file.\n";
            }
            fclose(fd);
        } else {
            error_msg = "Failed to open config file for reading. Check permissions.\n";
        }
    }

    config_tree.store_to_struct(&g_config);

    return success;
}

void xemu_settings_save(void)
{
    FILE *fd = qemu_fopen(xemu_settings_get_path(), "wb");
    if (!fd) {
        fprintf(stderr, "Failed to open config file for writing. Check permissions.\n");
        return;
    }

    char *previous_numeric_locale = setlocale(LC_NUMERIC, NULL);
    if (previous_numeric_locale) {
        previous_numeric_locale = g_strdup(previous_numeric_locale);
    }

    /* Ensure numeric values are printed with '.' radix, no grouping */
    setlocale(LC_NUMERIC, "C");

    config_tree.update_from_struct(&g_config);
    fprintf(fd, "%s", config_tree.generate_delta_toml().c_str());
    fclose(fd);

    if (previous_numeric_locale) {
        setlocale(LC_NUMERIC, previous_numeric_locale);
        g_free(previous_numeric_locale);
    }
}

void add_net_nat_forward_ports(int host, int guest, CONFIG_NET_NAT_FORWARD_PORTS_PROTOCOL protocol)
{
    // FIXME: - Realloc the arrays instead of free/alloc
    //        - Don't need to copy as much
    auto cnode = config_tree.child("net")
                            ->child("nat")
                            ->child("forward_ports");
    cnode->update_from_struct(&g_config);
    cnode->children.push_back(*cnode->array_item_type);
    auto &e = cnode->children.back();
    e.child("host")->set_integer(host);
    e.child("guest")->set_integer(guest);
    e.child("protocol")->set_enum_by_index(protocol);
    cnode->free_allocations(&g_config);
    cnode->store_to_struct(&g_config);
}

void remove_net_nat_forward_ports(unsigned int index)
{
    auto cnode = config_tree.child("net")
                            ->child("nat")
                            ->child("forward_ports");
    cnode->update_from_struct(&g_config);
    cnode->children.erase(cnode->children.begin()+index);
    cnode->free_allocations(&g_config);
    cnode->store_to_struct(&g_config);
}
