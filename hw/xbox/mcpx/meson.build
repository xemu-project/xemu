subdir('dsp')

mcpx_ss = ss.source_set()
mcpx_ss.add(sdl, libsamplerate, dsp, files(
	'apu.c',
	'aci.c',
	))

specific_ss.add_all(mcpx_ss)