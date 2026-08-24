//
// xemu Debug Tools - shared Guest Kernel RPC status/offset helpers
//
// Copyright (C) 2026 xemu contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
#pragma once

#include <cstdint>

namespace XemuGuestKernelRpcStatus {

constexpr uint32_t kDeleteIrqlOffset = 0x1000u;
constexpr uint32_t kImportIrqlOffset = 0x1000u;
constexpr uint64_t kSafePointSampleIntervalMs = 25u;

inline const char *IrqlName(uint32_t irql)
{
    switch (irql) {
    case 0: return "PASSIVE_LEVEL";
    case 1: return "APC_LEVEL";
    case 2: return "DISPATCH_LEVEL";
    default: return "higher IRQL";
    }
}

inline const char *NtStatusName(uint32_t status)
{
    switch (status) {
    case 0x00000000u: return "STATUS_SUCCESS";
    case 0x00000103u: return "STATUS_PENDING";
    case 0xc000000du: return "STATUS_INVALID_PARAMETER";
    case 0xc0000022u: return "STATUS_ACCESS_DENIED";
    case 0xc0000033u: return "STATUS_OBJECT_NAME_INVALID";
    case 0xc0000034u: return "STATUS_OBJECT_NAME_NOT_FOUND";
    case 0xc0000035u: return "STATUS_OBJECT_NAME_COLLISION";
    case 0xc000003au: return "STATUS_OBJECT_PATH_NOT_FOUND";
    case 0xc0000043u: return "STATUS_SHARING_VIOLATION";
    case 0xc000007fu: return "STATUS_DISK_FULL";
    case 0xc0000101u: return "STATUS_DIRECTORY_NOT_EMPTY";
    case 0xc0000121u: return "STATUS_CANNOT_DELETE";
    default: return "";
    }
}

} // namespace XemuGuestKernelRpcStatus
