/*
 * xemu Data File and Path Helpers
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

#ifndef XEMU_DATA
#define XEMU_DATA

#ifdef __cplusplus
extern "C" {
#endif

// Note: Not thread safe. Returns a pointer to an internally allocated buffer.
const char *xemu_get_resource_path(const char *filename);

#ifdef __cplusplus
}
#endif

#endif
