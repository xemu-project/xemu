/*
 * Geforce NV2A PGRAPH Vulkan Renderer
 *
 * Copyright (c) 2024 Matt Borgerson
 *
 * Based on GL implementation:
 *
 * Copyright (c) 2015 espes
 * Copyright (c) 2015 Jannik Vogel
 * Copyright (c) 2018-2024 Matt Borgerson
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
#include "hw/xbox/nv2a/pgraph/shaders.h"
#include "hw/xbox/nv2a/pgraph/util.h"
#include "hw/xbox/nv2a/pgraph/glsl/geom.h"
#include "hw/xbox/nv2a/pgraph/glsl/vsh.h"
#include "hw/xbox/nv2a/pgraph/glsl/psh.h"
#include "qemu/fast-hash.h"
#include "qemu/mstring.h"
#include "renderer.h"
#include <locale.h>

const size_t MAX_UNIFORM_ATTR_VALUES_SIZE = NV2A_VERTEXSHADER_ATTRIBUTES * 4 * sizeof(float);

static void create_descriptor_pool(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    size_t num_sets = ARRAY_SIZE(r->descriptor_sets);

    VkDescriptorPoolSize pool_sizes[] = {
        {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 2 * num_sets,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = NV2A_MAX_TEXTURES * num_sets,
        }
    };

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = ARRAY_SIZE(pool_sizes),
        .pPoolSizes = pool_sizes,
        .maxSets = ARRAY_SIZE(r->descriptor_sets),
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
    };
    VK_CHECK(vkCreateDescriptorPool(r->device, &pool_info, NULL,
                                    &r->descriptor_pool));
}

static void destroy_descriptor_pool(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    vkDestroyDescriptorPool(r->device, r->descriptor_pool, NULL);
    r->descriptor_pool = VK_NULL_HANDLE;
}

static void create_descriptor_set_layout(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    VkDescriptorSetLayoutBinding bindings[2 + NV2A_MAX_TEXTURES];

    bindings[0] = (VkDescriptorSetLayoutBinding){
        .binding = VSH_UBO_BINDING,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    };
    bindings[1] = (VkDescriptorSetLayoutBinding){
        .binding = PSH_UBO_BINDING,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };
    for (int i = 0; i < NV2A_MAX_TEXTURES; i++) {
        bindings[2 + i] = (VkDescriptorSetLayoutBinding){
            .binding = PSH_TEX_BINDING + i,
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
                                         &r->descriptor_set_layout));
}

static void destroy_descriptor_set_layout(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    vkDestroyDescriptorSetLayout(r->device, r->descriptor_set_layout, NULL);
    r->descriptor_set_layout = VK_NULL_HANDLE;
}

static void create_descriptor_sets(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    VkDescriptorSetLayout layouts[ARRAY_SIZE(r->descriptor_sets)];
    for (int i = 0; i < ARRAY_SIZE(layouts); i++) {
        layouts[i] = r->descriptor_set_layout;
    }

    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = r->descriptor_pool,
        .descriptorSetCount = ARRAY_SIZE(r->descriptor_sets),
        .pSetLayouts = layouts,
    };
    VK_CHECK(
        vkAllocateDescriptorSets(r->device, &alloc_info, r->descriptor_sets));
}

static void destroy_descriptor_sets(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    vkFreeDescriptorSets(r->device, r->descriptor_pool,
                         ARRAY_SIZE(r->descriptor_sets), r->descriptor_sets);
    for (int i = 0; i < ARRAY_SIZE(r->descriptor_sets); i++) {
        r->descriptor_sets[i] = VK_NULL_HANDLE;
    }
}

void pgraph_vk_update_descriptor_sets(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    bool need_uniform_write =
        r->uniforms_changed ||
        !r->storage_buffers[BUFFER_UNIFORM_STAGING].buffer_offset;

    if (!(r->shader_bindings_changed || r->texture_bindings_changed ||
          (r->descriptor_set_index == 0) || need_uniform_write)) {
        return; // Nothing changed
    }

    ShaderBinding *binding = r->shader_binding;
    ShaderUniformLayout *layouts[] = { &binding->vertex->uniforms,
                                       &binding->fragment->uniforms };
    VkDeviceSize ubo_buffer_total_size = 0;
    for (int i = 0; i < ARRAY_SIZE(layouts); i++) {
        ubo_buffer_total_size += layouts[i]->total_size;
    }
    bool need_ubo_staging_buffer_reset =
        r->uniforms_changed &&
        !pgraph_vk_buffer_has_space_for(pg, BUFFER_UNIFORM_STAGING,
                                        ubo_buffer_total_size,
                                        r->device_props.limits.minUniformBufferOffsetAlignment);

    bool need_descriptor_write_reset =
        (r->descriptor_set_index >= ARRAY_SIZE(r->descriptor_sets));

    if (need_descriptor_write_reset || need_ubo_staging_buffer_reset) {
        pgraph_vk_finish(pg, VK_FINISH_REASON_NEED_BUFFER_SPACE);
        need_uniform_write = true;
    }

    VkWriteDescriptorSet descriptor_writes[2 + NV2A_MAX_TEXTURES];

    assert(r->descriptor_set_index < ARRAY_SIZE(r->descriptor_sets));

    if (need_uniform_write) {
        for (int i = 0; i < ARRAY_SIZE(layouts); i++) {
            void *data = layouts[i]->allocation;
            VkDeviceSize size = layouts[i]->total_size;
            r->uniform_buffer_offsets[i] = pgraph_vk_append_to_buffer(
                pg, BUFFER_UNIFORM_STAGING, &data, &size, 1,
                r->device_props.limits.minUniformBufferOffsetAlignment);
        }

        r->uniforms_changed = false;
    }

    VkDescriptorBufferInfo ubo_buffer_infos[2];
    for (int i = 0; i < ARRAY_SIZE(layouts); i++) {
        ubo_buffer_infos[i] = (VkDescriptorBufferInfo){
            .buffer = r->storage_buffers[BUFFER_UNIFORM].buffer,
            .offset = r->uniform_buffer_offsets[i],
            .range = layouts[i]->total_size,
        };
        descriptor_writes[i] = (VkWriteDescriptorSet){
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = r->descriptor_sets[r->descriptor_set_index],
            .dstBinding = i == 0 ? VSH_UBO_BINDING : PSH_UBO_BINDING,
            .dstArrayElement = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .pBufferInfo = &ubo_buffer_infos[i],
        };
    }

    VkDescriptorImageInfo image_infos[NV2A_MAX_TEXTURES];
    for (int i = 0; i < NV2A_MAX_TEXTURES; i++) {
        image_infos[i] = (VkDescriptorImageInfo){
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .imageView = r->texture_bindings[i]->image_view,
            .sampler = r->texture_bindings[i]->sampler,
        };
        descriptor_writes[2 + i] = (VkWriteDescriptorSet){
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = r->descriptor_sets[r->descriptor_set_index],
            .dstBinding = PSH_TEX_BINDING + i,
            .dstArrayElement = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .pImageInfo = &image_infos[i],
        };
    }

    vkUpdateDescriptorSets(r->device, 6, descriptor_writes, 0, NULL);

    r->descriptor_set_index++;
}

static void update_shader_constant_locations(ShaderBinding *binding)
{
    char tmp[64];

    /* lookup fragment shader uniforms */
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 2; j++) {
            snprintf(tmp, sizeof(tmp), "c%d_%d", j, i);
            binding->psh_constant_loc[i][j] =
                uniform_index(&binding->fragment->uniforms, tmp);
        }
    }
    binding->alpha_ref_loc =
        uniform_index(&binding->fragment->uniforms, "alphaRef");
    binding->fog_color_loc =
        uniform_index(&binding->fragment->uniforms, "fogColor");
    for (int i = 1; i < NV2A_MAX_TEXTURES; i++) {
        snprintf(tmp, sizeof(tmp), "bumpMat%d", i);
        binding->bump_mat_loc[i] =
            uniform_index(&binding->fragment->uniforms, tmp);
        snprintf(tmp, sizeof(tmp), "bumpScale%d", i);
        binding->bump_scale_loc[i] =
            uniform_index(&binding->fragment->uniforms, tmp);
        snprintf(tmp, sizeof(tmp), "bumpOffset%d", i);
        binding->bump_offset_loc[i] =
            uniform_index(&binding->fragment->uniforms, tmp);
    }

    for (int i = 0; i < NV2A_MAX_TEXTURES; i++) {
        snprintf(tmp, sizeof(tmp), "texScale%d", i);
        binding->tex_scale_loc[i] =
            uniform_index(&binding->fragment->uniforms, tmp);
    }

    /* lookup vertex shader uniforms */
    binding->vsh_constant_loc = uniform_index(&binding->vertex->uniforms, "c");
    binding->surface_size_loc =
        uniform_index(&binding->vertex->uniforms, "surfaceSize");
    binding->clip_range_loc =
        uniform_index(&binding->vertex->uniforms, "clipRange");
    binding->fog_param_loc =
        uniform_index(&binding->vertex->uniforms, "fogParam");

    binding->inv_viewport_loc =
        uniform_index(&binding->vertex->uniforms, "invViewport");
    binding->ltctxa_loc = uniform_index(&binding->vertex->uniforms, "ltctxa");
    binding->ltctxb_loc = uniform_index(&binding->vertex->uniforms, "ltctxb");
    binding->ltc1_loc = uniform_index(&binding->vertex->uniforms, "ltc1");

    for (int i = 0; i < NV2A_MAX_LIGHTS; i++) {
        snprintf(tmp, sizeof(tmp), "lightInfiniteHalfVector%d", i);
        binding->light_infinite_half_vector_loc[i] =
            uniform_index(&binding->vertex->uniforms, tmp);
        snprintf(tmp, sizeof(tmp), "lightInfiniteDirection%d", i);
        binding->light_infinite_direction_loc[i] =
            uniform_index(&binding->vertex->uniforms, tmp);

        snprintf(tmp, sizeof(tmp), "lightLocalPosition%d", i);
        binding->light_local_position_loc[i] =
            uniform_index(&binding->vertex->uniforms, tmp);
        snprintf(tmp, sizeof(tmp), "lightLocalAttenuation%d", i);
        binding->light_local_attenuation_loc[i] =
            uniform_index(&binding->vertex->uniforms, tmp);
    }

    binding->clip_region_loc =
        uniform_index(&binding->fragment->uniforms, "clipRegion");

    binding->material_alpha_loc =
        uniform_index(&binding->vertex->uniforms, "material_alpha");

    binding->uniform_attrs_loc =
        uniform_index(&binding->vertex->uniforms, "inlineValue");
}

