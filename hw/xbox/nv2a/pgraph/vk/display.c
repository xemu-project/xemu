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

#include "renderer.h"

static const char *display_frag_glsl =
    "#version 450\n"
    "layout(binding = 0) uniform sampler2D tex;\n"
    "layout(binding = 1) uniform sampler2D pvideo_tex;\n"
    "layout(push_constant, std430) uniform PushConstants {\n"
    "    bool pvideo_enable;\n"
    "    vec2 pvideo_in_pos;\n"
    "    vec4 pvideo_pos;\n"
    "    vec3 pvideo_scale;\n"
    "    bool pvideo_color_key_enable;\n"
    "    vec2 display_size;\n"
    "    float line_offset;\n"
    "    vec4 pvideo_color_key;\n"
    "};\n"
    "layout(location = 0) out vec4 out_Color;\n"
    "void main()\n"
    "{\n"
    "    vec2 texCoord = gl_FragCoord.xy/display_size;\n"
    "    texCoord.y = 1 - texCoord.y;\n" // GL compat
    "    float rel = display_size.y/textureSize(tex, 0).y/line_offset;\n"
    "    texCoord.y = 1 + rel*(texCoord.y - 1);"
    "    out_Color.rgba = texture(tex, texCoord);\n"
    // "    if (pvideo_enable) {\n"
    // "        vec2 screenCoord = gl_FragCoord.xy - 0.5;\n"
    // "        vec4 output_region = vec4(pvideo_pos.xy, pvideo_pos.xy + pvideo_pos.zw);\n"
    // "        bvec4 clip = bvec4(lessThan(screenCoord, output_region.xy),\n"
    // "                           greaterThan(screenCoord, output_region.zw));\n"
    // "        if (!any(clip) && (!pvideo_color_key_enable || out_Color.rgba == pvideo_color_key)) {\n"
    // "            vec2 out_xy = (screenCoord - pvideo_pos.xy) * pvideo_scale.z;\n"
    // "            vec2 in_st = (pvideo_in_pos + out_xy * pvideo_scale.xy) / textureSize(pvideo_tex, 0);\n"
    // "            in_st.y *= -1.0;\n"
    // "            out_Color.rgba = texture(pvideo_tex, in_st);\n"
    // "        }\n"
    // "    }\n"
    "}\n";

static void create_descriptor_pool(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    VkDescriptorPoolSize pool_sizes = {
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 2,
    };

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_sizes,
        .maxSets = 1,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
    };
    VK_CHECK(vkCreateDescriptorPool(r->device, &pool_info, NULL,
                                    &r->display.descriptor_pool));
}

static void destroy_descriptor_pool(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    vkDestroyDescriptorPool(r->device, r->display.descriptor_pool, NULL);
    r->display.descriptor_pool = VK_NULL_HANDLE;
}

static void create_descriptor_set_layout(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    VkDescriptorSetLayoutBinding bindings[2];

    for (int i = 0; i < ARRAY_SIZE(bindings); i++) {
        bindings[i] = (VkDescriptorSetLayoutBinding){
            .binding = i,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        };
    }
    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = ARRAY_SIZE(bindings),
        .pBindings = bindings,
    };
    VK_CHECK(vkCreateDescriptorSetLayout(r->device, &layout_info, NULL,
                                         &r->display.descriptor_set_layout));
}

static void destroy_descriptor_set_layout(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    vkDestroyDescriptorSetLayout(r->device, r->display.descriptor_set_layout,
                                 NULL);
    r->display.descriptor_set_layout = VK_NULL_HANDLE;
}

static void create_descriptor_sets(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    VkDescriptorSetLayout layout = r->display.descriptor_set_layout;

    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = r->display.descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };
    VK_CHECK(vkAllocateDescriptorSets(r->device, &alloc_info,
                                      &r->display.descriptor_set));
}

static void create_render_pass(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    VkAttachmentDescription attachment;

    VkAttachmentReference color_reference;
    attachment = (VkAttachmentDescription){
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    color_reference = (VkAttachmentReference){
        0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
    };

    dependency.srcStageMask |=
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask |=
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_reference,
    };

    VkRenderPassCreateInfo renderpass_create_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };
    VK_CHECK(vkCreateRenderPass(r->device, &renderpass_create_info, NULL,
                                &r->display.render_pass));
}

