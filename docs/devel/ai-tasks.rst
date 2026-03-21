.. _ai-tasks:

========================
AI Task Format for OpenMidway
========================

.. contents::

Every task submitted to an AI coding assistant (GitHub Copilot, a
Copilot agent, etc.) that touches OpenMidway code **must** declare the
fields listed in this document.  The requirement exists because OpenMidway
mixes Xbox-specific logic with a large inherited QEMU/xemu tree; a task
that lacks a clear scope, risk assessment, or test plan can silently
break emulation correctness, save-state compatibility, or online
multiplayer.

Required Fields
---------------

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Field
     - Description
   * - ``target_folder``
     - The primary subtree the task operates in.  Must be one of the
       five domain roots defined in :ref:`xbox-architecture`
       (``hw/xbox/``, ``hw/xbox/nv2a/``, ``net/``, ``ui/xui/``,
       ``tests/xbox/``) or an explicit justification for why a
       different path is needed.
   * - ``expected_behavior``
     - A concise description of what the code should do *after* the
       change, written as an observable outcome (e.g. "nvnet no longer
       drops broadcast packets when PROMISC is set").
   * - ``test_plan``
     - How the change will be validated.  Must reference at least one
       of: an existing test suite target (e.g. ``meson test -C build
       --suite xbox``), a new test to be added under ``tests/xbox/``,
       or a manual test procedure with clear pass/fail criteria.
   * - ``perf_risk``
     - Assessment of performance impact.  One of **None**, **Low**,
       **Medium**, or **High**, plus a one-sentence rationale.  Tasks
       that touch the NV2A render loop, APU voice processor, or
       DSP are automatically **Medium** or higher unless a benchmark
       shows otherwise.
   * - ``save_state_risk``
     - Whether the change may alter VMState layout or break in-flight
       snapshots.  One of **None**, **Low**, **Medium**, or **High**,
       plus a brief justification.  Any field added to a ``VMStateDescription``
       or a struct that is serialised must be marked at least **High**.
   * - ``multiplayer_risk``
     - Whether the change may affect System Link / Xbox Live emulation.
       One of **None**, **Low**, **Medium**, or **High**.  Changes to
       ``net/``, ``hw/xbox/mcpx/nvnet/``, or the communicator device
       (``xblc.c``) must be marked at least **Medium**.

Example Task
------------

Below is a well-formed example task declaration in YAML front-matter
format (suitable for a GitHub issue body or a Copilot workspace file):

.. code-block:: yaml

   target_folder: hw/xbox/mcpx/nvnet/
   expected_behavior: >
     nvnet correctly passes broadcast Ethernet frames to the host TAP
     backend when NvRegPacketFilterFlags has the PROMISC bit set.
     Titles that rely on broadcast-based System Link discovery find
     peers without manual bridge configuration.
   test_plan: >
     1. Run `meson test -C build --suite xbox` (all existing Xbox tests
        pass).
     2. Boot a System Link-enabled title in a two-instance setup and
        verify peer discovery succeeds within 10 seconds.
   perf_risk: "Low — one extra bitwise check per RX packet."
   save_state_risk: "None — no VMState structs are modified."
   multiplayer_risk: "High — directly affects broadcast packet forwarding."

Template
--------

Copy the block below into a GitHub issue, PR description, or Copilot
workspace ``task.yaml`` file and fill in every field before assigning
work to an AI agent:

.. code-block:: yaml

   target_folder: "<domain root, e.g. hw/xbox/nv2a/>"
   expected_behavior: >
     <Observable outcome after the change.>
   test_plan: >
     <How correctness will be verified — test suite target, new test,
      or manual procedure.>
   perf_risk: "<None|Low|Medium|High> — <one-sentence rationale>"
   save_state_risk: "<None|Low|Medium|High> — <one-sentence rationale>"
   multiplayer_risk: "<None|Low|Medium|High> — <one-sentence rationale>"

Enforcement
-----------

* Pull requests that lack a complete task declaration in their
  description will be flagged by the PR template checklist.
* AI agents operating on this repository are instructed (via
  ``.github/copilot-instructions.md``) to read this document and
  produce a task declaration before writing any code.

See also
--------

* :ref:`xbox-architecture` — ownership boundary definitions
* ``.github/AI_TASK_TEMPLATE.md`` — standalone Markdown template for
  GitHub issues and PR descriptions
