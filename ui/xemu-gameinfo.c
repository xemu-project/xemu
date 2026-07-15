/*
 * xemu game info extraction
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
 *
 * Format references:
 *   XDVDFS  https://xboxdevwiki.net/XDVDFS
 *   XBE     http://www.caustik.com/cxbx/download/xbe.htm
 *   XPR0    https://xboxdevwiki.net/XPR
 * The concrete offsets and the XPR0/DXT1/swizzle decode mirror the well-tested
 * logic in the "xDisk" tool (MIT).
 */

#include "qemu/osdep.h"
#include "xemu-gameinfo.h"

#include <glib/gstdio.h> /* g_fopen: not pulled in transitively everywhere */
#include <math.h>

/* CHD support tracks the build system's decision (meson's libchdr feature),
 * not just whatever happens to be on the compiler include path -- otherwise
 * a stray system libchdr could get compiled in without being linked. */
#ifdef CONFIG_LIBCHDR
#define GAMEINFO_HAVE_CHD 1
#include <libchdr/chd.h>
#endif

#define XDVDFS_SECTOR       2048
#define XDVDFS_VOLDESC_OFF  0x10000  // partition base + 32 sectors
#define XDVDFS_MAGIC        "MICROSOFT*XBOX*MEDIA"
#define XDVDFS_MAGIC_LEN    20
#define XBE_MAX_BYTES       (16u * 1024 * 1024)
#define XPR_MAX_BYTES       (4u * 1024 * 1024)

// Known game-partition base offsets found in full/redump images, plus 0 for
// trimmed (game-partition-only) discs. Probed in order; first one carrying a
// default.xbe wins.
static const uint64_t g_partition_bases[] = {
    0x0, 0x18300000ULL, 0xFD90000ULL, 0x89D80000ULL, 0x2080000ULL,
};

// ---------------------------------------------------------------------------
// Bounds-checked little-endian readers over the mapped image
// ---------------------------------------------------------------------------

static inline bool rd_u32(const uint8_t *d, uint64_t len, uint64_t off,
                          uint32_t *v)
{
    if (off + 4 > len) return false;
    *v = (uint32_t)d[off] | ((uint32_t)d[off + 1] << 8) |
         ((uint32_t)d[off + 2] << 16) | ((uint32_t)d[off + 3] << 24);
    return true;
}

// ---------------------------------------------------------------------------
// XDVDFS filesystem
// ---------------------------------------------------------------------------

// Parse one 0x800-byte volume descriptor buffer (read from partition base +
// 32 sectors).
static bool xdvdfs_parse_voldesc(const uint8_t *desc, uint32_t *root_sector,
                                 uint32_t *root_size)
{
    if (memcmp(desc + 0x000, XDVDFS_MAGIC, XDVDFS_MAGIC_LEN) != 0)
        return false;
    if (memcmp(desc + 0x7EC, XDVDFS_MAGIC, XDVDFS_MAGIC_LEN) != 0)
        return false;
    if (!rd_u32(desc, 0x800, 0x14, root_sector)) return false;
    if (!rd_u32(desc, 0x800, 0x18, root_size)) return false;
    return true;
}

