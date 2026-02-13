/*
 * QEMU MCPX Audio Processing Unit implementation
 *
 * Copyright (c) 2012 espes
 * Copyright (c) 2018-2019 Jannik Vogel
 * Copyright (c) 2019-2025 Matt Borgerson
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

#include "hw/xbox/mcpx/apu/apu_int.h"
#include "adpcm.h"

static const struct {
    hwaddr top, current, next;
} voice_list_regs[] = {
    { NV_PAPU_TVL2D, NV_PAPU_CVL2D, NV_PAPU_NVL2D }, // 2D
    { NV_PAPU_TVL3D, NV_PAPU_CVL3D, NV_PAPU_NVL3D }, // 3D
    { NV_PAPU_TVLMP, NV_PAPU_CVLMP, NV_PAPU_NVLMP }, // MP
};

static void set_notify_status(MCPXAPUState *d, uint32_t v, int notifier,
                              int status)
{
    hwaddr notify_offset = d->regs[NV_PAPU_FENADDR];
    notify_offset += 16 * (MCPX_HW_NOTIFIER_BASE_OFFSET +
                           v * MCPX_HW_NOTIFIER_COUNT + notifier);
    notify_offset += 15; // Final byte is status, same for all notifiers

    // FIXME: Check notify enable
    // FIXME: Set NV1BA0_NOTIFICATION_STATUS_IN_PROGRESS when appropriate
    stb_phys(&address_space_memory, notify_offset, status);

    // FIXME: Refactor this out of here
    // FIXME: Actually provied current envelope state
    stb_phys(&address_space_memory, notify_offset - 1, 1);

    qatomic_or(&d->regs[NV_PAPU_ISTS],
              NV_PAPU_ISTS_FEVINTSTS | NV_PAPU_ISTS_FENINTSTS);
    d->set_irq = true;
}

static void voice_reset_filters(MCPXAPUState *d, uint16_t v)
{
    assert(v < MCPX_HW_MAX_VOICES);
    memset(&d->vp.filters[v].svf, 0, sizeof(d->vp.filters[v].svf));
    hrtf_filter_clear_history(&d->vp.filters[v].hrtf);
    if (d->vp.filters[v].resampler) {
        src_reset(d->vp.filters[v].resampler);
    }
}

static bool voice_should_mute(uint16_t v)
{
    bool m = (g_dbg_voice_monitor >= 0) && (v != g_dbg_voice_monitor);

    if (m && g_dbg_cache.vp.v[g_dbg_voice_monitor].multipass) {
        uint8_t mp_bin = g_dbg_cache.vp.v[g_dbg_voice_monitor].multipass_bin;
        struct McpxApuDebugVoice *d = &g_dbg_cache.vp.v[v];

        for (int i = 0; i < sizeof(d->bin) / sizeof(d->bin[0]); i++) {
            if (d->bin[i] == mp_bin) {
                m = false;
                break;
            }
        }
    }

    return m || mcpx_apu_debug_is_muted(v);
}

static float clampf(float v, float min, float max)
{
    if (v < min) {
        return min;
    } else if (v > max) {
        return max;
    } else {
        return v;
    }
}

static float attenuate(uint16_t vol)
{
    vol &= 0xFFF;
    return (vol == 0xFFF) ? 0.0 : powf(10.0f, vol/(64.0 * -20.0f));
}

static uint32_t voice_get_mask(MCPXAPUState *d, uint16_t voice_handle,
                               hwaddr offset, uint32_t mask)
{
    hwaddr voice = d->regs[NV_PAPU_VPVADDR] + voice_handle * NV_PAVS_SIZE;
    return (ldl_le_phys(&address_space_memory, voice + offset) & mask) >>
           ctz32(mask);
}

static void voice_set_mask(MCPXAPUState *d, uint16_t voice_handle,
                           hwaddr offset, uint32_t mask, uint32_t val)
{
    hwaddr voice = d->regs[NV_PAPU_VPVADDR]
                    + voice_handle * NV_PAVS_SIZE;
    uint32_t v = ldl_le_phys(&address_space_memory, voice + offset) & ~mask;
    stl_le_phys(&address_space_memory, voice + offset,
                v | ((val << ctz32(mask)) & mask));
}

static void voice_off(MCPXAPUState *d, uint16_t v)
{
    voice_set_mask(d, v, NV_PAVS_VOICE_PAR_STATE,
                   NV_PAVS_VOICE_PAR_STATE_ACTIVE_VOICE, 0);

    bool stream = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT,
                                 NV_PAVS_VOICE_CFG_FMT_DATA_TYPE);
    int notifier = MCPX_HW_NOTIFIER_SSLA_DONE;
    if (stream) {
        assert(v < MCPX_HW_MAX_VOICES);
        assert(d->vp.ssl[v].ssl_index <= 1);
        notifier += d->vp.ssl[v].ssl_index;
    }
    set_notify_status(d, v, notifier, NV1BA0_NOTIFICATION_STATUS_DONE_SUCCESS);
}

static void voice_lock(MCPXAPUState *d, uint16_t v, bool lock)
{
    assert(v < MCPX_HW_MAX_VOICES);
    qemu_mutex_lock(&d->lock);

    uint64_t mask = 1LL << (v % 64);
    if (lock) {
        d->vp.voice_locked[v / 64] |= mask;
    } else {
        d->vp.voice_locked[v / 64] &= ~mask;
    }

    qemu_cond_signal(&d->cond);
    qemu_mutex_unlock(&d->lock);
}

static bool is_voice_locked(MCPXAPUState *d, uint16_t v)
{
    assert(v < MCPX_HW_MAX_VOICES);
    uint64_t mask = 1LL << (v % 64);
    return (qatomic_read(&d->vp.voice_locked[v / 64]) & mask) != 0;
}

static void set_hrir_coeff_tar(MCPXAPUState *d, int channel, int coeff_idx,
                               int8_t value)
{
    int entry = d->vp.hrtf.current_entry;
    d->vp.hrtf.entries[entry].hrir[channel][coeff_idx] = int8_to_float(value);
}

static void fe_method(MCPXAPUState *d, uint32_t method, uint32_t argument)
{
    unsigned int slot;

    trace_mcpx_apu_method(method, argument);

    //assert((d->regs[NV_PAPU_FECTL] & NV_PAPU_FECTL_FEMETHMODE) == 0);

    d->regs[NV_PAPU_FEDECMETH] = method;
    d->regs[NV_PAPU_FEDECPARAM] = argument;
    unsigned int selected_handle, list;
    switch (method) {
    case NV1BA0_PIO_VOICE_LOCK:
        voice_lock(d, d->regs[NV_PAPU_FECV], argument & 1);
        break;
    case NV1BA0_PIO_SET_ANTECEDENT_VOICE:
        d->regs[NV_PAPU_FEAV] = argument;
        break;
    case NV1BA0_PIO_VOICE_ON: {
        selected_handle = argument & NV1BA0_PIO_VOICE_ON_HANDLE;
        DPRINTF("VOICE %d ON\n", selected_handle);

        bool locked = is_voice_locked(d, selected_handle);
        if (!locked) {
            voice_lock(d, selected_handle, true);
        }

        list = GET_MASK(d->regs[NV_PAPU_FEAV], NV_PAPU_FEAV_LST);
        if (list != NV1BA0_PIO_SET_ANTECEDENT_VOICE_LIST_INHERIT) {
            /* voice is added to the top of the selected list */
            unsigned int top_reg = voice_list_regs[list - 1].top;
            voice_set_mask(d, selected_handle, NV_PAVS_VOICE_TAR_PITCH_LINK,
                           NV_PAVS_VOICE_TAR_PITCH_LINK_NEXT_VOICE_HANDLE,
                           d->regs[top_reg]);
            d->regs[top_reg] = selected_handle;
        } else {
            unsigned int antecedent_voice =
                GET_MASK(d->regs[NV_PAPU_FEAV], NV_PAPU_FEAV_VALUE);
            /* voice is added after the antecedent voice */
            assert(antecedent_voice != 0xFFFF);

            uint32_t next_handle = voice_get_mask(
                d, antecedent_voice, NV_PAVS_VOICE_TAR_PITCH_LINK,
                NV_PAVS_VOICE_TAR_PITCH_LINK_NEXT_VOICE_HANDLE);
            voice_set_mask(d, selected_handle, NV_PAVS_VOICE_TAR_PITCH_LINK,
                           NV_PAVS_VOICE_TAR_PITCH_LINK_NEXT_VOICE_HANDLE,
                           next_handle);
            voice_set_mask(d, antecedent_voice, NV_PAVS_VOICE_TAR_PITCH_LINK,
                           NV_PAVS_VOICE_TAR_PITCH_LINK_NEXT_VOICE_HANDLE,
                           selected_handle);
        }

        // FIXME: Should set CBO here?
        voice_set_mask(d, selected_handle, NV_PAVS_VOICE_PAR_OFFSET,
                       NV_PAVS_VOICE_PAR_OFFSET_CBO, 0);
        d->vp.ssl[selected_handle].ssl_seg = 0; // FIXME: verify this
        d->vp.ssl[selected_handle].ssl_index = 0; // FIXME: verify this

        unsigned int ea_start = GET_MASK(argument, NV1BA0_PIO_VOICE_ON_ENVA);
        voice_set_mask(d, selected_handle, NV_PAVS_VOICE_PAR_STATE,
                       NV_PAVS_VOICE_PAR_STATE_EACUR, ea_start);
        if (ea_start == NV_PAVS_VOICE_PAR_STATE_EFCUR_DELAY) {
            uint16_t delay_time =
                voice_get_mask(d, selected_handle, NV_PAVS_VOICE_CFG_ENV0,
                               NV_PAVS_VOICE_CFG_ENV0_EA_DELAYTIME);
            voice_set_mask(d, selected_handle, NV_PAVS_VOICE_CUR_ECNT,
                           NV_PAVS_VOICE_CUR_ECNT_EACOUNT, delay_time * 16);
        } else if (ea_start == NV_PAVS_VOICE_PAR_STATE_EFCUR_ATTACK) {
            voice_set_mask(d, selected_handle, NV_PAVS_VOICE_CUR_ECNT,
                           NV_PAVS_VOICE_CUR_ECNT_EACOUNT, 0);
        } else if (ea_start == NV_PAVS_VOICE_PAR_STATE_EFCUR_HOLD) {
            uint16_t hold_time =
                voice_get_mask(d, selected_handle, NV_PAVS_VOICE_CFG_ENVA,
                               NV_PAVS_VOICE_CFG_ENVA_EA_HOLDTIME);
            voice_set_mask(d, selected_handle, NV_PAVS_VOICE_CUR_ECNT,
                           NV_PAVS_VOICE_CUR_ECNT_EACOUNT, hold_time * 16);
        }
        // FIXME: Will count be overwritten in other cases too?

        unsigned int ef_start = GET_MASK(argument, NV1BA0_PIO_VOICE_ON_ENVF);
        voice_set_mask(d, selected_handle, NV_PAVS_VOICE_PAR_STATE,
                       NV_PAVS_VOICE_PAR_STATE_EFCUR, ef_start);
        if (ef_start == NV_PAVS_VOICE_PAR_STATE_EFCUR_DELAY) {
            uint16_t delay_time =
                voice_get_mask(d, selected_handle, NV_PAVS_VOICE_CFG_ENV1,
                               NV_PAVS_VOICE_CFG_ENV0_EA_DELAYTIME);
            voice_set_mask(d, selected_handle, NV_PAVS_VOICE_CUR_ECNT,
                           NV_PAVS_VOICE_CUR_ECNT_EFCOUNT, delay_time * 16);
        } else if (ef_start == NV_PAVS_VOICE_PAR_STATE_EFCUR_ATTACK) {
            voice_set_mask(d, selected_handle, NV_PAVS_VOICE_CUR_ECNT,
                           NV_PAVS_VOICE_CUR_ECNT_EFCOUNT, 0);
        } else if (ef_start == NV_PAVS_VOICE_PAR_STATE_EFCUR_HOLD) {
            uint16_t hold_time =
                voice_get_mask(d, selected_handle, NV_PAVS_VOICE_CFG_ENVF,
                               NV_PAVS_VOICE_CFG_ENVA_EA_HOLDTIME);
            voice_set_mask(d, selected_handle, NV_PAVS_VOICE_CUR_ECNT,
                           NV_PAVS_VOICE_CUR_ECNT_EFCOUNT, hold_time * 16);
        }
        // FIXME: Will count be overwritten in other cases too?

        voice_reset_filters(d, selected_handle);
        voice_set_mask(d, selected_handle, NV_PAVS_VOICE_PAR_STATE,
                       NV_PAVS_VOICE_PAR_STATE_ACTIVE_VOICE, 1);

        if (!locked) {
            voice_lock(d, selected_handle, false);
        }

        break;
    }
    case NV1BA0_PIO_VOICE_RELEASE: {
        selected_handle = argument & NV1BA0_PIO_VOICE_ON_HANDLE;

        // FIXME: What if already in release? Restart envelope?
        // FIXME: Should release count ascend or descend?

        bool locked = is_voice_locked(d, selected_handle);
        if (!locked) {
            voice_lock(d, selected_handle, true);
        }

        uint16_t rr;
        rr = voice_get_mask(d, selected_handle, NV_PAVS_VOICE_TAR_LFO_ENV,
                            NV_PAVS_VOICE_TAR_LFO_ENV_EA_RELEASERATE);
        voice_set_mask(d, selected_handle, NV_PAVS_VOICE_CUR_ECNT,
                       NV_PAVS_VOICE_CUR_ECNT_EACOUNT, rr * 16);
        voice_set_mask(d, selected_handle, NV_PAVS_VOICE_PAR_STATE,
                       NV_PAVS_VOICE_PAR_STATE_EACUR,
                       NV_PAVS_VOICE_PAR_STATE_EFCUR_RELEASE);

        rr = voice_get_mask(d, selected_handle, NV_PAVS_VOICE_CFG_MISC,
                            NV_PAVS_VOICE_CFG_MISC_EF_RELEASERATE);
        voice_set_mask(d, selected_handle, NV_PAVS_VOICE_CUR_ECNT,
                       NV_PAVS_VOICE_CUR_ECNT_EFCOUNT, rr * 16);
        voice_set_mask(d, selected_handle, NV_PAVS_VOICE_PAR_STATE,
                       NV_PAVS_VOICE_PAR_STATE_EFCUR,
                       NV_PAVS_VOICE_PAR_STATE_EFCUR_RELEASE);

        if (!locked) {
            voice_lock(d, selected_handle, false);
        }

        break;
    }
    case NV1BA0_PIO_VOICE_OFF:
        voice_off(d, argument & NV1BA0_PIO_VOICE_OFF_HANDLE);
        break;
    case NV1BA0_PIO_VOICE_PAUSE:
        voice_set_mask(d, argument & NV1BA0_PIO_VOICE_PAUSE_HANDLE,
                       NV_PAVS_VOICE_PAR_STATE, NV_PAVS_VOICE_PAR_STATE_PAUSED,
                       (argument & NV1BA0_PIO_VOICE_PAUSE_ACTION) != 0);
        break;
    case NV1BA0_PIO_SET_CURRENT_HRTF_ENTRY: {
        int handle = GET_MASK(argument, NV1BA0_PIO_SET_CURRENT_HRTF_ENTRY_HANDLE);
        d->vp.hrtf.current_entry = handle;
        break;
    }
    case NV1BA0_PIO_SET_CURRENT_VOICE:
        d->regs[NV_PAPU_FECV] = argument;
        break;
    case NV1BA0_PIO_SET_VOICE_CFG_VBIN:
        voice_set_mask(d, d->regs[NV_PAPU_FECV], NV_PAVS_VOICE_CFG_VBIN,
                       0xFFFFFFFF, argument);
        break;
    case NV1BA0_PIO_SET_VOICE_CFG_FMT:
        voice_set_mask(d, d->regs[NV_PAPU_FECV], NV_PAVS_VOICE_CFG_FMT,
                       0xFFFFFFFF, argument);
        break;
    case NV1BA0_PIO_SET_VOICE_CFG_ENV0:
        voice_set_mask(d, d->regs[NV_PAPU_FECV], NV_PAVS_VOICE_CFG_ENV0,
                       0xFFFFFFFF, argument);
        break;
    case NV1BA0_PIO_SET_VOICE_CFG_ENVA:
        voice_set_mask(d, d->regs[NV_PAPU_FECV], NV_PAVS_VOICE_CFG_ENVA,
                       0xFFFFFFFF, argument);
        break;
    case NV1BA0_PIO_SET_VOICE_CFG_ENV1:
        voice_set_mask(d, d->regs[NV_PAPU_FECV], NV_PAVS_VOICE_CFG_ENV1,
                       0xFFFFFFFF, argument);
        break;
    case NV1BA0_PIO_SET_VOICE_CFG_ENVF:
        voice_set_mask(d, d->regs[NV_PAPU_FECV], NV_PAVS_VOICE_CFG_ENVF,
                       0xFFFFFFFF, argument);
        break;
    case NV1BA0_PIO_SET_VOICE_CFG_MISC:
        voice_set_mask(d, d->regs[NV_PAPU_FECV], NV_PAVS_VOICE_CFG_MISC,
                       0xFFFFFFFF, argument);
        break;
    case NV1BA0_PIO_SET_VOICE_TAR_HRTF: {
        int handle = GET_MASK(argument, NV1BA0_PIO_SET_VOICE_TAR_HRTF_HANDLE);
        int current_voice = d->regs[NV_PAPU_FECV];
        voice_set_mask(d, current_voice, NV_PAVS_VOICE_CFG_HRTF_TARGET,
                       NV_PAVS_VOICE_CFG_HRTF_TARGET_HANDLE, handle);
        if (current_voice < MCPX_HW_MAX_3D_VOICES &&
            handle != HRTF_NULL_HANDLE) {
            // FIXME: Xbox software seems to reliably set voice HRTF handles
            // after updating filter parameters, however it may be possible to
            // update parameter targets for an active voice.
            assert(handle < HRTF_ENTRY_COUNT);
            hrtf_filter_set_target_params(&d->vp.filters[current_voice].hrtf,
                                          d->vp.hrtf.entries[handle].hrir,
                                          d->vp.hrtf.entries[handle].itd);
        }
        break;
    }
    case NV1BA0_PIO_SET_VOICE_TAR_VOLA:
        voice_set_mask(d, d->regs[NV_PAPU_FECV], NV_PAVS_VOICE_TAR_VOLA,
                       0xFFFFFFFF, argument);
        break;
    case NV1BA0_PIO_SET_VOICE_TAR_VOLB:
        voice_set_mask(d, d->regs[NV_PAPU_FECV], NV_PAVS_VOICE_TAR_VOLB,
                       0xFFFFFFFF, argument);
        break;
    case NV1BA0_PIO_SET_VOICE_TAR_VOLC:
        voice_set_mask(d, d->regs[NV_PAPU_FECV], NV_PAVS_VOICE_TAR_VOLC,
                       0xFFFFFFFF, argument);
        break;
    case NV1BA0_PIO_SET_VOICE_LFO_ENV:
        voice_set_mask(d, d->regs[NV_PAPU_FECV], NV_PAVS_VOICE_TAR_LFO_ENV,
                       0xFFFFFFFF, argument);
        break;
    case NV1BA0_PIO_SET_VOICE_TAR_FCA:
        voice_set_mask(d, d->regs[NV_PAPU_FECV], NV_PAVS_VOICE_TAR_FCA,
                       0xFFFFFFFF, argument);
        break;
    case NV1BA0_PIO_SET_VOICE_TAR_FCB:
        voice_set_mask(d, d->regs[NV_PAPU_FECV], NV_PAVS_VOICE_TAR_FCB,
                       0xFFFFFFFF, argument);
        break;
    case NV1BA0_PIO_SET_VOICE_TAR_PITCH:
        voice_set_mask(d, d->regs[NV_PAPU_FECV], NV_PAVS_VOICE_TAR_PITCH_LINK,
                       NV_PAVS_VOICE_TAR_PITCH_LINK_PITCH,
                       (argument & NV1BA0_PIO_SET_VOICE_TAR_PITCH_STEP) >> 16);
        break;
    case NV1BA0_PIO_SET_VOICE_CFG_BUF_BASE:
        voice_set_mask(d, d->regs[NV_PAPU_FECV], NV_PAVS_VOICE_CUR_PSL_START,
                       NV_PAVS_VOICE_CUR_PSL_START_BA, argument);
        break;
    case NV1BA0_PIO_SET_VOICE_CFG_BUF_LBO:
        voice_set_mask(d, d->regs[NV_PAPU_FECV], NV_PAVS_VOICE_CUR_PSH_SAMPLE,
                       NV_PAVS_VOICE_CUR_PSH_SAMPLE_LBO, argument);
        break;
    case NV1BA0_PIO_SET_VOICE_BUF_CBO:
        voice_set_mask(d, d->regs[NV_PAPU_FECV], NV_PAVS_VOICE_PAR_OFFSET,
                       NV_PAVS_VOICE_PAR_OFFSET_CBO, argument);
        break;
    case NV1BA0_PIO_SET_VOICE_CFG_BUF_EBO:
        voice_set_mask(d, d->regs[NV_PAPU_FECV], NV_PAVS_VOICE_PAR_NEXT,
                       NV_PAVS_VOICE_PAR_NEXT_EBO, argument);
        break;
    case NV1BA0_PIO_SET_HRIR ... NV1BA0_PIO_SET_HRIR_X - 1: {
        assert(d->vp.hrtf.current_entry < HRTF_ENTRY_COUNT);
        slot = (method - NV1BA0_PIO_SET_HRIR) / 4;
        int8_t left0 = GET_MASK(argument, NV1BA0_PIO_SET_HRIR_LEFT0);
        int8_t right0 = GET_MASK(argument, NV1BA0_PIO_SET_HRIR_RIGHT0);
        int8_t left1 = GET_MASK(argument, NV1BA0_PIO_SET_HRIR_LEFT1);
        int8_t right1 = GET_MASK(argument, NV1BA0_PIO_SET_HRIR_RIGHT1);
        int coeff_idx = slot * 2;
        set_hrir_coeff_tar(d, 0, coeff_idx, left0);
        set_hrir_coeff_tar(d, 1, coeff_idx, right0);
        coeff_idx += 1;
        set_hrir_coeff_tar(d, 0, coeff_idx, left1);
        set_hrir_coeff_tar(d, 1, coeff_idx, right1);
        break;
    }
    case NV1BA0_PIO_SET_HRIR_X: {
        assert(d->vp.hrtf.current_entry < HRTF_ENTRY_COUNT);
        int8_t left30 = GET_MASK(argument, NV1BA0_PIO_SET_HRIR_X_LEFT30);
        int8_t right30 = GET_MASK(argument, NV1BA0_PIO_SET_HRIR_X_RIGHT30);
        int16_t itd = GET_MASK(argument, NV1BA0_PIO_SET_HRIR_X_ITD);
        set_hrir_coeff_tar(d, 0, 30, left30);
        set_hrir_coeff_tar(d, 1, 30, right30);
        d->vp.hrtf.entries[d->vp.hrtf.current_entry].itd = s6p9_to_float(itd);
        break;
    }
    case NV1BA0_PIO_SET_CURRENT_INBUF_SGE:
        d->vp.inbuf_sge_handle = argument & NV1BA0_PIO_SET_CURRENT_INBUF_SGE_HANDLE;
        break;
    case NV1BA0_PIO_SET_CURRENT_INBUF_SGE_OFFSET: {
        // FIXME: Is there an upper limit for the SGE table size?
        // FIXME: NV_PAPU_VPSGEADDR is probably bad, as outbuf SGE use the same
        // handle range (or that is also wrong)
        hwaddr sge_address =
            d->regs[NV_PAPU_VPSGEADDR] + d->vp.inbuf_sge_handle * 8;
        stl_le_phys(&address_space_memory, sge_address,
                    argument &
                        NV1BA0_PIO_SET_CURRENT_INBUF_SGE_OFFSET_PARAMETER);
        DPRINTF("Wrote inbuf SGE[0x%X] = 0x%08X\n", d->vp.inbuf_sge_handle,
                argument & NV1BA0_PIO_SET_CURRENT_INBUF_SGE_OFFSET_PARAMETER);
        break;
    }
    CASE_4(NV1BA0_PIO_SET_OUTBUF_BA, 8): // 8 byte pitch, 4 entries
