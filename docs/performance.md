# OpenXbox — Performance Guide

This document describes known performance bottlenecks in OpenXbox, the optimization strategies currently in use, and guidelines for profiling and regression tracking.

---

## 1. Performance Architecture

OpenXbox runs an Original Xbox (Intel Pentium III + nVidia NV2A) inside QEMU's software emulation stack. The main cost centers are:

| Subsystem | Host CPU cost | Notes |
|---|---|---|
| TCG (CPU JIT) | Medium | Block cache helps; CPUID-sensitive code can cause frequent exits |
| NV2A PGRAPH | **High** | Per-draw-call register decode + shader translation dominates |
| APU Voice Processor | Medium | 256-voice mix at 48 kHz; previously `powf()` in the hot path |
| APU DSP | Low–Medium | DSP ucode emulation; cost spikes with heavy reverb/EQ |
| nvnet NIC | Low | TX multi-packet fix reduces interrupt storm overhead |
| UI / display | Low | SDL2 blit; vsync path is cheap |

---

## 2. Current Optimizations

### 2.1 APU Voice Processor — `attenuate()` LUT

**Problem:** `attenuate()` in `hw/xbox/mcpx/apu/vp/vp.c` was called thousands of times per audio frame and internally called `powf()`, which is expensive on all platforms.

**Fix:** A 4096-entry lookup table (`attenuate_table[]`) is pre-computed once in `mcpx_apu_vp_init()`. `attenuate()` now does a single array index instead of a floating-point exponentiation.

**Impact:** Measurable reduction in audio thread CPU usage, especially on titles with many simultaneous voices.

### 2.2 NV2A PGRAPH — Register Read-Modify-Write Consolidation

**Problem:** `PG_SET_MASK` performs a full read-modify-write cycle on `pg->regs_`. Consecutive calls targeting the same register result in redundant reads.

**Guideline:** Where multiple consecutive `PG_SET_MASK` calls target the same register, consolidate them using:
```c
uint32_t val = pgraph_reg_r(pg, REG);
val = SET_MASK(val, FIELD_A, value_a);
val = SET_MASK(val, FIELD_B, value_b);
pgraph_reg_w(pg, REG, val);
```

This pattern is tracked in ongoing NV2A refactoring work.

### 2.3 NV2A PGRAPH — Shader Compile Stall Instrumentation (GL + Vulkan)

**Problem:** First-draw shader compilation stalls (P-1) were invisible to operators — no
counter existed to measure how much time was lost compiling GLSL shaders, or how often
the fast "state unchanged" path was taken.

**Fix (Phase 3A — GL backend):** Two new per-frame counters were added to the NV2A
profiling ring in `debug.h`:
- `NV2A_PROF_SHADER_COMPILE_US` — accumulates µs spent inside `generate_shaders()` on
  each cold cache miss.
- `NV2A_PROF_SHADER_HOT_DRAW` — counts draw calls where shader state was unchanged (the
  fastest path, no LRU lookup required).

Both counters are instrumented in `pgraph_gl_bind_shaders()` and auto-appear in the
Advanced debug overlay.

**Fix (Phase 3B — Vulkan backend parity):** The same two counters are now instrumented
in `pgraph_vk_bind_shaders()` and `shader_cache_entry_init()` in
`hw/xbox/nv2a/pgraph/vk/shaders.c`, giving the Vulkan rendering path identical
observability to the GL path.

**Impact:** Per-frame shader stall budget can now be monitored and capped via the
per-title `max_shader_stall_us` threshold in the compat test suite.

---

## 3. Known Bottlenecks (Unresolved)

| # | Subsystem | Description | Tracking label |
|---|---|---|---|
| P-1 | NV2A PGRAPH | Shader compilation stutter on first draw of new shader combinations — now tracked via `NV2A_PROF_SHADER_COMPILE_US` in both GL and Vulkan backends | `gpu-nv2a` |
| P-2 | NV2A PGRAPH | Surface resolve (GPU→CPU readback) blocks the main thread | `gpu-nv2a` |
| P-3 | TCG | Self-modifying code detection causes excessive TB flushes on some titles | `cpu-tcg` |
| P-4 | APU DSP | ucode interpreter per-sample loop not vectorized | `audio-dsp` |
| P-5 | UI | Frame pacing jitter on Wayland compositors with vsync enabled | `performance-regression` |

---

## 4. Profiling

### 4.1 Linux (perf)

```bash
perf record -g -F 999 -- ./qemu-system-i386 [xbox args]
perf report --stdio | head -80
```

### 4.2 macOS (Instruments)

Use the **Time Profiler** template in Instruments.app. Pay attention to `nv2a_pgraph_*` and `mcpx_apu_*` symbols.

### 4.3 Windows (WPR / VTune)

Use Intel VTune Profiler or Windows Performance Recorder. Filter by `qemu-system-i386.exe`.

### 4.4 QEMU built-in profiling

Build with `--enable-profiler` and use the `qemu_perf_*` trace points.

---

## 5. Regression Testing

Every PR that touches a performance-critical subsystem (NV2A, APU VP, TCG) should include:

1. A note in the PR description describing which bottleneck is addressed.
2. A before/after measurement (FPS on a reference title, or audio-thread CPU% from `htop`/Activity Monitor).
3. A new entry in this document if a previously unknown bottleneck is resolved.

Label performance-related issues with `performance-regression`.

---

## 6. Reference Titles for Benchmarking

| Title | Why useful |
|---|---|
| Halo: Combat Evolved | Heavy NV2A PGRAPH load; good shader diversity |
| Project Gotham Racing | Tests vertex shader throughput |
| Jet Set Radio Future | Heavy audio (many simultaneous voices) |
| Splinter Cell | Tests self-modifying code / TCG flush frequency |
