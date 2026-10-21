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

#ifndef HW_XBOX_NV2A_PGRAPH_VK_GLSL_H
#define HW_XBOX_NV2A_PGRAPH_VK_GLSL_H

#include "qemu/osdep.h"
#include <stdint.h>
#include <assert.h>
#include <string.h>

typedef struct ShaderUniform {
	const char *name;
	size_t dim_v;
	size_t dim_a;
	size_t align;
	size_t stride;
	size_t offset;
} ShaderUniform;

typedef struct ShaderUniformLayout {
	ShaderUniform *uniforms;
	size_t num_uniforms;
	size_t total_size;
	void *allocation;
} ShaderUniformLayout;

static inline void uniform_std140(ShaderUniformLayout *layout)
{
	size_t offset = 0;

	for (int i = 0; i < layout->num_uniforms; i++) {
		ShaderUniform *u = &layout->uniforms[i];
		size_t size = sizeof(float); // float or int
		size_t align = size;
		size_t stride = 0;

		size *= u->dim_v;
		align *= u->dim_v == 3 ? 4 : u->dim_v;

		// If an array, each element is padded to vec4.
		if (u->dim_a > 1) {
			align = 4 * sizeof(float);
			stride = align;
			size = u->dim_a * align;
		} else {
			align = size;
			stride = 0;
		}

		offset = ROUND_UP(offset, align);

		u->align = align;
		u->offset = offset;
		u->stride = stride;

		offset += size;
	}

	layout->total_size = offset;
	assert(layout->total_size);
}

static inline void uniform_std430(ShaderUniformLayout *layout)
{
	size_t offset = 0;

	for (int i = 0; i < layout->num_uniforms; i++) {
		ShaderUniform *u = &layout->uniforms[i];
		size_t size = sizeof(float); // float or int
		size *= u->dim_v;
		size_t align = size;
		size *= u->dim_a;

		offset = ROUND_UP(offset, align);

		u->align = align;
		u->offset = offset;
		u->stride = u->dim_a > 1 ? (size * u->dim_v) : 0;

		offset += size;
	}

	layout->total_size = offset;
	assert(layout->total_size);
}

static inline int uniform_index(ShaderUniformLayout *layout, const char *name)
{
    for (int i = 0; i < layout->num_uniforms; i++) {
        if (!strcmp(layout->uniforms[i].name, name)) {
            return i + 1;
        }
    }

    return -1;
}

static inline
void *uniform_ptr(ShaderUniformLayout *layout, int idx)
{
	assert(idx > 0 && "invalid uniform index");

    return (char *)layout->allocation + layout->uniforms[idx - 1].offset;
}

static inline void uniform_copy(ShaderUniformLayout *layout, int idx,
                                void *values, size_t value_size, size_t count)
{
    assert(idx > 0 && "invalid uniform index");

    ShaderUniform *u = &layout->uniforms[idx - 1];
    const size_t element_size = value_size * u->dim_v;

    size_t bytes_remaining = value_size * count;
    char *p_out = uniform_ptr(layout, idx);
    char *p_max = p_out + layout->total_size;
    char *p_in = (char *)values;

    int index = 0;
    while (bytes_remaining) {
        assert((p_out + element_size) <= p_max);
        assert(index < u->dim_a);
        memcpy(p_out, p_in, element_size);
        bytes_remaining -= element_size;
        p_out += u->stride;
        p_in += element_size;
        index += 1;
    }
}

static inline
void uniform1fv(ShaderUniformLayout *layout, int idx, size_t count, float *values)
{
	uniform_copy(layout, idx, values, sizeof(float), count);
}

static inline
void uniform1f(ShaderUniformLayout *layout, int idx, float value)
{
	uniform1fv(layout, idx, 1, &value);
}

static inline
void uniform2f(ShaderUniformLayout *layout, int idx, float v0, float v1)
{
	float values[] = { v0, v1 };
	uniform1fv(layout, idx, 2, values);
}

static inline
void uniform3f(ShaderUniformLayout *layout, int idx, float v0, float v1, float v2)
{
    float values[] = { v0, v1, v2 };
    uniform1fv(layout, idx, 3, values);
}

static inline
void uniform4f(ShaderUniformLayout *layout, int idx, float v0, float v1, float v2, float v3)
{
    float values[] = { v0, v1, v2, v3 };
    uniform1fv(layout, idx, 4, values);
}

static inline
void uniformMatrix2fv(ShaderUniformLayout *layout, int idx, float *values)
{
	uniform1fv(layout, idx, 4, values);
}

static inline
void uniformMatrix4fv(ShaderUniformLayout *layout, int idx, float *values)
{
	uniform1fv(layout, idx, 4 * 4, values);
}

static inline
void uniform1iv(ShaderUniformLayout *layout, int idx, size_t count, int32_t *values)
{
	uniform_copy(layout, idx, values, sizeof(int32_t), count);
}

static inline
void uniform1i(ShaderUniformLayout *layout, int idx, int32_t value)
{
	uniform1iv(layout, idx, 1, &value);
}

static inline
void uniform4i(ShaderUniformLayout *layout, int idx, int v0, int v1, int v2, int v3)
{
	int values[] = { v0, v1, v2, v3 };
	uniform1iv(layout, idx, 4, values);
}

static inline void uniform1uiv(ShaderUniformLayout *layout, int idx,
                               size_t count, uint32_t *values)
{
    uniform_copy(layout, idx, values, sizeof(uint32_t), count);
}

#endif
