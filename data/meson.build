pfiles = [
  'controller_mask.png',
  'logo_sdf.png',
  'xemu_64x64.png',
  'roboto_medium.ttf',
]

libpfile_targets = []

foreach e : pfiles
  t = custom_target('@0@.[ch]'.format(e),
                    output: ['@0@.c'.format(e), '@0@.h'.format(e)],
                    input: files('@0@'.format(e)),
                    command: [packfile, '@INPUT@', '@OUTPUT0@', '@OUTPUT1@'])
  libpfile_targets += t[0]
  genh += t[1]
endforeach

libpfile = static_library('pdata', libpfile_targets,
                          name_suffix: 'fa')
common_ss.add(declare_dependency(link_whole: libpfile))
