#ifndef QEMU_DATADIR_H
#define QEMU_DATADIR_H

#define QEMU_FILE_TYPE_BIOS   0
#define QEMU_FILE_TYPE_KEYMAP 1
/**
 * qemu_find_file:
 * @type: QEMU_FILE_TYPE_BIOS (for BIOS, VGA BIOS)
 *        or QEMU_FILE_TYPE_KEYMAP (for keymaps).
 * @name: Relative or absolute file name
 *
 * If @name exists on disk as an absolute path, or a path relative
 * to the current directory, then returns @name unchanged.
 * Otherwise searches for @name file in the data directories, either
 * configured at build time (DATADIR) or registered with the -L command
 * line option.
 *
 * The caller must use g_free() to free the returned data when it is
 * no longer required.
 *
 * Returns: a path that can access @name, or NULL if no matching file exists.
 */
char *qemu_find_file(int type, const char *name);
void qemu_add_default_firmwarepath(void);
void qemu_add_data_dir(char *path);
void qemu_list_data_dirs(void);

#endif
