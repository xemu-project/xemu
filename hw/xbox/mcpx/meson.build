
mcpx_ss = ss.source_set()
mcpx_ss.add(sdl, files(
	'apu.c',
	'aci.c',
	'debug.c',
	))

subdir('vp')
subdir('dsp')

specific_ss.add_all(mcpx_ss)
