/*
 * TI TSC2005 emulator.
 *
 * Copyright (c) 2006 Andrzej Zaborowski  <balrog@zabor.org>
 * Copyright (C) 2008 Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/timer.h"
#include "sysemu/reset.h"
#include "ui/console.h"
#include "hw/input/tsc2xxx.h"
#include "hw/irq.h"
#include "migration/vmstate.h"
#include "trace.h"

#define TSC_CUT_RESOLUTION(value, p)	((value) >> (16 - (p ? 12 : 10)))

typedef struct {
    qemu_irq pint;	/* Combination of the nPENIRQ and DAV signals */
    QEMUTimer *timer;
    uint16_t model;

    int32_t x, y;
    bool pressure;

    uint8_t reg, state;
    bool irq, command;
    uint16_t data, dav;

    bool busy;
    bool enabled;
    bool host_mode;
    int8_t function;
    int8_t nextfunction;
    bool precision;
    bool nextprecision;
    uint16_t filter;
    uint8_t pin_func;
    uint16_t timing[2];
    uint8_t noise;
    bool reset;
    bool pdst;
    bool pnd0;
    uint16_t temp_thr[2];
    uint16_t aux_thr[2];

    int32_t tr[8];
} TSC2005State;

enum {
    TSC_MODE_XYZ_SCAN	= 0x0,
    TSC_MODE_XY_SCAN,
    TSC_MODE_X,
    TSC_MODE_Y,
    TSC_MODE_Z,
    TSC_MODE_AUX,
    TSC_MODE_TEMP1,
    TSC_MODE_TEMP2,
    TSC_MODE_AUX_SCAN,
    TSC_MODE_X_TEST,
    TSC_MODE_Y_TEST,
    TSC_MODE_TS_TEST,
    TSC_MODE_RESERVED,
    TSC_MODE_XX_DRV,
    TSC_MODE_YY_DRV,
    TSC_MODE_YX_DRV,
};

static const uint16_t mode_regs[16] = {
    0xf000,	/* X, Y, Z scan */
    0xc000,	/* X, Y scan */
    0x8000,	/* X */
    0x4000,	/* Y */
    0x3000,	/* Z */
    0x0800,	/* AUX */
    0x0400,	/* TEMP1 */
    0x0200,	/* TEMP2 */
    0x0800,	/* AUX scan */
    0x0040,	/* X test */
    0x0020,	/* Y test */
    0x0080,	/* Short-circuit test */
    0x0000,	/* Reserved */
    0x0000,	/* X+, X- drivers */
    0x0000,	/* Y+, Y- drivers */
    0x0000,	/* Y+, X- drivers */
};

#define X_TRANSFORM(s)			\
    ((s->y * s->tr[0] - s->x * s->tr[1]) / s->tr[2] + s->tr[3])
#define Y_TRANSFORM(s)			\
    ((s->y * s->tr[4] - s->x * s->tr[5]) / s->tr[6] + s->tr[7])
#define Z1_TRANSFORM(s)			\
    ((400 - ((s)->x >> 7) + ((s)->pressure << 10)) << 4)
#define Z2_TRANSFORM(s)			\
    ((4000 + ((s)->y >> 7) - ((s)->pressure << 10)) << 4)

#define AUX_VAL				(700 << 4)	/* +/- 3 at 12-bit */
#define TEMP1_VAL			(1264 << 4)	/* +/- 5 at 12-bit */
#define TEMP2_VAL			(1531 << 4)	/* +/- 5 at 12-bit */

