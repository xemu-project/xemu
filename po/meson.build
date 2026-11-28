i18n = import('i18n')
fs = import('fs')

if find_program('xgettext', required: get_option('gettext')).found()
  i18n.gettext('xemu',
               args: [
                 '--add-location=file',
                 '--msgid-bugs-address=qemu-devel@nongnu.org',
               ],
               preset: 'glib')
endif

if host_os == 'windows'
  msgfmt = find_program('msgfmt', required: get_option('gettext'))
  if msgfmt.found()
    embedded_catalog = custom_target(
      'xemu embedded translation catalogs',
      input: fs.glob('*.po'),
      output: 'xemu-embedded-catalogs.cc',
      command: [python, files('../scripts/embed-mo-catalog.py'), msgfmt,
                '@INPUT@', '@OUTPUT@'])
    system_ss.add(embedded_catalog)
  else
    system_ss.add(files('xemu-empty-catalogs.cc'))
  endif
endif