static void destroy_render_pass(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;
    vkDestroyRenderPass(r->device, r->display.render_pass, NULL);
    r->display.render_pass = VK_NULL_HANDLE;
}

static void create_display_pipeline(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    r->display.display_frag =
        pgraph_vk_create_shader_module_from_glsl(
            r, VK_SHADER_STAGE_FRAGMENT_BIT, display_frag_glsl);

    VkPipelineShaderStageCreateInfo shader_stages[] = {
        (VkPipelineShaderStageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = r->quad_vert_module->module,
            .pName = "main",
        },
        (VkPipelineShaderStageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = r->display.display_frag->module,
            .pName = "main",
        },
     };

    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_FALSE,
        .depthCompareOp = VK_COMPARE_OP_ALWAYS,
        .depthBoundsTestEnable = VK_FALSE,
    };

    VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_FALSE,
    };

    VkPipelineColorBlendStateCreateInfo color_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment,
    };

    VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT,
                                        VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamic_states,
    };

    VkPushConstantRange push_constant_range = {
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = r->display.display_frag->push_constants.total_size,
    };

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &r->display.descriptor_set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant_range,
    };
    VK_CHECK(vkCreatePipelineLayout(r->device, &pipeline_layout_info, NULL,
                                    &r->display.pipeline_layout));

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = ARRAY_SIZE(shader_stages),
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = r->zeta_binding ? &depth_stencil : NULL,
        .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_state,
        .layout = r->display.pipeline_layout,
        .renderPass = r->display.render_pass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
    };
    VK_CHECK(vkCreateGraphicsPipelines(r->device, r->vk_pipeline_cache, 1,
                                       &pipeline_info, NULL,
                                       &r->display.pipeline));
}

static void destroy_display_pipeline(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    vkDestroyPipeline(r->device, r->display.pipeline, NULL);
    r->display.pipeline = VK_NULL_HANDLE;
}

static void create_frame_buffer(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    VkFramebufferCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = r->display.render_pass,
        .attachmentCount = 1,
        .pAttachments = &r->display.image_view,
        .width = r->display.width,
        .height = r->display.height,
        .layers = 1,
    };
    VK_CHECK(vkCreateFramebuffer(r->device, &create_info, NULL,
                                 &r->display.framebuffer));
}

static void destroy_frame_buffer(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;
    vkDestroyFramebuffer(r->device, r->display.framebuffer, NULL);
    r->display.framebuffer = NULL;
}

static void destroy_current_display_image(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;
    PGRAPHVkDisplayState *d = &r->display;

    if (d->image == VK_NULL_HANDLE) {
        return;
    }

    destroy_frame_buffer(pg);

#if HAVE_EXTERNAL_MEMORY
    glDeleteTextures(1, &d->gl_texture_id);
    d->gl_texture_id = 0;

    glDeleteMemoryObjectsEXT(1, &d->gl_memory_obj);
    d->gl_memory_obj = 0;

#ifdef WIN32
    CloseHandle(d->handle);
    d->handle = 0;
#endif
#endif

    vkDestroyImageView(r->device, d->image_view, NULL);
    d->image_view = VK_NULL_HANDLE;

    vkDestroyImage(r->device, d->image, NULL);
    d->image = VK_NULL_HANDLE;

    vkFreeMemory(r->device, d->memory, NULL);
    d->memory = VK_NULL_HANDLE;

    d->draw_time = 0;
}

// FIXME: We may need to use two images. One for actually rendering display,
// and another for GL in the correct tiling mode

