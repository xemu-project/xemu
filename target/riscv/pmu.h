/*
 * RISC-V PMU header file.
 *
 * Copyright (c) 2021 Western Digital Corporation or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef RISCV_PMU_H
#define RISCV_PMU_H

#include "cpu.h"
#include "qapi/error.h"

bool riscv_pmu_ctr_monitor_instructions(CPURISCVState *env,
                                        uint32_t target_ctr);
bool riscv_pmu_ctr_monitor_cycles(CPURISCVState *env,
                                  uint32_t target_ctr);
void riscv_pmu_timer_cb(void *priv);
void riscv_pmu_init(RISCVCPU *cpu, Error **errp);
int riscv_pmu_update_event_map(CPURISCVState *env, uint64_t value,
                               uint32_t ctr_idx);
int riscv_pmu_incr_ctr(RISCVCPU *cpu, enum riscv_pmu_event_idx event_idx);
void riscv_pmu_generate_fdt_node(void *fdt, uint32_t cmask, char *pmu_name);
int riscv_pmu_setup_timer(CPURISCVState *env, uint64_t value,
                          uint32_t ctr_idx);
void riscv_pmu_update_fixed_ctrs(CPURISCVState *env, target_ulong newpriv,
                                 bool new_virt);
RISCVException riscv_pmu_read_ctr(CPURISCVState *env, target_ulong *val,
                                  bool upper_half, uint32_t ctr_idx);

#endif /* RISCV_PMU_H */
