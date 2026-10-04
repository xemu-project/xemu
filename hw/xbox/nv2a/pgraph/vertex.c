/*
 * QEMU Geforce NV2A implementation
 *
 * Copyright (c) 2012 espes
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

#include "hw/xbox/nv2a/nv2a_int.h"
#include "nv2a_vsh_emulator.h"
#include "nv2a_vsh_disassembler.h"

void pgraph_update_inline_value(VertexAttribute *attr, const uint8_t *data)
{
    assert(attr->count <= 4);
    attr->inline_value[0] = 0.0f;
    attr->inline_value[1] = 0.0f;
    attr->inline_value[2] = 0.0f;
    attr->inline_value[3] = 1.0f;

    switch (attr->format) {
        case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_UB_D3D:
        case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_UB_OGL:
            for (uint32_t i = 0; i < attr->count; ++i) {
                attr->inline_value[i] = (float)data[i] / 255.0f;
            }
            break;
        case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_S1: {
            const int16_t *val = (const int16_t *) data;
            for (uint32_t i = 0; i < attr->count; ++i, ++val) {
                attr->inline_value[i] = MAX(-1.0f, (float) *val / 32767.0f);
            }
            break;
        }
        case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F:
            memcpy(attr->inline_value, data, attr->size * attr->count);
            break;
        case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_S32K: {
            const int16_t *val = (const int16_t *) data;
            for (uint32_t i = 0; i < attr->count; ++i, ++val) {
                attr->inline_value[i] = (float)*val;
            }
            break;
        }
        case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_CMP: {
            /* 3 signed, normalized components packed in 32-bits. (11,11,10) */
            const int32_t val = *(const int32_t *)data;
            int32_t x = val & 0x7FF;
            if (x & 0x400) {
                x |= 0xFFFFF800;
            }
            int32_t y = (val >> 11) & 0x7FF;
            if (y & 0x400) {
                y |= 0xFFFFF800;
            }
            int32_t z = (val >> 22) & 0x7FF;
            if (z & 0x200) {
                z |= 0xFFFFFC00;
            }

            attr->inline_value[0] = MAX(-1.0f, (float)x / 1023.0f);
            attr->inline_value[1] = MAX(-1.0f, (float)y / 1023.0f);
            attr->inline_value[2] = MAX(-1.0f, (float)z / 511.0f);
            break;
        }
    default:
        fprintf(stderr, "Unknown vertex attribute type: for format 0x%x\n",
                attr->format);
        assert(!"Unsupported attribute type");
        break;
    }
}

void pgraph_get_inline_values(PGRAPHState *pg, uint16_t attrs,
                               float values[NV2A_VERTEXSHADER_ATTRIBUTES][4],
                               int *count)
{
    int num_attributes = 0;

    for (int i = 0; i < NV2A_VERTEXSHADER_ATTRIBUTES; i++) {
        if (attrs & (1 << i)) {
            memcpy(values[num_attributes],
                   pg->vertex_attributes[i].inline_value, 4 * sizeof(float));
            num_attributes += 1;
        }
    }

    if (count) {
        *count = num_attributes;
    }
}


void pgraph_allocate_inline_buffer_vertices(PGRAPHState *pg, unsigned int attr)
{
    VertexAttribute *attribute = &pg->vertex_attributes[attr];

    if (attribute->inline_buffer_populated || pg->inline_buffer_length == 0) {
        return;
    }

    /* Now upload the previous attribute value */
    attribute->inline_buffer_populated = true;
    for (int i = 0; i < pg->inline_buffer_length; i++) {
        memcpy(&attribute->inline_buffer[i * 4], attribute->inline_value,
               sizeof(float) * 4);
    }
}

