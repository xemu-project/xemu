#subdir('generic')

if host_machine.system() == 'windows'
  subdir('windows')
else
  subdir('pthread')
endif

sources += files('SDL_thread.c')
