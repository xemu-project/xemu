QEMU<->ACPI BIOS NVDIMM interface
=================================

QEMU supports NVDIMM via ACPI. This document describes the basic concepts of
NVDIMM ACPI and the interface between QEMU and the ACPI BIOS.

NVDIMM ACPI Background
----------------------

NVDIMM is introduced in ACPI 6.0 which defines an NVDIMM root device under
_SB scope with a _HID of "ACPI0012". For each NVDIMM present or intended
to be supported by platform, platform firmware also exposes an ACPI
Namespace Device under the root device.

The NVDIMM child devices under the NVDIMM root device are defined with _ADR
corresponding to the NFIT device handle. The NVDIMM root device and the
NVDIMM devices can have device specific methods (_DSM) to provide additional
functions specific to a particular NVDIMM implementation.

This is an example from ACPI 6.0, a platform contains one NVDIMM::

  Scope (\_SB){
     Device (NVDR) // Root device
     {
        Name (_HID, "ACPI0012")
        Method (_STA) {...}
        Method (_FIT) {...}
        Method (_DSM, ...) {...}
        Device (NVD)
        {
           Name(_ADR, h) //where h is NFIT Device Handle for this NVDIMM
           Method (_DSM, ...) {...}
        }
     }
  }

Methods supported on both NVDIMM root device and NVDIMM device
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

_DSM (Device Specific Method)
   It is a control method that enables devices to provide device specific
   control functions that are consumed by the device driver.
   The NVDIMM DSM specification can be found at
   http://pmem.io/documents/NVDIMM_DSM_Interface_Example.pdf

   Arguments:

   Arg0
     A Buffer containing a UUID (16 Bytes)
   Arg1
     An Integer containing the Revision ID (4 Bytes)
   Arg2
     An Integer containing the Function Index (4 Bytes)
   Arg3
     A package containing parameters for the function specified by the
     UUID, Revision ID, and Function Index

   Return Value:

   If Function Index = 0, a Buffer containing a function index bitfield.
   Otherwise, the return value and type depends on the UUID, revision ID
   and function index which are described in the DSM specification.

Methods on NVDIMM ROOT Device
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

_FIT(Firmware Interface Table)
   It evaluates to a buffer returning data in the format of a series of NFIT
   Type Structure.

   Arguments: None

   Return Value:
   A Buffer containing a list of NFIT Type structure entries.

   The detailed definition of the structure can be found at ACPI 6.0: 5.2.25
   NVDIMM Firmware Interface Table (NFIT).

QEMU NVDIMM Implementation
--------------------------

QEMU uses 4 bytes IO Port starting from 0x0a18 and a RAM-based memory page
for NVDIMM ACPI.

Memory:
   QEMU uses BIOS Linker/loader feature to ask BIOS to allocate a memory
   page and dynamically patch its address into an int32 object named "MEMA"
   in ACPI.

   This page is RAM-based and it is used to transfer data between _DSM
   method and QEMU. If ACPI has control, this pages is owned by ACPI which
   writes _DSM input data to it, otherwise, it is owned by QEMU which
   emulates _DSM access and writes the output data to it.

   ACPI writes _DSM Input Data (based on the offset in the page):

   [0x0 - 0x3]
      4 bytes, NVDIMM Device Handle.

      The handle is completely QEMU internal thing, the values in
      range [1, 0xFFFF] indicate nvdimm device. Other values are
      reserved for other purposes.

      Reserved handles:

      - 0 is reserved for nvdimm root device named NVDR.
      - 0x10000 is reserved for QEMU internal DSM function called on
        the root device.

   [0x4 - 0x7]
      4 bytes, Revision ID, that is the Arg1 of _DSM method.

   [0x8 - 0xB]
      4 bytes. Function Index, that is the Arg2 of _DSM method.

   [0xC - 0xFFF]
      4084 bytes, the Arg3 of _DSM method.

   QEMU writes Output Data (based on the offset in the page):

   [0x0 - 0x3]
      4 bytes, the length of result

   [0x4 - 0xFFF]
      4092 bytes, the DSM result filled by QEMU

