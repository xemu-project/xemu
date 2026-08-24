# xemu Debug Tools Extensions

This directory contains the custom RAW Cheat Engine, Current Game manager,
Memory/Map tools, detached tool windows, x86 debugger UI, HDD/FATX tooling, and
the small C bridges used to access QEMU/xemu CPU and memory facilities.

The project rule is to keep custom implementation inside `ui/xui/debug-tools/`
whenever possible. Upstream xemu files should contain only the smallest required
integration hooks; unrelated upstream code is not a cleanup target.

## RAW Cheat Engine and Patch lifecycle

- Ordinary `+Cheat` blocks live on **Cheats** and execute only when the live
  Cheats control is enabled. Unchecking an ordinary Cheat is an unconditional
  OFF boundary: the block is disabled immediately and any active F0/F1 hook
  owned by it restores the exact original hook bytes captured at installation.
  Group deselection follows the same rule.
- Startup patches use the preferred `+:PREENTRY:Name{Description}` syntax. The
  standalone `:PREENTRY:` compatibility form remains supported.
- **Patch** is independent of the Cheats-tab Enabled/Disabled button. The global
  **Engine Enabled** option remains the master startup safety gate.
- Checking/unchecking a Patch stages the next startup/reset. A Patch is reported
  as applied only after its block executes successfully. Failed startup blocks
  keep their own error and remain reset-required.
- Direct Game A -> Game B identity changes are treated as real startup boundaries.
  Explicit UI Reset retains its separate `ResetRequested -> ApplyPending`
  lifecycle and does not depend on observing a transient no-XBE frame.
- Patch selection identity is code-file path + group path + block name +
  occurrence ordinal, so duplicate names remain independent. Stale identities
  are pruned only for the file currently being parsed.
- Live Type-F ownership and PREENTRY Type-F ownership remain separate. Disabling
  live Cheats never tears down an already-applied Patch hook.

## Debug Tools tab states

Debug Tools use exactly three local tab states without altering xemu's global
theme:

- **Inactive:** light grey (`#949494`)
- **Hovered:** steel blue (`#5B8FBA`)
- **Selected:** xemu's existing active-tab green

The style is owned by a scoped RAII helper so every pushed tab color is restored
automatically. Partially selected Cheat/Patch groups display an indeterminate
mark inside the checkbox.

## Current Game / debugger / memory tools

Current Game tracks the running XBE identity and mounted-disc metadata. Debugger
and Memory Tools provide disassembly/navigation, conditional breakpoints, live
register views, labels/importers, memory/search/map views, RAM dumps, and F0
CodeCave integration. The existing historical debugger behavior is protected by
the packaged golden tests.

## HDD / FATX safety

Filesystem mutation retains the permanent transaction invariant:

`Create/Open -> Write -> Flush -> Close -> fresh FATX Verify`

Cleanup and optimization must not weaken that sequence. Copy/Move verification
continues to use fresh coherent FATX snapshots and byte-for-byte content checks.
Read-side optimizations may reuse host buffers or measurement helpers only when
those verification semantics remain unchanged.

## x86 Debugger Inject restore behavior

`Inject > NOP` and `Inject > Change` share one remembered-original instruction
record for the session. Change exposes only `APPLY` and `RESTORE`; Restore fills
the Replacement field with the remembered original instruction and writes the
exact captured original bytes. A conditional `Inject > Restore` menu item is
shown on rows owned by an active NOP/Change patch and restores the complete
tracked instruction span after verifying that the live bytes still match the
patch xemu wrote. CodeCave continues to use its separate Type-F0 restore path.

The debugger's right-side section is tabbed as **Breakpoints | Changes**. Changes
lists active restorable debugger patches as `Address | Original | Changed | HEX`.
HEX is per-row and defaults off: unchecked rows show decoded instruction text;
checked rows show the captured/written bytes. Address right-click reuses the
normal debugger/disassembler context menu and double-click follows the address.
For the temporary debugger CodeCave/F0 hook, Changes records only the hook /
jump-from address and never enumerates the generated F0 cave body. A real user
Reset discards these debugger-only Changes records without writing captured
pre-reset bytes into the new guest. The disassembler refresh is deferred until QEMU
has consumed the pending reset, then the existing refresh path rereads post-reset RAM
exactly once; PREENTRY Patch activation remains separate.
CodeCave RUN is refused when the hook overwrite span overlaps the live XBE header
(`m_base` through `m_sizeof_headers - 1`) so a debugger JMP cannot modify the
header bytes used for running-XBE revision detection.

After NOP, Change, Restore, or CodeCave RUN/RESTORE, the disassembly refresh is
anchored to the instruction that was actually modified; an older Follow/Go-To
target cannot pull the view away, and the refresh does not add a Back/Forward
history entry.

## Files

