/*
 * MCPX DSP emulator - C interpreter backend
 *
 * Copyright (c) 2015 espes
 * Copyright (c) 2020-2025 Matt Borgerson
 *
 * Adapted from Hatari DSP M56001 emulation
 * (C) 2001-2008 ARAnyM developer team
 * Adaption to Hatari (C) 2008 by Thomas Huth
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "qemu/osdep.h"
#include "dsp_internal.h"
#include "interp/dsp_cpu.h"
#include "debug.h"

static dsp_core_t *c_core(DSPState *dsp)
{
    return (dsp_core_t *)dsp->backend;
}

static uint32_t c_dma_mem_read(void *opaque, int space, uint32_t addr)
{
    return dsp56k_read_memory((dsp_core_t *)opaque, space, addr);
}

static void c_dma_mem_write(void *opaque, int space, uint32_t addr,
                            uint32_t value)
{
    dsp56k_write_memory((dsp_core_t *)opaque, space, addr, value);
}

static uint32_t c_read_peripheral(dsp_core_t *core, uint32_t address)
{
    return read_peripheral((DSPState *)core->opaque, address);
}

static void c_write_peripheral(dsp_core_t *core, uint32_t address,
                               uint32_t value)
{
    write_peripheral((DSPState *)core->opaque, address, value);
}

static void dsp_c_reset(DSPState *dsp)
{
    dsp56k_reset_cpu(c_core(dsp));
    dsp->save_cycles = 0;
}

static void dsp_c_step(DSPState *dsp)
{
    dsp56k_execute_instruction(c_core(dsp));
}

static void dsp_c_run(DSPState *dsp, int cycles)
{
    dsp_core_t *core = c_core(dsp);

    dsp->save_cycles += cycles;

    if (dsp->save_cycles <= 0)
        return;

    while (dsp->save_cycles > 0) {
        dsp56k_execute_instruction(core);
        dsp->save_cycles -= core->instr_cycle;
        core->cycle_count += core->instr_cycle;

        if (core->is_idle) {
            break;
        }
    }
}

static void dsp_c_bootstrap(DSPState *dsp)
{
    dsp_core_t *core = c_core(dsp);

    // scratch memory is dma'd in to pram by the bootrom
    dsp->dma.scratch_rw(dsp->dma.rw_opaque, (uint8_t *)core->pram, 0, 0x800 * 4,
                        false);
    for (int i = 0; i < 0x800; i++) {
        if (core->pram[i] & 0xff000000) {
            DPRINTF("Bootstrap %04x: %08x\n", i, core->pram[i]);
            core->pram[i] &= 0x00ffffff;
        }
    }
    memset(core->pram_opcache, 0, sizeof(core->pram_opcache));
}

static uint32_t dsp_c_read_memory(DSPState *dsp, char space, uint32_t address)
{
    int space_id;

    switch (space) {
    case 'X':
        space_id = DSP_SPACE_X;
        break;
    case 'Y':
        space_id = DSP_SPACE_Y;
        break;
    case 'P':
        space_id = DSP_SPACE_P;
        break;
    default:
        assert(false);
        return 0;
    }

    return dsp56k_read_memory(c_core(dsp), space_id, address);
}

static void dsp_c_write_memory(DSPState *dsp, char space, uint32_t address,
                               uint32_t value)
{
    int space_id;

    switch (space) {
    case 'X':
        space_id = DSP_SPACE_X;
        break;
    case 'Y':
        space_id = DSP_SPACE_Y;
        break;
    case 'P':
        space_id = DSP_SPACE_P;
        break;
    default:
        assert(false);
        return;
    }

    dsp56k_write_memory(c_core(dsp), space_id, address, value);
}

static bool dsp_c_get_halt_requested(DSPState *dsp)
{
    return c_core(dsp)->is_idle;
}

static void dsp_c_set_halt_requested(DSPState *dsp, bool halt)
{
    c_core(dsp)->is_idle = halt;
}

static uint32_t dsp_c_get_cycle_count(DSPState *dsp)
{
    return c_core(dsp)->cycle_count;
}

static void dsp_c_set_cycle_count(DSPState *dsp, uint32_t count)
{
    c_core(dsp)->cycle_count = count;
}

static void dsp_c_invalidate_opcache(DSPState *dsp)
{
    memset(c_core(dsp)->pram_opcache, 0, sizeof(c_core(dsp)->pram_opcache));
}

/*
 * Sync: C interpreter -> DspCoreState (before VM save / debug)
 */