#ifdef DEBUG_MCPX
        slot = (method - NV1BA0_PIO_SET_OUTBUF_BA) / 8;
        //FIXME: Use NV1BA0_PIO_SET_OUTBUF_BA_ADDRESS = 0x007FFF00 ?
        DPRINTF("outbuf_ba[%d]: 0x%08X\n", slot, argument);
#endif
        //assert(false); //FIXME: Enable assert! no idea what this reg does
        break;
    CASE_4(NV1BA0_PIO_SET_OUTBUF_LEN, 8): // 8 byte pitch, 4 entries
#ifdef DEBUG_MCPX
        slot = (method - NV1BA0_PIO_SET_OUTBUF_LEN) / 8;
        //FIXME: Use NV1BA0_PIO_SET_OUTBUF_LEN_VALUE = 0x007FFF00 ?
        DPRINTF("outbuf_len[%d]: 0x%08X\n", slot, argument);
#endif
        //assert(false); //FIXME: Enable assert! no idea what this reg does
        break;
    case NV1BA0_PIO_SET_CURRENT_OUTBUF_SGE:
        d->vp.outbuf_sge_handle =
            argument & NV1BA0_PIO_SET_CURRENT_OUTBUF_SGE_HANDLE;
        break;
    case NV1BA0_PIO_SET_CURRENT_OUTBUF_SGE_OFFSET: {
        // FIXME: Is there an upper limit for the SGE table size?
        // FIXME: NV_PAPU_VPSGEADDR is probably bad, as inbuf SGE use the same
        // handle range (or that is also wrong)
        // NV_PAPU_EPFADDR   EP outbufs
        // NV_PAPU_GPFADDR   GP outbufs
        // But how does it know which outbuf is being written?!
        hwaddr sge_address =
            d->regs[NV_PAPU_VPSGEADDR] + d->vp.outbuf_sge_handle * 8;
        stl_le_phys(&address_space_memory, sge_address,
                    argument &
                        NV1BA0_PIO_SET_CURRENT_OUTBUF_SGE_OFFSET_PARAMETER);
        DPRINTF("Wrote outbuf SGE[0x%X] = 0x%08X\n", d->vp.outbuf_sge_handle,
                argument & NV1BA0_PIO_SET_CURRENT_OUTBUF_SGE_OFFSET_PARAMETER);
        break;
    }
    case NV1BA0_PIO_SET_VOICE_SSL_A: {
        int ssl = 0;
        int current_voice = d->regs[NV_PAPU_FECV];
        assert(current_voice < MCPX_HW_MAX_VOICES);
        d->vp.ssl[current_voice].base[ssl] =
            GET_MASK(argument, NV1BA0_PIO_SET_VOICE_SSL_A_BASE);
        d->vp.ssl[current_voice].count[ssl] =
            GET_MASK(argument, NV1BA0_PIO_SET_VOICE_SSL_A_COUNT);
        // d->vp.ssl[current_voice].ssl_index = 0;
        DPRINTF("SSL%c Base = %x, Count = %d\n", 'A' + ssl,
                d->vp.ssl[current_voice].base[ssl],
                d->vp.ssl[current_voice].count[ssl]);
        break;
    }
    // FIXME: Refactor into above
    case NV1BA0_PIO_SET_VOICE_SSL_B: {
        int ssl = 1;
        int current_voice = d->regs[NV_PAPU_FECV];
        assert(current_voice < MCPX_HW_MAX_VOICES);
        d->vp.ssl[current_voice].base[ssl] =
            GET_MASK(argument, NV1BA0_PIO_SET_VOICE_SSL_A_BASE);
        d->vp.ssl[current_voice].count[ssl] =
            GET_MASK(argument, NV1BA0_PIO_SET_VOICE_SSL_A_COUNT);
        // d->vp.ssl[current_voice].ssl_index = 0;
        DPRINTF("SSL%c Base = %x, Count = %d\n", 'A' + ssl,
                d->vp.ssl[current_voice].base[ssl],
                d->vp.ssl[current_voice].count[ssl]);
        break;
    }
    case NV1BA0_PIO_SET_CURRENT_SSL: {
        assert((argument & 0x3f) == 0);
        assert(argument < (MCPX_HW_MAX_SSL_PRDS*NV_PSGE_SIZE));
        d->vp.ssl_base_page = argument;
        break;
    }
    case NV1BA0_PIO_SET_SSL_SEGMENT_OFFSET ...
         NV1BA0_PIO_SET_SSL_SEGMENT_LENGTH+8*64-1: {
        // 64 offset/base pairs relative to segment base
        // FIXME: Entries are 64b, assuming they are stored
        // like this <[offset,length],...>
        assert((method & 0x3) == 0);
        hwaddr addr = d->regs[NV_PAPU_VPSSLADDR]
                      + (d->vp.ssl_base_page * 8)
                      + (method - NV1BA0_PIO_SET_SSL_SEGMENT_OFFSET);
        stl_le_phys(&address_space_memory, addr, argument);
        DPRINTF("  ssl_segment[%x + %x].%s = %x\n",
            d->vp.ssl_base_page,
            (method - NV1BA0_PIO_SET_SSL_SEGMENT_OFFSET)/8,
            method & 4 ? "length" : "offset",
            argument);
        break;
    }
    case NV1BA0_PIO_SET_HRTF_SUBMIXES:
        d->vp.hrtf_submix[0] = (argument >>  0) & 0x1f;
        d->vp.hrtf_submix[1] = (argument >>  8) & 0x1f;
        d->vp.hrtf_submix[2] = (argument >> 16) & 0x1f;
        d->vp.hrtf_submix[3] = (argument >> 24) & 0x1f;
        break;
    case NV1BA0_PIO_SET_HRTF_HEADROOM:
        d->vp.hrtf_headroom = argument & NV1BA0_PIO_SET_HRTF_HEADROOM_AMOUNT;
        break;
    case NV1BA0_PIO_SET_SUBMIX_HEADROOM ...
         NV1BA0_PIO_SET_SUBMIX_HEADROOM+4*(NUM_MIXBINS-1):
        assert((method & 3) == 0);
        slot = (method-NV1BA0_PIO_SET_SUBMIX_HEADROOM)/4;
        d->vp.submix_headroom[slot] =
            argument & NV1BA0_PIO_SET_SUBMIX_HEADROOM_AMOUNT;
        break;
    case SE2FE_IDLE_VOICE:
        if (d->regs[NV_PAPU_FETFORCE1] & NV_PAPU_FETFORCE1_SE2FE_IDLE_VOICE) {
            d->regs[NV_PAPU_FECTL] &= ~NV_PAPU_FECTL_FEMETHMODE;
            d->regs[NV_PAPU_FECTL] |= NV_PAPU_FECTL_FEMETHMODE_TRAPPED;
            d->regs[NV_PAPU_FECTL] &= ~NV_PAPU_FECTL_FETRAPREASON;
            d->regs[NV_PAPU_FECTL] |= NV_PAPU_FECTL_FETRAPREASON_REQUESTED;
            DPRINTF("idle voice %d\n", argument);
            d->set_irq = true;
        } else {
            assert(false);
        }
        break;
    default:
        assert(false);
        break;
    }
}

