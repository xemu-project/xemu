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

#include "hw/xbox/nv2a/pgraph/pgraph.h"
#include "renderer.h"
#include <vulkan/vulkan_core.h>

// TODO: Swizzle/Unswizzle
// TODO: Float depth format (low priority, but would be better for accuracy)

// FIXME: Below pipeline creation assumes identical 3 buffer setup. For
//        swizzle shader we will need more flexibility.

const char *pack_d24_unorm_s8_uint_to_z24s8_glsl =
    "#version 450\n"
    "layout(local_size_x = 256) in;\n"
    "layout(push_constant) uniform PushConstants { uint width_in, width_out; };\n"
    "layout(binding = 0) buffer DepthIn { uint depth_in[]; };\n"
    "layout(binding = 1) buffer StencilIn { uint stencil_in[]; };\n"
    "layout(binding = 2) buffer DepthStencilOut { uint depth_stencil_out[]; };\n"
    "uint get_input_idx(uint idx_out) {\n"
    "    uint scale = width_in / width_out;"
    "    uint y = (idx_out / width_out) * scale;\n"
    "    uint x = (idx_out % width_out) * scale;\n"
    "    return y * width_in + x;\n"
    "}\n"
    "void main() {\n"
    "    uint idx_out = gl_GlobalInvocationID.x;\n"
    "    uint idx_in = get_input_idx(idx_out);\n"
    "    uint depth_value = depth_in[idx_in];\n"
    "    uint stencil_value = (stencil_in[idx_in / 4] >> ((idx_in % 4) * 8)) & 0xff;\n"
    "    depth_stencil_out[idx_out] = depth_value << 8 | stencil_value;\n"
    "}\n";

const char *unpack_z24s8_to_d24_unorm_s8_uint_glsl =
    "#version 450\n"
    "layout(local_size_x = 256) in;\n"
    "layout(push_constant) uniform PushConstants { uint width_in, width_out; };\n"
    "layout(binding = 0) buffer DepthOut { uint depth_out[]; };\n"
    "layout(binding = 1) buffer StencilOut { uint stencil_out[]; };\n"
    "layout(binding = 2) buffer DepthStencilIn { uint depth_stencil_in[]; };\n"
    "uint get_input_idx(uint idx_out) {\n"
    "    uint scale = width_out / width_in;"
    "    uint y = (idx_out / width_out) / scale;\n"
    "    uint x = (idx_out % width_out) / scale;\n"
    "    return y * width_in + x;\n"
    "}\n"
    "void main() {\n"
    "    uint idx_out = gl_GlobalInvocationID.x;\n"
    "    uint idx_in = get_input_idx(idx_out);\n"
    "    depth_out[idx_out] = depth_stencil_in[idx_in] >> 8;\n"
    "    if (idx_out % 4 == 0) {\n"
    "       uint stencil_value = 0;\n"
    "       for (int i = 0; i < 4; i++) {\n" // Include next 3 pixels
    "           uint v = depth_stencil_in[get_input_idx(idx_out + i)] & 0xff;\n"
    "           stencil_value |= v << (i * 8);\n"
    "       }\n"
    "       stencil_out[idx_out / 4] = stencil_value;\n"
    "    }\n"
    "}\n";

const char *pack_d32_sfloat_s8_uint_to_z24s8_glsl =
    "#version 450\n"
    "layout(local_size_x = 256) in;\n"
    "layout(push_constant) uniform PushConstants { uint width_in, width_out; };\n"
    "layout(binding = 0) buffer DepthIn { float depth_in[]; };\n"
    "layout(binding = 1) buffer StencilIn { uint stencil_in[]; };\n"
    "layout(binding = 2) buffer DepthStencilOut { uint depth_stencil_out[]; };\n"
    "uint get_input_idx(uint idx_out) {\n"
    "    uint scale = width_in / width_out;"
    "    uint y = (idx_out / width_out) * scale;\n"
    "    uint x = (idx_out % width_out) * scale;\n"
    "    return y * width_in + x;\n"
    "}\n"
    "void main() {\n"
    "    uint idx_out = gl_GlobalInvocationID.x;\n"
    "    uint idx_in = get_input_idx(idx_out);\n"
    "    uint depth_value = int(depth_in[idx_in] * float(0xffffff));\n"
    "    uint stencil_value = (stencil_in[idx_in / 4] >> ((idx_in % 4) * 8)) & 0xff;\n"
    "    depth_stencil_out[idx_out] = depth_value << 8 | stencil_value;\n"
    "}\n";

