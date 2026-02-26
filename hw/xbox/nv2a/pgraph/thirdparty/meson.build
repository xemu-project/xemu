nv2a_vsh_cpu_opts = cmake.subproject_options()
nv2a_vsh_cpu_opts.add_cmake_defines({
  'CMAKE_OSX_ARCHITECTURES': cmake_macos_arch,
  'nv2a_vsh_cpu_UNIT_TEST': 'OFF',
  })
nv2a_vsh_cpu_subproj = cmake.subproject('nv2a_vsh_cpu', options: nv2a_vsh_cpu_opts)
nv2a_vsh_cpu = declare_dependency(include_directories: nv2a_vsh_cpu_subproj.include_directories('nv2a_vsh_emulator'),
                                  link_with: [
                                    nv2a_vsh_cpu_subproj.target('nv2a_vsh_emulator'),
                                    nv2a_vsh_cpu_subproj.target('nv2a_vsh_cpu'),
                                    nv2a_vsh_cpu_subproj.target('nv2a_vsh_disassembler'),
                                    ])

libgloffscreen = static_library('libgloffscreen',
                          sources: files('gloffscreen/common.c', 'gloffscreen/sdl.c'),
                          dependencies: sdl)
gloffscreen = declare_dependency(link_with: libgloffscreen,
                           include_directories: ['gloffscreen'])
