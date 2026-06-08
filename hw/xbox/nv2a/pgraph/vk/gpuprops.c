/*
 * Geforce NV2A PGRAPH Vulkan Renderer
 *
 * Copyright (c) 2024-2025 Matt Borgerson
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

#include "debug.h"
#include "renderer.h"

static GPUProperties pgraph_vk_gpu_properties;

static const char *vertex_shader_source =
    "#version 450\n"
    "layout(location = 0) out vec3 v_fragColor;\n"
    "\n"
    "vec2 positions[11] = vec2[](\n"
    "    vec2(-0.5, -0.75),\n"
    "    vec2(-0.25, -0.25),\n"
    "    vec2(-0.75, -0.25),\n"
    "    vec2(0.25, -0.25),\n"
    "    vec2(0.25, -0.75),\n"
    "    vec2(0.75, -0.25),\n"
    "    vec2(0.75, -0.75),\n"
    "    vec2(-0.75, 0.75),\n"
    "    vec2(-0.75, 0.25),\n"
    "    vec2(-0.25, 0.25),\n"
    "    vec2(-0.25, 0.75)\n"
    ");\n"
    "\n"
    "vec3 colors[11] = vec3[](\n"
    "    vec3(0.0, 0.0, 1.0),\n"
    "    vec3(0.0, 1.0, 0.0),\n"
    "    vec3(0.0, 1.0, 1.0),\n"
    "    vec3(0.0, 0.0, 1.0),\n"
    "    vec3(0.0, 1.0, 0.0),\n"
    "    vec3(0.0, 1.0, 1.0),\n"
    "    vec3(1.0, 0.0, 0.0),\n"
    "    vec3(0.0, 0.0, 1.0),\n"
    "    vec3(0.0, 1.0, 0.0),\n"
    "    vec3(0.0, 1.0, 1.0),\n"
    "    vec3(1.0, 0.0, 0.0)\n"
    ");\n"
    "\n"
    "void main() {\n"
    "    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);\n"
    "    v_fragColor = colors[gl_VertexIndex];\n"
    "}\n";

static const char *geometry_shader_source =
    "#version 450\n"
    "layout(triangles) in;\n"
    "layout(triangle_strip, max_vertices = 3) out;\n"
    "layout(location = 0) out vec3 fragColor;\n"
    "layout(location = 0) in vec3 v_fragColor[];\n"
    "\n"
    "void main() {\n"
    "    for (int i = 0; i < 3; i++) {\n"
             // This should be just:
             //   gl_Position = gl_in[i].gl_Position;
             //   fragColor = v_fragColor[0];
             // but we apply the same Nvidia bug work around from gl/gpuprops.c
             // to be on the safe side even if the compilers involved with
             // Vulkan are different.
    "        gl_Position = gl_in[i].gl_Position + vec4(1.0/16384.0, 1.0/16384.0, 0.0, 0.0);\n"
    "        precise vec3 color = v_fragColor[0]*(0.999 + gl_in[i].gl_Position.x/16384.0) + v_fragColor[1]*0.00005 + v_fragColor[2]*0.00005;\n"
    "        fragColor = color;\n"
    "        EmitVertex();\n"
    "    }\n"
    "    EndPrimitive();\n"
    "}\n";

static const char *fragment_shader_source =
    "#version 450\n"
    "layout(location = 0) out vec4 outColor;\n"
    "layout(location = 0) in vec3 fragColor;\n"
    "\n"
    "void main() {\n"
    "    outColor = vec4(fragColor, 1.0);\n"
    "}\n";

static VkPipeline create_test_pipeline(
    NV2AState *d, VkPrimitiveTopology primitive_topology,
    VkShaderModule vert_shader_module, VkShaderModule geom_shader_module,
    VkShaderModule frag_shader_module, VkPipelineLayout pipeline_layout,
    VkRenderPass render_pass, int width, int height)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHVkState *r = pg->vk_renderer_state;

    VkPipelineShaderStageCreateInfo shader_stages[] = {
        (VkPipelineShaderStageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vert_shader_module,
            .pName = "main",
        },
        (VkPipelineShaderStageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_GEOMETRY_BIT,
            .module = geom_shader_module,
            .pName = "main",
        },
        (VkPipelineShaderStageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = frag_shader_module,
            .pName = "main",
        },
    };

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .vertexAttributeDescriptionCount = 0,
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = primitive_topology,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)width,
        .height = (float)height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    VkRect2D scissor = {
        .offset = { 0, 0 },
        .extent.width = width,
        .extent.height = height,
    };
    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
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
        .blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f },
    };

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = ARRAY_SIZE(shader_stages),
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &color_blending,
        .layout = pipeline_layout,
        .renderPass = render_pass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
    };

    VkPipeline pipeline;
    VK_CHECK(vkCreateGraphicsPipelines(r->device, VK_NULL_HANDLE, 1,
                                       &pipeline_info, NULL, &pipeline));

    return pipeline;
}

static uint8_t *render_geom_shader_triangles(NV2AState *d, int width,
                                             int height)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHVkState *r = pg->vk_renderer_state;

    // Create image
    VkImageCreateInfo image_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent.width = width,
        .extent.height = height,
        .extent.depth = 1,
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VkImage offscreen_image;
    VK_CHECK(
        vkCreateImage(r->device, &image_create_info, NULL, &offscreen_image));

    // Allocate and bind image memory
    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(r->device, offscreen_image,
                                 &memory_requirements);

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memory_requirements.size,
        .memoryTypeIndex =
            pgraph_vk_get_memory_type(pg, memory_requirements.memoryTypeBits,
                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };

    VkDeviceMemory image_memory;
    VK_CHECK(vkAllocateMemory(r->device, &alloc_info, NULL, &image_memory));
    VK_CHECK(vkBindImageMemory(r->device, offscreen_image, image_memory, 0));

    // Create Image View
    VkImageViewCreateInfo image_view_create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = offscreen_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = image_create_info.format,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .subresourceRange.levelCount = 1,
        .subresourceRange.layerCount = 1,
    };

    VkImageView offscreen_image_view;
    VK_CHECK(vkCreateImageView(r->device, &image_view_create_info, NULL,
                               &offscreen_image_view));

    // Buffer for image CPU access
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = width * height * 4, // RGBA8 = 4 bytes per pixel
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VkBuffer cpu_buffer;
    VK_CHECK(vkCreateBuffer(r->device, &buffer_info, NULL, &cpu_buffer));

    // Allocate and bind memory for image CPU access
    VkMemoryRequirements host_mem_requirements;
    vkGetBufferMemoryRequirements(r->device, cpu_buffer,
                                  &host_mem_requirements);

    VkMemoryAllocateInfo host_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = host_mem_requirements.size,
        .memoryTypeIndex =
            pgraph_vk_get_memory_type(pg, host_mem_requirements.memoryTypeBits,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
    };

    VkDeviceMemory cpu_buffer_memory;
    VK_CHECK(vkAllocateMemory(r->device, &host_alloc_info, NULL,
                              &cpu_buffer_memory));
    VK_CHECK(vkBindBufferMemory(r->device, cpu_buffer, cpu_buffer_memory, 0));


    VkAttachmentDescription color_attachment = {
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentReference color_ref = {
        0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_ref,
    };

    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
    };

    VkRenderPass render_pass;
    VK_CHECK(
        vkCreateRenderPass(r->device, &render_pass_info, NULL, &render_pass));

    VkFramebufferCreateInfo fb_info = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = render_pass,
        .attachmentCount = 1,
        .pAttachments = &offscreen_image_view,
        .width = width,
        .height = height,
        .layers = 1,
    };

    VkFramebuffer framebuffer;
    VK_CHECK(vkCreateFramebuffer(r->device, &fb_info, NULL, &framebuffer));

    ShaderModuleInfo *vsh_info = pgraph_vk_create_shader_module_from_glsl(
        r, VK_SHADER_STAGE_VERTEX_BIT, vertex_shader_source);
    ShaderModuleInfo *geom_info = pgraph_vk_create_shader_module_from_glsl(
        r, VK_SHADER_STAGE_GEOMETRY_BIT, geometry_shader_source);
    ShaderModuleInfo *psh_info = pgraph_vk_create_shader_module_from_glsl(
        r, VK_SHADER_STAGE_FRAGMENT_BIT, fragment_shader_source);

    VkShaderModule vert_shader_module = vsh_info->module;
    VkShaderModule geom_shader_module = geom_info->module;
    VkShaderModule frag_shader_module = psh_info->module;

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 0,
        .pushConstantRangeCount = 0,
    };

    VkPipelineLayout pipeline_layout;
    VK_CHECK(vkCreatePipelineLayout(r->device, &pipeline_layout_info, NULL,
                                    &pipeline_layout));

    VkPipeline tri_pipeline = create_test_pipeline(
        d, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, vert_shader_module,
        geom_shader_module, frag_shader_module, pipeline_layout, render_pass,
        width, height);

    VkPipeline strip_pipeline = create_test_pipeline(
        d, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, vert_shader_module,
        geom_shader_module, frag_shader_module, pipeline_layout, render_pass,
        width, height);

    VkPipeline fan_pipeline = create_test_pipeline(
        d, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN, vert_shader_module,
        geom_shader_module, frag_shader_module, pipeline_layout, render_pass,
        width, height);

    pgraph_vk_destroy_shader_module(r, psh_info);
    pgraph_vk_destroy_shader_module(r, geom_info);
    pgraph_vk_destroy_shader_module(r, vsh_info);

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    VK_CHECK(vkBeginCommandBuffer(r->command_buffer, &begin_info));

    // Begin render pass
    VkClearValue clear_color = {
        .color.float32 = { 0.0f, 0.0f, 0.0f, 1.0f },
    };
    VkRenderPassBeginInfo rp_begin = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = render_pass,
        .framebuffer = framebuffer,
        .renderArea.extent.width = width,
        .renderArea.extent.height = height,
        .clearValueCount = 1,
        .pClearValues = &clear_color,
    };

    vkCmdBeginRenderPass(r->command_buffer, &rp_begin,
                         VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(r->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      tri_pipeline);
    vkCmdDraw(r->command_buffer, 3, 1, 0, 0);
    vkCmdBindPipeline(r->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      strip_pipeline);
    vkCmdDraw(r->command_buffer, 4, 1, 3, 0);
    vkCmdBindPipeline(r->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      fan_pipeline);
    vkCmdDraw(r->command_buffer, 4, 1, 7, 0);

    vkCmdEndRenderPass(r->command_buffer);

    // Synchronize and transition framebuffer for copying to CPU
    pgraph_vk_transition_image_layout(pg, r->command_buffer, offscreen_image,
                                      image_create_info.format,
                                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    // Copy framebuffer to CPU memory
    VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = 0, // tightly packed
        .bufferImageHeight = 0,

        .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .imageSubresource.mipLevel = 0,
        .imageSubresource.baseArrayLayer = 0,
        .imageSubresource.layerCount = 1,

        .imageOffset = { 0, 0, 0 },
        .imageExtent = { width, height, 1 },
    };

    vkCmdCopyImageToBuffer(r->command_buffer, offscreen_image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, cpu_buffer, 1,
                           &region);

    VK_CHECK(vkEndCommandBuffer(r->command_buffer));

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &r->command_buffer,
    };

    VK_CHECK(vkQueueSubmit(r->queue, 1, &submit_info, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(r->queue));

    void *data;
    VK_CHECK(
        vkMapMemory(r->device, cpu_buffer_memory, 0, VK_WHOLE_SIZE, 0, &data));
    void *pixels = g_malloc(width * height * 4);
    assert(pixels != NULL);
    memcpy(pixels, data, width * height * 4);
    vkUnmapMemory(r->device, cpu_buffer_memory);

    vkDestroyPipeline(r->device, strip_pipeline, NULL);
    vkDestroyPipeline(r->device, fan_pipeline, NULL);
    vkDestroyPipeline(r->device, tri_pipeline, NULL);
    vkDestroyPipelineLayout(r->device, pipeline_layout, NULL);
    vkDestroyFramebuffer(r->device, framebuffer, NULL);
    vkDestroyRenderPass(r->device, render_pass, NULL);
    vkDestroyImageView(r->device, offscreen_image_view, NULL);
    vkDestroyBuffer(r->device, cpu_buffer, NULL);
    vkFreeMemory(r->device, cpu_buffer_memory, NULL);
    vkDestroyImage(r->device, offscreen_image, NULL);
    vkFreeMemory(r->device, image_memory, NULL);

    return (uint8_t *)pixels;
}

static bool colors_match(int r1, int g1, int b1, int r2, int g2, int b2)
{
    int dr = r1 - r2;
    int dg = g1 - g2;
    int db = b1 - b2;

    return (dr * dr + dg * dg + db * db) <= 16;
}

static int get_color_index(uint8_t *pixel)
{
    int r = pixel[0];
    int g = pixel[1];
    int b = pixel[2];

    if (colors_match(r, g, b, 0, 0, 255)) {
        return 0;
    } else if (colors_match(r, g, b, 0, 255, 0)) {
        return 1;
    } else if (colors_match(r, g, b, 0, 255, 255)) {
        return 2;
    } else if (colors_match(r, g, b, 255, 0, 0)) {
        return 3;
    } else {
        return -1;
    }
}

static int calc_offset_from_ndc(float x, float y, int width, int height)
{
    int x0 = (int)((x + 1.0f) * width * 0.5f);
    int y0 = (int)((y + 1.0f) * height * 0.5f);

    x0 = MAX(x0, 0);
    y0 = MAX(y0, 0);
    x0 = MIN(x0, width - 1);
    y0 = MIN(y0, height - 1);

    return y0 * width + x0;
}

static void determine_triangle_winding_order(uint8_t *pixels, int width,
                                             int height, GPUProperties *props)
{
    uint8_t *tri_pix =
        pixels + calc_offset_from_ndc(-0.5f, -0.5f, width, height) * 4;
    uint8_t *strip0_pix =
        pixels + calc_offset_from_ndc(0.417f, -0.417f, width, height) * 4;
    uint8_t *strip1_pix =
        pixels + calc_offset_from_ndc(0.583f, -0.583f, width, height) * 4;
    uint8_t *fan_pix =
        pixels + calc_offset_from_ndc(-0.583f, 0.417f, width, height) * 4;
    uint8_t *fan2_pix =
        pixels + calc_offset_from_ndc(-0.417f, 0.583f, width, height) * 4;

    int tri_rot = get_color_index(tri_pix);
    if (tri_rot < 0 || tri_rot > 2) {
        fprintf(stderr,
                "Could not determine triangle rotation, got color: R=%d, G=%d, "
                "B=%d\n",
                tri_pix[0], tri_pix[1], tri_pix[2]);
        tri_rot = 0;
    }
    props->geom_shader_winding.tri = tri_rot;

    int strip0_rot = get_color_index(strip0_pix);
    if (strip0_rot < 0 || strip0_rot > 2) {
        fprintf(stderr,
                "Could not determine triangle strip0 rotation, got color: "
                "R=%d, G=%d, B=%d\n",
                strip0_pix[0], strip0_pix[1], strip0_pix[2]);
        strip0_rot = 0;
    }
    int strip1_rot = get_color_index(strip1_pix) - 1;
    if (strip1_rot < 0 || strip1_rot > 2) {
        fprintf(stderr,
                "Could not determine triangle strip1 rotation, got color: "
                "R=%d, G=%d, B=%d\n",
                strip1_pix[0], strip1_pix[1], strip1_pix[2]);
        strip1_rot = 0;
    }
    props->geom_shader_winding.tri_strip0 = strip0_rot;
    props->geom_shader_winding.tri_strip1 = (3 - strip1_rot) % 3;

    int fan_rot = get_color_index(fan_pix);
    int fan2_rot = get_color_index(fan2_pix);
    if (fan2_rot == 0) {
        fan2_rot = 1;
    }
    fan2_rot--;
    if (fan_rot != fan2_rot) {
        fprintf(stderr,
                "Unexpected inconsistency in triangle fan winding, got colors: "
                "R=%d, G=%d, B=%d and R=%d, G=%d, B=%d\n",
                fan_pix[0], fan_pix[1], fan_pix[2], fan2_pix[0], fan2_pix[1],
                fan2_pix[2]);
        fan_rot = 1;
    }
    if (fan_rot < 0 || fan_rot > 2) {
        fprintf(stderr,
                "Could not determine triangle fan rotation, got color: R=%d, "
                "G=%d, B=%d\n",
                fan_pix[0], fan_pix[1], fan_pix[2]);
        fan_rot = 1;
    }
    props->geom_shader_winding.tri_fan = (fan_rot + 2) % 3;
}

void pgraph_vk_determine_gpu_properties(NV2AState *d)
{
    const int width = 640;
    const int height = 480;

    uint8_t *pixels = render_geom_shader_triangles(d, width, height);
    determine_triangle_winding_order(pixels, width, height,
                                     &pgraph_vk_gpu_properties);
    g_free(pixels);

    fprintf(stderr, "VK geometry shader winding: %d, %d, %d, %d\n",
            pgraph_vk_gpu_properties.geom_shader_winding.tri,
            pgraph_vk_gpu_properties.geom_shader_winding.tri_strip0,
            pgraph_vk_gpu_properties.geom_shader_winding.tri_strip1,
            pgraph_vk_gpu_properties.geom_shader_winding.tri_fan);
}

GPUProperties *pgraph_vk_get_gpu_properties(void)
{
    return &pgraph_vk_gpu_properties;
}
