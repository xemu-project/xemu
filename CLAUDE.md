# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

xemu is an original Xbox emulator built as a **fork of QEMU**. It uses QEMU's
i386 softmmu target (the Xbox CPU is a Pentium III) plus a large body of
Xbox-specific hardware emulation under `hw/xbox/`. Most of the QEMU tree
(`block/`, `migration/`, `net/`, `target/`, `tcg/`, etc.) is upstream QEMU and
is rarely touched — xemu changes concentrate in `hw/xbox/` and `ui/xemu-*`.

The user-facing binary is `qemu-system-i386` (on Windows `qemu-system-i386w.exe`),
repackaged into `xemu` / `xemu.app` / `xemu.exe`.

## Building

**Always build via `./build.sh`, not raw `configure`/`make`.** It sets the
`-DXBOX=1` define, restricts the target list to `i386-softmmu`, handles the
macOS SDK/dylib bundling, and runs the platform-specific packaging step.

```bash
./build.sh                # release build for the host platform
./build.sh --debug        # adds -DXEMU_DEBUG_BUILD=1, --enable-debug, log trace backend
./build.sh -j8            # override parallel job count
```

- Output binary: `build/qemu-system-i386`; packaged app: `dist/xemu.app` (macOS).
- Build log is written to `build.log`.
- On macOS, `build.sh` calls `scripts/download-macos-libs.py <arch>` to fetch
  prebuilt dependency dylibs into `macos-libs/<arch>/` and bundles them with
  `dylibbundler`. Apple Silicon targets macOS 14.0+, Intel targets 12.7.5+.
- Incremental rebuilds: once configured, `ninja -C build qemu-system-i386` (or
  `make -C build qemu-system-i386`) is much faster than re-running `build.sh`,
  which reconfigures from scratch.

## Running

The emulator needs an MCPX boot ROM, a flash BIOS, an EEPROM, and an HDD image.
For this checkout those assets live in `../xemu-files/` (`bios/`, `mcpx/`,
`hdd/`, `Games/`). xemu is normally launched from the app and configured through
its GUI, which persists settings to a `toml` config file; renderer and other
options below can also be set there.

## Architecture

### Xbox hardware (`hw/xbox/`)
- `xbox.c` / `xbox_pci.c` — machine type, chipset (northbridge/southbridge), wiring.
- `smbus_*.c`, `lpc47m157.c` — SMBus devices and Super I/O.
- `xid*.c` — USB Xbox gamepad (XID) emulation.
- `mcpx/` — the MCPX southbridge media chip: `apu/` (audio DSP — note the new
  DSP56300 emulator with a JIT execution engine), `nvnet/` (ethernet), `aci.c`.

### NV2A GPU (`hw/xbox/nv2a/`) — the performance-critical subsystem
The NV2A is a GeForce 3-class GPU. This is where the bulk of emulation cost for
games like Halo 2 lives. Structure:
- `nv2a.c` — device/MMIO top level. `pfifo.c` — command FIFO (pushbuffer)
  processing. `pmc.c`, `pcrtc.c`, `pramdac.c`, `pvideo.c`, etc. — the smaller
  NV2A functional engines (named after real hardware blocks).
- `pgraph/` — the 3D graphics pipeline (PGRAPH engine), the hot path:
  - `pgraph.c` — engine core, method dispatch, and the **renderer registration /
    switching** machinery (`pgraph_renderer_register`, default-renderer selection).
  - `gl/` and `vk/` — two interchangeable backend renderers (OpenGL and Vulkan).
    Each implements the same `PGRAPHRenderer` interface: `renderer.c`, `draw.c`,
    `surface.c`, `texture.c`, `shaders.c`, `vertex.c`, `display.c`, `blit.c`.
    `vk/` additionally has `surface-compute.c`, `image.c`, `buffer.c`,
    `command.c`, `instance.c`. `null/` is a no-op renderer.
  - `glsl/` — generates GLSL for translated Xbox vertex/pixel shaders (shared by
    both backends; Vulkan compiles the GLSL to SPIR-V).
  - `texture.c`, `swizzle.c`, `s3tc.c` — texture decode/swizzle/DXT.
  - `profile.c` + `debug.h` — the `g_nv2a_stats` profiler and the
    `NV2A_PROF_*` counter set (draw calls, surface up/downloads, shader/pipeline
    gen vs. cache hits, flip stalls). This is the primary tool for diagnosing
    GPU-side performance.

**Renderer selection:** controlled by `display.renderer` in the config
(`NULL` / `OPENGL` / `VULKAN`). Config default is `OPENGL`; if the configured
renderer isn't compiled in, `get_default_renderer()` falls back (OpenGL, then
Vulkan, then NULL). On macOS, OpenGL is deprecated by Apple and the Vulkan
backend runs over MoltenVK — renderer choice is the first lever for macOS perf.

### Config schema (`config_spec.yml`)
xemu's settings are declared in `config_spec.yml` and **code-generated** into
`g_config` (referenced as e.g. `g_config.display.renderer`,
`g_config.perf.cache_shaders`). To add or change a setting, edit
`config_spec.yml` rather than hand-writing struct fields. Performance-relevant
keys: `display.renderer`, `display.quality.surface_scale`, `display.filtering`,
`display.window.vsync`, `perf.hard_fpu`, `perf.cache_shaders`.

### UI / integration layer (`ui/xemu-*`)
xemu-specific glue distinct from upstream QEMU UI: `xemu.c` (main loop/SDL),
`xemu-settings.cc` (config load/save), `xemu-input.c`/`xemu-controllers.cc`
(controller mapping), `xemu-snapshots.c`, `xemu-net.c`, `xemu-os-utils-*`
(per-OS helpers, `.m` for macOS). The in-app menus/overlays use ImGui.

## Conventions
- Style follows QEMU: see `.clang-format` and `.clang-tidy`. Xbox/xemu C files
  generally follow the surrounding QEMU style.
- `.git-blame-ignore-revs` lists bulk-reformat commits — use it when running
  `git blame` so authorship isn't masked by reformatting.
- The `roms/`, `tests/lcitool/libvirt-ci`, and most `subprojects/` are git
  submodules / vendored; treat them as upstream and avoid editing.
