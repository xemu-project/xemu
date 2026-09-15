/*
 * xemu clang-cl/MSVC ABI UWP core smoke test.
 *
 * Copyright (C) 2026 xemu Project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This executable intentionally has no /ZW dependency.  It exercises the
 * QEMU compiler, atomic, byte-swap, and Int128 headers together with the
 * smallest checked-in NV2A D3D11 bridge slice.  The PowerShell driver links
 * it as an AppContainer executable with the MSVC CRT.
 */

#include "qemu/osdep.h"

#include "qemu/atomic.h"
#include "qemu/bswap.h"
#include "qemu/int128.h"
#if defined(CONFIG_ATOMIC128) && CONFIG_ATOMIC128
#include "qemu/atomic128.h"
#endif

#include "hw/xbox/nv2a/pgraph/clear.h"
#include "hw/xbox/nv2a/pgraph/d3d11/clear.h"

#include <d3d11.h>
#include <windows.h>
#include <wrl/client.h>

#include <winrt/base.h>

#include <cstdint>
#include <cstdio>

namespace {

bool Check(bool condition, const char *name)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", name);
    }
    return condition;
}

bool RunCoreSmoke()
{
    bool ok = true;

    uint32_t word = 0;
    qatomic_set(&word, UINT32_C(0x12345678));
    ok &= Check(qatomic_read(&word) == UINT32_C(0x12345678),
                "32-bit qatomic load/store");
    ok &= Check(qatomic_cmpxchg(&word, UINT32_C(0x12345678),
                                UINT32_C(0x87654321)) == UINT32_C(0x12345678),
                "32-bit qatomic compare/exchange");
    ok &= Check(qatomic_read(&word) == UINT32_C(0x87654321),
                "32-bit qatomic compare/exchange result");

    int first = 1;
    int second = 2;
    void *pointer = nullptr;
    qatomic_set(&pointer, &first);
    ok &= Check(qatomic_read(&pointer) == &first,
                "pointer-width qatomic load/store");
    ok &= Check(qatomic_cmpxchg(&pointer, &first, &second) == &first,
                "pointer-width qatomic compare/exchange");
    ok &= Check(qatomic_read(&pointer) == &second,
                "pointer-width qatomic compare/exchange result");

    uintptr_t pointer_bits = 0;
    qatomic_set(&pointer_bits, UINTPTR_C(0x1234));
    ok &= Check(qatomic_read(&pointer_bits) == UINTPTR_C(0x1234),
                "uintptr_t qatomic load/store");
    ok &= Check(qatomic_cmpxchg(&pointer_bits, UINTPTR_C(0x1234),
                                UINTPTR_C(0x5678)) == UINTPTR_C(0x1234),
                "uintptr_t qatomic compare/exchange");

    smp_wmb();
    smp_rmb();
    smp_mb();
    ok &= Check(true, "compiler and memory barriers");

    ok &= Check(bswap16(UINT16_C(0x1234)) == UINT16_C(0x3412),
                "16-bit byte swap");
    ok &= Check(bswap32(UINT32_C(0x12345678)) == UINT32_C(0x78563412),
                "32-bit byte swap");
    ok &= Check(bswap64(UINT64_C(0x0123456789abcdef)) ==
                    UINT64_C(0xefcdab8967452301),
                "64-bit byte swap");

    const Int128 value = int128_make128(UINT64_C(0x0123456789abcdef),
                                        UINT64_C(0x1122334455667788));
    ok &= Check(int128_getlo(value) == UINT64_C(0x0123456789abcdef),
                "Int128 low half");
    ok &= Check(static_cast<uint64_t>(int128_gethi(value)) ==
                    UINT64_C(0x1122334455667788),
                "Int128 high half");
    const Int128 swapped = bswap128(value);
    ok &= Check(int128_getlo(swapped) == UINT64_C(0x8877665544332211) &&
                    static_cast<uint64_t>(int128_gethi(swapped)) ==
                        UINT64_C(0xefcdab8967452301),
                "Int128 byte swap");

    PGRAPHClearRect input = { 0, 0, 15, 15 };
    PGRAPHClearRect clipped = {};
    ok &= Check(pgraph_clear_rect_clamp(&input, 8, 8, &clipped) &&
                    clipped.right == 7 && clipped.bottom == 7,
                "NV2A clear rectangle bridge");

    xemu::D3D11ClearPrimitive clear(nullptr, nullptr);
    const xemu::D3D11ClearParams params = {};
    const xemu::D3D11ClearResult result = clear.Clear(params, nullptr, nullptr);
    ok &= Check(result.status == xemu::D3D11ClearStatus::Invalid,
                "D3D11 clear primitive linkage");

    Microsoft::WRL::ComPtr<IUnknown> unknown;
    ok &= Check(unknown.Get() == nullptr, "WRL ComPtr ABI");

    try {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        winrt::uninit_apartment();
        ok &= Check(true, "C++/WinRT apartment ABI");
    } catch (...) {
        ok &= Check(false, "C++/WinRT apartment ABI");
    }

    return ok;
}

} // namespace

int main()
{
    std::printf("xemu clang-cl UWP core smoke: pointer_bits=%zu\n",
                sizeof(void *) * 8);
#if defined(CONFIG_ATOMIC128) && CONFIG_ATOMIC128
    std::printf(
        "atomic128=enabled (header compiled; no atomic op exercised)\n");
#else
    std::printf(
        "atomic128=disabled; Int128 arithmetic-only (no atomic fallback)\n");
#endif
    return RunCoreSmoke() ? 0 : 1;
}