static void dsp_c_sync_to_vm(DSPState *dsp)
{
    dsp_core_t *core = c_core(dsp);
    DspCoreState *vm = &dsp->core;

    vm->pc = core->pc;
    vm->cycle_count = core->cycle_count;
    vm->instr_cycle = core->instr_cycle;
    vm->halt_requested = core->is_idle;
    vm->is_gp = core->is_gp;
    vm->loop_rep = core->loop_rep;
    vm->pc_on_rep = core->pc_on_rep;
    vm->cur_inst = core->cur_inst;
    vm->cur_inst_len = core->cur_inst_len;

    memcpy(vm->registers, core->registers, sizeof(vm->registers));
    memcpy(vm->stack, core->stack, sizeof(vm->stack));
    memcpy(vm->xram, core->xram, sizeof(vm->xram));
    memcpy(vm->yram, core->yram, sizeof(vm->yram));
    memcpy(vm->pram, core->pram, sizeof(vm->pram));
    memcpy(vm->mixbuffer, core->mixbuffer, sizeof(vm->mixbuffer));
    memcpy(vm->periph, core->periph, sizeof(vm->periph));

    vm->interrupt_state = core->interrupt_state;
    vm->interrupt_instr_fetch = core->interrupt_instr_fetch;
    vm->interrupt_save_pc = core->interrupt_save_pc;
    vm->interrupt_counter = core->interrupt_counter;
    vm->interrupt_ipl_to_raise = core->interrupt_ipl_to_raise;
    vm->interrupt_pipeline_count = core->interrupt_pipeline_count;
    memcpy(vm->interrupt_ipl, core->interrupt_ipl, sizeof(core->interrupt_ipl));
    memcpy(vm->interrupt_is_pending, core->interrupt_is_pending,
           sizeof(core->interrupt_is_pending));
}

/*
 * Sync: DspCoreState -> C interpreter (after VM load / engine switch)
 */
static void dsp_c_sync_from_vm(DSPState *dsp)
{
    dsp_core_t *core = c_core(dsp);
    DspCoreState *vm = &dsp->core;

    core->pc = vm->pc;
    core->cycle_count = vm->cycle_count;
    core->instr_cycle = vm->instr_cycle;
    core->is_idle = vm->halt_requested;
    core->is_gp = vm->is_gp;
    core->loop_rep = vm->loop_rep;
    core->pc_on_rep = vm->pc_on_rep;
    core->cur_inst = vm->cur_inst;
    core->cur_inst_len = vm->cur_inst_len;

    memcpy(core->registers, vm->registers, sizeof(vm->registers));
    memcpy(core->stack, vm->stack, sizeof(vm->stack));
    memcpy(core->xram, vm->xram, sizeof(vm->xram));
    memcpy(core->yram, vm->yram, sizeof(vm->yram));
    memcpy(core->pram, vm->pram, sizeof(vm->pram));
    memcpy(core->mixbuffer, vm->mixbuffer, sizeof(vm->mixbuffer));
    memcpy(core->periph, vm->periph, sizeof(vm->periph));

    core->interrupt_state = vm->interrupt_state;
    core->interrupt_instr_fetch = vm->interrupt_instr_fetch;
    core->interrupt_save_pc = vm->interrupt_save_pc;
    core->interrupt_counter = vm->interrupt_counter;
    core->interrupt_ipl_to_raise = vm->interrupt_ipl_to_raise;
    core->interrupt_pipeline_count = vm->interrupt_pipeline_count;
    memcpy(core->interrupt_ipl, vm->interrupt_ipl, sizeof(core->interrupt_ipl));
    memcpy(core->interrupt_is_pending, vm->interrupt_is_pending,
           sizeof(core->interrupt_is_pending));

    memset(core->pram_opcache, 0, sizeof(core->pram_opcache));
}

void dsp_c_init(DSPState *dsp)
{
    dsp_core_t *core = g_new0(dsp_core_t, 1);
    core->opaque = dsp;
    core->read_peripheral = c_read_peripheral;
    core->write_peripheral = c_write_peripheral;

    dsp->backend = core;
    dsp->ops = &c_dsp_ops;

    dsp->dma.mem_opaque = core;
    dsp->dma.mem_read = c_dma_mem_read;
    dsp->dma.mem_write = c_dma_mem_write;

    /* Ensure the interpreter's opcode decoder tables are initialized.
     * dsp56k_reset_cpu populates the static nonparallel_matches[] array
     * on first call.  The runtime state it writes (registers, PC, etc.)
     * will be overwritten by the subsequent sync_from_vm. */
    dsp56k_reset_cpu(core);

    memset(core->pram, 0xCA, DSP_PRAM_SIZE * sizeof(uint32_t));
    memset(core->xram, 0xCA, DSP_XRAM_SIZE * sizeof(uint32_t));
    memset(core->yram, 0xCA, DSP_YRAM_SIZE * sizeof(uint32_t));
    dsp->ops->invalidate_opcache(dsp);
}

static void dsp_c_finalize(DSPState *dsp)
{
    g_free(dsp->backend);
    dsp->backend = NULL;
}

const DSPOps c_dsp_ops = {
    .bootstrap = dsp_c_bootstrap,
    .finalize = dsp_c_finalize,
    .get_cycle_count = dsp_c_get_cycle_count,
    .get_halt_requested = dsp_c_get_halt_requested,
    .invalidate_opcache = dsp_c_invalidate_opcache,
    .read_memory = dsp_c_read_memory,
    .reset = dsp_c_reset,
    .run = dsp_c_run,
    .set_cycle_count = dsp_c_set_cycle_count,
    .set_halt_requested = dsp_c_set_halt_requested,
    .start_frame = dsp_start_frame_impl,
    .step = dsp_c_step,
    .sync_from_vm = dsp_c_sync_from_vm,
    .sync_to_vm = dsp_c_sync_to_vm,
    .write_memory = dsp_c_write_memory,
};
