/*
 * Unit tests for Xbox NV2A PTIMER emulation (hw/xbox/nv2a/ptimer.c)
 */

#include "qemu/osdep.h"
#include <glib.h>
#include "hw/xbox/nv2a/nv2a_int.h"
#include "mock-nv2a-ptimer.h"

#define PTIMER_WRITE(reg, val) ptimer_write(&state, (reg), (val), 4)
#define PTIMER_ENABLE_INTERRUPT_EXPECT_CLEARED()                              \
    do {                                                                      \
        PTIMER_WRITE(NV_PTIMER_INTR_EN_0, NV_PTIMER_INTR_0_ALARM);            \
        g_assert_cmphex(                                                      \
            state.ptimer.pending_interrupts & NV_PTIMER_INTR_0_ALARM, ==, 0); \
    } while (0)
#define PTIMER_CLEAR_INTERRUPT() \
    PTIMER_WRITE(NV_PTIMER_INTR_0, NV_PTIMER_INTR_0_ALARM)
#define FIRE_ALARM_TIMER() state.ptimer.timer.cb(state.ptimer.timer.opaque)

static void setup_ptimer_state(NV2AState *state)
{
    memset(state, 0, sizeof(*state));
    state->pramdac.core_clock_freq = 233333333;
    state->ptimer.numerator = 1;
    state->ptimer.denominator = 1;
    mock_ptimer_reset();

    ptimer_init(state);
}

/*
 * Set an alarm 1 tick into the future. Verify that the timer fires and the
 * alarm interrupt is raised.
 */
static void run_single_tick_alarm_test(uint32_t numerator, uint32_t denominator)
{
    NV2AState state;
    setup_ptimer_state(&state);
    state.ptimer.numerator = numerator;
    state.ptimer.denominator = denominator;

    uint32_t t0 = (uint32_t)ptimer_read(&state, NV_PTIMER_TIME_0, 4);
    uint32_t target_alarm = (t0 + (1 << 5)) & 0xffffffe0;
    PTIMER_WRITE(NV_PTIMER_ALARM_0, target_alarm);

    PTIMER_ENABLE_INTERRUPT_EXPECT_CLEARED();

    int64_t scheduled_time = last_timer_mod_expire;
    g_assert_true(mock_timer_active);

    mock_virtual_time_ns = scheduled_time;
    FIRE_ALARM_TIMER();

    uint32_t current_time = (uint32_t)ptimer_read(&state, NV_PTIMER_TIME_0, 4);
    if (!(state.ptimer.pending_interrupts & NV_PTIMER_INTR_0_ALARM)) {
        g_test_message(
            "Rounding failure (ratio %u/%u): timer expired at %" PRId64
            " ns, but TIME_0 (0x%08x) < target (0x%08x)",
            numerator, denominator, mock_virtual_time_ns, current_time,
            target_alarm);
    }
    g_assert_cmphex(state.ptimer.pending_interrupts & NV_PTIMER_INTR_0_ALARM,
                    ==, NV_PTIMER_INTR_0_ALARM);
    g_assert_true(mock_irq_raised);
}

static void test_single_tick_alarm(void)
{
    run_single_tick_alarm_test(1, 1);
}

static void test_single_tick_alarm_non_unit_ratio(void)
{
    run_single_tick_alarm_test(0xDE86, 0x1DCD);
}

/*
 * Sweep alarms across tick offsets (1 to 500 ticks) in the future. Verify that
 * the timer fires on expiration and the alarm interrupt is raised for all
 * offsets without rounding errors.
 */
