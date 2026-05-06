dsp56300_emu_ffi = dependency('dsp56300-emu-ffi', fallback: ['dsp56300', 'dsp56300_dep'])

subdir('interp')

libdsp = static_library(
  'dsp',
  files(['dsp.c', 'dsp_c.c', 'dsp_jit.c', 'dsp_dma.c']) + genh,
  dependencies: [dsp56300_emu_ffi],
)

dsp = declare_dependency(
  objects: [
    libdsp.extract_all_objects(recursive: false),
    libdsp_cpu_interp.extract_all_objects(recursive: false),
  ],
  dependencies: [dsp56300_emu_ffi],
)

mcpx_ss.add(dsp, files('gp_ep.c'))
