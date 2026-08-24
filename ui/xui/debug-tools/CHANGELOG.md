## v2.89.1 - Source-Owned Target-Aware Capstone Bootstrap

- Moves Debug Tools Capstone dependency preparation into the normal `build.sh` path so upstream Linux/macOS/Windows GitHub workflows can remain byte-identical.
- Adds `build-capstone.sh`, which reuses a compatible target Capstone when available or builds pinned Capstone 5.0.9 as a static library with only `CAPSTONE_X86_SUPPORT=ON`.
- Selects the xemu host/target independently from the Xbox disassembly architecture: Linux x86_64/AArch64, macOS x86_64/ARM64, and Windows x86_64/ARM64 all build an X86-capable Capstone library for their own executable architecture.
- Derives Windows cross architecture from `CROSSPREFIX` so the upstream ARM64 matrix does not incorrectly inherit the container runner's `uname`.
- Retains `build-capstone-windows.sh` as a thin compatibility wrapper for the existing local Docker builder.
- Restores/preserves upstream multi-platform workflow YAML; no Capstone-specific GitHub workflow steps are required.
- No Debug Tools runtime C/C++ behavior changes.

## v2.88.4 - Deferred Reset Disassembler Refresh Hotfix

- Replaces v2.88.3's too-early same-frame reset refresh with a deferred refresh that waits until QEMU has consumed the pending reset request.
- Reset still clears stale debugger Changes ownership immediately, but now sets a dedicated one-shot `g_refresh_disassembly_after_reset` state instead of immediately queuing `m_inject_disasm_refresh_pending`.
- Once `qemu_reset_requested_get() == SHUTDOWN_CAUSE_NONE`, the one-shot queues the existing Inject refresh path exactly once and `RefreshDisassembly()` rereads post-reset RAM.
- Explicitly removes the failed v2.88.3 immediate-refresh behavior; no second cache invalidation mechanism is retained.
- PREENTRY, the v2.88.2 live-XBE-header CodeCave guard, F0/F1 install/restore, and reset Changes ownership behavior are unchanged.

## v2.88.3 - Reset Disassembler Refresh Hotfix (superseded by v2.88.4)

- Attempted to refresh stale disassembly rows by queuing `m_inject_disasm_refresh_pending` on the same debugger frame that cleared Reset ownership.
- Windows/runtime testing showed this could still run before QEMU actually performed the queued reset, causing the disassembler to reread pre-reset RAM and remain stale.
- v2.88.4 removes this immediate-refresh timing and replaces it with a post-reset-completion one-shot.

## v2.88.2 - Debugger Reset / XBE Header CodeCave Safety

- Clears debugger-only NOP/Change/CodeCave Changes ownership when the user performs a real Reset, without writing stale pre-reset bytes back.
- Keeps PREENTRY activation/reset lifecycle separate and unchanged.
- Tracks the active XBE header size in Current Game and blocks debugger CodeCave RUN when the hook overwrite span overlaps the live XBE header region.
- Prevents low-header hooks (commonly 0x00010000-0x00011FFF for a 0x2000-byte header) from modifying the live revision header used by Current Game identity detection.

# xemu Debug Tools Changelog

- Windows macro-collision link hotfix: remove Cheat Engine's `std::ifstream`/`<fstream>` header probe so QEMU's Win32 `close -> qemu_close_wrap` macro cannot rename `std::basic_filebuf::close()`, and rename `TransferKind::CreateDirectory` to `CreateFatxDirectory` so Win32's `CreateDirectoryA/W` macro cannot make the enum differ between translation units under LTO.
- Windows build cache isolation hotfix: keep ordinary compiler ccache reusable across revisions, but key GCC incremental-LTO state to the exact source SHA with no cross-revision restore. This prevents stale pre-hotfix LTRANS state from resurfacing symbols such as `std::basic_filebuf::qemu_close_wrap()` or inconsistent ODR type summaries.
This file preserves the release-by-release history that previously lived in
`README.md`. The README now describes only the current Debug Tools behavior.


## v2.88.1 Debugger CodeCave Startup Ownership Hotfix

- Fixes a startup race where a debugger CodeCave could install successfully, appear briefly in Changes, then lose RESTORE ownership while its hook JMP remained in guest code.
- Stops treating the first delayed Current Game `invalid -> valid` XBE observation as proof that the guest address space changed. This preserves a CodeCave that was installed while XBE metadata was still becoming available.
- Keeps stale-guest safety intact: an already-observed valid game disappearing, Game A changing directly to Game B, or an explicit same-title Reset still forgets old F0 ownership and resets the private cave arena without restoring old-guest bytes into a new address space.
- Adds an explicit one-shot ownership-reset flag for the same-title Reset path so PREENTRY reset behavior remains unchanged.
- Adds regression coverage for startup `invalid -> valid`, valid-to-invalid, valid-to-different-valid, same-guest, and explicit-reset ownership decisions.


## v2.88 Debugger Changes Tab

- Converts the debugger's existing Breakpoints area into `Breakpoints | Changes` tabs without changing breakpoint behavior.
- Adds a Changes table with `Address | Original | Changed | HEX` columns for active restorable Inject NOP/Change patches.
- HEX is a per-row display toggle and defaults off; unchecked rows show instruction text while checked rows show the captured/written raw bytes.
- Reuses the normal debugger address context menu on Changes addresses, and double-click follows the address in the disassembler.
- Tracks the temporary debugger CodeCave/F0 hook as one row at the hook/jump-from address only. The generated F0 cave body/allocation addresses are intentionally not enumerated.
- CodeCave RUN records the captured original hook span plus installed jump bytes; CodeCave RESTORE clears the tracked row.
- Leaves the Cheat Engine/F0 execution core unchanged; Changes tracking is owned by Memory Tools around the existing proven install/restore path.


## v2.87 Regression Test Suite Final Cleanup

- Test-only cleanup on the confirmed v2.86.1 Windows/runtime baseline; no production Debug Tools source or behavior changes.
- Reduces the top-level Debug Tools test/support surface from 145 files to 55 by consolidating 96 historical release-numbered Python guards into six current `v287-*` suites.
- Preserves the retained historical behavioral contracts inside current suites for UI/runtime, HDD/FATX/Kernel RPC, PREENTRY/Patch/Cheat, and platform/build/test infrastructure.
- Replaces eleven stale v2.72-v2.85 per-phase byte/fingerprint guards with `v287-ownership-structure-golden.py`, which checks the current final ownership boundaries semantically; exact whole-production fingerprint ownership remains singular in `v287-final-production-audit-golden.py`.
- Renames the v2.86 final production audit to current v2.87 ownership without changing its 87-file production fingerprint.
- Keeps the v2.85.1 `<algorithm>`/`std::clamp` and v2.86.1 Current Game anonymous-namespace Windows compile regressions active inside the current platform/infrastructure suite.
- Marks every retained top-level Python/C++ golden as `v2.87 current regression ownership` while leaving stable behavior-based filenames unchanged.
- Removes the old cross-test filename dependency where the detached Current Game/HDD guard read `v197-detached-playback-golden.py`; current coverage now resolves through the consolidated v2.87 suite.
- Preserves the permanent HDD/KRPC mutation invariant: `Create/Open -> Write -> Flush -> Close -> fresh FATX Verify`.


## v2.86.1 Current Game Namespace Compile Hotfix

- Restores the closing brace for the anonymous helper namespace in `current-game.cc` that was accidentally removed while v2.86 consolidated duplicate local-file reading.
- Fixes the pinned GitHub-equivalent MinGW/GCC Windows compile failure beginning at `CurrentGameManager::SameIdentity()` and ending with `expected '}' at end of input`.
- No Current Game behavior, label parsing, disc export, HDD/FATX mutation behavior, Kernel RPC behavior, or public header changes.
- Updates the v2.86 production fingerprint and adds `v2861-current-game-namespace-hotfix-golden.py` to guard the required namespace boundary.

## v2.86 Final Debug Tools Production Audit / Cleanup

- Performs the cumulative Phase-11 audit over the complete Debug Tools production tree rather than another ownership-only split. The final production surface is 87 files.
- Removes the obsolete Guest Kernel RPC diagnostic mutation UI and its unreachable delete/import candidate, confirmation, preview, and start helpers. The production HDD frontend continues through `PrepareHdd*` / `StartHdd*` and the generalized filesystem executor.
- Removes the retired standalone single-file diagnostic delete mode, unused operation phases/state, unused Current Game/HDD/guest-pause accessors, and test-era filesystem compatibility wrappers/`TransferPlan` metadata that no production caller used.
- Splits the remaining harmless Kernel RPC diagnostics drawing into `guest-kernel-rpc-ui.cc` and centralizes shared IRQL/NTSTATUS/safe-point constants in `guest-kernel-rpc-status.hh`, leaving the scheduling/execution core free of ImGui rendering.
- Consolidates duplicate host-export name/path helpers, local Current Game file reading, MAP/PDB label sort/dedupe logic, Kernel RPC little-endian readers, and portable-label ASCII uppercase handling into their existing shared helper owners.
- Keeps intentionally separate subsystem adapters separate where merging would create coupling: guest physical-range allocators and HDD block-reader callbacks retain local ownership.
- Preserves the live HDD mutation invariant: `Create/Open -> Write -> Flush -> Close -> fresh FATX Verify`. The Phase-11 guard explicitly follows the production `StartHddDelete` / `StartHddImport` paths and rejects restoration of the retired diagnostic mutation layer.
- Adds `v286-final-production-audit-golden.py`, which fingerprints the complete 87-file production surface and guards dead-code removal, helper ownership, Kernel RPC UI/core separation, and the live HDD/KRPC transaction contracts.

## v2.85.1 Windows HDD UI Compile Hotfix

- Adds the explicit `<algorithm>` include required by `std::clamp` in `hdd-directory-ui.cc`.
- Fixes the native MSYS2/MinGW Windows compile failure at `hdd-directory-ui.cc:564` introduced when Phase 10 moved HDD rendering into its own translation unit.
- No `HddDirectoryWindow` method body, FATX behavior, filesystem transaction behavior, or Kernel RPC behavior changes.
- Adds a focused Windows compile-dependency regression guard for the `std::clamp` include ownership.

## v2.85 Combined Debug Tools Cleanup Phase 10

- Splits Xbox HDD browser rendering/popup ownership into new `hdd-directory-ui.cc`; `hdd-directory.cc` is reduced to action/state/request ownership with no direct ImGui calls. All 28 `HddDirectoryWindow` method bodies remain identical to confirmed v2.84.
- Removes the remaining private FATX copies of little-endian/range helpers. `fatx-hdd.cc` now aliases the established `binary-utils.hh` implementations while preserving existing call sites and parser behavior.
- Splits `ImportHostStream` state plus both `LoadImportFileChunk` overloads into new `kernel-rpc-filesystem-stream.cc`. Shared host write-time identity checking lives in private `kernel-rpc-filesystem-internal.hh`; planning/preflight remains in `kernel-rpc-filesystem.cc`.
- Updates the native regression runner so the filesystem stream translation unit is always compiled/linked under both GCC and Clang for filesystem goldens. Historical HDD/KRPC UI/source guards follow the split implementation view.
- Leaves `guest-kernel-rpc.cc`, `guest-kernel-rpc-completion.cc`, `guest-kernel-rpc-filesystem.cc`, `hdd-snapshot-service.cc/.hh`, public filesystem/HDD headers, and the proven mutation executor byte-identical to v2.84.
- The permanent transaction invariant remains unchanged: `Create/Open -> Write -> Flush -> Close -> fresh FATX Verify`.
- Adds a Combined Phase-10 guard over the complete 84-file production Debug Tools surface, exact Phase-10 file hashes, unchanged executor hashes, preserved HDD method bodies, FATX helper ownership, host-stream body hashes, Meson registration, and native-runner coverage.

## v2.84 Debug Tools Cleanup Phase 9