static void run_alarm_sweep_test(uint32_t numerator, uint32_t denominator)
{
    NV2AState state;
    int total_cases = 500;
    int failures = 0;

    for (int offset_ticks = 1; offset_ticks <= total_cases; offset_ticks++) {
        setup_ptimer_state(&state);
        state.ptimer.numerator = numerator;
        state.ptimer.denominator = denominator;

        uint32_t t0 = (uint32_t)ptimer_read(&state, NV_PTIMER_TIME_0, 4);
        uint32_t target_alarm = (t0 + (offset_ticks << 5)) & 0xffffffe0;
        PTIMER_WRITE(NV_PTIMER_ALARM_0, target_alarm);

        PTIMER_ENABLE_INTERRUPT_EXPECT_CLEARED();

        mock_virtual_time_ns = last_timer_mod_expire;
        FIRE_ALARM_TIMER();

        if (!(state.ptimer.pending_interrupts & NV_PTIMER_INTR_0_ALARM) ||
            !mock_irq_raised) {
            failures++;
        }
    }

    if (failures > 0) {
        g_test_message("Sweep failure (ratio %u/%u): %d / %d cases missed "
                       "alarm on expiration due to rounding",
                       numerator, denominator, failures, total_cases);
    }
    g_assert_cmpint(failures, ==, 0);
}

static void test_alarm_sweep(void)
{
    run_alarm_sweep_test(1, 1);
}

static void test_alarm_sweep_non_unit_ratio(void)
{
    run_alarm_sweep_test(0xDE86, 0x1DCD);
}

/*
 * Set an alarm and advance time until it is reached, then trigger a timer
 * reschedule via a NUMERATOR write. Verify that the alarm fires immediately,
 * IRQ is raised, and the timer is rescheduled for the next 2^32-tick epoch.
 */
static void test_nonzero_delta_reschedule_numerator(void)
{
    NV2AState state;
    setup_ptimer_state(&state);

    uint32_t t0 = (uint32_t)ptimer_read(&state, NV_PTIMER_TIME_0, 4);
    uint32_t target_alarm = (t0 + (1 << 5)) & 0xffffffe0;
    PTIMER_WRITE(NV_PTIMER_ALARM_0, target_alarm);

    PTIMER_ENABLE_INTERRUPT_EXPECT_CLEARED();

    mock_virtual_time_ns += 100;
    PTIMER_WRITE(NV_PTIMER_NUMERATOR, 1);

    int64_t reschedule_delta = last_timer_mod_expire - mock_virtual_time_ns;
    g_assert_true(mock_timer_active);
    g_assert_cmpint(reschedule_delta, ==, 575218741);
    g_assert_cmphex(state.ptimer.pending_interrupts & NV_PTIMER_INTR_0_ALARM,
                    ==, NV_PTIMER_INTR_0_ALARM);
    g_assert_true(mock_irq_raised);
}

/*
 * Set an alarm and advance time until it is reached, then trigger a timer
 * reschedule via a DENOMINATOR write. Verify that the alarm fires immediately,
 * IRQ is raised, and the timer is rescheduled for the next 2^32-tick epoch.
 */
static void test_nonzero_delta_reschedule_denominator(void)
{
    NV2AState state;
    setup_ptimer_state(&state);

    uint32_t t0 = (uint32_t)ptimer_read(&state, NV_PTIMER_TIME_0, 4);
    uint32_t target_alarm = (t0 + (1 << 5)) & 0xffffffe0;
    PTIMER_WRITE(NV_PTIMER_ALARM_0, target_alarm);

    PTIMER_ENABLE_INTERRUPT_EXPECT_CLEARED();

    mock_virtual_time_ns += 100;
    PTIMER_WRITE(NV_PTIMER_DENOMINATOR, 2);

    int64_t reschedule_delta = last_timer_mod_expire - mock_virtual_time_ns;
    g_assert_true(mock_timer_active);
    g_assert_cmpint(reschedule_delta, ==, 75218739);
    g_assert_cmphex(state.ptimer.pending_interrupts & NV_PTIMER_INTR_0_ALARM,
                    ==, NV_PTIMER_INTR_0_ALARM);
    g_assert_true(mock_irq_raised);
}