static void shader_cache_entry_init(Lru *lru, LruNode *node, void *state)
{
    ShaderBinding *snode = container_of(node, ShaderBinding, node);
    memcpy(&snode->state, state, sizeof(ShaderState));
    snode->initialized = false;
}

static void shader_cache_entry_post_evict(Lru *lru, LruNode *node)
{
    PGRAPHVkState *r = container_of(lru, PGRAPHVkState, shader_cache);
    ShaderBinding *snode = container_of(node, ShaderBinding, node);

    ShaderModuleInfo *modules[] = {
        snode->geometry,
        snode->vertex,
        snode->fragment,
    };
    for (int i = 0; i < ARRAY_SIZE(modules); i++) {
        if (modules[i]) {
            pgraph_vk_destroy_shader_module(r, modules[i]);
        }
    }

    snode->initialized = false;
}

static bool shader_cache_entry_compare(Lru *lru, LruNode *node, void *key)
{
    ShaderBinding *snode = container_of(node, ShaderBinding, node);
    return memcmp(&snode->state, key, sizeof(ShaderState));
}

static void shader_cache_init(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    const size_t shader_cache_size = 1024;
    lru_init(&r->shader_cache);
    r->shader_cache_entries = g_malloc_n(shader_cache_size, sizeof(ShaderBinding));
    assert(r->shader_cache_entries != NULL);
    for (int i = 0; i < shader_cache_size; i++) {
        lru_add_free(&r->shader_cache, &r->shader_cache_entries[i].node);
    }
    r->shader_cache.init_node = shader_cache_entry_init;
    r->shader_cache.compare_nodes = shader_cache_entry_compare;
    r->shader_cache.post_node_evict = shader_cache_entry_post_evict;
}

