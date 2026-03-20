.. _xbox-architecture:

==========================
OpenXbox Architecture Guide
==========================

.. contents::

This document describes the explicit **ownership boundaries** for all
Xbox-specific code in OpenXbox.  Because OpenXbox grafts a significant amount
of Xbox-specific logic onto a large, inherited QEMU/xemu tree, clear boundaries
are essential for both human contributors and AI coding tools to avoid
unintended cross-domain side effects.

Overview
--------

OpenXbox divides its Xbox-related work into five primary domains.  Each domain
owns a specific subtree and a well-defined set of responsibilities.  Changes
that cross a boundary must be reviewed by owners of **every** affected domain.

+-------------------+--------------------------------------------+---------------------------------+
| Domain            | Subtree                                    | One-line summary                |
+===================+============================================+=================================+
| Xbox machine      | ``hw/xbox/``                               | Machine init, MCPX boot flow,   |
|                   |                                            | SMBus devices, input/gamepad,   |
|                   |                                            | communicator/online peripherals |
+-------------------+--------------------------------------------+---------------------------------+
| NV2A GPU          | ``hw/xbox/nv2a/``                          | GPU emulation, shader           |
|                   |                                            | translation, surface/texture    |
|                   |                                            | correctness                     |
+-------------------+--------------------------------------------+---------------------------------+
| Network transport | ``net/``                                   | Generic transport plumbing      |
|                   |                                            | (host-side; Xbox-specific       |
|                   |                                            | NIC lives in ``hw/xbox/mcpx/``) |
+-------------------+--------------------------------------------+---------------------------------+
| Frontend / UI     | ``ui/xui/``                                | Frontend UX, debug overlays,    |
|                   |                                            | renderer path                   |
+-------------------+--------------------------------------------+---------------------------------+
| Xbox tests        | ``tests/xbox/``                            | Xbox-only correctness and       |
|                   |                                            | regression tests                |
+-------------------+--------------------------------------------+---------------------------------+

Domain Details
--------------

hw/xbox/ — Xbox Machine
~~~~~~~~~~~~~~~~~~~~~~~

**Subtree:** ``hw/xbox/``  (but **not** ``hw/xbox/nv2a/`` — that is its own domain)

Owns:

* Machine initialisation (``xbox.c``, ``xbox_pci.c``)
* MCPX SoC and boot-ROM flow (``hw/xbox/mcpx/``)
* SMBus controller and attached devices (``amd_smbus.c``,
  ``smbus_*.c``)
* Input devices: USB gamepad (``xid.c``, ``xid-gamepad.c``) and
  communicator / online peripherals (``xblc.c``)
* APU voice processor and DSP (``hw/xbox/mcpx/apu/``)
* Ethernet controller (``hw/xbox/mcpx/nvnet/``)

Does **not** own:

* NV2A GPU registers and pipeline — see ``hw/xbox/nv2a/``
* Generic network transport — see ``net/``

hw/xbox/nv2a/ — NV2A GPU
~~~~~~~~~~~~~~~~~~~~~~~~~

**Subtree:** ``hw/xbox/nv2a/``

Owns:

* Full NV2A register emulation (PGRAPH, PFIFO, PRAMDAC, …)
* Shader translation and GLSL/Vulkan back-ends
  (``pgraph/glsl/``, ``pgraph/vk/``, ``pgraph/gl/``)
* Surface management, texture conversion, and format correctness
* Null rendering stub (``pgraph/null/``)

Does **not** own:

* CPU-side DMA lists or command buffers initiated by the Xbox title —
  those are pushed through FIFO, which is a shared interface between
  ``hw/xbox/`` (pusher) and ``hw/xbox/nv2a/`` (consumer)

net/ — Network Transport
~~~~~~~~~~~~~~~~~~~~~~~~

**Subtree:** ``net/``

Owns:

* Host-side network backends (TAP, socket, SLIRP, pcap, …)
* Generic packet dispatch, filter, and hub infrastructure

Does **not** own:

* The Xbox NIC (nvnet) device model — that lives in
  ``hw/xbox/mcpx/nvnet/``

ui/xui/ — Frontend UX
~~~~~~~~~~~~~~~~~~~~~~

**Subtree:** ``ui/xui/``

Owns:

* Main menu, settings panel, and all in-emulator overlay windows
* Font rendering and animation helpers
* Input-manager integration for the UI layer
* Debug overlays and on-screen status information

Does **not** own:

* Low-level OpenGL / Vulkan context setup — that is in ``ui/`` proper
* NV2A surface presentation — that belongs to ``hw/xbox/nv2a/``

tests/xbox/ — Xbox Tests
~~~~~~~~~~~~~~~~~~~~~~~~~

**Subtree:** ``tests/xbox/``

Owns:

* Correctness and regression tests specific to Xbox hardware
  emulation (e.g. DSP opcode tests, texture swizzle round-trips)
* Benchmark targets scoped to Xbox subsystems

Does **not** own:

* Generic QEMU unit or functional tests — those live in
  ``tests/unit/`` and ``tests/functional/``

Cross-Domain Change Rules
-------------------------

1. A PR that modifies files in **more than one domain** must label
   itself with every affected domain (e.g. ``domain:nv2a``,
   ``domain:xbox-machine``).
2. Every new file must be placed inside exactly one domain subtree.
   If no existing subtree fits, open a discussion before creating a new
   one.
3. Changes to shared headers under ``include/hw/xbox/`` are considered
   cross-domain by default and require sign-off from the NV2A and Xbox
   machine owners.

See also
--------

* :ref:`ai-tasks` — required format for AI-generated coding tasks in
  this repository
* ``OWNERS`` files located in each domain subtree root