static uint64_t vp_read(void *opaque, hwaddr addr, unsigned int size)
{
    DPRINTF("mcpx apu VP: read [0x%" HWADDR_PRIx "] (%s)\n", addr,
            get_method_str(addr));

    switch (addr) {
    case NV1BA0_PIO_FREE:
        /* we don't simulate the queue for now,
         * pretend to always be empty */
        return 0x80;
    default:
        break;
    }

    return 0;
}

static void vp_write(void *opaque, hwaddr addr, uint64_t val, unsigned int size)
{
    MCPXAPUState *d = opaque;

    DPRINTF("mcpx apu VP: [0x%" HWADDR_PRIx "] %s = 0x%lx\n", addr,
            get_method_str(addr), val);

    switch (addr) {
    case NV1BA0_PIO_SET_ANTECEDENT_VOICE:
    case NV1BA0_PIO_VOICE_LOCK:
    case NV1BA0_PIO_VOICE_ON:
    case NV1BA0_PIO_VOICE_RELEASE:
    case NV1BA0_PIO_VOICE_OFF:
    case NV1BA0_PIO_VOICE_PAUSE:
    case NV1BA0_PIO_SET_CURRENT_HRTF_ENTRY:
    case NV1BA0_PIO_SET_CURRENT_VOICE:
    case NV1BA0_PIO_SET_VOICE_CFG_VBIN:
    case NV1BA0_PIO_SET_VOICE_CFG_FMT:
    case NV1BA0_PIO_SET_VOICE_CFG_ENV0:
    case NV1BA0_PIO_SET_VOICE_CFG_ENVA:
    case NV1BA0_PIO_SET_VOICE_CFG_ENV1:
    case NV1BA0_PIO_SET_VOICE_CFG_ENVF:
    case NV1BA0_PIO_SET_VOICE_CFG_MISC:
    case NV1BA0_PIO_SET_VOICE_TAR_HRTF:
    case NV1BA0_PIO_SET_VOICE_TAR_VOLA:
    case NV1BA0_PIO_SET_VOICE_TAR_VOLB:
    case NV1BA0_PIO_SET_VOICE_TAR_VOLC:
    case NV1BA0_PIO_SET_VOICE_LFO_ENV:
    case NV1BA0_PIO_SET_VOICE_TAR_FCA:
    case NV1BA0_PIO_SET_VOICE_TAR_FCB:
    case NV1BA0_PIO_SET_VOICE_TAR_PITCH:
    case NV1BA0_PIO_SET_VOICE_CFG_BUF_BASE:
    case NV1BA0_PIO_SET_VOICE_CFG_BUF_LBO:
    case NV1BA0_PIO_SET_VOICE_BUF_CBO:
    case NV1BA0_PIO_SET_VOICE_CFG_BUF_EBO:
    case NV1BA0_PIO_SET_HRIR ... NV1BA0_PIO_SET_HRIR_X - 1:
    case NV1BA0_PIO_SET_HRIR_X:
    case NV1BA0_PIO_SET_CURRENT_INBUF_SGE:
    case NV1BA0_PIO_SET_CURRENT_INBUF_SGE_OFFSET:
    CASE_4(NV1BA0_PIO_SET_OUTBUF_BA, 8): // 8 byte pitch, 4 entries
    CASE_4(NV1BA0_PIO_SET_OUTBUF_LEN, 8): // 8 byte pitch, 4 entries
    case NV1BA0_PIO_SET_CURRENT_OUTBUF_SGE:
    case NV1BA0_PIO_SET_CURRENT_OUTBUF_SGE_OFFSET:
    case NV1BA0_PIO_SET_CURRENT_SSL:
    case NV1BA0_PIO_SET_SSL_SEGMENT_OFFSET ...
         NV1BA0_PIO_SET_SSL_SEGMENT_LENGTH+8*64-1:
    case NV1BA0_PIO_SET_VOICE_SSL_A:
    case NV1BA0_PIO_SET_VOICE_SSL_B:
    case NV1BA0_PIO_SET_HRTF_SUBMIXES:
    case NV1BA0_PIO_SET_HRTF_HEADROOM:
    case NV1BA0_PIO_SET_SUBMIX_HEADROOM ...
         NV1BA0_PIO_SET_SUBMIX_HEADROOM+4*(NUM_MIXBINS-1):
        /* TODO: these should instead be queueing up fe commands */
        fe_method(d, addr, val);
        break;

    case NV1BA0_PIO_GET_VOICE_POSITION:
    case NV1BA0_PIO_SET_CONTEXT_DMA_NOTIFY:
    case NV1BA0_PIO_SET_CURRENT_SSL_CONTEXT_DMA:
        DPRINTF("unhandled method: %" HWADDR_PRIx " = %" HWADDR_PRIx "\n", addr,
                val);
        assert(0);
    default:
        break;
    }
}

