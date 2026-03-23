# OpenMidway

OpenMidway is a modern, user-friendly original Xbox emulator focused on:
- Seamless online multiplayer (System Link, tunneling, real hardware)
- Strong Windows support
- Performance and stability on real-world PCs
- Gradual improvements to compatibility and accuracy

It is forked from [xemu](https://xemu.app) (which is itself based on QEMU).

---

## Why OpenMidway?

Most emulators focus on accuracy first.

OpenMidway focuses on:
- "Can I play this with friends?"
- "Does it run well on my PC?"
- "Can I set it up in 5 minutes?"

---

## Features (Planned)

- One-click System Link setup
- Multiplayer relay (room codes)
- Improved GPU performance
- Windows-first UX
- Built-in diagnostics

---

## Quick Start for New Users

If you want the shortest path to a first successful boot:

1. Build OpenMidway with the commands in [Building](#building) or use an existing build artifact.
2. Gather your Xbox BIOS/boot ROM and your HDD/game image files in one easy-to-find folder.
3. Launch OpenMidway and configure only the minimum required files first.
4. Verify video, audio, and controller input before changing advanced settings.
5. Save that working setup, then move on to networking or performance tuning.

For a more detailed first-run walkthrough, use [docs/getting-started.md](docs/getting-started.md).

---

## Based on

This project is based on [xemu](https://xemu.app) and QEMU.

---

## What OpenMidway Changes vs Upstream xemu

| Area | Change |
|---|---|
| **Networking** | Added PROMISC mode support in nvnet; TX multi-packet processing fixed; graceful RX/TX overflow handling; BIT1 control register fix. Groundwork for System Link / LAN tunneling. |
| **Audio (APU/DSP)** | Added `mcpx_apu_dsp_reset()` helper; separate opcode-cache, DMA, interrupt, and GP/EP register clearing on reset. |
| **Audio (VP)** | `attenuate()` now uses a pre-computed 4096-entry lookup table to avoid repeated `powf()` calls during voice processing. |
| **GPU (NV2A)** | PG_SET_MASK read-modify-write consolidation for consecutive register writes; ongoing pgraph accuracy improvements. |
| **Fork tracking** | [`FORK_DIFF.md`](FORK_DIFF.md) records every intentional divergence from upstream xemu/QEMU. |

See [FORK_DIFF.md](FORK_DIFF.md) for the full, always-up-to-date change log.

---

## Supported Platforms

| Platform | Status |
|---|---|
| **Linux (x86-64)** | Primary development target |
| **macOS (x86-64 / Apple Silicon)** | Supported (Metal renderer path) |
| **Windows 10/11 (x86-64)** | Supported |

---

## Compatibility Goals

- **Tier 1 – Playable:** The game boots, runs without crashes, and is enjoyable to completion.
- **Tier 2 – Functional:** The game boots and runs but may have minor graphical or audio glitches.
- **Tier 3 – Broken:** The game crashes, hangs, or produces severe graphical corruption.

Current focus is promoting as many titles as possible from Tier 3 → Tier 2 and Tier 2 → Tier 1. See [docs/compatibility-triage.md](docs/compatibility-triage.md) for the current triage process.

---

## Online Multiplayer Goals

OpenMidway targets **System Link / LAN tunneling** — replicating the behavior of a physical Ethernet cable between consoles over the internet using standard tunneling protocols (e.g., UDP encapsulation). This approach is:

- Legally straightforward: it mirrors traffic that the games themselves generate with no server-side Xbox Live authentication.
- Compatible with existing tunneling tools (XLink Kai, Insignia system-link relay, and future built-in support).

> **Legal note:** OpenMidway does **not** aim to re-implement or bypass native Xbox Live (Microsoft's proprietary online service). System Link / LAN tunneling operates entirely at the network layer without touching Xbox Live authentication, title licensing, or Microsoft's servers.

See [docs/networking.md](docs/networking.md) for architecture details.

---

## Documentation

| Document | Purpose |
|---|---|
| [docs/getting-started.md](docs/getting-started.md) | Fast first-run guide for new users |
| [docs/vision.md](docs/vision.md) | Project vision and strategic direction |
| [ROADMAP.md](ROADMAP.md) | Development phases and priority tiers |
| [docs/architecture.md](docs/architecture.md) | High-level component map |
| [docs/networking.md](docs/networking.md) | Networking subsystem and System Link tunneling design |
| [docs/performance.md](docs/performance.md) | Known bottlenecks and optimization strategy |
| [docs/compatibility-triage.md](docs/compatibility-triage.md) | How to triage and categorize game compatibility |
| [FORK_DIFF.md](FORK_DIFF.md) | All intentional divergences from upstream xemu/QEMU |

---

## Building

```bash
# Install dependencies (Ubuntu/Debian example)
sudo apt-get install -y git build-essential ninja-build python3-pip \
    libsdl2-dev libepoxy-dev libpixman-1-dev libgtk-3-dev

# Configure and build
./configure --target-list=i386-softmmu
make -j$(nproc)
```

For first-time users, the easiest workflow is:

1. Finish the build.
2. Read [docs/getting-started.md](docs/getting-started.md).
3. Do one clean first boot before tuning performance or networking.

For detailed build instructions see [`docs/devel/build-environment.rst`](docs/devel/build-environment.rst).

---

## Contributing

Pull requests are welcome. Please read [`docs/devel/code-of-conduct.rst`](docs/devel/code-of-conduct.rst) and follow the coding style described in [`docs/devel/style.rst`](docs/devel/style.rst).

When your change is an intentional divergence from upstream xemu or QEMU, add a corresponding entry to [FORK_DIFF.md](FORK_DIFF.md).

---

## License

OpenMidway is distributed under the GNU General Public License v2 (or later), the same as QEMU/xemu. See [COPYING](COPYING) and [LICENSE](LICENSE) for details.
