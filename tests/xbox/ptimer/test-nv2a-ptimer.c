/*
 * Unit tests for Xbox NV2A PTIMER emulation (hw/xbox/nv2a/ptimer.c)
 */

#include "qemu/osdep.h"
#include <glib.h>
#include "hw/xbox/nv2a/nv2a_int.h"
#include "mock-nv2a-ptimer.h"

#define PTIMER_WRITE(reg, val) ptimer_write(&state, (reg), (val), 4)

static void setup_ptimer_state(NV2AState *state)
{
    memset(state, 0, sizeof(*state));
    state->pramdac.core_clock_freq =
        233333333; /* Standard NV2A core clock: ~233.33 MHz */
    state->ptimer.numerator = 1;
    state->ptimer.denominator = 1;
    mock_ptimer_reset();

    ptimer_init(state);
}

/*
 * Set an alarm 1 tick into the future. Verify that the timer fires and the
 * alarm interrupt is raised.
 */
static void test_single_tick_alarm(void)
{
    NV2AState state;
    setup_ptimer_state(&state);

    PTIMER_WRITE(NV_PTIMER_INTR_EN_0, NV_PTIMER_INTR_0_ALARM);

    uint32_t t0 = (uint32_t)ptimer_read(&state, NV_PTIMER_TIME_0, 4);
    uint32_t target_alarm = (t0 + (1 << 5)) & 0xffffffe0;
    PTIMER_WRITE(NV_PTIMER_ALARM_0, target_alarm);

    int64_t scheduled_time = last_timer_mod_expire;
    g_assert_true(mock_timer_active);

    mock_virtual_time_ns = scheduled_time;
    state.ptimer.timer.cb(state.ptimer.timer.opaque);

    uint32_t current_time = (uint32_t)ptimer_read(&state, NV_PTIMER_TIME_0, 4);
    if (!(state.ptimer.pending_interrupts & NV_PTIMER_INTR_0_ALARM)) {
        g_test_message("Rounding failure: timer expired at %" PRId64
                       " ns, but TIME_0 (0x%08x) < target (0x%08x)",
                       mock_virtual_time_ns, current_time, target_alarm);
    }
    g_assert_cmphex(state.ptimer.pending_interrupts & NV_PTIMER_INTR_0_ALARM,
                    ==, NV_PTIMER_INTR_0_ALARM);
}

/*
 * Sweep alarms across tick offsets (1 to 500 ticks) in the future. Verify that
 * the timer fires on expiration and the alarm interrupt is raised for all
 * offsets without rounding errors.
 */
static void test_alarm_sweep(void)
{
    NV2AState state;
    int total_cases = 500;
    int failures = 0;

    for (int offset_ticks = 1; offset_ticks <= total_cases; offset_ticks++) {
        setup_ptimer_state(&state);
        PTIMER_WRITE(NV_PTIMER_INTR_EN_0, NV_PTIMER_INTR_0_ALARM);

        uint32_t t0 = (uint32_t)ptimer_read(&state, NV_PTIMER_TIME_0, 4);
        uint32_t target_alarm = (t0 + (offset_ticks << 5)) & 0xffffffe0;

        PTIMER_WRITE(NV_PTIMER_ALARM_0, target_alarm);

        mock_virtual_time_ns = last_timer_mod_expire;
        state.ptimer.timer.cb(state.ptimer.timer.opaque);

        if (!(state.ptimer.pending_interrupts & NV_PTIMER_INTR_0_ALARM)) {
            failures++;
        }
    }

    if (failures > 0) {
        g_test_message("Sweep failure: %d / %d cases missed alarm on "
                       "expiration due to rounding",
                       failures, total_cases);
    }
    g_assert_cmpint(failures, ==, 0);
}

/*
 * Set an alarm and advance time until it is reached, then trigger a timer
 * reschedule via a register write. Verify that the rescheduled timer delta is
 * strictly greater than zero and does not reschedule with a 0-ns delta.
 */
static void test_zero_delta_reschedule_when_alarm_reached(void)
{
    NV2AState state;
    setup_ptimer_state(&state);

    PTIMER_WRITE(NV_PTIMER_INTR_EN_0, NV_PTIMER_INTR_0_ALARM);

    uint32_t t0 = (uint32_t)ptimer_read(&state, NV_PTIMER_TIME_0, 4);
    uint32_t target_alarm = (t0 + (1 << 5)) & 0xffffffe0;
    PTIMER_WRITE(NV_PTIMER_ALARM_0, target_alarm);

    mock_virtual_time_ns += 100;
    PTIMER_WRITE(NV_PTIMER_NUMERATOR, 1);

    int64_t reschedule_delta = last_timer_mod_expire - mock_virtual_time_ns;
    if (mock_timer_active) {
        g_assert_cmpint(reschedule_delta, >, 0);
    }
}

/*
 * Set an alarm with alarm interrupts disabled. Verify that the QEMU timer
 * is disarmed and does not run.
 */
static void test_timer_disarmed_when_interrupt_disabled(void)
{
    NV2AState state;
    setup_ptimer_state(&state);

    PTIMER_WRITE(NV_PTIMER_INTR_EN_0, 0);

    uint32_t t0 = (uint32_t)ptimer_read(&state, NV_PTIMER_TIME_0, 4);
    uint32_t target_alarm = (t0 + (100 << 5)) & 0xffffffe0;
    PTIMER_WRITE(NV_PTIMER_ALARM_0, target_alarm);

    g_assert_false(mock_timer_active);
}

/*
 * Set an alarm with interrupts disabled and advance time past the target.
 * Verify that polling NV_PTIMER_INTR_0 detects that the alarm has occurred even
 * when the timer is inactive.
 */
static void test_polling_without_timer(void)
{
    NV2AState state;
    setup_ptimer_state(&state);

    PTIMER_WRITE(NV_PTIMER_INTR_EN_0, 0);

    uint32_t t0 = (uint32_t)ptimer_read(&state, NV_PTIMER_TIME_0, 4);
    uint32_t target_alarm = (t0 + (10 << 5)) & 0xffffffe0;
    PTIMER_WRITE(NV_PTIMER_ALARM_0, target_alarm);

    mock_virtual_time_ns += 1000;

    uint32_t intr = (uint32_t)ptimer_read(&state, NV_PTIMER_INTR_0, 4);
    g_assert_cmphex(intr & NV_PTIMER_INTR_0_ALARM, ==, NV_PTIMER_INTR_0_ALARM);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/xbox/nv2a/ptimer/single_tick_alarm",
                    test_single_tick_alarm);
    g_test_add_func("/xbox/nv2a/ptimer/alarm_sweep", test_alarm_sweep);
    g_test_add_func("/xbox/nv2a/ptimer/zero_delta_reschedule",
                    test_zero_delta_reschedule_when_alarm_reached);
    g_test_add_func("/xbox/nv2a/ptimer/timer_disarmed_when_interrupt_disabled",
                    test_timer_disarmed_when_interrupt_disabled);
    g_test_add_func("/xbox/nv2a/ptimer/polling_without_timer",
                    test_polling_without_timer);

    return g_test_run();
}