// Walk a directory table (an embedded binary tree) held in `table`/`aligned`
// and return the entry matching name (case insensitive). Child links are in
// DWORD units; 0 and 0xFFFF mean "none".
static bool xdvdfs_find_in_table(const uint8_t *table, uint64_t aligned,
                                 const char *name, uint32_t *out_sector,
                                 uint32_t *out_size)
{
    if (aligned == 0) return false;

    // Iterative tree walk with an explicit stack of byte offsets.
    uint32_t max_entries = (uint32_t)(aligned / 4 + 8);
    uint32_t *stack = g_new(uint32_t, max_entries);
    uint32_t sp = 0, visited = 0;
    stack[sp++] = 0;
    bool found = false;

    while (sp > 0 && visited < max_entries) {
        uint32_t off = stack[--sp];
        visited++;
        if ((uint64_t)off + 14 > aligned) continue;

        const uint8_t *e = table + off;
        uint16_t left = (uint16_t)(e[0] | (e[1] << 8));
        uint16_t right = (uint16_t)(e[2] | (e[3] << 8));
        uint32_t sector = (uint32_t)e[4] | ((uint32_t)e[5] << 8) |
                          ((uint32_t)e[6] << 16) | ((uint32_t)e[7] << 24);
        uint32_t size = (uint32_t)e[8] | ((uint32_t)e[9] << 8) |
                        ((uint32_t)e[10] << 16) | ((uint32_t)e[11] << 24);
        uint8_t name_len = e[13];

        if (name_len > 0 && (uint64_t)off + 14 + name_len <= aligned) {
            if ((size_t)name_len == strlen(name) &&
                g_ascii_strncasecmp((const char *)(e + 14), name, name_len) ==
                    0) {
                *out_sector = sector;
                *out_size = size;
                found = true;
                break;
            }
        }

        if (left != 0 && left != 0xFFFF && sp < max_entries)
            stack[sp++] = (uint32_t)left * 4;
        if (right != 0 && right != 0xFFFF && sp < max_entries)
            stack[sp++] = (uint32_t)right * 4;
    }

    g_free(stack);
    return found;
}

// ---------------------------------------------------------------------------
// XPR0 texture decode ($$XTIMAGE title image)
// ---------------------------------------------------------------------------

// Undo the Xbox texture swizzle (Morton / interleaved-bits order).
static uint32_t morton_offset(int x, int y, int w, int h)
{
    uint32_t offset = 0, out_bit = 1, bit = 1;
    while ((int)bit < w || (int)bit < h) {
        if ((int)bit < w) {
            if (x & bit) offset |= out_bit;
            out_bit <<= 1;
        }
        if ((int)bit < h) {
            if (y & bit) offset |= out_bit;
            out_bit <<= 1;
        }
        bit <<= 1;
    }
    return offset;
}

static int square_side(uint64_t pixels)
{
    if (pixels == 0) return 0;
    int side = (int)lround(sqrt((double)pixels));
    if ((uint64_t)side * side == pixels && side >= 16 && side <= 1024)
        return side;
    return 0;
}

static uint8_t *decode_bgra8_swizzled(const uint8_t *p, uint64_t plen, int side)
{
    // Determine whether the source carries meaningful alpha.
    bool has_alpha = false;
    for (uint64_t i = 3; i < plen; i += 4) {
        if (p[i] != 0xFF && p[i] != 0x00) { has_alpha = true; break; }
    }

    uint8_t *rgba = g_malloc0((size_t)side * side * 4);
    for (int y = 0; y < side; y++) {
        for (int x = 0; x < side; x++) {
            uint64_t src = (uint64_t)morton_offset(x, y, side, side) * 4;
            if (src + 3 >= plen) continue;
            uint8_t *o = rgba + ((size_t)y * side + x) * 4;
            o[0] = p[src + 2]; // R
            o[1] = p[src + 1]; // G
            o[2] = p[src + 0]; // B
            o[3] = has_alpha ? p[src + 3] : 0xFF;
        }
    }
    return rgba;
}

static void rgb565(uint16_t v, uint8_t out[4])
{
    out[0] = (uint8_t)((((v >> 11) & 0x1F) * 255) / 31);
    out[1] = (uint8_t)((((v >> 5) & 0x3F) * 255) / 63);
    out[2] = (uint8_t)(((v & 0x1F) * 255) / 31);
    out[3] = 0xFF;
}