const char *unpack_z24s8_to_d32_sfloat_s8_uint_glsl =
    "#version 450\n"
    "layout(local_size_x = 256) in;\n"
    "layout(push_constant) uniform PushConstants { uint width_in, width_out; };\n"
    "layout(binding = 0) buffer DepthOut { float depth_out[]; };\n"
    "layout(binding = 1) buffer StencilOut { uint stencil_out[]; };\n"
    "layout(binding = 2) buffer DepthStencilIn { uint depth_stencil_in[]; };\n"
    "uint get_input_idx(uint idx_out) {\n"
    "    uint scale = width_out / width_in;"
    "    uint y = (idx_out / width_out) / scale;\n"
    "    uint x = (idx_out % width_out) / scale;\n"
    "    return y * width_in + x;\n"
    "}\n"
    "void main() {\n"
    "    uint idx_out = gl_GlobalInvocationID.x;\n"
    "    uint idx_in = get_input_idx(idx_out);\n"
    "    depth_out[idx_out] = float(depth_stencil_in[idx_in] >> 8) / float(0xffffff);\n"
    "    if (idx_out % 4 == 0) {\n"
    "       uint stencil_value = 0;\n"
    "       for (int i = 0; i < 4; i++) {\n" // Include next 3 pixels
    "           uint v = depth_stencil_in[get_input_idx(idx_out + i)] & 0xff;\n"
    "           stencil_value |= v << (i * 8);\n"
    "       }\n"
    "       stencil_out[idx_out / 4] = stencil_value;\n"
    "    }\n"
    "}\n";

static void create_descriptor_pool(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    VkDescriptorPoolSize pool_sizes[] = {
        {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 3 * ARRAY_SIZE(r->compute.descriptor_sets),
        },
    };

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = ARRAY_SIZE(pool_sizes),
        .pPoolSizes = pool_sizes,
        .maxSets = ARRAY_SIZE(r->compute.descriptor_sets),
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
    };
    VK_CHECK(vkCreateDescriptorPool(r->device, &pool_info, NULL,
                                    &r->compute.descriptor_pool));
}

static void destroy_descriptor_pool(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    vkDestroyDescriptorPool(r->device, r->compute.descriptor_pool, NULL);
    r->compute.descriptor_pool = VK_NULL_HANDLE;
}

static void create_descriptor_set_layout(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    const int num_buffers = 3;

    VkDescriptorSetLayoutBinding bindings[num_buffers];
    for (int i = 0; i < num_buffers; i++) {
        bindings[i] = (VkDescriptorSetLayoutBinding){
            .binding = i,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        };
    }
    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = ARRAY_SIZE(bindings),
        .pBindings = bindings,
    };
    VK_CHECK(vkCreateDescriptorSetLayout(r->device, &layout_info, NULL,
                                         &r->compute.descriptor_set_layout));
}

static void destroy_descriptor_set_layout(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    vkDestroyDescriptorSetLayout(r->device, r->compute.descriptor_set_layout,
                                 NULL);
    r->compute.descriptor_set_layout = VK_NULL_HANDLE;
}

static void create_descriptor_sets(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    VkDescriptorSetLayout layouts[ARRAY_SIZE(r->compute.descriptor_sets)];
    for (int i = 0; i < ARRAY_SIZE(layouts); i++) {
        layouts[i] = r->compute.descriptor_set_layout;
    }
    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = r->compute.descriptor_pool,
        .descriptorSetCount = ARRAY_SIZE(r->compute.descriptor_sets),
        .pSetLayouts = layouts,
    };
    VK_CHECK(vkAllocateDescriptorSets(r->device, &alloc_info,
                                      r->compute.descriptor_sets));
}

static void destroy_descriptor_sets(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    vkFreeDescriptorSets(r->device, r->compute.descriptor_pool,
                         ARRAY_SIZE(r->compute.descriptor_sets),
                         r->compute.descriptor_sets);
    for (int i = 0; i < ARRAY_SIZE(r->compute.descriptor_sets); i++) {
        r->compute.descriptor_sets[i] = VK_NULL_HANDLE;
    }
}

static void create_compute_pipeline_layout(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    VkPushConstantRange push_constant_range = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .size = 2 * sizeof(uint32_t),
    };
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &r->compute.descriptor_set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant_range,
    };
    VK_CHECK(vkCreatePipelineLayout(r->device, &pipeline_layout_info, NULL,
                                    &r->compute.pipeline_layout));
}

