// v2.87 current regression ownership.
#include "guest-pause-guard.hh"

// This must be safe after including the public pause-guard header. On Windows,
// a leaked qemu/osdep.h would define `close` as `qemu_close_wrap` and rename
// std::basic_filebuf::close() while <fstream> is parsed.
#include <fstream>

int main()
{
    std::ifstream stream;
    stream.close();
    return 0;
}
