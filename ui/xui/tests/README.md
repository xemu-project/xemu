# Debug Tools regression tests — v2.87

This directory is the current **v2.87** regression suite for the xemu Debug
Tools additions. Tests are organized by the behavior or ownership contract they
protect rather than by the release in which each regression was first found.
Historical version labels are retained inside the consolidated suites only as
provenance.

## v2.87 cleanup result

The v2.86.1 baseline contained 145 top-level test/support files, including 96
standalone `v###-...-golden.py` historical guards. v2.87 reduces that to 55
top-level files:

- 33 Python golden tests.
- 19 native C++ golden tests.
- `v287-run-regression-tests.py`, `v287_source_test_utils.py`, and this README.
- Three small native-test stub headers remain in the existing subdirectories.

All retained top-level `*-golden.py` and `*-golden.cpp` files carry a
`v2.87 current regression ownership` marker. Stable behavior-based filenames
remain stable; old version-specific filenames are consolidated under current
`v287-...` suites so future releases do not need to maintain a long chain of
release-numbered files.

No production Debug Tools source is changed by this cleanup.

## Current v2.87 consolidated suites

### `v287-final-production-audit-golden.py`

Owns the complete 87-file production Debug Tools fingerprint and the final
whole-tree production audit. It rejects restoration of retired Kernel RPC
mutation diagnostics/test wrappers, protects shared helper ownership, and
follows the production HDD delete/import paths through the Xbox-kernel executor.

The permanent HDD mutation invariant remains:

`Create/Open -> Write -> Flush -> Close -> fresh FATX Verify`

### `v287-ownership-structure-golden.py`

Replaces the stale v2.72-v2.85 per-phase structural/fingerprint guards with one
current semantic ownership test. It checks the final split boundaries for:

- shared binary and label-symbol helpers;
- Cheat Engine source/PREENTRY, F-hook, RAW execution, UI, and Tick ownership;
- Debugger core/UI;
- Memory Viewer core/UI;
- Current Game core/UI;
- Labels, Search, Dump, and Inject core/UI;
- x86 assembler frontend/encoder ownership;
- HDD core/UI, FATX shared helpers, and filesystem host-stream ownership;
- Meson and combined-source/native-runner registration needed by those splits.

Historical exact whole-tree fingerprints are not duplicated here; the current
production fingerprint is owned only by the final production audit.

### `v287-ui-runtime-regressions-golden.py`

Consolidates seven retained UI/runtime contracts: detached playback, COPY ALL
registers, detached Current Game/HDD behavior, detached font handling, Inject
Restore safety, F0/F1 uncheck restoration, and Inject refresh-anchor behavior.

### `v287-hdd-krpc-regressions-golden.py`

Consolidates 44 retained HDD/FATX/Kernel-RPC contracts covering the proven path
from FATX metadata/export and the harmless Kernel RPC foundation through
recursive delete/import, New Folder/Rename/Copy/Move, F:/G: partition handling,
coherent snapshots, capacity preflight, whole-tree/content verification,
partition-scoped verification, atomic host export, current Disc export, and the
cumulative HDD/KRPC safety/integrity audits.

### `v287-preentry-cheat-regressions-golden.py`

Consolidates 21 retained PREENTRY/Patch/Cheat contracts covering parser syntax,
selection identity, reset/retry lifecycle, Type-F ownership, live-cheat
isolation, game identity, deterministic file selection, per-patch errors,
lifecycle status, mixed-group checkbox state, tab styling, and the const-safety
compile hotfix.

### `v287-platform-infrastructure-regressions-golden.py`

Consolidates 12 retained platform/build/test-infrastructure contracts. This
includes guest-pause/QEMU include ownership, regression-runner phase/QoL checks,
documentation/history separation, temporary Git-index safety, ImGui style-stack
balance, targeted formatting cleanup, Windows `qemu_close_wrap` macro safety,
incremental-LTO cache isolation, the v2.85.1 `<algorithm>`/`std::clamp` Windows
compile fix, and the v2.86.1 Current Game anonymous-namespace compile fix.

## Behavior-based golden tests

The remaining stable-name goldens are not historical release markers. They are
direct current behavior tests and remain individually named so they can be run
and targeted by subsystem:

- Assembler / allocation / F0: `assembler-golden.cpp`, `v287-allocator-golden.py`,
  `v287-f0-handoff-golden.py`, `v287-f0-steady-state-golden.py`.
- Debugger / conditions / registers: `conditions-golden.cpp`,
  `v287-debugger-conditions-registers-golden.py`, `v287-debugger-navigation-focus-golden.py`,
  `v287-debugger-streamlining-golden.py`, `register-copy-golden.cpp`.
- Memory Viewer / Search / dump: `v287-memory-format-golden.py`,
  `v287-memory-map-golden.py`, `v287-search-compare-golden.py`, `v287-dump-ram-golden.py`, and
  the retained Pass 1-11 behavioral/optimization guards.
- FATX / Kernel RPC / filesystem: `fatx-hdd-golden.cpp`, `kernel-rpc-golden.cpp`,
  `kernel-rpc-filesystem-golden.cpp`, `filesystem-transfer-contract-golden.cpp`,
  `guest-pause-guard-golden.cpp`, `guest-pause-fstream-order-golden.cpp`, and
  `windows-transferkind-macro-golden.cpp`.
- Labels / disc: `label-packs-golden.cpp`, `label-batch-golden.cpp`,
  `label-symbol-utils-golden.cpp`, `v287-labels-ui-golden.py`, `map-labels-golden.cpp`,
  `pdb-labels-golden.cpp`, `xbe-labels-golden.cpp`, `xdk-labels-golden.cpp`,
  `xdvdfs-golden.cpp`, `v287-current-game-disc-golden.py`, `v287-file-io-golden.py`, and
  `v287-label-performance-golden.py`.
- UI/style and current state: `tab-style-stack-golden.cpp` and the remaining
  stable-name source/model guards discovered automatically by the runner.

## Runner phases

`v287-run-regression-tests.py` keeps three independently runnable phases:

- **Static** — project/layout validation, Python syntax validation, Bash syntax
  validation, and all non-heavy Python source/model goldens. `--jobs` may run
  independent static tests in parallel.
- **Native** — GCC/Clang host-native C++ contracts for parsers, assembler,
  conditions, labels, FATX, filesystem planning/transfer, guest-pause, style
  stack, and register formatting.
- **Heavy** — the full randomized allocator, Memory Viewer formatting, Memory
  Search comparison, debugger/F-hook model, and F0 steady-state stress coverage.

Use `--test` for targeted shell-style name/stem selection and `--compiler` to
select native/heavy compilers.

These tests are regression guards, not replacements for the pinned Windows
build and runtime confirmation.
