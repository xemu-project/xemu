# OpenMidway

OpenMidway is a fork of [xemu](https://xemu.app) and QEMU that aims to
make original Xbox emulation easier to use for everyday players while
still improving compatibility over time.

The project is currently centered on four practical goals:

- get people to a first successful boot quickly,
- improve the Windows experience,
- make System Link / LAN play easier to set up,
- and keep pushing more titles from broken to playable.

> OpenMidway does **not** ship Xbox firmware, hard-drive contents, or
> game data. You must provide the files you are legally allowed to use.

---

## Project status

OpenMidway is an active fork with its own direction, documentation, and
roadmap. The project still builds on upstream xemu/QEMU work, but it is
explicitly prioritizing multiplayer usability, practical setup flow, and
real-world performance.

- **Vision:** [docs/vision.md](docs/vision.md)
- **Roadmap:** [ROADMAP.md](ROADMAP.md)
- **Architecture overview:** [docs/architecture.md](docs/architecture.md)
- **Upstream differences:** [FORK_DIFF.md](FORK_DIFF.md)

If you just want to get running, start with the sections below instead
of reading the full developer documentation first.

---

## Start here

| If you want to... | Read this first |
|---|---|
| Boot OpenMidway as quickly as possible | [Quick start](#quick-start) |
| Build on Windows 11 | [docs/windows-11.md](docs/windows-11.md) |
| Build on Linux or another development platform | [Build from source](#build-from-source) |
| Understand System Link goals | [docs/networking.md](docs/networking.md) |
| Report or triage compatibility | [docs/compatibility-triage.md](docs/compatibility-triage.md) |
| Contribute code or documentation | [Contributing](#contributing) |

---

## What you need before first boot

Before launching OpenMidway, have these ready:

- an original Xbox MCPX boot ROM / BIOS dump,
- an Xbox HDD image or game image you are legally allowed to use,
- a controller or keyboard mapping that works on your platform,
- and either a local source build or a build artifact provided for your
  branch or release.

Keeping those files in one clearly named folder makes first-run setup
and troubleshooting much easier.

---

## Quick start

Use this order for the shortest path to a working setup:

1. Build OpenMidway with the commands in
   [Build from source](#build-from-source), or use an existing build
   artifact if one is available.
2. Put your BIOS/boot ROM and HDD or game files somewhere easy to find.
3. Launch OpenMidway and configure only the required files first.
4. Verify video, audio, and controller input before changing advanced
   settings.
5. Save that working setup before experimenting with networking or
   renderer tweaks.

For the step-by-step first-run checklist, read
[docs/getting-started.md](docs/getting-started.md).

If your end goal is online play, get a reliable single-player boot
working first. That separates setup problems from networking problems.

---

## Build from source

### Windows 11 x86-64 quick path

From a normal PowerShell window at the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\windows\openmidway-win11.ps1 all
powershell -ExecutionPolicy Bypass -File .\scripts\windows\openmidway-win11.ps1 run
```

This uses MSYS2 MinGW-w64 under the hood and places the runnable build
in `dist\xemu.exe`.

For the full walkthrough, dependency notes, and manual fallback path,
see [docs/windows-11.md](docs/windows-11.md).

### Linux quick path

```bash
# Install dependencies (Ubuntu/Debian example)
sudo apt-get install -y git build-essential ninja-build python3-pip \
    libsdl2-dev libepoxy-dev libpixman-1-dev libgtk-3-dev

# Configure and build
./configure --target-list=i386-softmmu
make -j$(nproc)
```

After the build finishes:

1. read [docs/getting-started.md](docs/getting-started.md),
2. do one clean first boot,
3. and only then tune networking or performance settings.

### Other supported platforms

OpenMidway also targets:

- **macOS (x86-64 / Apple Silicon)**
- **Windows 10/11 (x86-64)**
- **Linux (x86-64)** as the primary development target

For broader platform notes and developer-oriented build details, see
[docs/devel/build-environment.rst](docs/devel/build-environment.rst).

---

## Troubleshooting the first run

The most common setup mistakes are simple ordering problems:

- **Changed too many settings at once:** establish a known-good baseline
  before tuning graphics, timing, or networking.
- **Tried System Link before basic boot worked:** get one title booting
  reliably with stable input, audio, and video first.
- **Used an already-problematic title for validation:** confirm your
  setup on a title that is known to behave well on your machine.

Best next documents:

- [docs/getting-started.md](docs/getting-started.md) for the first-run
  checklist
- [docs/networking.md](docs/networking.md) for System Link and LAN
  tunneling design
- [docs/performance.md](docs/performance.md) for optimization strategy
- [docs/compatibility-triage.md](docs/compatibility-triage.md) for
  reporting whether a title is playable, functional, or broken

---

## Documentation map

| Document | Purpose |
|---|---|
| [docs/getting-started.md](docs/getting-started.md) | Fast first-run guide for new users |
| [docs/windows-11.md](docs/windows-11.md) | Fast Windows 11 source-build path |
| [docs/vision.md](docs/vision.md) | Project vision and strategic direction |
| [ROADMAP.md](ROADMAP.md) | Development phases and priority tiers |
| [docs/architecture.md](docs/architecture.md) | High-level component map |
| [docs/networking.md](docs/networking.md) | Networking subsystem and System Link tunneling design |
| [docs/performance.md](docs/performance.md) | Known bottlenecks and optimization strategy |
| [docs/compatibility-triage.md](docs/compatibility-triage.md) | Compatibility triage process |
| [FORK_DIFF.md](FORK_DIFF.md) | Intentional divergences from upstream xemu/QEMU |

---

## Compatibility goals

OpenMidway uses three practical compatibility tiers:

- **Tier 1 – Playable:** boots, runs without crashes, and is enjoyable
  to completion.
- **Tier 2 – Functional:** boots and runs, but has minor graphical or
  audio issues.
- **Tier 3 – Broken:** crashes, hangs, or has severe corruption.

Current compatibility work is focused on moving more titles from Tier 3
to Tier 2 and from Tier 2 to Tier 1. For the current reporting process,
see [docs/compatibility-triage.md](docs/compatibility-triage.md).

---

## Multiplayer direction

OpenMidway is focused on **System Link / LAN tunneling**, which mirrors
the traffic a real Xbox would send over Ethernet rather than attempting
to recreate Xbox Live.

That direction is intended to be:

- legally straightforward,
- compatible with existing tunneling tools,
- and a practical base for future built-in multiplayer UX.

> **Legal note:** OpenMidway does **not** aim to re-implement or bypass
> native Xbox Live authentication, licensing, or Microsoft servers.

See [docs/networking.md](docs/networking.md) for the current networking
architecture and design notes.

---

## What OpenMidway changes vs upstream xemu

Selected project-specific changes currently documented in the tree:

- **Networking:** PROMISC mode support in nvnet, TX multi-packet
  processing fixes, graceful RX/TX overflow handling, and BIT1 control
  register fixes.
- **Audio (APU/DSP):** Added `mcpx_apu_dsp_reset()` and clearer
  reset-state handling for opcode cache, DMA, interrupts, and GP/EP
  registers.
- **Audio (VP):** `attenuate()` uses a precomputed lookup table instead
  of repeated `powf()` calls during voice processing.
- **GPU (NV2A):** Ongoing pgraph accuracy and performance work,
  including register update cleanup for repeated mask operations.
- **Fork tracking:** [FORK_DIFF.md](FORK_DIFF.md) records intentional
  divergences from upstream xemu/QEMU.

See [FORK_DIFF.md](FORK_DIFF.md) for the full change log.

---

## Contributing

Pull requests are welcome.

Before contributing, please read:

- [docs/devel/code-of-conduct.rst](docs/devel/code-of-conduct.rst)
- [docs/devel/style.rst](docs/devel/style.rst)

If your change is an intentional divergence from upstream xemu or QEMU,
add a corresponding entry to [FORK_DIFF.md](FORK_DIFF.md).

---

## License

OpenMidway is distributed under the GNU General Public License v2 (or
later), the same as QEMU/xemu. See [COPYING](COPYING) and
[LICENSE](LICENSE) for details.
