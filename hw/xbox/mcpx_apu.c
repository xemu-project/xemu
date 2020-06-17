/*
 * QEMU MCPX Audio Processing Unit implementation
 *
 * Copyright (c) 2012 espes
 * Copyright (c) 2018-2019 Jannik Vogel
 * Copyright (c) 2020 Matt Borgerson
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
#include "hw/hw.h"
#include "hw/i386/pc.h"
#include "hw/pci/pci.h"
#include "cpu.h"
#include "hw/xbox/dsp/dsp.h"
#include "hw/xbox/dsp/dsp_dma.h"
#include "hw/xbox/dsp/dsp_cpu.h"
#include "hw/xbox/dsp/dsp_state.h"
#include "migration/vmstate.h"

#include <math.h>

#define NUM_SAMPLES_PER_FRAME 32
#define NUM_MIXBINS 32

#include "hw/xbox/mcpx_apu.h"

#define NV_PAPU_ISTS                                     0x00001000
#   define NV_PAPU_ISTS_GINTSTS                               (1 << 0)
#   define NV_PAPU_ISTS_FETINTSTS                             (1 << 4)
#define NV_PAPU_IEN                                      0x00001004
#define NV_PAPU_FECTL                                    0x00001100
#   define NV_PAPU_FECTL_FEMETHMODE                         0x000000E0
#       define NV_PAPU_FECTL_FEMETHMODE_FREE_RUNNING            0x00000000
#       define NV_PAPU_FECTL_FEMETHMODE_HALTED                  0x00000080
#       define NV_PAPU_FECTL_FEMETHMODE_TRAPPED                 0x000000E0
#   define NV_PAPU_FECTL_FETRAPREASON                       0x00000F00
#       define NV_PAPU_FECTL_FETRAPREASON_REQUESTED             0x00000F00
#define NV_PAPU_FECV                                     0x00001110
#define NV_PAPU_FEAV                                     0x00001118
#   define NV_PAPU_FEAV_VALUE                               0x0000FFFF
#   define NV_PAPU_FEAV_LST                                 0x00030000
#define NV_PAPU_FEDECMETH                                0x00001300
#define NV_PAPU_FEDECPARAM                               0x00001304
#define NV_PAPU_FEMEMADDR                                0x00001324
#define NV_PAPU_FEMEMDATA                                0x00001334
#define NV_PAPU_FETFORCE0                                0x00001500
#define NV_PAPU_FETFORCE1                                0x00001504
#   define NV_PAPU_FETFORCE1_SE2FE_IDLE_VOICE               (1 << 15)
#define NV_PAPU_SECTL                                    0x00002000
#   define NV_PAPU_SECTL_XCNTMODE                           0x00000018
#       define NV_PAPU_SECTL_XCNTMODE_OFF                       0
#define NV_PAPU_XGSCNT                                   0x0000200C
#define NV_PAPU_VPVADDR                                  0x0000202C
#define NV_PAPU_VPSGEADDR                                0x00002030
#define NV_PAPU_GPSADDR                                  0x00002040
#define NV_PAPU_GPFADDR                                  0x00002044
#define NV_PAPU_EPSADDR                                  0x00002048
#define NV_PAPU_EPFADDR                                  0x0000204C
#define NV_PAPU_TVL2D                                    0x00002054
#define NV_PAPU_CVL2D                                    0x00002058
#define NV_PAPU_NVL2D                                    0x0000205C
#define NV_PAPU_TVL3D                                    0x00002060
#define NV_PAPU_CVL3D                                    0x00002064
#define NV_PAPU_NVL3D                                    0x00002068
#define NV_PAPU_TVLMP                                    0x0000206C
#define NV_PAPU_CVLMP                                    0x00002070
#define NV_PAPU_NVLMP                                    0x00002074
#define NV_PAPU_GPSMAXSGE                                0x000020D4
#define NV_PAPU_GPFMAXSGE                                0x000020D8
#define NV_PAPU_EPSMAXSGE                                0x000020DC
#define NV_PAPU_EPFMAXSGE                                0x000020E0

/* Each FIFO has the same fields */
#define NV_PAPU_GPOFBASE0                                0x00003024
#   define NV_PAPU_GPOFBASE0_VALUE                          0x00FFFF00
#define NV_PAPU_GPOFEND0                                 0x00003028
#   define NV_PAPU_GPOFEND0_VALUE                           0x00FFFF00
#define NV_PAPU_GPOFCUR0                                 0x0000302C
#   define NV_PAPU_GPOFCUR0_VALUE                           0x00FFFFFC
#define NV_PAPU_GPOFBASE1                                0x00003034
#define NV_PAPU_GPOFEND1                                 0x00003038
#define NV_PAPU_GPOFCUR1                                 0x0000303C
#define NV_PAPU_GPOFBASE2                                0x00003044
#define NV_PAPU_GPOFEND2                                 0x00003048
#define NV_PAPU_GPOFCUR2                                 0x0000304C
#define NV_PAPU_GPOFBASE3                                0x00003054
#define NV_PAPU_GPOFEND3                                 0x00003058
#define NV_PAPU_GPOFCUR3                                 0x0000305C

/* Fields are same as for the 4 output FIFOs, but only 2 input FIFOs */
#define NV_PAPU_GPIFBASE0                                0x00003064
#define NV_PAPU_GPIFEND0                                 0x00003068
#define NV_PAPU_GPIFCUR0                                 0x0000306C
#define NV_PAPU_GPIFBASE1                                0x00003074
#define NV_PAPU_GPIFEND1                                 0x00003078
#define NV_PAPU_GPIFCUR1                                 0x0000307C

/* Fields, strides and count is same as for GP FIFOs */
#define NV_PAPU_EPOFBASE0                                0x00004024
#define NV_PAPU_EPOFEND0                                 0x00004028
#define NV_PAPU_EPOFCUR0                                 0x0000402C
#define NV_PAPU_EPIFBASE0                                0x00004064
#define NV_PAPU_EPIFEND0                                 0x00004068
#define NV_PAPU_EPIFCUR0                                 0x0000406C

#define NV_PAPU_GPXMEM                                   0x00000000
#define NV_PAPU_GPMIXBUF                                 0x00005000
#define NV_PAPU_GPYMEM                                   0x00006000
#define NV_PAPU_GPPMEM                                   0x0000A000
#define NV_PAPU_GPRST                                    0x0000FFFC
#define NV_PAPU_GPRST_GPRST                                 (1 << 0)
#define NV_PAPU_GPRST_GPDSPRST                              (1 << 1)
#define NV_PAPU_GPRST_GPNMI                                 (1 << 2)
#define NV_PAPU_GPRST_GPABORT                               (1 << 3)

#define NV_PAPU_EPXMEM                                   0x00000000
#define NV_PAPU_EPYMEM                                   0x00006000
#define NV_PAPU_EPPMEM                                   0x0000A000
#define NV_PAPU_EPRST                                    0x0000FFFC

static const struct {
    hwaddr top, current, next;
} voice_list_regs[] = {
    {NV_PAPU_TVL2D, NV_PAPU_CVL2D, NV_PAPU_NVL2D}, //2D
    {NV_PAPU_TVL3D, NV_PAPU_CVL3D, NV_PAPU_NVL3D}, //3D
    {NV_PAPU_TVLMP, NV_PAPU_CVLMP, NV_PAPU_NVLMP}, //MP
};


/* audio processor object / front-end messages */
#define NV1BA0_PIO_FREE                                  0x00000010
#define NV1BA0_PIO_SET_ANTECEDENT_VOICE                  0x00000120
#   define NV1BA0_PIO_SET_ANTECEDENT_VOICE_HANDLE           0x0000FFFF
#   define NV1BA0_PIO_SET_ANTECEDENT_VOICE_LIST             0x00030000
#       define NV1BA0_PIO_SET_ANTECEDENT_VOICE_LIST_INHERIT     0
#       define NV1BA0_PIO_SET_ANTECEDENT_VOICE_LIST_2D_TOP      1
#       define NV1BA0_PIO_SET_ANTECEDENT_VOICE_LIST_3D_TOP      2
#       define NV1BA0_PIO_SET_ANTECEDENT_VOICE_LIST_MP_TOP      3
#define NV1BA0_PIO_VOICE_ON                              0x00000124
#   define NV1BA0_PIO_VOICE_ON_HANDLE                       0x0000FFFF
#   define NV1BA0_PIO_VOICE_ON_ENVF                         0x0F000000
#   define NV1BA0_PIO_VOICE_ON_ENVA                         0xF0000000
#define NV1BA0_PIO_VOICE_OFF                             0x00000128
#   define NV1BA0_PIO_VOICE_OFF_HANDLE                      0x0000FFFF
#define NV1BA0_PIO_VOICE_RELEASE                           0x0000012C
#   define NV1BA0_PIO_VOICE_RELEASE_HANDLE                  0x0000FFFF
#define NV1BA0_PIO_VOICE_PAUSE                           0x00000140
#   define NV1BA0_PIO_VOICE_PAUSE_HANDLE                    0x0000FFFF
#   define NV1BA0_PIO_VOICE_PAUSE_ACTION                    (1 << 18)
#define NV1BA0_PIO_SET_CURRENT_VOICE                     0x000002F8
#define NV1BA0_PIO_SET_VOICE_CFG_VBIN                    0x00000300
#define NV1BA0_PIO_SET_VOICE_CFG_FMT                     0x00000304
#define NV1BA0_PIO_SET_VOICE_CFG_ENV0                    0x00000308
#define NV1BA0_PIO_SET_VOICE_CFG_ENVA                    0x0000030C
#define NV1BA0_PIO_SET_VOICE_CFG_ENV1                    0x00000310
#define NV1BA0_PIO_SET_VOICE_CFG_ENVF                    0x00000314
#define NV1BA0_PIO_SET_VOICE_CFG_MISC                    0x00000318
#define NV1BA0_PIO_SET_VOICE_TAR_VOLA                    0x00000360
#define NV1BA0_PIO_SET_VOICE_TAR_VOLB                    0x00000364
#define NV1BA0_PIO_SET_VOICE_TAR_VOLC                    0x00000368
#define NV1BA0_PIO_SET_VOICE_LFO_ENV                     0x0000036C
#define NV1BA0_PIO_SET_VOICE_TAR_PITCH                   0x0000037C
#   define NV1BA0_PIO_SET_VOICE_TAR_PITCH_STEP              0xFFFF0000
#define NV1BA0_PIO_SET_VOICE_CFG_BUF_BASE                0x000003A0
#   define NV1BA0_PIO_SET_VOICE_CFG_BUF_BASE_OFFSET         0x00FFFFFF
#define NV1BA0_PIO_SET_VOICE_CFG_BUF_LBO                 0x000003A4
#   define NV1BA0_PIO_SET_VOICE_CFG_BUF_LBO_OFFSET          0x00FFFFFF
#define NV1BA0_PIO_SET_VOICE_BUF_CBO                     0x000003D8
#   define NV1BA0_PIO_SET_VOICE_BUF_CBO_OFFSET              0x00FFFFFF
#define NV1BA0_PIO_SET_VOICE_CFG_BUF_EBO                 0x000003DC
#   define NV1BA0_PIO_SET_VOICE_CFG_BUF_EBO_OFFSET          0x00FFFFFF
#define NV1BA0_PIO_SET_CURRENT_INBUF_SGE                 0x00000804
#   define NV1BA0_PIO_SET_CURRENT_INBUF_SGE_HANDLE          0xFFFFFFFF
#define NV1BA0_PIO_SET_CURRENT_INBUF_SGE_OFFSET          0x00000808
#   define NV1BA0_PIO_SET_CURRENT_INBUF_SGE_OFFSET_PARAMETER 0xFFFFF000
#define NV1BA0_PIO_SET_OUTBUF_BA                         0x00001000 // 8 byte pitch, 4 entries
#   define NV1BA0_PIO_SET_OUTBUF_BA_ADDRESS                  0x007FFF00
#define NV1BA0_PIO_SET_OUTBUF_LEN                        0x00001004 // 8 byte pitch, 4 entries
#   define NV1BA0_PIO_SET_OUTBUF_LEN_VALUE                   0x007FFF00
#define NV1BA0_PIO_SET_CURRENT_OUTBUF_SGE                0x00001800
#   define NV1BA0_PIO_SET_CURRENT_OUTBUF_SGE_HANDLE          0xFFFFFFFF
#define NV1BA0_PIO_SET_CURRENT_OUTBUF_SGE_OFFSET         0x00001808
#   define NV1BA0_PIO_SET_CURRENT_OUTBUF_SGE_OFFSET_PARAMETER 0xFFFFF000

