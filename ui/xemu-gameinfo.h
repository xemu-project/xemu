/*
 * xemu game info extraction
 *
 * Reads metadata (title name, title id, region and the embedded title image)
 * out of an Xbox disc image (.iso / .xiso) WITHOUT booting it. The XDVDFS
 * filesystem is parsed directly off disk to locate default.xbe, whose
 * certificate provides the title/id/region and whose $$XTIMAGE section holds
 * the dashboard title image (an XPR0-packed texture) used as cover art.
 *
 * Copyright (C) 2024 xemu contributors
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

#ifndef XEMU_GAMEINFO_H
#define XEMU_GAMEINFO_H

#include <stdint.h>
#include <stdbool.h>

typedef struct XemuGameInfo {
    char     title[128];        // UTF-8 title name, "" if unavailable
    uint32_t title_id;          // raw title id (0 if unavailable)
    char     title_id_str[16];  // formatted, e.g. "SC-0250"
    uint32_t region;            // raw XBE region bitfield
    uint32_t rating;            // raw XBE game-ratings bitfield

    // Decoded title image, RGBA8888 top-left origin. NULL when the disc has no
    // embedded title image. Owned by the struct; release with
    // xemu_gameinfo_free().
    uint8_t *icon_rgba;
    int      icon_width;
    int      icon_height;
} XemuGameInfo;

#ifdef __cplusplus
extern "C" {
#endif

// Extract metadata from the disc image at iso_path (.iso/.xiso, or .chd when
// built with libchdr). Returns true if a default.xbe was located and its
// certificate parsed (icon may still be NULL). On failure returns false and
// leaves *out zeroed.
bool xemu_gameinfo_extract(const char *iso_path, XemuGameInfo *out);

// Whether .chd disc images can be parsed by this build.
bool xemu_gameinfo_chd_supported(void);

// Release the icon buffer owned by info and zero it.
void xemu_gameinfo_free(XemuGameInfo *info);

#ifdef __cplusplus
}
#endif

#endif
