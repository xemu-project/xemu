if host_machine.system() == 'windows'
  subdir('d3d12')
  subdir('vulkan')
elif host_machine.system() == 'darwin'
  subdir('metal')
  subdir('vulkan')
else
  subdir('vulkan')
endif

sources += files('SDL_gpu.c')
