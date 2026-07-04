/*
 * xemu temporary XBE patch support
 *
 * Copyright (c) 2026 JBW89
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * later.  See the COPYING file in the top-level directory.
 */
#include "qemu/osdep.h"
#include "qemu/iov.h"
#include "system/xemu-xbe-patch.h"
#include "system/block-backend.h"
#include "qemu/cutils.h"
#include "qemu/error-report.h"
#include "ui/xemu-settings.h"
#include "system/block-backend-global-state.h"
#include "xemu-xbe.h"

/*
 * This module applies user-supplied byte patches to the XBE executables on a
 * loaded Xbox disc "on the fly": the disc image itself is never modified.
 * When a disc is inserted, prepare() locates default.xbe in the XDVDFS
 * filesystem, hashes it to pick a matching patch profile, reads the target
 * XBE(s) into memory and applies the profile's patches to those in-memory
 * copies. Every guest read of the CD backend then has the patched bytes
 * spliced into the returned buffer by apply_read(), so the guest sees the
 * patched executable while the on-disk data stays untouched.
 *
 * XDVDFS constants below: 2048-byte sectors, a volume descriptor 32 sectors
 * in carrying the magic string and root directory location. All disc access
 * here assumes the filesystem starts at block-backend offset 0 (a plain xiso,
 * or an image already presented at the game-partition offset).
 */
#define XEMU_XDVDFS_SECTOR_SIZE 2048
#define XEMU_XDVDFS_VOLUME_DESCRIPTOR_SECTOR 32
#define XEMU_XDVDFS_MAGIC_OFFSET 0x7ec
#define XEMU_XDVDFS_ROOT_SECTOR_OFFSET 0x14
#define XEMU_XDVDFS_ROOT_SIZE_OFFSET 0x18
#define XEMU_XBE_PATCH_MAX_XBE_SIZE (64 * 1024 * 1024)
#define XEMU_XBE_PATCH_MAX_TREE_DEPTH 1024
#define XEMU_XBE_MAGIC "XBEH"

static const char xemu_xdvdfs_magic[] = "MICROSOFT*XBOX*MEDIA";

/*
 * Shared state guarded by xemu_xbe_patch_mutex. generation is bumped on every
 * prepare()/reset(); a prepare() runs its slow disc I/O without the lock and
 * only commits its result if the generation still matches, so a disc change
 * that races with an in-flight prepare discards the stale result instead of
 * publishing patches for the wrong disc. preparing suppresses patching while
 * prepare() is itself reading the disc.
 */
typedef struct XemuXbePatchState {
    BlockBackend *blk;
    GPtrArray *patched_xbes;
    char *current_hash;
    char *current_title_id;
    char *current_title_name;
    char *current_region;
    char *status;
    uint64_t generation;
    bool preparing;
} XemuXbePatchState;

/* One target XBE read from the disc, with patches already applied to data.
 * offset/size are in block-backend byte coordinates so apply_read() can splice
 * data back into overlapping guest reads. */
typedef struct XemuXbePatchFile {
    char *name;
    uint64_t offset;
    size_t size;
    uint8_t *data;
} XemuXbePatchFile;

static XemuXbePatchState xemu_xbe_patch_state;
static GMutex xemu_xbe_patch_mutex;

/*
 * Lock-free fast-path hint for apply_read(), which runs on every block-backend
 * read of every device. It mirrors "patched_xbes is non-NULL" and is written
 * only under the mutex. apply_read() may read a stale value in either
 * direction without harm: a stale 0 at most skips patching for a read that
 * races the disc-insert commit (before the guest boots), and a stale 1 just
 * takes the lock and re-checks the authoritative state. Correctness always
 * comes from the re-check under the mutex; this only avoids the lock in the
 * common case where no patches are active.
 */
static int xemu_xbe_patch_active;

