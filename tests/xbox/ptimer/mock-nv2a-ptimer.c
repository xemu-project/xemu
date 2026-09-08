/*
 * Mock environment for Xbox NV2A PTIMER emulation tests
 */

#include "qemu/osdep.h"
#include "mock-nv2a-ptimer.h"

int64_t mock_virtual_time_ns = 1000000000LL;
int64_t last_timer_mod_expire = -1;
bool mock_timer_active = false;
bool mock_irq_raised = false;

void mock_ptimer_reset(void)
{
    mock_virtual_time_ns = 1000000000LL;
    last_timer_mod_expire = -1;
    mock_timer_active = false;
    mock_irq_raised = false;
}

int64_t qemu_clock_get_ns(QEMUClockType type)
{
    return mock_virtual_time_ns;
}

void timer_init_full(QEMUTimer *ts, QEMUTimerListGroup *timer_list_group,
                     QEMUClockType type, int scale, int attributes,
                     QEMUTimerCB *cb, void *opaque)
{
    ts->cb = cb;
    ts->opaque = opaque;
    ts->scale = scale;
    ts->expire_time = -1;
}

void timer_mod(QEMUTimer *ts, int64_t expire_time)
{
    last_timer_mod_expire = expire_time;
    ts->expire_time = expire_time;
    mock_timer_active = true;
}

void timer_del(QEMUTimer *ts)
{
    ts->expire_time = -1;
    mock_timer_active = false;
}

bool timer_pending(const QEMUTimer *ts)
{
    return mock_timer_active;
}

void nv2a_update_irq(NV2AState *d)
{
    g_assert_nonnull(d);
    mock_irq_raised =
        (d->ptimer.pending_interrupts & d->ptimer.enabled_interrupts) != 0;
}

const NV2ABlockInfo blocktable[NV_NUM_BLOCKS] = { 0 };
