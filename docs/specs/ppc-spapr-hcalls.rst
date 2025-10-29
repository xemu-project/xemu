======================
sPAPR hypervisor calls
======================

When used with the ``pseries`` machine type, ``qemu-system-ppc64`` implements
a set of hypervisor calls (a.k.a. hcalls) defined in the Linux on Power
Architecture Reference ([LoPAR]_) document. This document is a subset of the
Power Architecture Platform Reference (PAPR+) specification (IBM internal only),
which is what PowerVM, the IBM proprietary hypervisor, adheres to.

The subset in LoPAR is selected based on the requirements of Linux as a guest.

In addition to those calls, we have added our own private hypervisor
calls which are mostly used as a private interface between the firmware
running in the guest and QEMU.

All those hypercalls start at hcall number 0xf000 which correspond
to an implementation specific range in PAPR.

``H_RTAS (0xf000)``
===================

RTAS stands for Run-Time Abstraction Sercies and is a set of runtime services
generally provided by the firmware inside the guest to the operating system. It
predates the existence of hypervisors (it was originally an extension to Open
Firmware) and is still used by PAPR and LoPAR to provide various services that
are not performance sensitive.

We currently implement the RTAS services in QEMU itself. The actual RTAS
"firmware" blob in the guest is a small stub of a few instructions which
calls our private H_RTAS hypervisor call to pass the RTAS calls to QEMU.

Arguments:

  ``r3``: ``H_RTAS (0xf000)``

  ``r4``: Guest physical address of RTAS parameter block.

Returns:

  ``H_SUCCESS``: Successfully called the RTAS function (RTAS result will have
  been stored in the parameter block).

  ``H_PARAMETER``: Unknown token.

``H_LOGICAL_MEMOP (0xf001)``
============================

When the guest runs in "real mode" (in powerpc terminology this means with MMU
disabled, i.e. guest effective address equals to guest physical address), it
only has access to a subset of memory and no I/Os.

PAPR and LoPAR provides a set of hypervisor calls to perform cacheable or
non-cacheable accesses to any guest physical addresses that the
guest can use in order to access IO devices while in real mode.

This is typically used by the firmware running in the guest.

However, doing a hypercall for each access is extremely inefficient
(even more so when running KVM) when accessing the frame buffer. In
that case, things like scrolling become unusably slow.

This hypercall allows the guest to request a "memory op" to be applied
to memory. The supported memory ops at this point are to copy a range
of memory (supports overlap of source and destination) and XOR which
is used by our SLOF firmware to invert the screen.

Arguments:

  ``r3 ``: ``H_LOGICAL_MEMOP (0xf001)``

  ``r4``: Guest physical address of destination.

  ``r5``: Guest physical address of source.

  ``r6``: Individual element size, defined by the binary logarithm of the
  desired size. Supported values are:

    ``0`` = 1 byte

    ``1`` = 2 bytes

    ``2`` = 4 bytes

    ``3`` = 8 bytes

  ``r7``: Number of elements.

  ``r8``: Operation. Supported values are:

    ``0``: copy

    ``1``: xor

Returns:

  ``H_SUCCESS``: Success.

  ``H_PARAMETER``: Invalid argument.
