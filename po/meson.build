i18n = import('i18n')

if find_program('xgettext', required: get_option('gettext')).found()
  i18n.gettext(meson.project_name(),
               args: [
                 '--msgid-bugs-address=qemu-devel@nongnu.org',
                 '--keyword=SectionTitle:1',
                 '--keyword=Toggle:1',
                 '--keyword=Slider:1',
                 '--keyword=FilePicker:1',
                 '--keyword=ChevronCombo:1',
                 '--keyword=ChevronCombo:4',
                 '--keyword=ChevronCombo:6',
                 '--keyword=MainMenuTabButton:1',
                 '--keyword=PopupMenuButton:1',
                 '--keyword=PopupMenuSubmenuButton:1',
                 '--keyword=PopupMenuToggle:1',
               ],
               preset: 'glib')
endif
