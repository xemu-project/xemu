//
// xemu Redump ISO support
//
// Copyright (C) 2026 JBW89
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
#ifndef SYSTEM_XEMU_REDUMP_H
#define SYSTEM_XEMU_REDUMP_H

bool xemu_redump_detect_game_partition(const char *path, uint64_t *offset);

#endif /* SYSTEM_XEMU_REDUMP_H */