/*
 * Set an alarm and advance time until it is reached, then trigger a timer
 * reschedule via a TIME_0 write. Verify that the alarm fires immediately,
 * IRQ is raised, and the timer is rescheduled for the next 2^32-tick epoch.
 */
static void test_nonzero_delta_reschedule_time_0(void)
{
    NV2AState state;
    setup_ptimer_state(&state);

    uint32_t t0 = (uint32_t)ptimer_read(&state, NV_PTIMER_TIME_0, 4);
    uint32_t target_alarm = (t0 + (1 << 5)) & 0xffffffe0;
    PTIMER_WRITE(NV_PTIMER_ALARM_0, target_alarm);

    PTIMER_ENABLE_INTERRUPT_EXPECT_CLEARED();

    mock_virtual_time_ns += 100;
    uint32_t current_time0 = (uint32_t)ptimer_read(&state, NV_PTIMER_TIME_0, 4);
    PTIMER_WRITE(NV_PTIMER_TIME_0, current_time0);

    int64_t reschedule_delta = last_timer_mod_expire - mock_virtual_time_ns;
    g_assert_true(mock_timer_active);
    g_assert_cmpint(reschedule_delta, ==, 575218741);
    g_assert_cmphex(state.ptimer.pending_interrupts & NV_PTIMER_INTR_0_ALARM,
                    ==, NV_PTIMER_INTR_0_ALARM);
    g_assert_true(mock_irq_raised);
}

/*
 * Set an alarm and advance time until it is reached, then trigger a timer
 * reschedule via a TIME_1 write. Verify that the alarm fires immediately,
 * IRQ is raised, and the timer is rescheduled for the next 2^32-tick epoch.
 */
static void test_nonzero_delta_reschedule_time_1(void)
{
    NV2AState state;
    setup_ptimer_state(&state);

    uint32_t t0 = (uint32_t)ptimer_read(&state, NV_PTIMER_TIME_0, 4);
    uint32_t target_alarm = (t0 + (1 << 5)) & 0xffffffe0;
    PTIMER_WRITE(NV_PTIMER_ALARM_0, target_alarm);

    PTIMER_ENABLE_INTERRUPT_EXPECT_CLEARED();

    mock_virtual_time_ns += 100;
    uint32_t current_time1 = (uint32_t)ptimer_read(&state, NV_PTIMER_TIME_1, 4);
    PTIMER_WRITE(NV_PTIMER_TIME_1, current_time1);

    int64_t reschedule_delta = last_timer_mod_expire - mock_virtual_time_ns;
    g_assert_true(mock_timer_active);
    g_assert_cmpint(reschedule_delta, ==, 575218741);
    g_assert_cmphex(state.ptimer.pending_interrupts & NV_PTIMER_INTR_0_ALARM,
                    ==, NV_PTIMER_INTR_0_ALARM);
    g_assert_true(mock_irq_raised);
}

/*
 * Verify that writing 0 to NV_PTIMER_INTR_0 does not clear the pending
 * interrupt.
 */
static void test_interrupt_write_zero_does_not_clear(void)
{
    NV2AState state;
    setup_ptimer_state(&state);

    uint32_t t0 = (uint32_t)ptimer_read(&state, NV_PTIMER_TIME_0, 4);
    uint32_t target_alarm = (t0 + (1 << 5)) & 0xffffffe0;
    PTIMER_WRITE(NV_PTIMER_ALARM_0, target_alarm);
    PTIMER_ENABLE_INTERRUPT_EXPECT_CLEARED();

    mock_virtual_time_ns = last_timer_mod_expire;
    FIRE_ALARM_TIMER();

    g_assert_cmphex(state.ptimer.pending_interrupts & NV_PTIMER_INTR_0_ALARM,
                    ==, NV_PTIMER_INTR_0_ALARM);
    g_assert_true(mock_irq_raised);

    PTIMER_WRITE(NV_PTIMER_INTR_0, 0);
    g_assert_cmphex(state.ptimer.pending_interrupts & NV_PTIMER_INTR_0_ALARM,
                    ==, NV_PTIMER_INTR_0_ALARM);
    g_assert_true(mock_irq_raised);
}