static uint16_t tsc2005_read(TSC2005State *s, int reg)
{
    uint16_t ret;

    switch (reg) {
    case 0x0:	/* X */
        s->dav &= ~mode_regs[TSC_MODE_X];
        return TSC_CUT_RESOLUTION(X_TRANSFORM(s), s->precision) +
                (s->noise & 3);
    case 0x1:	/* Y */
        s->dav &= ~mode_regs[TSC_MODE_Y];
        s->noise ++;
        return TSC_CUT_RESOLUTION(Y_TRANSFORM(s), s->precision) ^
                (s->noise & 3);
    case 0x2:	/* Z1 */
        s->dav &= 0xdfff;
        return TSC_CUT_RESOLUTION(Z1_TRANSFORM(s), s->precision) -
                (s->noise & 3);
    case 0x3:	/* Z2 */
        s->dav &= 0xefff;
        return TSC_CUT_RESOLUTION(Z2_TRANSFORM(s), s->precision) |
                (s->noise & 3);

    case 0x4:	/* AUX */
        s->dav &= ~mode_regs[TSC_MODE_AUX];
        return TSC_CUT_RESOLUTION(AUX_VAL, s->precision);

    case 0x5:	/* TEMP1 */
        s->dav &= ~mode_regs[TSC_MODE_TEMP1];
        return TSC_CUT_RESOLUTION(TEMP1_VAL, s->precision) -
                (s->noise & 5);
    case 0x6:	/* TEMP2 */
        s->dav &= 0xdfff;
        s->dav &= ~mode_regs[TSC_MODE_TEMP2];
        return TSC_CUT_RESOLUTION(TEMP2_VAL, s->precision) ^
                (s->noise & 3);

    case 0x7:	/* Status */
        ret = s->dav | (s->reset << 7) | (s->pdst << 2) | 0x0;
        s->dav &= ~(mode_regs[TSC_MODE_X_TEST] | mode_regs[TSC_MODE_Y_TEST] |
                        mode_regs[TSC_MODE_TS_TEST]);
        s->reset = true;
        return ret;

    case 0x8:	/* AUX high treshold */
        return s->aux_thr[1];
    case 0x9:	/* AUX low treshold */
        return s->aux_thr[0];

    case 0xa:	/* TEMP high treshold */
        return s->temp_thr[1];
    case 0xb:	/* TEMP low treshold */
        return s->temp_thr[0];

    case 0xc:	/* CFR0 */
        return (s->pressure << 15) | ((!s->busy) << 14) |
                (s->nextprecision << 13) | s->timing[0]; 
    case 0xd:	/* CFR1 */
        return s->timing[1];
    case 0xe:	/* CFR2 */
        return (s->pin_func << 14) | s->filter;

    case 0xf:	/* Function select status */
        return s->function >= 0 ? 1 << s->function : 0;
    }

    /* Never gets here */
    return 0xffff;
}

static void tsc2005_write(TSC2005State *s, int reg, uint16_t data)
{
    switch (reg) {
    case 0x8:	/* AUX high treshold */
        s->aux_thr[1] = data;
        break;
    case 0x9:	/* AUX low treshold */
        s->aux_thr[0] = data;
        break;

    case 0xa:	/* TEMP high treshold */
        s->temp_thr[1] = data;
        break;
    case 0xb:	/* TEMP low treshold */
        s->temp_thr[0] = data;
        break;

    case 0xc:	/* CFR0 */
        s->host_mode = (data >> 15) != 0;
        if (s->enabled != !(data & 0x4000)) {
            s->enabled = !(data & 0x4000);
            trace_tsc2005_sense(s->enabled ? "enabled" : "disabled");
            if (s->busy && !s->enabled)
                timer_del(s->timer);
            s->busy = s->busy && s->enabled;
        }
        s->nextprecision = (data >> 13) & 1;
        s->timing[0] = data & 0x1fff;
        if ((s->timing[0] >> 11) == 3) {
            qemu_log_mask(LOG_GUEST_ERROR,
                          "tsc2005_write: illegal conversion clock setting\n");
        }
        break;
    case 0xd:	/* CFR1 */
        s->timing[1] = data & 0xf07;
        break;
    case 0xe:	/* CFR2 */
        s->pin_func = (data >> 14) & 3;
        s->filter = data & 0x3fff;
        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: write into read-only register 0x%x\n",
                      __func__, reg);
    }
}

/* This handles most of the chip's logic.  */
static void tsc2005_pin_update(TSC2005State *s)
{
    int64_t expires;
    bool pin_state;

    switch (s->pin_func) {
    case 0:
        pin_state = !s->pressure && !!s->dav;
        break;
    case 1:
    case 3:
    default:
        pin_state = !s->dav;
        break;
    case 2:
        pin_state = !s->pressure;
    }

    if (pin_state != s->irq) {
        s->irq = pin_state;
        qemu_set_irq(s->pint, s->irq);
    }

    switch (s->nextfunction) {
    case TSC_MODE_XYZ_SCAN:
    case TSC_MODE_XY_SCAN:
        if (!s->host_mode && s->dav)
            s->enabled = false;
        if (!s->pressure)
            return;
        /* Fall through */
    case TSC_MODE_AUX_SCAN:
        break;

    case TSC_MODE_X:
    case TSC_MODE_Y:
    case TSC_MODE_Z:
        if (!s->pressure)
            return;
        /* Fall through */
    case TSC_MODE_AUX:
    case TSC_MODE_TEMP1:
    case TSC_MODE_TEMP2:
    case TSC_MODE_X_TEST:
    case TSC_MODE_Y_TEST:
    case TSC_MODE_TS_TEST:
        if (s->dav)
            s->enabled = false;
        break;

    case TSC_MODE_RESERVED:
    case TSC_MODE_XX_DRV:
    case TSC_MODE_YY_DRV:
    case TSC_MODE_YX_DRV:
    default:
        return;
    }

    if (!s->enabled || s->busy)
        return;

    s->busy = true;
    s->precision = s->nextprecision;
    s->function = s->nextfunction;
    s->pdst = !s->pnd0;	/* Synchronised on internal clock */
    expires = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL) +
        (NANOSECONDS_PER_SECOND >> 7);
    timer_mod(s->timer, expires);
}

