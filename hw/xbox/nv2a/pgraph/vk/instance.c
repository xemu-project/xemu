/*
 * Geforce NV2A PGRAPH Vulkan Renderer
 *
 * Copyright (c) 2024 Matt Borgerson
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "ui/xemu-settings.h"
#include "renderer.h"
#include "xemu-version.h"

#include <SDL.h>
#include <SDL_syswm.h>
#include <SDL_vulkan.h>

#include <volk.h>

#define VkExtensionPropertiesArray GArray
#define StringArray GArray

static bool enable_validation = false;

static char const *const validation_layers[] = {
    "VK_LAYER_KHRONOS_validation",
};

static char const *const required_instance_extensions[] = {
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
    VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
};

static char const *const required_device_extensions[] = {
    VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
    VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
#ifdef WIN32
    VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,
#else
    VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
#endif
};

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
{
    fprintf(stderr, "[vk] %s\n", pCallbackData->pMessage);

    if ((messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) &&
        (messageSeverity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT))) {
        assert(!g_config.display.vulkan.assert_on_validation_msg);
    }
    return VK_FALSE;
}

static bool check_validation_layer_support(void)
{
    uint32_t num_available_layers;
    vkEnumerateInstanceLayerProperties(&num_available_layers, NULL);

    g_autofree VkLayerProperties *available_layers =
        g_malloc_n(num_available_layers, sizeof(VkLayerProperties));
    vkEnumerateInstanceLayerProperties(&num_available_layers, available_layers);

    for (int i = 0; i < ARRAY_SIZE(validation_layers); i++) {
        bool found = false;
        for (int j = 0; j < num_available_layers; j++) {
            if (!strcmp(validation_layers[i], available_layers[j].layerName)) {
                found = true;
                break;
            }
        }
        if (!found) {
            fprintf(stderr, "desired validation layer not found: %s\n",
                    validation_layers[i]);
            return false;
        }
    }

    return true;
}

static void create_window(PGRAPHVkState *r, Error **errp)
{
    r->window = SDL_CreateWindow(
        "SDL Offscreen Window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        640, 480, SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN);

    if (r->window == NULL) {
        error_setg(errp, "SDL_CreateWindow failed: %s", SDL_GetError());
    }
}

static void destroy_window(PGRAPHVkState *r)
{
    if (r->window) {
        SDL_DestroyWindow(r->window);
        r->window = NULL;
    }
}

static VkExtensionPropertiesArray *
get_available_instance_extensions(PGRAPHState *pg)
{
    uint32_t num_extensions = 0;

    VK_CHECK(
        vkEnumerateInstanceExtensionProperties(NULL, &num_extensions, NULL));

    VkExtensionPropertiesArray *extensions = g_array_sized_new(
        FALSE, FALSE, sizeof(VkExtensionProperties), num_extensions);

    g_array_set_size(extensions, num_extensions);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(
        NULL, &num_extensions, (VkExtensionProperties *)extensions->data));

    return extensions;
}

static bool
is_extension_available(VkExtensionPropertiesArray *available_extensions,
                       const char *extension_name)
{
    for (int i = 0; i < available_extensions->len; i++) {
        VkExtensionProperties *e =
            &g_array_index(available_extensions, VkExtensionProperties, i);
        if (!strcmp(e->extensionName, extension_name)) {
            return true;
        }
    }

    return false;
}

static StringArray *get_required_instance_extension_names(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    // Add instance extensions SDL lists as required
    unsigned int sdl_count = 0;
    SDL_Vulkan_GetInstanceExtensions((SDL_Window *)r->window, &sdl_count, NULL);

    StringArray *extensions =
        g_array_sized_new(FALSE, FALSE, sizeof(char *),
                          sdl_count + ARRAY_SIZE(required_instance_extensions));

    if (sdl_count) {
        g_array_set_size(extensions, sdl_count);
        SDL_Vulkan_GetInstanceExtensions((SDL_Window *)r->window, &sdl_count,
                                         (const char **)extensions->data);
    }

    // Add additional required extensions
    g_array_append_vals(extensions, required_instance_extensions,
                        ARRAY_SIZE(required_instance_extensions));

    return extensions;
}

static bool
add_extension_if_available(VkExtensionPropertiesArray *available_extensions,
                           StringArray *enabled_extension_names,
                           const char *desired_extension_name)
{
    if (is_extension_available(available_extensions, desired_extension_name)) {
        g_array_append_val(enabled_extension_names, desired_extension_name);
        return true;
    }

    fprintf(stderr, "Warning: extension not available: %s\n",
            desired_extension_name);
    return false;
}

static void
add_optional_instance_extension_names(PGRAPHState *pg,
                                      VkExtensionPropertiesArray *available_extensions,
                                      StringArray *enabled_extension_names)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    r->debug_utils_extension_enabled =
        g_config.display.vulkan.validation_layers &&
        add_extension_if_available(available_extensions, enabled_extension_names,
                                   VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
}

static bool create_instance(PGRAPHState *pg, Error **errp)
{
    PGRAPHVkState *r = pg->vk_renderer_state;
    VkResult result;

    create_window(r, errp);
    if (*errp) {
        return false;
    }

    result = volkInitialize();
    if (result != VK_SUCCESS) {
        error_setg(errp, "volkInitialize failed");
        destroy_window(r);
        return false;
    }

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "xemu",
        .applicationVersion = VK_MAKE_VERSION(
            xemu_version_major, xemu_version_minor, xemu_version_patch),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };

    g_autoptr(VkExtensionPropertiesArray) available_extensions =
        get_available_instance_extensions(pg);

    g_autoptr(StringArray) enabled_extension_names =
        get_required_instance_extension_names(pg);

    bool all_required_extensions_available = true;
    for (int i = 0; i < enabled_extension_names->len; i++) {
        const char *required_extension =
            g_array_index(enabled_extension_names, const char *, i);
        if (!is_extension_available(available_extensions, required_extension)) {
            fprintf(stderr,
                    "Error: Required instance extension not available: %s\n",
                    required_extension);
            all_required_extensions_available = false;
        }
    }

    if (!all_required_extensions_available) {
        error_setg(errp, "Required instance extensions not available");
        goto error;
    }

    add_optional_instance_extension_names(pg, available_extensions,
                                          enabled_extension_names);

    fprintf(stderr, "Enabled instance extensions:\n");
    for (int i = 0; i < enabled_extension_names->len; i++) {
        fprintf(stderr, "- %s\n",
                g_array_index(enabled_extension_names, char *, i));
    }

    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = enabled_extension_names->len,
        .ppEnabledExtensionNames =
            &g_array_index(enabled_extension_names, const char *, 0),
    };

    enable_validation = g_config.display.vulkan.validation_layers;

    VkValidationFeatureEnableEXT enables[] = {
        VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
        // VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
    };

    VkValidationFeaturesEXT validationFeatures = {
        .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
        .enabledValidationFeatureCount = ARRAY_SIZE(enables),
        .pEnabledValidationFeatures = enables,
    };

    if (enable_validation) {
        if (check_validation_layer_support()) {
            fprintf(stderr, "Warning: Validation layers enabled. Expect "
                            "performance impact.\n");
            create_info.enabledLayerCount = ARRAY_SIZE(validation_layers);
            create_info.ppEnabledLayerNames = validation_layers;
            create_info.pNext = &validationFeatures;
        } else {
            fprintf(stderr, "Warning: validation layers not available\n");
            enable_validation = false;
        }
    }

    result = vkCreateInstance(&create_info, NULL, &r->instance);
    if (result != VK_SUCCESS) {
        error_setg(errp, "Failed to create instance (%d)", result);
        return false;
    }

    volkLoadInstance(r->instance);

    if (r->debug_utils_extension_enabled) {
        VkDebugUtilsMessengerCreateInfoEXT messenger_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = debugCallback,
        };
        VK_CHECK(vkCreateDebugUtilsMessengerEXT(r->instance, &messenger_info,
                                                NULL, &r->debug_messenger));
    }

    return true;

error:
    volkFinalize();
    destroy_window(r);
    return false;
}

static bool is_queue_family_indicies_complete(QueueFamilyIndices indices)
{
    return indices.queue_family >= 0;
}

QueueFamilyIndices pgraph_vk_find_queue_families(VkPhysicalDevice device)
{
    QueueFamilyIndices indices = {
        .queue_family = -1,
    };

    uint32_t num_queue_families = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &num_queue_families, NULL);

    g_autofree VkQueueFamilyProperties *queue_families =
        g_malloc_n(num_queue_families, sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(device, &num_queue_families,
                                             queue_families);

    for (int i = 0; i < num_queue_families; i++) {
        VkQueueFamilyProperties queueFamily = queue_families[i];
        // FIXME: Support independent graphics, compute queues
        int required_flags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
        if ((queueFamily.queueFlags & required_flags) == required_flags) {
            indices.queue_family = i;
        }
        if (is_queue_family_indicies_complete(indices)) {
            break;
        }
    }

    return indices;
}

static VkExtensionPropertiesArray *
get_available_device_extensions(VkPhysicalDevice device)
{
    uint32_t num_extensions = 0;

    VK_CHECK(vkEnumerateDeviceExtensionProperties(device, NULL, &num_extensions,
                                                  NULL));

    VkExtensionPropertiesArray *extensions = g_array_sized_new(
        FALSE, FALSE, sizeof(VkExtensionProperties), num_extensions);

    g_array_set_size(extensions, num_extensions);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(
        device, NULL, &num_extensions,
        (VkExtensionProperties *)extensions->data));

    return extensions;
}

static StringArray *get_required_device_extension_names(void)
{
    StringArray *extensions =
        g_array_sized_new(FALSE, FALSE, sizeof(char *),
                          ARRAY_SIZE(required_device_extensions));

    g_array_append_vals(extensions, required_device_extensions,
                        ARRAY_SIZE(required_device_extensions));

    return extensions;
}

static void add_optional_device_extension_names(
    PGRAPHState *pg, VkExtensionPropertiesArray *available_extensions,
    StringArray *enabled_extension_names)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    r->custom_border_color_extension_enabled =
        add_extension_if_available(available_extensions, enabled_extension_names,
                                   VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME);

    r->provoking_vertex_extension_enabled =
        add_extension_if_available(available_extensions, enabled_extension_names,
                                   VK_EXT_PROVOKING_VERTEX_EXTENSION_NAME);

    r->memory_budget_extension_enabled = add_extension_if_available(
        available_extensions, enabled_extension_names,
        VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
}

static bool check_device_support_required_extensions(VkPhysicalDevice device)
{
    g_autoptr(VkExtensionPropertiesArray) available_extensions =
        get_available_device_extensions(device);

    for (int i = 0; i < ARRAY_SIZE(required_device_extensions); i++) {
        if (!is_extension_available(available_extensions,
                                    required_device_extensions[i])) {
            fprintf(stderr, "required device extension not found: %s\n",
                    required_device_extensions[i]);
            return false;
        }
    }

    return true;
}

static bool is_device_compatible(VkPhysicalDevice device)
{
    QueueFamilyIndices indices = pgraph_vk_find_queue_families(device);

    return is_queue_family_indicies_complete(indices) &&
           check_device_support_required_extensions(device);
    // FIXME: Check formats
    // FIXME: Check vram
}

static bool select_physical_device(PGRAPHState *pg, Error **errp)
{
    PGRAPHVkState *r = pg->vk_renderer_state;
    VkResult result;

    uint32_t num_physical_devices = 0;

    result =
        vkEnumeratePhysicalDevices(r->instance, &num_physical_devices, NULL);
    if (result != VK_SUCCESS || num_physical_devices == 0) {
        error_setg(errp, "Failed to find GPUs with Vulkan support");
        return false;
    }

    g_autofree VkPhysicalDevice *devices =
        g_malloc_n(num_physical_devices, sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(r->instance, &num_physical_devices, devices);

    fprintf(stderr, "Available physical devices:\n");
    for (int i = 0; i < num_physical_devices; i++) {
        vkGetPhysicalDeviceProperties(devices[i], &r->device_props);
        fprintf(stderr, "- %s\n", r->device_props.deviceName);
    }

    // FIXME: Store preferred device

    r->physical_device = VK_NULL_HANDLE;
    for (int i = 0; i < num_physical_devices; i++) {
        if (is_device_compatible(devices[i])) {
            r->physical_device = devices[i];
            break;
        }
    }
    if (r->physical_device == VK_NULL_HANDLE) {
        error_setg(errp, "Failed to find a suitable GPU");
        return false;
    }

    vkGetPhysicalDeviceProperties(r->physical_device, &r->device_props);
    fprintf(stderr,
            "Selected physical device: %s\n"
            "- Vendor: %x, Device: %x\n"
            "- Driver Version: %d.%d.%d\n",
            r->device_props.deviceName,
            r->device_props.vendorID,
            r->device_props.deviceID,
            VK_VERSION_MAJOR(r->device_props.driverVersion),
            VK_VERSION_MINOR(r->device_props.driverVersion),
            VK_VERSION_PATCH(r->device_props.driverVersion));

    return true;
}

static bool create_logical_device(PGRAPHState *pg, Error **errp)
{
    PGRAPHVkState *r = pg->vk_renderer_state;
    VkResult result;

    QueueFamilyIndices indices =
        pgraph_vk_find_queue_families(r->physical_device);

    g_autoptr(VkExtensionPropertiesArray) available_extensions =
        get_available_device_extensions(r->physical_device);

    g_autoptr(StringArray) enabled_extension_names =
        get_required_device_extension_names();

    add_optional_device_extension_names(pg, available_extensions,
                                        enabled_extension_names);

    fprintf(stderr, "Enabled device extensions:\n");
    for (int i = 0; i < enabled_extension_names->len; i++) {
        fprintf(stderr, "- %s\n",
                g_array_index(enabled_extension_names, char *, i));
    }

    float queuePriority = 1.0f;

    VkDeviceQueueCreateInfo queue_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = indices.queue_family,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };

    // Ensure device supports required features
    VkPhysicalDeviceFeatures available_features, enabled_features;
    vkGetPhysicalDeviceFeatures(r->physical_device, &available_features);
    memset(&enabled_features, 0, sizeof(enabled_features));

    struct {
        const char *name;
        VkBool32 available, *enabled;
    } required_features[] = {
        #define F(n) { #n, available_features.n, &enabled_features.n }
        F(shaderClipDistance),
        F(geometryShader),
        F(shaderTessellationAndGeometryPointSize),
        F(depthClamp),
        F(occlusionQueryPrecise),
        #undef F
    };

    bool all_features_available = true;
    for (int i = 0; i < ARRAY_SIZE(required_features); i++) {
        if (required_features[i].available != VK_TRUE) {
            fprintf(stderr,
                    "Error: Device does not support required feature %s\n",
                    required_features[i].name);
            all_features_available = false;
        }
        *required_features[i].enabled = VK_TRUE;
    }

    if (!all_features_available) {
        error_setg(errp, "Device does not support required features");
        return false;
    }

    void *next_struct = NULL;

    VkPhysicalDeviceProvokingVertexFeaturesEXT provoking_vertex_features;
    if (r->provoking_vertex_extension_enabled) {
        provoking_vertex_features = (VkPhysicalDeviceProvokingVertexFeaturesEXT){
            .sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_FEATURES_EXT,
            .provokingVertexLast = VK_TRUE,
            .pNext = next_struct,
        };
        next_struct = &provoking_vertex_features;
    }

    VkPhysicalDeviceCustomBorderColorFeaturesEXT custom_border_features;
    if (r->custom_border_color_extension_enabled) {
        custom_border_features = (VkPhysicalDeviceCustomBorderColorFeaturesEXT){
            .sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT,
            .customBorderColors = VK_TRUE,
            .pNext = next_struct,
        };
        next_struct = &custom_border_features;
    }

    VkDeviceCreateInfo device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_create_info,
        .pEnabledFeatures = &enabled_features,
        .enabledExtensionCount = enabled_extension_names->len,
        .ppEnabledExtensionNames =
            &g_array_index(enabled_extension_names, const char *, 0),
        .pNext = next_struct,
    };

    if (enable_validation) {
        device_create_info.enabledLayerCount = ARRAY_SIZE(validation_layers);
        device_create_info.ppEnabledLayerNames = validation_layers;
    }

    result = vkCreateDevice(r->physical_device, &device_create_info, NULL,
                            &r->device);
    if (result != VK_SUCCESS) {
        error_setg(errp, "Failed to create logical device (%d)", result);
        return false;
    }

    vkGetDeviceQueue(r->device, indices.queue_family, 0, &r->queue);
    return true;
}

uint32_t pgraph_vk_get_memory_type(PGRAPHState *pg, uint32_t type_bits,
                                   VkMemoryPropertyFlags properties)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    VkPhysicalDeviceMemoryProperties prop;
    vkGetPhysicalDeviceMemoryProperties(r->physical_device, &prop);
    for (uint32_t i = 0; i < prop.memoryTypeCount; i++) {
        if ((prop.memoryTypes[i].propertyFlags & properties) == properties &&
            type_bits & (1 << i)) {
            return i;
        }
    }
    return 0xFFFFFFFF; // Unable to find memoryType
}

static bool init_allocator(PGRAPHState *pg, Error **errp)
{
    PGRAPHVkState *r = pg->vk_renderer_state;
    VkResult result;

    VmaVulkanFunctions vulkanFunctions = {
        /// Required when using VMA_DYNAMIC_VULKAN_FUNCTIONS.
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        /// Required when using VMA_DYNAMIC_VULKAN_FUNCTIONS.
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
        .vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties,
        .vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
        .vkAllocateMemory = vkAllocateMemory,
        .vkFreeMemory = vkFreeMemory,
        .vkMapMemory = vkMapMemory,
        .vkUnmapMemory = vkUnmapMemory,
        .vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges,
        .vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges,
        .vkBindBufferMemory = vkBindBufferMemory,
        .vkBindImageMemory = vkBindImageMemory,
        .vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements,
        .vkGetImageMemoryRequirements = vkGetImageMemoryRequirements,
        .vkCreateBuffer = vkCreateBuffer,
        .vkDestroyBuffer = vkDestroyBuffer,
        .vkCreateImage = vkCreateImage,
        .vkDestroyImage = vkDestroyImage,
        .vkCmdCopyBuffer = vkCmdCopyBuffer,
    #if VMA_DEDICATED_ALLOCATION || VMA_VULKAN_VERSION >= 1001000
        /// Fetch "vkGetBufferMemoryRequirements2" on Vulkan >= 1.1, fetch "vkGetBufferMemoryRequirements2KHR" when using VK_KHR_dedicated_allocation extension.
        .vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2,
        /// Fetch "vkGetImageMemoryRequirements2" on Vulkan >= 1.1, fetch "vkGetImageMemoryRequirements2KHR" when using VK_KHR_dedicated_allocation extension.
        .vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2,
    #endif
    #if VMA_BIND_MEMORY2 || VMA_VULKAN_VERSION >= 1001000
        /// Fetch "vkBindBufferMemory2" on Vulkan >= 1.1, fetch "vkBindBufferMemory2KHR" when using VK_KHR_bind_memory2 extension.
        .vkBindBufferMemory2KHR = vkBindBufferMemory2,
        /// Fetch "vkBindImageMemory2" on Vulkan >= 1.1, fetch "vkBindImageMemory2KHR" when using VK_KHR_bind_memory2 extension.
        .vkBindImageMemory2KHR = vkBindImageMemory2,
    #endif
    #if VMA_MEMORY_BUDGET || VMA_VULKAN_VERSION >= 1001000
        /// Fetch from "vkGetPhysicalDeviceMemoryProperties2" on Vulkan >= 1.1, but you can also fetch it from "vkGetPhysicalDeviceMemoryProperties2KHR" if you enabled extension VK_KHR_get_physical_device_properties2.
        .vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2KHR,
    #endif
    #if VMA_KHR_MAINTENANCE4 || VMA_VULKAN_VERSION >= 1003000
        /// Fetch from "vkGetDeviceBufferMemoryRequirements" on Vulkan >= 1.3, but you can also fetch it from "vkGetDeviceBufferMemoryRequirementsKHR" if you enabled extension VK_KHR_maintenance4.
        .vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements,
        /// Fetch from "vkGetDeviceImageMemoryRequirements" on Vulkan >= 1.3, but you can also fetch it from "vkGetDeviceImageMemoryRequirementsKHR" if you enabled extension VK_KHR_maintenance4.
        .vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements,
    #endif
    };

    VmaAllocatorCreateInfo create_info = {
        .flags = (r->memory_budget_extension_enabled ?
                      VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT :
                      0),
        .vulkanApiVersion = VK_API_VERSION_1_3,
        .instance = r->instance,
        .physicalDevice = r->physical_device,
        .device = r->device,
        .pVulkanFunctions = &vulkanFunctions,
    };

    result = vmaCreateAllocator(&create_info, &r->allocator);
    if (result != VK_SUCCESS) {
        error_setg(errp, "vmaCreateAllocator failed");
        return false;
    }

    return true;
}

void pgraph_vk_init_instance(PGRAPHState *pg, Error **errp)
{
    if (create_instance(pg, errp) &&
        select_physical_device(pg, errp) &&
        create_logical_device(pg, errp) &&
        init_allocator(pg, errp)) {
        return;
    }

    pgraph_vk_finalize_instance(pg);

    const char *msg = "Failed to initialize Vulkan renderer";
    if (*errp) {
        error_prepend(errp, "%s: ", msg);
    } else {
        error_setg(errp, "%s", msg);
    }
}

void pgraph_vk_finalize_instance(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    if (r->allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(r->allocator);
        r->allocator = VK_NULL_HANDLE;
    }

    if (r->device != VK_NULL_HANDLE) {
        vkDestroyDevice(r->device, NULL);
        r->device = VK_NULL_HANDLE;
    }

    if (r->debug_messenger != VK_NULL_HANDLE) {
        vkDestroyDebugUtilsMessengerEXT(r->instance, r->debug_messenger, NULL);
        r->debug_messenger = VK_NULL_HANDLE;
    }

    if (r->instance != VK_NULL_HANDLE) {
        vkDestroyInstance(r->instance, NULL);
        r->instance = VK_NULL_HANDLE;
    }

    volkFinalize();
    destroy_window(r);
}
