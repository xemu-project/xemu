// v2.87 current regression ownership.
#include "guest-pause-guard.hh"

extern "C" {
#include "system/runstate.h"
}

#include <cassert>

static bool g_running;
static int g_stop_calls;
static int g_start_calls;
static int g_stop_result;
static bool g_stop_changes_state;

bool runstate_is_running(void)
{
    return g_running;
}

int vm_stop(RunState state)
{
    assert(state == RUN_STATE_PAUSED);
    ++g_stop_calls;
    if (g_stop_changes_state) {
        g_running = false;
    }
    return g_stop_result;
}

void vm_start(void)
{
    ++g_start_calls;
    g_running = true;
}

static void reset(bool running)
{
    g_running = running;
    g_stop_calls = 0;
    g_start_calls = 0;
    g_stop_result = 0;
    g_stop_changes_state = true;
}

int main()
{
    reset(true);
    {
        XemuDebugGuestPauseGuard pause;
        assert(pause.IsValid());
        assert(!g_running);
        assert(g_stop_calls == 1);
        assert(g_start_calls == 0);
    }
    assert(g_running);
    assert(g_stop_calls == 1);
    assert(g_start_calls == 1);

    reset(false);
    {
        XemuDebugGuestPauseGuard pause;
        assert(pause.IsValid());
        assert(!g_running);
        assert(g_stop_calls == 0);
        assert(g_start_calls == 0);
    }
    assert(!g_running);
    assert(g_stop_calls == 0);
    assert(g_start_calls == 0);

    reset(true);
    {
        XemuDebugGuestPauseGuard pause;
        pause.Resume();
        assert(g_running);
        assert(g_start_calls == 1);
        pause.Resume();
        assert(g_start_calls == 1);
    }
    assert(g_start_calls == 1);

    reset(true);
    {
        XemuDebugGuestPauseGuard outer;
        assert(!g_running);
        {
            XemuDebugGuestPauseGuard inner;
            assert(g_stop_calls == 1);
            assert(g_start_calls == 0);
        }
        assert(!g_running);
        assert(g_start_calls == 0);
    }
    assert(g_running);
    assert(g_stop_calls == 1);
    assert(g_start_calls == 1);

    // vm_stop failure without a state transition: fail closed and do not resume.
    reset(true);
    g_stop_result = -1;
    g_stop_changes_state = false;
    {
        XemuDebugGuestPauseGuard pause;
        assert(!pause.IsValid());
        assert(g_running);
    }
    assert(g_running);
    assert(g_start_calls == 0);

    // vm_stop may report failure after pausing. The transaction is invalid,
    // but the guard still owns and performs the matching resume.
    reset(true);
    g_stop_result = -1;
    {
        XemuDebugGuestPauseGuard pause;
        assert(!pause.IsValid());
        assert(!g_running);
    }
    assert(g_running);
    assert(g_start_calls == 1);

    return 0;
}