- Splits the 3,301-line Cheat Engine runtime core into focused translation units without changing any `CheatEngineWindow` method body.
- `cheat-engine.cc` becomes a 99-line per-frame coordinator owning only `Tick()`.
- New `cheat-engine-source.cc` owns source/header parsing, cheat-file discovery/loading, game identity, and PREENTRY/Patch selection/application lifecycle.
- New `cheat-engine-fhooks.cc` owns debugger/Live Cheat F0/F1 parsing, precompile/install, captured-original-byte restoration, cave retirement/reclamation, and hook ownership.
- New `cheat-engine-execute.cc` owns guest read/write helpers and RAW command execution, including Type 9 contexts, conditionals, and Type F execution.
- Existing `cheat-engine-ui.cc` remains byte-identical; `cheat-engine.hh` is unchanged. Non-UI Cheat Engine units contain no direct ImGui rendering calls.
- Historical source-level guards now use the combined Cheat Engine implementation view; the Pass-10 method freeze is order-independent so translation-unit movement cannot masquerade as a behavior change.
- Adds a Phase-9 guard over the complete 81-file production Debug Tools surface, exact Phase-9 file hashes/ownership, unchanged public header/UI, and all 85 v2.83 Cheat Engine method bodies.
- No Live Cheat F0/F1 enable-disable semantics, original-byte restore, Patch/PREENTRY behavior, Inject/CodeCave behavior, Memory Tools, HDD/FATX, or Kernel RPC behavior changes.

## v2.83 Combined Debug Tools Cleanup Phase 8

- Combines Inject ownership cleanup with the compact x86 Cheat Assembler structural split.
- Splits `DrawInstructionChanger`, `DrawCodeCaveBuilder`, and `DrawAddressContextMenu` byte-for-byte into new `memory-tools-inject-ui.cc`. The eight remaining Inject action/state methods stay byte-identical to v2.82, leaving `memory-tools-inject.cc` with no direct ImGui rendering calls.
- Splits low-level operand parsing/instruction encoding into new `x86-cheat-assembler-encode.cc` behind private `x86-cheat-assembler-internal.hh`; `x86-cheat-assembler.cc` retains F0 directive/temp/preserve expansion and the public assembler entry points.
- Preserves all 19 low-level encoder function bodies and all 29 F0/directive/public frontend function bodies from v2.82; only the internal linkage/translation-unit ownership needed by the split changes.
- Updates the native assembler golden to compile both assembler translation units under GCC and Clang, while historical Inject Restore and refresh-anchor guards follow the split source view.
- Adds a Combined Phase-8 guard over the complete 78-file production Debug Tools surface, exact Inject core/UI method digests, exact assembler body digests, unchanged public headers, and Meson/native-test registration.
- No Cheat/F0 enable-disable lifecycle, Patch/PREENTRY, debugger navigation, Memory Viewer, Search/Dump, Labels/Current Game, Type-F/CodeCave semantics, HDD/FATX, or Kernel RPC behavior changes.

## v2.82 Combined Debug Tools Cleanup Phase 7

- Combines the next low-risk cleanup areas: Memory Search, RAM Dump, and the
  Microsoft label-parser backend helpers.
- Splits `DrawSearch()` byte-for-byte from `memory-tools-search.cc` into new
  `memory-tools-search-ui.cc`; all 10 Search execution/comparison/snapshot methods
  remain byte-identical to v2.81.
- Splits `DrawDumpRam()` byte-for-byte from `memory-tools-dump.cc` into new
  `memory-tools-dump-ui.cc`; all 10 dump/path/map/write methods remain
  byte-identical to v2.81, including the deliberate leave-paused full RAM dump.
- Consolidates the duplicated ASCII lowercase and common Microsoft symbol-display
  filtering into `label-symbol-utils.hh`. XDK now uses shared `lower_ascii`; MAP
  and PDB share the common compiler-internal/display path while PDB retains its
  two PDB-only generated-symbol exclusions. XBE parsing remains byte-identical.
- Adds a Combined Phase-7 guard over the complete 75-file production Debug Tools
  surface, exact moved UI bodies, unchanged Search/Dump core methods, parser helper
  ownership, headers, and Meson registration.
- No Cheat/F0 restore, Patch/PREENTRY, debugger/Inject, Memory Viewer, Current Game,
  Type-F/CodeCave, HDD/FATX, or Kernel RPC behavior changes.

## v2.81 Debug Tools Cleanup Phase 6

- Splits Labels rendering ownership out of the 614-line `memory-tools-labels.cc`
  implementation into a dedicated `memory-tools-labels-ui.cc` translation unit.
  The label dump/export core is reduced to 141 lines and the UI owner is 510 lines.
- Moves `DrawLabelBrowser` byte-for-byte from the confirmed v2.80 baseline and
  keeps `DumpLabels` byte-identical to v2.80. `memory-tools.hh` and
  `memory-tools-internal.hh` remain unchanged.
- Updates combined MemoryTools source inspection so historical label, display-cache,
  lifecycle, table-render, and register-copy guards follow the new translation-unit
  ownership without relaxing their protected behavior.
- Adds a Phase-6 regression guard over the complete 73-file production Debug Tools
  surface, the exact moved UI body, the unchanged dump/export body, and Meson/source
  ownership.
- No Cheat/F0 restore, Patch/PREENTRY, debugger/Inject, Memory Viewer, Current Game,
  Type-F/CodeCave, HDD/FATX, or Kernel RPC behavior changes.

## v2.80 Debug Tools Cleanup Phase 5

- Splits Current Game rendering ownership out of the 1,233-line
  `current-game.cc` implementation into a dedicated `current-game-ui.cc`
  translation unit. The XBE/disc/label state core is reduced to 907 lines and
  the UI owner is 340 lines.
- Moves `DrawInlineSummary`, `DrawGameInfoTab`, `DrawDiscEntry`,
  `DrawDiscContentsTab`, and `Draw` byte-for-byte from the confirmed v2.79
  baseline.
- Keeps all 25 non-rendering `CurrentGameManager` methods byte-identical to
  v2.79, including XBE refresh/detection, mounted-disc hashing, label/XDK/MAP/PDB
  loading, and `RequestDiscExport`. `current-game.hh` remains unchanged.
- Moves UI-only dependencies (`tab-style.hh`, `font-manager.hh`,
  `hdd-directory.hh`, and `guest-kernel-rpc.hh`) out of the Current Game core.
- Adds a Phase-5 regression guard over the complete 72-file production Debug
  Tools surface, exact moved-function hashes, and the unchanged core method set.
  Historical guards follow the new ownership boundary without relaxing their
  unaffected behavior coverage.
- No Cheat/F0 restore, Patch/PREENTRY, debugger/Inject, Memory Viewer,
  Type-F/CodeCave, HDD/FATX, or Kernel RPC behavior changes.

## v2.79 Debug Tools Cleanup Phase 4

- Splits Memory Viewer rendering/edit ownership out of the 1,111-line
  `memory-tools-memory.cc` implementation into a dedicated
  `memory-tools-memory-ui.cc` translation unit. The mapping/state core is
  reduced to 540 lines and the UI owner is 584 lines.
- Moves `PrepareMemoryByteEdit`, `DrawScrollableMemoryPane`,
  `DrawMemoryMapPane`, and `DrawMemoryWorkspace` byte-for-byte from the
  confirmed v2.78 baseline.
- Keeps all 11 remaining Memory Viewer mapping/selection/map-refresh methods
  byte-identical to v2.78 and leaves `memory-tools.hh` plus
  `memory-tools-internal.hh` unchanged.
- Removes render-only standard-library dependencies from the core; ImGui
  rendering now belongs exclusively to the new Memory Viewer UI translation
  unit.
- Adds a Phase-4 regression guard over the complete 71-file production Debug
  Tools surface, exact moved-function hashes, and the unchanged core method set.
  Historical guards follow the new ownership boundary without relaxing their
  unaffected behavior coverage.
- No Cheat/F0 restore, Patch/PREENTRY, debugger, Inject/Restore, Type-F/CodeCave,
  HDD/FATX, or Kernel RPC behavior changes.

## v2.78 Inject disassembly refresh anchor fix

- Fixes the x86 Debugger address-state leak where editing an instruction after
  **Follow**, Right-arrow navigation, Back/Forward, or another Go-To could make
  the next Inject refresh jump back to the older navigation target.
- NOP, Change Apply/Restore, tracked `Inject > Restore`, and CodeCave
  RUN/RESTORE now call the existing `FollowDebuggerAddress(target, false)`
  immediately before scheduling their normal disassembly refresh. The edited
  instruction therefore becomes the refresh anchor without creating a new
  Back/Forward history entry.
- Keeps the v2.77 debugger core/UI split and `memory-tools.hh` byte-identical;
  the only production runtime change is `memory-tools-inject.cc`.
- No breakpoint/watchpoint semantics, Cheat/F0 uncheck restore, Patch/PREENTRY,
  Type-F allocation, HDD/FATX, or Kernel RPC behavior changes.

## v2.77 Debug Tools Cleanup Phase 3

- Splits x86 debugger rendering out of the 3,258-line
  `memory-tools-debugger.cc` implementation into a dedicated
  `memory-tools-debugger-ui.cc` translation unit. The state/execution core is
  reduced to 1,573 lines while the rendering owner is 1,716 lines.
- Moves all eight debugger `Draw*` methods byte-for-byte from the confirmed
  v2.76 baseline: breakpoint-condition UI, register tables/views, F0 temporary
  registers, breakpoint/watchpoint UI, disassembly pane, and main debugger UI.
- Keeps all 39 non-rendering debugger methods byte-identical to v2.76, including
  breakpoint/watchpoint installation/removal, condition application, navigation,
  register writes, disassembly refresh, and breakpoint-hit state.
- Moves frontend-only includes (`register-copy-utils.hh`, `tab-style.hh`,
  `cheat-engine.hh`, and `font-manager.hh`) out of the debugger core and into
  the new UI translation unit.
- Adds a Phase-3 regression guard over the complete 70-file production Debug
  Tools surface, exact moved-function hashes, and the unchanged non-rendering
  debugger method set. Historical guards follow translation-unit ownership
  without relaxing their unaffected runtime coverage.
- No Live Cheat/F0 restore, Patch/PREENTRY, Inject/Restore, Type-F/CodeCave,
  HDD/FATX, or Kernel RPC behavior changes.

## v2.76 F0/F1 uncheck restore hardening

- Audits the confirmed v2.74 behavior through the v2.75 Cheat Engine UI split and
  confirms the underlying `DeactivateFHook()` original-byte write was not removed.
- Strengthens individual Cheat checkbox OFF so it always sets the block disabled
  and calls `DeactivateFHooksForBlock()` before considering the global Live Cheats
  state. A persistent F0/F1 hook therefore restores its captured original hook
  bytes even if UI/global state has become temporarily out of sync.
- Applies the same unconditional OFF/restore contract to group deselection. ON
  behavior remains unchanged and still requires Live Cheats to be enabled.
- Adds a dedicated regression guard that fingerprints the complete production
  Debug Tools surface and the two changed UI methods, while also verifying the
  real F0/F1 deactivation path still writes `state.original_bytes` through
  `xemu_cheat_patch_virtual()`.
- No F0 assembly/install semantics, Type-F allocation/retirement, PREENTRY/Patch,
  Inject/Restore, debugger, HDD/FATX, or Kernel RPC behavior changes.

## v2.75 Debug Tools Cleanup Phase 2

- Splits the Cheat Engine frontend out of the 4,009-line `cheat-engine.cc`
  monolith into a dedicated `cheat-engine-ui.cc` translation unit. The runtime
  core is reduced to about 3,300 lines and the frontend owner is about 724
  lines.
- Moves group/Patch selection helpers plus Cheat/Patch drawing, menu/help, and
  the main `Draw` frontend without rewriting their bodies. Every moved function
  is SHA-256 checked against the confirmed v2.74 implementation.
