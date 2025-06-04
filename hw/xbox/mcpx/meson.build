subdir('dsp')

mcpx_ss = ss.source_set()
mcpx_ss.add(sdl, dsp, files(
	'apu.c',
	'aci.c',
	'debug.c',
	'dsp.c',
	))

subdir('vp')

specific_ss.add_all(mcpx_ss)