const MemoryRegionOps vp_ops = {
    .read = vp_read,
    .write = vp_write,
};

static hwaddr get_data_ptr(hwaddr sge_base, unsigned int max_sge, uint32_t addr)
{
    unsigned int entry = addr / TARGET_PAGE_SIZE;
    assert(entry <= max_sge);
    uint32_t prd_address =
        ldl_le_phys(&address_space_memory, sge_base + entry * 4 * 2);
    // uint32_t prd_control =
    //     ldl_le_phys(&address_space_memory, sge_base + entry * 4 * 2 + 4);
    DPRINTF("Addr: 0x%08X, control: 0x%08X\n", prd_address, prd_control);
    return prd_address + addr % TARGET_PAGE_SIZE;
}

static float voice_step_envelope(MCPXAPUState *d, uint16_t v, uint32_t reg_0,
                           uint32_t reg_a, uint32_t rr_reg, uint32_t rr_mask,
                           uint32_t lvl_reg, uint32_t lvl_mask,
                           uint32_t count_mask, uint32_t cur_mask)
{
    uint8_t cur = voice_get_mask(d, v, NV_PAVS_VOICE_PAR_STATE, cur_mask);
    switch (cur) {
    case NV_PAVS_VOICE_PAR_STATE_EFCUR_OFF:
        voice_set_mask(d, v, NV_PAVS_VOICE_CUR_ECNT, count_mask, 0);
        voice_set_mask(d, v, lvl_reg, lvl_mask, 0xFF);
        return 1.0f;
    case NV_PAVS_VOICE_PAR_STATE_EFCUR_DELAY: {
        uint16_t count =
            voice_get_mask(d, v, NV_PAVS_VOICE_CUR_ECNT, count_mask);
        voice_set_mask(d, v, lvl_reg, lvl_mask, 0x00); // FIXME: Confirm this?

        if (count == 0) {
            cur++;
            voice_set_mask(d, v, NV_PAVS_VOICE_PAR_STATE, cur_mask, cur);
            count = 0;
        } else {
            count--;
        }
        voice_set_mask(d, v, NV_PAVS_VOICE_CUR_ECNT, count_mask, count);
        return 0.0f;
    }
    case NV_PAVS_VOICE_PAR_STATE_EFCUR_ATTACK: {
        uint16_t count =
            voice_get_mask(d, v, NV_PAVS_VOICE_CUR_ECNT, count_mask);
        uint16_t attack_rate =
            voice_get_mask(d, v, reg_0, NV_PAVS_VOICE_CFG_ENV0_EA_ATTACKRATE);

        float value;
        if (attack_rate == 0) {
            // FIXME: [division by zero]
            //       Got crackling sound in hardware for amplitude env.
            value = 255.0f;
        } else {
            if (count <= (attack_rate * 16)) {
                value = (count * 0xFF) / (attack_rate * 16);
            } else {
                // FIXME: Overflow in hardware
                //       The actual value seems to overflow, but not sure how
                value = 255.0f;
            }
        }
        voice_set_mask(d, v, lvl_reg, lvl_mask, value);
        // FIXME: Comparison could also be the other way around?! Test please.
        if (count == (attack_rate * 16)) {
            cur++;
            voice_set_mask(d, v, NV_PAVS_VOICE_PAR_STATE, cur_mask, cur);
            uint16_t hold_time =
                voice_get_mask(d, v, reg_a, NV_PAVS_VOICE_CFG_ENVA_EA_HOLDTIME);
            count = hold_time * 16; // FIXME: Skip next phase if count is 0?
                                    // [other instances too]
        } else {
            count++;
        }
        voice_set_mask(d, v, NV_PAVS_VOICE_CUR_ECNT, count_mask, count);
        return value / 255.0f;
    }
    case NV_PAVS_VOICE_PAR_STATE_EFCUR_HOLD: {
        uint16_t count =
            voice_get_mask(d, v, NV_PAVS_VOICE_CUR_ECNT, count_mask);
        voice_set_mask(d, v, lvl_reg, lvl_mask, 0xFF);

        if (count == 0) {
            cur++;
            voice_set_mask(d, v, NV_PAVS_VOICE_PAR_STATE, cur_mask, cur);
            uint16_t decay_rate = voice_get_mask(
                d, v, reg_a, NV_PAVS_VOICE_CFG_ENVA_EA_DECAYRATE);
            count = decay_rate * 16;
        } else {
            count--;
        }
        voice_set_mask(d, v, NV_PAVS_VOICE_CUR_ECNT, count_mask, count);
        return 1.0f;
    }
    case NV_PAVS_VOICE_PAR_STATE_EFCUR_DECAY: {
        uint16_t count =
            voice_get_mask(d, v, NV_PAVS_VOICE_CUR_ECNT, count_mask);
        uint16_t decay_rate =
            voice_get_mask(d, v, reg_a, NV_PAVS_VOICE_CFG_ENVA_EA_DECAYRATE);
        uint8_t sustain_level =
            voice_get_mask(d, v, reg_a, NV_PAVS_VOICE_CFG_ENVA_EA_SUSTAINLEVEL);

        // FIXME: Decay should return a value no less than sustain
        float value;
        if (decay_rate == 0) {
            value = 0.0f;
        } else {
            // FIXME: This formula and threshold is not accurate, but I can't
            // get it any better for now
            value = 255.0f * powf(0.99988799f, (decay_rate * 16 - count) *
                                                   4096 / decay_rate);
        }
        if (value <= (sustain_level + 0.2f) || (value > 255.0f)) {
            // FIXME: Should we still update lvl?
            cur++;
            voice_set_mask(d, v, NV_PAVS_VOICE_PAR_STATE, cur_mask, cur);
        } else {
            count--;
            voice_set_mask(d, v, NV_PAVS_VOICE_CUR_ECNT, count_mask, count);
            voice_set_mask(d, v, lvl_reg, lvl_mask, value);
        }
        return value / 255.0f;
    }
    case NV_PAVS_VOICE_PAR_STATE_EFCUR_SUSTAIN: {
        uint8_t sustain_level =
            voice_get_mask(d, v, reg_a, NV_PAVS_VOICE_CFG_ENVA_EA_SUSTAINLEVEL);
        voice_set_mask(
            d, v, NV_PAVS_VOICE_CUR_ECNT, count_mask,
            0x00); // FIXME: is this only set to 0 once or forced to zero?
        voice_set_mask(d, v, lvl_reg, lvl_mask, sustain_level);
        return sustain_level / 255.0f;
    }
    case NV_PAVS_VOICE_PAR_STATE_EFCUR_RELEASE: {
        uint16_t count =
            voice_get_mask(d, v, NV_PAVS_VOICE_CUR_ECNT, count_mask);
        uint16_t release_rate = voice_get_mask(d, v, rr_reg, rr_mask);

        if (release_rate == 0) {
            count = 0;
        }

        float value = 0;
        if (count == 0) {
            voice_set_mask(d, v, NV_PAVS_VOICE_PAR_STATE, cur_mask, ++cur);
        } else {
            // FIXME: Appears to be an exponential but unsure about actual
            // curve; performing standard decay of current level to T60 over the
            // release interval which seems about right.
            // FIXME: Based on sustain level or just decay of current level?
            // FIXME: Update level? A very similar, alternative decay function
            // (probably what the hw actually does): y(t)=2^(-10t), which would
            // permit simpler attenuation more efficiently and update level on
            // each round.
            float pos = clampf(1 - count / (release_rate * 16.0), 0, 1);
            uint8_t lvl = voice_get_mask(d, v, lvl_reg, lvl_mask);
            value = powf(M_E, -6.91*pos)*lvl;
            count--; // FIXME: Should release count ascend or descend?
            voice_set_mask(d, v, NV_PAVS_VOICE_CUR_ECNT, count_mask, count);
        }

        return value / 255.0f;
    }
    case NV_PAVS_VOICE_PAR_STATE_EFCUR_FORCE_RELEASE:
        if (count_mask == NV_PAVS_VOICE_CUR_ECNT_EACOUNT) {
            voice_off(d, v);
        }
        return 0.0f;
    default:
        fprintf(stderr, "Unknown envelope state 0x%x\n", cur);
        assert(false);
        return 0.0f;
    }
}

