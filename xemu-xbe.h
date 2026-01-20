/*
 * xemu XBE accessing
 *
 * Helper functions to get details about the currently running executable.
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

#ifndef XEMU_XBE_H
#define XEMU_XBE_H

#include <stdint.h>

// http://www.caustik.com/cxbx/download/xbe.htm
#pragma pack(1)
struct xbe_header
{
    uint32_t m_magic;                         // magic number [should be "XBEH"]
    uint8_t  m_digsig[256];                   // digital signature
    uint32_t m_base;                          // base address
    uint32_t m_sizeof_headers;                // size of headers
    uint32_t m_sizeof_image;                  // size of image
    uint32_t m_sizeof_image_header;           // size of image header
    uint32_t m_timedate;                      // timedate stamp
    uint32_t m_certificate_addr;              // certificate address
    uint32_t m_sections;                      // number of sections
    uint32_t m_section_headers_addr;          // section headers address

    struct init_flags
    {
        uint32_t m_mount_utility_drive    : 1;  // mount utility drive flag
        uint32_t m_format_utility_drive   : 1;  // format utility drive flag
        uint32_t m_limit_64mb             : 1;  // limit development kit run time memory to 64mb flag
        uint32_t m_dont_setup_harddisk    : 1;  // don't setup hard disk flag
        uint32_t m_unused                 : 4;  // unused (or unknown)
        uint32_t m_unused_b1              : 8;  // unused (or unknown)
        uint32_t m_unused_b2              : 8;  // unused (or unknown)
        uint32_t m_unused_b3              : 8;  // unused (or unknown)
    } m_init_flags;

    uint32_t m_entry;                         // entry point address
    uint32_t m_tls_addr;                      // thread local storage directory address
    uint32_t m_pe_stack_commit;               // size of stack commit
    uint32_t m_pe_heap_reserve;               // size of heap reserve
    uint32_t m_pe_heap_commit;                // size of heap commit
    uint32_t m_pe_base_addr;                  // original base address
    uint32_t m_pe_sizeof_image;               // size of original image
    uint32_t m_pe_checksum;                   // original checksum
    uint32_t m_pe_timedate;                   // original timedate stamp
    uint32_t m_debug_pathname_addr;           // debug pathname address
    uint32_t m_debug_filename_addr;           // debug filename address
    uint32_t m_debug_unicode_filename_addr;   // debug unicode filename address
    uint32_t m_kernel_image_thunk_addr;       // kernel image thunk address
    uint32_t m_nonkernel_import_dir_addr;     // non kernel import directory address
    uint32_t m_library_versions;              // number of library versions
    uint32_t m_library_versions_addr;         // library versions address
    uint32_t m_kernel_library_version_addr;   // kernel library version address
    uint32_t m_xapi_library_version_addr;     // xapi library version address
    uint32_t m_logo_bitmap_addr;              // logo bitmap address
    uint32_t m_logo_bitmap_size;              // logo bitmap size
};

struct xbe_certificate
{
    uint32_t m_size;                          // size of certificate
    uint32_t m_timedate;                      // timedate stamp
    uint32_t m_titleid;                       // title id
    uint16_t m_title_name[40];                // title name (unicode)
    uint32_t m_alt_title_id[0x10];            // alternate title ids
    uint32_t m_allowed_media;                 // allowed media types
    uint32_t m_game_region;                   // game region
    uint32_t m_game_ratings;                  // game ratings
    uint32_t m_disk_number;                   // disk number
    uint32_t m_version;                       // version
    uint8_t  m_lan_key[16];                   // lan key
    uint8_t  m_sig_key[16];                   // signature key
    uint8_t  m_title_alt_sig_key[16][16];     // alternate signature keys
};
#pragma pack()

struct xbe {
	// Full XBE headers, copied into an allocated buffer
	uint8_t *headers;
	uint32_t headers_len;

	// Pointers into `headers` (note: little-endian!)
	struct xbe_header *header;
	struct xbe_certificate *cert;
};

#ifdef __cplusplus
extern "C" {
#endif

// Get current XBE info
struct xbe *xemu_get_xbe_info(void);

#ifdef __cplusplus
}
#endif

#endif