- Keeps `CheatEngineWindow::Tick()` in `cheat-engine.cc` because it owns
  execution/F-hook/PREENTRY lifecycle work rather than rendering.
- Removes `mixed-checkbox.hh` and `tab-style.hh` from the runtime translation
  unit; those frontend-only dependencies are now owned by `cheat-engine-ui.cc`.
- Adds a combined Cheat Engine source-view helper for historical semantic
  goldens so regression coverage follows class behavior instead of depending on
  which translation unit owns a method.
- Historical v2.72-v2.74 fingerprints are narrowed only for the explicit Phase-2
  files; their unaffected production surfaces remain byte-frozen.
- No Cheat/Patch behavior, PREENTRY lifecycle, Type-F/CodeCave behavior,
  Inject/Restore behavior, HDD/FATX behavior, or Kernel RPC behavior changes.

## v2.74 Inject Restore / Change crash fix

- Fixes the `Inject > Change` crash triggered by `REVERT TO ORIGINAL`. The old
  button could change `m_change_instruction_applied` before the matching
  conditional `EndDisabled()`, leaving ImGui with an unbalanced disabled stack.
  Change now uses one unconditional `BeginDisabled(bool)` / `EndDisabled()`
  scope around Apply.
- Simplifies the Change editor to the requested two actions: `APPLY` and
  `RESTORE`. `RESTORE` puts the remembered original instruction back into the
  Replacement field, restores the exact captured original bytes through the
  normal transactional Apply path, and clears active patch ownership.
- Extends the existing remembered-original record to `Inject > NOP`. NOP now
  records the complete original instruction span and the exact NOP bytes it
  wrote.
- Adds conditional `Inject > Restore` for any disassembly row inside a tracked
  NOP/Change span. Restore validates that live bytes still match the tracked
  patch before writing the saved originals, so a newer/external patch is never
  overwritten silently.
- Prevents NOP/Change from creating a second overlapping patch when the selected
  disassembly row is inside (but not at the start of) an already tracked span.
- No CodeCave/Type-F, PREENTRY/Patch, HDD/FATX, or Kernel RPC behavior changes.

## v2.73 Debug Tools Cleanup Phase 1

- Starts the behavior-preserving Debug Tools folder cleanup from the v2.72
  cumulative rebuild baseline.
- Inventory found no trivially dead file-local production functions: every
  `static` function has at least one in-translation-unit reference.
- Consolidates 15 local helper definitions into four shared helpers, eliminating
  11 redundant copies:
  little-endian 16/32-bit reads and overflow-safe range checks now live in
  `binary-utils.hh`, while the repeated ASCII uppercase helper lives in the
  existing `label-symbol-utils.hh`.
- Removes the duplicate local implementations from Current Game plus the XBE,
  XDK, PDB, XDVDFS, and MAP parsers without changing behavior. The affected parser goldens
  compile and run under both GCC and Clang.
- Historical source-freeze guards now delegate only this exact cleanup surface
  to `v273-debug-tools-cleanup-phase1-golden.py`; the remaining v2.72 production
  surface stays fingerprinted instead of broadly relaxing earlier guards.
- No PREENTRY/Patch, debugger, Type-F, HDD/FATX, or Kernel RPC runtime logic is
  changed. The permanent filesystem mutation transaction remains
  `Create/Open -> Write -> Flush -> Close -> fresh FATX Verify`.

## v2.72 cumulative Debug Tools rebuild baseline / audit

- Establishes the v2.71 production Debug Tools source as the explicit rebuild
  baseline before any further behavior-bearing work.
- Adds one cumulative production-source fingerprint over all 67 shipping Debug
  Tools files (including `meson.build`) so an unplanned edit, added file, removed
  file, or ownership drift is caught by the normal Static regression phase.
- No runtime behavior changes: PREENTRY/Patch lifecycle, debugger behavior,
  Type-F ownership, FATX/KRPC transaction semantics, and the v2.71 ImGui
  tab-style stack hotfix remain source-identical to the verified v2.71 base.

## v2.71 targeted formatting / pruning cleanup

- Hotfix: restore Debug Tools tab-style colors immediately after each tab bar so ImGui window stack validation never sees outstanding `PushStyleColor()` entries; keep the RAII destructor as early-return fallback.

- Windows/LTO compile-link hotfix: `guest-pause-guard.hh` no longer includes
  `qemu/osdep.h`. The guard implementation moved to `guest-pause-guard.cc`,
  preventing QEMU's Windows `#define close qemu_close_wrap` from leaking into
  C++ standard headers such as `<fstream>` and renaming
  `std::basic_filebuf::close()`.
- Compile hotfix: `RememberPreEntrySelection` now takes a mutable `CheatBlock &` because deselection clears that block's `preentry_error`; the prior `const CheatBlock &` signature was ill-formed.
- Removes formatting residue introduced by the Debug Tools cleanup sequence and
  clarifies current validation-helper documentation. No runtime behavior changes.

## v2.70 temporary Git-index validation

- Adds `--temporary-git-index` so validation/packaging can exercise executable-bit
  Git index updates without mutating the repository's real `.git/index`.
- Keeps the existing CI `--update-git-index` behavior unchanged.

## v2.69 README / CHANGELOG documentation split

- Keeps the operational README focused on current behavior and ownership.
- Moves the complete historical release narrative into this CHANGELOG without
  deleting the prior notes.

## v2.68 semantic historical-test normalization

- Replaces brittle exact-string PREENTRY header stripping with semantic
  declaration/field stripping while reproducing the original v1.94 normalized
  header byte-for-byte.

## v2.67 regression-runner optimization / QoL

- Adds per-command timing and optional `--jobs` parallelism for independent
  static golden tests.
- Reuses compiled FATX and kernel-filesystem objects within each native compiler
  matrix.

## v2.66 heavy randomized test phase split

- Static keeps quick deterministic model checks; Heavy owns the original large
  randomized debugger/F0 model iteration counts.

## v2.65 HDD performance-measurement cleanup

- Consolidates repeated local timing guards into one scoped measurement owner
  without changing the counters or FATX behavior being measured.

## v2.64 FATX verification buffer reuse

- Reuses the two host compare buffers across final Copy verification chunks while
  preserving fresh snapshot identity and byte-for-byte comparison.

## v2.63 scoped Debug Tools tab style

- Replaces manual tab-color Push/Pop pairs with a scoped RAII owner; the
  inactive-grey / hover-blue / selected-green palette is unchanged.

## v2.62 indeterminate group checkboxes

- Partially selected Cheat/Patch groups draw the mixed state inside the checkbox
  instead of a separate `[-]` marker.

## v2.61 Patch lifecycle summary QoL

- Shows Selected / Applied / Reset Required / Failed counts and warns when the
  master Engine Enabled gate is off with Patches staged.

## v2.60 per-Patch PREENTRY diagnostics

- Stores the last failed startup execution error on the individual Patch row and
  exposes `[ERROR - RESET REQUIRED]`.

## v2.59 stale PREENTRY selection pruning

- Removes unreachable selection keys only for the currently parsed file while
  preserving staged selections for other games.

## v2.58 stable duplicate-safe block identity

- Uses file + group path + block name + occurrence ordinal for live reload state
  and PREENTRY selection identity.

## v2.57 deterministic matching-file selection

- Uses stable case-insensitive filename ordering as a score tie-break and reports
  ambiguous authoritative matches.

## v2.56 cheat-file discovery optimization

- Reads only a bounded header prefix for candidate matching, validates the
  already-loaded file first on same-title reload, and checks filename-prefixed
  candidates before generic filenames while keeping the header authoritative.

## v2.55 valid-to-valid PREENTRY startup detection

- Treats a real Current Game identity change from one valid XBE to another as a
  startup boundary without competing with explicit Reset ownership.

## v2.54 game-identity live-cheat safety

- Clears old live Cheat execution/selection state on a guest identity change even
  when Auto-load is disabled, without restoring stale Type-F bytes into the new
  guest or clearing staged Patch selections.

---

# Windows-only v0.1.42 backport: Step Into + Type F

This source tree keeps the original v0.1.42 Windows-only build/platform layout and
backports only the later debugger stepping fix and finalized Type-F code-cave engine.
No Linux/macOS builder restoration, local MINGW builder, DLL packaging, cv2pdb, or
other v0.1.43-v0.1.59 build-system changes are included.

Backported runtime features:

- WHPX explicit one-shot **Step Into** behavior from v0.1.52.
- F0 automatic 32-bit x86 assembly caves with labels and `DEADCODE`.
- F1 aligned raw-hex caves with `DEADCODE 000000NN` (`NN=01..08`).
- Type-F stable free-list lifecycle: disabling restores the hook, safely clears/frees only that cave, coalesces adjacent free blocks, and never shifts another active cave.
- Automatic external 1 MiB xemu-owned mapped arena, split into 960 KiB for
  F0/F1 code + DD data and 64 KiB for preservation/T0-T7 private state; includes cave allocation,
  hook sizing, JMP in, generated JMP back, and original hook-byte restoration.
- Optional leading `$` on all Type-F lines.





## v0.1.65 private TFLAGS + exact debugger navigation/layout

- Every T-using F0 now owns a private `TFLAGS` state alongside `T0-T7`.
  Flag-producing instructions that involve a T register save the guest EFLAGS,
  execute, capture the resulting flags into `TFLAGS`, then immediately restore
  the guest EFLAGS.
- Following `Jcc`, `SETcc`, `CMOVcc`, and `LOOPcc` instructions consume private
  `TFLAGS` until a normal x86 flag-writing instruction takes ownership of the
  architectural EFLAGS again. This keeps T-register comparisons/loop counters
  from leaking flag changes back into the game.
- The logical private bank is now 40 bytes: T0-T7 (32 bytes), TFLAGS (4 bytes),
  and one hidden saved-game-EFLAGS scratch dword (4 bytes). The allocator still
  rounds the allocation to its existing 16-byte boundary.
- The debugger F0 Temp Registers pane now shows read-only `TFLAGS` and decodes the
  common CF/ZF/SF/OF/PF/AF bits.
- Typing or following a debugger address now resolves to the containing opcode
  start. If the requested byte is in the middle of an instruction, the input,
  selection, and navigation history are normalized to that instruction start.
- The lower debugger layout is compacted to `Current Registers | Last BP` with
  F-Type temporary state below that pair, while Breakpoints moves to the right.
  This leaves the disassembly workspace free to expand horizontally with the
  window instead of forcing the register panes to stretch across the full width.
- Added a true full-width horizontal resize handle immediately below the paired
  Virtual/Physical disassembly panes. Dragging it resizes both panes vertically
  from 160 to 1200 px; double-click resets to 320 px. The handle lives above
  the Physical-pane explanatory text so its drag target cannot be obscured.
- Fixed Follow/keyboard branch navigation so the scroll-to-destination request is
  applied equally from either Virtual or Physical pane instead of being cleared
  when the Physical pane owned keyboard focus. Right now follows the selected
  branch/call/return and positions the destination in the disassembly view.
- Single-clicked branch sources are synchronized into navigation history before
  their target is pushed, so Left returns to the exact instruction the branch was
  followed from. Context-menu Follow/Back/Forward is deferred until both panes
  finish drawing, avoiding disassembly-row invalidation while a menu is active.
- Disassembly hover is now neutral grey. Hovering a row never changes selection;
  a normal single left-click remains the action that selects an instruction.

## v0.1.64 F0 persistent T0-T7 temp registers

- Added eight F0-only virtual 32-bit scratch registers: `T0` through `T7`.
- Each active F0 hook that references a T register receives one persistent
  private state bank in the existing `680F0000-680FFFFF` private area.
  Different F0 cheats never share a T bank.
- T values are not cleared when the cave returns; the last values remain
  visible for debugging and are reused on the next execution. The bank is
  zeroed/freed only when that F0 is disabled, the XBE changes, or Type-F
  allocations are reset.