/*
 * Verify that writing NV_PTIMER_INTR_0_ALARM to NV_PTIMER_INTR_0 clears the
 * pending interrupt and deasserts the IRQ.
 */
static void test_interrupt_clear_alarm(void)
{
    NV2AState state;
    setup_ptimer_state(&state);

    uint32_t t0 = (uint32_t)ptimer_read(&state, NV_PTIMER_TIME_0, 4);
    uint32_t target_alarm = (t0 + (1 << 5)) & 0xffffffe0;
    PTIMER_WRITE(NV_PTIMER_ALARM_0, target_alarm);
    PTIMER_ENABLE_INTERRUPT_EXPECT_CLEARED();

    mock_virtual_time_ns = last_timer_mod_expire;
    FIRE_ALARM_TIMER();

    g_assert_cmphex(state.ptimer.pending_interrupts & NV_PTIMER_INTR_0_ALARM,
                    ==, NV_PTIMER_INTR_0_ALARM);
    g_assert_true(mock_irq_raised);

    PTIMER_CLEAR_INTERRUPT();
    g_assert_cmphex(state.ptimer.pending_interrupts & NV_PTIMER_INTR_0_ALARM,
                    ==, 0);
    g_assert_false(mock_irq_raised);
}

/*
 * Verify that writing NV_PTIMER_NUMERATOR while the timer is inactive does not
 * activate or schedule the timer.
 */
static void test_inactive_timer_write_numerator(void)
{
    NV2AState state;
    setup_ptimer_state(&state);

    PTIMER_WRITE(NV_PTIMER_INTR_EN_0, 0);
    g_assert_false(mock_timer_active);

    PTIMER_WRITE(NV_PTIMER_NUMERATOR, 2);
    g_assert_false(mock_timer_active);
    g_assert_false(mock_irq_raised);
}

/*
 * Verify that writing NV_PTIMER_DENOMINATOR while the timer is inactive does
 * not activate or schedule the timer.
 */
static void test_inactive_timer_write_denominator(void)
{
    NV2AState state;
    setup_ptimer_state(&state);

    PTIMER_WRITE(NV_PTIMER_INTR_EN_0, 0);
    g_assert_false(mock_timer_active);

    PTIMER_WRITE(NV_PTIMER_DENOMINATOR, 2);
    g_assert_false(mock_timer_active);
    g_assert_false(mock_irq_raised);
}

/*
 * Verify that writing NV_PTIMER_TIME_0 while the timer is inactive does not
 * activate or schedule the timer.
 */
static void test_inactive_timer_write_time_0(void)
{
    NV2AState state;
    setup_ptimer_state(&state);

    PTIMER_WRITE(NV_PTIMER_INTR_EN_0, 0);
    g_assert_false(mock_timer_active);

    uint32_t t0 = (uint32_t)ptimer_read(&state, NV_PTIMER_TIME_0, 4);
    PTIMER_WRITE(NV_PTIMER_TIME_0, t0 + 100);
    g_assert_false(mock_timer_active);
    g_assert_false(mock_irq_raised);
}

/*
 * Verify that writing NV_PTIMER_TIME_1 while the timer is inactive does not
 * activate or schedule the timer.
 */
static void test_inactive_timer_write_time_1(void)
{
    NV2AState state;
    setup_ptimer_state(&state);

    PTIMER_WRITE(NV_PTIMER_INTR_EN_0, 0);
    g_assert_false(mock_timer_active);

    uint32_t t1 = (uint32_t)ptimer_read(&state, NV_PTIMER_TIME_1, 4);
    PTIMER_WRITE(NV_PTIMER_TIME_1, t1 + 1);
    g_assert_false(mock_timer_active);
    g_assert_false(mock_irq_raised);
}