static void shader_cache_finalize(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    lru_flush(&r->shader_cache);
    g_free(r->shader_cache_entries);
    r->shader_cache_entries = NULL;
}

static ShaderBinding *gen_shaders(PGRAPHState *pg, ShaderState *state)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    uint64_t hash = fast_hash((void *)state, sizeof(*state));
    LruNode *node = lru_lookup(&r->shader_cache, hash, state);
    ShaderBinding *snode = container_of(node, ShaderBinding, node);

    NV2A_VK_DPRINTF("shader state hash: %016" PRIx64 " %p", hash, snode);

    if (!snode->initialized) {
        NV2A_VK_DPRINTF("cache miss");
        nv2a_profile_inc_counter(NV2A_PROF_SHADER_GEN);

        char *previous_numeric_locale = setlocale(LC_NUMERIC, NULL);
        if (previous_numeric_locale) {
            previous_numeric_locale = g_strdup(previous_numeric_locale);
        }

        /* Ensure numeric values are printed with '.' radix, no grouping */
        setlocale(LC_NUMERIC, "C");

        MString *geometry_shader_code = pgraph_gen_geom_glsl(
            state->polygon_front_mode, state->polygon_back_mode,
            state->primitive_mode, state->smooth_shading, true);
        if (geometry_shader_code) {
            NV2A_VK_DPRINTF("geometry shader: \n%s",
                            mstring_get_str(geometry_shader_code));
            snode->geometry = pgraph_vk_create_shader_module_from_glsl(
                r, VK_SHADER_STAGE_GEOMETRY_BIT,
                mstring_get_str(geometry_shader_code));
            mstring_unref(geometry_shader_code);
        } else {
            snode->geometry = NULL;
        }

        MString *vertex_shader_code =
            pgraph_gen_vsh_glsl(state, geometry_shader_code != NULL);
        NV2A_VK_DPRINTF("vertex shader: \n%s",
                        mstring_get_str(vertex_shader_code));
        snode->vertex = pgraph_vk_create_shader_module_from_glsl(
            r, VK_SHADER_STAGE_VERTEX_BIT,
            mstring_get_str(vertex_shader_code));
        mstring_unref(vertex_shader_code);

        MString *fragment_shader_code = pgraph_gen_psh_glsl(state->psh);
        NV2A_VK_DPRINTF("fragment shader: \n%s",
                        mstring_get_str(fragment_shader_code));
        snode->fragment = pgraph_vk_create_shader_module_from_glsl(
            r, VK_SHADER_STAGE_FRAGMENT_BIT,
            mstring_get_str(fragment_shader_code));
        mstring_unref(fragment_shader_code);

        if (previous_numeric_locale) {
            setlocale(LC_NUMERIC, previous_numeric_locale);
            g_free(previous_numeric_locale);
        }

        update_shader_constant_locations(snode);

        snode->initialized = true;
    }

    return snode;
}