#define SE2FE_IDLE_VOICE                                 0x00008000


/* voice structure */
#define NV_PAVS_SIZE                                     0x00000080
#define NV_PAVS_VOICE_CFG_VBIN                           0x00000000
#   define NV_PAVS_VOICE_CFG_VBIN_V0BIN                     (0x1F << 0)
#   define NV_PAVS_VOICE_CFG_VBIN_V1BIN                     (0x1F << 5)
#   define NV_PAVS_VOICE_CFG_VBIN_V2BIN                     (0x1F << 10)
#   define NV_PAVS_VOICE_CFG_VBIN_V3BIN                     (0x1F << 16)
#   define NV_PAVS_VOICE_CFG_VBIN_V4BIN                     (0x1F << 21)
#   define NV_PAVS_VOICE_CFG_VBIN_V5BIN                     (0x1F << 26)
#define NV_PAVS_VOICE_CFG_FMT                            0x00000004
#   define NV_PAVS_VOICE_CFG_FMT_V6BIN                      (0x1F << 0)
#   define NV_PAVS_VOICE_CFG_FMT_V7BIN                      (0x1F << 5)
#   define NV_PAVS_VOICE_CFG_FMT_SAMPLES_PER_BLOCK          (0x1F << 16)
#   define NV_PAVS_VOICE_CFG_FMT_DATA_TYPE                  (1 << 24)
#   define NV_PAVS_VOICE_CFG_FMT_LOOP                       (1 << 25)
#   define NV_PAVS_VOICE_CFG_FMT_STEREO                     (1 << 27)
#   define NV_PAVS_VOICE_CFG_FMT_SAMPLE_SIZE                (0x3 << 28)
#       define NV_PAVS_VOICE_CFG_FMT_SAMPLE_SIZE_U8             0
#       define NV_PAVS_VOICE_CFG_FMT_SAMPLE_SIZE_S16            1
#       define NV_PAVS_VOICE_CFG_FMT_SAMPLE_SIZE_S24            2
#       define NV_PAVS_VOICE_CFG_FMT_SAMPLE_SIZE_S32            3
#   define NV_PAVS_VOICE_CFG_FMT_CONTAINER_SIZE             (0x3 << 30)
#       define NV_PAVS_VOICE_CFG_FMT_CONTAINER_SIZE_B8          0
#       define NV_PAVS_VOICE_CFG_FMT_CONTAINER_SIZE_B16         1
#       define NV_PAVS_VOICE_CFG_FMT_CONTAINER_SIZE_ADPCM       2
#       define NV_PAVS_VOICE_CFG_FMT_CONTAINER_SIZE_B32         3
#define NV_PAVS_VOICE_CFG_ENV0                           0x00000008
#   define NV_PAVS_VOICE_CFG_ENV0_EA_ATTACKRATE             (0xFFF << 0)
#   define NV_PAVS_VOICE_CFG_ENV0_EA_DELAYTIME              (0xFFF << 12)
#   define NV_PAVS_VOICE_CFG_ENV0_EF_PITCHSCALE             (0xFF << 24)
#define NV_PAVS_VOICE_CFG_ENVA                           0x0000000C
#   define NV_PAVS_VOICE_CFG_ENVA_EA_DECAYRATE              (0xFFF << 0)
#   define NV_PAVS_VOICE_CFG_ENVA_EA_HOLDTIME               (0xFFF << 12)
#   define NV_PAVS_VOICE_CFG_ENVA_EA_SUSTAINLEVEL           (0xFF << 24)
#define NV_PAVS_VOICE_CFG_ENV1                           0x00000010
#   define NV_PAVS_VOICE_CFG_ENV1_EF_FCSCALE                (0xFF << 24)
#define NV_PAVS_VOICE_CFG_ENVF                           0x00000014
#define NV_PAVS_VOICE_CFG_MISC                           0x00000018
#   define NV_PAVS_VOICE_CFG_MISC_EF_RELEASERATE            (0xFFF << 0)

#define NV_PAVS_VOICE_CUR_PSL_START                      0x00000020
#   define NV_PAVS_VOICE_CUR_PSL_START_BA                   0x00FFFFFF
#define NV_PAVS_VOICE_CUR_PSH_SAMPLE                     0x00000024
#   define NV_PAVS_VOICE_CUR_PSH_SAMPLE_LBO                 0x00FFFFFF

#define NV_PAVS_VOICE_CUR_ECNT                           0x00000034
#   define NV_PAVS_VOICE_CUR_ECNT_EACOUNT                   0x0000FFFF
#   define NV_PAVS_VOICE_CUR_ECNT_EFCOUNT                   0xFFFF0000

#define NV_PAVS_VOICE_PAR_STATE                          0x00000054
#   define NV_PAVS_VOICE_PAR_STATE_PAUSED                   (1 << 18)
#   define NV_PAVS_VOICE_PAR_STATE_NEW_VOICE                (1 << 20)
#   define NV_PAVS_VOICE_PAR_STATE_ACTIVE_VOICE             (1 << 21)
#   define NV_PAVS_VOICE_PAR_STATE_EFCUR                    (0xF << 24)
#   define NV_PAVS_VOICE_PAR_STATE_EACUR                    (0xF << 28)
#define NV_PAVS_VOICE_PAR_OFFSET                         0x00000058
#   define NV_PAVS_VOICE_PAR_OFFSET_CBO                     0x00FFFFFF
#   define NV_PAVS_VOICE_PAR_OFFSET_EALVL                   0xFF000000
#define NV_PAVS_VOICE_PAR_NEXT                           0x0000005C
#   define NV_PAVS_VOICE_PAR_NEXT_EBO                       0x00FFFFFF
#   define NV_PAVS_VOICE_PAR_NEXT_EFLVL                     0xFF000000
#define NV_PAVS_VOICE_TAR_VOLA                           0x00000060
#   define NV_PAVS_VOICE_TAR_VOLA_VOLUME6_B3_0              0x0000000F
#   define NV_PAVS_VOICE_TAR_VOLA_VOLUME0                   0x0000FFF0
#   define NV_PAVS_VOICE_TAR_VOLA_VOLUME7_B3_0              0x000F0000
#   define NV_PAVS_VOICE_TAR_VOLA_VOLUME1                   0xFFF00000
#define NV_PAVS_VOICE_TAR_VOLB                           0x00000064
#   define NV_PAVS_VOICE_TAR_VOLB_VOLUME6_B7_4              0x0000000F
#   define NV_PAVS_VOICE_TAR_VOLB_VOLUME2                   0x0000FFF0
#   define NV_PAVS_VOICE_TAR_VOLB_VOLUME7_B7_4              0x000F0000
#   define NV_PAVS_VOICE_TAR_VOLB_VOLUME3                   0xFFF00000
#define NV_PAVS_VOICE_TAR_VOLC                           0x00000068
#   define NV_PAVS_VOICE_TAR_VOLC_VOLUME6_B11_8             0x0000000F
#   define NV_PAVS_VOICE_TAR_VOLC_VOLUME4                   0x0000FFF0
#   define NV_PAVS_VOICE_TAR_VOLC_VOLUME7_B11_8             0x000F0000
#   define NV_PAVS_VOICE_TAR_VOLC_VOLUME5                   0xFFF00000
#define NV_PAVS_VOICE_TAR_LFO_ENV                        0x0000006C
#   define NV_PAVS_VOICE_TAR_LFO_ENV_EA_RELEASERATE         (0xFFF << 0)

#define NV_PAVS_VOICE_TAR_PITCH_LINK                     0x0000007C
#   define NV_PAVS_VOICE_TAR_PITCH_LINK_NEXT_VOICE_HANDLE   0x0000FFFF
#   define NV_PAVS_VOICE_TAR_PITCH_LINK_PITCH               0xFFFF0000


#define GP_DSP_MIXBUF_BASE 0x001400

#define GP_OUTPUT_FIFO_COUNT  4
#define GP_INPUT_FIFO_COUNT   2

#define EP_OUTPUT_FIFO_COUNT  4
#define EP_INPUT_FIFO_COUNT   2

#define MCPX_HW_MAX_VOICES 256

#define GET_MASK(v, mask) (((v) & (mask)) >> ctz32(mask))

#define SET_MASK(v, mask, val)                                       \
    do {                                                             \
        (v) &= ~(mask);                                              \
        (v) |= ((val) << ctz32(mask)) & (mask);                      \
    } while (0)

#define CASE_4(v, step)                                              \
    case (v):                                                        \
    case (v)+(step):                                                 \
    case (v)+(step)*2:                                               \
    case (v)+(step)*3


// #define MCPX_DEBUG
#ifdef MCPX_DEBUG
# define MCPX_DPRINTF(format, ...)       printf(format, ## __VA_ARGS__)
#else
# define MCPX_DPRINTF(format, ...)       do { } while (0)
#endif

/* More debug functionality */
#define GENERATE_MIXBIN_BEEP      0

typedef struct MCPXAPUState {
    PCIDevice dev;

    MemoryRegion *ram;
    uint8_t *ram_ptr;

    MemoryRegion mmio;

    /* Setup Engine */
    struct {
        QEMUTimer *frame_timer;
    } se;

    /* Voice Processor */
    struct {
        MemoryRegion mmio;
    } vp;

    /* Global Processor */
    struct {
        MemoryRegion mmio;
        DSPState *dsp;
        uint32_t regs[0x10000];
    } gp;

    /* Encode Processor */
    struct {
        MemoryRegion mmio;
        DSPState *dsp;
        uint32_t regs[0x10000];
    } ep;

    uint32_t inbuf_sge_handle; //FIXME: Where is this stored?
    uint32_t outbuf_sge_handle; //FIXME: Where is this stored?
    uint32_t regs[0x20000];

} MCPXAPUState;

#define MCPX_APU_DEVICE(obj) \
    OBJECT_CHECK(MCPXAPUState, (obj), "mcpx-apu")

