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

### Next levers (larger, in priority order)
1. **Cache vertex-attribute state across draws** — `ATTR_BIND` runs ~1.5× per
   draw call (redundant `glVertexAttribPointer`/`glEnableVertexAttribArray`).
   Track enabled-mask + last pointers/format in `PGRAPHGLState` and skip
   unchanged rebinds. Must route *all* attribute paths through the cache
   (`vertex.c` + the inline-buffer branch in `draw.c`) to stay correct.
2. **Reduce per-draw buffer re-specification** — inline buffer/array draws
   re-`glBufferData` every draw; a persistent ring buffer would cut allocations.
3. **Vulkan/Metal** (see above) — the structural ceiling on Apple Silicon.

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

## ⚠️⚠️ Showstopper: the Vulkan backend can't present on macOS

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