static void update_uniform_attr_values(PGRAPHState *pg, ShaderBinding *binding)
{
    float values[NV2A_VERTEXSHADER_ATTRIBUTES][4];
    int num_uniform_attrs = 0;

    pgraph_get_inline_values(pg, binding->state.uniform_attrs, values,
                             &num_uniform_attrs);

    if (num_uniform_attrs > 0) {
        uniform1fv(&binding->vertex->uniforms, binding->uniform_attrs_loc,
                   num_uniform_attrs * 4, &values[0][0]);
    }
}

// FIXME: Move to common
static void shader_update_constants(PGRAPHState *pg, ShaderBinding *binding,
                                    bool binding_changed, bool vertex_program,
                                    bool fixed_function)
{
    ShaderState *state = &binding->state;

    /* update combiner constants */
    for (int i = 0; i < 9; i++) {
        uint32_t constant[2];
        if (i == 8) {
            /* final combiner */
            constant[0] = pgraph_reg_r(pg, NV_PGRAPH_SPECFOGFACTOR0);
            constant[1] = pgraph_reg_r(pg, NV_PGRAPH_SPECFOGFACTOR1);
        } else {
            constant[0] = pgraph_reg_r(pg, NV_PGRAPH_COMBINEFACTOR0 + i * 4);
            constant[1] = pgraph_reg_r(pg, NV_PGRAPH_COMBINEFACTOR1 + i * 4);
        }

        for (int j = 0; j < 2; j++) {
            GLint loc = binding->psh_constant_loc[i][j];
            if (loc != -1) {
                float value[4];
                pgraph_argb_pack32_to_rgba_float(constant[j], value);
                uniform1fv(&binding->fragment->uniforms, loc, 4, value);
            }
        }
    }
    if (binding->alpha_ref_loc != -1) {
        float alpha_ref = GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_CONTROL_0),
                                   NV_PGRAPH_CONTROL_0_ALPHAREF) /
                          255.0;
        uniform1f(&binding->fragment->uniforms, binding->alpha_ref_loc,
                         alpha_ref);
    }


    /* For each texture stage */
    for (int i = 0; i < NV2A_MAX_TEXTURES; i++) {
        int loc;

        /* Bump luminance only during stages 1 - 3 */
        if (i > 0) {
            loc = binding->bump_mat_loc[i];
            if (loc != -1) {
                uint32_t m_u32[4];
                m_u32[0] = pgraph_reg_r(pg, NV_PGRAPH_BUMPMAT00 + 4 * (i - 1));
                m_u32[1] = pgraph_reg_r(pg, NV_PGRAPH_BUMPMAT01 + 4 * (i - 1));
                m_u32[2] = pgraph_reg_r(pg, NV_PGRAPH_BUMPMAT10 + 4 * (i - 1));
                m_u32[3] = pgraph_reg_r(pg, NV_PGRAPH_BUMPMAT11 + 4 * (i - 1));
                float m[4];
                m[0] = *(float*)&m_u32[0];
                m[1] = *(float*)&m_u32[1];
                m[2] = *(float*)&m_u32[2];
                m[3] = *(float*)&m_u32[3];
                uniformMatrix2fv(&binding->fragment->uniforms, loc, m);
            }
            loc = binding->bump_scale_loc[i];
            if (loc != -1) {
                uint32_t v =
                    pgraph_reg_r(pg, NV_PGRAPH_BUMPSCALE1 + (i - 1) * 4);
                uniform1f(&binding->fragment->uniforms, loc,
                                 *(float *)&v);
            }
            loc = binding->bump_offset_loc[i];
            if (loc != -1) {
                uint32_t v =
                    pgraph_reg_r(pg, NV_PGRAPH_BUMPOFFSET1 + (i - 1) * 4);
                uniform1f(&binding->fragment->uniforms, loc,
                                 *(float *)&v);
            }
        }

        loc = binding->tex_scale_loc[i];
        if (loc != -1) {
            assert(pg->vk_renderer_state->texture_bindings[i] != NULL);
            float scale = pg->vk_renderer_state->texture_bindings[i]->key.scale;
            BasicColorFormatInfo f_basic = kelvin_color_format_info_map[pg->vk_renderer_state->texture_bindings[i]->key.state.color_format];
            if (!f_basic.linear) {
                scale = 1.0;
            }
            uniform1f(&binding->fragment->uniforms, loc, scale);
        }
    }

    if (binding->fog_color_loc != -1) {
        uint32_t fog_color = pgraph_reg_r(pg, NV_PGRAPH_FOGCOLOR);
        uniform4f(&binding->fragment->uniforms, binding->fog_color_loc,
                         GET_MASK(fog_color, NV_PGRAPH_FOGCOLOR_RED) / 255.0,
                         GET_MASK(fog_color, NV_PGRAPH_FOGCOLOR_GREEN) / 255.0,
                         GET_MASK(fog_color, NV_PGRAPH_FOGCOLOR_BLUE) / 255.0,
                         GET_MASK(fog_color, NV_PGRAPH_FOGCOLOR_ALPHA) / 255.0);
    }
    if (binding->fog_param_loc != -1) {
        uint32_t v[2];
        v[0] = pgraph_reg_r(pg, NV_PGRAPH_FOGPARAM0);
        v[1] = pgraph_reg_r(pg, NV_PGRAPH_FOGPARAM1);
        uniform2f(&binding->vertex->uniforms,
                         binding->fog_param_loc, *(float *)&v[0],
                         *(float *)&v[1]);
    }

    float zmax;
    switch (pg->surface_shape.zeta_format) {
    case NV097_SET_SURFACE_FORMAT_ZETA_Z16:
        zmax = pg->surface_shape.z_format ? f16_max : (float)0xFFFF;
        break;
    case NV097_SET_SURFACE_FORMAT_ZETA_Z24S8:
        zmax = pg->surface_shape.z_format ? f24_max : (float)0xFFFFFF;
        break;
    default:
        assert(0);
    }

    if (fixed_function) {
        /* update lighting constants */
        struct {
            uint32_t *v;
            int locs;
            size_t len;
        } lighting_arrays[] = {
            { &pg->ltctxa[0][0], binding->ltctxa_loc, NV2A_LTCTXA_COUNT },
            { &pg->ltctxb[0][0], binding->ltctxb_loc, NV2A_LTCTXB_COUNT },
            { &pg->ltc1[0][0], binding->ltc1_loc, NV2A_LTC1_COUNT },
        };

        for (int i = 0; i < ARRAY_SIZE(lighting_arrays); i++) {
            uniform1iv(
                &binding->vertex->uniforms, lighting_arrays[i].locs,
                lighting_arrays[i].len * 4, (void *)lighting_arrays[i].v);
        }

        for (int i = 0; i < NV2A_MAX_LIGHTS; i++) {
            int loc = binding->light_infinite_half_vector_loc[i];
            if (loc != -1) {
                uniform1fv(&binding->vertex->uniforms, loc, 3,
                                 pg->light_infinite_half_vector[i]);
            }
            loc = binding->light_infinite_direction_loc[i];
            if (loc != -1) {
                uniform1fv(&binding->vertex->uniforms, loc, 3,
                                 pg->light_infinite_direction[i]);
            }

            loc = binding->light_local_position_loc[i];
            if (loc != -1) {
                uniform1fv(&binding->vertex->uniforms, loc, 3,
                                 pg->light_local_position[i]);
            }
            loc = binding->light_local_attenuation_loc[i];
            if (loc != -1) {
                uniform1fv(&binding->vertex->uniforms, loc, 3,
                                 pg->light_local_attenuation[i]);
            }
        }

        /* estimate the viewport by assuming it matches the surface ... */
        unsigned int aa_width = 1, aa_height = 1;
        pgraph_apply_anti_aliasing_factor(pg, &aa_width, &aa_height);

        float m11 = 0.5 * (pg->surface_binding_dim.width / aa_width);
        float m22 = -0.5 * (pg->surface_binding_dim.height / aa_height);
        float m33 = zmax;
        float m41 = *(float *)&pg->vsh_constants[NV_IGRAPH_XF_XFCTX_VPOFF][0];
        float m42 = *(float *)&pg->vsh_constants[NV_IGRAPH_XF_XFCTX_VPOFF][1];

        float invViewport[16] = {
            1.0 / m11, 0,  0, 0,         0, 1.0 / m22,        0,
            0,         0,  0, 1.0 / m33, 0, -1.0 + m41 / m11, 1.0 + m42 / m22,
            0,         1.0
        };

        if (binding->inv_viewport_loc != -1) {
            uniformMatrix4fv(&binding->vertex->uniforms,
                                    binding->inv_viewport_loc, &invViewport[0]);
        }
    }

    /* update vertex program constants */
    uniform1iv(&binding->vertex->uniforms, binding->vsh_constant_loc,
               NV2A_VERTEXSHADER_CONSTANTS * 4, (void *)pg->vsh_constants);

    if (binding->surface_size_loc != -1) {
        unsigned int aa_width = 1, aa_height = 1;
        pgraph_apply_anti_aliasing_factor(pg, &aa_width, &aa_height);
        uniform2f(&binding->vertex->uniforms, binding->surface_size_loc,
                         pg->surface_binding_dim.width / aa_width,
                         pg->surface_binding_dim.height / aa_height);
    }

    if (binding->clip_range_loc != -1) {
        uint32_t v[2];
        v[0] = pgraph_reg_r(pg, NV_PGRAPH_ZCLIPMIN);
        v[1] = pgraph_reg_r(pg, NV_PGRAPH_ZCLIPMAX);
        float zclip_min = *(float *)&v[0] / zmax * 2.0 - 1.0;
        float zclip_max = *(float *)&v[1] / zmax * 2.0 - 1.0;
        uniform4f(&binding->vertex->uniforms, binding->clip_range_loc, 0,
                         zmax, zclip_min, zclip_max);
    }

    /* Clipping regions */
    unsigned int max_gl_width = pg->surface_binding_dim.width;
    unsigned int max_gl_height = pg->surface_binding_dim.height;
    pgraph_apply_scaling_factor(pg, &max_gl_width, &max_gl_height);

    uint32_t clip_regions[8][4];

    for (int i = 0; i < 8; i++) {
        uint32_t x = pgraph_reg_r(pg, NV_PGRAPH_WINDOWCLIPX0 + i * 4);
        unsigned int x_min = GET_MASK(x, NV_PGRAPH_WINDOWCLIPX0_XMIN);
        unsigned int x_max = GET_MASK(x, NV_PGRAPH_WINDOWCLIPX0_XMAX) + 1;
        uint32_t y = pgraph_reg_r(pg, NV_PGRAPH_WINDOWCLIPY0 + i * 4);
        unsigned int y_min = GET_MASK(y, NV_PGRAPH_WINDOWCLIPY0_YMIN);
        unsigned int y_max = GET_MASK(y, NV_PGRAPH_WINDOWCLIPY0_YMAX) + 1;
        pgraph_apply_anti_aliasing_factor(pg, &x_min, &y_min);
        pgraph_apply_anti_aliasing_factor(pg, &x_max, &y_max);

        pgraph_apply_scaling_factor(pg, &x_min, &y_min);
        pgraph_apply_scaling_factor(pg, &x_max, &y_max);

        clip_regions[i][0] = x_min;
        clip_regions[i][1] = y_min;
        clip_regions[i][2] = x_max;
        clip_regions[i][3] = y_max;
    }
    uniform1iv(&binding->fragment->uniforms, binding->clip_region_loc,
                     8 * 4, (void *)clip_regions);

    if (binding->material_alpha_loc != -1) {
        uniform1f(&binding->vertex->uniforms, binding->material_alpha_loc,
                         pg->material_alpha);
    }

    if (!state->use_push_constants_for_uniform_attrs && state->uniform_attrs) {
        update_uniform_attr_values(pg, binding);
    }
}

