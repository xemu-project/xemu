The directory where this file exists is Vulkan Memory Allocator (VMA) - a software library implemented in C++, with a C interface, intended for developers who use Vulkan API. It is distributed under the MIT license.

# Requirements

Using VMA requires a C++ compiler and a Vulkan SDK or Vulkan headers available. The public API is C-compatible. The implementation translation unit must be compiled as C++. VMA requires at least C++14 for the implementation.

It supports any platform that has a C++ compiler and Vulkan API available (including but not limited to: Windows, Linux, Android) and any Vulkan-compatible GPU (whether a discrete or integrated PC GPU or a mobile SoC).

# Scope

Using this library is not required for using Vulkan, but it is helpful and recommended, as it simplifies some aspects of the Vulkan API: allocating device memory, creating buffers and images. Specifically, what the library does is:

1. It automatically selects the correct and optimal memory type among the ones available on the current GPU, based on the intended usage flags of the buffer/image, memory requirements that Vulkan returns for it, and parameters of the allocation to be created.
2. It allocates large blocks of `VkDeviceMemory` and implements an allocation algorithm to manage parts of them, to be assigned to individual buffers/images.
3. It provides a simple API (with the most important functions being `vmaCreateBuffer`, `vmaCreateImage`) that internally does the following things (so you don't need to directly call these `vk*` functions mentioned below):
    - Creates the new resource (buffer or image) using functions like `vkCreateBuffer`, `vkCreateImage`.
    - Queries for its memory requirements using a function like `vkGetBufferMemoryRequirements`, `vkGetImageMemoryRequirements` or similar.
    - Allocates a new `VkDeviceMemory` block using `vkAllocateMemory` function or assigns a region of an existing one, suitable for the resource, while fulfilling the requirements of alignment and size.
    - Binds the resource to the memory using a function like `vkBindBufferMemory`, `vkBindImageMemory`.

# How to use

VMA is distributed in source form. All library code lives in `include/vk_mem_alloc.h`. The file is very long, so don't read it entirely! Focus on specific sections or search for specific symbols.

The library is STB-style single header. It means all its interface and implementation is contained within this one file, but it doesn't mean all C++ functions are `inline`. Instead, it means that the internal implementation needs to be extracted in exactly one .cpp file by defining macro `VMA_IMPLEMENTATION` before the include.

Other C++ source/header files you can see in this repository are sample, support, and test code, so you don't need to use them when developing software using this library.

The `include/vk_mem_alloc.h` file consists of the following sections:

1. From the beginning until the line `#ifdef VMA_IMPLEMENTATION`: Public interface (API) of the library, which has the form of a C header file with all the public enums, structures, functions.
    - Accompanying comments before every symbol, in Doxygen format, are complete documentation of the API.
    - You may need to analyze pieces of this section to understand how to correctly use the library.
2. From the line `#ifdef VMA_IMPLEMENTATION` to the line `#endif // VMA_IMPLEMENTATION`: Internal implementation of the library in C++.
    - You typically don't need to analyze it when using the library. You can use it only when you develop the library itself or when you need to know its internal implementation beyond what the API and its documentation provides.
3. From the line `#endif // VMA_IMPLEMENTATION` until the end: A long comment in Doxygen format providing a generic documentation of the library, with high-level overview chapters, FAQ, and code examples.
    - You may use this section if you need to learn more generic information about the library or find some example code with correct and recommended usage of its functions.

# Library API

The API of VMA is similar in style to Vulkan.

Errors are reported using `VkResult` error codes returned from most functions, with `VK_SUCCESS` meaning success.

Types that represent objects are opaque pointers or handles. Most important types of objects are:

- `VmaAllocator` - represents the main allocator object. Creating only one per `=` and keeping it alive for the whole time when using Vulkan is recommended.
- `VmaAllocation` - represents a specific region of allocated Vulkan memory - a dedicated `VkDeviceMemory` block or a piece of a larger block. It may or may not be bound to a specific buffer/image. That memory block is managed internally by VMA.

Example code:

```
// 1. Fill in Create structures:
VkBufferCreateInfo bufferCreateInfo = ...
 
VmaAllocationCreateInfo allocCreateInfo = {};
allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
allocCreateInfo.flags = 0;

// 2. Create two objects: buffer and its allocation:
VkBuffer buffer = VK_NULL_HANDLE;
VmaAllocation allocation = VK_NULL_HANDLE;
VkResult res = vmaCreateBuffer(allocator, &bufferCreateInfo, &allocCreateInfo, &buffer, &allocation, nullptr);
if(res != VK_SUCCESS) { ... }

// 3. Using buffer or allocation here, e.g.:
VkMemoryPropertyFlags memPropFlags = 0;
vmaGetAllocationMemoryProperties(allocator, allocation, &memPropFlags);
// Using memPropFlags...

// 4. At the end, destroy the buffer and the allocation:
vmaDestroyBuffer(allocator, buffer, allocation);
```

The pattern for creating an allocation object, as well as other library objects, looks like in Vulkan:

1. Fill `Vma*CreateInfo` structure.
    - ALWAYS fully initialize such structure with zeros before filling selected members. Don't rely on the number or sizes of members - new ones may be added in future versions!
2. Call `vmaCreate*` function.
    - Check the result of this function (for `VK_SUCCESS`) to make sure the creation succeeded. Handle potential failure in a way consistent with the surrounding code of the project you develop.
    - If succeeded, it returns via the last parameter a newly created `Vma*` object handle. You receive ownership of it and you are responsible for destroying it when no longer needed!
3. Use the object, passing it to other functions as needed, like `vmaGetAllocationInfo`, `vmaGetAllocationMemoryProperties`.
    - DO NOT EVER access members of such object directly, like `allocation->m_Size`, `allocation->m_MemoryTypeIndex`, even if it would compile and work! This would be a violation of the library interface and may not compile in future versions of VMA. Treat them as opaque handles and only pass them as arguments to the library functions to read their parameters or manipulate them.
4. When no longer needed, destroy the object using the corresponding function `vmaDestroy*`.
    - Before destroying the object, make sure it is no longer used or needed, on both CPU and GPU. You need to be sure GPU finished executing Vulkan commands that may use the buffer/image/memory, e.g. by waiting on the appropriate `VkFence`.

Working with images follows the same workflow as in the example shown above for buffers, just use `vmaCreateImage`, `vmaDestroyImage`, and other image-related rather than buffer-related structures and functions.

# Allocation parameters

When filling `VmaAllocationCreateInfo` members, using `allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO` is recommended in most cases, as it makes VMA automatically choose the best memory type among those available on the current GPU and compatible with the buffer/image created.

When the memory is going to be mapped and one of the `VMA_MEMORY_USAGE_AUTO*` values is used, you need to also set `allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT` or `allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT`.

- Use `VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT` when you only write to the mapped memory (sequential number-by-number or as a destination of `memcpy`), e.g. when using a buffer intended for uploading data from CPU to GPU.
    - The memory assigned to such allocation may be uncached and write-combined, which means any reads and scattered accesses may be extremely slow! DO NOT EVER read from such mapped pointer, even implicitly, e.g. when doing `pMappedPointer[i] += x`.
- Use `VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT` when you need to read from the mapped memory, e.g. when doing a readback of some data downloaded from GPU to CPU.