static uint32_t voice_get_mask(MCPXAPUState *d,
                               unsigned int voice_handle,
                               hwaddr offset,
                               uint32_t mask)
{
    assert(voice_handle < 0xFFFF);
    hwaddr voice = d->regs[NV_PAPU_VPVADDR]
                    + voice_handle * NV_PAVS_SIZE;
    return (ldl_le_phys(&address_space_memory, voice + offset) & mask)
              >> ctz32(mask);
}
static void voice_set_mask(MCPXAPUState *d,
                           unsigned int voice_handle,
                           hwaddr offset,
                           uint32_t mask,
                           uint32_t val)
{
    assert(voice_handle < 0xFFFF);
    hwaddr voice = d->regs[NV_PAPU_VPVADDR]
                    + voice_handle * NV_PAVS_SIZE;
    uint32_t v = ldl_le_phys(&address_space_memory, voice + offset) & ~mask;
    stl_le_phys(&address_space_memory, voice + offset,
                v | ((val << ctz32(mask)) & mask));
}

static void update_irq(MCPXAPUState *d)
{
    if ((d->regs[NV_PAPU_IEN] & NV_PAPU_ISTS_GINTSTS)
        && ((d->regs[NV_PAPU_ISTS] & ~NV_PAPU_ISTS_GINTSTS)
              & d->regs[NV_PAPU_IEN])) {

        d->regs[NV_PAPU_ISTS] |= NV_PAPU_ISTS_GINTSTS;
        MCPX_DPRINTF("mcpx irq raise\n");
        pci_irq_assert(&d->dev);
    } else {
        d->regs[NV_PAPU_ISTS] &= ~NV_PAPU_ISTS_GINTSTS;
        MCPX_DPRINTF("mcpx irq lower\n");
        pci_irq_deassert(&d->dev);
    }
}

static uint64_t mcpx_apu_read(void *opaque,
                              hwaddr addr, unsigned int size)
{
    MCPXAPUState *d = opaque;

    uint64_t r = 0;
    switch (addr) {
    case NV_PAPU_XGSCNT:
        r = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) / 100; //???
        break;
    default:
        if (addr < 0x20000) {
            r = d->regs[addr];
        }
        break;
    }

    MCPX_DPRINTF("mcpx apu: read [0x%llx] -> 0x%llx\n", addr, r);
    return r;
}

static void mcpx_apu_write(void *opaque, hwaddr addr,
                           uint64_t val, unsigned int size)
{
    MCPXAPUState *d = opaque;

    MCPX_DPRINTF("mcpx apu: [0x%llx] = 0x%llx\n", addr, val);

    switch (addr) {
    case NV_PAPU_ISTS:
        /* the bits of the interrupts to clear are wrtten */
        d->regs[NV_PAPU_ISTS] &= ~val;
        update_irq(d);
        break;
    case NV_PAPU_SECTL:
        if (((val & NV_PAPU_SECTL_XCNTMODE) >> 3)
              == NV_PAPU_SECTL_XCNTMODE_OFF) {
            timer_del(d->se.frame_timer);
        } else {
            timer_mod(d->se.frame_timer,
                qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL) + 10);
        }
        d->regs[addr] = val;
        break;
    case NV_PAPU_FEMEMDATA:
        /* 'magic write'
         * This value is expected to be written to FEMEMADDR on completion of
         * something to do with notifies. Just do it now :/ */
        stl_le_phys(&address_space_memory, d->regs[NV_PAPU_FEMEMADDR], val);
        d->regs[addr] = val;
        break;
    default:
        if (addr < 0x20000) {
            d->regs[addr] = val;
        }
        break;
    }
}

static const MemoryRegionOps mcpx_apu_mmio_ops = {
    .read = mcpx_apu_read,
    .write = mcpx_apu_write,
};

static void fe_method(MCPXAPUState *d,
                      uint32_t method, uint32_t argument)
{
#ifdef MCPX_DEBUG
    unsigned int slot;
#endif

    MCPX_DPRINTF("mcpx fe_method 0x%x 0x%x\n", method, argument);

    //assert((d->regs[NV_PAPU_FECTL] & NV_PAPU_FECTL_FEMETHMODE) == 0);

    d->regs[NV_PAPU_FEDECMETH] = method;
    d->regs[NV_PAPU_FEDECPARAM] = argument;
    unsigned int selected_handle, list;
    switch (method) {
    case NV1BA0_PIO_SET_ANTECEDENT_VOICE:
        d->regs[NV_PAPU_FEAV] = argument;
        break;
    case NV1BA0_PIO_VOICE_ON:
        selected_handle = argument & NV1BA0_PIO_VOICE_ON_HANDLE;
        list = GET_MASK(d->regs[NV_PAPU_FEAV], NV_PAPU_FEAV_LST);
        if (list != NV1BA0_PIO_SET_ANTECEDENT_VOICE_LIST_INHERIT) {
            /* voice is added to the top of the selected list */
            unsigned int top_reg = voice_list_regs[list - 1].top;
            voice_set_mask(d, selected_handle,
                NV_PAVS_VOICE_TAR_PITCH_LINK,
                NV_PAVS_VOICE_TAR_PITCH_LINK_NEXT_VOICE_HANDLE,
                d->regs[top_reg]);
            d->regs[top_reg] = selected_handle;
        } else {
            unsigned int antecedent_voice =
                GET_MASK(d->regs[NV_PAPU_FEAV], NV_PAPU_FEAV_VALUE);
            /* voice is added after the antecedent voice */
            assert(antecedent_voice != 0xFFFF);

            uint32_t next_handle = voice_get_mask(d, antecedent_voice,
                NV_PAVS_VOICE_TAR_PITCH_LINK,
                NV_PAVS_VOICE_TAR_PITCH_LINK_NEXT_VOICE_HANDLE);
            voice_set_mask(d, selected_handle,
                NV_PAVS_VOICE_TAR_PITCH_LINK,
                NV_PAVS_VOICE_TAR_PITCH_LINK_NEXT_VOICE_HANDLE,
                next_handle);
            voice_set_mask(d, antecedent_voice,
                NV_PAVS_VOICE_TAR_PITCH_LINK,
                NV_PAVS_VOICE_TAR_PITCH_LINK_NEXT_VOICE_HANDLE,
                selected_handle);
        }
        unsigned int ea_start = GET_MASK(argument, NV1BA0_PIO_VOICE_ON_ENVA);
        voice_set_mask(d, selected_handle,
                NV_PAVS_VOICE_PAR_STATE,
                NV_PAVS_VOICE_PAR_STATE_EACUR,
                ea_start);
        if (ea_start == 1) { // Delay
            uint16_t delay_time = voice_get_mask(d, selected_handle, NV_PAVS_VOICE_CFG_ENV0, NV_PAVS_VOICE_CFG_ENV0_EA_DELAYTIME);
            voice_set_mask(d, selected_handle, NV_PAVS_VOICE_CUR_ECNT, NV_PAVS_VOICE_CUR_ECNT_EACOUNT, delay_time * 16);
        } else if (ea_start == 2) { // Attack
            voice_set_mask(d, selected_handle, NV_PAVS_VOICE_CUR_ECNT, NV_PAVS_VOICE_CUR_ECNT_EACOUNT, 0x0000);
        } else if (ea_start == 3) { // Hold
            uint16_t hold_time = voice_get_mask(d, selected_handle, NV_PAVS_VOICE_CFG_ENVA, NV_PAVS_VOICE_CFG_ENVA_EA_HOLDTIME);
            voice_set_mask(d, selected_handle, NV_PAVS_VOICE_CUR_ECNT, NV_PAVS_VOICE_CUR_ECNT_EACOUNT, hold_time * 16);
        }
        //FIXME: Will count be overwritten in other cases too?

        unsigned int ef_start = GET_MASK(argument, NV1BA0_PIO_VOICE_ON_ENVF);
        voice_set_mask(d, selected_handle,
                NV_PAVS_VOICE_PAR_STATE,
                NV_PAVS_VOICE_PAR_STATE_EFCUR,
                ef_start);
        if (ef_start == 1) { // Delay
            uint16_t delay_time = voice_get_mask(d, selected_handle, NV_PAVS_VOICE_CFG_ENV1, NV_PAVS_VOICE_CFG_ENV0_EA_DELAYTIME);
            voice_set_mask(d, selected_handle, NV_PAVS_VOICE_CUR_ECNT, NV_PAVS_VOICE_CUR_ECNT_EFCOUNT, delay_time * 16);
        } else if (ef_start == 2) { // Attack
            voice_set_mask(d, selected_handle, NV_PAVS_VOICE_CUR_ECNT, NV_PAVS_VOICE_CUR_ECNT_EFCOUNT, 0x0000);
        } else if (ef_start == 3) { // Hold
            uint16_t hold_time = voice_get_mask(d, selected_handle, NV_PAVS_VOICE_CFG_ENVF, NV_PAVS_VOICE_CFG_ENVA_EA_HOLDTIME);
            voice_set_mask(d, selected_handle, NV_PAVS_VOICE_CUR_ECNT, NV_PAVS_VOICE_CUR_ECNT_EFCOUNT, hold_time * 16);
        }
        //FIXME: Will count be overwritten in other cases too?

        voice_set_mask(d, selected_handle,
                NV_PAVS_VOICE_PAR_STATE,
                NV_PAVS_VOICE_PAR_STATE_ACTIVE_VOICE,
                1);
        break;
    case NV1BA0_PIO_VOICE_OFF:
        voice_set_mask(d, argument & NV1BA0_PIO_VOICE_OFF_HANDLE,
                NV_PAVS_VOICE_PAR_STATE,
                NV_PAVS_VOICE_PAR_STATE_ACTIVE_VOICE,
                0);
        break;
    case NV1BA0_PIO_VOICE_PAUSE:
        voice_set_mask(d, argument & NV1BA0_PIO_VOICE_PAUSE_HANDLE,
                NV_PAVS_VOICE_PAR_STATE,
                NV_PAVS_VOICE_PAR_STATE_PAUSED,
                (argument & NV1BA0_PIO_VOICE_PAUSE_ACTION) != 0);
        break;
    case NV1BA0_PIO_SET_CURRENT_VOICE:
        d->regs[NV_PAPU_FECV] = argument;
        break;
    case NV1BA0_PIO_SET_VOICE_CFG_VBIN:
        voice_set_mask(d, d->regs[NV_PAPU_FECV],
                NV_PAVS_VOICE_CFG_VBIN,
                0xFFFFFFFF,
                argument);
        break;
    case NV1BA0_PIO_SET_VOICE_CFG_FMT:
        voice_set_mask(d, d->regs[NV_PAPU_FECV],
                NV_PAVS_VOICE_CFG_FMT,
                0xFFFFFFFF,
                argument);
        break;
    case NV1BA0_PIO_SET_VOICE_CFG_ENV0:
        voice_set_mask(d, d->regs[NV_PAPU_FECV],
                NV_PAVS_VOICE_CFG_ENV0,
                0xFFFFFFFF,
                argument);
        break;
    case NV1BA0_PIO_SET_VOICE_CFG_ENVA:
        voice_set_mask(d, d->regs[NV_PAPU_FECV],
                NV_PAVS_VOICE_CFG_ENVA,
                0xFFFFFFFF,
                argument);
        break;
    case NV1BA0_PIO_SET_VOICE_CFG_ENV1:
        voice_set_mask(d, d->regs[NV_PAPU_FECV],
                NV_PAVS_VOICE_CFG_ENV1,
                0xFFFFFFFF,
                argument);
        break;
    case NV1BA0_PIO_SET_VOICE_CFG_ENVF:
        voice_set_mask(d, d->regs[NV_PAPU_FECV],
                NV_PAVS_VOICE_CFG_ENVF,
                0xFFFFFFFF,
                argument);
        break;
    case NV1BA0_PIO_SET_VOICE_CFG_MISC:
        voice_set_mask(d, d->regs[NV_PAPU_FECV],
                NV_PAVS_VOICE_CFG_MISC,
                0xFFFFFFFF,
                argument);
        break;
    case NV1BA0_PIO_SET_VOICE_TAR_VOLA:
        voice_set_mask(d, d->regs[NV_PAPU_FECV],
                NV_PAVS_VOICE_TAR_VOLA,
                0xFFFFFFFF,
                argument);
        break;
    case NV1BA0_PIO_SET_VOICE_TAR_VOLB:
        voice_set_mask(d, d->regs[NV_PAPU_FECV],
                NV_PAVS_VOICE_TAR_VOLB,
                0xFFFFFFFF,
                argument);
        break;
    case NV1BA0_PIO_SET_VOICE_TAR_VOLC:
        voice_set_mask(d, d->regs[NV_PAPU_FECV],
                NV_PAVS_VOICE_TAR_VOLC,
                0xFFFFFFFF,
                argument);
        break;
    case NV1BA0_PIO_SET_VOICE_LFO_ENV:
        voice_set_mask(d, d->regs[NV_PAPU_FECV],
                NV_PAVS_VOICE_TAR_LFO_ENV,
                0xFFFFFFFF,
                argument);
        break;
    case NV1BA0_PIO_SET_VOICE_TAR_PITCH:
        voice_set_mask(d, d->regs[NV_PAPU_FECV],
                NV_PAVS_VOICE_TAR_PITCH_LINK,
                NV_PAVS_VOICE_TAR_PITCH_LINK_PITCH,
                (argument & NV1BA0_PIO_SET_VOICE_TAR_PITCH_STEP) >> 16);
        break;
    case NV1BA0_PIO_SET_VOICE_CFG_BUF_BASE:
        voice_set_mask(d, d->regs[NV_PAPU_FECV],
                NV_PAVS_VOICE_CUR_PSL_START,
                NV_PAVS_VOICE_CUR_PSL_START_BA,
                argument);
        break;
    case NV1BA0_PIO_SET_VOICE_CFG_BUF_LBO:
        voice_set_mask(d, d->regs[NV_PAPU_FECV],
                NV_PAVS_VOICE_CUR_PSH_SAMPLE,
                NV_PAVS_VOICE_CUR_PSH_SAMPLE_LBO,
                argument);
        break;
    case NV1BA0_PIO_SET_VOICE_BUF_CBO:
        voice_set_mask(d, d->regs[NV_PAPU_FECV],
                NV_PAVS_VOICE_PAR_OFFSET,
                NV_PAVS_VOICE_PAR_OFFSET_CBO,
                argument);
        break;
    case NV1BA0_PIO_SET_VOICE_CFG_BUF_EBO:
        voice_set_mask(d, d->regs[NV_PAPU_FECV],
                NV_PAVS_VOICE_PAR_NEXT,
                NV_PAVS_VOICE_PAR_NEXT_EBO,
                argument);
        break;
    case NV1BA0_PIO_SET_CURRENT_INBUF_SGE:
        d->inbuf_sge_handle = argument & NV1BA0_PIO_SET_CURRENT_INBUF_SGE_HANDLE;
        break;
    case NV1BA0_PIO_SET_CURRENT_INBUF_SGE_OFFSET: {
        //FIXME: Is there an upper limit for the SGE table size?
        //FIXME: NV_PAPU_VPSGEADDR is probably bad, as outbuf SGE use the same handle range (or that is also wrong)
        hwaddr sge_address = d->regs[NV_PAPU_VPSGEADDR] + d->inbuf_sge_handle * 8;
        stl_le_phys(&address_space_memory, sge_address, argument & NV1BA0_PIO_SET_CURRENT_INBUF_SGE_OFFSET_PARAMETER);
        MCPX_DPRINTF("Wrote inbuf SGE[0x%X] = 0x%08X\n", d->inbuf_sge_handle, argument & NV1BA0_PIO_SET_CURRENT_INBUF_SGE_OFFSET_PARAMETER);
        break;
    }
    CASE_4(NV1BA0_PIO_SET_OUTBUF_BA, 8): // 8 byte pitch, 4 entries
#ifdef MCPX_DEBUG
        slot = (method - NV1BA0_PIO_SET_OUTBUF_BA) / 8;
        //FIXME: Use NV1BA0_PIO_SET_OUTBUF_BA_ADDRESS = 0x007FFF00 ?
        MCPX_DPRINTF("outbuf_ba[%d]: 0x%08X\n", slot, argument);
#endif
        //assert(false); //FIXME: Enable assert! no idea what this reg does
        break;
    CASE_4(NV1BA0_PIO_SET_OUTBUF_LEN, 8): // 8 byte pitch, 4 entries
#ifdef MCPX_DEBUG
        slot = (method - NV1BA0_PIO_SET_OUTBUF_LEN) / 8;
        //FIXME: Use NV1BA0_PIO_SET_OUTBUF_LEN_VALUE = 0x007FFF00 ?
        MCPX_DPRINTF("outbuf_len[%d]: 0x%08X\n", slot, argument);
#endif
        //assert(false); //FIXME: Enable assert! no idea what this reg does
        break;
    case NV1BA0_PIO_SET_CURRENT_OUTBUF_SGE:
        d->outbuf_sge_handle = argument & NV1BA0_PIO_SET_CURRENT_OUTBUF_SGE_HANDLE;
        break;
    case NV1BA0_PIO_SET_CURRENT_OUTBUF_SGE_OFFSET: {
        //FIXME: Is there an upper limit for the SGE table size?
        //FIXME: NV_PAPU_VPSGEADDR is probably bad, as inbuf SGE use the same handle range (or that is also wrong)
        // NV_PAPU_EPFADDR   EP outbufs
        // NV_PAPU_GPFADDR   GP outbufs
        // But how does it know which outbuf is being written?!
        hwaddr sge_address = d->regs[NV_PAPU_VPSGEADDR] + d->outbuf_sge_handle * 8;
        stl_le_phys(&address_space_memory, sge_address, argument & NV1BA0_PIO_SET_CURRENT_OUTBUF_SGE_OFFSET_PARAMETER);
        MCPX_DPRINTF("Wrote outbuf SGE[0x%X] = 0x%08X\n", d->outbuf_sge_handle, argument & NV1BA0_PIO_SET_CURRENT_OUTBUF_SGE_OFFSET_PARAMETER);
        break;
    }
    case SE2FE_IDLE_VOICE:
        if (d->regs[NV_PAPU_FETFORCE1] & NV_PAPU_FETFORCE1_SE2FE_IDLE_VOICE) {

            d->regs[NV_PAPU_FECTL] &= ~NV_PAPU_FECTL_FEMETHMODE;
            d->regs[NV_PAPU_FECTL] |= NV_PAPU_FECTL_FEMETHMODE_TRAPPED;

            d->regs[NV_PAPU_FECTL] &= ~NV_PAPU_FECTL_FETRAPREASON;
            d->regs[NV_PAPU_FECTL] |= NV_PAPU_FECTL_FETRAPREASON_REQUESTED;

            d->regs[NV_PAPU_ISTS] |= NV_PAPU_ISTS_FETINTSTS;
            update_irq(d);
        } else {
            assert(false);
        }
        break;
    default:
        assert(false);
        break;
    }
}