static int voice_get_samples(MCPXAPUState *d, uint32_t v, float samples[][2],
                       int num_samples_requested)
{
    assert(v < MCPX_HW_MAX_VOICES);
    bool stereo = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT,
                                 NV_PAVS_VOICE_CFG_FMT_STEREO);
    unsigned int channels = stereo ? 2 : 1;
    unsigned int sample_size = voice_get_mask(
        d, v, NV_PAVS_VOICE_CFG_FMT, NV_PAVS_VOICE_CFG_FMT_SAMPLE_SIZE);
    unsigned int container_sizes[4] = { 1, 2, 0, 4 }; /* B8, B16, ADPCM, B32 */
    unsigned int container_size_index = voice_get_mask(
        d, v, NV_PAVS_VOICE_CFG_FMT, NV_PAVS_VOICE_CFG_FMT_CONTAINER_SIZE);
    unsigned int container_size = container_sizes[container_size_index];
    bool stream = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT,
                                 NV_PAVS_VOICE_CFG_FMT_DATA_TYPE);
    bool paused = voice_get_mask(d, v, NV_PAVS_VOICE_PAR_STATE,
                                 NV_PAVS_VOICE_PAR_STATE_PAUSED);
    bool loop =
        voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT, NV_PAVS_VOICE_CFG_FMT_LOOP);
    uint32_t ebo = voice_get_mask(d, v, NV_PAVS_VOICE_PAR_NEXT,
                                  NV_PAVS_VOICE_PAR_NEXT_EBO);
    uint32_t cbo = voice_get_mask(d, v, NV_PAVS_VOICE_PAR_OFFSET,
                                  NV_PAVS_VOICE_PAR_OFFSET_CBO);
    uint32_t lbo = voice_get_mask(d, v, NV_PAVS_VOICE_CUR_PSH_SAMPLE,
                                  NV_PAVS_VOICE_CUR_PSH_SAMPLE_LBO);
    uint32_t ba = voice_get_mask(d, v, NV_PAVS_VOICE_CUR_PSL_START,
                                 NV_PAVS_VOICE_CUR_PSL_START_BA);
    unsigned int samples_per_block =
        1 + voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT,
                           NV_PAVS_VOICE_CFG_FMT_SAMPLES_PER_BLOCK);
    bool persist = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT,
                                  NV_PAVS_VOICE_CFG_FMT_PERSIST);
    bool multipass = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT,
                                    NV_PAVS_VOICE_CFG_FMT_MULTIPASS);
    bool linked = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT,
                                 NV_PAVS_VOICE_CFG_FMT_LINKED); /* FIXME? */

    assert(!multipass); // Multipass is handled before this

    int ssl_index = 0;
    int ssl_seg = 0;
    int page = 0;
    int count = 0;
    int seg_len = 0;
    int seg_cs = 0;
    int seg_spb = 0;
    int seg_s = 0;
    hwaddr segment_offset = 0;
    uint32_t segment_length = 0;
    size_t block_size;

    int adpcm_block_index = -1;
    uint32_t adpcm_block[36*2/4];
    int16_t adpcm_decoded[65*2]; // FIXME: Move out of here

    // FIXME: Only update if necessary
    struct McpxApuDebugVoice *dbg = &g_dbg.vp.v[v];
    dbg->container_size = container_size_index;
    dbg->sample_size = sample_size;
    dbg->stream = stream;
    dbg->loop = loop;
    dbg->ebo = ebo;
    dbg->cbo = cbo;
    dbg->lbo = lbo;
    dbg->ba = ba;
    dbg->samples_per_block = samples_per_block;
    dbg->persist = persist;
    dbg->multipass = multipass;
    dbg->linked = linked;

    // This is probably cleared when the first sample is played
    // FIXME: How will this behave if CBO > EBO on first play?
    // FIXME: How will this behave if paused?
    voice_set_mask(d, v, NV_PAVS_VOICE_PAR_STATE,
                   NV_PAVS_VOICE_PAR_STATE_NEW_VOICE, 0);

    if (paused) {
        return -1;
    }

    if (stream) {
        if (!persist) {
            // FIXME: Confirm. Unsure if this should wait until end of SSL or
            // terminate immediately. Definitely not before end of envelope.
            int eacur = voice_get_mask(d, v, NV_PAVS_VOICE_PAR_STATE,
                                       NV_PAVS_VOICE_PAR_STATE_EACUR);
            if (eacur < NV_PAVS_VOICE_PAR_STATE_EFCUR_RELEASE) {
                DPRINTF("Voice %d envelope not in release state (%d) and "
                        "persist is not set. Ending stream now!\n",
                        v, eacur);
                voice_off(d, v);
                return -1;
            }
        }

        DPRINTF("**** STREAMING (%d) ****\n", v);
        assert(!loop);

        ssl_index = d->vp.ssl[v].ssl_index;
        ssl_seg = d->vp.ssl[v].ssl_seg;
        page = d->vp.ssl[v].base[ssl_index] + ssl_seg;
        count = d->vp.ssl[v].count[ssl_index];

        // Check to see if the stream has ended
        if (count == 0) {
            DPRINTF("Stream has ended\n");
            voice_set_mask(d, v, NV_PAVS_VOICE_PAR_OFFSET,
                           NV_PAVS_VOICE_PAR_OFFSET_CBO, 0);
            d->vp.ssl[v].ssl_seg = 0;
            if (!persist) {
                d->vp.ssl[v].ssl_index = 0;
                voice_off(d, v);
            } else {
                set_notify_status(
                    d, v, MCPX_HW_NOTIFIER_SSLA_DONE + d->vp.ssl[v].ssl_index,
                    NV1BA0_NOTIFICATION_STATUS_DONE_SUCCESS);
            }
            return -1;
        }

        hwaddr addr = d->regs[NV_PAPU_VPSSLADDR] + page * 8;
        segment_offset = ldl_le_phys(&address_space_memory, addr);
        segment_length = ldl_le_phys(&address_space_memory, addr + 4);
        assert(segment_offset != 0);
        assert(segment_length != 0);
        seg_len = (segment_length >> 0) & 0xffff;
        seg_cs = (segment_length >> 16) & 3;
        seg_spb = (segment_length >> 18) & 0x1f;
        seg_s = (segment_length >> 23) & 1;
        assert(seg_cs == container_size_index);
        assert((seg_spb + 1) == samples_per_block);
        assert(seg_s == stereo);
        container_size_index = seg_cs;
        if (seg_cs == NV_PAVS_VOICE_CFG_FMT_CONTAINER_SIZE_ADPCM) {
            sample_size = NV_PAVS_VOICE_CFG_FMT_SAMPLE_SIZE_S24;
        }

        assert(seg_len > 0);
        ebo = seg_len - 1; // FIXME: Confirm seg_len-1 is last valid sample index

        DPRINTF("Segment: SSL%c[%d]\n", 'A' + ssl_index, ssl_seg);
        DPRINTF("Page: %x\n", page);
        DPRINTF("Count: %d\n", count);
        DPRINTF("Segment offset: 0x%" HWADDR_PRIx "\n", segment_offset);
        DPRINTF("Segment length: %x\n", segment_length);
        DPRINTF("...len = 0x%x\n", seg_len);
        DPRINTF("...cs  = %d (%s)\n", seg_cs, container_size_str[seg_cs]);
        DPRINTF("...spb = %d\n", seg_spb);
        DPRINTF("...s   = %d (%s)\n", seg_s, seg_s ? "stereo" : "mono");
    } else {
        DPRINTF("**** BUFFER (%d) ****\n", v);
    }

    bool adpcm =
        (container_size_index == NV_PAVS_VOICE_CFG_FMT_CONTAINER_SIZE_ADPCM);

    if (adpcm) {
        block_size = 36;
        DPRINTF("ADPCM:\n");
    } else {
        assert(container_size_index < 4);
        assert(sample_size < 4);
        block_size = container_size;
        DPRINTF("PCM:\n");
        DPRINTF("  Container Size: %s\n",
                container_size_str[container_size_index]);
        DPRINTF("  Sample Size: %s\n", sample_size_str[sample_size]);
    }

    DPRINTF("CBO=%d EBO=%d\n", cbo, ebo);

    block_size *= samples_per_block;

    // FIXME: Restructure this loop
    int sample_count = 0;
    for (; (sample_count < num_samples_requested) && (cbo <= ebo);
         sample_count++, cbo++) {
        if (adpcm) {
            unsigned int block_index = cbo / ADPCM_SAMPLES_PER_BLOCK;
            unsigned int block_position = cbo % ADPCM_SAMPLES_PER_BLOCK;
            if (adpcm_block_index != block_index) {
                uint32_t linear_addr = block_index * block_size;
                if (stream) {
                    hwaddr addr = segment_offset + linear_addr;
                    int max_seg_byte = (seg_len >> 6) * block_size;
                    assert(linear_addr + block_size <= max_seg_byte);
                    memcpy(adpcm_block, &d->ram_ptr[addr],
                           block_size); // FIXME: Use idiomatic DMA function
                } else {
                    linear_addr += ba;
                    for (unsigned int word_index = 0;
                         word_index < (9 * samples_per_block); word_index++) {
                        hwaddr addr = get_data_ptr(d->regs[NV_PAPU_VPSGEADDR],
                                                   0xFFFFFFFF, linear_addr);
                        adpcm_block[word_index] =
                            ldl_le_phys(&address_space_memory, addr);
                        linear_addr += 4;
                    }
                }
                adpcm_decode_block(adpcm_decoded, (uint8_t *)adpcm_block,
                                   block_size, channels);
                adpcm_block_index = block_index;
            }

            samples[sample_count][0] =
                int16_to_float(adpcm_decoded[block_position * channels]);
            if (stereo) {
                samples[sample_count][1] = int16_to_float(
                    adpcm_decoded[block_position * channels + 1]);
            }
        } else {
            // FIXME: Handle reading accross pages?!

            hwaddr addr;
            if (stream) {
                addr = segment_offset + cbo * block_size;
            } else {
                uint32_t linear_addr = ba + cbo * block_size;
                addr = get_data_ptr(d->regs[NV_PAPU_VPSGEADDR], 0xFFFFFFFF,
                                    linear_addr);
            }

            for (unsigned int channel = 0; channel < channels; channel++) {
                uint32_t ival;
                float fval;
                switch (sample_size) {
                case NV_PAVS_VOICE_CFG_FMT_SAMPLE_SIZE_U8:
                    ival = ldub_phys(&address_space_memory, addr);
                    fval = uint8_to_float(ival & 0xff);
                    break;
                case NV_PAVS_VOICE_CFG_FMT_SAMPLE_SIZE_S16:
                    ival = lduw_le_phys(&address_space_memory, addr);
                    fval = int16_to_float(ival & 0xffff);
                    break;
                case NV_PAVS_VOICE_CFG_FMT_SAMPLE_SIZE_S24:
                    ival = ldl_le_phys(&address_space_memory, addr);
                    fval = int24_to_float(ival);
                    break;
                case NV_PAVS_VOICE_CFG_FMT_SAMPLE_SIZE_S32:
                    ival = ldl_le_phys(&address_space_memory, addr);
                    fval = int32_to_float(ival);
                    break;
                default:
                    assert(false);
                    break;
                }
                samples[sample_count][channel] = fval;
                addr += container_size;
            }
        }

        if (!stereo) {
            samples[sample_count][1] = samples[sample_count][0];
        }
    }

    if (cbo >= ebo) {
        if (stream) {
            d->vp.ssl[v].ssl_seg += 1;
            cbo = 0;
            if (d->vp.ssl[v].ssl_seg < d->vp.ssl[v].count[ssl_index]) {
                DPRINTF("SSL%c[%d]\n", 'A' + ssl_index, d->vp.ssl[v].ssl_seg);
            } else {
                int next_index = (ssl_index + 1) % 2;
                DPRINTF("SSL%c\n", 'A' + next_index);
                d->vp.ssl[v].ssl_index = next_index;
                d->vp.ssl[v].ssl_seg = 0;
                set_notify_status(d, v, MCPX_HW_NOTIFIER_SSLA_DONE + ssl_index,
                                  NV1BA0_NOTIFICATION_STATUS_DONE_SUCCESS);
            }
        } else {
            if (loop) {
                cbo = lbo;
            } else {
                cbo = ebo;
                voice_off(d, v);
                DPRINTF("end of buffer!\n");
            }
        }
    }

    voice_set_mask(d, v, NV_PAVS_VOICE_PAR_OFFSET,
                   NV_PAVS_VOICE_PAR_OFFSET_CBO, cbo);
    return sample_count;
}