/*
 * Set an alarm with alarm interrupts disabled. Verify that the QEMU timer
 * is disarmed and does not run.
 */
static void test_timer_disarmed_when_interrupt_disabled(void)
{
    NV2AState state;
    setup_ptimer_state(&state);

    PTIMER_WRITE(NV_PTIMER_ALARM_0, 0);
    PTIMER_ENABLE_INTERRUPT_EXPECT_CLEARED();
    g_assert_true(mock_timer_active);
    g_assert_false(mock_irq_raised);

    PTIMER_WRITE(NV_PTIMER_INTR_EN_0, 0);
    g_assert_false(mock_timer_active);
    g_assert_false(mock_irq_raised);

    uint32_t t0 = (uint32_t)ptimer_read(&state, NV_PTIMER_TIME_0, 4);
    uint32_t target_alarm = (t0 + (100 << 5)) & 0xffffffe0;
    PTIMER_WRITE(NV_PTIMER_ALARM_0, target_alarm);

    g_assert_false(mock_timer_active);
    g_assert_false(mock_irq_raised);
}

/*
 * Set an alarm with interrupts disabled and advance time past the target.
 * Verify that polling NV_PTIMER_INTR_0 detects that the alarm has occurred even
 * when the timer is inactive, and IRQ is not raised while masked.
 */
static void test_polling_without_timer(void)
{
    NV2AState state;
    setup_ptimer_state(&state);

    PTIMER_WRITE(NV_PTIMER_INTR_EN_0, 0);

    uint32_t t0 = (uint32_t)ptimer_read(&state, NV_PTIMER_TIME_0, 4);
    uint32_t target_alarm = (t0 + (10 << 5)) & 0xffffffe0;
    PTIMER_WRITE(NV_PTIMER_ALARM_0, target_alarm);

    uint32_t intr = (uint32_t)ptimer_read(&state, NV_PTIMER_INTR_0, 4);
    g_assert_cmphex(intr & NV_PTIMER_INTR_0_ALARM, ==, 0);
    g_assert_false(mock_irq_raised);

    mock_virtual_time_ns += 1000;

    intr = (uint32_t)ptimer_read(&state, NV_PTIMER_INTR_0, 4);
    g_assert_cmphex(intr & NV_PTIMER_INTR_0_ALARM, ==, NV_PTIMER_INTR_0_ALARM);
    g_assert_false(mock_irq_raised);
}

/*
 * Verify that an alarm set to 0 still fires when the alarm interrupt is enabled
 * and the timer wraps to 0.
 */
static void test_zero_alarm_fires(void)
{
    NV2AState state;
    setup_ptimer_state(&state);

    PTIMER_WRITE(NV_PTIMER_ALARM_0, 0);
    PTIMER_ENABLE_INTERRUPT_EXPECT_CLEARED();
    g_assert_true(mock_timer_active);
    g_assert_false(mock_irq_raised);

    mock_virtual_time_ns = last_timer_mod_expire;
    FIRE_ALARM_TIMER();

    g_assert_cmphex(state.ptimer.pending_interrupts & NV_PTIMER_INTR_0_ALARM,
                    ==, NV_PTIMER_INTR_0_ALARM);
    g_assert_true(mock_irq_raised);
}

/*
 * Verify that polling NV_PTIMER_INTR_0 detects an alarm set to 0 when
 * interrupts are disabled and TIME_0 wraps past 0.
 */