static uint64_t vp_read(void *opaque,
                        hwaddr addr, unsigned int size)
{
    MCPX_DPRINTF("mcpx apu VP: read [0x%llx]\n", addr);
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

static void vp_write(void *opaque, hwaddr addr,
                     uint64_t val, unsigned int size)
{
    MCPXAPUState *d = opaque;

    MCPX_DPRINTF("mcpx apu VP: [0x%llx] = 0x%llx\n", addr, val);

    switch (addr) {
    case NV1BA0_PIO_SET_ANTECEDENT_VOICE:
    case NV1BA0_PIO_VOICE_ON:
    case NV1BA0_PIO_VOICE_OFF:
    case NV1BA0_PIO_VOICE_PAUSE:
    case NV1BA0_PIO_SET_CURRENT_VOICE:
    case NV1BA0_PIO_SET_VOICE_CFG_VBIN:
    case NV1BA0_PIO_SET_VOICE_CFG_FMT:
    case NV1BA0_PIO_SET_VOICE_CFG_ENV0:
    case NV1BA0_PIO_SET_VOICE_CFG_ENVA:
    case NV1BA0_PIO_SET_VOICE_CFG_ENV1:
    case NV1BA0_PIO_SET_VOICE_CFG_ENVF:
    case NV1BA0_PIO_SET_VOICE_CFG_MISC:
    case NV1BA0_PIO_SET_VOICE_TAR_VOLA:
    case NV1BA0_PIO_SET_VOICE_TAR_VOLB:
    case NV1BA0_PIO_SET_VOICE_TAR_VOLC:
    case NV1BA0_PIO_SET_VOICE_LFO_ENV:
    case NV1BA0_PIO_SET_VOICE_TAR_PITCH:
    case NV1BA0_PIO_SET_VOICE_CFG_BUF_BASE:
    case NV1BA0_PIO_SET_VOICE_CFG_BUF_LBO:
    case NV1BA0_PIO_SET_VOICE_BUF_CBO:
    case NV1BA0_PIO_SET_VOICE_CFG_BUF_EBO:
    case NV1BA0_PIO_SET_CURRENT_INBUF_SGE:
    case NV1BA0_PIO_SET_CURRENT_INBUF_SGE_OFFSET:
    CASE_4(NV1BA0_PIO_SET_OUTBUF_BA, 8): // 8 byte pitch, 4 entries
    CASE_4(NV1BA0_PIO_SET_OUTBUF_LEN, 8): // 8 byte pitch, 4 entries
    case NV1BA0_PIO_SET_CURRENT_OUTBUF_SGE:
    case NV1BA0_PIO_SET_CURRENT_OUTBUF_SGE_OFFSET:
        /* TODO: these should instead be queueing up fe commands */
        fe_method(d, addr, val);
        break;
    default:
        break;
    }
}

static const MemoryRegionOps vp_ops = {
    .read = vp_read,
    .write = vp_write,
};

static void scatter_gather_rw(MCPXAPUState *d,
                              hwaddr sge_base, unsigned int max_sge,
                              uint8_t *ptr, uint32_t addr, size_t len,
                              bool dir)
{
    unsigned int page_entry = addr / TARGET_PAGE_SIZE;
    unsigned int offset_in_page = addr % TARGET_PAGE_SIZE;
    unsigned int bytes_to_copy = TARGET_PAGE_SIZE - offset_in_page;

    while (len > 0) {
        assert(page_entry <= max_sge);

        uint32_t prd_address = ldl_le_phys(&address_space_memory,
                                           sge_base + page_entry * 8 + 0);
        /* uint32_t prd_control = ldl_le_phys(&address_space_memory,
                                            sge_base + page_entry * 8 + 4); */

        hwaddr paddr = prd_address + offset_in_page;

        if (bytes_to_copy > len) {
            bytes_to_copy = len;
        }

        assert(paddr + bytes_to_copy < memory_region_size(d->ram));

        if (dir) {
            memcpy(&d->ram_ptr[paddr], ptr, bytes_to_copy);
            memory_region_set_dirty(d->ram, paddr, bytes_to_copy);
        } else {
            memcpy(ptr, &d->ram_ptr[paddr], bytes_to_copy);
        }

        ptr += bytes_to_copy;
        len -= bytes_to_copy;

        /* After the first iteration, we are page aligned */
        page_entry += 1;
        bytes_to_copy = TARGET_PAGE_SIZE;
        offset_in_page = 0;
    }
}

static void gp_scratch_rw(void *opaque,
                          uint8_t *ptr,
                          uint32_t addr,
                          size_t len,
                          bool dir)
{
    MCPXAPUState *d = opaque;
    scatter_gather_rw(d, d->regs[NV_PAPU_GPSADDR], d->regs[NV_PAPU_GPSMAXSGE],
                      ptr, addr, len, dir);
}

static void ep_scratch_rw(void *opaque,
                          uint8_t *ptr,
                          uint32_t addr,
                          size_t len,
                          bool dir)
{
    MCPXAPUState *d = opaque;
    scatter_gather_rw(d, d->regs[NV_PAPU_EPSADDR], d->regs[NV_PAPU_EPSMAXSGE],
                      ptr, addr, len, dir);
}

static uint32_t circular_scatter_gather_rw(MCPXAPUState *d,
                                           hwaddr sge_base,
                                           unsigned int max_sge,
                                           uint8_t *ptr,
                                           uint32_t base, uint32_t end,
                                           uint32_t cur,
                                           size_t len,
                                           bool dir)
{
    while (len > 0) {
        unsigned int bytes_to_copy = end - cur;

        if (bytes_to_copy > len) {
            bytes_to_copy = len;
        }

        MCPX_DPRINTF("circular scatter gather %s in range 0x%x - 0x%x at 0x%x of length 0x%x / 0x%lx bytes\n",
            dir ? "write" : "read", base, end, cur, bytes_to_copy, len);

        assert((cur >= base) && ((cur + bytes_to_copy) <= end));
        scatter_gather_rw(d, sge_base, max_sge, ptr, cur, bytes_to_copy, dir);

        ptr += bytes_to_copy;
        len -= bytes_to_copy;

        /* After the first iteration we might have to wrap */
        cur += bytes_to_copy;
        if (cur >= end) {
            assert(cur == end);
            cur = base;
        }
    }

    return cur;
}

static void gp_fifo_rw(void *opaque, uint8_t *ptr,
                       unsigned int index, size_t len,
                       bool dir)
{
    MCPXAPUState *d = opaque;
    uint32_t base;
    uint32_t end;
    hwaddr cur_reg;
    if (dir) {
        assert(index < GP_OUTPUT_FIFO_COUNT);
        base = GET_MASK(d->regs[NV_PAPU_GPOFBASE0 + 0x10 * index],
                        NV_PAPU_GPOFBASE0_VALUE);
        end = GET_MASK(d->regs[NV_PAPU_GPOFEND0 + 0x10 * index],
                       NV_PAPU_GPOFEND0_VALUE);
        cur_reg = NV_PAPU_GPOFCUR0 + 0x10 * index;
    } else {
        assert(index < GP_INPUT_FIFO_COUNT);
        base = GET_MASK(d->regs[NV_PAPU_GPIFBASE0 + 0x10 * index],
                        NV_PAPU_GPOFBASE0_VALUE);
        end = GET_MASK(d->regs[NV_PAPU_GPIFEND0 + 0x10 * index],
                       NV_PAPU_GPOFEND0_VALUE);
        cur_reg = NV_PAPU_GPIFCUR0 + 0x10 * index;
    }

    uint32_t cur = GET_MASK(d->regs[cur_reg], NV_PAPU_GPOFCUR0_VALUE);

    /* DSP hangs if current >= end; but forces current >= base */
    assert(cur < end);
    if (cur < base) {
        cur = base;
    }

    cur = circular_scatter_gather_rw(d,
        d->regs[NV_PAPU_GPFADDR], d->regs[NV_PAPU_GPFMAXSGE],
        ptr, base, end, cur, len, dir);

    SET_MASK(d->regs[cur_reg], NV_PAPU_GPOFCUR0_VALUE, cur);
}

static void ep_fifo_rw(void *opaque, uint8_t *ptr,
                       unsigned int index, size_t len,
                       bool dir)
{
    MCPXAPUState *d = opaque;
    uint32_t base;
    uint32_t end;
    hwaddr cur_reg;
    if (dir) {
        assert(index < EP_OUTPUT_FIFO_COUNT);
        base = GET_MASK(d->regs[NV_PAPU_EPOFBASE0 + 0x10 * index],
                        NV_PAPU_GPOFBASE0_VALUE);
        end = GET_MASK(d->regs[NV_PAPU_EPOFEND0 + 0x10 * index],
                       NV_PAPU_GPOFEND0_VALUE);
        cur_reg = NV_PAPU_EPOFCUR0 + 0x10 * index;
    } else {
        assert(index < EP_INPUT_FIFO_COUNT);
        base = GET_MASK(d->regs[NV_PAPU_EPIFBASE0 + 0x10 * index],
                        NV_PAPU_GPOFBASE0_VALUE);
        end = GET_MASK(d->regs[NV_PAPU_EPIFEND0 + 0x10 * index],
                       NV_PAPU_GPOFEND0_VALUE);
        cur_reg = NV_PAPU_EPIFCUR0 + 0x10 * index;
    }

    uint32_t cur = GET_MASK(d->regs[cur_reg], NV_PAPU_GPOFCUR0_VALUE);

    /* DSP hangs if current >= end; but forces current >= base */
    assert(cur < end);
    if (cur < base) {
        cur = base;
    }

    cur = circular_scatter_gather_rw(d,
        d->regs[NV_PAPU_EPFADDR], d->regs[NV_PAPU_EPFMAXSGE],
        ptr, base, end, cur, len, dir);

    SET_MASK(d->regs[cur_reg], NV_PAPU_GPOFCUR0_VALUE, cur);
}

static void proc_rst_write(DSPState *dsp, uint32_t oldval, uint32_t val)
{
    if (!(val & NV_PAPU_GPRST_GPRST) || !(val & NV_PAPU_GPRST_GPDSPRST)) {
        dsp_reset(dsp);
    } else if (
        (!(oldval & NV_PAPU_GPRST_GPRST) || !(oldval & NV_PAPU_GPRST_GPDSPRST))
        && ((val & NV_PAPU_GPRST_GPRST) && (val & NV_PAPU_GPRST_GPDSPRST))) {
        dsp_bootstrap(dsp);
    }
}

/* Global Processor - programmable DSP */
static uint64_t gp_read(void *opaque,
                        hwaddr addr,
                        unsigned int size)
{
    MCPXAPUState *d = opaque;

    assert(size == 4);
    assert(addr % 4 == 0);

    uint64_t r = 0;
    switch (addr) {
    case NV_PAPU_GPXMEM ... NV_PAPU_GPXMEM + 0x1000 * 4 - 1: {
        uint32_t xaddr = (addr - NV_PAPU_GPXMEM) / 4;
        r = dsp_read_memory(d->gp.dsp, 'X', xaddr);
        break;
    }
    case NV_PAPU_GPMIXBUF ... NV_PAPU_GPMIXBUF + 0x400 * 4 - 1: {
        uint32_t xaddr = (addr - NV_PAPU_GPMIXBUF) / 4;
        r = dsp_read_memory(d->gp.dsp, 'X', GP_DSP_MIXBUF_BASE + xaddr);
        break;
    }
    case NV_PAPU_GPYMEM ... NV_PAPU_GPYMEM + 0x800 * 4 - 1: {
        uint32_t yaddr = (addr - NV_PAPU_GPYMEM) / 4;
        r = dsp_read_memory(d->gp.dsp, 'Y', yaddr);
        break;
    }
    case NV_PAPU_GPPMEM ... NV_PAPU_GPPMEM + 0x1000 * 4 - 1: {
        uint32_t paddr = (addr - NV_PAPU_GPPMEM) / 4;
        r = dsp_read_memory(d->gp.dsp, 'P', paddr);
        break;
    }
    default:
        r = d->gp.regs[addr];
        break;
    }
    MCPX_DPRINTF("mcpx apu GP: read [0x%llx] -> 0x%llx\n", addr, r);
    return r;
}

static void gp_write(void *opaque, hwaddr addr,
                     uint64_t val, unsigned int size)
{
    MCPXAPUState *d = opaque;

    assert(size == 4);
    assert(addr % 4 == 0);

    MCPX_DPRINTF("mcpx apu GP: [0x%llx] = 0x%llx\n", addr, val);

    switch (addr) {
    case NV_PAPU_GPXMEM ... NV_PAPU_GPXMEM + 0x1000 * 4 - 1: {
        uint32_t xaddr = (addr - NV_PAPU_GPXMEM) / 4;
        dsp_write_memory(d->gp.dsp, 'X', xaddr, val);
        break;
    }
    case NV_PAPU_GPMIXBUF ... NV_PAPU_GPMIXBUF + 0x400 * 4 - 1: {
        uint32_t xaddr = (addr - NV_PAPU_GPMIXBUF) / 4;
        dsp_write_memory(d->gp.dsp, 'X', GP_DSP_MIXBUF_BASE + xaddr, val);
        break;
    }
    case NV_PAPU_GPYMEM ... NV_PAPU_GPYMEM + 0x800 * 4 - 1: {
        uint32_t yaddr = (addr - NV_PAPU_GPYMEM) / 4;
        dsp_write_memory(d->gp.dsp, 'Y', yaddr, val);
        break;
    }
    case NV_PAPU_GPPMEM ... NV_PAPU_GPPMEM + 0x1000 * 4 - 1: {
        uint32_t paddr = (addr - NV_PAPU_GPPMEM) / 4;
        dsp_write_memory(d->gp.dsp, 'P', paddr, val);
        break;
    }
    case NV_PAPU_GPRST:
        proc_rst_write(d->gp.dsp, d->gp.regs[NV_PAPU_GPRST], val);
        d->gp.regs[NV_PAPU_GPRST] = val;
        break;
    default:
        d->gp.regs[addr] = val;
        break;
    }
}

static const MemoryRegionOps gp_ops = {
    .read = gp_read,
    .write = gp_write,
};

/* Encode Processor - encoding DSP */
static uint64_t ep_read(void *opaque,
                        hwaddr addr,
                        unsigned int size)
{
    MCPXAPUState *d = opaque;

    assert(size == 4);
    assert(addr % 4 == 0);

    uint64_t r = 0;
    switch (addr) {
    case NV_PAPU_EPXMEM ... NV_PAPU_EPXMEM + 0xC00 * 4 - 1: {
        uint32_t xaddr = (addr - NV_PAPU_EPXMEM) / 4;
        r = dsp_read_memory(d->ep.dsp, 'X', xaddr);
        break;
    }
    case NV_PAPU_EPYMEM ... NV_PAPU_EPYMEM + 0x100 * 4 - 1: {
        uint32_t yaddr = (addr - NV_PAPU_EPYMEM) / 4;
        r = dsp_read_memory(d->ep.dsp, 'Y', yaddr);
        break;
    }
    case NV_PAPU_EPPMEM ... NV_PAPU_EPPMEM + 0x1000 * 4 - 1: {
        uint32_t paddr = (addr - NV_PAPU_EPPMEM) / 4;
        r = dsp_read_memory(d->ep.dsp, 'P', paddr);
        break;
    }
    default:
        r = d->ep.regs[addr];
        break;
    }
    MCPX_DPRINTF("mcpx apu EP: read [0x%llx] -> 0x%llx\n", addr, r);
    return r;
}

static void ep_write(void *opaque, hwaddr addr,
                     uint64_t val, unsigned int size)
{
    MCPXAPUState *d = opaque;

    assert(size == 4);
    assert(addr % 4 == 0);

    MCPX_DPRINTF("mcpx apu EP: [0x%llx] = 0x%llx\n", addr, val);

    switch (addr) {
    case NV_PAPU_EPXMEM ... NV_PAPU_EPXMEM + 0xC00 * 4 - 1: {
        uint32_t xaddr = (addr - NV_PAPU_EPXMEM) / 4;
        dsp_write_memory(d->ep.dsp, 'X', xaddr, val);
        break;
    }
    case NV_PAPU_EPYMEM ... NV_PAPU_EPYMEM + 0x100 * 4 - 1: {
        uint32_t yaddr = (addr - NV_PAPU_EPYMEM) / 4;
        dsp_write_memory(d->ep.dsp, 'Y', yaddr, val);
        break;
    }
    case NV_PAPU_EPPMEM ... NV_PAPU_EPPMEM + 0x1000 * 4 - 1: {
        uint32_t paddr = (addr - NV_PAPU_EPPMEM) / 4;
        dsp_write_memory(d->ep.dsp, 'P', paddr, val);
        break;
    }
    case NV_PAPU_EPRST:
        proc_rst_write(d->ep.dsp, d->ep.regs[NV_PAPU_EPRST], val);
        d->ep.regs[NV_PAPU_EPRST] = val;
        break;
    default:
        d->ep.regs[addr] = val;
        break;
    }
}

static const MemoryRegionOps ep_ops = {
    .read = ep_read,
    .write = ep_write,
};

#if 0
#include "adpcm_block.h"

static hwaddr get_data_ptr(hwaddr sge_base, unsigned int max_sge, uint32_t addr) {
    unsigned int entry = addr / TARGET_PAGE_SIZE;
    assert(entry <= max_sge);
    uint32_t prd_address = ldl_le_phys(&address_space_memory, sge_base + entry*4*2);
    // uint32_t prd_control = ldl_le_phys(&address_space_memory, sge_base + entry*4*2 + 4);
    MCPX_DPRINTF("Addr: 0x%08X, control: 0x%08X\n", prd_address, prd_control);

    return prd_address + addr % TARGET_PAGE_SIZE;
}

static float step_envelope(MCPXAPUState *d, unsigned int v, uint32_t reg_0, uint32_t reg_a, uint32_t rr_reg, uint32_t rr_mask, uint32_t lvl_reg, uint32_t lvl_mask, uint32_t count_mask, uint32_t cur_mask) {
    uint8_t cur = voice_get_mask(d, v, NV_PAVS_VOICE_PAR_STATE, cur_mask);
    switch(cur) {
    case 0: // Off
        voice_set_mask(d, v, NV_PAVS_VOICE_CUR_ECNT, count_mask, 0);
        voice_set_mask(d, v, lvl_reg, lvl_mask, 0xFF);
        return 1.0f;
    case 1: { // Delay
        uint16_t count = voice_get_mask(d, v, NV_PAVS_VOICE_CUR_ECNT, count_mask);
        voice_set_mask(d, v, lvl_reg, lvl_mask, 0x00); // FIXME: Confirm this?
        if (count == 0) {
                cur++;
                voice_set_mask(d, v, NV_PAVS_VOICE_PAR_STATE, cur_mask, cur);
                count = 0;
        } else {
                count--;
        }
        voice_set_mask(d, v, NV_PAVS_VOICE_CUR_ECNT, count_mask, count);
        break;
    }
    case 2: { // Attack
        uint16_t count = voice_get_mask(d, v, NV_PAVS_VOICE_CUR_ECNT, count_mask);
        uint16_t attack_rate = voice_get_mask(d, v, reg_0, NV_PAVS_VOICE_CFG_ENV0_EA_ATTACKRATE);
        float value;
        if (attack_rate == 0) {
                //FIXME: [division by zero]
                //       Got crackling sound in hardware for amplitude env.
                value = 255.0f;
        } else {
                if (count <= attack_rate) {
                        value = (count * 0xFF) / attack_rate;
                } else {
                        //FIXME: Overflow in hardware
                        //       The actual value seems to overflow, but not sure how
                        value = 255.0f;
                }
        }
        voice_set_mask(d, v, lvl_reg, lvl_mask, value);
        //FIXME: Comparison could also be the other way around?! Test please.
        if (count == (attack_rate * 16)) {
                cur++;
                voice_set_mask(d, v, NV_PAVS_VOICE_PAR_STATE, cur_mask, cur);
                uint16_t hold_time = voice_get_mask(d, v, reg_a, NV_PAVS_VOICE_CFG_ENVA_EA_HOLDTIME);
                count = hold_time * 16; //FIXME: Skip next phase if count is 0? [other instances too]
        } else {
                count++;
        }
        voice_set_mask(d, v, NV_PAVS_VOICE_CUR_ECNT, count_mask, count);
        return value / 255.0f;
    }
    case 3: { // Hold
        uint16_t count = voice_get_mask(d, v, NV_PAVS_VOICE_CUR_ECNT, count_mask);
        voice_set_mask(d, v, lvl_reg, lvl_mask, 0xFF);
        if (count == 0) {
                cur++;
                voice_set_mask(d, v, NV_PAVS_VOICE_PAR_STATE, cur_mask, cur);
                uint16_t decay_rate = voice_get_mask(d, v, reg_a, NV_PAVS_VOICE_CFG_ENVA_EA_DECAYRATE);
                count = decay_rate * 16;
        } else {
                count--;
        }
        voice_set_mask(d, v, NV_PAVS_VOICE_CUR_ECNT, count_mask, count);
        return 1.0f;
    }
    case 4: { // Decay
        uint16_t count = voice_get_mask(d, v, NV_PAVS_VOICE_CUR_ECNT, count_mask);
        uint16_t decay_rate = voice_get_mask(d, v, reg_a, NV_PAVS_VOICE_CFG_ENVA_EA_DECAYRATE);
        uint8_t sustain_level = voice_get_mask(d, v, reg_a, NV_PAVS_VOICE_CFG_ENVA_EA_SUSTAINLEVEL);
        float value;
        if (decay_rate == 0) {
                //FIXME: [division by zero]
                //       Not tested on hardware
                value = 0.0f;
        } else {
            //FIXME: This formula and threshold is not accurate, but I can't get it any better for now
            value = 255.0f * powf(0.99988799f, (decay_rate * 16 - count) * 4096 / decay_rate);
        }
        if (value <= (sustain_level + 0.2f) || (value > 255.0f)) {
                //FIXME: Should we still update lvl?
                cur++;
                voice_set_mask(d, v, NV_PAVS_VOICE_PAR_STATE, cur_mask, cur);
        } else {
                count--;
                voice_set_mask(d, v, NV_PAVS_VOICE_CUR_ECNT, count_mask, count);
                voice_set_mask(d, v, lvl_reg, lvl_mask, value);
        }
        return value / 255.0f;
    }
    case 5: { // Sustain
        uint8_t sustain_level = voice_get_mask(d, v, reg_a, NV_PAVS_VOICE_CFG_ENVA_EA_SUSTAINLEVEL);
        voice_set_mask(d, v, NV_PAVS_VOICE_CUR_ECNT, count_mask, 0x00); // FIXME: is this only set to 0 once or forced to zero?
        voice_set_mask(d, v, lvl_reg, lvl_mask, sustain_level);
        return sustain_level / 255.0f;
    }
    case 6: { // Release
        uint16_t release_rate = voice_get_mask(d, v, rr_reg, rr_mask);
        uint16_t count = voice_get_mask(d, v, NV_PAVS_VOICE_CUR_ECNT, count_mask);
        count--;
        voice_set_mask(d, v, NV_PAVS_VOICE_CUR_ECNT, count_mask, count);
        uint8_t lvl = voice_get_mask(d, v, lvl_reg, lvl_mask);
        float value = count * lvl / (release_rate * 16);
        if (count == 0) {
            //FIXME: What to do now?!
#if 0 // Hack so we don't assert
            cur++; // FIXME: Does this happen?!
            voice_set_mask(d, v, NV_PAVS_VOICE_PAR_STATE, cur_mask, cur);
#else
            voice_set_mask(d, v, NV_PAVS_VOICE_PAR_STATE, cur_mask, 0x0); // Is this correct? FIXME: Turn off voice?
#endif
        }
        return value / 255.0f;
    }
    case 7: // Force release
        assert(false); //FIXME: This mode is not understood yet
        return 1.0f;
    default:
        fprintf(stderr, "Unknown envelope state 0x%x\n", cur);
        assert(false);
    }

    return 0;
}
#endif

static void process_voice(MCPXAPUState *d,
                          int32_t mixbins[NUM_MIXBINS][NUM_SAMPLES_PER_FRAME],
                          uint32_t voice)
{
#if 0
    uint32_t v = voice;
    int32_t samples[2][0x20] = {0};

    float ea_value = step_envelope(d, v, NV_PAVS_VOICE_CFG_ENV0, NV_PAVS_VOICE_CFG_ENVA, NV_PAVS_VOICE_TAR_LFO_ENV, NV_PAVS_VOICE_TAR_LFO_ENV_EA_RELEASERATE, NV_PAVS_VOICE_PAR_OFFSET, NV_PAVS_VOICE_PAR_OFFSET_EALVL, NV_PAVS_VOICE_CUR_ECNT_EACOUNT, NV_PAVS_VOICE_PAR_STATE_EACUR);
    float ef_value = step_envelope(d, v, NV_PAVS_VOICE_CFG_ENV1, NV_PAVS_VOICE_CFG_ENVF, NV_PAVS_VOICE_CFG_MISC, NV_PAVS_VOICE_CFG_MISC_EF_RELEASERATE, NV_PAVS_VOICE_PAR_NEXT, NV_PAVS_VOICE_PAR_NEXT_EFLVL, NV_PAVS_VOICE_CUR_ECNT_EFCOUNT, NV_PAVS_VOICE_PAR_STATE_EFCUR);
    assert(ea_value >= 0.0f);
    assert(ea_value <= 1.0f);
    assert(ef_value >= 0.0f);
    assert(ef_value <= 1.0f);

    int16_t p = voice_get_mask(d, v, NV_PAVS_VOICE_TAR_PITCH_LINK, NV_PAVS_VOICE_TAR_PITCH_LINK_PITCH);
    int8_t pm = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_ENV0, NV_PAVS_VOICE_CFG_ENV0_EF_PITCHSCALE);
    float rate = powf(2.0f, (p + pm * 32 * ef_value) / 4096.0f);
    MCPX_DPRINTF("Got %f\n", rate * 48000.0f);

    float overdrive = 1.0f; //FIXME: This is just a hack because our APU runs too rarely

    //NV_PAVS_VOICE_PAR_OFFSET_CBO
    bool stereo = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT, NV_PAVS_VOICE_CFG_FMT_STEREO);
    unsigned int channels = stereo ? 2 : 1;
    unsigned int sample_size = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT, NV_PAVS_VOICE_CFG_FMT_SAMPLE_SIZE);

    // B8, B16, ADPCM, B32
    unsigned int container_sizes[4] = { 1, 2, 0, 4 };
    unsigned int container_size_index = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT, NV_PAVS_VOICE_CFG_FMT_CONTAINER_SIZE);
    //FIXME: Move down, but currently debug code depends on this
    unsigned int container_size = container_sizes[container_size_index];

    bool stream = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT, NV_PAVS_VOICE_CFG_FMT_DATA_TYPE);
    assert(!stream);
    bool paused = voice_get_mask(d, v, NV_PAVS_VOICE_PAR_STATE, NV_PAVS_VOICE_PAR_STATE_PAUSED);
    bool loop = voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT, NV_PAVS_VOICE_CFG_FMT_LOOP);
    uint32_t ebo = voice_get_mask(d, v, NV_PAVS_VOICE_PAR_NEXT, NV_PAVS_VOICE_PAR_NEXT_EBO);
    uint32_t cbo = voice_get_mask(d, v, NV_PAVS_VOICE_PAR_OFFSET, NV_PAVS_VOICE_PAR_OFFSET_CBO);
    uint32_t lbo = voice_get_mask(d, v, NV_PAVS_VOICE_CUR_PSH_SAMPLE, NV_PAVS_VOICE_CUR_PSH_SAMPLE_LBO);
    uint32_t ba = voice_get_mask(d, v, NV_PAVS_VOICE_CUR_PSL_START, NV_PAVS_VOICE_CUR_PSL_START_BA);
    unsigned int samples_per_block = 1 + voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT, NV_PAVS_VOICE_CFG_FMT_SAMPLES_PER_BLOCK);

    // This is probably cleared when the first sample is played
    //FIXME: How will this behave if CBO > EBO on first play?
    //FIXME: How will this behave if paused?
    voice_set_mask(d, v, NV_PAVS_VOICE_PAR_STATE, NV_PAVS_VOICE_PAR_STATE_NEW_VOICE, 0);

    for(unsigned int i = 0; i < 0x20; i++) {

        uint32_t sample_pos = (uint32_t)(i * rate * overdrive);

        if ((cbo + sample_pos) > ebo) {
          if (!loop) {
            // Set to safe state
            cbo = ebo; //FIXME: Will the hw do this?
            //FIXME: Not sure if this happens.. needs a hwtest.
            // Some RE also suggests that the voices will automaticly be removed from the list (!!!)
            voice_set_mask(d, v, NV_PAVS_VOICE_PAR_STATE, NV_PAVS_VOICE_PAR_STATE_ACTIVE_VOICE, 0);
            break;
          }
          // Now go to loop start
          //FIXME: Make sure this logic still works for very high sample_pos greater than ebo
          cbo += sample_pos;
          cbo %= ebo + 1;
          cbo += lbo;
          cbo -= sample_pos;
        }

        sample_pos += cbo;
        assert(sample_pos <= ebo);

        if (container_size_index == NV_PAVS_VOICE_CFG_FMT_CONTAINER_SIZE_ADPCM) {
            //FIXME: Not sure how this behaves otherwise
            assert(sample_size == NV_PAVS_VOICE_CFG_FMT_SAMPLE_SIZE_S24);

            unsigned int block_index = sample_pos / 65;
            unsigned int block_position = sample_pos % 65;

            MCPX_DPRINTF("ADPCM: %d + %d\n", block_index, block_position);

            //FIXME: Remove this from the loop which collects required samples
            //       We can always just grab one or two blocks to get all required samples
            if (stereo) {
                assert(samples_per_block == 2);
                // There are 65 samples per 72 byte block
                uint32_t block[72/4];
                uint32_t linear_addr = ba + block_index * 72;
                // FIXME: Only load block data which will be used
                for(unsigned int word_index = 0; word_index < 18; word_index++) {
                    hwaddr addr = get_data_ptr(d->regs[NV_PAPU_VPSGEADDR], 0xFFFFFFFF, linear_addr);
                    block[word_index] = ldl_le_phys(&address_space_memory, addr);
                    linear_addr += 4;
                }
                //FIXME: Used wrong sample format in my own decoder.. stupid me lol
                int16_t tmp[2];
                adpcm_decode_stereo_block(&tmp[0], &tmp[1], (uint8_t*)block, block_position, block_position);
                samples[0][i] = tmp[0];
                samples[1][i] = tmp[1];
            } else {
                assert(samples_per_block == 1);
                // There are 65 samples per 36 byte block
                uint32_t block[36/4];
                uint32_t linear_addr = ba + block_index * 36;
                for(unsigned int word_index = 0; word_index < 9; word_index++) {
                    hwaddr addr = get_data_ptr(d->regs[NV_PAPU_VPSGEADDR], 0xFFFFFFFF, linear_addr);
                    block[word_index] = ldl_le_phys(&address_space_memory, addr);
                    linear_addr += 4;
                }
                //FIXME: Used wrong sample format in my own decoder.. stupid me lol
                int16_t tmp;
                adpcm_decode_mono_block(&tmp, (uint8_t*)block, block_position, block_position);
                samples[0][i] = tmp;
            }

        } else {
            unsigned int block_size = container_size;
            block_size *= samples_per_block;

            uint32_t linear_addr = ba + sample_pos * block_size;
            hwaddr addr = get_data_ptr(d->regs[NV_PAPU_VPSGEADDR], 0xFFFFFFFF, linear_addr);
            //FIXME: Handle reading accross pages?!

            MCPX_DPRINTF("Sampling from 0x%08X\n", addr);

            // Get samples for this voice
            for(unsigned int channel = 0; channel < channels; channel++) {
                switch(sample_size) {
                case NV_PAVS_VOICE_CFG_FMT_SAMPLE_SIZE_U8:
                    samples[channel][i] = ldub_phys(&address_space_memory, addr);
                    break;
                case NV_PAVS_VOICE_CFG_FMT_SAMPLE_SIZE_S16:
                    samples[channel][i] = (int16_t)lduw_le_phys(&address_space_memory, addr);
                    break;
                case NV_PAVS_VOICE_CFG_FMT_SAMPLE_SIZE_S24:
                    samples[channel][i] = (int32_t)(ldl_le_phys(&address_space_memory, addr) << 8) >> 8;
                    break;
                case NV_PAVS_VOICE_CFG_FMT_SAMPLE_SIZE_S32:
                    samples[channel][i] = (int32_t)ldl_le_phys(&address_space_memory, addr);
                    break;
                }
             
                // Advance cursor to second channel for stereo
                addr += container_size;
            }
        }
    }

    //FIXME: Decode voice volume and bins
    int bin[8] = {
      voice_get_mask(d, v, NV_PAVS_VOICE_CFG_VBIN, NV_PAVS_VOICE_CFG_VBIN_V0BIN),
      voice_get_mask(d, v, NV_PAVS_VOICE_CFG_VBIN, NV_PAVS_VOICE_CFG_VBIN_V1BIN),
      voice_get_mask(d, v, NV_PAVS_VOICE_CFG_VBIN, NV_PAVS_VOICE_CFG_VBIN_V2BIN),
      voice_get_mask(d, v, NV_PAVS_VOICE_CFG_VBIN, NV_PAVS_VOICE_CFG_VBIN_V3BIN),
      voice_get_mask(d, v, NV_PAVS_VOICE_CFG_VBIN, NV_PAVS_VOICE_CFG_VBIN_V4BIN),
      voice_get_mask(d, v, NV_PAVS_VOICE_CFG_VBIN, NV_PAVS_VOICE_CFG_VBIN_V5BIN),
      voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT, NV_PAVS_VOICE_CFG_FMT_V6BIN),
      voice_get_mask(d, v, NV_PAVS_VOICE_CFG_FMT, NV_PAVS_VOICE_CFG_FMT_V7BIN)
    };
    uint16_t vol[8] = {
      voice_get_mask(d, v, NV_PAVS_VOICE_TAR_VOLA, NV_PAVS_VOICE_TAR_VOLA_VOLUME0),
      voice_get_mask(d, v, NV_PAVS_VOICE_TAR_VOLA, NV_PAVS_VOICE_TAR_VOLA_VOLUME1),
      voice_get_mask(d, v, NV_PAVS_VOICE_TAR_VOLB, NV_PAVS_VOICE_TAR_VOLB_VOLUME2),
      voice_get_mask(d, v, NV_PAVS_VOICE_TAR_VOLB, NV_PAVS_VOICE_TAR_VOLB_VOLUME3),
      voice_get_mask(d, v, NV_PAVS_VOICE_TAR_VOLC, NV_PAVS_VOICE_TAR_VOLC_VOLUME4),
      voice_get_mask(d, v, NV_PAVS_VOICE_TAR_VOLC, NV_PAVS_VOICE_TAR_VOLC_VOLUME5),
      (voice_get_mask(d, v, NV_PAVS_VOICE_TAR_VOLC, NV_PAVS_VOICE_TAR_VOLC_VOLUME6_B11_8) << 8) |
      (voice_get_mask(d, v, NV_PAVS_VOICE_TAR_VOLB, NV_PAVS_VOICE_TAR_VOLB_VOLUME6_B7_4) << 4) |
      voice_get_mask(d, v, NV_PAVS_VOICE_TAR_VOLA, NV_PAVS_VOICE_TAR_VOLA_VOLUME6_B3_0),
      (voice_get_mask(d, v, NV_PAVS_VOICE_TAR_VOLC, NV_PAVS_VOICE_TAR_VOLC_VOLUME7_B11_8) << 8) |
      (voice_get_mask(d, v, NV_PAVS_VOICE_TAR_VOLB, NV_PAVS_VOICE_TAR_VOLB_VOLUME7_B7_4) << 4) |
      voice_get_mask(d, v, NV_PAVS_VOICE_TAR_VOLA, NV_PAVS_VOICE_TAR_VOLA_VOLUME7_B3_0),
    };

    if (paused) {
      assert(sizeof(samples) == 0x20 * 4 * 2);
      memset(samples, 0x00, sizeof(samples));
    } else {
      cbo += 0x20 * rate * overdrive;
      voice_set_mask(d, v, NV_PAVS_VOICE_PAR_OFFSET, NV_PAVS_VOICE_PAR_OFFSET_CBO, cbo);
    }

    // Apply the amplitude envelope
    //FIXME: Figure out when exactly and how exactly this is actually done
    for(unsigned int j = 0; j < channels; j++) {
        for(unsigned int i = 0; i < 0x20; i++) {
            samples[j][i] *= ea_value;
        }
    }

    //FIXME: If phase negations means to flip the signal upside down
    //       we should modify volume of bin6 and bin7 here.

    // Mix samples into voice bins
    for(unsigned int j = 0; j < 8; j++) {
        MCPX_DPRINTF("Adding voice 0x%04X to bin %d [Rate %.2f, Volume 0x%03X] sample %d at %d [%.2fs]\n", v, bin[j], rate, vol[j], samples[0], cbo, cbo / (rate * 48000.0f));
        for(unsigned int i = 0; i < 0x20; i++) {
            //FIXME: how is the volume added?
            //FIXME: What happens to the other channel? Is this behaviour correct?
            mixbins[bin[j]][i] += (0xFFF - vol[j]) * samples[j % channels][i] / 0xFFF;
        }
    }
