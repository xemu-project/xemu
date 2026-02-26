if host_machine.system() == 'windows'
  cdata.set('SDL_TIME_WINDOWS', 1)
  subdir('windows')
else
  cdata.set('SDL_TIME_UNIX', 1)
  subdir('unix')
endif

sources += files('SDL_time.c')