static long voice_resample_callback(void *cb_data, float **data)
{
    MCPXAPUVoiceFilter *filter = cb_data;
    uint16_t v = filter->voice;
    assert(v < MCPX_HW_MAX_VOICES);
    MCPXAPUState *d = container_of(filter, MCPXAPUState, vp.filters[v]);

    int sample_count = 0;
    while (sample_count < NUM_SAMPLES_PER_FRAME) {
        int active = voice_get_mask(d, v, NV_PAVS_VOICE_PAR_STATE,
                                    NV_PAVS_VOICE_PAR_STATE_ACTIVE_VOICE);
        if (!active) {
            break;
        }
        int count = voice_get_samples(
            d, v, (float(*)[2]) & filter->resample_buf[2 * sample_count],
            NUM_SAMPLES_PER_FRAME - sample_count);
        if (count < 0) {
            break;
        }
        sample_count += count;
    }

    if (sample_count < NUM_SAMPLES_PER_FRAME) {
        /* Starvation causes SRC hang on repeated calls. Provide silence. */
        memset(&filter->resample_buf[2*sample_count], 0,
            2*(NUM_SAMPLES_PER_FRAME-sample_count)*sizeof(float));
        sample_count = NUM_SAMPLES_PER_FRAME;
    }

    *data = filter->resample_buf;
    return sample_count;
}

static int voice_resample(MCPXAPUState *d, uint16_t v, float samples[][2],
                          int requested_num, float rate)
{
    assert(v < MCPX_HW_MAX_VOICES);
    MCPXAPUVoiceFilter *filter = &d->vp.filters[v];

    if (filter->resampler == NULL) {
        filter->voice = v;
        int err;

        /* Note: Using a sinc based resampler for quality. Unsure about
         * hardware's actual interpolation method; it could just be linear, in
         * which case using this resampler is overkill, but quality is good
         * so use it for now.
         */
        // FIXME: Don't do 2ch resampling if this is a mono voice
        filter->resampler = src_callback_new(&voice_resample_callback,
                                           SRC_SINC_FASTEST, 2, &err, filter);
        if (filter->resampler == NULL) {
            fprintf(stderr, "src error: %s\n", src_strerror(err));
            assert(0);
        }
    }

    int count = src_callback_read(filter->resampler, rate, requested_num,
                                  (float *)samples);
    if (count == -1) {
        DPRINTF("resample error\n");
    }
    if (count != requested_num) {
        DPRINTF("resample returned fewer than expected: %d\n", count);

        if (count == 0)
            return -1;
    }

    return count;
}

static int peek_ahead_multipass_bin(MCPXAPUState *d, uint16_t v,
                                    uint16_t *dst_voice)
{
    bool first = true;

    while (v != 0xFFFF) {
        bool multipass = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT,
                                        NV_PAVS_VOICE_CFG_FMT_MULTIPASS);
        if (multipass) {
            if (first) {
                break;
            }

            *dst_voice = v;
            int mp_bin = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT,
                                        NV_PAVS_VOICE_CFG_FMT_MULTIPASS_BIN);
            return mp_bin;
        }

        v = voice_get_mask(d, v, NV_PAVS_VOICE_TAR_PITCH_LINK,
                           NV_PAVS_VOICE_TAR_PITCH_LINK_NEXT_VOICE_HANDLE);
        first = false;
    }

    *dst_voice = 0xFFFF;
    return -1;
}

static void dump_multipass_unused_debug_info(MCPXAPUState *d, uint16_t v)
{
    unsigned int sample_size = voice_get_mask(
        d, v, NV_PAVS_VOICE_CFG_FMT, NV_PAVS_VOICE_CFG_FMT_SAMPLE_SIZE);
    unsigned int container_size_index = voice_get_mask(
        d, v, NV_PAVS_VOICE_CFG_FMT, NV_PAVS_VOICE_CFG_FMT_CONTAINER_SIZE);
    bool stream = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT,
                                 NV_PAVS_VOICE_CFG_FMT_DATA_TYPE);
    bool loop =
        voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT, NV_PAVS_VOICE_CFG_FMT_LOOP);
    uint32_t ebo = voice_get_mask(d, v, NV_PAVS_VOICE_PAR_NEXT,
                                  NV_PAVS_VOICE_PAR_NEXT_EBO);
    uint32_t cbo = voice_get_mask(d, v, NV_PAVS_VOICE_PAR_OFFSET,
                                  NV_PAVS_VOICE_PAR_OFFSET_CBO);
    uint32_t lbo = voice_get_mask(d, v, NV_PAVS_VOICE_CUR_PSH_SAMPLE,
                                  NV_PAVS_VOICE_CUR_PSH_SAMPLE_LBO);
    uint32_t ba = voice_get_mask(d, v, NV_PAVS_VOICE_CUR_PSL_START,
                                 NV_PAVS_VOICE_CUR_PSL_START_BA);
    bool persist = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT,
                                  NV_PAVS_VOICE_CFG_FMT_PERSIST);
    bool linked = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT,
                                 NV_PAVS_VOICE_CFG_FMT_LINKED);

    struct McpxApuDebugVoice *dbg = &g_dbg.vp.v[v];
    dbg->container_size = container_size_index;
    dbg->sample_size = sample_size;
    dbg->stream = stream;
    dbg->loop = loop;
    dbg->ebo = ebo;
    dbg->cbo = cbo;
    dbg->lbo = lbo;
    dbg->ba = ba;
    dbg->samples_per_block = 0; // Value overloaded with multipass bin
    dbg->persist = persist;
    dbg->linked = linked;
}

static void get_multipass_samples(MCPXAPUState *d,
                                  float mixbins[][NUM_SAMPLES_PER_FRAME],
                                  uint16_t v, float samples[][2])
{
    struct McpxApuDebugVoice *dbg = &g_dbg.vp.v[v];

    // DirectSound sets bin to 31, but hardware would allow other bins
    int mp_bin = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT,
                                NV_PAVS_VOICE_CFG_FMT_MULTIPASS_BIN);
    dbg->multipass_bin = mp_bin;

    for (int i = 0; i < NUM_SAMPLES_PER_FRAME; i++) {
        samples[i][0] = mixbins[mp_bin][i];
        samples[i][1] = mixbins[mp_bin][i];
    }

    // DirectSound sets clear mix to true
    bool clear_mix = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT,
                                    NV_PAVS_VOICE_CFG_FMT_CLEAR_MIX);
    if (clear_mix) {
        memset(&mixbins[mp_bin][0], 0, sizeof(mixbins[0]));
    }

    // Dump irrelevant data for audio debug UI to avoid showing stale info
    dump_multipass_unused_debug_info(d, v);
}