#endif
}

/* This routine must run at 1500 Hz */
/* TODO: this should be on a thread so it waits on the voice lock */
static void se_frame(void *opaque)
{
    MCPXAPUState *d = opaque;
    int mixbin;
    int sample;

    timer_mod(d->se.frame_timer, qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL) + 10);
    MCPX_DPRINTF("mcpx frame ping\n");

    /* Buffer for all mixbins for this frame */
    int32_t mixbins[NUM_MIXBINS][NUM_SAMPLES_PER_FRAME] = { 0 };

    /* Process all voices, mixing each into the affected MIXBINs */
    int list;
    for (list = 0; list < 3; list++) {
        hwaddr top, current, next;
        top = voice_list_regs[list].top;
        current = voice_list_regs[list].current;
        next = voice_list_regs[list].next;

        d->regs[current] = d->regs[top];
        MCPX_DPRINTF("list %d current voice %d\n", list, d->regs[current]);
        while (d->regs[current] != 0xFFFF) {
            d->regs[next] = voice_get_mask(d, d->regs[current],
                NV_PAVS_VOICE_TAR_PITCH_LINK,
                NV_PAVS_VOICE_TAR_PITCH_LINK_NEXT_VOICE_HANDLE);
            if (!voice_get_mask(d, d->regs[current],
                    NV_PAVS_VOICE_PAR_STATE,
                    NV_PAVS_VOICE_PAR_STATE_ACTIVE_VOICE)) {
                MCPX_DPRINTF("voice %d not active...!\n", d->regs[current]);
                fe_method(d, SE2FE_IDLE_VOICE, d->regs[current]);
            } else {
                process_voice(d, mixbins, d->regs[current]);
            }
            MCPX_DPRINTF("next voice %d\n", d->regs[next]);
            d->regs[current] = d->regs[next];
        }
    }

