project('keycodemapdb', meson_version: '>=0.46.0')

keymap_gen = find_program('tools/keymap-gen')
meson.override_find_program('keymap-gen', keymap_gen)

keymaps_csv = files('data/keymaps.csv')
