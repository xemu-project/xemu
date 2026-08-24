#pragma once

// Emulate the Windows QEMU compatibility macro closely enough to catch C++
// header leakage into <fstream>. guest-pause-guard.cc may see this macro; the
// public guest-pause header must not expose it to its consumers.
#define close qemu_close_wrap
