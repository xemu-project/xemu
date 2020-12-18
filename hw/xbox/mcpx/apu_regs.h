/*
 * QEMU MCPX Audio Processing Unit implementation
 *
 * Copyright (c) 2012 espes
 * Copyright (c) 2018-2019 Jannik Vogel
 * Copyright (c) 2019-2021 Matt Borgerson
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

#ifndef MCPX_APU_REGS_H
#define MCPX_APU_REGS_H

#define NV_PAPU_ISTS                                     0x00001000
#   define NV_PAPU_ISTS_GINTSTS                               (1 << 0)
#   define NV_PAPU_ISTS_FETINTSTS                             (1 << 4)
#   define NV_PAPU_ISTS_FENINTSTS                             (1 << 5)
#   define NV_PAPU_ISTS_FEVINTSTS                             (1 << 6)
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

#define NV_PAPU_FENADDR                                    0x0000115c

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
#define NV_PAPU_VPSSLADDR                                0x00002034
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
#   define NV_PAPU_GPOFBASE0_VALUE                          0x00FFFFFF // FIXME: Use ffff00 mask but shift appropriately
#define NV_PAPU_GPOFEND0                                 0x00003028
#   define NV_PAPU_GPOFEND0_VALUE                           0x00FFFFFF
#define NV_PAPU_GPOFCUR0                                 0x0000302C
#   define NV_PAPU_GPOFCUR0_VALUE                           0x00FFFFFF
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
#define NV1BA0_PIO_GET_VOICE_POSITION                    0x00000130
#   define NV1BA0_PIO_VOICE_RELEASE_HANDLE                  0x0000FFFF
#define NV1BA0_PIO_VOICE_PAUSE                           0x00000140
#   define NV1BA0_PIO_VOICE_PAUSE_HANDLE                    0x0000FFFF
#   define NV1BA0_PIO_VOICE_PAUSE_ACTION                    (1 << 18)
#define NV1BA0_PIO_SET_CONTEXT_DMA_NOTIFY                0x00000180
#define NV1BA0_PIO_SET_CURRENT_SSL_CONTEXT_DMA           0x0000018C
#define NV1BA0_PIO_SET_CURRENT_SSL                       0x00000190
#   define NV1BA0_PIO_SET_CURRENT_SSL_BASE_PAGE             0x3FFFC0
#define NV1BA0_PIO_SET_SSL_SEGMENT_OFFSET                0x00000600
#define NV1BA0_PIO_SET_SSL_SEGMENT_LENGTH                0x00000604
#define NV1BA0_PIO_SET_SUBMIX_HEADROOM                   0x00000200
#   define NV1BA0_PIO_SET_SUBMIX_HEADROOM_AMOUNT            0x7
#define NV1BA0_PIO_SET_HRTF_HEADROOM                       0x00000280
#   define NV1BA0_PIO_SET_HRTF_HEADROOM_AMOUNT              0x7
#define NV1BA0_PIO_SET_HRTF_SUBMIXES                     0x000002C0
#define NV1BA0_PIO_SET_CURRENT_VOICE                     0x000002F8
#define NV1BA0_PIO_VOICE_LOCK                            0x000002FC
#define NV1BA0_PIO_SET_VOICE_CFG_VBIN                    0x00000300
#define NV1BA0_PIO_SET_VOICE_CFG_FMT                     0x00000304
#define NV1BA0_PIO_SET_VOICE_CFG_ENV0                    0x00000308
#define NV1BA0_PIO_SET_VOICE_CFG_ENVA                    0x0000030C
#define NV1BA0_PIO_SET_VOICE_CFG_ENV1                    0x00000310
#define NV1BA0_PIO_SET_VOICE_CFG_ENVF                    0x00000314
#define NV1BA0_PIO_SET_VOICE_CFG_MISC                    0x00000318
#define NV1BA0_PIO_SET_VOICE_SSL_A                       0x00000320
#   define NV1BA0_PIO_SET_VOICE_SSL_A_COUNT                 0x000000FF
#   define NV1BA0_PIO_SET_VOICE_SSL_A_BASE                  0xFFFFFF00
#define NV1BA0_PIO_SET_VOICE_SSL_B                       0x0000035C
#define NV1BA0_PIO_SET_VOICE_TAR_VOLA                    0x00000360
#define NV1BA0_PIO_SET_VOICE_TAR_VOLB                    0x00000364
#define NV1BA0_PIO_SET_VOICE_TAR_VOLC                    0x00000368
#define NV1BA0_PIO_SET_VOICE_LFO_ENV                     0x0000036C
#define NV1BA0_PIO_SET_VOICE_TAR_FCA                     0x00000374
#define NV1BA0_PIO_SET_VOICE_TAR_FCB                     0x00000378
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
#define NV1BA0_PIO_SET_SSL_SEGMENT_OFFSET                0x00000600
#define NV1BA0_PIO_SET_SSL_SEGMENT_LENGTH                0x00000604
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
#   define NV_PAVS_VOICE_CFG_FMT_HEADROOM                   (0x7  << 13)
#   define NV_PAVS_VOICE_CFG_FMT_SAMPLES_PER_BLOCK          (0x1F << 16)
#   define NV_PAVS_VOICE_CFG_FMT_MULTIPASS_BIN              (0x1F << 16)
#   define NV_PAVS_VOICE_CFG_FMT_MULTIPASS                  (1 << 21)
#   define NV_PAVS_VOICE_CFG_FMT_LINKED                     (1 << 22)
#   define NV_PAVS_VOICE_CFG_FMT_PERSIST                    (1 << 23)
#   define NV_PAVS_VOICE_CFG_FMT_DATA_TYPE                  (1 << 24)
#   define NV_PAVS_VOICE_CFG_FMT_LOOP                       (1 << 25)
#   define NV_PAVS_VOICE_CFG_FMT_CLEAR_MIX                  (1 << 26)
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
#   define NV_PAVS_VOICE_CFG_MISC_FMODE                     (3 << 16)
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
#      define NV_PAVS_VOICE_PAR_STATE_EFCUR_OFF                 0
#      define NV_PAVS_VOICE_PAR_STATE_EFCUR_DELAY               1
#      define NV_PAVS_VOICE_PAR_STATE_EFCUR_ATTACK              2
#      define NV_PAVS_VOICE_PAR_STATE_EFCUR_HOLD                3
#      define NV_PAVS_VOICE_PAR_STATE_EFCUR_DECAY               4
#      define NV_PAVS_VOICE_PAR_STATE_EFCUR_SUSTAIN             5
#      define NV_PAVS_VOICE_PAR_STATE_EFCUR_RELEASE             6
#      define NV_PAVS_VOICE_PAR_STATE_EFCUR_FORCE_RELEASE       7
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
#define NV_PAVS_VOICE_TAR_FCA                            0x00000074
#   define NV_PAVS_VOICE_TAR_FCA_FC0                        0x0000FFFF
#   define NV_PAVS_VOICE_TAR_FCA_FC1                        0xFFFF0000
#define NV_PAVS_VOICE_TAR_FCB                            0x00000078
#define NV_PAVS_VOICE_TAR_PITCH_LINK                     0x0000007C
#   define NV_PAVS_VOICE_TAR_PITCH_LINK_NEXT_VOICE_HANDLE   0x0000FFFF
#   define NV_PAVS_VOICE_TAR_PITCH_LINK_PITCH               0xFFFF0000


#define GP_DSP_MIXBUF_BASE 0x001400

#define GP_OUTPUT_FIFO_COUNT  4
#define GP_INPUT_FIFO_COUNT   2

#define EP_OUTPUT_FIFO_COUNT  4
#define EP_INPUT_FIFO_COUNT   2

#define MCPX_HW_MAX_VOICES 256

#define NUM_SAMPLES_PER_FRAME 32
#define NUM_MIXBINS 32

#define ADPCM_SAMPLES_PER_BLOCK 64 // FIXME: Should be 65? Check interpolation

#define MCPX_HW_MAX_PRD_ENTRIES_PER_SSL     16
#define MCPX_HW_SSLS_PER_VOICE              2
#define MCPX_HW_MAX_PRD_ENTRIES_PER_VOICE   (MCPX_HW_MAX_PRD_ENTRIES_PER_SSL * MCPX_HW_SSLS_PER_VOICE)
#define MCPX_HW_MAX_SSL_PRDS                (MCPX_HW_MAX_VOICES * MCPX_HW_MAX_PRD_ENTRIES_PER_VOICE)

#define NV_PSGE_SIZE                                     0x00000008

#define MCPX_HW_NOTIFIER_BASE_OFFSET 2
enum MCPX_HW_NOTIFIER {
    MCPX_HW_NOTIFIER_SSLA_DONE = 0,
    MCPX_HW_NOTIFIER_SSLB_DONE,
    // ...
    MCPX_HW_NOTIFIER_COUNT = 4,
};
#define NV1BA0_NOTIFICATION_STATUS_DONE_SUCCESS       0x01
#define NV1BA0_NOTIFICATION_STATUS_IN_PROGRESS        0x80

#endif