static VkPipeline create_compute_pipeline(PGRAPHState *pg, const char *glsl)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    ShaderModuleInfo *module = pgraph_vk_create_shader_module_from_glsl(
        r, VK_SHADER_STAGE_COMPUTE_BIT, glsl);

    VkComputePipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .layout = r->compute.pipeline_layout,
        .stage =
            (VkPipelineShaderStageCreateInfo){
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .pName = "main",
                .module = module->module,
            },
    };
    VkPipeline pipeline;
    VK_CHECK(vkCreateComputePipelines(r->device, r->vk_pipeline_cache, 1,
                                       &pipeline_info, NULL,
                                       &pipeline));

    pgraph_vk_destroy_shader_module(r, module);

    return pipeline;
}

static void update_descriptor_sets(PGRAPHState *pg,
                                   VkDescriptorBufferInfo *buffers, int count)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    assert(count == 3);
    VkWriteDescriptorSet descriptor_writes[3];

    assert(r->compute.descriptor_set_index <
           ARRAY_SIZE(r->compute.descriptor_sets));

    for (int i = 0; i < count; i++) {
        descriptor_writes[i] = (VkWriteDescriptorSet){
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet =
                r->compute.descriptor_sets[r->compute.descriptor_set_index],
            .dstBinding = i,
            .dstArrayElement = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .pBufferInfo = &buffers[i],
        };
    }
    vkUpdateDescriptorSets(r->device, count, descriptor_writes, 0, NULL);

    r->compute.descriptor_set_index += 1;
}

bool pgraph_vk_compute_needs_finish(PGRAPHVkState *r)
{
    bool need_descriptor_write_reset = (r->compute.descriptor_set_index >=
                                        ARRAY_SIZE(r->compute.descriptor_sets));

    return need_descriptor_write_reset;
}

void pgraph_vk_compute_finish_complete(PGRAPHVkState *r)
{
    r->compute.descriptor_set_index = 0;
}

//
// Pack depth+stencil into NV097_SET_SURFACE_FORMAT_ZETA_Z24S8
// formatted buffer with depth in bits 31-8 and stencil in bits 7-0.
//
void pgraph_vk_pack_depth_stencil(PGRAPHState *pg, SurfaceBinding *surface,
                                  VkCommandBuffer cmd, VkBuffer src,
                                  VkBuffer dst, bool downscale)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    unsigned int input_width = surface->width, input_height = surface->height;
    pgraph_apply_scaling_factor(pg, &input_width, &input_height);

    unsigned int output_width = surface->width, output_height = surface->height;
    if (!downscale) {
        pgraph_apply_scaling_factor(pg, &output_width, &output_height);
    }

    size_t depth_bytes_per_pixel = 4;
    size_t depth_size = input_width * input_height * depth_bytes_per_pixel;

    size_t stencil_bytes_per_pixel = 1;
    size_t stencil_size = input_width * input_height * stencil_bytes_per_pixel;

    size_t output_bytes_per_pixel = 4;
    size_t output_size = output_width * output_height * output_bytes_per_pixel;

    VkDescriptorBufferInfo buffers[] = {
        {
            .buffer = src,
            .offset = 0,
            .range = depth_size,
        },
        {
            .buffer = src,
            .offset = depth_size,
            .range = stencil_size,
        },
        {
            .buffer = dst,
            .offset = 0,
            .range = output_size,
        },
    };

    update_descriptor_sets(pg, buffers, ARRAY_SIZE(buffers));

    if (surface->host_fmt.vk_format == VK_FORMAT_D24_UNORM_S8_UINT) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                          r->compute.pipeline_pack_d24s8);
    } else if (surface->host_fmt.vk_format == VK_FORMAT_D32_SFLOAT_S8_UINT) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                          r->compute.pipeline_pack_f32s8);
    } else {
        assert(!"Unsupported pack format");
    }
    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r->compute.pipeline_layout, 0, 1,
        &r->compute.descriptor_sets[r->compute.descriptor_set_index - 1], 0,
        NULL);

    uint32_t push_constants[2] = { input_width, output_width };
    assert(sizeof(push_constants) == 8);
    vkCmdPushConstants(cmd, r->compute.pipeline_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants),
                       push_constants);

    size_t workgroup_size_in_units = 256;
    size_t output_size_in_units = output_width * output_height;
    assert(output_size_in_units % workgroup_size_in_units == 0);
    size_t group_count = output_size_in_units / workgroup_size_in_units;

    // FIXME: Check max group count

    vkCmdDispatch(cmd, group_count, 1, 1);
}