static void voice_process(MCPXAPUState *d,
                          float mixbins[NUM_MIXBINS][NUM_SAMPLES_PER_FRAME],
                          float sample_buf[NUM_SAMPLES_PER_FRAME][2],
                          uint16_t v, int voice_list)
{
    assert(v < MCPX_HW_MAX_VOICES);
    bool stereo = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT,
                                 NV_PAVS_VOICE_CFG_FMT_STEREO);
    unsigned int channels = stereo ? 2 : 1;
    bool paused = voice_get_mask(d, v, NV_PAVS_VOICE_PAR_STATE,
                                 NV_PAVS_VOICE_PAR_STATE_PAUSED);

    struct McpxApuDebugVoice *dbg = &g_dbg.vp.v[v];
    dbg->active = true;
    dbg->stereo = stereo;
    dbg->paused = paused;

    if (paused) {
        return;
    }

    float ef_value = voice_step_envelope(
        d, v, NV_PAVS_VOICE_CFG_ENV1, NV_PAVS_VOICE_CFG_ENVF,
        NV_PAVS_VOICE_CFG_MISC, NV_PAVS_VOICE_CFG_MISC_EF_RELEASERATE,
        NV_PAVS_VOICE_PAR_NEXT, NV_PAVS_VOICE_PAR_NEXT_EFLVL,
        NV_PAVS_VOICE_CUR_ECNT_EFCOUNT, NV_PAVS_VOICE_PAR_STATE_EFCUR);
    assert(ef_value >= 0.0f);
    assert(ef_value <= 1.0f);
    int16_t p = voice_get_mask(d, v, NV_PAVS_VOICE_TAR_PITCH_LINK,
                               NV_PAVS_VOICE_TAR_PITCH_LINK_PITCH);
    int8_t ps = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_ENV0,
                               NV_PAVS_VOICE_CFG_ENV0_EF_PITCHSCALE);
    float rate = 1.0 / powf(2.0f, (p + ps * 32 * ef_value) / 4096.0f);
    dbg->rate = rate;

    float ea_value = voice_step_envelope(
        d, v, NV_PAVS_VOICE_CFG_ENV0, NV_PAVS_VOICE_CFG_ENVA,
        NV_PAVS_VOICE_TAR_LFO_ENV, NV_PAVS_VOICE_TAR_LFO_ENV_EA_RELEASERATE,
        NV_PAVS_VOICE_PAR_OFFSET, NV_PAVS_VOICE_PAR_OFFSET_EALVL,
        NV_PAVS_VOICE_CUR_ECNT_EACOUNT, NV_PAVS_VOICE_PAR_STATE_EACUR);
    assert(ea_value >= 0.0f);
    assert(ea_value <= 1.0f);

    float samples[NUM_SAMPLES_PER_FRAME][2] = { 0 };

    bool multipass = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT,
                                    NV_PAVS_VOICE_CFG_FMT_MULTIPASS);
    dbg->multipass = multipass;

    if (multipass) {
        get_multipass_samples(d, mixbins, v, samples);
    } else {
        for (int sample_count = 0; sample_count < NUM_SAMPLES_PER_FRAME;) {
            int active = voice_get_mask(d, v, NV_PAVS_VOICE_PAR_STATE,
                                        NV_PAVS_VOICE_PAR_STATE_ACTIVE_VOICE);
            if (!active) {
                return;
            }
            int count =
                voice_resample(d, v, &samples[sample_count],
                               NUM_SAMPLES_PER_FRAME - sample_count, rate);
            if (count < 0) {
                break;
            }
            sample_count += count;
        }
    }

    int active = voice_get_mask(d, v, NV_PAVS_VOICE_PAR_STATE,
                                NV_PAVS_VOICE_PAR_STATE_ACTIVE_VOICE);
    if (!active) {
        return;
    }

    int bin[8];
    bin[0] = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_VBIN,
                            NV_PAVS_VOICE_CFG_VBIN_V0BIN);
    bin[1] = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_VBIN,
                            NV_PAVS_VOICE_CFG_VBIN_V1BIN);
    bin[2] = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_VBIN,
                            NV_PAVS_VOICE_CFG_VBIN_V2BIN);
    bin[3] = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_VBIN,
                            NV_PAVS_VOICE_CFG_VBIN_V3BIN);
    bin[4] = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_VBIN,
                            NV_PAVS_VOICE_CFG_VBIN_V4BIN);
    bin[5] = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_VBIN,
                            NV_PAVS_VOICE_CFG_VBIN_V5BIN);
    bin[6] = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT,
                            NV_PAVS_VOICE_CFG_FMT_V6BIN);
    bin[7] = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT,
                            NV_PAVS_VOICE_CFG_FMT_V7BIN);

    if (v < MCPX_HW_MAX_3D_VOICES) {
        bin[0] = d->vp.hrtf_submix[0];
        bin[1] = d->vp.hrtf_submix[1];
        bin[2] = d->vp.hrtf_submix[2];
        bin[3] = d->vp.hrtf_submix[3];
    }

    uint16_t vol[8];
    vol[0] = voice_get_mask(d, v, NV_PAVS_VOICE_TAR_VOLA,
                            NV_PAVS_VOICE_TAR_VOLA_VOLUME0);
    vol[1] = voice_get_mask(d, v, NV_PAVS_VOICE_TAR_VOLA,
                            NV_PAVS_VOICE_TAR_VOLA_VOLUME1);
    vol[2] = voice_get_mask(d, v, NV_PAVS_VOICE_TAR_VOLB,
                            NV_PAVS_VOICE_TAR_VOLB_VOLUME2);
    vol[3] = voice_get_mask(d, v, NV_PAVS_VOICE_TAR_VOLB,
                            NV_PAVS_VOICE_TAR_VOLB_VOLUME3);
    vol[4] = voice_get_mask(d, v, NV_PAVS_VOICE_TAR_VOLC,
                            NV_PAVS_VOICE_TAR_VOLC_VOLUME4);
    vol[5] = voice_get_mask(d, v, NV_PAVS_VOICE_TAR_VOLC,
                            NV_PAVS_VOICE_TAR_VOLC_VOLUME5);

    vol[6] = voice_get_mask(d, v, NV_PAVS_VOICE_TAR_VOLC,
                            NV_PAVS_VOICE_TAR_VOLC_VOLUME6_B11_8) << 8;
    vol[6] |= voice_get_mask(d, v, NV_PAVS_VOICE_TAR_VOLB,
                             NV_PAVS_VOICE_TAR_VOLB_VOLUME6_B7_4) << 4;
    vol[6] |= voice_get_mask(d, v, NV_PAVS_VOICE_TAR_VOLA,
                             NV_PAVS_VOICE_TAR_VOLA_VOLUME6_B3_0);
    vol[7] = voice_get_mask(d, v, NV_PAVS_VOICE_TAR_VOLC,
                            NV_PAVS_VOICE_TAR_VOLC_VOLUME7_B11_8) << 8;
    vol[7] |= voice_get_mask(d, v, NV_PAVS_VOICE_TAR_VOLB,
                             NV_PAVS_VOICE_TAR_VOLB_VOLUME7_B7_4) << 4;
    vol[7] |= voice_get_mask(d, v, NV_PAVS_VOICE_TAR_VOLA,
                             NV_PAVS_VOICE_TAR_VOLA_VOLUME7_B3_0);

    // FIXME: If phase negations means to flip the signal upside down
    //        we should modify volume of bin6 and bin7 here.

    for (int i = 0; i < 8; i++) {
        dbg->bin[i] = bin[i];
        dbg->vol[i] = vol[i];
    }

    if (voice_should_mute(v)) {
        return;
    }

    int fmode = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_MISC,
                               NV_PAVS_VOICE_CFG_MISC_FMODE);

    // FIXME: Move to function
    bool lpf = false;
    if (v < MCPX_HW_MAX_3D_VOICES) {
        /* 1:DLS2+I3DL2 2:ParaEQ+I3DL2 3:I3DL2 */
        lpf = (fmode == 1);
    } else {
        /* 0:Bypass 1:DLS2 2:ParaEQ 3(Mono):DLS2+ParaEQ 3(Stereo):Bypass */
        lpf = stereo ? (fmode == 1) : (fmode & 1);
    }
    if (lpf) {
        for (int ch = 0; ch < 2; ch++) {
            // FIXME: Cutoff modulation via NV_PAVS_VOICE_CFG_ENV1_EF_FCSCALE
            int16_t fc = voice_get_mask(
                d, v, NV_PAVS_VOICE_TAR_FCA + (ch % channels) * 4,
                NV_PAVS_VOICE_TAR_FCA_FC0);
            float fc_f = clampf(pow(2, fc / 4096.0), 0.003906f, 1.0f);
            uint16_t q = voice_get_mask(
                d, v, NV_PAVS_VOICE_TAR_FCA + (ch % channels) * 4,
                NV_PAVS_VOICE_TAR_FCA_FC1);
            float q_f = clampf(q / (1.0 * 0x8000), 0.079407f, 1.0f);
            sv_filter *filter = &d->vp.filters[v].svf[ch];
            setup_svf(filter, fc_f, q_f, F_LP);
            for (int i = 0; i < NUM_SAMPLES_PER_FRAME; i++) {
                samples[i][ch] = run_svf(filter, samples[i][ch]);
                samples[i][ch] = fmin(fmax(samples[i][ch], -1.0), 1.0);
            }
        }
    }

    if (v < MCPX_HW_MAX_3D_VOICES && g_config.audio.hrtf) {
        uint16_t hrtf_handle =
            voice_get_mask(d, v, NV_PAVS_VOICE_CFG_HRTF_TARGET,
                           NV_PAVS_VOICE_CFG_HRTF_TARGET_HANDLE);
        if (hrtf_handle != HRTF_NULL_HANDLE) {
            hrtf_filter_process(&d->vp.filters[v].hrtf, samples, samples);
        }
    }

    // FIXME: ParaEQ

    for (int b = 0; b < 8; b++) {
        float g = ea_value;
        float hr;
        if ((v < MCPX_HW_MAX_3D_VOICES) && (b < 4)) {
            // FIXME: Not sure if submix/voice headroom factor in for HRTF
            hr = 1 << d->vp.hrtf_headroom;
        } else {
            hr = 1 << d->vp.submix_headroom[bin[b]];
        }
        g *= attenuate(vol[b])/hr;
        for (int i = 0; i < NUM_SAMPLES_PER_FRAME; i++) {
            mixbins[bin[b]][i] += g*samples[i][b % channels];
        }
    }

    if (d->monitor.point == MCPX_APU_DEBUG_MON_VP) {
        /* For VP mon, simply mix all voices together here, selecting the
         * maximal volume used for any given mixbin as the overall volume for
         * this voice.
         *
         * If the current voice belongs to a multipass sub-voice group we must
         * skip it here to avoid mixing it in twice because the sub-voices are
         * mixed into the multipass bin and that sub-mix will be mixed in here
         * later when the destination (i.e. second pass) voice is processed.
         * TODO: Are the 2D, 3D and MP voice lists merely a DirectSound
         *       convention? Perhaps hardware doesn't care if e.g. a multipass
         *       voice is in the 2D or 3D list. On the other hand, MON_VP is
         *       not how the hardware works anyway so not much point worrying
         *       about precise emulation here. DirectSound compatibility is
         *       enough.
         */
        int mp_bin = -1;
        uint16_t mp_dst_voice = 0xFFFF;
        if (voice_list == NV1BA0_PIO_SET_ANTECEDENT_VOICE_LIST_MP_TOP - 1) {
            mp_bin = peek_ahead_multipass_bin(d, v, &mp_dst_voice);
        }
        dbg->multipass_dst_voice = mp_dst_voice;

        bool debug_isolation =
            g_dbg_voice_monitor >= 0 && g_dbg_voice_monitor == v;
        float g = 0.0f;
        for (int b = 0; b < 8; b++) {
            if (bin[b] == mp_bin && !debug_isolation) {
                continue;
            }
            float hr = 1 << d->vp.submix_headroom[bin[b]];
            g = fmax(g, attenuate(vol[b]) / hr);
        }
        g *= ea_value;
        for (int i = 0; i < NUM_SAMPLES_PER_FRAME; i++) {
            sample_buf[i][0] += g*samples[i][0];
            sample_buf[i][1] += g*samples[i][1];
        }
    }
}

static void get_voice_bin_src_dst(MCPXAPUState *d, int v,
                                  uint32_t *src, uint32_t *dst, uint32_t *clr)
{
    uint32_t src_v = 0;
    uint32_t dst_v = 0;
    uint32_t clr_v = 0;

    bool multipass = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT,
                                    NV_PAVS_VOICE_CFG_FMT_MULTIPASS);
    if (multipass) {
        int mp_bin = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT,
                                    NV_PAVS_VOICE_CFG_FMT_MULTIPASS_BIN);
        bool clear_mix = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT,
                                        NV_PAVS_VOICE_CFG_FMT_CLEAR_MIX);
        src_v |= (1 << mp_bin);
        if (clear_mix) {
            clr_v |= (1 << mp_bin);
        }
    }

    int bin[8];
    if (v < MCPX_HW_MAX_3D_VOICES) {
        bin[0] = d->vp.hrtf_submix[0];
        bin[1] = d->vp.hrtf_submix[1];
        bin[2] = d->vp.hrtf_submix[2];
        bin[3] = d->vp.hrtf_submix[3];
    } else {
        bin[0] = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_VBIN,
                                NV_PAVS_VOICE_CFG_VBIN_V0BIN);
        bin[1] = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_VBIN,
                                NV_PAVS_VOICE_CFG_VBIN_V1BIN);
        bin[2] = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_VBIN,
                                NV_PAVS_VOICE_CFG_VBIN_V2BIN);
        bin[3] = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_VBIN,
                                NV_PAVS_VOICE_CFG_VBIN_V3BIN);
    }
    bin[4] = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_VBIN,
                            NV_PAVS_VOICE_CFG_VBIN_V4BIN);
    bin[5] = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_VBIN,
                            NV_PAVS_VOICE_CFG_VBIN_V5BIN);
    bin[6] = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT,
                            NV_PAVS_VOICE_CFG_FMT_V6BIN);
    bin[7] = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT,
                            NV_PAVS_VOICE_CFG_FMT_V7BIN);

    for (int i = 0; i < 8; i++) {
        dst_v |= 1 << bin[i];
    }

    if (src) {
        *src = src_v;
    }
    if (dst) {
        *dst = dst_v;
    }
    if (clr) {
        *clr = clr_v;
    }
}

