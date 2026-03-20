# FORK_DIFF — OpenXbox Divergences from Upstream xemu / QEMU

This file tracks every **intentional** change that OpenXbox has made relative to upstream [xemu](https://github.com/xemu-project/xemu) (and transitively QEMU). It is the authoritative record of what makes OpenXbox different.

**Maintainer rule:** Every PR that intentionally diverges from upstream xemu **must** add or update an entry here before merging.

---

## Format

Each entry follows this template:

```
### <Short title>

- **Area:** <component path or subsystem name>
- **Status:** Active | Upstreamed | Reverted
- **Upstream PR / issue:** <link or "N/A">
- **Description:** One-paragraph explanation of what was changed and why.
- **Files:** List of modified files.
```

---

## Entries

---

### nvnet: PROMISC mode support

- **Area:** `hw/xbox/mcpx/nvnet/nvnet.c`
- **Status:** Active
- **Upstream PR / issue:** N/A
- **Description:** The `NvRegPacketFilterFlags` PROMISC bit is now correctly accepted and forwarded to the host network backend. Without this, titles that use broadcast-based System Link discovery fail to receive peer announcements when the host TAP/bridge is in promiscuous mode.
- **Files:** `hw/xbox/mcpx/nvnet/nvnet.c`

---

### nvnet: TX multi-packet processing fix

- **Area:** `hw/xbox/mcpx/nvnet/nvnet.c`
- **Status:** Active
- **Upstream PR / issue:** N/A
- **Description:** The transmit descriptor loop previously stopped after processing the first ready descriptor. It now walks all ready descriptors in a single pass, matching real hardware behavior and reducing interrupt-storm overhead on burst-heavy titles.
- **Files:** `hw/xbox/mcpx/nvnet/nvnet.c`

---

### nvnet: Graceful RX/TX overflow handling

- **Area:** `hw/xbox/mcpx/nvnet/nvnet.c`
- **Status:** Active
- **Upstream PR / issue:** N/A
- **Description:** When the receive or transmit ring buffer is full, the emulator previously asserted or silently corrupted state. It now drops the packet and increments the appropriate overflow statistics counter, matching the behavior documented in the nForce datasheet.
- **Files:** `hw/xbox/mcpx/nvnet/nvnet.c`

---

### nvnet: BIT1 control register fix

- **Area:** `hw/xbox/mcpx/nvnet/nvnet.c`
- **Status:** Active
- **Upstream PR / issue:** N/A
- **Description:** The undocumented BIT1 field in the MCPX NIC control register was being clobbered on soft reset. It is now preserved across resets, fixing initialization failures observed in a small number of titles.
- **Files:** `hw/xbox/mcpx/nvnet/nvnet.c`

---

### APU VP: `attenuate()` lookup table

- **Area:** `hw/xbox/mcpx/apu/vp/vp.c`
- **Status:** Active
- **Upstream PR / issue:** N/A
- **Description:** `attenuate()` was calling `powf()` on every invocation, creating measurable CPU overhead in the audio thread. A 4096-entry pre-computed lookup table (`attenuate_table[]`) is now initialized in `mcpx_apu_vp_init()`. `attenuate()` performs a single array lookup, eliminating the floating-point exponentiation from the hot path.
- **Files:** `hw/xbox/mcpx/apu/vp/vp.c`

---

### APU DSP: `mcpx_apu_dsp_reset()` helper

- **Area:** `hw/xbox/mcpx/apu/apu.c`, `hw/xbox/mcpx/apu/dsp/dsp.c`
- **Status:** Active
- **Upstream PR / issue:** N/A
- **Description:** Upstream `dsp_reset()` only resets the DSP CPU core. The new `mcpx_apu_dsp_reset()` helper additionally clears the opcode cache, DMA state, interrupt state, and GP/EP registers. This matches the hardware reset sequence documented in available Xbox reverse-engineering notes and fixes audio corruption after title transitions.
- **Files:** `hw/xbox/mcpx/apu/apu.c`, `hw/xbox/mcpx/apu/dsp/dsp.c`

---

### NV2A PGRAPH: shader compile stall instrumentation (GL backend)

- **Area:** `hw/xbox/nv2a/pgraph/gl/shaders.c`, `hw/xbox/nv2a/debug.h`
- **Status:** Active
- **Upstream PR / issue:** N/A
- **Description:** Adds two new per-frame profiling counters: `NV2A_PROF_SHADER_COMPILE_US` accumulates microseconds spent compiling GLSL shaders inside `generate_shaders()` on each cache miss; `NV2A_PROF_SHADER_HOT_DRAW` counts draw calls that skip the shader-state check entirely (state unchanged, zero LRU lookup). Together with the existing `NV2A_PROF_SHADER_GEN` and `NV2A_PROF_SHADER_BIND` counters, these provide a full hot/warm/cold draw-call breakdown visible in the Advanced debug overlay. A companion `nv2a_profile_add_counter()` inline was added to `debug.h` for accumulating non-unit values. Per-title compile stall budgets (`max_shader_stall_us`) were added to all eight compat title configs and validated in `test-compat.c`.
- **Files:** `hw/xbox/nv2a/debug.h`, `hw/xbox/nv2a/pgraph/gl/shaders.c`, `tests/xbox/compat/titles/*.json`, `tests/xbox/compat/test-compat.c`

---

### NV2A PGRAPH: shader compile stall instrumentation (Vulkan backend parity)

- **Area:** `hw/xbox/nv2a/pgraph/vk/shaders.c`
- **Status:** Active
- **Upstream PR / issue:** N/A
- **Description:** Extends the Phase 3A shader stall instrumentation to the Vulkan rendering backend. `shader_cache_entry_init()` now wraps all `get_and_ref_shader_module_for_key()` calls with a `qemu_clock_get_us` timer and accumulates the result into `NV2A_PROF_SHADER_COMPILE_US`, matching the GL path. `pgraph_vk_bind_shaders()` now also increments `NV2A_PROF_SHADER_HOT_DRAW` in the not-dirty fast-path branch, giving Vulkan the same hot/warm/cold draw-call visibility as the GL backend.
- **Files:** `hw/xbox/nv2a/pgraph/vk/shaders.c`

---

### NV2A PGRAPH: register read-modify-write consolidation (ongoing)

- **Area:** `hw/xbox/nv2a/pgraph/pgraph.c`
- **Status:** Active (in progress)
- **Upstream PR / issue:** N/A
- **Description:** `PG_SET_MASK` performs a read-modify-write on `pg->regs_`. In several code paths, consecutive calls target the same register, resulting in redundant reads. These are being consolidated using a `pgraph_reg_r` + `SET_MASK` + `pgraph_reg_w` pattern to reduce register access overhead in the PGRAPH hot path.
- **Files:** `hw/xbox/nv2a/pgraph/pgraph.c`

---

## Upstreamed / Reverted

*(None yet — entries will be moved here once a divergence is merged into upstream xemu or intentionally reverted.)*

---

## How to Add an Entry

1. Copy the template from the **Format** section above.
2. Fill in all fields. "N/A" is acceptable for **Upstream PR / issue** if no upstream issue exists yet.
3. Set **Status** to `Active`.
4. Commit the updated `FORK_DIFF.md` in the same PR as the code change.