static uint32_t xemu_xbe_patch_ldl_le(const uint8_t *p)
{
    return p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t xemu_xbe_patch_lduw_le(const uint8_t *p)
{
    return p[0] | ((uint16_t)p[1] << 8);
}

static void xemu_xbe_patch_file_free(gpointer opaque)
{
    XemuXbePatchFile *file = opaque;

    if (!file) {
        return;
    }

    g_free(file->name);
    g_free(file->data);
    g_free(file);
}

static void xemu_xbe_patch_file_array_free(GPtrArray *files)
{
    if (files) {
        g_ptr_array_unref(files);
    }
}

static void xemu_xbe_patch_clear_state_locked(void)
{
    xemu_xbe_patch_state.blk = NULL;
    g_clear_pointer(&xemu_xbe_patch_state.patched_xbes,
                    xemu_xbe_patch_file_array_free);
    g_clear_pointer(&xemu_xbe_patch_state.current_hash, g_free);
    g_clear_pointer(&xemu_xbe_patch_state.current_title_id, g_free);
    g_clear_pointer(&xemu_xbe_patch_state.current_title_name, g_free);
    g_clear_pointer(&xemu_xbe_patch_state.current_region, g_free);
    g_clear_pointer(&xemu_xbe_patch_state.status, g_free);
    qatomic_set(&xemu_xbe_patch_active, 0);
}

static void xemu_xbe_patch_free_commit_values(GPtrArray *patched_xbes,
                                              char *current_hash,
                                              char *current_title_id,
                                              char *current_title_name,
                                              char *current_region,
                                              char *status)
{
    xemu_xbe_patch_file_array_free(patched_xbes);
    g_free(current_hash);
    g_free(current_title_id);
    g_free(current_title_name);
    g_free(current_region);
    g_free(status);
}

static void xemu_xbe_patch_commit(uint64_t generation, BlockBackend *blk,
                                  GPtrArray *patched_xbes,
                                  char *current_hash,
                                  char *current_title_id,
                                  char *current_title_name,
                                  char *current_region, char *status)
{
    g_mutex_lock(&xemu_xbe_patch_mutex);
    if (xemu_xbe_patch_state.generation != generation) {
        g_mutex_unlock(&xemu_xbe_patch_mutex);
        xemu_xbe_patch_free_commit_values(patched_xbes, current_hash,
                                          current_title_id,
                                          current_title_name,
                                          current_region, status);
        return;
    }

    xemu_xbe_patch_clear_state_locked();
    xemu_xbe_patch_state.blk = blk;
    xemu_xbe_patch_state.patched_xbes = patched_xbes;
    xemu_xbe_patch_state.current_hash = current_hash;
    xemu_xbe_patch_state.current_title_id = current_title_id;
    xemu_xbe_patch_state.current_title_name = current_title_name;
    xemu_xbe_patch_state.current_region = current_region;
    xemu_xbe_patch_state.status = status;
    xemu_xbe_patch_state.preparing = false;
    /* Enable the apply_read() fast path only when there is something to splice.
     * Written last, under the mutex; cleared by clear_state_locked() above. */
    qatomic_set(&xemu_xbe_patch_active, patched_xbes != NULL);
    g_mutex_unlock(&xemu_xbe_patch_mutex);
}

char *xemu_xbe_patch_dup_status(void)
{
    char *copy;

    g_mutex_lock(&xemu_xbe_patch_mutex);
    copy = g_strdup(xemu_xbe_patch_state.status ?
                    xemu_xbe_patch_state.status :
                    "XBE patches are not active.");
    g_mutex_unlock(&xemu_xbe_patch_mutex);

    return copy;
}

char *xemu_xbe_patch_dup_current_hash(void)
{
    char *copy;

    g_mutex_lock(&xemu_xbe_patch_mutex);
    copy = g_strdup(xemu_xbe_patch_state.current_hash);
    g_mutex_unlock(&xemu_xbe_patch_mutex);

    return copy;
}

char *xemu_xbe_patch_dup_current_title_id(void)
{
    char *copy;

    g_mutex_lock(&xemu_xbe_patch_mutex);
    copy = g_strdup(xemu_xbe_patch_state.current_title_id);
    g_mutex_unlock(&xemu_xbe_patch_mutex);

    return copy;
}

char *xemu_xbe_patch_dup_current_title_name(void)
{
    char *copy;

    g_mutex_lock(&xemu_xbe_patch_mutex);
    copy = g_strdup(xemu_xbe_patch_state.current_title_name);
    g_mutex_unlock(&xemu_xbe_patch_mutex);

    return copy;
}

char *xemu_xbe_patch_dup_current_region(void)
{
    char *copy;

    g_mutex_lock(&xemu_xbe_patch_mutex);
    copy = g_strdup(xemu_xbe_patch_state.current_region);
    g_mutex_unlock(&xemu_xbe_patch_mutex);

    return copy;
}

static bool xemu_xbe_patch_is_cdrom(BlockBackend *blk)
{
    const char *name;

    if (!blk) {
        return false;
    }

    name = blk_name(blk);
    return name && strcmp(name, "ide0-cd1") == 0;
}

static int xemu_xbe_patch_profile_index(const char *xbe_hash)
{
    if (!xbe_hash || !xbe_hash[0]) {
        return -1;
    }

    for (unsigned int i = 0; i < g_config.sys.xbe_patches.profiles_count;
         i++) {
        const char *candidate = g_config.sys.xbe_patches.profiles[i].xbe_hash;
        if (candidate && !strcmp(candidate, xbe_hash)) {
            return i;
        }
    }

    return -1;
}

static const char *xemu_xbe_patch_path_for_index(int profile_index, int index)
{
    if (profile_index < 0 ||
        (unsigned int)profile_index >=
        g_config.sys.xbe_patches.profiles_count) {
        return NULL;
    }

    switch (index) {
    case 0:
        return g_config.sys.xbe_patches.profiles[profile_index].patch_1;
    case 1:
        return g_config.sys.xbe_patches.profiles[profile_index].patch_2;
    case 2:
        return g_config.sys.xbe_patches.profiles[profile_index].patch_3;
    case 3:
        return g_config.sys.xbe_patches.profiles[profile_index].patch_4;
    case 4:
        return g_config.sys.xbe_patches.profiles[profile_index].patch_5;
    default:
        return NULL;
    }
}

static int xemu_xbe_patch_selected_count(int profile_index)
{
    int count = 0;

    for (int i = 0; i < XEMU_XBE_PATCH_MAX_FILES; i++) {
        const char *path = xemu_xbe_patch_path_for_index(profile_index, i);
        if (path && path[0]) {
            count++;
        }
    }

    return count;
}

static bool xemu_xbe_patch_string_array_contains(GPtrArray *array,
                                                 const char *value)
{
    for (unsigned int i = 0; i < array->len; i++) {
        const char *candidate = g_ptr_array_index(array, i);
        if (candidate && !g_ascii_strcasecmp(candidate, value)) {
            return true;
        }
    }

    return false;
}

static void xemu_xbe_patch_add_target_name(GPtrArray *targets,
                                           const char *target_name)
{
    g_autofree char *lower_name = NULL;

    if (!target_name || !target_name[0]) {
        return;
    }

    lower_name = g_ascii_strdown(target_name, -1);
    if (!xemu_xbe_patch_string_array_contains(targets, lower_name)) {
        g_ptr_array_add(targets, g_steal_pointer(&lower_name));
    }
}

static bool xemu_xbe_patch_comment_is_global(const char *line)
{
    const char *p;

    if (!line || line[0] != '#') {
        return false;
    }

    p = line + 1;
    while (g_ascii_isspace(*p)) {
        p++;
    }

    return !g_ascii_strncasecmp(p, "global", strlen("global")) &&
           (!p[strlen("global")] ||
            g_ascii_isspace(p[strlen("global")]) ||
            p[strlen("global")] == '-');
}

static char *xemu_xbe_patch_comment_target(const char *line)
{
    const char *p;
    const char *start;
    const char *end;
    size_t target_len;
    g_autofree char *target = NULL;
    bool quoted = false;

    if (!line || line[0] != '#') {
        return NULL;
    }

    p = line + 1;
    while (g_ascii_isspace(*p)) {
        p++;
    }

    if (!*p) {
        return NULL;
    }

    if (*p == '"') {
        quoted = true;
        p++;
    }

    start = p;
    while (*p && !g_ascii_isspace(*p) && *p != '-' &&
           (!quoted || *p != '"')) {
        p++;
    }
    end = p;
    target_len = end - start;

    if (!target_len || target_len < strlen(".xbe") ||
        g_ascii_strncasecmp(end - strlen(".xbe"), ".xbe",
                            strlen(".xbe"))) {
        return NULL;
    }

    if (quoted) {
        if (*p != '"') {
            return NULL;
        }
        p++;
    }

    while (g_ascii_isspace(*p)) {
        p++;
    }
    if (*p && *p != '-') {
        return NULL;
    }

    target = g_strndup(start, target_len);
    return g_ascii_strdown(target, -1);
}

static bool xemu_xbe_patch_collect_file_targets(const char *path,
                                                GPtrArray *targets,
                                                char **error)
{
    g_autofree char *contents = NULL;
    g_auto(GStrv) lines = NULL;
    gsize contents_len = 0;
    bool found_target = false;
    GError *err = NULL;

    if (!g_file_get_contents(path, &contents, &contents_len, &err)) {
        *error = g_strdup_printf("Could not load XBE patch '%s': %s",
                                 path, err->message);
        g_error_free(err);
        return false;
    }

    lines = g_strsplit(contents, "\n", -1);
    for (int i = 0; lines[i]; i++) {
        char *line = g_strstrip(lines[i]);
        g_autofree char *target = NULL;

        if (i == 0 && g_str_has_prefix(line, "\xef\xbb\xbf")) {
            line += 3;
        }

        target = xemu_xbe_patch_comment_target(line);
        if (target) {
            xemu_xbe_patch_add_target_name(targets, target);
            found_target = true;
        }
    }

    if (!found_target) {
        xemu_xbe_patch_add_target_name(targets, "default.xbe");
    }

    return true;
}

static XemuXbePatchFile *xemu_xbe_patch_file_by_name(GPtrArray *files,
                                                     const char *name)
{
    for (unsigned int i = 0; files && i < files->len; i++) {
        XemuXbePatchFile *file = g_ptr_array_index(files, i);
        if (file->name && !g_ascii_strcasecmp(file->name, name)) {
            return file;
        }
    }

    return NULL;
}

void xemu_xbe_patch_reset(BlockBackend *blk)
{
    g_mutex_lock(&xemu_xbe_patch_mutex);
    if (blk && xemu_xbe_patch_state.blk && xemu_xbe_patch_state.blk != blk) {
        g_mutex_unlock(&xemu_xbe_patch_mutex);
        return;
    }

    xemu_xbe_patch_state.generation++;
    xemu_xbe_patch_clear_state_locked();
    xemu_xbe_patch_state.preparing = false;
    g_mutex_unlock(&xemu_xbe_patch_mutex);
}

static bool xemu_xbe_patch_read(BlockBackend *blk, uint64_t offset,
                                void *buf, size_t bytes)
{
    if (offset > INT64_MAX || bytes > INT64_MAX ||
        offset > INT64_MAX - bytes) {
        return false;
    }

    return blk_pread(blk, (int64_t)offset, (int64_t)bytes, buf, 0) == 0;
}

/*
 * Recursively search the XDVDFS directory binary tree for target_name. The
 * left/right child offsets come from untrusted disc data, so the tree may be
 * cyclic or converging. Three guards keep the search finite and in-bounds:
 * a per-position visited[] byte array (each entry expanded at most once, so
 * total work is linear in root_size), a recursion depth cap, and an explicit
 * self-loop check. Every field read is bounds-checked against root_size.
 */
static bool xemu_xbe_patch_find_xbe_entry(const uint8_t *root_dir,
                                          uint32_t root_size,
                                          uint32_t pos,
                                          const char *target_name,
                                          uint64_t *offset,
                                          size_t *size,
                                          uint8_t *visited,
                                          unsigned int depth)
{
    uint32_t left_pos;
    uint32_t right_pos;
    uint32_t entry_len;
    uint32_t file_sector;
    uint32_t file_size;
    uint8_t name_len;

    if (depth > XEMU_XBE_PATCH_MAX_TREE_DEPTH ||
        pos >= root_size || root_size - pos < 14 || visited[pos]) {
        return false;
    }
    visited[pos] = 1;

    name_len = root_dir[pos + 13];
    entry_len = (14 + name_len + 3) & ~3;
    if (!name_len || entry_len > root_size - pos) {
        return false;
    }

    left_pos = xemu_xbe_patch_lduw_le(root_dir + pos);
    right_pos = xemu_xbe_patch_lduw_le(root_dir + pos + 2);
    if (left_pos > UINT32_MAX / 4 || right_pos > UINT32_MAX / 4) {
        return false;
    }
    left_pos *= 4;
    right_pos *= 4;

    if (left_pos &&
        left_pos != pos &&
        xemu_xbe_patch_find_xbe_entry(root_dir, root_size, left_pos,
                                      target_name, offset, size, visited,
                                      depth + 1)) {
        return true;
    }

    if (name_len == strlen(target_name) &&
        !g_ascii_strncasecmp((const char *)&root_dir[pos + 14],
                             target_name, name_len)) {
        file_sector = xemu_xbe_patch_ldl_le(root_dir + pos + 4);
        file_size = xemu_xbe_patch_ldl_le(root_dir + pos + 8);
        if (file_sector && file_size &&
            file_size <= XEMU_XBE_PATCH_MAX_XBE_SIZE) {
            *offset = (uint64_t)file_sector * XEMU_XDVDFS_SECTOR_SIZE;
            *size = file_size;
            return true;
        }
    }

    if (right_pos &&
        right_pos != pos &&
        xemu_xbe_patch_find_xbe_entry(root_dir, root_size, right_pos,
                                      target_name, offset, size, visited,
                                      depth + 1)) {
        return true;
    }

    return false;
}

static bool xemu_xbe_patch_find_xbe(BlockBackend *blk, const char *target_name,
                                    uint64_t *offset, size_t *size)
{
    uint8_t sector[XEMU_XDVDFS_SECTOR_SIZE];
    uint8_t *root_dir = NULL;
    uint64_t volume_offset;
    uint32_t root_sector;
    uint32_t root_size;
    uint8_t *visited = NULL;
    bool found = false;

    volume_offset = (uint64_t)XEMU_XDVDFS_VOLUME_DESCRIPTOR_SECTOR *
                    XEMU_XDVDFS_SECTOR_SIZE;

    if (!xemu_xbe_patch_read(blk, volume_offset, sector, sizeof(sector))) {
        return false;
    }

    if (memcmp(sector, xemu_xdvdfs_magic, sizeof(xemu_xdvdfs_magic) - 1) ||
        memcmp(sector + XEMU_XDVDFS_MAGIC_OFFSET, xemu_xdvdfs_magic,
               sizeof(xemu_xdvdfs_magic) - 1)) {
        return false;
    }

    root_sector = xemu_xbe_patch_ldl_le(sector +
                                        XEMU_XDVDFS_ROOT_SECTOR_OFFSET);
    root_size = xemu_xbe_patch_ldl_le(sector + XEMU_XDVDFS_ROOT_SIZE_OFFSET);
    if (!root_sector || !root_size || root_size > 1024 * 1024) {
        return false;
    }

    root_dir = g_try_malloc(root_size);
    if (!root_dir) {
        return false;
    }

    if (!xemu_xbe_patch_read(blk,
                             (uint64_t)root_sector *
                             XEMU_XDVDFS_SECTOR_SIZE,
                             root_dir, root_size)) {
        goto out;
    }

    /*
     * Preferred path: walk the directory as a binary tree. If the visited[]
     * allocation fails we simply skip it and fall through to the linear scan
     * below, which enumerates the packed entries sequentially and is bounded
     * by root_size. Either path locates target_name safely.
     */
    visited = g_try_malloc0(root_size);
    if (visited) {
        found = xemu_xbe_patch_find_xbe_entry(root_dir, root_size, 0,
                                              target_name, offset, size,
                                              visited, 0);
    }

    for (uint32_t pos = 0; !found && pos < root_size;) {
        uint8_t name_len;
        uint32_t entry_len;
        uint32_t file_sector;
        uint32_t file_size;

        if (root_size - pos < 14) {
            break;
        }

        name_len = root_dir[pos + 13];
        entry_len = 14 + name_len;
        entry_len = (entry_len + 3) & ~3;
        if (!name_len || entry_len > root_size - pos) {
            pos += (root_size - pos < 4) ? root_size - pos : 4;
            continue;
        }

        if (name_len == strlen(target_name) &&
            !g_ascii_strncasecmp((const char *)&root_dir[pos + 14],
                                 target_name, name_len)) {
            file_sector = xemu_xbe_patch_ldl_le(root_dir + pos + 4);
            file_size = xemu_xbe_patch_ldl_le(root_dir + pos + 8);
            if (file_sector && file_size &&
                file_size <= XEMU_XBE_PATCH_MAX_XBE_SIZE) {
                *offset = (uint64_t)file_sector * XEMU_XDVDFS_SECTOR_SIZE;
                *size = file_size;
                found = true;
                break;
            }
        }

        pos += entry_len;
    }

out:
    g_free(visited);
    g_free(root_dir);
    return found;
}

void xemu_xbe_patch_refresh_current(void)
{
    BlockBackend *blk = blk_by_name("ide0-cd1");

    if (blk) {
        xemu_xbe_patch_prepare(blk);
    }
}

/*
 * Resolve the file offset of the XBE certificate from the header's virtual
 * addresses. The certificate address is a virtual address; subtracting the
 * image base gives a file offset. Everything is validated against xbe_size so
 * the returned offset leaves room for a full xbe_certificate, letting callers
 * read fixed certificate fields without further bounds checks.
 */
static bool xemu_xbe_patch_certificate_offset(const uint8_t *xbe,
                                              size_t xbe_size,
                                              size_t *cert_offset)
{
    uint32_t base;
    uint32_t cert_addr;
    uint64_t offset;

    if (xbe_size < sizeof(struct xbe_header)) {
        return false;
    }

    base = xemu_xbe_patch_ldl_le(xbe + offsetof(struct xbe_header, m_base));
    cert_addr = xemu_xbe_patch_ldl_le(
        xbe + offsetof(struct xbe_header, m_certificate_addr));
    if (!base || !cert_addr || cert_addr < base) {
        return false;
    }

    offset = (uint64_t)cert_addr - base;
    if (offset > xbe_size ||
        xbe_size - offset < sizeof(struct xbe_certificate)) {
        return false;
    }

    *cert_offset = offset;
    return true;
}

static char *xemu_xbe_patch_extract_title_id(const uint8_t *xbe,
                                             size_t xbe_size)
{
    size_t cert_offset;
    uint32_t title_id;
    uint16_t publisher;
    uint16_t number;
    char publisher_id[3];

    if (!xemu_xbe_patch_certificate_offset(xbe, xbe_size, &cert_offset)) {
        return NULL;
    }

    title_id = xemu_xbe_patch_ldl_le(
        xbe + cert_offset + offsetof(struct xbe_certificate, m_titleid));
    publisher = title_id >> 16;
    number = title_id & 0xffff;
    publisher_id[0] = publisher >> 8;
    publisher_id[1] = publisher & 0xff;
    publisher_id[2] = '\0';

    if (g_ascii_isalnum(publisher_id[0]) &&
        g_ascii_isalnum(publisher_id[1])) {
        return g_strdup_printf("%s-%04u", publisher_id, number);
    }

    return NULL;
}

static char *xemu_xbe_patch_extract_title_name(const uint8_t *xbe,
                                               size_t xbe_size)
{
    gunichar2 title[41];
    size_t cert_offset;
    size_t title_offset;
    int title_len = 0;

    if (!xemu_xbe_patch_certificate_offset(xbe, xbe_size, &cert_offset)) {
        return NULL;
    }

    title_offset = cert_offset + offsetof(struct xbe_certificate, m_title_name);
    for (int i = 0; i < 40; i++) {
        const uint8_t *p = xbe + title_offset + i * 2;
        gunichar2 ch = p[0] | ((gunichar2)p[1] << 8);
        if (!ch) {
            break;
        }
        title[title_len++] = ch;
    }
    title[title_len] = 0;

    if (!title_len) {
        return NULL;
    }

    return g_utf16_to_utf8(title, title_len, NULL, NULL, NULL);
}

static char *xemu_xbe_patch_extract_region(const uint8_t *xbe,
                                           size_t xbe_size)
{
    GString *region_name;
    size_t cert_offset;
    uint32_t region;

    if (!xemu_xbe_patch_certificate_offset(xbe, xbe_size, &cert_offset)) {
        return NULL;
    }

    region = xemu_xbe_patch_ldl_le(
        xbe + cert_offset + offsetof(struct xbe_certificate, m_game_region));
    if (!region || (region & ~0x7)) {
        return NULL;
    }

    if (region == 0x7) {
        return g_strdup("Region Free");
    }

    region_name = g_string_new(NULL);
    if (region & 0x1) {
        g_string_append(region_name, "NTSC-U");
    }
    if (region & 0x2) {
        if (region_name->len) {
            g_string_append_c(region_name, '/');
        }
        g_string_append(region_name, "NTSC-J");
    }
    if (region & 0x4) {
        if (region_name->len) {
            g_string_append_c(region_name, '/');
        }
        g_string_append(region_name, "PAL");
    }

    return g_string_free(region_name, false);
}

static int xemu_xbe_patch_hex_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static bool xemu_xbe_patch_parse_hex(const char *str, uint8_t **data,
                                     size_t *size)
{
    g_autofree char *hex = NULL;
    size_t len = 0;
    size_t j = 0;

    if (!str || !str[0]) {
        return false;
    }

    for (const char *p = str; *p; p++) {
        if (g_ascii_isspace(*p)) {
            continue;
        }
        if (xemu_xbe_patch_hex_value(*p) < 0) {
            return false;
        }
        len++;
    }

    if (!len || (len & 1)) {
        return false;
    }

    hex = g_malloc(len + 1);
    for (const char *p = str; *p; p++) {
        if (!g_ascii_isspace(*p)) {
            hex[j++] = *p;
        }
    }
    hex[j] = 0;

    *data = g_malloc(len / 2);
    *size = len / 2;
    for (size_t i = 0; i < *size; i++) {
        int hi = xemu_xbe_patch_hex_value(hex[i * 2]);
        int lo = xemu_xbe_patch_hex_value(hex[i * 2 + 1]);
        (*data)[i] = (hi << 4) | lo;
    }

    return true;
}

static bool xemu_xbe_patch_parse_offset_line(const char *line,
                                             uint64_t *offset,
                                             uint8_t **replacement,
                                             size_t *replacement_size)
{
    const char *colon;
    g_autofree char *offset_text = NULL;
    uint64_t parsed_offset;

    if (!g_str_has_prefix(line, "0x") && !g_str_has_prefix(line, "0X")) {
        return false;
    }

    colon = strchr(line, ':');
    if (!colon || colon == line) {
        return false;
    }

    offset_text = g_strndup(line, colon - line);
    if (qemu_strtou64(offset_text, NULL, 0, &parsed_offset) < 0) {
        return false;
    }

    if (!xemu_xbe_patch_parse_hex(colon + 1, replacement,
                                  replacement_size)) {
        return false;
    }

    *offset = parsed_offset;
    return true;
}

static bool xemu_xbe_patch_apply_direct(uint8_t *xbe, size_t xbe_size,
                                        uint64_t offset,
                                        const uint8_t *replacement,
                                        size_t replacement_size)
{
    if (offset > SIZE_MAX || replacement_size > xbe_size ||
        (size_t)offset > xbe_size - replacement_size) {
        return false;
    }

    memcpy(xbe + offset, replacement, replacement_size);
    return true;
}

/*
 * Replace every non-overlapping occurrence of search with replacement (same
 * length) in the in-memory XBE copy. After a hit the cursor skips past the
 * replaced span so a replacement that reintroduces the pattern is not matched
 * again. Bounded by xbe_size; returns whether at least one match was applied.
 */
static bool xemu_xbe_patch_apply_search(uint8_t *xbe, size_t xbe_size,
                                        const uint8_t *search,
                                        const uint8_t *replacement,
                                        size_t size, unsigned int *matches)
{
    if (!size || size > xbe_size) {
        return false;
    }

    *matches = 0;
    for (size_t offset = 0; offset <= xbe_size - size; offset++) {
        if (!memcmp(xbe + offset, search, size)) {
            memcpy(xbe + offset, replacement, size);
            (*matches)++;
            offset += size - 1;
        }
    }

    return *matches > 0;
}

static bool xemu_xbe_patch_apply_direct_to_target(GPtrArray *files,
                                                  const char *target_name,
                                                  uint64_t offset,
                                                  const uint8_t *replacement,
                                                  size_t replacement_size,
                                                  int line_number,
                                                  int *applied,
                                                  char **error)
{
    XemuXbePatchFile *file = xemu_xbe_patch_file_by_name(files, target_name);

    if (!file) {
        *error = g_strdup_printf("target '%s' was not found on the disc",
                                 target_name);
        return false;
    }

    if (!xemu_xbe_patch_apply_direct(file->data, file->size, offset,
                                     replacement, replacement_size)) {
        *error = g_strdup_printf("line %d writes outside %s",
                                 line_number, target_name);
        return false;
    }

    (*applied)++;
    return true;
}

static bool xemu_xbe_patch_apply_search_to_target(GPtrArray *files,
                                                  const char *target_name,
                                                  const uint8_t *search,
                                                  const uint8_t *replacement,
                                                  size_t size,
                                                  int line_number,
                                                  int *applied,
                                                  char **error)
{
    XemuXbePatchFile *file = xemu_xbe_patch_file_by_name(files, target_name);
    unsigned int matches = 0;

    if (!file) {
        *error = g_strdup_printf("target '%s' was not found on the disc",
                                 target_name);
        return false;
    }

    if (!xemu_xbe_patch_apply_search(file->data, file->size, search,
                                     replacement, size, &matches)) {
        *error = g_strdup_printf("line %d search pattern was not found in %s",
                                 line_number, target_name);
        return false;
    }

    *applied += matches;
    return true;
}

static bool xemu_xbe_patch_apply_direct_to_scope(GPtrArray *files,
                                                 GPtrArray *scope,
                                                 const char *target_name,
                                                 bool global,
                                                 uint64_t offset,
                                                 const uint8_t *replacement,
                                                 size_t replacement_size,
                                                 int line_number,
                                                 int *applied,
                                                 char **error)
{
    if (!global) {
        return xemu_xbe_patch_apply_direct_to_target(
            files, target_name, offset, replacement, replacement_size,
            line_number, applied, error);
    }

    for (unsigned int i = 0; i < scope->len; i++) {
        const char *scope_target = g_ptr_array_index(scope, i);
        if (!xemu_xbe_patch_apply_direct_to_target(
                files, scope_target, offset, replacement, replacement_size,
                line_number, applied, error)) {
            return false;
        }
    }

    return true;
}

static bool xemu_xbe_patch_apply_search_to_scope(GPtrArray *files,
                                                 GPtrArray *scope,
                                                 const char *target_name,
                                                 bool global,
                                                 const uint8_t *search,
                                                 const uint8_t *replacement,
                                                 size_t size,
                                                 int line_number,
                                                 int *applied,
                                                 char **error)
{
    if (!global) {
        return xemu_xbe_patch_apply_search_to_target(
            files, target_name, search, replacement, size,
            line_number, applied, error);
    }

    for (unsigned int i = 0; i < scope->len; i++) {
        const char *scope_target = g_ptr_array_index(scope, i);
        if (!xemu_xbe_patch_apply_search_to_target(
                files, scope_target, search, replacement, size,
                line_number, applied, error)) {
            return false;
        }
    }

    return true;
}

static bool xemu_xbe_patch_apply_file(const char *path, GPtrArray *files,
                                      int *applied, char **error)
{
    g_autofree char *contents = NULL;
    g_auto(GStrv) lines = NULL;
    g_autoptr(GPtrArray) scope = NULL;
    g_autofree char *current_target = g_strdup("default.xbe");
    g_autofree uint8_t *pending_search = NULL;
    g_autofree char *pending_target = NULL;
    size_t pending_search_size = 0;
    gsize contents_len = 0;
    bool current_global = false;
    bool pending_global = false;
    bool explicit_targets = false;
    bool current_enabled = true;
    GError *err = NULL;

    if (!g_file_get_contents(path, &contents, &contents_len, &err)) {
        *error = g_strdup_printf("Could not load XBE patch '%s': %s",
                                 path, err->message);
        g_error_free(err);
        return false;
    }

    scope = g_ptr_array_new_with_free_func(g_free);
    if (!xemu_xbe_patch_collect_file_targets(path, scope, error)) {
        return false;
    }

    lines = g_strsplit(contents, "\n", -1);
    for (int i = 0; lines[i]; i++) {
        char *line = g_strstrip(lines[i]);
        char *target;

        if (i == 0 && g_str_has_prefix(line, "\xef\xbb\xbf")) {
            line += 3;
        }

        target = xemu_xbe_patch_comment_target(line);
        if (target) {
            g_free(target);
            explicit_targets = true;
            break;
        }
    }
    current_enabled = !explicit_targets;

    for (int i = 0; lines[i]; i++) {
        char *line = g_strstrip(lines[i]);
        g_autofree char *comment_target = NULL;
        bool is_direct_patch;
        uint64_t direct_offset = 0;
        g_autofree uint8_t *direct_replacement = NULL;
        size_t direct_replacement_size = 0;
        g_autofree uint8_t *bytes = NULL;
        size_t bytes_size = 0;

        if (i == 0 && g_str_has_prefix(line, "\xef\xbb\xbf")) {
            line += 3;
        }

        if (!line[0] || strchr(line, '=')) {
            continue;
        }

        if (line[0] == '#') {
            if (pending_search) {
                *error = g_strdup_printf(
                    "'%s' line %d has a search pattern without replacement",
                    path, i + 1);
                return false;
            }

            if (xemu_xbe_patch_comment_is_global(line)) {
                current_global = true;
                current_enabled = true;
                continue;
            }

            comment_target = xemu_xbe_patch_comment_target(line);
            if (comment_target) {
                g_free(current_target);
                current_target = g_steal_pointer(&comment_target);
                current_global = false;
                current_enabled = true;
            } else if (explicit_targets) {
                current_global = false;
                current_enabled = false;
            }
            continue;
        }

        if (!current_enabled) {
            continue;
        }

        is_direct_patch = g_str_has_prefix(line, "0x") ||
                          g_str_has_prefix(line, "0X");
        if (is_direct_patch &&
            xemu_xbe_patch_parse_offset_line(line, &direct_offset,
                                             &direct_replacement,
                                             &direct_replacement_size)) {
            if (pending_search) {
                *error = g_strdup_printf(
                    "'%s' line %d has a search pattern without replacement",
                    path, i + 1);
                return false;
            }

            if (!xemu_xbe_patch_apply_direct_to_scope(
                    files, scope, current_target, current_global,
                    direct_offset, direct_replacement,
                    direct_replacement_size, i + 1, applied, error)) {
                return false;
            }
            continue;
        }

        if (is_direct_patch) {
            *error = g_strdup_printf("'%s' line %d is not a valid offset patch",
                                     path, i + 1);
            return false;
        }

        if (!xemu_xbe_patch_parse_hex(line, &bytes, &bytes_size)) {
            *error = g_strdup_printf("'%s' line %d is not valid patch data",
                                     path, i + 1);
            return false;
        }

        if (!pending_search) {
            pending_search = g_steal_pointer(&bytes);
            pending_target = g_strdup(current_target);
            pending_search_size = bytes_size;
            pending_global = current_global;
            continue;
        }

        if (pending_search_size != bytes_size) {
            *error = g_strdup_printf(
                "'%s' line %d replacement size does not match search size",
                path, i + 1);
            return false;
        }

        if (!xemu_xbe_patch_apply_search_to_scope(
                files, scope, pending_target, pending_global,
                pending_search, bytes, bytes_size, i, applied, error)) {
            return false;
        }

        g_clear_pointer(&pending_search, g_free);
        g_clear_pointer(&pending_target, g_free);
        pending_search_size = 0;
        pending_global = false;
    }

    if (pending_search) {
        *error = g_strdup_printf("'%s' ended with a search pattern without "
                                 "replacement", path);
        return false;
    }

    return true;
}

static bool xemu_xbe_patch_read_file(BlockBackend *blk, const char *name,
                                     XemuXbePatchFile **file, char **error)
{
    g_autofree uint8_t *data = NULL;
    uint64_t offset = 0;
    size_t size = 0;
    XemuXbePatchFile *new_file;

    if (!xemu_xbe_patch_find_xbe(blk, name, &offset, &size)) {
        *error = g_strdup_printf("%s was not found on the loaded disc", name);
        return false;
    }

    data = g_try_malloc(size);
    if (!data) {
        *error = g_strdup_printf("Could not allocate memory for %s", name);
        return false;
    }

    if (!xemu_xbe_patch_read(blk, offset, data, size)) {
        *error = g_strdup_printf("Could not read %s from the disc", name);
        return false;
    }

    if (size < 4 || memcmp(data, XEMU_XBE_MAGIC, 4)) {
        *error = g_strdup_printf("%s does not look like an XBE file", name);
        return false;
    }

    new_file = g_new0(XemuXbePatchFile, 1);
    new_file->name = g_strdup(name);
    new_file->offset = offset;
    new_file->size = size;
    new_file->data = g_steal_pointer(&data);
    *file = new_file;
    return true;
}

/*
 * Locate default.xbe on the freshly inserted disc, identify the game by its
 * SHA1, and if a matching enabled patch profile exists, read every target XBE
 * and apply the profile's patch files to in-memory copies. The result is
 * published atomically via commit(), which drops it if another disc change
 * bumped the generation meanwhile. All slow disc I/O runs without the mutex
 * held; only the short book-keeping at start and the commit take the lock.
 */
void xemu_xbe_patch_prepare(BlockBackend *blk)
{
    uint64_t xbe_offset = 0;
    uint64_t generation;
    size_t xbe_size = 0;
    g_autofree uint8_t *xbe = NULL;
    g_autoptr(GPtrArray) target_names = NULL;
    g_autoptr(GPtrArray) patched_xbes = NULL;
    g_autofree char *current_hash = NULL;
    g_autofree char *current_title_id = NULL;
    g_autofree char *current_title_name = NULL;
    g_autofree char *current_region = NULL;
    g_autofree char *patch_error = NULL;
    int profile_index;
    int selected;
    int applied = 0;
    unsigned int patched_file_count;

    if (!xemu_xbe_patch_is_cdrom(blk)) {
        return;
    }

    g_mutex_lock(&xemu_xbe_patch_mutex);
    generation = ++xemu_xbe_patch_state.generation;
    xemu_xbe_patch_clear_state_locked();
    xemu_xbe_patch_state.blk = blk;
    xemu_xbe_patch_state.preparing = true;
    xemu_xbe_patch_state.status = g_strdup("Preparing XBE patches...");
    g_mutex_unlock(&xemu_xbe_patch_mutex);

    if (!xemu_xbe_patch_find_xbe(blk, "default.xbe", &xbe_offset,
                                 &xbe_size)) {
        xemu_xbe_patch_commit(generation, blk, NULL, NULL, NULL, NULL, NULL,
                              g_strdup("No default.xbe found on the loaded "
                                       "disc."));
        return;
    }

    xbe = g_try_malloc(xbe_size);
    if (!xbe) {
        xemu_xbe_patch_commit(generation, blk, NULL, NULL, NULL, NULL, NULL,
                              g_strdup("Could not allocate memory for "
                                       "default.xbe."));
        return;
    }

    if (!xemu_xbe_patch_read(blk, xbe_offset, xbe, xbe_size)) {
        xemu_xbe_patch_commit(generation, blk, NULL, NULL, NULL, NULL, NULL,
                              g_strdup("Could not read default.xbe from the "
                                       "disc."));
        return;
    }

    if (xbe_size < 4 || memcmp(xbe, XEMU_XBE_MAGIC, 4)) {
        xemu_xbe_patch_commit(generation, blk, NULL, NULL, NULL, NULL, NULL,
                              g_strdup("default.xbe does not look like an "
                                       "XBE file."));
        return;
    }

    current_hash = g_compute_checksum_for_data(G_CHECKSUM_SHA1, xbe, xbe_size);
    current_title_id = xemu_xbe_patch_extract_title_id(xbe, xbe_size);
    current_title_name = xemu_xbe_patch_extract_title_name(xbe, xbe_size);
    current_region = xemu_xbe_patch_extract_region(xbe, xbe_size);

    if (!g_config.sys.xbe_patches.enabled) {
        xemu_xbe_patch_commit(generation, blk, NULL,
                              g_steal_pointer(&current_hash),
                              g_steal_pointer(&current_title_id),
                              g_steal_pointer(&current_title_name),
                              g_steal_pointer(&current_region),
                              g_strdup("XBE patches are disabled."));
        return;
    }

    profile_index = xemu_xbe_patch_profile_index(current_hash);
    if (profile_index < 0) {
        xemu_xbe_patch_commit(generation, blk, NULL,
                              g_steal_pointer(&current_hash),
                              g_steal_pointer(&current_title_id),
                              g_steal_pointer(&current_title_name),
                              g_steal_pointer(&current_region),
                              g_strdup("No XBE patch profile for this game."));
        return;
    }

    selected = xemu_xbe_patch_selected_count(profile_index);
    if (!selected) {
        xemu_xbe_patch_commit(generation, blk, NULL,
                              g_steal_pointer(&current_hash),
                              g_steal_pointer(&current_title_id),
                              g_steal_pointer(&current_title_name),
                              g_steal_pointer(&current_region),
                              g_strdup("XBE patch profile has no patch files "
                                       "selected."));
        return;
    }

    target_names = g_ptr_array_new_with_free_func(g_free);
    for (int i = 0; i < XEMU_XBE_PATCH_MAX_FILES; i++) {
        const char *path = xemu_xbe_patch_path_for_index(profile_index, i);
        if (path && path[0] &&
            !xemu_xbe_patch_collect_file_targets(path, target_names,
                                                &patch_error)) {
            xemu_xbe_patch_commit(generation, blk, NULL,
                                  g_steal_pointer(&current_hash),
                                  g_steal_pointer(&current_title_id),
                                  g_steal_pointer(&current_title_name),
                                  g_steal_pointer(&current_region),
                                  g_strdup_printf("XBE patches were not "
                                                  "applied: %s",
                                                  patch_error));
            return;
        }
    }

    patched_xbes = g_ptr_array_new_with_free_func(xemu_xbe_patch_file_free);
    for (unsigned int i = 0; i < target_names->len; i++) {
        const char *target_name = g_ptr_array_index(target_names, i);
        XemuXbePatchFile *file = NULL;

        if (!xemu_xbe_patch_read_file(blk, target_name, &file,
                                      &patch_error)) {
            xemu_xbe_patch_commit(generation, blk, NULL,
                                  g_steal_pointer(&current_hash),
                                  g_steal_pointer(&current_title_id),
                                  g_steal_pointer(&current_title_name),
                                  g_steal_pointer(&current_region),
                                  g_strdup_printf("XBE patches were not "
                                                  "applied: %s",
                                                  patch_error));
            return;
        }
        g_ptr_array_add(patched_xbes, file);
    }

    for (int i = 0; i < XEMU_XBE_PATCH_MAX_FILES; i++) {
        const char *path = xemu_xbe_patch_path_for_index(profile_index, i);
        if (path && path[0] &&
            !xemu_xbe_patch_apply_file(path, patched_xbes, &applied,
                                       &patch_error)) {
            xemu_xbe_patch_commit(generation, blk, NULL,
                                  g_steal_pointer(&current_hash),
                                  g_steal_pointer(&current_title_id),
                                  g_steal_pointer(&current_title_name),
                                  g_steal_pointer(&current_region),
                                  g_strdup_printf("XBE patches were not "
                                                  "applied: %s",
                                                  patch_error));
            return;
        }
    }

    if (!applied) {
        xemu_xbe_patch_commit(generation, blk, NULL,
                              g_steal_pointer(&current_hash),
                              g_steal_pointer(&current_title_id),
                              g_steal_pointer(&current_title_name),
                              g_steal_pointer(&current_region),
                              g_strdup("No XBE patch replacements were "
                                       "applied."));
        return;
    }

    patched_file_count = patched_xbes->len;
    xemu_xbe_patch_commit(generation, blk,
                          g_steal_pointer(&patched_xbes),
                          g_steal_pointer(&current_hash),
                          g_steal_pointer(&current_title_id),
                          g_steal_pointer(&current_title_name),
                          g_steal_pointer(&current_region),
                          g_strdup_printf("Applied %d XBE patch "
                                          "replacement%s to %u XBE file%s "
                                          "from %d patch file%s.",
                                          applied,
                                          applied == 1 ? "" : "s",
                                          patched_file_count,
                                          patched_file_count == 1 ? "" : "s",
                                          selected,
                                          selected == 1 ? "" : "s"));
}

/*
 * Splice patched XBE bytes into a completed guest read. Called from the block
 * backend read path for every backend, so it bails out immediately unless the
 * read is for the CD backend that currently has patches committed. For each
 * patched XBE it computes the overlap between the read range and the XBE's
 * on-disc extent (all in block-backend byte coordinates) and overwrites just
 * that slice of the returned buffer. All deltas are overflow- and
 * bounds-checked so the copy stays within both file->data and the qiov.
 */
void xemu_xbe_patch_apply_read(BlockBackend *blk, int64_t offset,
                               int64_t bytes, QEMUIOVector *qiov,
                               size_t qiov_offset)
{
    uint64_t read_start;
    uint64_t read_end;

    /* Fast path: skip the lock entirely when no patches are active, which is
     * the common case for every read of every block device. The authoritative
     * state is still re-checked under the mutex below. */
    if (!qatomic_read(&xemu_xbe_patch_active)) {
        return;
    }

    g_mutex_lock(&xemu_xbe_patch_mutex);
    if (xemu_xbe_patch_state.preparing ||
        xemu_xbe_patch_state.blk != blk ||
        !xemu_xbe_patch_state.patched_xbes ||
        offset < 0 || bytes <= 0) {
        goto out;
    }

    read_start = offset;
    read_end = read_start + bytes;
    if (read_end < read_start) {
        goto out;
    }

    for (unsigned int i = 0; i < xemu_xbe_patch_state.patched_xbes->len; i++) {
        XemuXbePatchFile *file =
            g_ptr_array_index(xemu_xbe_patch_state.patched_xbes, i);
        uint64_t xbe_start = file->offset;
        uint64_t xbe_end = xbe_start + file->size;
        uint64_t overlap_start;
        uint64_t overlap_end;
        size_t request_offset;
        size_t xbe_offset;
        size_t overlap_size;

        if (xbe_end < xbe_start || read_end <= xbe_start ||
            read_start >= xbe_end) {
            continue;
        }

        overlap_start = MAX(read_start, xbe_start);
        overlap_end = MIN(read_end, xbe_end);
        if (overlap_start - read_start > SIZE_MAX ||
            overlap_start - xbe_start > SIZE_MAX ||
            overlap_end - overlap_start > SIZE_MAX) {
            continue;
        }

        request_offset = (size_t)(overlap_start - read_start);
        xbe_offset = (size_t)(overlap_start - xbe_start);
        overlap_size = (size_t)(overlap_end - overlap_start);
        if (request_offset > SIZE_MAX - qiov_offset) {
            continue;
        }

        qemu_iovec_from_buf(qiov, qiov_offset + request_offset,
                            file->data + xbe_offset, overlap_size);
    }

out:
    g_mutex_unlock(&xemu_xbe_patch_mutex);
}
