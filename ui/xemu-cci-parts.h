/*
 * Discover split CCI images: name.1.cci ... name.8.cci
 */
#ifndef UI_XEMU_CCI_PARTS_H
#define UI_XEMU_CCI_PARTS_H

/**
 * If @picked_path matches name.[1-8].cci, builds the ordered list
 * name.1.cci, name.2.cci, ... up to the first missing part (max 8).
 * Requires name.1.cci to exist when using numbered parts.
 * Plain name.cci (no .N.) yields a single path.
 * @paths_out: g_strfreev when done. Returns 0, or -1 on failure.
 */
int xemu_cci_collect_part_paths(const char *picked_path, char ***paths_out,
                                int *n_out);

#endif