#if GENERATE_MIXBIN_BEEP
    /* Inject some audio to the mixbin for debugging.
     * Signal is 1500 Hz sine wave, phase shifted by mixbin number. */
    for (mixbin = 0; mixbin < NUM_MIXBINS; mixbin++) {
        for (sample = 0; sample < NUM_SAMPLES_PER_FRAME; sample++) {
            /* Avoid multiple of 1.0 / NUM_SAMPLES_PER_FRAME for phase shift,
             * or waves cancel out */
            float offset = sample / (float)NUM_SAMPLES_PER_FRAME -
                           mixbin / (float)(NUM_SAMPLES_PER_FRAME + 1);
            float wave = sinf(offset * M_PI * 2.0f);
            mixbins[mixbin][sample] += wave * 0x3FFFFF;
        }
    }
#endif

    /* Write VP results to the GP DSP MIXBUF */
    for (mixbin = 0; mixbin < NUM_MIXBINS; mixbin++) {
        for (sample = 0; sample < NUM_SAMPLES_PER_FRAME; sample++) {
            dsp_write_memory(d->gp.dsp,
                             'X', GP_DSP_MIXBUF_BASE + mixbin * 0x20 + sample,
                             mixbins[mixbin][sample] & 0xFFFFFF);
        }
    }

    /* Kickoff DSP processing */
    if ((d->gp.regs[NV_PAPU_GPRST] & NV_PAPU_GPRST_GPRST)
        && (d->gp.regs[NV_PAPU_GPRST] & NV_PAPU_GPRST_GPDSPRST)) {
        dsp_start_frame(d->gp.dsp);

        // hax
        dsp_run(d->gp.dsp, 1000);
    }
    if ((d->ep.regs[NV_PAPU_EPRST] & NV_PAPU_GPRST_GPRST)
        && (d->ep.regs[NV_PAPU_EPRST] & NV_PAPU_GPRST_GPDSPRST)) {
        dsp_start_frame(d->ep.dsp);

        // hax
        // dsp_run(d->ep.dsp, 1000);
    }
}

