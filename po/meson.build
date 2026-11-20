i18n = import('i18n')

if find_program('xgettext', required: get_option('gettext')).found()
  i18n.gettext('xemu',
               args: [
                 '--add-location=file',
                 '--msgid-bugs-address=qemu-devel@nongnu.org',
               ],
               preset: 'glib')
endif

if host_os == 'windows'
  msgfmt = find_program('msgfmt', required: true)
  xemu_sv_catalog = custom_target(
    'xemu Swedish catalog',
    input: files('sv.po'),
    output: 'xemu-sv-catalog.cc',
    command: [python, files('../scripts/embed-mo-catalog.py'), msgfmt,
              '@INPUT@', '@OUTPUT@'])
  # ui/meson.build has already transferred xemu_ss into system_ss.
  system_ss.add(xemu_sv_catalog)
endif