void pgraph_finish_inline_buffer_vertex(PGRAPHState *pg)
{
    pgraph_check_within_begin_end_block(pg);
    assert(pg->inline_buffer_length < NV2A_MAX_BATCH_LENGTH);

    for (int i = 0; i < NV2A_VERTEXSHADER_ATTRIBUTES; i++) {
        VertexAttribute *attribute = &pg->vertex_attributes[i];
        if (attribute->inline_buffer_populated) {
            memcpy(&attribute->inline_buffer[pg->inline_buffer_length * 4],
                   attribute->inline_value, sizeof(float) * 4);
        }
    }

    pg->inline_buffer_length++;
}

void pgraph_reset_inline_buffers(PGRAPHState *pg)
{
    pg->inline_elements_length = 0;
    pg->inline_array_length = 0;
    pg->inline_buffer_length = 0;
    pgraph_reset_draw_arrays(pg);
}

void pgraph_reset_draw_arrays(PGRAPHState *pg)
{
    pg->draw_arrays_length = 0;
    pg->draw_arrays_min_start = -1;
    pg->draw_arrays_max_count = 0;
    pg->draw_arrays_prevent_connect = false;
}

void pgraph_vsh_carryover_invalidate_program(PGRAPHState *pg)
{
    if (pg->vsh_carry_cached_program) {
        nv2a_vsh_program_destroy(pg->vsh_carry_cached_program);
        g_free(pg->vsh_carry_cached_program);
        pg->vsh_carry_cached_program = NULL;
    }
    pg->vsh_carry_cached_start = (uint32_t)-1;
}

void pgraph_vsh_carryover_reset(PGRAPHState *pg)
{
    pg->vsh_carry_fog[0] = 0.0f;
    pg->vsh_carry_fog[1] = 0.0f;
    pg->vsh_carry_fog[2] = 0.0f;
    pg->vsh_carry_fog[3] = 1.0f;
    pgraph_vsh_carryover_invalidate_program(pg);
}

void pgraph_vsh_carryover_update(PGRAPHState *pg)
{
    bool is_vertex_program = GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_CSV0_D),
                                      NV_PGRAPH_CSV0_D_MODE) == 2;
    if (!is_vertex_program) {
        return;
    }

    uint32_t program_start = GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_CSV0_C),
                                      NV_PGRAPH_CSV0_C_CHEOPS_PROGRAM_START);
    if (program_start >= NV2A_MAX_TRANSFORM_PROGRAM_LENGTH) {
        return;
    }

    if (!pg->vsh_carry_cached_program ||
        pg->vsh_carry_cached_start != program_start) {
        pgraph_vsh_carryover_invalidate_program(pg);
        Nv2aVshProgram *program = g_new0(Nv2aVshProgram, 1);
        Nv2aVshParseResult result = nv2a_vsh_parse_program(
            program, pg->program_data[program_start],
            NV2A_MAX_TRANSFORM_PROGRAM_LENGTH - program_start);
        if (result != NV2AVPR_SUCCESS) {
            g_free(program);
            return;
        }
        pg->vsh_carry_cached_program = program;
        pg->vsh_carry_cached_start = program_start;
    }

    Nv2aVshCPUFullExecutionState execution_state;
    Nv2aVshExecutionState state =
        nv2a_vsh_emu_initialize_full_execution_state(&execution_state);

    for (int i = 0; i < ARRAY_SIZE(pg->vertex_attributes); ++i) {
        memcpy(&execution_state.input_regs[i * 4],
               pg->vertex_attributes[i].inline_value, sizeof(float) * 4);
    }

    memcpy(&execution_state.output_regs[NV2AOR_FOG_COORD * 4],
           pg->vsh_carry_fog, sizeof(pg->vsh_carry_fog));

    QEMU_BUILD_BUG_MSG(sizeof(execution_state.context_regs) !=
                           sizeof(pg->vsh_constants),
                       "context_regs and vsh_constants size mismatch");
    memcpy(execution_state.context_regs, pg->vsh_constants,
           sizeof(execution_state.context_regs));

    nv2a_vsh_emu_execute(&state, pg->vsh_carry_cached_program);

    memcpy(pg->vsh_carry_fog,
           &execution_state.output_regs[NV2AOR_FOG_COORD * 4],
           sizeof(pg->vsh_carry_fog));
}
