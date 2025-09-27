specific_ss.add(files(
	'pgraph.c',
	'profile.c',
	'rdi.c',
	's3tc.c',
	'swizzle.c',
	'texture.c',
	'vertex.c',
	))
if have_renderdoc
	specific_ss.add(files('debug_renderdoc.c'))
endif
subdir('thirdparty')
subdir('null')
subdir('gl')
subdir('glsl')
subdir('vk')
specific_ss.add(nv2a_vsh_cpu)
