# OpenMidway

OpenMidway is an open-source Original Xbox emulator forked from [xemu](https://xemu.app) (which is itself based on QEMU). The goal of this project is to improve game compatibility, online multiplayer support via System Link tunneling, and overall performance beyond what is available in upstream xemu.

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

For detailed build instructions see [`docs/devel/build-environment.rst`](docs/devel/build-environment.rst).

---

## Contributing

Pull requests are welcome. Please read [`docs/devel/code-of-conduct.rst`](docs/devel/code-of-conduct.rst) and follow the coding style described in [`docs/devel/style.rst`](docs/devel/style.rst).

When your change is an intentional divergence from upstream xemu or QEMU, add a corresponding entry to [FORK_DIFF.md](FORK_DIFF.md).

---

## License

OpenMidway is distributed under the GNU General Public License v2 (or later), the same as QEMU/xemu. See [COPYING](COPYING) and [LICENSE](LICENSE) for details.