- Common syntax includes `mov T0,CarList`, `mov T1,[T0]`, `mov eax,T0`,
  `mov T0,eax`, `cmp eax,T1`, `test T1,T1`, `add T0,4`, and ordinary unary/
  shift/push/pop forms that accept a dword memory operand. `[Tn]` means a
  32-bit memory access through the pointer stored in Tn.
- The x86 Debugger Current Registers pane now adds an **F0 Temp Registers**
  section whenever an active F0 uses T0-T7. It shows the owning cheat, hook,
  cave, T-bank address, live T0-T7/TFLAGS values, and allows paused editing
  of T0-T7; TFLAGS is shown read-only. With multiple active T-using F0 cheats, a selector is
  shown and the bank owning the current cave EIP is auto-selected.
- `T0` through `T7` are reserved names and cannot be used as F0 labels.

## v0.1.62 F0 static data + register preservation

- Added F0 `DD` static-data declarations. Multiple 32-bit values may be placed on
  one line, for example `dd 01D28710, 8B80C5FC, E3BDE8CB`.
- Labels attached to DD data resolve to the final guest address automatically, so
  normal source can use forms such as `mov edx, CarList`.
- DD declarations may be written after `DEADCODE`; the parser attaches them to
  that F0 block and the assembler physically places the data after the generated
  return JMP. Code + return JMP + DD data are one allocator-owned cave block.
- Added `PRESERVEALL`, selective `PRESERVE EAX, ECX, ...`, `RESTORE`, and
  selective `RESTORE EAX, ECX, ...` directives. Cheat-style `PRESERVE, EAX` /
  `RESTORE, EAX` spellings are accepted too. Supported saved state is
  EAX/EBX/ECX/EDX/ESI/EDI/EBP/EFLAGS; ESP and EIP are intentionally excluded.
- Adjacent `PRESERVE` directives accumulate into one frame. Each later preserve
  region opens another LIFO frame, and RESTORE when nothing is saved is a no-op.
- The top 64 KiB of the existing xemu-owned Type-F mapping is now permanently
  reserved for PRESERVE/T0-T7/TFLAGS private state: `680F0000-680FFFFF`. Normal F0/F1 code and DD
  allocations are limited to `68000000-680EFFFF`.
- Each F0 hook that uses preservation receives a private block with 16 nested
  48-byte frames. RESTORE clears restored slots; a fully restored frame is zeroed
  and reused. Disabling the cave zeroes/frees its private preservation block once
  the cave is safe to reclaim.
- Preservation values live in xemu-owned external memory, not Xbox machine RAM.
  PUSHFD/PUSHAD are used only as a short internal transfer mechanism while the
  live register values are copied to/from the private preservation frames.

## v0.1.61 expanded F0 x86 assembler

- Added 8-bit registers `AL/CL/DL/BL/AH/CH/DH/BH` and 16-bit registers
  `AX/CX/DX/BX/SP/BP/SI/DI` in addition to the existing 32-bit register set.
- `MOV`, `ADD/ADC/SUB/SBB/AND/OR/XOR/CMP`, `TEST`, `INC/DEC/NEG/NOT`, and
  shifts/rotates now support useful 8/16/32-bit register and memory forms.
- `MOVZX/MOVSX` now accept register sources as well as byte/word memory sources.
- Added `RET imm16`, indirect `CALL/JMP reg32` and `CALL/JMP dword ptr [memory]`,
  plus word/dword memory forms for `PUSH/POP`.
- Added common stack/flags/helper instructions: `PUSHAD/POPAD`, `PUSHFD/POPFD`,
  `LAHF/SAHF`, `CLC/STC/CMC`, `CLD/STD`, `CBW/CWDE`, `CWD/CDQ`, `LEAVE`,
  `INT3`, and `IRET`.
- Expanded multiply/divide support: one/two/three-operand `IMUL`, plus `MUL`,
  `DIV`, and `IDIV`.
- Added `XCHG`, `XADD`, `CMPXCHG`, `BSWAP`, `BSF/BSR`, `BT/BTS/BTR/BTC`,
  `SHLD/SHRD`, `SETcc`, and `CMOVcc`.
- Added `LOOP/LOOPE/LOOPNE/JECXZ` label branches and common x86 string
  instructions with `REP/REPE/REPNE` prefixes.
- FPU/MMX/SSE/SSE2 remain intentionally outside the built-in F0 assembler for
  now; F1 remains available for exact raw machine code.

---

# xemu Debug Tools Extensions

This directory contains the custom RAW Cheat Engine, Current Game manager,
Memory/Map tools, detached tool windows, x86 debugger UI, and the small C
bridge used to access QEMU/xemu CPU and memory facilities.

The goal of this layout is to keep custom tooling separate from upstream xemu
sources. The custom implementation lives here; upstream files should contain
only the minimum integration hooks needed to expose and render the tools.







## v0.1.60 compile fix

- Fixed the Windows-only backport build after the Type-F cave reclamation helper
  began taking `XemuCheatX86Registers` by reference.
- `XemuCheatX86Registers` already lives in `cheat-engine-memory.h`;
  `cheat-engine.hh` now forward-declares that existing type instead of creating
  a duplicate definition or incorrectly placing it in the F0 assembler header.
- No debugger, Type-F, allocator, or code-format behavior changed.

## v0.1.59 debugger navigation + live Current Registers

- x86 disassembly navigation now behaves like a small code browser: Right follows a selected
  JMP/Jcc/CALL/RET target, Shift+Right follows fall-through, Left goes Back, and Alt+Right goes Forward.
  Navigation never executes the guest and is separate from Step Into.
- Direct targets are followed from the disassembly itself. While paused, register-indirect targets,
  common `[base+index*scale+disp]` memory-indirect targets, and `RET` via `[ESP]` are resolved from the
  live Current Registers/RAM when possible; unresolved targets are reported rather than guessed.
- x86 right-click adds `Follow > Branch / Call Target / Fall Through / Return Target` and
  `Navigation > Back / Forward`. Double-click remains Execute-breakpoint creation.
- The disassembly branch gutter uses baby blue for backward/negative JMP/Jcc links and yellow for
  forward/positive JMP/Jcc links, including edge arrows when a direct target is outside the visible rows.
- Current Registers is now editable while the guest is paused/debug-stopped. Double-click a live
  value or use right-click `Edit Register Value`; writes go through QEMU's GDB register bridge and WHPX
  uploads the dirty CPU state before execution resumes.
- Current-register right-click also provides `Copy > Register / Value` and `View In > Memory / x86 Debugger`.
  `PC` is derived and remains read-only; edit `EIP` instead. Registers at Last Breakpoint stays read-only.

## v0.1.58 Type-F allocator + unified context menus

- Type-F external memory now uses a 16-byte-aligned first-fit free-list allocator.
  Each active cave keeps a fixed address for its lifetime; caves are never compacted
  or shifted when another cave grows.
- Actual Type-F install/update transactions pause a running guest until the new cave
  bytes, generated return JMP, and guest hook JMP are all written, preventing WHPX
  from observing partially-written executable code. An already-paused/debug-stopped
  guest remains stopped.
- External cave bytes are written directly to xemu's cheat-owned RAM backing after
  allocation establishes the guest mapping, then translation state is synchronized.
  This removes the redundant second mapping/write path that could fail on reactivation.
- Disabling F0/F1 pauses/synchronizes the guest, restores the original hook, and
  clears/frees the retired cave when EIP is outside that allocation and the
  live stack has no return address back into it. If execution/reference is still
  active, the block stays retired and the disabled-cheat Tick path
  retries later instead of erasing live instructions.
- Free neighboring blocks are coalesced. A larger edited cave reuses the old address
  only when the resulting free block is large enough; otherwise it receives another
  free address while every other active cave remains unchanged.
- Memory, Search, and x86 Debugger rows now share the same right-click layout:
  `Dump Ram > Current Page / DUMP PHYSICAL / DUMP MAPPED VIRTUAL RAM / DUMP PHYSICAL + DUMP MAPPED VIRTUAL RAM`,
  `Copy > Address / Value / x86 Instructions`, and `Break Point > Exe / Read / Write / Read/Write`.
- `View In` is context-specific: Memory -> x86 Debugger; Search -> x86 Debugger or
  Memory; x86 Debugger -> Memory.
- Double-clicking an x86 disassembly row toggles the Execute breakpoint at that
  row's guest virtual instruction address: add/re-enable when absent/disabled, remove when active.


## v1.69 conditional breakpoints + Current Registers tabs

- The Breakpoints table adds a clickable `Condition` column. `NO` means the
  breakpoint has no conditions; `YES` means one or more conditions are active.
  Both values open the same editor so conditions can always be added, edited,
  or removed.
- The condition editor accepts one register comparison per non-empty line.
  Multiple lines are ANDed. Supported operators are `==`, `!=`, `<`, `<=`,
  `>`, and `>=`; each operator has a plain-English hover tooltip. Values are
  hexadecimal with an optional `0x` prefix.
- Conditions are debugger-side filters. The backend breakpoint/watchpoint still
  triggers normally, xemu snapshots the architectural registers, and a false
  condition resumes automatically. Execute-breakpoint filtering reuses the
  existing TCG/KVM/WHPX step-over paths so a false condition cannot immediately
  retrigger at the same EIP.
- The existing Current Registers grid is preserved as the `General` tab inside
  the same pane. Additional tabs expose `x87 / FPU` (ST0-ST7 plus FCTRL/FSTAT/
  TOP/FOP), `MMX` (MM0-MM7), and `SSE` (XMM0-XMM7 plus MXCSR). The existing
  `Last BP` pane remains unchanged.

## v1.69 split full-RAM dump modes

- The Dump RAM tab now exposes three independent full-range actions:
  `DUMP PHYSICAL`, `DUMP MAPPED VIRTUAL RAM`, and
  `DUMP PHYSICAL + DUMP MAPPED VIRTUAL RAM`.
- `DUMP PHYSICAL` writes only the complete installed Xbox RAM `.bin`; it does not
  require building/scanning the virtual page map.
- `DUMP MAPPED VIRTUAL RAM` scans the full 32-bit guest virtual address space and
  writes only RAM-backed contiguous virtual regions plus `VIRTUAL-MAP.txt`; MMIO
  remains excluded and aliases remain represented.
- The combined action preserves the prior behavior and produces both sets from the
  same paused guest state. All three full-range actions leave the VM paused after
  completion for debugger inspection.
- The same three full-range actions are also available under the shared
  `Dump Ram` right-click submenu, below the existing `Current Page` action.

## x86 Debugger Inject menu

- Right-click a Virtual or Physical disassembly row and choose `Inject > NOP` to
  replace exactly that complete x86 instruction with `90` bytes. A running guest
  is paused for the patch and resumed only if it was running beforehand.
- `Inject > Change` opens a compact one-instruction editor showing the selected
  address, current bytes/instruction, an editable replacement assembly line, and
  the exact replacement-byte preview. Replacements must fit inside the original
  instruction span; shorter instructions are NOP-padded and longer ones are rejected
  with a prompt to use CodeCave. APPLY patches transactionally and RESTORE is guarded
  so it will not overwrite newer guest bytes that changed after the manual patch.
- `Inject > CodeCave` opens a compact Type-F0 builder using the selected virtual
  instruction as `$F0000000 AAAAAAAA`. The starter source automatically copies
  every complete instruction that the 5-byte hook JMP will overwrite, then adds
  `$DEADCODE`.
- The preview shows the current overwritten instruction span and the future JMP /
  return address before RUN. After RUN it shows the actual allocated cave address
  and hook bytes, and the main disassembly refreshes to the live JMP.
- RUN calls the same Type-F0 assembler, external allocator, hook sizing, original-byte
  save/rollback, T-register/PRESERVE handling, and retired-cave safety path as normal
  Cheat Engine F0 codes. RESTORE explicitly puts the saved original bytes back.
