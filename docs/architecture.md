# OpenMidway — Architecture Overview

This document provides a high-level map of the OpenMidway codebase and how its major components relate to one another.

---

## Relationship to Upstream Projects

```
QEMU (upstream)
  └─ xemu (Xbox-specific fork of QEMU)
       └─ OpenMidway (this project — focused on compatibility, networking, and performance)
```

OpenMidway tracks xemu as its primary upstream. Intentional divergences are recorded in [FORK_DIFF.md](../FORK_DIFF.md).

---

## Component Map

### CPU — `accel/tcg/` and `target/i386/`

The Original Xbox uses a custom Intel Pentium III (Coppermine) derivative. QEMU's Tiny Code Generator (TCG) provides software emulation. JIT translation accuracy is critical for games that rely on CPUID quirks or precise floating-point behavior.

Key paths:
- `target/i386/` — x86 instruction decode and TCG translation
- `accel/tcg/` — TCG execution loop and block cache

### GPU — `hw/xbox/nv2a/`

The NV2A is an nVidia custom GPU. It is the most complex subsystem in the emulator.

| Sub-component | Path | Notes |
|---|---|---|
| PGRAPH (3-D engine) | `hw/xbox/nv2a/pgraph/` | Vertex/fragment shader translation, surface management |
| PVIDEO (video overlay) | `hw/xbox/nv2a/` | PVIDEO overlay rendering |
| PRAMDAC | `hw/xbox/nv2a/` | DAC / CRTC output |
| Renderer backends | `hw/xbox/nv2a/pgraph/` | OpenGL (default); future Vulkan/Metal hooks |

### Audio — `hw/xbox/mcpx/apu/`

The MCPX APU implements hardware voice mixing and DSP effects.

| Sub-component | Path | Notes |
|---|---|---|
| Voice Processor (VP) | `hw/xbox/mcpx/apu/vp/` | 256-voice HW mixer; `attenuate()` uses LUT to avoid `powf()` overhead |
| DSP (GP/EP) | `hw/xbox/mcpx/apu/dsp/` | Effects DSP; reset sequence resets opcode cache, DMA, interrupts separately |

### Networking — `hw/xbox/mcpx/nvnet/`

The MCPX NIC is an nForce Ethernet controller (nvnet). OpenMidway has improved the emulation accuracy (PROMISC mode, TX multi-packet, overflow handling). See [docs/networking.md](networking.md) for the System Link design.

### Storage — `hw/xbox/`

- Hard-disk image formatted with FATX.
- DVD drive emulated via standard QEMU block layer.

### USB / Input — `hw/usb/`

Standard QEMU USB stack with Xbox HID descriptor patches. Duke and S-controller descriptor tables are in `hw/xbox/`.

### BIOS / Boot ROM — `hw/xbox/`

The BIOS image is loaded at startup. OpenMidway does not redistribute the BIOS; users must supply their own dump.

### System Bus — `hw/xbox/`

- LPC bus: `hw/xbox/lpc47m157.c` (SuperIO)
- SMBus: `hw/xbox/smbus*.c` (fan controller, EEPROM, video encoder, clock synthesizer)
- ACPI: `hw/xbox/acpi_xbox.c`

---

## Data Flow (simplified boot path)

```
BIOS ROM loaded
  → CPU executes x86 bootstrap
  → SMBus devices (SMC, EEPROM) initialized
  → NV2A GPU initialized (PGRAPH, PVIDEO)
  → APU initialized (VP, DSP)
  → nvnet NIC initialized
  → Dashboard or game XBE loaded and executed
```

---

## Directory Quick Reference

| Directory | Content |
|---|---|
| `hw/xbox/` | All Xbox-specific hardware models |
| `hw/xbox/nv2a/` | NV2A GPU |
| `hw/xbox/mcpx/` | MCPX (audio + NIC + IDE) |
| `target/i386/` | x86 TCG guest CPU |
| `ui/` | SDL2 / GTK front-end, input, display |
| `audio/` | Host audio backends (PA, CoreAudio, WASAPI…) |
| `net/` | Host network backends (TAP, SLiRP, PCAP…) |
| `docs/` | Project documentation |