static void test_polling_zero_alarm_without_timer(void)
{
    NV2AState state;
    setup_ptimer_state(&state);

    PTIMER_WRITE(NV_PTIMER_ALARM_0, 0);
    PTIMER_WRITE(NV_PTIMER_INTR_EN_0, 0);
    g_assert_false(mock_timer_active);
    g_assert_false(mock_irq_raised);

    uint32_t intr = (uint32_t)ptimer_read(&state, NV_PTIMER_INTR_0, 4);
    g_assert_cmphex(intr & NV_PTIMER_INTR_0_ALARM, ==, 0);
    g_assert_false(mock_irq_raised);

    uint32_t t1 = (uint32_t)ptimer_read(&state, NV_PTIMER_TIME_1, 4);
    PTIMER_WRITE(NV_PTIMER_TIME_1, t1 + 1);

    intr = (uint32_t)ptimer_read(&state, NV_PTIMER_INTR_0, 4);
    g_assert_cmphex(intr & NV_PTIMER_INTR_0_ALARM, ==, NV_PTIMER_INTR_0_ALARM);
    g_assert_false(mock_irq_raised);
}

/*
 * Verify that advancing time past multiple 2^32-tick epochs normalizes directly
 * to the first future alarm epoch rather than advancing one epoch at a time
 * and creating bursts of immediate (0 / 1-ns) reschedules.
 */
static void test_alarm_multiple_missed_periods(void)
{
    NV2AState state;
    setup_ptimer_state(&state);

    PTIMER_WRITE(NV_PTIMER_INTR_EN_0, 0);
    uint32_t t0 = (uint32_t)ptimer_read(&state, NV_PTIMER_TIME_0, 4);
    uint32_t target_alarm = (t0 + (100 << 5)) & 0xffffffe0;
    PTIMER_WRITE(NV_PTIMER_ALARM_0, target_alarm);

    uint32_t t1 = (uint32_t)ptimer_read(&state, NV_PTIMER_TIME_1, 4);
    PTIMER_WRITE(NV_PTIMER_TIME_1, t1 + 3);

    PTIMER_WRITE(NV_PTIMER_INTR_EN_0, NV_PTIMER_INTR_0_ALARM);
    g_assert_cmphex(state.ptimer.pending_interrupts & NV_PTIMER_INTR_0_ALARM,
                    ==, NV_PTIMER_INTR_0_ALARM);
    g_assert_true(mock_irq_raised);

    g_assert_true(mock_timer_active);
    int64_t reschedule_delta = last_timer_mod_expire - mock_virtual_time_ns;
    g_assert_cmpint(reschedule_delta, >, 1);

    PTIMER_CLEAR_INTERRUPT();
    g_assert_false(mock_irq_raised);

    mock_virtual_time_ns = last_timer_mod_expire;
    FIRE_ALARM_TIMER();
    g_assert_cmphex(state.ptimer.pending_interrupts & NV_PTIMER_INTR_0_ALARM,
                    ==, NV_PTIMER_INTR_0_ALARM);
    g_assert_true(mock_irq_raised);
}

/*
 * Verify that when interrupts are disabled and time advances past multiple
 * 2^32-tick epochs, reading NV_PTIMER_INTR_0 detects the alarm, does not
 * start a timer while interrupts are disabled, normalizes alarm_time to the
 * future epoch, and subsequently unmasking schedules the timer for the future.
 */
