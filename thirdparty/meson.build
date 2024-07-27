if vulkan.found()

libvolk = static_library('volk', sources: 'volk/volk.c', c_args: ['-DVK_NO_PROTOTYPES'], dependencies: vulkan)
volk = declare_dependency(compile_args: ['-DVK_NO_PROTOTYPES'], include_directories: 'volk', link_with: libvolk, dependencies: vulkan)

vma_defns = [
	'-DVMA_STATIC_VULKAN_FUNCTIONS=0',
	'-DVMA_DYNAMIC_VULKAN_FUNCTIONS=0',
]
libvma = static_library('vma', sources: 'vma.cc', cpp_args: vma_defns, include_directories: 'VulkanMemoryAllocator/include', dependencies: [vulkan, volk])
vma = declare_dependency(compile_args: vma_defns, include_directories: 'VulkanMemoryAllocator/include', link_with: libvma)

libspirv_reflect = static_library('spirv_reflect', sources: 'SPIRV-Reflect/spirv_reflect.c', dependencies: vulkan)
spirv_reflect = declare_dependency(include_directories: 'SPIRV-Reflect', link_with: libspirv_reflect, dependencies: vulkan)

endif
