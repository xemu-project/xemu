# Plan: Halo 2 performance on Apple Silicon MacBooks

Goal: make Halo 2 playable on Apple MacBooks, with the **most minimal changes
necessary**. This plan is staged so we measure before we change, and so the
first lever (renderer choice) is verified before touching any hot-path code.

## Status

- [x] Toolchain set up: `dylibbundler`, `ninja` installed via Homebrew (meson
      bootstraps inside QEMU's `pyvenv`).
- [x] Build via `./build.sh` (arm64, macOS 14.0 target). Builds and runs:
      `xemu 0.8.136`. (Fixed the duplicate `LC_RPATH` in `build.sh`'s
      `package_macos` — newer dyld rejects it — see "Build notes".)
- [x] Added opt-in profiler dump (`XEMU_NV2A_PROFILE=<interval>` → stderr) in
      `hw/xbox/nv2a/pgraph/profile.c`. Off by default.
- [x] Hardware confirmed: **Apple M5**, `GL_VERSION: 4.1 Metal - 91.6`
      (OpenGL frozen at 4.1, running over Metal), macOS 27.0 (26A5368g).
- [⚠] Booting Halo 2 **reproducibly SIGSEGVs during startup** — see below.
      Could not capture a frame baseline because it crashes before rendering.

## ✅ It RUNS when launched via LaunchServices (the crash was CLI-only)

Launching the binary directly from a shell SIGSEGVs (see next section), but
`open dist/xemu.app --env XEMU_NV2A_PROFILE=60 --stdout /tmp/h2.log --stderr
/tmp/h2.log` boots Halo 2 and renders. The crash was a launch-method artifact
(no Cocoa main thread / run loop), consistent with "stock xemu works." This also
gives us a working **measure loop**: change → rebuild → `open` → read FPS.

## 📊 Baseline profile (Halo 2 main menu — the "skippy" scene)

Build is confirmed **-O2 + NDEBUG** (not a debug build). At the animated main
menu, FPS collapses to **~4-5 fps (~85-118 ms/frame)**. Per-frame NV2A counters
at that point:

| counter | ~per frame | meaning |
|---|---|---|
| BEGIN_ENDS | 250-390 | draw calls / frame |
| ATTR_BIND | 425-590 | vertex-attribute (re)binds |
| INLINE_ARRAYS | 112-212 | `pgraph_gl_bind_inline_array`: `glBufferData`+`glBufferSubData` **per draw** |
| GEOM_BUFFER_UPDATE_2 | 112-212 | the orphan+upload inside that |
| INLINE_ELEMENTS | 98-114 | `glDrawElements` (element cache) |
| GEOM_BUFFER_UPDATE_3 | 88-162 | inline-buffer `glBufferData` per attribute |
| INLINE_BUFFERS | 35-64 | inline-buffer draws |
| SHADER_GEN | <1 | **not** shader-comp bound |
| SURF_DOWNLOAD / SURF_TO_TEX / TEX_UPLOAD | ~0-4 | **not** surface-readback bound |

**Diagnosis:** the menu is **draw-submission / vertex-upload bound**, not
surface-readback or shader bound as first hypothesized. ~300 immediate-mode
draws/frame, each re-uploading vertex data (`glBufferData` orphan +
`glBufferSubData`, `GL_STREAM_DRAW`) and rebinding all 16 vertex attributes
(`pgraph_gl_bind_vertex_attributes`). On Apple's GL-over-Metal that per-draw
buffer churn is very expensive. Because the pfifo render thread takes 85-118 ms
per frame, the machine can't run real-time, which **starves audio** → the user's
"skippy audio *and* picture at the menu." Both symptoms are the same root cause.

Hot path: `hw/xbox/nv2a/pgraph/gl/draw.c` (inline array/buffer/elements branches)
and `gl/vertex.c` (`pgraph_gl_bind_vertex_attributes`, `pgraph_gl_bind_inline_array`).

Open question: is stock xemu's menu *also* ~4-5 fps here? If yes, this path is
the genuine optimization target. If stock is smooth, there's a specific
regression in this snapshot to diff instead.

### Optimization #1 applied + measured (≈20% frame-time win)

Two safe, clearly-correct reductions in the per-draw GL path
(`hw/xbox/nv2a/pgraph/gl/vertex.c`):
1. Hoisted a redundant per-attribute `glBindBuffer(gl_inline_array_buffer)` out
   of the loop in `pgraph_gl_bind_vertex_attributes` (it rebound the *same*
   buffer up to 16× per draw; the caller already bound it once).
2. Merged the orphan + upload in `pgraph_gl_bind_inline_array`
   (`glBufferData(NULL)` + `glBufferSubData`) into a single `glBufferData(data)`,
   preserving the `GL_STREAM_DRAW` non-stalling behavior, one fewer call/draw.

Measured on the Halo 2 menu, normalized by draw count (frames matched on
`BEGIN_ENDS`, since the uncontrolled scene varies):

| metric | baseline | optimized |
|---|---|---|
| frame_ms vs draws (linear fit) | 36.5 + **0.200·draws** | 26.6 + **0.163·draws** |
| per-draw cost | 0.200 ms | **0.163 ms (-18%)** |
| ~500-draw frame | ~136 ms | **~108 ms (-20%)** |

Stable, no crash, no visible regression through the same scene progression.
Modest in absolute terms — the menu is pathologically draw-heavy (200-900
immediate-mode draws/frame), so it's still single-digit FPS at peak. The win is
real and free; bigger levers remain (below).

### Optimization #2 attempted + REVERTED (no measurable gain)

Cached the per-attribute enable/disable mask + generic `glVertexAttrib4fv`
values in `PGRAPHGLState` and skipped redundant calls (routed `vertex.c` and the
`draw.c` inline-buffer branch through helpers; correctness verified — only those
files touch `gl_vertex_array`'s enable state / the context generic values).

Measured (same linear-fit method, menu): **162 µs/draw → 157 µs/draw (~3%)**,
within scene-to-scene noise; fixed overhead unchanged. Conclusion: the
`glEnable/DisableVertexAttribArray` + `glVertexAttrib4fv` calls are **cheap** on
Apple's GL — they are *not* where the per-draw time goes. Reverted to keep the
tree minimal (no benefit ≠ worth the shadow-state complexity).

**What this tells us:** the per-draw cost is dominated by the calls we *kept* —
`glVertexAttribPointer` setup, `glBufferData`/`glBufferSubData` uploads, and the
draw + Metal command encoding itself — not attribute enable state. The committed
opt #1 (which removed redundant `glBindBuffer` + a redundant upload) was the real
win precisely because it cut buffer-binding/upload work.

### Next levers (larger, in priority order)
1. **Reduce per-draw buffer re-specification** — inline buffer/array draws
   re-`glBufferData` every draw (orphan+realloc). A persistent/ring buffer with
   `glBufferSubData` into sub-ranges (or `glMapBufferRange` with
   `UNSYNCHRONIZED`) would cut per-draw allocations — the measured hot cost.
2. **Reduce draw-call count** — the menu issues 200-900 immediate-mode draws/
   frame; batching consecutive compatible inline draws is the structural fix but
   is a larger change.
3. **Vulkan/Metal** (see above) — the structural ceiling on Apple Silicon.

These are no longer "minimal changes." Opt #1 (~20%, committed) is the
reasonable stopping point for the minimal-change mandate; further gains require
structural work or the Vulkan port.

## ⛔ Aside: direct-from-shell launch crashes (not how users run it)

Booting Halo 2 crashes deterministically (EXC_BAD_ACCESS, null deref) inside
**Apple's GL driver** (`libGLImage.dylib` `glgProcessPixelsWithProcessor` →
`AppleMetalOpenGLRenderer ...uploadTextureLevel`). Root cause from the crash
report: **two xemu threads call `glTexImage2D` concurrently**:
- **main thread:** `gl_render_frame` → (surface-cache miss, `tex == 0`) →
  `xb_surface_gl_create_texture` → `glTexImage2D` on the window context
  (`ui/xemu.c:825-837`).
- **pfifo thread:** `pgraph_gl_clear_surface` → `pgraph_gl_surface_update` →
  `pgraph_gl_upload_surface_data` → `glTexImage2D` on the pgraph context.

Apple's Metal-backed GL driver shares a pixel-processing dispatch pool across GL
contexts, and a concurrent texture upload from two threads null-derefs in that
pool. On M5 + macOS 27 (both bleeding-edge) this path is hit during Halo 2's
boot surface churn. **The crash site is exactly the surface-upload hot path**
that would dominate Halo 2 perf anyway — so the crash and the slowness are the
same subsystem.

Implications:
- The "make it faster" task is really "make it *run*, then faster" on this HW.
- Candidate minimal mitigation: serialize GL texture uploads so the main-thread
  display path and the pfifo render path can't call into the driver's pixel
  pipeline at once (a shared upload mutex, or force the `tex == 0` display
  fallback to run on the pgraph thread/context). Needs interactive iteration to
  verify — which is hard here because the app can't currently complete a boot.
- This strongly reinforces that the durable answer on modern Apple Silicon is
  the **native Vulkan/Metal path** (MoltenVK or KosmicKrisp + a real swapchain
  present), since Apple's OpenGL is both deprecated *and* now crashing.

## ✅✅ BREAKTHROUGH: Vulkan/MoltenVK renderer WORKS on macOS

Phase 0 of the port landed and runs. Status: **xemu renders Halo 2 on the
Vulkan backend via MoltenVK (Metal) on Apple M5 — confirmed visually smooth by
the user, "miles better than before."**

What was done:
- `meson.build`: darwin branch resolves `dependency('vulkan')` from the SDK.
- `build.sh`: locates the installed Vulkan SDK, exports `VULKAN_SDK` and adds its
  pkgconfig to `PKG_CONFIG_PATH`. (Bundling MoltenVK into the .app: TODO.)
- `vk/renderer.h`: `HAVE_EXTERNAL_MEMORY = 0` on `__APPLE__` + readback buffer
  fields in the display state.
- `vk/instance.c`: portability-enumeration instance ext + create flag,
  portability-subset device ext, external-memory device exts conditional,
  `geometryShader` made optional on macOS.
- `vk/renderer.c`: GL context (`g_gl_context`) created unconditionally (needed
  for the present texture); `get_framebuffer_surface` uses the sync→render path
  for both interop and readback.
- `vk/display.c`: **CPU-readback present** — render to `disp->image`, copy to a
  host-visible buffer, `glTexSubImage2D` into `gl_texture_id`, present via the
  existing GL path. Avoids the GL/Vulkan external-memory interop Apple GL can't do.

Surprise: **MoltenVK 1.4.350 accepts the geometry-shader pipeline stages** (it
appears to emulate them) — no VK errors, pipelines create/bind, ~400 draws/frame
render correctly. So the planned geometry-shader-free rewrite (Phases 1-2) may
not be necessary; keep as fallback if specific scenes glitch.

### Measured (Halo 2 menu, the previously-"skippy" scene)
- OpenGL: collapsed to **3-5 fps** (85-220 ms/frame) at ~400 draws/frame.
- Vulkan/MoltenVK: **~29-30 fps** (22-37 ms/frame) at the same ~400 draws/frame.
- ≈ **6-10× improvement.** Runs without crashes or validation errors.

### Remaining to ship
- Bundle `libvulkan`/`libMoltenVK` + ICD json into `dist/xemu.app` and set the
  loader env from the bundle at startup, so it runs without an installed SDK.
- Default the macOS renderer to Vulkan.
- Gameplay validation (in progress) — watch for scenes where MoltenVK's
  geom-shader emulation might differ; if found, do Phases 1-2.

## (historical) Was a FUNDAMENTAL BLOCKER: Metal has no geometry shaders

Investigated the full Vulkan/MoltenVK path (SDK 1.4.350 installed, instance/
device compat drafted). It is blocked by a hardware/API-level limitation, not a
wiring gap:

- xemu's Vulkan renderer marks **`geometryShader` as a *required* device
  feature** (`vk/instance.c` `desired_features`). If absent, device creation
  aborts and the renderer can't initialize.
- `pgraph_glsl_need_geom()` (`glsl/geom.c`) returns **true for TRIANGLES,
  TRIANGLE_STRIP/FAN, QUADS, QUAD_STRIP, LINES/LOOP/STRIP, and POLYGON** — i.e.
  every primitive except POINTS. The geometry shader performs essential work:
  quad→triangle decomposition (`calc_quadz`), Z-perspective (`calc_triz`),
  provoking-vertex/winding correction. Without it, virtually nothing renders
  correctly.
- **Metal has no geometry shader stage.** `vulkaninfo` on this M5 confirms
  MoltenVK reports `geometryShader = false` (also `wideLines = false`).
  KosmicKrisp is *also* a Vulkan-on-Metal driver, so it has the same limitation
  (geometry shaders aren't something a Metal backend can expose; MoltenVK
  emulates tessellation via compute but not geometry shaders).

**Conclusion:** This is *why* xemu's `meson.build` only wires Vulkan for Windows
and Linux — the renderer can't run on any Metal-based Vulkan driver as-is.
"Move to Vulkan/Metal" is therefore **not a wiring task**; it requires
re-architecting the primitive pipeline to eliminate geometry shaders (emulate
quad/line/triangle expansion + Z-perspective via **compute pre-pass or
CPU-side/vertex expansion**). That is a large graphics-engineering project
(weeks), independent of (and larger than) the present-path issue below.

Verification done up front (per "profile everything"): SDK installed, MoltenVK
device features dumped, geometry-shader dependency traced through the renderer —
so we know this *before* committing build cycles to a dead end.

### CHOSEN PATH: geometry-shader-free Vulkan renderer (phased)

Geometry shader does only: (1) primitive expansion, (2) Z-perspective slope
(`calc_triz`/`calc_quadz` → `triMZ`, `vtxPos0/1/2`), (3) provoking-vertex/winding
(`tri_rot`). MoltenVK lacks `VK_EXT_provoking_vertex` dynamic state, so winding/
provoking is handled via CPU index ordering. Plan:

- **Phase 0 — plumbing & device init (build milestone):** meson darwin vulkan
  dep; `HAVE_EXTERNAL_MEMORY=0` on darwin; instance: portability-enumeration +
  portability-subset, external-memory device exts conditional, `geometryShader`
  made optional; CPU-readback present in `vk/display.c`; build.sh SDK env +
  bundle MoltenVK. Goal: compiles, MoltenVK device initializes, presents.
- **Phase 1 — CPU primitive expansion:** in `vk/draw.c`, expand QUADS/QUAD_STRIP
  (→ triangle list), TRIANGLE_FAN/STRIP, LINE_* on the CPU; map topology to
  non-adjacency; never attach a geometry stage (`need_geom`→false on darwin).
  Goal: geometry renders (winding via index order).
- **Phase 2 — Z-perspective in VSH:** replicate `calc_triz`/`calc_quadz` per-
  vertex in the vertex shader, emit `triMZ`/`vtxPos0/1/2` as flat varyings so the
  fragment shader matches the geom-shader output. Goal: correct depth.
- **Phase 3 — polygon line/point modes, edge cases, perf tuning.**

### Other options (not chosen)
- **A. Big rewrite — geometry-shader-free Vulkan renderer.** Replace geom-shader
  primitive emulation with compute/vertex expansion so the Vulkan renderer runs
  on MoltenVK/KosmicKrisp. Highest ceiling, weeks of work, real risk.
- **B. Stay on OpenGL, attack the real per-frame cost** — persistent/ring vertex
  buffers + draw batching (targets the measured `glBufferData`/draw hot cost),
  and/or the CPU/TCG + audio side that makes *boot* skippy. Tractable now.
- **C. Profile boot first** to see if the first-startup skippiness is even
  GPU-bound (could be CPU/TCG or audio), then pick A or B with data.

## ⚠️⚠️ (Secondary) Even if geom shaders worked: the Vulkan backend can't present on macOS

Deeper investigation (`hw/xbox/nv2a/pgraph/vk/display.c`) found that the Vulkan
renderer does **not** present via a Vulkan swapchain. It renders the NV2A frame
into a `VkImage` backed by *exported external memory*, then **imports that memory
into an OpenGL texture** and draws it through the existing GL window. The two
code paths are:
- `#ifdef WIN32` → `glImportMemoryWin32HandleEXT`
- `#else` (Linux) → `glImportMemoryFdEXT`

Both finish with `glCreateMemoryObjectsEXT` + `glTexStorageMem2DEXT`. Those are
**OpenGL 4.5 + `EXT_memory_object`/`EXT_external_objects`**. Apple's OpenGL is
frozen at **4.1 and has none of them.** There is **no CPU-readback path and no
native VkSurfaceKHR/swapchain present path.**

**Consequence:** enabling MoltenVK is necessary but *not sufficient*. The
renderer would run on Metal but have no way to put pixels on screen on macOS.
Making it work requires a **new macOS present path** — either:
- (A) a native Vulkan swapchain (create the SDL window with `SDL_WINDOW_VULKAN`
  instead of the hardcoded `SDL_WINDOW_OPENGL` at `ui/xemu.c:1024`, and port the
  ImGui overlay to the Vulkan backend), or
- (B) a CPU readback each frame (`vkMapMemory` → upload to a GL texture), which
  is simpler but adds a full-framebuffer GPU→CPU→GPU copy per frame.

This is a **port, not a minimal change** — which is what upstream xemu actually
had to build for macOS Vulkan. It contradicts the "most minimal changes"
constraint, so it needs a decision before proceeding.

## Note: KosmicKrisp (LunarG, Vulkan 1.3 on Apple Silicon)
KosmicKrisp is a Vulkan→Metal driver that reached Vulkan 1.3 conformance and is
upstreamed to Mesa; it'll ship in the macOS Vulkan SDK alongside MoltenVK. It is
a *Vulkan* driver, so it does **not** address the present-path blocker above
(which is on the *OpenGL* side). It is, however, the preferred ICD target **if**
we later build a native Vulkan-swapchain present path (no GL interop) — fuller
1.3 coverage than MoltenVK, drop-in as an alternate ICD. Not in the 1.4.350 SDK
yet; LunarG notes conformance ≠ app compatibility. Back-pocket for a future
Vulkan phase, not relevant to the current OpenGL tuning.

## Background: Vulkan is also not compiled on macOS

`meson.build` (line ~2387) only enables the `vulkan` dependency for
`host_os == 'windows'` and `'linux'`. On **macOS it is `not_found`**, so
`CONFIG_VULKAN` is undefined and the entire `hw/xbox/nv2a/pgraph/vk/` backend is
**never built**. This Mac binary can only run **OpenGL** (or NULL).

That matters because Apple **deprecated OpenGL** — it's frozen at 4.1 and runs
on a slow compatibility layer. The Vulkan backend exists and is complete; it
would run on **Metal via MoltenVK**, the native fast path. So the headline lever
for "Halo 2 on a MacBook" isn't a config toggle — **the fast renderer isn't in
the build at all.** The subproject wraps it needs already exist
(`volk`, `glslang`, `SPIRV-Reflect`, `VulkanMemoryAllocator`); what's missing is
a darwin branch that points `vulkan` at a MoltenVK/Vulkan loader, plus bundling
`libMoltenVK.dylib` into the `.app`. (Upstream xemu does exactly this.)
MoltenVK is not currently installed on this machine.

## Why it's slow on a Mac (hypothesis, ranked)

1. **Stuck on OpenGL because Vulkan isn't compiled (see above).** Biggest lever;
   not a one-liner — requires wiring MoltenVK into the macOS build.
2. **Surface (render-target) up/downloads.** Halo 2 leans heavily on
   render-to-texture and reading surfaces back. Each readback can force a
   GPU→CPU sync. Tracked by `NV2A_PROF_SURF_DOWNLOAD` / `_UPLOAD` /
   `_SURF_TO_TEX`.
3. **Shader / pipeline generation stalls.** First-time shader translation and
   pipeline creation stall the frame. `perf.cache_shaders` already defaults on;
   the question is hit-rate during gameplay (`NV2A_PROF_SHADER_GEN` vs
   `_SHADER_BIND_NOTDIRTY`, `_PIPELINE_GEN`).
4. **CPU/TCG translation of the Pentium III.** Real, but hardest to move and
   least "minimal" — treat as out of scope unless 1–3 don't get us there.

## Steps

### 1. Baseline (no code changes)
- Boot Halo 2, reach a known-heavy scene (e.g. first outdoor area).
- Open the in-app debug overlay (`ui/xui/debug.cc`) — it already plots **FPS**,
  **MSPF**, and every `NV2A_PROF_*` counter from `g_nv2a_stats`.
- Record FPS/MSPF and the top counters **on OpenGL**, then switch the renderer
  to **Vulkan** at runtime (Settings → Display → Renderer → Backend) and record
  the same scene again. No rebuild needed — both backends are compiled in.

### 2. The real win: enable the Vulkan/MoltenVK backend on macOS
This is the change that actually unlocks performance, and it's "minimal" in the
sense of being targeted (a build/packaging change, not new emulation logic):
- Add a `darwin` branch in `meson.build` so `vulkan` resolves to a MoltenVK /
  Vulkan-loader dependency (Vulkan SDK for macOS, or a MoltenVK from Homebrew /
  the SDK). The `vk/` C code and subproject wraps are already present.
- Bundle `libMoltenVK.dylib` (and the loader, if used) into `dist/xemu.app` via
  the `package_macos` step in `build.sh`.
- Then make Vulkan the macOS **default** (`get_default_renderer()` at
  `hw/xbox/nv2a/pgraph/pgraph.c:254`, or the `config_spec.yml` default) and
  verify a fresh config boots Halo 2 on Vulkan/Metal.
- Re-run the step-1 measurement on the same scene to quantify the win.

### 3. Targeted hot-path fix (only if profiling demands it)
Pick the **one** dominant counter from step 1 and address just that, e.g.:
- If `SURF_DOWNLOAD`/`SURF_TO_TEX` dominate → look at surface readback /
  surface-as-texture caching in `vk/surface.c` (+ `surface-compute.c`).
- If `SHADER_GEN`/`PIPELINE_GEN` dominate → check shader/pipeline cache
  invalidation in `vk/shaders.c` / `glsl/`.
Keep it surgical; re-measure against the same scene each time.

### 4. Cheap user-facing knobs to confirm headroom
Confirm `display.quality.surface_scale` is `1` (not upscaling) and test with
vsync off to see if we're GPU- or present-bound — diagnostic only, not a shipped
change.

## Build notes
- Host deps installed via Homebrew: `ninja`, `dylibbundler`. meson is
  bootstrapped by QEMU into `build/pyvenv` automatically.
- `build.sh` produces a packaged `.app` with a **duplicate `LC_RPATH`**
  (`@executable_path/../Libraries/arm64/` appears twice), which recent dyld
  rejects with "duplicate LC_RPATH". Workaround applied:
  `install_name_tool -delete_rpath ... && codesign -s - -f`. Worth fixing in
  `package_macos` so it doesn't recur.

## Decision points for you
- This is Apple Silicon (arm64). Given Vulkan isn't compiled here, the real path
  to playable Halo 2 is wiring up MoltenVK (step 2). Are you OK with that scope
  — adding a Vulkan/MoltenVK dependency to the macOS build and bundling it — or
  do you want me to first squeeze what I can out of the existing OpenGL path?
- Where does MoltenVK come from: install the **Vulkan SDK for macOS** (ships
  MoltenVK + loader) vs. a Homebrew `molten-vk`? Affects bundling/licensing.
