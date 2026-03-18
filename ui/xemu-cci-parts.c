/*
 * Collect split .N.cci disc parts (1..8) in sorted order.
 */
#include "qemu/osdep.h"
#include "ui/xemu-cci-parts.h"
#include <glib.h>

static bool basename_is_numbered_cci(const char *base, char **prefix_out)
{
    size_t len = strlen(base);

    /* e.g. name.1.cci — suffix ".N.cci" is 6 chars */
    if (len < 7) {
        return false;
    }
    if (qemu_tolower((unsigned char)base[len - 1]) != 'i' ||
        qemu_tolower((unsigned char)base[len - 2]) != 'c' ||
        qemu_tolower((unsigned char)base[len - 3]) != 'c' ||
        base[len - 4] != '.') {
        return false;
    }
    if (base[len - 5] < '1' || base[len - 5] > '8' || base[len - 6] != '.') {
        return false;
    }
    *prefix_out = g_strndup(base, len - 6);
    return true;
}

int xemu_cci_collect_part_paths(const char *picked_path, char ***paths_out,
                                int *n_out)
{
    char *abspath;
    char *dir = NULL;
    char *base = NULL;
    char *prefix = NULL;
    GPtrArray *arr;
    int i;

    g_return_val_if_fail(picked_path && paths_out && n_out, -1);
    *paths_out = NULL;
    *n_out = 0;

    abspath = g_canonicalize_filename(picked_path, NULL);
    if (!abspath) {
        abspath = g_strdup(picked_path);
    }

    dir = g_path_get_dirname(abspath);
    base = g_path_get_basename(abspath);

    if (!basename_is_numbered_cci(base, &prefix)) {
        *paths_out = g_new(char *, 1);
        (*paths_out)[0] = abspath;
        *n_out = 1;
        g_free(dir);
        g_free(base);
        return 0;
    }

    g_free(abspath);

    /*
     * Contiguous parts only: .1.cci, .2.cci, ... stop at first gap (max 8).
     * Ensures the virtual disc matches a single concatenated stream.
     */
    arr = g_ptr_array_new();

    for (i = 1; i <= 8; i++) {
        char *fname = g_strdup_printf("%s.%d.cci", prefix, i);
        char *full = g_build_filename(dir, fname, NULL);

        g_free(fname);
        if (!g_file_test(full, G_FILE_TEST_IS_REGULAR)) {
            g_free(full);
            break;
        }
        g_ptr_array_add(arr, full);
    }

    g_free(prefix);
    g_free(base);
    g_free(dir);

    if (arr->len == 0) {
        g_ptr_array_free(arr, TRUE);
        return -1;
    }

    *n_out = (int)arr->len;
    *paths_out = (char **)g_ptr_array_free(arr, FALSE);
    return 0;
}
