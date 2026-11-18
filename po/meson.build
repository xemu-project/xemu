i18n = import('i18n')

if find_program('xgettext', required: get_option('gettext')).found()
  i18n.gettext('xemu',
               args: [
                 '--add-location=file',
                 '--msgid-bugs-address=qemu-devel@nongnu.org',
               ],
               preset: 'glib')
endif