static void create_display_image_from_surface(PGRAPHState *pg,
                                              SurfaceBinding *surface)
{
    PGRAPHVkState *r = pg->vk_renderer_state;
    PGRAPHVkDisplayState *d = &r->display;

    if (r->display.image != VK_NULL_HANDLE) {
        destroy_current_display_image(pg);
    }

    const GLint gl_internal_format = GL_RGBA8;
    bool use_optimal_tiling = true;

#if HAVE_EXTERNAL_MEMORY
    GLint num_tiling_types;
    glGetInternalformativ(GL_TEXTURE_2D, gl_internal_format,
                          GL_NUM_TILING_TYPES_EXT, 1, &num_tiling_types);
    // XXX: Apparently on AMD GL_OPTIMAL_TILING_EXT is reported to be
    // supported, but doesn't work? On nVidia, GL_LINEAR_TILING_EXT may not
    // be supported so we must use optimal. Default to optimal unless
    // linear is explicitly specified...
    GLint tiling_types[num_tiling_types];
    glGetInternalformativ(GL_TEXTURE_2D, gl_internal_format,
                          GL_TILING_TYPES_EXT, num_tiling_types, tiling_types);
    for (int i = 0; i < num_tiling_types; i++) {
        if (tiling_types[i] == GL_LINEAR_TILING_EXT) {
            use_optimal_tiling = false;
            break;
        }
    }
#endif

    // Create image
    VkImageCreateInfo image_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent.width = surface->width,
        .extent.height = surface->height,
        .extent.depth = 1,
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .tiling = use_optimal_tiling ? VK_IMAGE_TILING_OPTIMAL : VK_IMAGE_TILING_LINEAR,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    pgraph_apply_scaling_factor(pg, &image_create_info.extent.width,
                                &image_create_info.extent.height);

    VkExternalMemoryImageCreateInfo external_memory_image_create_info = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR,
    };
    image_create_info.pNext = &external_memory_image_create_info;

    VK_CHECK(vkCreateImage(r->device, &image_create_info, NULL, &d->image));

    // Allocate and bind image memory
    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(r->device, d->image, &memory_requirements);

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memory_requirements.size,
        .memoryTypeIndex =
            pgraph_vk_get_memory_type(pg, memory_requirements.memoryTypeBits,
                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };

    VkExportMemoryAllocateInfo export_memory_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO,
        .handleTypes =
#ifdef WIN32
            VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR
#else
            VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT
#endif
            ,
    };
    alloc_info.pNext = &export_memory_alloc_info;

    VK_CHECK(vkAllocateMemory(r->device, &alloc_info, NULL, &d->memory));

    vkBindImageMemory(r->device, d->image, d->memory, 0);

    // Create Image View
    VkImageViewCreateInfo image_view_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = d->image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = image_create_info.format,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .subresourceRange.levelCount = 1,
        .subresourceRange.layerCount = 1,
    };
    VK_CHECK(vkCreateImageView(r->device, &image_view_create_info, NULL,
                               &d->image_view));

#if HAVE_EXTERNAL_MEMORY

#ifdef WIN32

    VkMemoryGetWin32HandleInfoKHR handle_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
        .memory = d->memory,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR
    };
    VK_CHECK(vkGetMemoryWin32HandleKHR(r->device, &handle_info, &d->handle));

    glCreateMemoryObjectsEXT(1, &d->gl_memory_obj);
    glImportMemoryWin32HandleEXT(d->gl_memory_obj, memory_requirements.size, GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, d->handle);
    assert(glGetError() == GL_NO_ERROR);

#else

    VkMemoryGetFdInfoKHR fd_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
        .memory = d->memory,
        .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
    };
    VK_CHECK(vkGetMemoryFdKHR(r->device, &fd_info, &d->fd));

    glCreateMemoryObjectsEXT(1, &d->gl_memory_obj);
    glImportMemoryFdEXT(d->gl_memory_obj, memory_requirements.size,
                        GL_HANDLE_TYPE_OPAQUE_FD_EXT, d->fd);
    assert(glIsMemoryObjectEXT(d->gl_memory_obj));
    assert(glGetError() == GL_NO_ERROR);

#endif // WIN32
 
    glGenTextures(1, &d->gl_texture_id);
    glBindTexture(GL_TEXTURE_2D, d->gl_texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_TILING_EXT,
                    use_optimal_tiling ? GL_OPTIMAL_TILING_EXT :
                                         GL_LINEAR_TILING_EXT);
    glTexStorageMem2DEXT(GL_TEXTURE_2D, 1, gl_internal_format,
                         image_create_info.extent.width,
                         image_create_info.extent.height, d->gl_memory_obj, 0);
    assert(glGetError() == GL_NO_ERROR);

#endif // HAVE_EXTERNAL_MEMORY

    d->width = image_create_info.extent.width;
    d->height = image_create_info.extent.height;

    create_frame_buffer(pg);
}

