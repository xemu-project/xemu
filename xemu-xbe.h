/*
 * xemu XBE accessing
 *
 * Helper functions to get details about the currently running executable.
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

#ifndef XEMU_XBE_H
#define XEMU_XBE_H

#include <stdint.h>

struct xbe_info {
    uint32_t timedate;
    uint32_t cert_timedate;
    uint32_t cert_title_id;
    char     cert_title_id_str[12];
    uint32_t cert_version;
};

#ifdef __cplusplus
extern "C" {
#endif

// Get current XBE info
struct xbe_info *xemu_get_xbe_info(void);

#ifdef __cplusplus
}
#endif

#endif