static uint8_t *decode_dxt1(const uint8_t *p, uint64_t plen, int side)
{
    uint8_t *rgba = g_malloc0((size_t)side * side * 4);
    uint64_t off = 0;
    for (int by = 0; by < side; by += 4) {
        for (int bx = 0; bx < side; bx += 4) {
            if (off + 8 > plen) return rgba;
            uint16_t c0 = (uint16_t)(p[off] | (p[off + 1] << 8));
            uint16_t c1 = (uint16_t)(p[off + 2] | (p[off + 3] << 8));
            uint32_t bits = (uint32_t)p[off + 4] | ((uint32_t)p[off + 5] << 8) |
                            ((uint32_t)p[off + 6] << 16) |
                            ((uint32_t)p[off + 7] << 24);
            off += 8;

            uint8_t col[4][4];
            rgb565(c0, col[0]);
            rgb565(c1, col[1]);
            if (c0 > c1) {
                for (int k = 0; k < 3; k++) {
                    col[2][k] = (uint8_t)((2 * col[0][k] + col[1][k]) / 3);
                    col[3][k] = (uint8_t)((col[0][k] + 2 * col[1][k]) / 3);
                }
                col[2][3] = col[3][3] = 0xFF;
            } else {
                for (int k = 0; k < 3; k++)
                    col[2][k] = (uint8_t)((col[0][k] + col[1][k]) / 2);
                col[2][3] = 0xFF;
                col[3][0] = col[3][1] = col[3][2] = col[3][3] = 0;
            }

            for (int y = 0; y < 4; y++) {
                for (int x = 0; x < 4; x++) {
                    int idx = (bits >> ((y * 4 + x) * 2)) & 0x3;
                    int tx = bx + x, ty = by + y;
                    if (tx >= side || ty >= side) continue;
                    uint8_t *o = rgba + ((size_t)ty * side + tx) * 4;
                    o[0] = col[idx][0];
                    o[1] = col[idx][1];
                    o[2] = col[idx][2];
                    o[3] = col[idx][3];
                }
            }
        }
    }
    return rgba;
}

// Decode an XPR0 blob into RGBA. Returns malloc'd buffer, or NULL.
static uint8_t *decode_xpr0(const uint8_t *s, uint64_t size, int *out_w,
                            int *out_h)
{
    if (size < 0x20 || memcmp(s, "XPR0", 4) != 0) return NULL;
    uint32_t total, header;
    if (!rd_u32(s, size, 4, &total) || !rd_u32(s, size, 8, &header))
        return NULL;
    if (header < 0x20 || total <= header || total > size) return NULL;

    const uint8_t *payload = s + header;
    uint64_t plen = total - header;

    int side = square_side(plen / 4);
    if (side && (uint64_t)side * side * 4 == plen) {
        *out_w = *out_h = side;
        return decode_bgra8_swizzled(payload, plen, side);
    }

    side = square_side(plen * 2);
    if (side && (uint64_t)side * side / 2 == plen) {
        *out_w = *out_h = side;
        return decode_dxt1(payload, plen, side);
    }

    return NULL;
}

// ---------------------------------------------------------------------------
// XBE parsing
// ---------------------------------------------------------------------------

static void format_title_id(uint32_t id, char out[16])
{
    uint8_t c0 = (id >> 24) & 0xFF;
    uint8_t c1 = (id >> 16) & 0xFF;
    uint16_t num = id & 0xFFFF;
    if (c0 >= 0x20 && c0 < 0x7F && c1 >= 0x20 && c1 < 0x7F) {
        snprintf(out, 16, "%c%c-%04u", c0, c1, num);
    } else {
        snprintf(out, 16, "0x%08X", id);
    }
}

static void clean_title(const char *raw, char out[128])
{
    // Copy, mapping underscores to spaces and collapsing runs of whitespace.
    char tmp[128];
    size_t j = 0;
    bool prev_space = true; // trims leading space
    for (size_t i = 0; raw[i] && j < sizeof(tmp) - 1; i++) {
        char c = raw[i];
        if (c == '_') c = ' ';
        bool is_space = (c == ' ' || c == '\t' || c == '\n' || c == '\r');
        if (is_space) {
            if (prev_space) continue;
            c = ' ';
        }
        tmp[j++] = c;
        prev_space = is_space;
    }
    while (j > 0 && tmp[j - 1] == ' ') j--; // trim trailing
    tmp[j] = '\0';
    g_strlcpy(out, tmp, 128);
}

