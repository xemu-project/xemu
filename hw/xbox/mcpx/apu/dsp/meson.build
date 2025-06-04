libdsp = static_library('dsp', files(['debug.c', 'dsp.c', 'dsp_cpu.c', 'dsp_dma.c']) + genh)
dsp = declare_dependency(objects: libdsp.extract_all_objects(recursive: false))

mcpx_ss.add(dsp, files('gp_ep.c'))