static void *voice_worker_thread(void *arg)
{
    MCPXAPUState *d = arg;
    VoiceWorkDispatch *vwd = &d->vp.voice_work_dispatch;

    rcu_register_thread();
    qemu_mutex_lock(&vwd->lock);

    int worker_id = ctz64(vwd->workers_pending);
    VoiceWorker *self = &d->vp.voice_work_dispatch.workers[worker_id];
    self->queue_len = 0;

    do {
        int64_t start_time = qemu_clock_get_us(QEMU_CLOCK_REALTIME);
        g_dbg.vp.workers[worker_id].num_voices = self->queue_len;

        if (self->queue_len) {
            qemu_mutex_unlock(&vwd->lock);

            // Process queued voices
            memset(self->mixbins, 0, sizeof(self->mixbins));
            if (d->monitor.point == MCPX_APU_DEBUG_MON_VP) {
                memset(self->sample_buf, 0, sizeof(self->sample_buf));
            }
            for (int i = 0; i < self->queue_len; i++) {
                voice_process(d, self->mixbins, self->sample_buf,
                              self->queue[i].voice, self->queue[i].list);
            }

            qemu_mutex_lock(&vwd->lock);

            // Add voice contributions
            for (int b = 0; b < NUM_MIXBINS; b++) {
                for (int s = 0; s < NUM_SAMPLES_PER_FRAME; s++) {
                    vwd->mixbins[b][s] += self->mixbins[b][s];
                }
            }
            if (d->monitor.point == MCPX_APU_DEBUG_MON_VP) {
                for (int i = 0; i < NUM_SAMPLES_PER_FRAME; i++) {
                    d->vp.sample_buf[i][0] += self->sample_buf[i][0];
                    d->vp.sample_buf[i][1] += self->sample_buf[i][1];
                }
            }

            self->queue_len = 0;
        }

        vwd->workers_pending &= ~(1 << worker_id);
        if (!vwd->workers_pending) {
            qemu_cond_signal(&vwd->work_finished);
        }

        int64_t end_time = qemu_clock_get_us(QEMU_CLOCK_REALTIME);
        g_dbg.vp.workers[worker_id].time_us = end_time - start_time;

        qemu_cond_wait(&vwd->work_pending, &vwd->lock);
    } while (!vwd->workers_should_exit);

    rcu_unregister_thread();
    return NULL;
}

static void voice_work_enqueue(MCPXAPUState *d, int v, int list)
{
    VoiceWorkDispatch *vwd = &d->vp.voice_work_dispatch;

    assert(vwd->queue_len < ARRAY_SIZE(vwd->queue));
    vwd->queue[vwd->queue_len++] = (VoiceWorkItem){
        .voice = v,
        .list = list,
    };
}

static void voice_work_schedule(MCPXAPUState *d)
{
    VoiceWorkDispatch *vwd = &d->vp.voice_work_dispatch;
    int next_worker_to_schedule = 0;
    bool group = false;
    uint32_t dirty = 0;

    for (int i = 0; i < vwd->queue_len; i++) {
        uint32_t src, dst, clr;
        get_voice_bin_src_dst(d, vwd->queue[i].voice, &src, &dst, &clr);

        // TODO: To simplify submix scheduling, we make a few assumptions based
        // on Xbox software observations. However, the configurability of
        // multipass sources suggests the hardware may not be so strict. We'll
        // defer making this more robust for now.
        //
        // We currently assume that:
        //
        // - MP bin is constant
        assert(!src || (src == MULTIPASS_BIN_MASK));
        //
        // - MP voice always clears MP bin
        assert(!src || (clr == MULTIPASS_BIN_MASK));
        //
        // - MP source voices are ordered consecutively in voice lists
        assert(src || (dst & MULTIPASS_BIN_MASK) ||
               !(dirty & MULTIPASS_BIN_MASK));

        if ((dst & MULTIPASS_BIN_MASK) & ~dirty) {
            group = true;
        }

        // Assign voice to worker
        VoiceWorker *worker = &vwd->workers[next_worker_to_schedule];
        worker->queue[worker->queue_len++] = vwd->queue[i];
        vwd->workers_pending |= 1 << next_worker_to_schedule;

        dirty = (dirty & ~clr) | dst;
        if (clr & MULTIPASS_BIN_MASK) {
            group = false;
        }

        if (!group) {
            next_worker_to_schedule =
                (next_worker_to_schedule + 1) % vwd->num_workers;
        }
    }
}

static bool any_queued_voice_locked(MCPXAPUState *d)
{
    VoiceWorkDispatch *vwd = &d->vp.voice_work_dispatch;

    for (int i = 0; i < vwd->queue_len; i++) {
        if (is_voice_locked(d, vwd->queue[i].voice)) {
            return true;
        }
    }

    return false;
}

static void
voice_work_dispatch(MCPXAPUState *d,
                    float mixbins[NUM_MIXBINS][NUM_SAMPLES_PER_FRAME])
{
    VoiceWorkDispatch *vwd = &d->vp.voice_work_dispatch;

    int64_t start_time = qemu_clock_get_us(QEMU_CLOCK_REALTIME);

    while (true) {
        if (qatomic_read(&d->pause_requested)) {
            vwd->queue_len = 0;
            return;
        }

        if (!any_queued_voice_locked(d)) {
            break;
        }

        qemu_cond_timedwait(&d->cond, &d->lock, 1);
    }

    qemu_mutex_lock(&vwd->lock);

    if (vwd->queue_len) {
        memset(vwd->mixbins, 0, sizeof(vwd->mixbins));

        // Signal workers and wait for completion
        voice_work_schedule(d);
        qemu_cond_broadcast(&vwd->work_pending);
        qemu_cond_wait(&vwd->work_finished, &vwd->lock);
        assert(!vwd->workers_pending);
        vwd->queue_len = 0;

        // Add voice contributions
        for (int b = 0; b < NUM_MIXBINS; b++) {
            for (int s = 0; s < NUM_SAMPLES_PER_FRAME; s++) {
                mixbins[b][s] += vwd->mixbins[b][s];
            }
        }
    }

    int64_t end_time = qemu_clock_get_us(QEMU_CLOCK_REALTIME);
    g_dbg.vp.total_worker_time_us = end_time - start_time;

    qemu_mutex_unlock(&vwd->lock);
}

static void voice_work_init(MCPXAPUState *d)
{
    VoiceWorkDispatch *vwd = &d->vp.voice_work_dispatch;

    int num_workers = g_config.audio.vp.num_workers ?: SDL_GetNumLogicalCPUCores();
    vwd->num_workers = MAX(1, MIN(num_workers, MAX_VOICE_WORKERS));
    vwd->workers = g_malloc0_n(vwd->num_workers, sizeof(VoiceWorker));
    vwd->workers_should_exit = false;
    vwd->workers_pending = 0;
    vwd->queue_len = 0;

    g_dbg.vp.num_workers = vwd->num_workers;

    qemu_mutex_init(&vwd->lock);
    qemu_mutex_lock(&vwd->lock);
    qemu_cond_init(&vwd->work_pending);
    qemu_cond_init(&vwd->work_finished);
    for (int i = 0; i < vwd->num_workers; i++) {
        vwd->workers_pending |= 1 << i;
        qemu_thread_create(&vwd->workers[i].thread, "mcpx.voice_worker",
                           voice_worker_thread, d, QEMU_THREAD_JOINABLE);
    }
    qemu_cond_wait(&vwd->work_finished, &vwd->lock);
    assert(!vwd->workers_pending);
    qemu_mutex_unlock(&vwd->lock);
}

static void voice_work_finalize(MCPXAPUState *d)
{
    VoiceWorkDispatch *vwd = &d->vp.voice_work_dispatch;

    qemu_mutex_lock(&vwd->lock);
    vwd->workers_should_exit = true;
    qemu_cond_broadcast(&vwd->work_pending);
    qemu_mutex_unlock(&vwd->lock);
    for (int i = 0; i < vwd->num_workers; i++) {
        qemu_thread_join(&vwd->workers[i].thread);
    }
    g_free(vwd->workers);
    vwd->workers = NULL;
}

void mcpx_apu_vp_frame(MCPXAPUState *d, float mixbins[NUM_MIXBINS][NUM_SAMPLES_PER_FRAME])
{
    memset(d->vp.sample_buf, 0, sizeof(d->vp.sample_buf));

    /* Process all voices, mixing each into the affected MIXBINs */
    for (int list = 0; list < 3; list++) {
        hwaddr top, current, next;
        top = voice_list_regs[list].top;
        current = voice_list_regs[list].current;
        next = voice_list_regs[list].next;

        d->regs[current] = d->regs[top];
        DPRINTF("list %d current voice %d\n", list, d->regs[current]);

        for (int i = 0; d->regs[current] != 0xFFFF; i++) {
            /* Make sure not to get stuck... */
            if (i >= MCPX_HW_MAX_VOICES) {
                DPRINTF("Voice list contains invalid entry!\n");
                break;
            }

            uint16_t v = d->regs[current];
            d->regs[next] = voice_get_mask(d, v, NV_PAVS_VOICE_TAR_PITCH_LINK,
                               NV_PAVS_VOICE_TAR_PITCH_LINK_NEXT_VOICE_HANDLE);
            if (!voice_get_mask(d, v, NV_PAVS_VOICE_PAR_STATE,
                                NV_PAVS_VOICE_PAR_STATE_ACTIVE_VOICE)) {
                fe_method(d, SE2FE_IDLE_VOICE, v);
            } else {
                voice_work_enqueue(d, v, list);
            }
            d->regs[current] = d->regs[next];
        }
    }
    voice_work_dispatch(d, mixbins);

    if (d->monitor.point == MCPX_APU_DEBUG_MON_VP) {
        /* Mix all voices together to hear any audible voice */
        int16_t isamp[NUM_SAMPLES_PER_FRAME * 2];
        src_float_to_short_array((float *)d->vp.sample_buf, isamp,
                                 NUM_SAMPLES_PER_FRAME * 2);
        int off = (d->ep_frame_div % 8) * NUM_SAMPLES_PER_FRAME;
        for (int i = 0; i < NUM_SAMPLES_PER_FRAME; i++) {
            d->monitor.frame_buf[off + i][0] += isamp[2*i];
            d->monitor.frame_buf[off + i][1] += isamp[2*i+1];
        }

        memset(d->vp.sample_buf, 0, sizeof(d->vp.sample_buf));
        memset(mixbins, 0, sizeof(float[32][32]));
    }
}

void mcpx_apu_vp_init(MCPXAPUState *d)
{
    voice_work_init(d);
}

void mcpx_apu_vp_finalize(MCPXAPUState *d)
{
    voice_work_finalize(d);
}

void mcpx_apu_vp_reset(MCPXAPUState *d)
{
    d->vp.ssl_base_page = 0;
    d->vp.hrtf_headroom = 0;
    memset(d->vp.ssl, 0, sizeof(d->vp.ssl));
    memset(d->vp.hrtf_submix, 0, sizeof(d->vp.hrtf_submix));
    memset(d->vp.submix_headroom, 0, sizeof(d->vp.submix_headroom));
    memset(d->vp.voice_locked, 0, sizeof(d->vp.voice_locked));
    for (int v = 0; v < ARRAY_SIZE(d->vp.filters); v++) {
        hrtf_filter_init(&d->vp.filters[v].hrtf);
    }
}