static void tsc2005_reset(TSC2005State *s)
{
    s->state = 0;
    s->pin_func = 0;
    s->enabled = false;
    s->busy = false;
    s->nextprecision = false;
    s->nextfunction = 0;
    s->timing[0] = 0;
    s->timing[1] = 0;
    s->irq = false;
    s->dav = 0;
    s->reset = false;
    s->pdst = true;
    s->pnd0 = false;
    s->function = -1;
    s->temp_thr[0] = 0x000;
    s->temp_thr[1] = 0xfff;
    s->aux_thr[0] = 0x000;
    s->aux_thr[1] = 0xfff;

    tsc2005_pin_update(s);
}

static uint8_t tsc2005_txrx_word(void *opaque, uint8_t value)
{
    TSC2005State *s = opaque;
    uint32_t ret = 0;

    switch (s->state ++) {
    case 0:
        if (value & 0x80) {
            /* Command */
            if (value & (1 << 1))
                tsc2005_reset(s);
            else {
                s->nextfunction = (value >> 3) & 0xf;
                s->nextprecision = (value >> 2) & 1;
                if (s->enabled != !(value & 1)) {
                    s->enabled = !(value & 1);
                    trace_tsc2005_sense(s->enabled ? "enabled" : "disabled");
                    if (s->busy && !s->enabled)
                        timer_del(s->timer);
                    s->busy = s->busy && s->enabled;
                }
                tsc2005_pin_update(s);
            }

            s->state = 0;
        } else if (value) {
            /* Data transfer */
            s->reg = (value >> 3) & 0xf;
            s->pnd0 = (value >> 1) & 1;
            s->command = value & 1;

            if (s->command) {
                /* Read */
                s->data = tsc2005_read(s, s->reg);
                tsc2005_pin_update(s);
            } else
                s->data = 0;
        } else
            s->state = 0;
        break;

    case 1:
        if (s->command)
            ret = (s->data >> 8) & 0xff;
        else
            s->data |= value << 8;
        break;

    case 2:
        if (s->command)
            ret = s->data & 0xff;
        else {
            s->data |= value;
            tsc2005_write(s, s->reg, s->data);
            tsc2005_pin_update(s);
        }

        s->state = 0;
        break;
    }

    return ret;
}

uint32_t tsc2005_txrx(void *opaque, uint32_t value, int len)
{
    uint32_t ret = 0;

    len &= ~7;
    while (len > 0) {
        len -= 8;
        ret |= tsc2005_txrx_word(opaque, (value >> len) & 0xff) << len;
    }

    return ret;
}

static void tsc2005_timer_tick(void *opaque)
{
    TSC2005State *s = opaque;

    /* Timer ticked -- a set of conversions has been finished.  */

    if (!s->busy)
        return;

    s->busy = false;
    s->dav |= mode_regs[s->function];
    s->function = -1;
    tsc2005_pin_update(s);
}

static void tsc2005_touchscreen_event(void *opaque,
                int x, int y, int z, int buttons_state)
{
    TSC2005State *s = opaque;
    int p = s->pressure;

    if (buttons_state) {
        s->x = x;
        s->y = y;
    }
    s->pressure = !!buttons_state;

    /*
     * Note: We would get better responsiveness in the guest by
     * signaling TS events immediately, but for now we simulate
     * the first conversion delay for sake of correctness.
     */
    if (p != s->pressure)
        tsc2005_pin_update(s);
}

static int tsc2005_post_load(void *opaque, int version_id)
{
    TSC2005State *s = (TSC2005State *) opaque;

    s->busy = timer_pending(s->timer);
    tsc2005_pin_update(s);

    return 0;
}