static void mcpx_apu_realize(PCIDevice *dev, Error **errp)
{
    MCPXAPUState *d = MCPX_APU_DEVICE(dev);

    dev->config[PCI_INTERRUPT_PIN] = 0x01;

    memory_region_init_io(&d->mmio, OBJECT(dev), &mcpx_apu_mmio_ops, d,
                          "mcpx-apu-mmio", 0x80000);

    memory_region_init_io(&d->vp.mmio, OBJECT(dev), &vp_ops, d,
                          "mcpx-apu-vp", 0x10000);
    memory_region_add_subregion(&d->mmio, 0x20000, &d->vp.mmio);

    memory_region_init_io(&d->gp.mmio, OBJECT(dev), &gp_ops, d,
                          "mcpx-apu-gp", 0x10000);
    memory_region_add_subregion(&d->mmio, 0x30000, &d->gp.mmio);

    memory_region_init_io(&d->ep.mmio, OBJECT(dev), &ep_ops, d,
                          "mcpx-apu-ep", 0x10000);
    memory_region_add_subregion(&d->mmio, 0x50000, &d->ep.mmio);

    pci_register_bar(&d->dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &d->mmio);


    d->se.frame_timer = timer_new_ms(QEMU_CLOCK_VIRTUAL, se_frame, d);
    d->gp.dsp = dsp_init(d, gp_scratch_rw, gp_fifo_rw);
    d->ep.dsp = dsp_init(d, ep_scratch_rw, ep_fifo_rw);
}

