if host_machine.system() == 'windows'
  cdata.set('SDL_HAPTIC_DINPUT', 1)
  subdir('windows')
elif host_machine.system() == 'darwin'
  cdata.set('SDL_HAPTIC_IOKIT', 1)
  subdir('darwin')
else
  cdata.set('SDL_HAPTIC_LINUX', 1)
  subdir('linux')
endif

sources += files('SDL_haptic.c')
