//
// xemu Debug Tools - scoped guest pause helper
//
#include "qemu/osdep.h"

#include "guest-pause-guard.hh"

extern "C" {
#include "system/runstate.h"
}

XemuDebugGuestPauseGuard::XemuDebugGuestPauseGuard()
    : m_was_running(runstate_is_running())
{
    if (!m_was_running) {
        return;
    }

    const int stop_result = vm_stop(RUN_STATE_PAUSED);
    const bool stopped = !runstate_is_running();
    // If vm_stop() reports an error after already transitioning the VM to
    // paused, we still own the matching resume. Callers must not perform
    // coherent reads unless the stop itself succeeded.
    m_resume = stopped;
    m_valid = stop_result == 0 && stopped;
}

XemuDebugGuestPauseGuard::~XemuDebugGuestPauseGuard()
{
    Resume();
}

void XemuDebugGuestPauseGuard::Resume()
{
    if (m_resume) {
        vm_start();
        m_resume = false;
    }
}
