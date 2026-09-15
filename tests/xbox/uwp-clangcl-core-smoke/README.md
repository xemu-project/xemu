# clang-cl/MSVC ABI UWP core smoke

This is a focused build gate for the Xbox Series X|S Developer Mode port. It
does not register a renderer, create an MSIX, or deploy to a console. The
smoke executable is an x64 AppContainer binary built with `clang-cl`,
`lld-link`, the installed MSVC CRT (`/MD`), the Windows SDK, WRL, and
C++/WinRT headers. It deliberately does not use `/ZW`.

The source exercises the current `qemu/compiler.h`, `qemu/atomic.h`,
`qemu/bswap.h`, and `qemu/int128.h` headers, plus the small NV2A D3D11 clear
bridge (`d3d11/clear.cc` and the renderer-neutral `pgraph/clear.c`). Runtime
checks cover 32-bit and pointer-width atomics, barriers, byte swaps, Int128,
the clear rectangle helper, WRL, and C++/WinRT apartment initialization.

## Build on the Windows VM

The script consumes a real Meson build directory. It never writes
`config-host.h`, a target config, or typedef/configuration stubs. Configure a
Windows x64 build first (with at least `x86_64-softmmu` enabled), then run:

```powershell
.\scripts\build.ps1 -RepositoryRoot C:\src\xxemu `
  -BuildDirectory C:\src\xxemu\build-win
```

`-BuildDirectory` must be the current repository's authentic Meson build. It
must contain `meson-private/coredata.dat`, generated `config-host.h` and
`config-host.mak`, `SRC_PATH` pointing at the supplied checkout,
`TARGET_DIRS` containing the exact `x86_64-softmmu` target, and exactly one
generated `x86_64-softmmu-config-target.h` with matching target markers. Copied
or stale/fake headers are rejected. A missing clang-cl, lld-link, Visual Studio
C++ workload, Windows SDK, C++/WinRT header, or import-audit tool produces an
explicit prerequisite error; LLVM is never installed by this repository
script.

The output defaults to a unique temporary directory. Pass
`-OutputDirectory` to retain the executable and `imports.txt`. The import
audit rejects MinGW, libgcc, libstdc++, and libatomic dependencies, which is
the guard against silently leaving the MSVC/UWP ABI. `CONFIG_ATOMIC128` is
read from the real generated host config. When enabled, the smoke includes
`qemu/atomic128.h`; when disabled, it reports that only ordinary non-atomic
`Int128` arithmetic is tested. It does not claim an atomic128 fallback.

The script intentionally does not execute the raw AppContainer executable:
without an MSIX package identity that would be a misleading result. Runtime
checks in the binary become actionable after packaging/signing and launching
the MSIX in Xbox Dev Mode. This smoke itself only proves compile, link, and
import hygiene.

> Agent declaration: implementation assisted by OpenAI Codex GPT-5 / Luna subagent.