static int mcpx_apu_pre_load(void *opaque)
{
    MCPXAPUState *d = opaque;

    timer_del(d->se.frame_timer);

    return 0;
}

static int mcpx_apu_post_load(void *opaque, int version_id)
{
    MCPXAPUState *d = opaque;

    if (((d->regs[NV_PAPU_SECTL] & NV_PAPU_SECTL_XCNTMODE) >> 3) != NV_PAPU_SECTL_XCNTMODE_OFF) {
        timer_mod(d->se.frame_timer, qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL) + 10);
    }

    return 0;
}

const VMStateDescription vmstate_vp_dsp_dma_state = {
    .name = "mcpx-apu/dsp-state/dma",
    .version_id = 1,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields      = (VMStateField[]) {
        VMSTATE_UINT32(configuration, DSPDMAState),
        VMSTATE_UINT32(control, DSPDMAState),
        VMSTATE_UINT32(start_block, DSPDMAState),
        VMSTATE_UINT32(next_block, DSPDMAState),
        VMSTATE_BOOL(error, DSPDMAState),
        VMSTATE_BOOL(eol, DSPDMAState),
        VMSTATE_END_OF_LIST()
    }
};

const VMStateDescription vmstate_vp_dsp_core_state = {
    .name = "mcpx-apu/dsp-state/core",
    .version_id = 1,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields      = (VMStateField[]) {
        // FIXME: Remove unnecessary fields
        VMSTATE_UINT16(instr_cycle, dsp_core_t),
        VMSTATE_UINT32(pc, dsp_core_t),
        VMSTATE_UINT32_ARRAY(registers, dsp_core_t, DSP_REG_MAX),
        VMSTATE_UINT32_2DARRAY(stack, dsp_core_t, 2, 16),
        VMSTATE_UINT32_ARRAY(xram, dsp_core_t, DSP_XRAM_SIZE),
        VMSTATE_UINT32_ARRAY(yram, dsp_core_t, DSP_YRAM_SIZE),
        VMSTATE_UINT32_ARRAY(pram, dsp_core_t, DSP_PRAM_SIZE),
        VMSTATE_UINT32_ARRAY(mixbuffer, dsp_core_t, DSP_MIXBUFFER_SIZE),
        VMSTATE_UINT32_ARRAY(periph, dsp_core_t, DSP_PERIPH_SIZE),
        VMSTATE_UINT32(loop_rep, dsp_core_t),
        VMSTATE_UINT32(pc_on_rep, dsp_core_t),
        VMSTATE_UINT16(interrupt_state, dsp_core_t),
        VMSTATE_UINT16(interrupt_instr_fetch, dsp_core_t),
        VMSTATE_UINT16(interrupt_save_pc, dsp_core_t),
        VMSTATE_UINT16(interrupt_counter, dsp_core_t),
        VMSTATE_UINT16(interrupt_ipl_to_raise, dsp_core_t),
        VMSTATE_UINT16(interrupt_pipeline_count, dsp_core_t),
        VMSTATE_INT16_ARRAY(interrupt_ipl, dsp_core_t, 12),
        VMSTATE_UINT16_ARRAY(interrupt_is_pending, dsp_core_t, 12),
        VMSTATE_UINT32(num_inst, dsp_core_t),
        VMSTATE_UINT32(cur_inst_len, dsp_core_t),
        VMSTATE_UINT32(cur_inst, dsp_core_t),
        VMSTATE_BOOL(executing_for_disasm, dsp_core_t),
        VMSTATE_UINT32(disasm_memory_ptr, dsp_core_t),
        VMSTATE_BOOL(exception_debugging, dsp_core_t),
        VMSTATE_UINT32(disasm_prev_inst_pc, dsp_core_t),
        VMSTATE_BOOL(disasm_is_looping, dsp_core_t),
        VMSTATE_UINT32(disasm_cur_inst, dsp_core_t),
        VMSTATE_UINT16(disasm_cur_inst_len, dsp_core_t),
        VMSTATE_UINT32_ARRAY(disasm_registers_save, dsp_core_t, 64),
// #ifdef DSP_DISASM_REG_PC
//         VMSTATE_UINT32(pc_save, dsp_core_t),
// #endif
        VMSTATE_END_OF_LIST()
    }
};

const VMStateDescription vmstate_vp_dsp_state = {
    .name = "mcpx-apu/dsp-state",
    .version_id = 1,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields = (VMStateField[]) {
        VMSTATE_STRUCT(core, DSPState, 1, vmstate_vp_dsp_core_state, dsp_core_t),
        VMSTATE_STRUCT(dma, DSPState, 1, vmstate_vp_dsp_dma_state, DSPDMAState),
        VMSTATE_INT32(save_cycles, DSPState),
        VMSTATE_UINT32(interrupts, DSPState),
        VMSTATE_END_OF_LIST()
    }
};

static const VMStateDescription vmstate_mcpx_apu = {
    .name = "mcpx-apu",
    .version_id = 1,
    .minimum_version_id = 1,
    .pre_load = mcpx_apu_pre_load,
    .post_load = mcpx_apu_post_load,
    .fields = (VMStateField[]) {
        VMSTATE_PCI_DEVICE(dev, MCPXAPUState),
        VMSTATE_STRUCT_POINTER(gp.dsp, MCPXAPUState, vmstate_vp_dsp_state, DSPState),
        VMSTATE_UINT32_ARRAY(gp.regs, MCPXAPUState, 0x10000),
        VMSTATE_STRUCT_POINTER(ep.dsp, MCPXAPUState, vmstate_vp_dsp_state, DSPState),
        VMSTATE_UINT32_ARRAY(ep.regs, MCPXAPUState, 0x10000),
        VMSTATE_UINT32_ARRAY(regs, MCPXAPUState, 0x20000),
        VMSTATE_END_OF_LIST()
    },
};

static void mcpx_apu_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->vendor_id = PCI_VENDOR_ID_NVIDIA;
    k->device_id = PCI_DEVICE_ID_NVIDIA_MCPX_APU;
    k->revision = 210;
    k->class_id = PCI_CLASS_MULTIMEDIA_AUDIO;
    k->realize = mcpx_apu_realize;

    dc->desc = "MCPX Audio Processing Unit";
    dc->vmsd = &vmstate_mcpx_apu;
}

static const TypeInfo mcpx_apu_info = {
    .name          = "mcpx-apu",
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(MCPXAPUState),
    .class_init    = mcpx_apu_class_init,
    .interfaces = (InterfaceInfo[]) {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { },
    },
};

static void mcpx_apu_register(void)
{
    type_register_static(&mcpx_apu_info);
}
type_init(mcpx_apu_register);

void mcpx_apu_init(PCIBus *bus, int devfn, MemoryRegion *ram)
{
    PCIDevice *dev = pci_create_simple(bus, devfn, "mcpx-apu");
    MCPXAPUState *d = MCPX_APU_DEVICE(dev);

    /* Keep pointers to system memory */
    d->ram = ram;
    d->ram_ptr = memory_region_get_ram_ptr(d->ram);
}