static void test_polling_after_multiple_missed_periods_while_masked(void)
{
    NV2AState state;
    setup_ptimer_state(&state);

    PTIMER_WRITE(NV_PTIMER_INTR_EN_0, 0);
    uint32_t t0 = (uint32_t)ptimer_read(&state, NV_PTIMER_TIME_0, 4);
    uint32_t target_alarm = (t0 + (100 << 5)) & 0xffffffe0;
    PTIMER_WRITE(NV_PTIMER_ALARM_0, target_alarm);

    uint32_t t1 = (uint32_t)ptimer_read(&state, NV_PTIMER_TIME_1, 4);
    PTIMER_WRITE(NV_PTIMER_TIME_1, t1 + 5);

    uint32_t intr = (uint32_t)ptimer_read(&state, NV_PTIMER_INTR_0, 4);
    g_assert_cmphex(intr & NV_PTIMER_INTR_0_ALARM, ==, NV_PTIMER_INTR_0_ALARM);
    g_assert_false(mock_timer_active);
    g_assert_false(mock_irq_raised);

    PTIMER_CLEAR_INTERRUPT();

    intr = (uint32_t)ptimer_read(&state, NV_PTIMER_INTR_0, 4);
    g_assert_cmphex(intr & NV_PTIMER_INTR_0_ALARM, ==, 0);
    g_assert_false(mock_irq_raised);

    PTIMER_ENABLE_INTERRUPT_EXPECT_CLEARED();
    g_assert_true(mock_timer_active);
    g_assert_false(mock_irq_raised);

    int64_t reschedule_delta = last_timer_mod_expire - mock_virtual_time_ns;
    g_assert_cmpint(reschedule_delta, >, 1);

    mock_virtual_time_ns = last_timer_mod_expire;
    FIRE_ALARM_TIMER();
    g_assert_cmphex(state.ptimer.pending_interrupts & NV_PTIMER_INTR_0_ALARM,
                    ==, NV_PTIMER_INTR_0_ALARM);
    g_assert_true(mock_irq_raised);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/xbox/nv2a/ptimer/single_tick_alarm",
                    test_single_tick_alarm);
    g_test_add_func("/xbox/nv2a/ptimer/single_tick_alarm_non_unit_ratio",
                    test_single_tick_alarm_non_unit_ratio);
    g_test_add_func("/xbox/nv2a/ptimer/alarm_sweep", test_alarm_sweep);
    g_test_add_func("/xbox/nv2a/ptimer/alarm_sweep_non_unit_ratio",
                    test_alarm_sweep_non_unit_ratio);
    g_test_add_func("/xbox/nv2a/ptimer/nonzero_delta_reschedule_numerator",
                    test_nonzero_delta_reschedule_numerator);
    g_test_add_func("/xbox/nv2a/ptimer/nonzero_delta_reschedule_denominator",
                    test_nonzero_delta_reschedule_denominator);
    g_test_add_func("/xbox/nv2a/ptimer/nonzero_delta_reschedule_time_0",
                    test_nonzero_delta_reschedule_time_0);
    g_test_add_func("/xbox/nv2a/ptimer/nonzero_delta_reschedule_time_1",
                    test_nonzero_delta_reschedule_time_1);
    g_test_add_func("/xbox/nv2a/ptimer/inactive_timer_write_numerator",
                    test_inactive_timer_write_numerator);
    g_test_add_func("/xbox/nv2a/ptimer/inactive_timer_write_denominator",
                    test_inactive_timer_write_denominator);
    g_test_add_func("/xbox/nv2a/ptimer/inactive_timer_write_time_0",
                    test_inactive_timer_write_time_0);
    g_test_add_func("/xbox/nv2a/ptimer/inactive_timer_write_time_1",
                    test_inactive_timer_write_time_1);
    g_test_add_func("/xbox/nv2a/ptimer/interrupt_write_zero_does_not_clear",
                    test_interrupt_write_zero_does_not_clear);
    g_test_add_func("/xbox/nv2a/ptimer/interrupt_clear_alarm",
                    test_interrupt_clear_alarm);
    g_test_add_func("/xbox/nv2a/ptimer/timer_disarmed_when_interrupt_disabled",
                    test_timer_disarmed_when_interrupt_disabled);
    g_test_add_func("/xbox/nv2a/ptimer/polling_without_timer",
                    test_polling_without_timer);
    g_test_add_func("/xbox/nv2a/ptimer/zero_alarm_fires",
                    test_zero_alarm_fires);
    g_test_add_func("/xbox/nv2a/ptimer/polling_zero_alarm_without_timer",
                    test_polling_zero_alarm_without_timer);
    g_test_add_func("/xbox/nv2a/ptimer/multiple_missed_periods",
                    test_alarm_multiple_missed_periods);
    g_test_add_func(
        "/xbox/nv2a/ptimer/polling_after_multiple_missed_periods_while_masked",
        test_polling_after_multiple_missed_periods_while_masked);

    return g_test_run();
}