// Quickly check PGRAPH state to see if any registers have changed that
// necessitate a full shader state inspection.
static bool check_shaders_dirty(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;

    if (!r->shader_binding) {
        return true;
    }
    if (pg->program_data_dirty) {
        return true;
    }

    int num_stages = pgraph_reg_r(pg, NV_PGRAPH_COMBINECTL) & 0xFF;
    for (int i = 0; i < num_stages; i++) {
        if (pgraph_is_reg_dirty(pg, NV_PGRAPH_COMBINEALPHAI0 + i * 4) ||
            pgraph_is_reg_dirty(pg, NV_PGRAPH_COMBINEALPHAO0 + i * 4) ||
            pgraph_is_reg_dirty(pg, NV_PGRAPH_COMBINECOLORI0 + i * 4) ||
            pgraph_is_reg_dirty(pg, NV_PGRAPH_COMBINECOLORO0 + i * 4)) {
            return true;
        }
    }

    unsigned int regs[] = {
        NV_PGRAPH_COMBINECTL,
        NV_PGRAPH_COMBINESPECFOG0,
        NV_PGRAPH_COMBINESPECFOG1,
        NV_PGRAPH_CONTROL_0,
        NV_PGRAPH_CONTROL_3,
        NV_PGRAPH_CSV0_C,
        NV_PGRAPH_CSV0_D,
        NV_PGRAPH_CSV1_A,
        NV_PGRAPH_CSV1_B,
        NV_PGRAPH_POINTSIZE,
        NV_PGRAPH_SETUPRASTER,
        NV_PGRAPH_SHADERCLIPMODE,
        NV_PGRAPH_SHADERCTL,
        NV_PGRAPH_SHADERPROG,
        NV_PGRAPH_SHADOWCTL,
    };
    for (int i = 0; i < ARRAY_SIZE(regs); i++) {
        if (pgraph_is_reg_dirty(pg, regs[i])) {
            return true;
        }
    }

    ShaderState *state = &r->shader_binding->state;
    if (pg->uniform_attrs != state->uniform_attrs ||
        pg->swizzle_attrs != state->swizzle_attrs ||
        pg->compressed_attrs != state->compressed_attrs ||
        pg->primitive_mode != state->primitive_mode ||
        pg->surface_scale_factor != state->surface_scale_factor) {
        return true;
    }

    // Textures
    for (int i = 0; i < 4; i++) {
        if (pg->texture_matrix_enable[i] != pg->vk_renderer_state->shader_binding->state.texture_matrix_enable[i] ||
            pgraph_is_reg_dirty(pg, NV_PGRAPH_TEXCTL0_0 + i * 4) ||
            pgraph_is_reg_dirty(pg, NV_PGRAPH_TEXFILTER0 + i * 4) ||
            pgraph_is_reg_dirty(pg, NV_PGRAPH_TEXFMT0 + i * 4)) {
            return true;
        }
    }

    nv2a_profile_inc_counter(NV2A_PROF_SHADER_BIND_NOTDIRTY);

    return false;
}