// Find an XBE section by exact name. On success returns true and fills
// *raw_addr / *raw_size with the section's file-space range, already bounds
// checked so [*raw_addr, *raw_addr + *raw_size) lies within the mapped image.
// Centralises the section-table walk (and its bounds checks) for all callers.
static bool xbe_find_section(const uint8_t *xbe, uint64_t xlen,
                             uint32_t base_addr, const char *name,
                             uint32_t max_size, uint32_t *raw_addr,
                             uint32_t *raw_size)
{
    uint32_t sec_count, sec_headers_va;
    if (!rd_u32(xbe, xlen, 0x11C, &sec_count)) return false;
    if (!rd_u32(xbe, xlen, 0x120, &sec_headers_va)) return false;
    if (sec_count == 0 || sec_count >= 512) return false;
    if (sec_headers_va < base_addr) return false;

    uint64_t headers_off = sec_headers_va - base_addr;
    const uint32_t entry_size = 0x38;
    if (headers_off + (uint64_t)sec_count * entry_size > xlen) return false;

    size_t wlen = strlen(name);
    for (uint32_t i = 0; i < sec_count; i++) {
        uint64_t b = headers_off + (uint64_t)i * entry_size;
        uint32_t ra, rs, name_va;
        if (!rd_u32(xbe, xlen, b + 0x0C, &ra)) continue;
        if (!rd_u32(xbe, xlen, b + 0x10, &rs)) continue;
        if (!rd_u32(xbe, xlen, b + 0x14, &name_va)) continue;
        if (rs == 0 || rs > max_size) continue;
        if (name_va < base_addr) continue;

        uint64_t name_off = name_va - base_addr;
        if (name_off + wlen + 1 > xlen) continue;
        if (memcmp(xbe + name_off, name, wlen) != 0) continue;
        if (xbe[name_off + wlen] != '\0') continue;
        if ((uint64_t)ra + rs > xlen) continue;

        *raw_addr = ra;
        *raw_size = rs;
        return true;
    }
    return false;
}

// Fallback title source: the optional $$XTINFO section holds a small UTF-16
// ini ("[default]\r\nTitleName=..."). Some discs (e.g. NFS Most Wanted PAL)
// leave the certificate title empty but fill this in.
static void xbe_parse_xtinfo_title(const uint8_t *xbe, uint64_t xlen,
                                   uint32_t base_addr, XemuGameInfo *out)
{
    uint32_t raw_addr, raw_size;
    if (!xbe_find_section(xbe, xlen, base_addr, "$$XTINFO", 4096, &raw_addr,
                          &raw_size) ||
        raw_size < 4) {
        return;
    }

    {
        // Decode the whole section as UTF-16LE (unaligned; skip a BOM if
        // present) and look for the TitleName= key.
        long units = raw_size / 2;
        gunichar2 *buf = g_new(gunichar2, units);
        for (long u = 0; u < units; u++) {
            buf[u] = (gunichar2)(xbe[raw_addr + u * 2] |
                                 (xbe[raw_addr + u * 2 + 1] << 8));
        }
        long start = (units > 0 && buf[0] == 0xFEFF) ? 1 : 0;
        char *utf8 = g_utf16_to_utf8(buf + start, units - start,
                                     NULL, NULL, NULL);
        g_free(buf);
        if (!utf8) return;

        const char *key = strstr(utf8, "TitleName=");
        if (key) {
            char val[128];
            key += strlen("TitleName=");
            size_t n = strcspn(key, "\r\n");
            if (n >= sizeof(val)) n = sizeof(val) - 1;
            memcpy(val, key, n);
            val[n] = '\0';
            clean_title(val, out->title);
        }
        g_free(utf8);
        return;
    }
}