- NOP/Change/CodeCave injection is blocked when another active Type-F hook or cave owns the
  selected address, preventing the debugger from silently corrupting an active cheat.

## v0.1.42 true single-step fix

- Fixes **Step Into** under WHPX so it resumes through QEMU's step-aware VM
  startup path instead of ordinary `vm_start()`.
- WHPX now receives `step_pending=true` before execution resumes, ensuring the
  next x86 Trap Flag (`#DB`) is intercepted even when no execute breakpoint or
  data watchpoint is active.
- Prevents Step Into from running many instructions and appearing to behave
  like Step Over/Step Out.
- The fix remains entirely inside `ui/xui/debug-tools/`; no upstream WHPX/QEMU
  source modification is required.

## v0.1.41 live-register and breakpoint workflow

The debugger now keeps Current Registers live beside the frozen Registers at
Last Breakpoint snapshot. Execute and data breakpoints share one Address/Type/Len
control row and one combined list. Memory Viewer row addresses/byte values and
Search result addresses expose right-click breakpoint creation; Search also
provides View in Memory. Physical addresses are translated through the current
memory-map alias before debugger breakpoints are created.

## v0.1.40 actual watchpoint-access highlight

Read/Write watchpoint #DB exits occur after the memory instruction executes.
The debugger now resolves the instruction immediately ending at that stop EIP
and makes that actual memory-access instruction the persistent light-blue row.
The architectural Current EIP remains visible separately. Execute-breakpoint
highlighting is unchanged. If the previous instruction cannot be resolved
without guessing, the stop EIP is used as the fallback highlight.

## v0.1.39 full-page breakpoint disassembly

Breakpoint/watchpoint follow now uses a scrollable 4 KiB disassembly page by default.
The architectural debugger stop EIP is highlighted light blue in both Virtual and
Physical panes, and the initial scroll position leaves preceding instructions visible.
The previous Count-based forward view remains selectable from the debugger toolbar.
All of this implementation remains inside `ui/xui/debug-tools/`.

## v0.1.38 breakpoint backend layout

`backend/xemu-dbg.c` initializes QEMU's internal gdbstub state with the special
`none` transport before creating `BP_GDB` debugger objects. `backend/whpx-debug.c`
contains the WHPX guest-debug implementation and now mirrors data watchpoints
into x86 DR0-DR3 hardware debug registers.

WHPX supports four hardware data slots. Write and Read/Write normally consume
one slot per aligned 1/2/4-byte chunk. Read-only is implemented with a paired
Read/Write + Write slot so DR6 can distinguish reads from writes; this consumes
two slots per chunk. The upstream WHPX sources only contain the minimum hooks
needed to keep #DB interception active and forward a debug trap to this backend.

## Files

- `cheat-engine.cc/.hh` - CMP/RAW cheat parser, UI, and runtime engine.
- `cheat-engine-memory.c/.h` - C bridge to QEMU/xemu memory, mapping,
  disassembly and breakpoint/debug facilities.
- `current-game.cc/.hh` - current XBE/title/revision tracking.
- `detached-tools.cc/.hh` - independent SDL/OpenGL/ImGui tool windows.
- `memory-tools.cc/.hh` - linked Physical/Map/Virtual memory workspace,
  search/compare, RAM dump and x86 debugger UI.
- `backend/xemu-dbg.c/.h` - target-specific QEMU execute/data breakpoint bridge, adapted from Josh's debugger design; TCG uses QEMU watchpoints directly and WHPX delegates them to the hardware backend.
- `backend/whpx-debug.c/.h` - WHPX guest-debug registration plus DR0-DR3 hardware Read/Write watchpoint implementation.
- `meson.build` - all custom source registration, target-specific debugger backend registration, and Capstone dependency.

## Upstream integration points

The intended upstream-touch surface is small:

- `ui/xui/main.cc` - initialize, tick, draw and route detached tool windows.
- `ui/xui/menubar.cc` - Debug menu entries.
- `ui/xui/meson.build` - `subdir('debug-tools')`.
- `ui/xemu.c` - filters console-window expose/resize events so detached tool
  windows cannot trigger console redraw handling.
- `target/i386/whpx/whpx-accel-ops.c` - one debug-tools header include and one
  `xemu_debug_register_whpx_ops(ops)` registration call.
- `target/i386/whpx/whpx-all.c` - small hooks to keep debug-exception exits
  enabled for data watchpoints and forward #DB events to the debug-tools backend.

Other project/workflow changes used by this fork (for example the Windows-only
release build and Capstone provisioning) remain outside this directory because
they are build-system concerns rather than debugger implementation code.

### Type-F stable allocation/reactivation lifecycle

- Deactivation restores the original hook before a cave can be reclaimed.
- A stopped guest is checked before retired cave bytes are cleared; a cave caught at the current EIP is left intact and retried later.
- Freed 16-byte-aligned blocks return to a first-fit free list and adjacent holes are coalesced.
- Active caves never move. Growing one cave cannot shift or rewrite neighboring cave addresses/hooks.
- Editing/reloading a Type-F cheat can therefore safely allocate the same freed block when it fits or another free block when it does not.
- Mutually-exclusive Type-F cheats targeting the same hook can hand off the inactive hook state cleanly.
- Hook-install failures report the exact failed stage instead of only `Type-F Hook Installation Failed`.

## v0.1.63 Type-F PDE accessed-bit allocation fix

- Fixes repeated F0/F1 activation/allocation failures after the guest has executed the 0x68000000 Type-F mapping.
- The IA-32 CPU may set the page-directory Accessed bit; Type-F now validates the PDE page-table base and required mapping flags instead of requiring a byte-for-byte pristine PDE value.
- Allocation errors now report whether the guest mapping is unavailable/conflicting versus the 960 KiB code/DD arena actually being exhausted or fragmented.

## v0.1.64 debugger T-register view

The x86 Debugger shows a live `F0 Temp Registers` block under Current Registers
for active F0 cheats that use T0-T7. Values are persistent, can be edited while
paused, and can be viewed either as their value target or as their private
storage address. With multiple active T-using F0 cheats the pane has a selector;
when EIP is inside one of those caves that bank is selected automatically.

## v1.69 Current Game mounted-disc browser + full XBE SHA-256

- `Current Game` now separates the existing loaded-header hash from a new
  `XBE SHA-256`, which hashes the complete `default.xbe` file directly from the
  currently mounted `ide0-cd1` Xbox DVD backend. The browser never reopens a
  host ISO filename, so the displayed hash follows the media actually mounted
  in xemu.
- The window now has `Game Info` and `Disc Contents` tabs. `Disc Contents` is a
  read-only XDVDFS tree showing file/folder name, type, size, starting sector,
  and absolute disc-image offset. `Refresh Now` rescans the mounted medium and
  recalculates the full `default.xbe` SHA-256.
- The XDVDFS parser validates both volume-descriptor signatures, bounds-checks
  directory/file offsets, caps malformed nesting/table sizes/entry counts, and
  supports the common game-partition base offsets recognized by Xbox XISO
  tooling.
- The register-family tabs remain drawn only above `Current Registers`, but the
  selected General/x87-FPU/MMX/SSE family controls both Current Registers and
  Last BP. Last BP reserves matching tab-row height and snapshots the selected
  extended register state at accepted breakpoint/watchpoint hits.

## v1.71 mounted-disc build compatibility fix

- Moves QEMU `BlockBackend` headers and calls out of `current-game.cc` into the
  C-only `disc-block-io.c` bridge. This prevents QEMU C11 `_Generic` lock macros
  from being parsed by the C++ Current Game translation unit.
- The bridge passes an explicit `(BdrvRequestFlags)0` to `blk_pread()`, fixing
  the strict C++ `int` -> `BdrvRequestFlags` conversion failure while keeping
  mounted-disc reads read-only and behavior-identical.
- Current Game disc browsing and full `default.xbe` SHA-256 continue to read the
  currently mounted `ide0-cd1` backend; no host ISO/XISO path is reopened.


## v1.72 XBE labels

The x86 debugger can build a read-only label database from the complete
`default.xbe` on the currently mounted Xbox DVD. The XBE virtual address is
the stable master address; the Physical column is translated from the current
guest page tables at display/export time.

Label sources include the XBE entry point, section starts, Xbox kernel thunk
ordinals (resolved through the CC0 XboxDev/nxdk export list), useful ASCII
strings, MSVC RTTI strings, exact immediate xrefs from the title `.text`, and
clearly marked inferred function starts. Inferred names begin with `~` and are
heuristics, not original PDB symbols.

`Labels` toggles disassembly annotations. `LABELS` opens the searchable Current
Labels browser with Virtual/Physical jump actions. `DUMP LABELS` writes a text
index containing each stable Virtual address and its current Physical mapping
(or `UNMAPPED`).


## v1.73 Inject Change original-history + direct branch addresses

- `Inject > Change` remembers the first original instruction bytes and ASM for
  each changed virtual address for the current xemu session. Closing/reopening
  the editor no longer loses the original state.
- The editor shows both `Original instruction (remembered)` and the live
  `Current instruction`. An active tracked patch can be restored later with
  `REVERT TO ORIGINAL`; the restore is still guarded by an exact live-byte
  comparison so it cannot silently overwrite a newer/external patch.
- Reopening a changed instruction keeps the original instruction span even when
  the replacement decoded to a shorter instruction plus NOP padding.
- Direct hexadecimal control-flow targets are accepted by Change, for example
  `jmp 0008C5A2`, `jne 0x0008C5A2`, and `call 0008C5A2`. Nearby direct JMP/Jcc
  targets use their 2-byte short encoding when that is the only/smallest safe
  fit; farther targets use the normal rel32 encoding.
- The general Type-F0 assembler also accepts absolute hexadecimal JMP/Jcc/CALL
  targets while preserving existing label behavior and cave sizing.

## v1.74 portable `.xlabel` label packs

- Adds a root `Labels` workspace beside the xemu executable with four isolated
  subfolders created on demand:
  - `Labels/XDK/` for future local XDK reference/import files.
  - `Labels/PDB/` for future local PDB reference/import files.
  - `Labels/Packs/` for portable/distributable `.xlabel` files.
  - `Labels/Cache/` for future locally generated indexes/caches.
- Adds a portable `.xlabel` format whose header deliberately mirrors the Cheat
  file identity fields: `^1 = Hash`, `^2 = GameID`, and `^3 = NAME`. It also
  records the complete `default.xbe` SHA-256 when available and a format version.
- Label records persist XBE section + section-relative offset as their stable
  location. The absolute Virtual address is retained only as a revision-bound
  hint/fallback for labels that are outside normal XBE sections.
- Physical addresses are never stored in `.xlabel`. The debugger continues to
  translate Virtual -> Physical from the running Xbox page tables, so a launch
  whose Physical RAM backing shifts does not invalidate a label pack.
- Matching `.xlabel` files in `Labels/Packs/` are loaded automatically for the
  current GameID/Header hash. The full `default.xbe` hash is an additional
  revision guard when both the pack and mounted disc provide it.
- The Current Labels window adds `LABELS FOLDER`, `LOAD .xlabel`, `SAVE .xlabel`,
  and `RELOAD PACKS`. Saved packs can be copied to another XEMU - CHEATS install
  without distributing the original XDK/PDB input files.
- Labels now retain provenance (`XBE`, `XDK`, `PDB`, `MANUAL`) and confidence
  metadata. Manual/PDB/XDK names can take primary-display priority over an
  automatically inferred XBE name at the same Virtual address.
- This version establishes the portable label-pack/import foundation only. XDK
  and PDB source parsers are intentionally separate follow-up work; no Microsoft
  XDK/PDB code or binary data is included in the repository by this feature.

Generated `.xlabel` files use a simple text layout similar to the Cheat files:

