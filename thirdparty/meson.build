if vulkan.found()

libvolk = static_library('volk', sources: 'volk/volk.c', c_args: ['-DVK_NO_PROTOTYPES'], dependencies: vulkan)
volk = declare_dependency(compile_args: ['-DVK_NO_PROTOTYPES'], include_directories: 'volk', link_with: libvolk, dependencies: vulkan)

libvma = static_library('vma', sources: 'vma.cc', include_directories: 'VulkanMemoryAllocator/include', dependencies: [vulkan, volk])
vma = declare_dependency(include_directories: 'VulkanMemoryAllocator/include', link_with: libvma)

libspirv_reflect = static_library('spirv_reflect', sources: 'SPIRV-Reflect/spirv_reflect.c', dependencies: vulkan)
spirv_reflect = declare_dependency(include_directories: 'SPIRV-Reflect', link_with: libspirv_reflect, dependencies: vulkan)

endif
