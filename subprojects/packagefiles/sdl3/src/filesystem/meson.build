if host_machine.system() == 'windows'
  cdata.set('SDL_FSOPS_WINDOWS', 1)
  cdata.set('SDL_FILESYSTEM_WINDOWS', 1)
  subdir('windows')
else
  cdata.set('SDL_FSOPS_POSIX', 1)
  cdata.set('SDL_FILESYSTEM_UNIX', 1)
  subdir('unix')
  subdir('posix')
endif

sources += files('SDL_filesystem.c')
