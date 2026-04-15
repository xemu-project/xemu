if host_machine.system() == 'windows'
  subdir('windows')
elif host_machine.system() == 'darwin'
  subdir('macos')
else
  subdir('linux')
endif

sources += files('SDL_power.c')