```text
^1 = Hash: <loaded XBE header SHA-256>
^2 = GameID: EA009E
^3 = NAME: Example Game
^4 = XBEHASH: <complete default.xbe SHA-256>
^5 = FORMAT: 1

[LABELS]
; SECTION|OFFSET|VIRTUAL_HINT|TYPE|SOURCE|CONFIDENCE|LABEL
.text|00099230|001A9230|INFERRED|MANUAL|MANUAL|UnlockSystem_IsDLCUnlock
```

## v1.75 local XDK label importer

- Adds a local-only XDK importer on top of the v1.74 `.xlabel` foundation.
- The current XBE library-version table is parsed (for example `XAPILIB 5849`,
  `D3D8 5849`, `D3DX8 5849`, `XGRAPHC 5849`, `LIBCMT 5849`, and `DSOUND 5849`).
- `Labels/XDK/<build>/` is searched recursively, so a normal extracted XDK tree
  such as `Labels/XDK/5849/XDK/xbox/lib/` can be used without reorganizing its
  internal folders.
- `BUILD / REFRESH XDK INDEX` reads only the local matching `.lib` files and
  builds `Labels/Cache/XDK-<build>.xdkidx`.
- The cache contains symbol names, function sizes, relocation masks, and
  fingerprints only. It deliberately does **not** store the original XDK
  machine-code bytes, `.obj` members, `.lib` contents, PDB streams, or source.
- COFF relocations are normalized before fingerprinting so linker-fixed calls,
  addresses, and other relocation fields can still match the linked XBE.
- Only unambiguous fingerprints with a unique current-XBE match become labels;
  short functions below 16 bytes and unsupported relocation forms are skipped
  rather than being guessed.
- Exact matches are added as `FUNCTION` labels with `Source = XDK` and
  `Confidence = EXACT`, and continue to use XBE section-relative locations.
  Physical addresses are still resolved live from the running page tables.
- Current Game now shows XDK build/library/signature/exact-label/cache status.
  The Current Labels window adds the index build/refresh control and a Source
  filter (`XBE`, `XDK`, `PDB`, `Manual`).
- XDK-derived labels can be exported with the existing `.xlabel` saver. A user
  receiving that `.xlabel` does not need the original XDK or the local cache.

## v1.76 Microsoft linker MAP label importer

- Adds `LOAD .map` to the Current Labels window. The file picker starts in
  `Labels/PDB/`, so game `.map` and later `.pdb` inputs can be kept together.
- Microsoft linker MAP files are parsed locally; no MAP/PDB content is embedded
  in xemu or copied into the source tree.
- MAP `segment:offset` symbols are resolved against the current XBE section
  table, so imported labels remain XBE/Virtual-relative and Physical addresses
  continue to be resolved live from the running page tables.
- Function records become `FUNCTION` labels; non-function public/static records
  become `SYMBOL` labels. A lightweight MSVC-name cleanup makes common names
  such as `?Pause@cBackgroundLoader@@...` display as
  `cBackgroundLoader::Pause` without adding a Microsoft ABI library.
- MAP labels use `Source = MAP` and `Confidence = EXACT`. `MAP` is available in
  the Current Labels Source filter and survives portable `.xlabel` export.
- Safety is intentionally strict: the MAP linker timestamp must match the
  current XBE's original PE timestamp, and mapped segment spans must fit the XBE
  sections. A different build is parsed for diagnostics but **no addresses are
  applied**. This prevents a near-match MAP from silently naming the wrong code.
- Current Game and Current Labels show MAP timestamp, symbol, section, and exact
  label counts after an import attempt.
- The supplied Crusty Demons research set is a useful negative/lineage test:
  `Crusty.map` reports timestamp `4476D0D9`, while the supplied `Default.xbe`
  reports PE timestamp `44771B02`. Even if the timestamp is artificially made
  equal, the `.text` segment is larger than the current XBE `.text` section, so
  v1.76 correctly rejects it as a different build rather than importing stale
  addresses.


## v1.77 Microsoft PDB label importer

- Adds `LOAD .pdb` beside the MAP importer in Current Labels. The picker starts
  in `Labels/PDB/` so a game's XBE, MAP, and PDB research files can stay grouped.
- Reads Microsoft MSF 7.00 PDBs locally and imports CodeView `S_PUB32` public
  function/data symbols. The original PDB is never copied into xemu source, an
  `.xlabel`, or a distributable cache.
- Reads the current XBE's embedded `RSDS` record and displays its original PDB
  path, GUID, and Age in Current Game.
- Safety is intentionally strict: PDB symbols are applied only when the PDB GUID
  **and Age** exactly match the current XBE and the PDB's original section layout
  fits the XBE section table. A same-GUID/different-Age file is reported as the
  same symbol lineage but a different link and contributes zero labels.
- Public code/function symbols become `FUNCTION`; public data symbols become
  `SYMBOL`. Common MSVC decorated names receive the same lightweight cleanup used
  by the MAP importer. Labels use `Source = PDB`, `Confidence = EXACT`, and PDB
  has higher primary-display priority than MAP/XDK/XBE labels at the same address.
- PDB segment offsets are converted to XBE section-relative locations. Physical
  addresses remain runtime-only and continue to be translated through the current
  Xbox page tables.
- Imported PDB labels can be saved through `SAVE .xlabel`; recipients of the
  resulting label pack do not need the original PDB.
- The supplied Crusty Demons PDB is a useful safety case: its GUID matches the
  XBE (`BB75F5CA-2E73-4229-9F5E-904AC286D4C8`) but its PDB Age is 2 while the
  XBE requests Age 5, so v1.77 identifies the lineage and deliberately applies
  zero PDB addresses. Its older `.text` layout also exceeds the supplied XBE's
  `.text` span.

## v1.78 same-address Type-F handoff fix

- Fixes the normal checkbox workflow where one F0 is enabled, unchecked, and a
  different F0 using the **same hook address** is then checked.
- Restoring the first F0 still happens immediately. If its old executable cave
  is still running (or a `CALL` can still return into it), those old cave,
  PRESERVE, and T0-T7/TFLAGS allocations are detached into a private retirement
  queue instead of blocking the replacement hook.
- The second F0 receives a fresh cave and can install at the restored hook
  immediately; it never overwrites or reuses the first cave while that cave may
  still be referenced.
- Retired caves are checked every Cheat Engine tick and freed only after EIP and
  pending CALL-return checks prove that the old code is no longer live.
- Retired executable ranges remain protected by the existing Inject ownership
  guard until reclamation, so NOP/Change/CodeCave cannot edit code that may
  still be finishing execution.
- A title/XBE change clears both active and retired F0 state before resetting the
  external allocator, preserving the existing cross-title safety rule.
- The global Enabled/Disabled button is **not required** for this handoff; the
  intended workflow is simply `A checked -> A unchecked -> B checked`.


## v1.79 virtual memory-map optimization

- Memory Map and Mapped Virtual RAM dump now ask QEMU's x86 page-table walker
  for a point-in-time mapping snapshot instead of probing all 1,048,576 4 KiB
  virtual pages one by one.
- The snapshot is restricted to installed Xbox RAM, expanded into the existing
  Virtual/Physical page model, and preserves alias/linear-region behavior.
- The original exhaustive page probe remains as a compatibility fallback when
  an accelerator cannot provide the generic mapping snapshot.


## v1.80 disassembler bulk-read / Capstone optimization

- Sequential paired disassembly now caches 4 KiB guest pages instead of issuing
  up to fifteen guest-memory reads for every x86 instruction.
- Capstone uses one reusable instruction object with `cs_disasm_iter()` instead
  of allocating/freeing a decode result for every row.
- Full-page disassembly translates the page's Physical base once and derives row
  Physical addresses by offset.
- Instruction bytes, unmapped-page stopping behavior, and exact focus-address
  resynchronization remain unchanged.


## v1.81 Type-F precompile / retired-cave optimization

- F0 probe assembly, F1 raw parsing, and Type-F definition signatures are now
  prepared once when the cheat source is parsed instead of rebuilt every tick.
- F0 final assembly still happens once at the allocator-selected cave address,
  preserving label/DD/PRESERVE/T-register relocation behavior.
- Each installed cave caches its real CALL return sites once; retired-cave
  checks no longer redisassemble immutable code every cleanup pass.
- Retired caves still in use use a bounded tick backoff up to about one second,
  reducing repeated stack scans while preserving the v1.78 same-address handoff.


## v1.82 label generation/filter/batch-merge optimization

- Current Labels caches its filtered index and rebuilds it only when the label
  database generation, search text, Type filter, or Source filter changes.
- XBE/XDK/MAP/PDB/portable-pack labels are appended first and canonicalized
  once, instead of sorting the entire growing database after every source.
- Existing Merge() semantics remain available for standalone/manual imports.


## v1.83 XBE/PDB/MAP file-I/O optimization

- Mounted `default.xbe` is read once: the same buffer is SHA-256 hashed, parsed
  for labels/PDB identity, and then moved into Current Game storage.
- PDB files are read directly into their final byte vector instead of a GLib
  allocation followed by a second full-size vector copy.
- MAP files are read directly into their final `std::string` instead of a GLib
  buffer followed by another full text copy.
- Existing size limits and parser validation remain in place.

## v1.84 debugger/Cheat Engine streamlining cleanup

- The paired x86 disassembly panes now cache their complete per-row display
  strings between disassembly/label changes. Instruction-byte formatting and
  primary-label lookup happen once per refresh instead of once per visible row
  in both panes on every rendered frame.
- The exact-address/interior-opcode lookup used by refresh alignment and
  scroll-to-focus is shared and uses binary search over the already sorted
  disassembly rows instead of two separate linear scans.
- After a RUN_STATE_DEBUG stop has already been processed, the hit-state path
  no longer performs a second architectural-register backend read every frame;
  the existing 100 ms live-register refresh remains authoritative while paused.
- Cheat Engine Tick now identifies all disabled normal cheat-owned F hooks with
  one hook-table scan and retires them inside one guest pause window. The old
  block-by-block ordering is retained, while debugger-owned Inject > CodeCave
  hooks remain independent of the Cheat Engine global/live enable checkboxes.
- No code type, F0/F1 allocation, breakpoint/watchpoint, UI layout, label text,
  navigation, or memory read/write semantics are intentionally changed.

## v1.85 OPT Pass 1

- Memory Viewer Physical/Virtual panes now snapshot each visible 4 KiB guest
  page at most once per pane/frame instead of reading every visible 16-byte row
  separately. The cache is frame-local, so live-memory refresh behavior is
  unchanged, and the previous short-span read remains as a fallback for unusual
  partial mappings/MMIO where a full-page debug read fails.
- The Memory Viewer page cache uses a fixed four-entry ring, avoiding a new heap
  allocation in the render hot path.
- Active F0 T0-T7/TFLAGS display metadata is cached by the Cheat Engine and
  borrowed directly by the debugger. The sorted cache is rebuilt only when
  source/game/hook lifecycle state changes instead of every rendered frame.
- RAM virtual-mapping snapshots allocate their result array once from QEMU's
  known mapping-count upper bound instead of reallocating for every accepted
  mapping.
- Shared source-test helpers remove duplicated brace-balanced function extraction
  code, and a dedicated Pass-1 regression guard freezes these optimizations.

## v1.86 OPT Pass 2

- Capstone x86 decoder state is now retained in a per-thread disassembly
  context. Each thread opens/configures its Capstone handle and allocates the
  `cs_disasm_iter()` instruction cache once, then reuses them for later page,
  paired, Inject, Type-F, and watchpoint helper decodes.
- The same context retains the two paired-disassembly 4 KiB page buffers and
  the full-page scratch buffer. They are resized only if QEMU's runtime target
  page size changes instead of allocated/freed for every disassembly request.
- Debugger Count mode reuses the already decoded full-page rows used to align
  an interior address whenever the requested instruction slice is safely
  inside that page. The final 14 bytes deliberately keep the paired decoder so
  a valid x86 instruction crossing the 4 KiB boundary preserves the previous
  behavior.
- RefreshDisassembly reuses the selected decoded row's already resolved
  Physical address instead of preparing/translating the same Virtual address a
  second time after decoding.