static void xbe_parse_title_image(const uint8_t *xbe, uint64_t xlen,
                                  uint32_t base_addr, XemuGameInfo *out)
{
    const char *wanted[] = { "$$XTIMAGE", "$$XSIMAGE" };
    for (int w = 0; w < 2; w++) {
        uint32_t raw_addr, raw_size;
        if (!xbe_find_section(xbe, xlen, base_addr, wanted[w], XPR_MAX_BYTES,
                              &raw_addr, &raw_size)) {
            continue;
        }
        int iw = 0, ih = 0;
        uint8_t *rgba = decode_xpr0(xbe + raw_addr, raw_size, &iw, &ih);
        if (rgba) {
            out->icon_rgba = rgba;
            out->icon_width = iw;
            out->icon_height = ih;
            return;
        }
    }
}

static bool xbe_parse(const uint8_t *xbe, uint64_t xlen, XemuGameInfo *out)
{
    if (xlen < 0x180 || memcmp(xbe, "XBEH", 4) != 0) return false;
    uint32_t base_addr, cert_va;
    if (!rd_u32(xbe, xlen, 0x104, &base_addr)) return false;
    if (!rd_u32(xbe, xlen, 0x118, &cert_va)) return false;
    if (cert_va < base_addr) return false;

    uint64_t cert = cert_va - base_addr;
    if (cert + 0xA8 > xlen) return false;

    if (!rd_u32(xbe, xlen, cert + 0x08, &out->title_id)) return false;
    format_title_id(out->title_id, out->title_id_str);
    rd_u32(xbe, xlen, cert + 0xA0, &out->region);
    rd_u32(xbe, xlen, cert + 0xA4, &out->rating);

    // Title name: up to 40 UTF-16LE units, NUL terminated.
    const uint8_t *np = xbe + cert + 0x0C;
    gunichar2 units[40];
    int n = 0;
    for (; n < 40; n++) {
        uint16_t u = (uint16_t)(np[n * 2] | (np[n * 2 + 1] << 8));
        if (u == 0) break;
        units[n] = u;
    }
    if (n > 0) {
        char *utf8 = g_utf16_to_utf8(units, n, NULL, NULL, NULL);
        if (utf8) {
            clean_title(utf8, out->title);
            g_free(utf8);
        }
    }

    // Certificate title missing: fall back to the $$XTINFO section.
    if (out->title[0] == '\0') {
        xbe_parse_xtinfo_title(xbe, xlen, base_addr, out);
    }

    xbe_parse_title_image(xbe, xlen, base_addr, out);
    return true;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// Round a directory size up to whole sectors, capped to keep allocations
// bounded on corrupt images.
static uint64_t table_span(uint32_t table_size)
{
    uint64_t aligned = ((uint64_t)table_size + XDVDFS_SECTOR - 1) /
                       XDVDFS_SECTOR * XDVDFS_SECTOR;
    return MIN(aligned, (uint64_t)XBE_MAX_BYTES);
}

// Abstract random-access reader over a disc image, so the partition probe
// below works the same for a flat mmap and a decompressed CHD. read() must
// fill buf with n bytes at absolute offset off and return false if that range
// is not fully available.
typedef struct {
    bool (*read)(void *ctx, uint64_t off, uint8_t *buf, uint64_t n);
    uint64_t len;
    void    *ctx;
} ImgReader;

// Probe the known game-partition bases; on the first one that carries a
// default.xbe, parse it into *out. Returns true on success.
static bool probe_and_parse(ImgReader *r, XemuGameInfo *out)
{
    for (size_t i = 0; i < G_N_ELEMENTS(g_partition_bases); i++) {
        uint64_t base = g_partition_bases[i];
        if (base + XDVDFS_VOLDESC_OFF + 0x800 > r->len) continue;

        uint8_t desc[0x800];
        if (!r->read(r->ctx, base + XDVDFS_VOLDESC_OFF, desc, sizeof(desc)))
            continue;
        uint32_t root_sector, root_size;
        if (!xdvdfs_parse_voldesc(desc, &root_sector, &root_size)) continue;
        if (root_size == 0) continue;

        uint64_t table_off = base + (uint64_t)root_sector * XDVDFS_SECTOR;
        if (table_off >= r->len) continue;
        uint64_t aligned = MIN(table_span(root_size), r->len - table_off);

        uint8_t *table = (uint8_t *)g_malloc(aligned);
        uint32_t xbe_sector, xbe_size;
        bool found = r->read(r->ctx, table_off, table, aligned) &&
                     xdvdfs_find_in_table(table, aligned, "default.xbe",
                                          &xbe_sector, &xbe_size);
        g_free(table);
        if (!found) continue;

        uint64_t xbe_off = base + (uint64_t)xbe_sector * XDVDFS_SECTOR;
        if (xbe_off >= r->len || xbe_size == 0) continue;
        uint64_t xlen = MIN((uint64_t)xbe_size, r->len - xbe_off);
        xlen = MIN(xlen, (uint64_t)XBE_MAX_BYTES);

        uint8_t *xbe = (uint8_t *)g_malloc(xlen);
        bool ok = r->read(r->ctx, xbe_off, xbe, xlen) &&
                  xbe_parse(xbe, xlen, out);
        g_free(xbe);
        if (ok) return true;
    }
    return false;
}

// Reader over a contiguous in-memory buffer (the mmap'd flat image).
typedef struct {
    const uint8_t *d;
    uint64_t       len;
} MemImg;

static bool mem_img_read(void *ctx, uint64_t off, uint8_t *buf, uint64_t n)
{
    MemImg *m = (MemImg *)ctx;
    if (off + n < off || off + n > m->len) return false; // overflow / OOB
    memcpy(buf, m->d + off, n);
    return true;
}

#ifdef GAMEINFO_HAVE_CHD

// Minimal CHD reader: one decompressed-hunk cache, byte-addressed reads over
// the logical (decompressed) image. Mirrors block/chd.c.
typedef struct ChdImg {
    chd_file         *chd;
    FILE             *file;    // owned; chd_open_file() does not take it
    const chd_header *hdr;
    uint8_t          *hunk;
    uint32_t          cur_hunk;
    bool              hunk_valid;
    uint64_t          len;      // logical image size in bytes
} ChdImg;

static bool chd_uses_cd_codec(const chd_header *header)
{
    for (size_t i = 0; i < ARRAY_SIZE(header->compression); i++) {
        switch (header->compression[i]) {
        case CHD_CODEC_CD_ZLIB:
        case CHD_CODEC_CD_LZMA:
        case CHD_CODEC_CD_FLAC:
        case CHD_CODEC_CD_ZSTD:
            return true;
        default:
            break;
        }
    }
    return false;
}

static void chd_img_close(ChdImg *img)
{
    g_free(img->hunk);
    if (img->chd) chd_close(img->chd);
    if (img->file) fclose(img->file);
    memset(img, 0, sizeof(*img));
}

static bool chd_img_open(const char *path, ChdImg *img)
{
    memset(img, 0, sizeof(*img));
    /* Open via g_fopen: paths are UTF-8 throughout xemu, which plain
     * fopen (used by chd_open) would misinterpret on Windows. */
    img->file = g_fopen(path, "rb");
    if (!img->file)
        return false;
    if (chd_open_file(img->file, CHD_OPEN_READ, NULL, &img->chd) !=
        CHDERR_NONE) {
        fclose(img->file);
        img->file = NULL;
        return false;
    }
    img->hdr = chd_get_header(img->chd);
    // Xbox discs are DVDs; CD-codec CHDs carry per-frame subcode data the
    // plain byte mapping below would misread.
    if (!img->hdr || img->hdr->hunkbytes == 0 ||
        img->hdr->logicalbytes == 0 || chd_uses_cd_codec(img->hdr)) {
        chd_img_close(img);
        return false;
    }
    img->hunk = (uint8_t *)g_malloc(img->hdr->hunkbytes);
    img->len = img->hdr->logicalbytes;
    return true;
}

static bool chd_img_read(ChdImg *img, uint64_t offset, uint8_t *buf,
                         uint64_t bytes)
{
    uint64_t done = 0;
    while (done < bytes) {
        uint64_t pos = offset + done;
        if (pos < offset || pos >= img->len) return false; // overflow / EOF
        uint64_t hunk = pos / img->hdr->hunkbytes;
        if (hunk > UINT32_MAX) return false;

        if (!img->hunk_valid || img->cur_hunk != (uint32_t)hunk) {
            if (chd_read(img->chd, (uint32_t)hunk, img->hunk) != CHDERR_NONE)
                return false;
            img->cur_hunk = (uint32_t)hunk;
            img->hunk_valid = true;
        }

        uint64_t hunk_off = pos % img->hdr->hunkbytes;
        uint64_t copy = MIN(bytes - done,
                            (uint64_t)img->hdr->hunkbytes - hunk_off);
        copy = MIN(copy, img->len - pos);
        memcpy(buf + done, img->hunk + hunk_off, copy);
        done += copy;
    }
    return true;
}

static bool chd_img_read_cb(void *ctx, uint64_t off, uint8_t *buf, uint64_t n)
{
    return chd_img_read((ChdImg *)ctx, off, buf, n);
}

static bool extract_from_chd(const char *path, XemuGameInfo *out)
{
    ChdImg img;
    if (!chd_img_open(path, &img)) return false;

    ImgReader r = { chd_img_read_cb, img.len, &img };
    bool ok = probe_and_parse(&r, out);

    chd_img_close(&img);
    return ok;
}

#endif /* GAMEINFO_HAVE_CHD */

bool xemu_gameinfo_chd_supported(void)
{
#ifdef GAMEINFO_HAVE_CHD
    return true;
#else
    return false;
#endif
}

bool xemu_gameinfo_extract(const char *iso_path, XemuGameInfo *out)
{
    memset(out, 0, sizeof(*out));
    if (!iso_path || !iso_path[0]) return false;

    const char *dot = strrchr(iso_path, '.');
#ifdef GAMEINFO_HAVE_CHD
    if (dot && g_ascii_strcasecmp(dot, ".chd") == 0) {
        bool ok = extract_from_chd(iso_path, out);
        if (!ok) memset(out, 0, sizeof(*out));
        return ok;
    }
#else
    if (dot && g_ascii_strcasecmp(dot, ".chd") == 0) return false;
#endif

    GMappedFile *map = g_mapped_file_new(iso_path, FALSE, NULL);
    if (!map) return false;

    const uint8_t *d = (const uint8_t *)g_mapped_file_get_contents(map);
    uint64_t len = (uint64_t)g_mapped_file_get_length(map);
    if (!d || len < XDVDFS_VOLDESC_OFF + 0x800) {
        g_mapped_file_unref(map);
        return false;
    }

    MemImg mem = { d, len };
    ImgReader r = { mem_img_read, len, &mem };
    bool ok = probe_and_parse(&r, out);

    g_mapped_file_unref(map);
    if (!ok) memset(out, 0, sizeof(*out));
    return ok;
}

void xemu_gameinfo_free(XemuGameInfo *info)
{
    if (!info) return;
    g_free(info->icon_rgba);
    info->icon_rgba = NULL;
    info->icon_width = info->icon_height = 0;
}
