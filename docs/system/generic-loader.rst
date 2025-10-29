..
   Copyright (c) 2016, Xilinx Inc.

   This work is licensed under the terms of the GNU GPL, version 2 or later.  See
   the COPYING file in the top-level directory.

Generic Loader
--------------

The 'loader' device allows the user to load multiple images or values into
QEMU at startup.

Loading Data into Memory Values
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
The loader device allows memory values to be set from the command line. This
can be done by following the syntax below::

   -device loader,addr=<addr>,data=<data>,data-len=<data-len> \
                   [,data-be=<data-be>][,cpu-num=<cpu-num>]

``<addr>``
  The address to store the data in.

``<data>``
  The value to be written to the address. The maximum size of the data
  is 8 bytes.

``<data-len>``
  The length of the data in bytes. This argument must be included if
  the data argument is.

``<data-be>``
  Set to true if the data to be stored on the guest should be written
  as big endian data. The default is to write little endian data.

``<cpu-num>``
  The number of the CPU's address space where the data should be
  loaded. If not specified the address space of the first CPU is used.

All values are parsed using the standard QemuOps parsing. This allows the user
to specify any values in any format supported. By default the values
will be parsed as decimal. To use hex values the user should prefix the number
with a '0x'.

An example of loading value 0x8000000e to address 0xfd1a0104 is::

    -device loader,addr=0xfd1a0104,data=0x8000000e,data-len=4

Setting a CPU's Program Counter
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The loader device allows the CPU's PC to be set from the command line. This
can be done by following the syntax below::

     -device loader,addr=<addr>,cpu-num=<cpu-num>

``<addr>``
  The value to use as the CPU's PC.

``<cpu-num>``
  The number of the CPU whose PC should be set to the specified value.

All values are parsed using the standard QemuOpts parsing. This allows the user
to specify any values in any format supported. By default the values
will be parsed as decimal. To use hex values the user should prefix the number
with a '0x'.

An example of setting CPU 0's PC to 0x8000 is::

    -device loader,addr=0x8000,cpu-num=0

Loading Files
^^^^^^^^^^^^^

The loader device also allows files to be loaded into memory. It can load ELF,
U-Boot, and Intel HEX executable formats as well as raw images.  The syntax is
shown below:

    -device loader,file=<file>[,addr=<addr>][,cpu-num=<cpu-num>][,force-raw=<raw>]

``<file>``
  A file to be loaded into memory

``<addr>``
  The memory address where the file should be loaded. This is required
  for raw images and ignored for non-raw files.

``<cpu-num>``
  This specifies the CPU that should be used. This is an
  optional argument and will cause the CPU's PC to be set to the
  memory address where the raw file is loaded or the entry point
  specified in the executable format header. This option should only
  be used for the boot image. This will also cause the image to be
  written to the specified CPU's address space. If not specified, the
  default is CPU 0.

``<force-raw>``
  Setting 'force-raw=on' forces the file to be treated as a raw image.
  This can be used to load supported executable formats as if they
  were raw.

All values are parsed using the standard QemuOpts parsing. This allows the user
to specify any values in any format supported. By default the values
will be parsed as decimal. To use hex values the user should prefix the number
with a '0x'.

An example of loading an ELF file which CPU0 will boot is shown below::

    -device loader,file=./images/boot.elf,cpu-num=0

Restrictions and ToDos
^^^^^^^^^^^^^^^^^^^^^^

At the moment it is just assumed that if you specify a cpu-num then
you want to set the PC as well. This might not always be the case. In
future the internal state 'set_pc' (which exists in the generic loader
now) should be exposed to the user so that they can choose if the PC
is set or not.