static const VMStateDescription vmstate_tsc2005 = {
    .name = "tsc2005",
    .version_id = 2,
    .minimum_version_id = 2,
    .post_load = tsc2005_post_load,
    .fields      = (VMStateField []) {
        VMSTATE_BOOL(pressure, TSC2005State),
        VMSTATE_BOOL(irq, TSC2005State),
        VMSTATE_BOOL(command, TSC2005State),
        VMSTATE_BOOL(enabled, TSC2005State),
        VMSTATE_BOOL(host_mode, TSC2005State),
        VMSTATE_BOOL(reset, TSC2005State),
        VMSTATE_BOOL(pdst, TSC2005State),
        VMSTATE_BOOL(pnd0, TSC2005State),
        VMSTATE_BOOL(precision, TSC2005State),
        VMSTATE_BOOL(nextprecision, TSC2005State),
        VMSTATE_UINT8(reg, TSC2005State),
        VMSTATE_UINT8(state, TSC2005State),
        VMSTATE_UINT16(data, TSC2005State),
        VMSTATE_UINT16(dav, TSC2005State),
        VMSTATE_UINT16(filter, TSC2005State),
        VMSTATE_INT8(nextfunction, TSC2005State),
        VMSTATE_INT8(function, TSC2005State),
        VMSTATE_INT32(x, TSC2005State),
        VMSTATE_INT32(y, TSC2005State),
        VMSTATE_TIMER_PTR(timer, TSC2005State),
        VMSTATE_UINT8(pin_func, TSC2005State),
        VMSTATE_UINT16_ARRAY(timing, TSC2005State, 2),
        VMSTATE_UINT8(noise, TSC2005State),
        VMSTATE_UINT16_ARRAY(temp_thr, TSC2005State, 2),
        VMSTATE_UINT16_ARRAY(aux_thr, TSC2005State, 2),
        VMSTATE_INT32_ARRAY(tr, TSC2005State, 8),
        VMSTATE_END_OF_LIST()
    }
};

void *tsc2005_init(qemu_irq pintdav)
{
    TSC2005State *s;

    s = g_new0(TSC2005State, 1);
    s->x = 400;
    s->y = 240;
    s->pressure = false;
    s->precision = s->nextprecision = false;
    s->timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, tsc2005_timer_tick, s);
    s->pint = pintdav;
    s->model = 0x2005;

    s->tr[0] = 0;
    s->tr[1] = 1;
    s->tr[2] = 1;
    s->tr[3] = 0;
    s->tr[4] = 1;
    s->tr[5] = 0;
    s->tr[6] = 1;
    s->tr[7] = 0;

    tsc2005_reset(s);

    qemu_add_mouse_event_handler(tsc2005_touchscreen_event, s, 1,
                    "QEMU TSC2005-driven Touchscreen");

    qemu_register_reset((void *) tsc2005_reset, s);
    vmstate_register(NULL, 0, &vmstate_tsc2005, s);

    return s;
}

/*
 * Use tslib generated calibration data to generate ADC input values
 * from the touchscreen.  Assuming 12-bit precision was used during
 * tslib calibration.
 */
void tsc2005_set_transform(void *opaque, MouseTransformInfo *info)
{
    TSC2005State *s = (TSC2005State *) opaque;

    /* This version assumes touchscreen X & Y axis are parallel or
     * perpendicular to LCD's  X & Y axis in some way.  */
    if (abs(info->a[0]) > abs(info->a[1])) {
        s->tr[0] = 0;
        s->tr[1] = -info->a[6] * info->x;
        s->tr[2] = info->a[0];
        s->tr[3] = -info->a[2] / info->a[0];
        s->tr[4] = info->a[6] * info->y;
        s->tr[5] = 0;
        s->tr[6] = info->a[4];
        s->tr[7] = -info->a[5] / info->a[4];
    } else {
        s->tr[0] = info->a[6] * info->y;
        s->tr[1] = 0;
        s->tr[2] = info->a[1];
        s->tr[3] = -info->a[2] / info->a[1];
        s->tr[4] = 0;
        s->tr[5] = -info->a[6] * info->x;
        s->tr[6] = info->a[3];
        s->tr[7] = -info->a[5] / info->a[3];
    }

    s->tr[0] >>= 11;
    s->tr[1] >>= 11;
    s->tr[3] <<= 4;
    s->tr[4] >>= 11;
    s->tr[5] >>= 11;
    s->tr[7] <<= 4;
}