- `cheat-engine.cc` - small per-frame Cheat Engine coordinator (`Tick`).
- `cheat-engine-source.cc` - CMP/RAW source parsing, file discovery/loading, game identity, and PREENTRY/Patch lifecycle.
- `cheat-engine-fhooks.cc` - debugger/Live Cheat Type-F0/F1 compile, install, restore, retirement, and ownership lifecycle.
- `cheat-engine-execute.cc` - guest read/write helpers and RAW code execution (0/1/2/3/4/5/6/7/9/A/D/E/F).
- `cheat-engine-ui.cc` - Cheat/Patch selection controls, menu/help, and Cheat Engine rendering frontend.
- `cheat-engine.hh` - shared Cheat Engine state/API contract; unchanged by the Phase-9 split.
- `cheat-engine-memory.c/.h` - C bridge to QEMU/xemu memory and debugger access.
- `current-game.cc/.hh` - current XBE/title/revision, mounted-disc tracking, label/XDK/MAP/PDB state, and disc export core.
- `current-game-ui.cc` - Current Game rendering frontend: summary, Game Info, Disc Contents, HDD/diagnostics tabs, and disc-entry context UI.
- `detached-tools.cc/.hh` - independent SDL/OpenGL/ImGui tool windows.
- `memory-tools-debugger.cc` - x86 debugger state/execution, navigation, breakpoint/watchpoint ownership, register writes, and disassembly refresh.
- `memory-tools-debugger-ui.cc` - debugger rendering frontend: condition editor, register/F0 views, breakpoint list, disassembly pane, and main debugger window.
- `memory-tools-memory.cc` - Memory Viewer mapping/state core: Physical/Virtual region lookup, selection synchronization, map collection/refresh, and alias lookup.
- `memory-tools-memory-ui.cc` - Memory Viewer rendering/edit frontend: byte grid, edit field, Memory Map pane, and three-pane workspace.
- `memory-tools-labels.cc` - label dump/export core and guest-address translation for exported label files.
- `memory-tools-labels-ui.cc` - Labels browser/import/filter/render frontend.
- `memory-tools-search.cc` / `memory-tools-search-ui.cc` - Memory Search execution/comparison/snapshot core and rendering frontend.
- `memory-tools-dump.cc` / `memory-tools-dump-ui.cc` - RAM dump/path/map/write core and dump-control rendering frontend.
- `label-symbol-utils.hh` - shared ASCII and Microsoft symbol-display helpers used by XDK/MAP/PDB importers.
- `memory-tools-inject.cc` - Inject NOP/Change/Restore/CodeCave action/state core.
- `memory-tools-inject-ui.cc` - Inject Change/CodeCave windows plus the shared address context-menu rendering.
- `x86-cheat-assembler.cc` - Type-F0 directive expansion, temp/preserve handling, labels/data, and public assembler frontend.
- `x86-cheat-assembler-encode.cc` / `x86-cheat-assembler-internal.hh` - low-level 32-bit operand parsing and instruction encoding used by the assembler frontend.
- Other `memory-tools*.cc/.hh` files - remaining shared Memory Tools ownership.
- `hdd-directory.cc` / `hdd-directory-ui.cc` - HDD browser action/state core and rendering/popup frontend.
- `hdd-snapshot-service.cc/.hh`, `hdd-export-service.cc/.hh` - coherent FATX snapshots, verification, capacity/readback, and collision-safe host export.
- `fatx-hdd.cc/.hh` - read-side FATX parser/file streaming/free-space logic; now reuses the shared binary parsing helpers without changing on-disk semantics.
- `kernel-rpc-filesystem.cc` / `kernel-rpc-filesystem-stream.cc` / `kernel-rpc-filesystem-internal.hh` - filesystem planning/preflight plus isolated host-file streaming state used by the Xbox-kernel executor.
- `backend/xemu-dbg.c/.h`, `backend/whpx-debug.c/.h` - debugger backend bridge.
- `tests/` - historical freeze guards, current feature guards, native goldens,
  and Heavy randomized regression models.

## Upstream integration points

The intended upstream-touch surface remains deliberately small. In particular,
the PREENTRY reset lifecycle uses only the existing two-line Reset notification
bridge in `ui/xui/actions.cc`; PREENTRY execution/state remains inside Debug
Tools.

Capstone is source-owned by Debug Tools rather than by GitHub workflow edits.
`build.sh` detects `ui/xui/debug-tools/build-capstone.sh`, reuses a compatible
target Capstone when one is already available, or builds the pinned static
Capstone with only the Xbox-required X86 decoder for the active xemu target.
The helper is target-aware for Linux x86_64/AArch64, macOS x86_64/ARM64, and
Windows x86_64/ARM64. Upstream multi-platform workflow YAML remains unchanged.
`build-capstone-windows.sh` is retained only as a compatibility wrapper for the
existing local Docker builder.

## Validation

The regression suite is under current **v2.87** ownership.
`tests/v287-run-regression-tests.py` exposes Static, Native, and Heavy phases, targeted
`--test` selection, compiler selection, per-command timing, and optional Static
`--jobs` parallelism. Stable behavior-based tests keep subsystem-oriented names,
while historical release-numbered guards are consolidated into the current
`v287-*` suites.

`v287-final-production-audit-golden.py` owns the current v2.88 complete 87-file production
fingerprint and the cumulative dead-code/helper-ownership audit.
`v287-ownership-structure-golden.py` owns the current final split boundaries
without retaining stale per-phase byte fingerprints. Current consolidated suites
cover UI/runtime, HDD/FATX/Kernel RPC, PREENTRY/Patch/Cheat, and Windows/build/test
infrastructure regressions. Every retained top-level golden carries a v2.87
current-ownership marker.

The final production audit explicitly follows the production HDD delete/import
entry points and preserves the permanent Xbox-kernel transaction:
`Create/Open -> Write -> Flush -> Close -> fresh FATX Verify`.

Packaging validation must keep generated Python bytecode and `.git/index`
mutations out of source artifacts. The suite remains a regression layer in
addition to the pinned Windows build and runtime confirmation.

## Release history

See [`CHANGELOG.md`](CHANGELOG.md) for the complete release-by-release history.