void pgraph_vk_unpack_depth_stencil(PGRAPHState *pg, SurfaceBinding *surface,
                                    VkCommandBuffer cmd, VkBuffer src,
                                    VkBuffer dst)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    unsigned int input_width = surface->width, input_height = surface->height;

    unsigned int output_width = surface->width, output_height = surface->height;
    pgraph_apply_scaling_factor(pg, &output_width, &output_height);

    size_t depth_bytes_per_pixel = 4;
    size_t depth_size = output_width * output_height * depth_bytes_per_pixel;

    size_t stencil_bytes_per_pixel = 1;
    size_t stencil_size = output_width * output_height * stencil_bytes_per_pixel;

    size_t input_bytes_per_pixel = 4;
    size_t input_size = input_width * input_height * input_bytes_per_pixel;

    VkDescriptorBufferInfo buffers[] = {
        {
            .buffer = dst,
            .offset = 0,
            .range = depth_size,
        },
        {
            .buffer = dst,
            .offset = depth_size,
            .range = stencil_size,
        },
        {
            .buffer = src,
            .offset = 0,
            .range = input_size,
        },
    };
    update_descriptor_sets(pg, buffers, ARRAY_SIZE(buffers));

    if (surface->host_fmt.vk_format == VK_FORMAT_D24_UNORM_S8_UINT) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                          r->compute.pipeline_unpack_d24s8);
    } else if (surface->host_fmt.vk_format == VK_FORMAT_D32_SFLOAT_S8_UINT) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                          r->compute.pipeline_unpack_f32s8);
    } else {
        assert(!"Unsupported pack format");
    }
    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_COMPUTE, r->compute.pipeline_layout, 0, 1,
        &r->compute.descriptor_sets[r->compute.descriptor_set_index - 1], 0,
        NULL);

    assert(output_width >= input_width);
    uint32_t push_constants[2] = { input_width, output_width };
    assert(sizeof(push_constants) == 8);
    vkCmdPushConstants(cmd, r->compute.pipeline_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants),
                       push_constants);

    size_t workgroup_size_in_units = 256;
    size_t output_size_in_units = output_width * output_height;
    assert(output_size_in_units % workgroup_size_in_units == 0);
    size_t group_count = output_size_in_units / workgroup_size_in_units;

    // FIXME: Check max group count

    vkCmdDispatch(cmd, group_count, 1, 1);
}

void pgraph_vk_init_compute(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    create_descriptor_pool(pg);
    create_descriptor_set_layout(pg);
    create_descriptor_sets(pg);
    create_compute_pipeline_layout(pg);

    r->compute.pipeline_pack_d24s8 =
        create_compute_pipeline(pg, pack_d24_unorm_s8_uint_to_z24s8_glsl);
    r->compute.pipeline_unpack_d24s8 =
        create_compute_pipeline(pg, unpack_z24s8_to_d24_unorm_s8_uint_glsl);
    r->compute.pipeline_pack_f32s8 =
        create_compute_pipeline(pg, pack_d32_sfloat_s8_uint_to_z24s8_glsl);
    r->compute.pipeline_unpack_f32s8 =
        create_compute_pipeline(pg, unpack_z24s8_to_d32_sfloat_s8_uint_glsl);
}

void pgraph_vk_finalize_compute(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    VkPipeline *pipelines[] = {
        &r->compute.pipeline_pack_d24s8,
        &r->compute.pipeline_unpack_d24s8,
        &r->compute.pipeline_pack_f32s8,
        &r->compute.pipeline_unpack_f32s8,
    };

    for (int i = 0; i < ARRAY_SIZE(pipelines); i++) {
        vkDestroyPipeline(r->device, *pipelines[i], NULL);
        pipelines[i] = VK_NULL_HANDLE;
    }

    vkDestroyPipelineLayout(r->device, r->compute.pipeline_layout, NULL);
    r->compute.pipeline_layout = VK_NULL_HANDLE;

    destroy_descriptor_sets(pg);
    destroy_descriptor_set_layout(pg);
    destroy_descriptor_pool(pg);
}