static void update_descriptor_set(PGRAPHState *pg, SurfaceBinding *surface)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    VkDescriptorImageInfo image_infos[2];
    VkWriteDescriptorSet descriptor_writes[2];

    // Display surface
    image_infos[0] = (VkDescriptorImageInfo){
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .imageView = surface->image_view,
        .sampler = r->display.sampler,
    };
    descriptor_writes[0] = (VkWriteDescriptorSet){
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = r->display.descriptor_set,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .pImageInfo = &image_infos[0],
    };

    // FIXME: PVIDEO Overlay
    image_infos[1] = (VkDescriptorImageInfo){
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .imageView = r->dummy_texture.image_view,
        .sampler = r->dummy_texture.sampler,
    };
    descriptor_writes[1] = (VkWriteDescriptorSet){
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = r->display.descriptor_set,
        .dstBinding = 1,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .pImageInfo = &image_infos[1],
    };

    vkUpdateDescriptorSets(r->device, ARRAY_SIZE(descriptor_writes),
                           descriptor_writes, 0, NULL);
}

static void update_uniforms(PGRAPHState *pg, SurfaceBinding *surface)
{
    NV2AState *d = container_of(pg, NV2AState, pgraph);
    PGRAPHVkState *r = pg->vk_renderer_state;

    unsigned int width, height;
    uint32_t pline_offset, pstart_addr, pline_compare;
    d->vga.get_resolution(&d->vga, (int*)&width, (int*)&height);
    d->vga.get_offsets(&d->vga, &pline_offset, &pstart_addr, &pline_compare);
    int line_offset = surface->pitch / pline_offset;

    /* Adjust viewport height for interlaced mode, used only in 1080i */
    if (d->vga.cr[NV_PRMCIO_INTERLACE_MODE] != NV_PRMCIO_INTERLACE_MODE_DISABLED) {
        height *= 2;
    }

    pgraph_apply_scaling_factor(pg, &width, &height);

    ShaderUniformLayout *l = &r->display.display_frag->push_constants;
    int display_size_loc = uniform_index(l, "display_size");  // FIXME: Cache
    int line_offset_loc = uniform_index(l, "line_offset");
    uniform2f(l, display_size_loc, width, height);
    uniform1f(l, line_offset_loc, line_offset);

#if 0  // FIXME: PVIDEO overlay 
    // FIXME: This check against PVIDEO_SIZE_IN does not match HW behavior.
    // Many games seem to pass this value when initializing or tearing down
    // PVIDEO. On its own, this generally does not result in the overlay being
    // hidden, however there are certain games (e.g., Ultimate Beach Soccer)
    // that use an unknown mechanism to hide the overlay without explicitly
    // stopping it.
    // Since the value seems to be set to 0xFFFFFFFF only in cases where the
    // content is not valid, it is probably good enough to treat it as an
    // implicit stop.
    bool enabled = (d->pvideo.regs[NV_PVIDEO_BUFFER] & NV_PVIDEO_BUFFER_0_USE)
        && d->pvideo.regs[NV_PVIDEO_SIZE_IN] != 0xFFFFFFFF;
    glUniform1ui(d->pgraph.renderer_state->disp_rndr.pvideo_enable_loc, enabled);
    if (!enabled) {
        return;
    }

    hwaddr base = d->pvideo.regs[NV_PVIDEO_BASE];
    hwaddr limit = d->pvideo.regs[NV_PVIDEO_LIMIT];
    hwaddr offset = d->pvideo.regs[NV_PVIDEO_OFFSET];

    int in_width =
        GET_MASK(d->pvideo.regs[NV_PVIDEO_SIZE_IN], NV_PVIDEO_SIZE_IN_WIDTH);
    int in_height =
        GET_MASK(d->pvideo.regs[NV_PVIDEO_SIZE_IN], NV_PVIDEO_SIZE_IN_HEIGHT);

    int in_s = GET_MASK(d->pvideo.regs[NV_PVIDEO_POINT_IN],
                        NV_PVIDEO_POINT_IN_S);
    int in_t = GET_MASK(d->pvideo.regs[NV_PVIDEO_POINT_IN],
                        NV_PVIDEO_POINT_IN_T);

    int in_pitch =
        GET_MASK(d->pvideo.regs[NV_PVIDEO_FORMAT], NV_PVIDEO_FORMAT_PITCH);
    int in_color =
        GET_MASK(d->pvideo.regs[NV_PVIDEO_FORMAT], NV_PVIDEO_FORMAT_COLOR);

    unsigned int out_width =
        GET_MASK(d->pvideo.regs[NV_PVIDEO_SIZE_OUT], NV_PVIDEO_SIZE_OUT_WIDTH);
    unsigned int out_height =
        GET_MASK(d->pvideo.regs[NV_PVIDEO_SIZE_OUT], NV_PVIDEO_SIZE_OUT_HEIGHT);

    float scale_x = 1.0f;
    float scale_y = 1.0f;
    unsigned int ds_dx = d->pvideo.regs[NV_PVIDEO_DS_DX];
    unsigned int dt_dy = d->pvideo.regs[NV_PVIDEO_DT_DY];
    if (ds_dx != NV_PVIDEO_DIN_DOUT_UNITY) {
        scale_x = pvideo_calculate_scale(ds_dx, out_width);
    }
    if (dt_dy != NV_PVIDEO_DIN_DOUT_UNITY) {
        scale_y = pvideo_calculate_scale(dt_dy, out_height);
    }

    // On HW, setting NV_PVIDEO_SIZE_IN larger than NV_PVIDEO_SIZE_OUT results
    // in them being capped to the output size, content is not scaled. This is
    // particularly important as NV_PVIDEO_SIZE_IN may be set to 0xFFFFFFFF
    // during initialization or teardown.
    if (in_width > out_width) {
        in_width = floorf((float)out_width * scale_x + 0.5f);
    }
    if (in_height > out_height) {
        in_height = floorf((float)out_height * scale_y + 0.5f);
    }

    /* TODO: support other color formats */
    assert(in_color == NV_PVIDEO_FORMAT_COLOR_LE_CR8YB8CB8YA8);

    unsigned int out_x =
        GET_MASK(d->pvideo.regs[NV_PVIDEO_POINT_OUT], NV_PVIDEO_POINT_OUT_X);
    unsigned int out_y =
        GET_MASK(d->pvideo.regs[NV_PVIDEO_POINT_OUT], NV_PVIDEO_POINT_OUT_Y);

    unsigned int color_key_enabled =
        GET_MASK(d->pvideo.regs[NV_PVIDEO_FORMAT], NV_PVIDEO_FORMAT_DISPLAY);
    glUniform1ui(d->pgraph.renderer_state->disp_rndr.pvideo_color_key_enable_loc,
                 color_key_enabled);

    // TODO: Verify that masking off the top byte is correct.
    // SeaBlade sets a color key of 0x80000000 but the texture passed into the
    // shader is cleared to 0 alpha.
    unsigned int color_key = d->pvideo.regs[NV_PVIDEO_COLOR_KEY] & 0xFFFFFF;
    glUniform4f(d->pgraph.renderer_state->disp_rndr.pvideo_color_key_loc,
                GET_MASK(color_key, NV_PVIDEO_COLOR_KEY_RED) / 255.0,
                GET_MASK(color_key, NV_PVIDEO_COLOR_KEY_GREEN) / 255.0,
                GET_MASK(color_key, NV_PVIDEO_COLOR_KEY_BLUE) / 255.0,
                GET_MASK(color_key, NV_PVIDEO_COLOR_KEY_ALPHA) / 255.0);

    assert(offset + in_pitch * in_height <= limit);
    hwaddr end = base + offset + in_pitch * in_height;
    assert(end <= memory_region_size(d->vram));

    pgraph_apply_scaling_factor(pg, &out_x, &out_y);
    pgraph_apply_scaling_factor(pg, &out_width, &out_height);

    // Translate for the GL viewport origin.
    out_y = MAX(pg->renderer_state->gl_display_buffer_height - 1 - (int)(out_y + out_height), 0);

    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, d->pgraph.renderer_state->disp_rndr.pvideo_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    uint8_t *tex_rgba = convert_texture_data__CR8YB8CB8YA8(
        d->vram_ptr + base + offset, in_width, in_height, in_pitch);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, in_width, in_height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, tex_rgba);
    g_free(tex_rgba);
    glUniform1i(d->pgraph.renderer_state->disp_rndr.pvideo_tex_loc, 1);
    glUniform2f(d->pgraph.renderer_state->disp_rndr.pvideo_in_pos_loc, in_s, in_t);
    glUniform4f(d->pgraph.renderer_state->disp_rndr.pvideo_pos_loc,
                out_x, out_y, out_width, out_height);
    glUniform3f(d->pgraph.renderer_state->disp_rndr.pvideo_scale_loc,
                scale_x, scale_y, 1.0f / pg->surface_scale_factor);
