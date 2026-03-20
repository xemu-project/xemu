/*
 * MCPX DSP emulator - internal declarations
 *
 * Copyright (c) 2015 espes
 * Copyright (c) 2020-2025 Matt Borgerson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef DSP_INTERNAL_H
#define DSP_INTERNAL_H

#include "dsp.h"

uint32_t read_peripheral(DSPState *dsp, uint32_t address);
void write_peripheral(DSPState *dsp, uint32_t address, uint32_t value);
void dsp_start_frame_impl(DSPState *dsp);

extern const DSPOps c_dsp_ops;
void dsp_c_init(DSPState *dsp);

extern const DSPOps jit_dsp_ops;
void dsp_jit_init(DSPState *dsp);

#endif /* DSP_INTERNAL_H */
