# OpenXbox — Compatibility Triage

This document defines how game compatibility is categorized, how to submit a triage report, and what the current focus areas are.

---

## 1. Compatibility Matrix

Titles are assigned to one of four buckets.  Bucket assignment is
determined by the **worst observable failure mode** during a
standardised test run (boot to title screen, 5 minutes of gameplay).

| Bucket | Label | Color | Definition |
|---|---|---|---|
| **Boots** | `compat-boots` | 🟢 Green | Reaches the title screen reliably.  Gameplay may have issues. |
| **In-Game Broken** | `compat-ingame-broken` | 🟡 Yellow | Boots but crashes or hangs shortly after entering gameplay. |
| **Playable with Issues** | `compat-playable-issues` | 🟠 Orange | Playable from start to finish with notable glitches (visual corruption, audio crackling, missing effects). |
| **High Priority Regression** | `compat-regression` | 🔴 Red | Was previously in a better bucket; a recent commit caused a measurable regression. |

> **Legacy tiers (Tier 0–3)** are still accepted on incoming issues and
> are mapped to the matrix automatically: Tier 1 → Playable with Issues,
> Tier 2 → In-Game Broken, Tier 3 → In-Game Broken or Boots (depending on
> whether the title reaches the title screen), Tier 0 → not yet triaged.

The current sprint focuses on:

1. Promoting **In-Game Broken** titles to **Boots** by fixing root-cause subsystem bugs.
2. Eliminating all **High Priority Regressions** before each release tag.
3. Promoting **Playable with Issues** titles to fully working where the fix is low-risk.

---

## 2. Top Priority Titles

The following titles are the primary focus for the current sprint.  Each
entry lists the primary subsystems under stress and which bucket the title
currently sits in.  Per-game issue templates live in
`.github/ISSUE_TEMPLATE/` and per-title runtime configurations live in
`tests/xbox/compat/titles/`.

| Title | Bucket | Primary Subsystems | Config File |
|---|---|---|---|
| Halo: Combat Evolved | In-Game Broken | GPU · networking · audio | `halo-ce.json` |
| Halo 2 | In-Game Broken | GPU · networking · audio | `halo-2.json` |
| Jet Set Radio Future | In-Game Broken | timing · GPU | `jsrf.json` |
| Project Gotham Racing 2 | Playable with Issues | CPU/GPU sync | `pgr2.json` |
| Ninja Gaiden Black | In-Game Broken | memory · GPU correctness | `ninja-gaiden-black.json` |
| Fable: The Lost Chapters | Playable with Issues | filesystem · timing · shaders | `fable.json` |
| MechAssault | Boots | system link · GPU | `mechassault.json` |
| Crimson Skies | Boots | system link · GPU | `crimson-skies.json` |

---

## 3. How to Submit a Triage Report

1. **Open an issue** using the per-game template (if available) or the
   generic **Title Issue** template.
2. Apply the relevant bucket label (`compat-boots` / `compat-ingame-broken` /
   `compat-playable-issues` / `compat-regression`).
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

## 4. Triage Labels

| Label | Use |
|---|---|
| `compat-boots` | Reaches title screen |
| `compat-ingame-broken` | Boots but crashes in gameplay |
| `compat-playable-issues` | Playable with notable glitches |
| `compat-regression` | Previously better; now regressed |
| `gpu-nv2a` | NV2A GPU emulation inaccuracy |
| `audio-dsp` | MCPX APU / DSP emulation issue |
| `cpu-tcg` | TCG CPU emulation issue (incorrect instruction, TLB, FPU) |
| `multiplayer-system-link` | System Link / online multiplayer issue |
| `insignia` | Compatibility with Insignia's System Link relay service (not native Xbox Live) |
| `performance-regression` | A previously acceptable frame rate has regressed |

---

## 5. Priority Matrix

Issues are prioritized by the following heuristics:

1. **Regression first:** A title that _used_ to work and now doesn't is always high priority.
2. **Popular titles second:** Higher-profile titles (Halo, Fable, JSRF) get more attention because they surface issues that affect many games.
3. **Root-cause generality third:** A fix that unblocks many titles is preferred over a per-title hack.

---

## 6. Known Focus Areas

### 6.1 GPU — NV2A PGRAPH

Many In-Game Broken titles fail due to unimplemented or incorrectly emulated NV2A state. Track these under `gpu-nv2a`.

Common root causes:
- Unimplemented combiner modes
- Incorrect surface tiling/swizzle
- Missing vertex attribute formats
- Z-buffer precision differences

### 6.2 CPU — TCG

Some titles trigger QEMU TCG paths that are inaccurate for the Pentium III variant used in the Xbox. Track these under `cpu-tcg`.

Common root causes:
- `CPUID` returns unexpected values
- `FXSAVE`/`FXRSTOR` state mismatch
- Self-modifying code detection false positives

### 6.3 Audio — APU DSP

Titles that use heavy reverb or EQ effects may crash or produce silence due to DSP ucode emulation gaps. Track under `audio-dsp`.

### 6.4 System Link / Online

Track multiplayer issues under `multiplayer-system-link`. See [docs/networking.md](networking.md) for design context.

---

## 7. Automated Testing

The compatibility harness lives in `tests/xbox/compat/harness.py`.  It
can be run against any title that has a config file in
`tests/xbox/compat/titles/`:

```
python tests/xbox/compat/harness.py --title halo-ce \
    --xemu /path/to/xemu --iso /path/to/halo.iso
```

The harness boots the title, waits up to `boot_timeout_s` seconds for the
frame hash to match the golden value stored in the title config, and then
records:

- **FPS** (frames per second during the observation window)
- **Shader compile count** (new shaders compiled during the run)
- **Audio underruns** (MCPX APU buffer starvation events)
- **GPU warnings** (NV2A PGRAPH assertions logged by xemu)

Results are written to `tests/xbox/compat/results/<title>-<timestamp>.json`.

Crash logs can be analysed with `tests/xbox/tools/crash_cluster.py` to
group stack traces by signature and surface the most common failure modes.

---

## 8. Upstream Tracking

When a fix has been upstreamed to xemu, close the issue and add a note referencing the upstream PR. When an upstream fix is ported to OpenXbox, update [FORK_DIFF.md](../FORK_DIFF.md) accordingly.