void pgraph_vk_bind_shaders(PGRAPHState *pg)
{
    NV2A_VK_DGROUP_BEGIN("%s", __func__);

    PGRAPHVkState *r = pg->vk_renderer_state;

    r->shader_bindings_changed = false;

    if (check_shaders_dirty(pg)) {
        ShaderState new_state;
        memset(&new_state, 0, sizeof(ShaderState));
        new_state = pgraph_get_shader_state(pg);
        new_state.vulkan = true;
        new_state.psh.vulkan = true;
        new_state.use_push_constants_for_uniform_attrs =
            (r->device_props.limits.maxPushConstantsSize >=
             MAX_UNIFORM_ATTR_VALUES_SIZE);

        if (!r->shader_binding || memcmp(&r->shader_binding->state, &new_state, sizeof(ShaderState))) {
            r->shader_binding = gen_shaders(pg, &new_state);
            r->shader_bindings_changed = true;
        }
    }

    // FIXME: Use dirty bits
    pgraph_vk_update_shader_uniforms(pg);

    NV2A_VK_DGROUP_END();
}

void pgraph_vk_update_shader_uniforms(PGRAPHState *pg)
{
    PGRAPHVkState *r = pg->vk_renderer_state;
    NV2A_VK_DGROUP_BEGIN("%s", __func__);
    nv2a_profile_inc_counter(NV2A_PROF_SHADER_BIND);

    assert(r->shader_binding);
    ShaderBinding *binding = r->shader_binding;
    ShaderUniformLayout *layouts[] = { &binding->vertex->uniforms,
                                        &binding->fragment->uniforms };
    shader_update_constants(pg, r->shader_binding, true,
                            r->shader_binding->state.vertex_program,
                            r->shader_binding->state.fixed_function);

    for (int i = 0; i < ARRAY_SIZE(layouts); i++) {
        uint64_t hash = fast_hash(layouts[i]->allocation, layouts[i]->total_size);
        r->uniforms_changed |= (hash != r->uniform_buffer_hashes[i]);
        r->uniform_buffer_hashes[i] = hash;
    }

    nv2a_profile_inc_counter(r->uniforms_changed ?
                                 NV2A_PROF_SHADER_UBO_DIRTY :
                                 NV2A_PROF_SHADER_UBO_NOTDIRTY);

    NV2A_VK_DGROUP_END();
}

void pgraph_vk_init_shaders(PGRAPHState *pg)
{
    pgraph_vk_init_glsl_compiler();
    create_descriptor_pool(pg);
    create_descriptor_set_layout(pg);
    create_descriptor_sets(pg);
    shader_cache_init(pg);
}

void pgraph_vk_finalize_shaders(PGRAPHState *pg)
{
    shader_cache_finalize(pg);
    destroy_descriptor_sets(pg);
    destroy_descriptor_set_layout(pg);
    destroy_descriptor_pool(pg);
    pgraph_vk_finalize_glsl_compiler();
}
