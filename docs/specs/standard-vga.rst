
QEMU Standard VGA
=================

Exists in two variants, for isa and pci.

command line switches:

``-vga std``
   picks isa for -M isapc, otherwise pci
``-device VGA``
   pci variant
``-device isa-vga``
   isa variant
``-device secondary-vga``
   legacy-free pci variant


PCI spec
--------

Applies to the pci variant only for obvious reasons.

PCI ID
   ``1234:1111``

PCI Region 0
   Framebuffer memory, 16 MB in size (by default).
   Size is tunable via vga_mem_mb property.

PCI Region 1
   Reserved (so we have the option to make the framebuffer bar 64bit).

PCI Region 2
   MMIO bar, 4096 bytes in size (QEMU 1.3+)

PCI ROM Region
   Holds the vgabios (QEMU 0.14+).


The legacy-free variant has no ROM and has ``PCI_CLASS_DISPLAY_OTHER``
instead of ``PCI_CLASS_DISPLAY_VGA``.


IO ports used
-------------

Doesn't apply to the legacy-free pci variant, use the MMIO bar instead.

``03c0 - 03df``
   standard vga ports
``01ce``
   bochs vbe interface index port
``01cf``
   bochs vbe interface data port (x86 only)
``01d0``
   bochs vbe interface data port


Memory regions used
-------------------

``0xe0000000``
  Framebuffer memory, isa variant only.

The pci variant used to mirror the framebuffer bar here, QEMU 0.14+
stops doing that (except when in ``-M pc-$old`` compat mode).


MMIO area spec
--------------

Likewise applies to the pci variant only for obvious reasons.

``0000 - 03ff``
  edid data blob.
``0400 - 041f``
  vga ioports (``0x3c0`` to ``0x3df``), remapped 1:1. Word access
  is supported, bytes are written in little endian order (aka index
  port first),  so indexed registers can be updated with a single
  mmio write (and thus only one vmexit).
``0500 - 0515``
  bochs dispi interface registers, mapped flat without index/data ports.
  Use ``(index << 1)`` as offset for (16bit) register access.
``0600 - 0607``
  QEMU extended registers.  QEMU 2.2+ only.
  The pci revision is 2 (or greater) when these registers are present.
  The registers are 32bit.
``0600``
  QEMU extended register region size, in bytes.
``0604``
  framebuffer endianness register.
  - ``0xbebebebe`` indicates big endian.
  - ``0x1e1e1e1e`` indicates little endian.
