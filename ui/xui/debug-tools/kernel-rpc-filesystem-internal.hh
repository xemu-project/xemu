//
// xemu Guest Kernel RPC filesystem private shared helpers
//
#pragma once

#include <filesystem>
#include <string>

namespace XemuKernelFs {

inline bool ReadHostWriteTime(const std::filesystem::path &path,
                       int64_t &write_time,
                       std::string &error)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::file_time_type value = fs::last_write_time(path, ec);
    if (ec) {
        error = "Could not read host item write time: " + path.u8string();
        return false;
    }
    write_time = static_cast<int64_t>(value.time_since_epoch().count());
    return true;
}

} // namespace XemuKernelFs
