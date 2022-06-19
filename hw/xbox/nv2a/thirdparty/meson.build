nv2a_vsh_cpu_files = files(
  'nv2a_vsh_cpu/src/nv2a_vsh_cpu.c',
  'nv2a_vsh_cpu/src/nv2a_vsh_disassembler.c',
  'nv2a_vsh_cpu/src/nv2a_vsh_emulator.c',
  'nv2a_vsh_cpu/src/nv2a_vsh_emulator_execution_state.c',
)

libnv2a_vsh_cpu = static_library('nv2a_vsh_cpu',
                          sources: nv2a_vsh_cpu_files,
                          include_directories: ['.', 'nv2a_vsh_cpu/src'])
nv2a_vsh_cpu = declare_dependency(link_with: libnv2a_vsh_cpu,
                           include_directories: ['nv2a_vsh_cpu/src'])