#endif
}

static void render_display(PGRAPHState *pg, SurfaceBinding *surface)
{
    PGRAPHVkState *r = pg->vk_renderer_state;
    PGRAPHVkDisplayState *disp = &r->display;

    if (disp->draw_time >= surface->draw_time) {
        return;
    }

    if (r->in_command_buffer &&
        surface->draw_time >= r->command_buffer_start_time) {
        pgraph_vk_finish(pg, VK_FINISH_REASON_PRESENTING);
    }

    update_uniforms(pg, surface);
    update_descriptor_set(pg, surface);

    VkCommandBuffer cmd = pgraph_vk_begin_single_time_commands(pg);

    pgraph_vk_transition_image_layout(pg, cmd, surface->image,
                                      surface->host_fmt.vk_format,
                                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    pgraph_vk_transition_image_layout(
        pg, cmd, disp->image, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    VkRenderPassBeginInfo render_pass_begin_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = disp->render_pass,
        .framebuffer = disp->framebuffer,
        .renderArea.extent.width = disp->width,
        .renderArea.extent.height = disp->height,
    };
    vkCmdBeginRenderPass(cmd, &render_pass_begin_info,
                         VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      disp->pipeline);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            disp->pipeline_layout, 0, 1, &disp->descriptor_set,
                            0, NULL);

    VkViewport viewport = {
        .width = disp->width,
        .height = disp->height,
        .minDepth = 0.0,
        .maxDepth = 1.0,
    };
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {
        .extent.width = disp->width,
        .extent.height = disp->height,
    };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdPushConstants(cmd, disp->pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, disp->display_frag->push_constants.total_size,
                       disp->display_frag->push_constants.allocation);

    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRenderPass(cmd);

#if 0
    VkImageCopy region = {
        .srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .srcSubresource.layerCount = 1,
        .dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .dstSubresource.layerCount = 1,
        .extent.width = surface->width,
        .extent.height = surface->height,
        .extent.depth = 1,
    };
    pgraph_apply_scaling_factor(pg, &region.extent.width,
                                &region.extent.height);

    vkCmdCopyImage(cmd, surface->image,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, disp->image,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
#endif

    pgraph_vk_transition_image_layout(pg, cmd, surface->image,
                                      surface->host_fmt.vk_format,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    pgraph_vk_transition_image_layout(pg, cmd, disp->image,
                                      VK_FORMAT_R8G8B8_UNORM,
                                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    pgraph_vk_end_single_time_commands(pg, cmd);
    nv2a_profile_inc_counter(NV2A_PROF_QUEUE_SUBMIT_5);

    disp->draw_time = surface->draw_time;
}

static void create_surface_sampler(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    VkSamplerCreateInfo sampler_create_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_FALSE,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
    };

    VK_CHECK(vkCreateSampler(r->device, &sampler_create_info, NULL,
                             &r->display.sampler));
}

static void destroy_surface_sampler(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    vkDestroySampler(r->device, r->display.sampler, NULL);
    r->display.sampler = VK_NULL_HANDLE;
}

void pgraph_vk_init_display(PGRAPHState *pg)
{
    create_descriptor_pool(pg);
    create_descriptor_set_layout(pg);
    create_descriptor_sets(pg);
    create_render_pass(pg);
    create_display_pipeline(pg);
    create_surface_sampler(pg);
}

void pgraph_vk_finalize_display(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    if (r->display.image != VK_NULL_HANDLE) {
        destroy_current_display_image(pg);
    }

    destroy_surface_sampler(pg);
    destroy_display_pipeline(pg);
    destroy_render_pass(pg);
    destroy_descriptor_set_layout(pg);
    destroy_descriptor_pool(pg);
}

void pgraph_vk_render_display(PGRAPHState *pg)
{
    NV2AState *d = container_of(pg, NV2AState, pgraph);
    PGRAPHVkState *r = pg->vk_renderer_state;

    uint32_t pline_offset, pstart_addr, pline_compare;
    d->vga.get_offsets(&d->vga, &pline_offset, &pstart_addr, &pline_compare);
    SurfaceBinding *surface =
        pgraph_vk_surface_get_within(d, d->pcrtc.start + pline_offset);
    if (surface == NULL || !surface->color) {
        return;
    }

    unsigned int width = surface->width, height = surface->height;
    pgraph_apply_scaling_factor(pg, &width, &height);

    PGRAPHVkDisplayState *disp = &r->display;
    if (!disp->image || disp->width != width || disp->height != height) {
        create_display_image_from_surface(pg, surface);
    }

    render_display(pg, surface);
}
