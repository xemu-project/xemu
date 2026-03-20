# OpenXbox — Compatibility Triage

This document defines how game compatibility is categorized, how to submit a triage report, and what the current focus areas are.

---

## 1. Compatibility Tiers

| Tier | Label | Definition |
|---|---|---|
| **Tier 1 — Playable** | `compat-tier1` | Boots, runs to completion without crashes. Minor visual/audio glitches acceptable if non-intrusive. |
| **Tier 2 — Functional** | `compat-tier2` | Boots and runs but has notable glitches (graphical corruption, audio crackling, save/load issues). |
| **Tier 3 — Broken** | `compat-tier3` | Crashes on boot or shortly after, hangs indefinitely, or produces severe corruption that makes the game unplayable. |
| **Tier 0 — Untested** | `compat-untested` | Not yet tested in OpenXbox. |

The current focus is **Tier 3 → Tier 2** promotions, followed by **Tier 2 → Tier 1** polish.

---

## 2. How to Submit a Triage Report

1. **Open an issue** titled `[Compat] <Game Title> — <Tier>`.
2. Apply the relevant tier label (`compat-tier1` / `compat-tier2` / `compat-tier3`).
3. Include the following information:

```
## Game Info
- Title: 
- Region / Media ID:
- Genre:

## Environment
- OpenXbox commit hash:
- Host OS + version:
- Host GPU + driver version:
- BIOS version (hash, not the ROM):

## Observed Behavior
<!-- Describe what happens. Attach screenshots or a short video if possible. -->

## Expected Behavior
<!-- Describe what should happen on real hardware. -->

## Regression?
<!-- Was this working in a previous commit? If so, which one? -->
```

4. If you know the root cause (e.g., a specific NV2A register behavior), add the relevant component label (`gpu-nv2a`, `audio-dsp`, `cpu-tcg`, etc.).

---

## 3. Triage Labels

| Label | Use |
|---|---|
| `compatibility` | General compatibility issue |
| `gpu-nv2a` | NV2A GPU emulation inaccuracy |
| `audio-dsp` | MCPX APU / DSP emulation issue |
| `cpu-tcg` | TCG CPU emulation issue (incorrect instruction, TLB, FPU) |
| `multiplayer-system-link` | System Link / online multiplayer issue |
| `insignia` | Compatibility with Insignia's System Link relay service (not native Xbox Live) |
| `performance-regression` | A previously acceptable frame rate has regressed |

---

## 4. Priority Matrix

Issues are prioritized by the following heuristics:

1. **Regression first:** A title that _used_ to work and now doesn't is always high priority.
2. **Popular titles second:** Higher-profile titles (Halo, Fable, JSRF) get more attention because they surface issues that affect many games.
3. **Root-cause generality third:** A fix that unblocks many titles is preferred over a per-title hack.

---

## 5. Known Focus Areas

### 5.1 GPU — NV2A PGRAPH

Many Tier 3 titles fail due to unimplemented or incorrectly emulated NV2A state. Track these under `gpu-nv2a`.

Common root causes:
- Unimplemented combiner modes
- Incorrect surface tiling/swizzle
- Missing vertex attribute formats
- Z-buffer precision differences

### 5.2 CPU — TCG

Some titles trigger QEMU TCG paths that are inaccurate for the Pentium III variant used in the Xbox. Track these under `cpu-tcg`.

Common root causes:
- `CPUID` returns unexpected values
- `FXSAVE`/`FXRSTOR` state mismatch
- Self-modifying code detection false positives

### 5.3 Audio — APU DSP

Titles that use heavy reverb or EQ effects may crash or produce silence due to DSP ucode emulation gaps. Track under `audio-dsp`.

### 5.4 System Link / Online

Track multiplayer issues under `multiplayer-system-link`. See [docs/networking.md](networking.md) for design context.

---

## 6. Upstream Tracking

When a fix has been upstreamed to xemu, close the issue and add a note referencing the upstream PR. When an upstream fix is ported to OpenXbox, update [FORK_DIFF.md](../FORK_DIFF.md) accordingly.
