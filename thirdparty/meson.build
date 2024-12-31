if vulkan.found()

volk_opts = cmake.subproject_options()
volk_opts.add_cmake_defines({'VOLK_STATIC_DEFINES': 'VK_NO_PROTOTYPES'})
volk_subproj = cmake.subproject('volk', options: volk_opts)
volk = declare_dependency(compile_args: ['-DVK_NO_PROTOTYPES'],
                          include_directories: volk_subproj.include_directories('volk'),
                          link_with: volk_subproj.target('volk'),
                          dependencies: vulkan)

debug_vma = false

vma_defns = [
	'-DVMA_STATIC_VULKAN_FUNCTIONS=0',
	'-DVMA_DYNAMIC_VULKAN_FUNCTIONS=0',
	]

if debug_vma
	vma_defns += [
		'-DVMA_DEBUG_MARGIN=16',
		'-DVMA_DEBUG_DETECT_CORRUPTION=1',
		'-DVMA_DEBUG_MIN_BUFFER_IMAGE_GRANULARITY=256',
	]
endif
libvma = static_library('vma', sources: 'vma.cc', cpp_args: vma_defns, include_directories: 'VulkanMemoryAllocator/include', dependencies: [vulkan, volk])
vma = declare_dependency(compile_args: vma_defns, include_directories: 'VulkanMemoryAllocator/include', link_with: libvma)

libspirv_reflect = static_library('spirv_reflect', sources: 'SPIRV-Reflect/spirv_reflect.c', dependencies: vulkan)
spirv_reflect = declare_dependency(include_directories: 'SPIRV-Reflect', link_with: libspirv_reflect, dependencies: vulkan)

endif