- Virtual and Physical debugger panes continue to consume the same paired row
  set; no separate decode/cache state was introduced for either pane.
- Existing Capstone failure results, exact focus-address resynchronization,
  page-boundary behavior, instruction bytes/text, Inject > NOP/Change/CodeCave,
  Type-F hook analysis, and breakpoint/watchpoint helper paths are otherwise
  unchanged.
- The disassembler golden guard now protects the reusable-context lifecycle,
  and `v287-disassembly-page-cache-golden.py` freezes the Count-mode page-slice reuse and
  conservative cross-page fallback.

## v1.87 debugger navigation focus fix

- Browser-style x86 navigation now hands ImGui keyboard focus to the actual
  destination instruction after Right/Shift+Right Follow, Left Back, and
  Alt+Right Forward. Up/Down therefore continues from the newly selected
  destination instead of the pre-navigation source row.
- The active Virtual/Physical pane is preserved across that focus handoff. A
  Follow initiated from Physical stays keyboard-focused in Physical, and the
  same applies to Virtual.
- Generic debugger jumps such as Go to EIP, breakpoint Go, and label navigation
  retain their existing focus behavior; only browser-style navigation performs
  this keyboard-focus transfer.
- Navigation history, exact-source Back behavior, opcode-start alignment, branch
  target resolution, scrolling, and guest execution state are unchanged.

## v1.88 OPT Pass 3 structural / technical-debt cleanup

- `memory-tools.cc` is split by ownership into small core, Memory Viewer/map,
  Search, Debugger, Inject, Labels, and Dump translation units. The public
  `MemoryToolsWindow` state layout and method declarations remain unchanged.
- Shared implementation-only constants/formatting/address helpers moved to
  `memory-tools-internal.hh`; this header is private to the split translation
  units and is not exposed through `memory-tools.hh`.
- The split is deliberately move-only for behavior-bearing code. All 99
  `MemoryToolsWindow` member bodies and the shared helper bodies are byte-for-
  byte identical to the v1.87 baseline.
- Source-level regression tests now read the complete split MemoryTools
  implementation rather than assuming every method lives in `memory-tools.cc`.
  This removes a source-location dependency that previously made harmless file
  ownership refactors look like behavioral regressions.
- `v287-memory-tools-structural-refactor-golden.py` fingerprints every v1.87 MemoryTools
  member/helper body plus the protected Cheat Engine, F0/F1, Capstone bridge,
  external CodeCave-memory, and public MemoryTools header files. It also guards
  Meson ownership of every split unit and prevents the core file from growing
  back into the previous monolith.
- No Cheat Engine, F0/F1 lifecycle, Inject > NOP/Change/CodeCave, breakpoint/
  watchpoint, disassembler navigation, Memory Viewer behavior, UI layout, v1.87
  navigation-focus fix, or v1.86 Capstone optimization behavior is changed by
  this pass.


## v1.89 OPT Pass 4 debugger QoL / persistent preferences

- Debugger-owned display preferences now persist through xemu's existing
  `xemu.toml` settings tree without enabling ImGui's global `.ini` storage.
- Persisted values are the horizontal disassembly pane height, Full Page/Count
  mode, Count instruction total, Follow EIP, Labels visibility, and the selected
  Current Registers tab (General/x87-FPU/MMX/SSE).
- Preference loading is lazy on the first Memory Tools draw, after xemu's
  normal settings load has completed. Invalid/manual values are clamped to the
  existing UI ranges (160-1200 px, 1-128 instructions, register tab 0-3).
- Register-tab restoration notices a change of ImGui context, so the selected
  tab also follows the Memory Tools window into a detached debugger context.
- `RESET UI` restores only those six debugger display preferences to their
  established defaults. It does not touch breakpoints/watchpoints, navigation
  history, Inject state, addresses, register snapshots, guest execution, Cheat
  Engine state, or F0/F1 lifecycle.
- Preference changes update only the in-memory xemu configuration during draw;
  the existing exit-time settings save remains responsible for disk I/O.
- The v1.88 split, v1.87 navigation-focus behavior, and v1.86 Capstone path are
  otherwise unchanged.

## v1.90 OPT Pass 5 Current Game / recurring-runtime polling cleanup

- Current Game keeps an exact copy of the most recently derived in-memory XBE
  header block. The existing 500 ms poll still rereads the complete loaded
  header before making any cache decision; title/version/base fields alone are
  deliberately not treated as proof because the full loaded-header SHA-256 is
  part of game/revision identity.
- When the complete header length and every byte are identical, the poll skips
  repeated SHA-256 calculation, UTF-16 title conversion, revision-key
  construction, and equivalent `GameInfo` string assignment. Mounted-disc
  backend identity/length polling still runs, so disc changes remain visible
  even while the running XBE is unchanged.
- `Refresh Now` bypasses the derived-state cache and retains the previous full
  refresh behavior. A failed title conversion or SHA calculation also leaves
  the cache non-authoritative so the next poll retries instead of freezing a
  transient failure.
- The stable no-XBE/frontend state no longer rebuilds and assigns an equivalent
  empty `GameInfo` every 500 ms; disc polling and the valid-XBE -> no-XBE
  generation transition remain unchanged.
- `xemu_get_xbe_info()` now retains its small header scratch allocation while
  capacity is sufficient instead of free+malloc on each successful poll. The
  complete guest XBE header is still reread and reparsed every call, preserving
  exact revision/change detection and all existing XBE consumers.
- No Cheat Engine/F0/F1, Inject, breakpoint/watchpoint, disassembler navigation,
  Memory Viewer, debugger layout/preference, label/disc parsing, or UI behavior
  is intentionally changed by this pass.

## v1.91 OPT Pass 6 debugger steady-state / render-path cleanup

- The Current Registers pane keeps the established 100 ms architectural
  register refresh used by EIP highlighting/navigation, but no longer asks the
  debugger backend for x87/MMX/SSE state while the General tab is selected.
  Entering x87/FPU, MMX, or SSE from General performs an immediate same-frame
  extra-register refresh before drawing the selected table, so visible register
  behavior and breakpoint snapshots remain unchanged.
- Breakpoint/watchpoint Physical columns now use a fixed 16-entry, frame-local
  4 KiB page-translation cache while the guest is stopped and the current
  virtual map has been synchronized. Breakpoints sharing a page therefore avoid
  repeated page walks during the same table draw. The cache is never retained
  across frames, never used while the guest is running, and falls back to the
  original direct translator when preparation is unavailable or the fixed cache
  is full.
- `DrawDebugger()` and `DrawBreakpoints()` each take one frame-local debugger
  backend snapshot instead of repeatedly querying the unchanged accelerator
  backend while rendering the same frame.
- The frame-end debugger preference mirror still owns the same six v1.89
  values, but now writes an in-memory config field only when its value actually
  changed. No settings-file I/O was added.
- No new `MemoryToolsWindow` state was introduced. The v1.88 split, v1.89
  preference ownership, v1.90 Current Game polling, v1.87 navigation-focus fix,
  v1.86 Capstone path, Memory Viewer behavior, Inject behavior, and Cheat/F0/F1
  lifecycle are unchanged.

## v1.92 OPT Pass 7 disassembly / label display-cache cleanup

- The paired Virtual/Physical disassembly panes now validate their shared
  immutable display-text/label cache once per debugger frame before either pane
  is drawn, instead of repeating the same row-count, label-generation, and
  Labels-toggle checks independently in both panes.
- Rebuilding cached disassembly text keeps the same exact primary-label result
  but replaces one binary `PrimaryLabelAt()` lookup per decoded row with one
  initial `lower_bound` followed by a forward walk through the already-sorted
  label database. Full-page refreshes therefore avoid repeated logarithmic
  label searches while label names and formatting remain unchanged.
- The Label Browser keeps Physical addresses live. While the guest is stopped
  and the virtual map has been prepared, a fixed 32-entry frame-local 4 KiB
  page cache lets visible/selected labels on the same page share a translation.
  While the guest is running, every label continues through the historical
  direct-per-address translator so mapping changes cannot be hidden.
- The label translation cache is stack/frame-local, has no persistent
  `MemoryToolsWindow` state, and falls back to direct page translation after the
  fixed cache fills. The selected-label controls reuse the same frame-local
  translator as the table instead of repeating an already-resolved page walk.
- No Cheat Engine/F0/F1, Inject, breakpoint/watchpoint semantics, disassembler
  navigation, Memory Viewer behavior, UI layout, v1.86 Capstone path, v1.87
  navigation-focus fix, v1.88 source split, v1.89 preferences, v1.90 Current
  Game polling, or v1.91 steady-state behavior is intentionally changed.


## v1.93 OPT Pass 8 table/list presentation + allocation cleanup

- Memory Search keeps a small fixed 256-entry direct-mapped presentation cache
  for clipped result rows. Address, Previous, and Current display strings are
  formatted once and reused while the exact result index, address, previous raw
  value, current raw value, and value kind remain identical. Any scan mutation
  or type change therefore self-invalidates without a separate generation or
  persistent search-semantic state.
- The Search display cache is bounded (~tens of KiB), does not scale with the
  potentially multi-million-result scan vector, and does not alter scan/filter
  algorithms or result ordering. Context-menu value text uses the same exact
  cached string that is rendered in the Current column.
- Active F0 bank metadata now prebuilds the existing `name  [hook XXXXXXXX]`
  combo label when the already-established F0 metadata cache rebuilds. The
  debugger consumes that cached label directly instead of formatting preview
  and item strings every frame/open-combo pass. Hook ordering, selection,
  T0-T7/TFLAGS reads, editing, install/deactivate behavior, and cache invalidation
  points are unchanged.
- The breakpoint/watchpoint table and Label Browser were audited but deliberately
  left without new persistent caches: their expensive Physical translation work
  is already frame-local cached by Passes 6-7, and xemu still has no authoritative
  guest page-table generation suitable for persistent translation caching.
- No Cheat Engine/F0/F1 lifecycle, Inject, breakpoint/watchpoint semantics,
  disassembler navigation, Memory Viewer behavior, UI layout, v1.86 Capstone
  path, v1.87 navigation-focus fix, v1.88 source split, v1.89 preferences,
  v1.90 Current Game polling, v1.91 steady-state behavior, or v1.92
  disassembly/label caching is intentionally changed.

## v1.94 OPT Pass 9 audit / pruning / technical-debt cleanup

- Performed a whole `ui/xui/debug-tools/` audit before taking new changes. No
  additional persistent breakpoint/label translation cache was added: the
  existing frame-local stopped-state caches already cover the expensive page
  walks, and xemu still has no authoritative guest page-table generation that
  would make a persistent mapping cache provably safe.
- The Microsoft MAP and PDB label importers now share one private
  `label-symbol-utils.hh` implementation for their previously duplicated C/
  stdcall/import-name cleanup and simple MSVC C++ name formatting. MAP and PDB
  keep their intentionally different compiler-internal filtering rules, so
  accepted/rejected symbol behavior is unchanged.
- Removed the unused exported
  `xemu_breakpoint_condition_register_name()` declaration/definition after a
  tracked-source audit confirmed it had no caller. The condition parser,
  evaluator, register set, operators, editor, and breakpoint semantics are
  unchanged.
- Source-level regression tests now share one generic brace-balanced class
  member extractor in `tests/v287_source_test_utils.py`. Passes 3, 5, 6, 7, and 8 no
  longer carry local copies of the same parser, reducing implementation-specific
  test maintenance without changing their protected digests/invariants.
- Added direct shared-symbol-helper golden cases plus
  `v287-audit-pruning-cleanup-golden.py`. The Pass-9 guard fingerprints the behavior-
  sensitive v1.93 Cheat Engine, F0/F1, Memory Tools, Capstone bridge, debugger
  backend, Current Game, Inject/external-code, label-pack/XDK/XBE, and disc
  paths so this cleanup cannot silently expand into a runtime feature change.
