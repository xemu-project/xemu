//
// xemu Debug Tools - scoped guest pause helper
//
// Keeps short debugger/cheat transactions symmetric across every early return:
// a running VM is paused on construction and resumed exactly once on release or
// destruction. A VM that was already paused/stopped is left untouched.
//
#pragma once

// Keep QEMU's osdep/runstate headers out of this C++ interface. On Windows,
// os-win32.h defines close as qemu_close_wrap; leaking that macro through a
// header before <fstream> can rename std::basic_filebuf::close() and break LTO
// linking. The implementation owns the QEMU header environment instead.
class XemuDebugGuestPauseGuard
{
public:
    XemuDebugGuestPauseGuard();
    ~XemuDebugGuestPauseGuard();

    bool IsValid() const { return m_valid; }

    void Resume();

    XemuDebugGuestPauseGuard(const XemuDebugGuestPauseGuard &) = delete;
    XemuDebugGuestPauseGuard &operator=(const XemuDebugGuestPauseGuard &) = delete;

private:
    bool m_was_running = false;
    bool m_resume = false;
    bool m_valid = true;
};