IO Port 0x0a18 - 0xa1b:
   ACPI writes the address of the memory page allocated by BIOS to this
   port then QEMU gets the control and fills the result in the memory page.

   Write Access:

   [0x0a18 - 0xa1b]
      4 bytes, the address of the memory page allocated by BIOS.

_DSM process diagram
--------------------

"MEMA" indicates the address of memory page allocated by BIOS.

::

 +----------------------+      +-----------------------+
 |    1. OSPM           |      |    2. OSPM            |
 | save _DSM input data |      |  write "MEMA" to      | Exit to QEMU
 | to the page          +----->|  IO port 0x0a18       +------------+
 | indicated by "MEMA"  |      |                       |            |
 +----------------------+      +-----------------------+            |
                                                                    |
                                                                    v
 +--------------------+       +-----------+      +------------------+--------+
 |      5 QEMU        |       | 4 QEMU    |      |        3. QEMU            |
 | write _DSM result  |       |  emulate  |      | get _DSM input data from  |
 | to the page        +<------+ _DSM      +<-----+ the page indicated by the |
 |                    |       |           |      | value from the IO port    |
 +--------+-----------+       +-----------+      +---------------------------+
          |
          | Enter Guest
          |
          v
 +--------------------------+      +--------------+
 |     6 OSPM               |      |   7 OSPM     |
 | result size is returned  |      |  _DSM return |
 | by reading  DSM          +----->+              |
 | result from the page     |      |              |
 +--------------------------+      +--------------+

NVDIMM hotplug
--------------

ACPI BIOS GPE.4 handler is dedicated for notifying OS about nvdimm device
hot-add event.

QEMU internal use only _DSM functions
-------------------------------------

Read FIT
^^^^^^^^

_FIT method uses _DSM method to fetch NFIT structures blob from QEMU
in 1 page sized increments which are then concatenated and returned
as _FIT method result.

Input parameters:

Arg0
  UUID {set to 648B9CF2-CDA1-4312-8AD9-49C4AF32BD62}
Arg1
  Revision ID (set to 1)
Arg2
  Function Index, 0x1
Arg3
  A package containing a buffer whose layout is as follows:

   +----------+--------+--------+-------------------------------------------+
   |  Field   | Length | Offset |                 Description               |
   +----------+--------+--------+-------------------------------------------+
   | offset   |   4    |   0    | offset in QEMU's NFIT structures blob to  |
   |          |        |        | read from                                 |
   +----------+--------+--------+-------------------------------------------+

Output layout in the dsm memory page:

   +----------+--------+--------+-------------------------------------------+
   | Field    | Length | Offset | Description                               |
   +----------+--------+--------+-------------------------------------------+
   | length   | 4      | 0      | length of entire returned data            |
   |          |        |        | (including this header)                   |
   +----------+--------+--------+-------------------------------------------+
   |          |        |        | return status codes                       |
   |          |        |        |                                           |
   |          |        |        | - 0x0 - success                           |
   |          |        |        | - 0x100 - error caused by NFIT update     |
   | status   | 4      | 4      |   while read by _FIT wasn't completed     |
   |          |        |        | - other codes follow Chapter 3 in         |
   |          |        |        |   DSM Spec Rev1                           |
   +----------+--------+--------+-------------------------------------------+
   | fit data | Varies | 8      | contains FIT data. This field is present  |
   |          |        |        | if status field is 0.                     |
   +----------+--------+--------+-------------------------------------------+

The FIT offset is maintained by the OSPM itself, current offset plus
the size of the fit data returned by the function is the next offset
OSPM should read. When all FIT data has been read out, zero fit data
size is returned.

If it returns status code 0x100, OSPM should restart to read FIT (read
from offset 0 again).