- Matching helper names such as generic endian/range utilities in the XBE,
  XDVDFS, XDK, MAP, and PDB parsers were deliberately left local where their
  data types/ownership differ; centralizing them would add cross-parser coupling
  without a meaningful runtime or maintenance win.
- No Cheat Engine/F0/F1 lifecycle, Inject > NOP/Change/CodeCave, breakpoint/
  watchpoint semantics, disassembler navigation, Memory Viewer behavior, UI
  layout, v1.86 Capstone path, v1.87 navigation-focus fix, v1.88 source split,
  v1.89 preferences, v1.90 Current Game polling, v1.91 steady-state behavior,
  v1.92 display/label caching, or v1.93 presentation caching is intentionally
  changed.

## v1.95 OPT Pass 10 lifecycle / resource / failure-path hardening

- Consolidated the C++ Debug Tools transactional guest pause/resume pattern into
  one private `guest-pause-guard.hh`. Type-F install/deactivate/reclamation,
  current-page RAM dumping, and label dumping now get automatic resume on every
  early return while preserving an already-paused/debugger-stopped guest.
- The label dump explicitly releases the scoped pause at the historical point
  immediately after `fclose()`, before status/UI bookkeeping. Full-range RAM
  dumping is intentionally **not** converted: it continues to leave the VM
  paused after a dump exactly as before.
- Type-F `InstallFHook()` now has one local failed-install cleanup contract for
  the five post-allocation failure branches (preserve allocation, T-register
  allocation, final F0 assembly, layout verification, and external payload
  write). The helper performs the same pre-hook/unreachable resource release as
  the former duplicated branches; hook installation/rollback and retired-cave
  semantics are unchanged.
- `ReleaseFHookCaveIfSafe()` and `DeactivateFHook()` use the same scoped pause
  owner instead of manual `vm_start()` calls on individual success/error exits.
  Nested callers remain safe because a guard only resumes when it observed and
  paused a running VM itself.
- No persistent state, allocator policy, retired-cave timing, mapping cache,
  Inject behavior, breakpoint/watchpoint semantics, navigation, Memory Viewer
  behavior, UI layout, or Current Game detection behavior is changed.
- `v287-lifecycle-resource-hardening-golden.py` fingerprints all untouched v1.94
  CheatEngine/MemoryTools methods plus the Inject, debugger, Capstone, external
  cave, Current Game/XBE, and debugger-backend files. It separately freezes the
  five-path F0 install cleanup contract and the intentionally leave-paused full
  RAM dump behavior.


## v2.43 Disc Contents export / HDD QoL

- Disc Contents now exposes a right-click Export menu for XDVDFS files and folders plus Copy Disc Path. Export is read-only, reparses the mounted disc before transfer, checks backend identity on every data read, and uses collision-safe host create-new semantics.
- The HDD browser gains a case-insensitive name filter that keeps matching ancestor branches visible. No HDD mutation or kernel write transaction semantics are changed.
- `v243-disc-export-hdd-qol-golden.py` locks the read-only export ownership and filter UI contract; `xdvdfs-golden.cpp` covers nested path re-resolution.


## v2.44 cumulative HDD/KRPC audit + measurement pass

- Adds explicit timing/count instrumentation for partition-set preflight snapshots so the next optimization decision can be based on measured cost rather than removing safety work.
- Adds `v244-hdd-krpc-final-audit-golden.py`, a cumulative final-state contract covering the permanent Create/Open -> Write -> Flush -> Close -> fresh FATX Verify invariant, coherent pause safety, whole-tree/byte Copy verification, capacity preflight, partition-scoped ownership, read-only production FATX policy, atomic HDD/disc export, Disc Contents right-click Export, and HDD name filtering.
- This is intentionally a measurement/test consolidation pass; it does not reduce write verification or change the Xbox-kernel mutation sequence.

## v2.45 PREENTRY Patch tab lifecycle

> Historical implementation note: v2.45's transient no-XBE startup detection
> and file+name selection identity were superseded by v2.46-v2.52. They are
> retained below as release history; the later sections describe current behavior.

- Adds block-scoped `:PREENTRY:` parsing. Place `:PREENTRY:` immediately before
  a `+Cheat` block, or directly after the `+Cheat` line before its RAW lines.
  The marker applies to that one block only; following ordinary cheats remain
  live cheats without requiring a closing directive.
- Adds separate **Cheats** and **Patch** tabs to the RAW Cheat Engine. PREENTRY
  blocks appear only on **Patch** and ordinary blocks remain on **Cheats**.
- The Patch tab warns that selected patches require a game reset to fully
  activate. Patch rows expose four lifecycle states: `ACTIVATED - RESET
  REQUIRED`, `ACTIVATED`, `DEACTIVATED - RESET REQUIRED`, and `INACTIVE`.
- Checking or unchecking a PREENTRY patch only stages the requested state; it
  never executes the block immediately. Selected PREENTRY blocks execute once
  when the next no-XBE -> XBE game-start boundary is observed.
- Patch selections are kept for the current xemu session across the temporary
  no-XBE interval of a guest reset and are keyed by matched cheat-file path plus
  block name, preventing same-named patches from unrelated game files from
  sharing state.
- The normal live-cheat Enabled/Disabled control, group selection, active count,
  periodic execution, and Type-F deactivation path explicitly exclude PREENTRY
  blocks. An applied PREENTRY Type-F hook therefore remains installed until the
  game resets instead of being restored when live cheats are disabled.
- UI Reset arms a short-lived frame-cadence XBE lifecycle probe so a fast
  same-title reset cannot be hidden by the Current Game manager's normal 500 ms
  steady-state polling interval. Normal steady-state polling remains unchanged.
- Adds `v245-preentry-patch-tab-golden.py` and scopes the historical Pass 3/7/8/
  9/10/11 freeze guards around the exact v2.45 additions. All untouched legacy
  methods/header content remain protected by their original fingerprints.

## v2.46 PREENTRY reset retry + inline syntax correction

- Adds the preferred inline CMP form `+:PREENTRY:Cheat Name{Description}`. The
  `:PREENTRY:` marker is stripped from the displayed Patch name. The v2.45
  standalone forms remain supported for backward compatibility.
- Fixes same-title Reset handling: PREENTRY no longer depends on observing a
  transient no-XBE interval, which can be missed when copied XBE metadata stays
  valid throughout a fast reset. The existing two-line `actions.cc` Reset bridge
  remains unchanged from v2.45; the corrected state machine stays in Debug Tools.
- UI Reset now keeps PREENTRY armed until QEMU has consumed the reset request,
  then forces the existing current-game reload path once so stale Type-F/cave
  ownership is discarded even when the Title ID/XBE hash did not change.
- Startup/Reset application remains pending while the matching code file or guest
  CPU is not yet ready. Once both are ready, selected PREENTRY blocks execute once
  before normal live-cheat blocks. A transient early CPU-unavailable frame can no
  longer silently lose the patch.
- Patch activation remains independent of the Cheats-tab `Enabled/Disabled`
  button. The global `Engine Enabled` option remains the master safety gate.
- A Patch row changes to `ACTIVATED` only when its PREENTRY block completed
  without a runtime error. Failed blocks remain reset-required rather than being
  reported as applied.
- Adds `v246-preentry-reset-retry-inline-golden.py` and keeps the v2.45 ownership,
  Patch UI, live-cheat isolation, and historical freeze guards intact.

## v2.47 PREENTRY lifecycle hardening

- Replaces the overlapping reset/apply booleans with one `PreEntryLifecycle`
  state (`Idle`, `ResetRequested`, `ApplyPending`), preventing contradictory
  combinations and keeping explicit Reset ownership separate from normal startup.
- Current Game identity is observed even when Auto-load is disabled. Turning
  Auto-load back on mid-game may load the code file, but cannot synthesize a
  startup edge or execute PREENTRY late.
- `Engine Enabled` remains the startup safety gate, but disabling it no longer
  tears down already-applied Patch Type-F hooks. The Cheats-tab Enabled/Disabled
  button remains completely independent of Patch activation.

## v2.48 PREENTRY parser / identity cleanup

- Centralizes `+:PREENTRY:Name` marker consumption in one parser helper while
  retaining the standalone `:PREENTRY:` forms for compatibility.
- A standalone `:PREENTRY:` found after RAW lines is now diagnosed and ignored
  instead of silently reclassifying the following block.
- Patch selection identity is now `code-file path + group path + block name`, so
  duplicate Patch names in different groups do not share state.

## v2.49 Patch UI / tab-state QoL

- Centralizes the four Patch lifecycle status labels and binds description/credit
  tooltips to the Patch name rather than the status label.
- Gives PREENTRY failures a dedicated Patch runtime-message channel so normal
  live-cheat runtime status is not overwritten.
- Debug Tools tabs now make the active location obvious: the selected tab keeps
  xemu's current green, while unselected Debug Tools tabs use light grey. The
  override is local to Debug Tools and does not change xemu's global theme.

## v2.50 PREENTRY Type-F ownership / resource cleanup

- Extracts one live-only F-hook teardown path. Disabling live Cheats cannot
  restore Patch-owned Type-F hooks, while ordinary files retain the historical
  all-live fast path.
- Extracts stale-guest hook/cave ownership cleanup for title/reset transitions.
  It forgets previous guest ownership without writing old hook bytes into the
  new guest address space and resets only Debug Tools' private cave arena.

## v2.51 PREENTRY reset / refresh optimization

- Adds `CurrentGameManager::RefreshRunningXbe()` and shares the existing loaded-XBE
  logic through `RefreshInternal()`. Normal `Refresh()` still performs the same
  mounted-disc/XDVDFS/label refresh behavior.
- Explicit PREENTRY Reset recovery now force-refreshes only running-XBE identity,
  avoiding unnecessary default.xbe hashing, label-pack/XDK/MAP/PDB work, and
  mounted-disc rescans on every reset.

## v2.52 PREENTRY technical-debt / pruning pass

- Removes the unused Cheat Engine `xemu-xbe.h` include left by the earlier reset
  experiments and updates PREENTRY selection comments to the current group-aware key.
- Renames transitional `V245_*`/`strip_v245_*` regression helpers to durable
  PREENTRY subsystem names while preserving the historical freeze comparisons.
- Adds a cumulative v2.52 cleanup guard to reject decommissioned reset booleans,
  stale helper names, expansion of the non-Debug-Tools Reset bridge, or loss of
  the selected-green/unselected-grey tab contract.


## v2.52 PREENTRY prefix compile hotfix

- Makes `ConsumePreEntryPrefix()` a private static `CheatEngineWindow` member so
  it can legally use the class-owned `Upper()` and `Trim()` helpers. This fixes
  the native `Upper/Trim was not declared in this scope` compile regression
  without changing parser behavior or the preferred `+:PREENTRY:Name` syntax.
- Adds a focused compile-scope regression guard while leaving `actions.cc` and
  PREENTRY runtime ordering unchanged.

## v2.53 Debug Tools additions audit / tab-state QoL

- Re-audits only the Debug Tools additions on top of the corrected v2.52 source;
  PREENTRY timing, reset ordering, Type-F execution, Current Game refresh, HDD/KRPC,
  and the two-line `actions.cc` reset bridge are intentionally unchanged.
- Debug Tools tabs now have exactly three visual states: inactive light grey,
  hovered steel blue, and selected xemu green. Focus loss reuses the same
  inactive/selected colors instead of introducing extra grey/green shades.
- PREENTRY selection persistence now stores selected identities in an
  `unordered_set`; unchecking erases the key instead of accumulating false map
  entries. PREENTRY is also explicitly excluded from the legacy name-only live
  cheat state-preservation map, keeping Patch identity authoritative.
- Centralizes selected-Patch lookup through `IsPreEntrySelected()` and adds a
  cumulative v2.53 audit guard for the tab palette, selection storage, compile
  hotfix ownership, Patch/live isolation, and unchanged non-Debug-Tools bridge.
