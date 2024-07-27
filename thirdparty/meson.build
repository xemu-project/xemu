if vulkan.found()

libvma = static_library('vma', sources: 'vma.cc', include_directories: 'VulkanMemoryAllocator/include', dependencies: vulkan)
vma = declare_dependency(include_directories: 'VulkanMemoryAllocator/include', link_with: libvma)

libvolk = static_library('volk', sources: 'volk/volk.c', dependencies: vulkan)
volk = declare_dependency(include_directories: 'volk', link_with: libvolk, dependencies: vulkan)

libspirv_reflect = static_library('spirv_reflect', sources: 'SPIRV-Reflect/spirv_reflect.c', dependencies: vulkan)
spirv_reflect = declare_dependency(include_directories: 'SPIRV-Reflect', link_with: libspirv_reflect, dependencies: vulkan)

endif
