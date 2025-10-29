Adjunct Processor (AP) Device
=============================

.. contents::

Introduction
------------

The IBM Adjunct Processor (AP) Cryptographic Facility is comprised
of three AP instructions and from 1 to 256 PCIe cryptographic adapter cards.
These AP devices provide cryptographic functions to all CPUs assigned to a
linux system running in an IBM Z system LPAR.

On s390x, AP adapter cards are exposed via the AP bus. This document
describes how those cards may be made available to KVM guests using the
VFIO mediated device framework.

AP Architectural Overview
-------------------------

In order understand the terminology used in the rest of this document, let's
start with some definitions:

* AP adapter

  An AP adapter is an IBM Z adapter card that can perform cryptographic
  functions. There can be from 0 to 256 adapters assigned to an LPAR depending
  on the machine model. Adapters assigned to the LPAR in which a linux host is
  running will be available to the linux host. Each adapter is identified by a
  number from 0 to 255; however, the maximum adapter number allowed is
  determined by machine model. When installed, an AP adapter is accessed by
  AP instructions executed by any CPU.

* AP domain

  An adapter is partitioned into domains. Each domain can be thought of as
  a set of hardware registers for processing AP instructions. An adapter can
  hold up to 256 domains; however, the maximum domain number allowed is
  determined by machine model. Each domain is identified by a number from 0 to
  255. Domains can be further classified into two types:

    * Usage domains are domains that can be accessed directly to process AP
      commands

    * Control domains are domains that are accessed indirectly by AP
      commands sent to a usage domain to control or change the domain; for
      example, to set a secure private key for the domain.

* AP Queue

  An AP queue is the means by which an AP command-request message is sent to an
  AP usage domain inside a specific AP. An AP queue is identified by a tuple
  comprised of an AP adapter ID (APID) and an AP queue index (APQI). The
  APQI corresponds to a given usage domain number within the adapter. This tuple
  forms an AP Queue Number (APQN) uniquely identifying an AP queue. AP
  instructions include a field containing the APQN to identify the AP queue to
  which the AP command-request message is to be sent for processing.

* AP Instructions:

  There are three AP instructions:

  * NQAP: to enqueue an AP command-request message to a queue
  * DQAP: to dequeue an AP command-reply message from a queue
  * PQAP: to administer the queues

  AP instructions identify the domain that is targeted to process the AP
  command; this must be one of the usage domains. An AP command may modify a
  domain that is not one of the usage domains, but the modified domain
  must be one of the control domains.

Start Interpretive Execution (SIE) Instruction
----------------------------------------------

A KVM guest is started by executing the Start Interpretive Execution (SIE)
instruction. The SIE state description is a control block that contains the
state information for a KVM guest and is supplied as input to the SIE
instruction. The SIE state description contains a satellite control block called
the Crypto Control Block (CRYCB). The CRYCB contains three fields to identify
the adapters, usage domains and control domains assigned to the KVM guest:

* The AP Mask (APM) field is a bit mask that identifies the AP adapters assigned
  to the KVM guest. Each bit in the mask, from left to right, corresponds to
  an APID from 0-255. If a bit is set, the corresponding adapter is valid for
  use by the KVM guest.

* The AP Queue Mask (AQM) field is a bit mask identifying the AP usage domains
  assigned to the KVM guest. Each bit in the mask, from left to right,
  corresponds to  an AP queue index (APQI) from 0-255. If a bit is set, the
  corresponding queue is valid for use by the KVM guest.

* The AP Domain Mask field is a bit mask that identifies the AP control domains
  assigned to the KVM guest. The ADM bit mask controls which domains can be
  changed by an AP command-request message sent to a usage domain from the
  guest. Each bit in the mask, from left to right, corresponds to a domain from
  0-255. If a bit is set, the corresponding domain can be modified by an AP
  command-request message sent to a usage domain.

If you recall from the description of an AP Queue, AP instructions include
an APQN to identify the AP adapter and AP queue to which an AP command-request
message is to be sent (NQAP and PQAP instructions), or from which a
command-reply message is to be received (DQAP instruction). The validity of an
APQN is defined by the matrix calculated from the APM and AQM; it is the
cross product of all assigned adapter numbers (APM) with all assigned queue
indexes (AQM). For example, if adapters 1 and 2 and usage domains 5 and 6 are
assigned to a guest, the APQNs (1,5), (1,6), (2,5) and (2,6) will be valid for
the guest.

The APQNs can provide secure key functionality - i.e., a private key is stored
on the adapter card for each of its domains - so each APQN must be assigned to
at most one guest or the linux host.

Example 1: Valid configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

+----------+--------+--------+
|          | Guest1 | Guest2 |
+==========+========+========+
| adapters |  1, 2  |  1, 2  |
+----------+--------+--------+
| domains  |  5, 6  |  7     |
+----------+--------+--------+

This is valid because both guests have a unique set of APQNs:

* Guest1 has APQNs (1,5), (1,6), (2,5) and (2,6);
* Guest2 has APQNs (1,7) and (2,7).

Example 2: Valid configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

+----------+--------+--------+
|          | Guest1 | Guest2 |
+==========+========+========+
| adapters |  1, 2  |  3, 4  |
+----------+--------+--------+
| domains  |  5, 6  |  5, 6  |
+----------+--------+--------+

This is also valid because both guests have a unique set of APQNs:

* Guest1 has APQNs (1,5), (1,6), (2,5), (2,6);
* Guest2 has APQNs (3,5), (3,6), (4,5), (4,6)

Example 3: Invalid configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

+----------+--------+--------+
|          | Guest1 | Guest2 |
+==========+========+========+
| adapters |  1, 2  |  1     |
+----------+--------+--------+
| domains  |  5, 6  |  6, 7  |
+----------+--------+--------+

This is an invalid configuration because both guests have access to
APQN (1,6).

AP Matrix Configuration on Linux Host
-------------------------------------

A linux system is a guest of the LPAR in which it is running and has access to
the AP resources configured for the LPAR. The LPAR's AP matrix is
configured via its Activation Profile which can be edited on the HMC. When the
linux system is started, the AP bus will detect the AP devices assigned to the
LPAR and create the following in sysfs::

  /sys/bus/ap
  ... [devices]
  ...... xx.yyyy
  ...... ...
  ...... cardxx
  ...... ...

Where:

``cardxx``
  is AP adapter number xx (in hex)

``xx.yyyy``
  is an APQN with xx specifying the APID and yyyy specifying the APQI

For example, if AP adapters 5 and 6 and domains 4, 71 (0x47), 171 (0xab) and
255 (0xff) are configured for the LPAR, the sysfs representation on the linux
host system would look like this::

  /sys/bus/ap
  ... [devices]
  ...... 05.0004
  ...... 05.0047
  ...... 05.00ab
  ...... 05.00ff
  ...... 06.0004
  ...... 06.0047
  ...... 06.00ab
  ...... 06.00ff
  ...... card05
  ...... card06

A set of default device drivers are also created to control each type of AP
device that can be assigned to the LPAR on which a linux host is running::

  /sys/bus/ap
  ... [drivers]
  ...... [cex2acard]        for Crypto Express 2/3 accelerator cards
  ...... [cex2aqueue]       for AP queues served by Crypto Express 2/3
                            accelerator cards
  ...... [cex4card]         for Crypto Express 4/5/6 accelerator and coprocessor
                            cards
  ...... [cex4queue]        for AP queues served by Crypto Express 4/5/6
                            accelerator and coprocessor cards
  ...... [pcixcccard]       for Crypto Express 2/3 coprocessor cards
  ...... [pcixccqueue]      for AP queues served by Crypto Express 2/3
                            coprocessor cards

Binding AP devices to device drivers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

There are two sysfs files that specify bitmasks marking a subset of the APQN
range as 'usable by the default AP queue device drivers' or 'not usable by the
default device drivers' and thus available for use by the alternate device
driver(s). The sysfs locations of the masks are::

   /sys/bus/ap/apmask
   /sys/bus/ap/aqmask

The ``apmask`` is a 256-bit mask that identifies a set of AP adapter IDs
(APID). Each bit in the mask, from left to right (i.e., from most significant
to least significant bit in big endian order), corresponds to an APID from
0-255. If a bit is set, the APID is marked as usable only by the default AP
queue device drivers; otherwise, the APID is usable by the vfio_ap
device driver.

The ``aqmask`` is a 256-bit mask that identifies a set of AP queue indexes
(APQI). Each bit in the mask, from left to right (i.e., from most significant
to least significant bit in big endian order), corresponds to an APQI from
0-255. If a bit is set, the APQI is marked as usable only by the default AP
queue device drivers; otherwise, the APQI is usable by the vfio_ap device
driver.

Take, for example, the following mask::

      0x7dffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff

It indicates:

      1, 2, 3, 4, 5, and 7-255 belong to the default drivers' pool, and 0 and 6
      belong to the vfio_ap device driver's pool.

The APQN of each AP queue device assigned to the linux host is checked by the
AP bus against the set of APQNs derived from the cross product of APIDs
and APQIs marked as usable only by the default AP queue device drivers. If a
match is detected,  only the default AP queue device drivers will be probed;
otherwise, the vfio_ap device driver will be probed.

By default, the two masks are set to reserve all APQNs for use by the default
AP queue device drivers. There are two ways the default masks can be changed:

 1. The sysfs mask files can be edited by echoing a string into the
    respective sysfs mask file in one of two formats:

    * An absolute hex string starting with 0x - like "0x12345678" - sets
      the mask. If the given string is shorter than the mask, it is padded
      with 0s on the right; for example, specifying a mask value of 0x41 is
      the same as specifying::

           0x4100000000000000000000000000000000000000000000000000000000000000

      Keep in mind that the mask reads from left to right (i.e., most
      significant to least significant bit in big endian order), so the mask
      above identifies device numbers 1 and 7 (``01000001``).

      If the string is longer than the mask, the operation is terminated with
      an error (EINVAL).

    * Individual bits in the mask can be switched on and off by specifying
      each bit number to be switched in a comma separated list. Each bit
      number string must be prepended with a (``+``) or minus (``-``) to indicate
      the corresponding bit is to be switched on (``+``) or off (``-``). Some
      valid values are::

           "+0"    switches bit 0 on
           "-13"   switches bit 13 off
           "+0x41" switches bit 65 on
           "-0xff" switches bit 255 off

      The following example::

              +0,-6,+0x47,-0xf0

      Switches bits 0 and 71 (0x47) on
      Switches bits 6 and 240 (0xf0) off

      Note that the bits not specified in the list remain as they were before
      the operation.

 2. The masks can also be changed at boot time via parameters on the kernel
    command line like this::

         ap.apmask=0xffff ap.aqmask=0x40

    This would create the following masks:

    apmask::

            0xffff000000000000000000000000000000000000000000000000000000000000

    aqmask::

            0x4000000000000000000000000000000000000000000000000000000000000000

    Resulting in these two pools::

            default drivers pool:    adapter 0-15, domain 1
            alternate drivers pool:  adapter 16-255, domains 0, 2-255

Configuring an AP matrix for a linux guest
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The sysfs interfaces for configuring an AP matrix for a guest are built on the
VFIO mediated device framework. To configure an AP matrix for a guest, a
mediated matrix device must first be created for the ``/sys/devices/vfio_ap/matrix``
device. When the vfio_ap device driver is loaded, it registers with the VFIO
mediated device framework. When the driver registers, the sysfs interfaces for
creating mediated matrix devices is created::

  /sys/devices
  ... [vfio_ap]
  ......[matrix]
  ......... [mdev_supported_types]
  ............ [vfio_ap-passthrough]
  ............... create
  ............... [devices]

A mediated AP matrix device is created by writing a UUID to the attribute file
named ``create``, for example::

   uuidgen > create

or

::

   echo $uuid > create

When a mediated AP matrix device is created, a sysfs directory named after
the UUID is created in the ``devices`` subdirectory::

  /sys/devices
  ... [vfio_ap]
  ......[matrix]
  ......... [mdev_supported_types]
  ............ [vfio_ap-passthrough]
  ............... create
  ............... [devices]
  .................. [$uuid]

There will also be three sets of attribute files created in the mediated
matrix device's sysfs directory to configure an AP matrix for the
KVM guest::

  /sys/devices
  ... [vfio_ap]
  ......[matrix]
  ......... [mdev_supported_types]
  ............ [vfio_ap-passthrough]
  ............... create
  ............... [devices]
  .................. [$uuid]
  ..................... assign_adapter
  ..................... assign_control_domain
  ..................... assign_domain
  ..................... matrix
  ..................... unassign_adapter
  ..................... unassign_control_domain
  ..................... unassign_domain

``assign_adapter``
   To assign an AP adapter to the mediated matrix device, its APID is written
   to the ``assign_adapter`` file. This may be done multiple times to assign more
   than one adapter. The APID may be specified using conventional semantics
   as a decimal, hexadecimal, or octal number. For example, to assign adapters
   4, 5 and 16 to a mediated matrix device in decimal, hexadecimal and octal
   respectively::

       echo 4 > assign_adapter
       echo 0x5 > assign_adapter
       echo 020 > assign_adapter

   In order to successfully assign an adapter:

   * The adapter number specified must represent a value from 0 up to the
     maximum adapter number allowed by the machine model. If an adapter number
     higher than the maximum is specified, the operation will terminate with
     an error (ENODEV).

   * All APQNs that can be derived from the adapter ID being assigned and the
     IDs of the previously assigned domains must be bound to the vfio_ap device
     driver. If no domains have yet been assigned, then there must be at least
     one APQN with the specified APID bound to the vfio_ap driver. If no such
     APQNs are bound to the driver, the operation will terminate with an
     error (EADDRNOTAVAIL).

   * No APQN that can be derived from the adapter ID and the IDs of the
     previously assigned domains can be assigned to another mediated matrix
     device. If an APQN is assigned to another mediated matrix device, the
     operation will terminate with an error (EADDRINUSE).

``unassign_adapter``
   To unassign an AP adapter, its APID is written to the ``unassign_adapter``
   file. This may also be done multiple times to unassign more than one adapter.

``assign_domain``
   To assign a usage domain, the domain number is written into the
   ``assign_domain`` file. This may be done multiple times to assign more than one
   usage domain. The domain number is specified using conventional semantics as
   a decimal, hexadecimal, or octal number. For example, to assign usage domains
   4, 8, and 71 to a mediated matrix device in decimal, hexadecimal and octal
   respectively::

      echo 4 > assign_domain
      echo 0x8 > assign_domain
      echo 0107 > assign_domain

   In order to successfully assign a domain:

   * The domain number specified must represent a value from 0 up to the
     maximum domain number allowed by the machine model. If a domain number
     higher than the maximum is specified, the operation will terminate with
     an error (ENODEV).

   * All APQNs that can be derived from the domain ID being assigned and the IDs
     of the previously assigned adapters must be bound to the vfio_ap device
     driver. If no domains have yet been assigned, then there must be at least
     one APQN with the specified APQI bound to the vfio_ap driver. If no such
     APQNs are bound to the driver, the operation will terminate with an
     error (EADDRNOTAVAIL).

   * No APQN that can be derived from the domain ID being assigned and the IDs
     of the previously assigned adapters can be assigned to another mediated
     matrix device. If an APQN is assigned to another mediated matrix device,
     the operation will terminate with an error (EADDRINUSE).

``unassign_domain``
   To unassign a usage domain, the domain number is written into the
   ``unassign_domain`` file. This may be done multiple times to unassign more than
   one usage domain.

``assign_control_domain``
   To assign a control domain, the domain number is written into the
   ``assign_control_domain`` file. This may be done multiple times to
   assign more than one control domain. The domain number may be specified using
   conventional semantics as a decimal, hexadecimal, or octal number. For
   example, to assign  control domains 4, 8, and 71 to  a mediated matrix device
   in decimal, hexadecimal and octal respectively::

      echo 4 > assign_domain
      echo 0x8 > assign_domain
      echo 0107 > assign_domain

   In order to successfully assign a control domain, the domain number
   specified must represent a value from 0 up to the maximum domain number
   allowed by the machine model. If a control domain number higher than the
   maximum is specified, the operation will terminate with an error (ENODEV).

``unassign_control_domain``
   To unassign a control domain, the domain number is written into the
   ``unassign_domain`` file. This may be done multiple times to unassign more than
   one control domain.

Notes: No changes to the AP matrix will be allowed while a guest using
the mediated matrix device is running. Attempts to assign an adapter,
domain or control domain will be rejected and an error (EBUSY) returned.

Starting a Linux Guest Configured with an AP Matrix
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To provide a mediated matrix device for use by a guest, the following option
must be specified on the QEMU command line::

   -device vfio_ap,sysfsdev=$path-to-mdev

The sysfsdev parameter specifies the path to the mediated matrix device.
There are a number of ways to specify this path::

  /sys/devices/vfio_ap/matrix/$uuid
  /sys/bus/mdev/devices/$uuid
  /sys/bus/mdev/drivers/vfio_mdev/$uuid
  /sys/devices/vfio_ap/matrix/mdev_supported_types/vfio_ap-passthrough/devices/$uuid

When the linux guest is started, the guest will open the mediated
matrix device's file descriptor to get information about the mediated matrix
device. The ``vfio_ap`` device driver will update the APM, AQM, and ADM fields in
the guest's CRYCB with the adapter, usage domain and control domains assigned
via the mediated matrix device's sysfs attribute files. Programs running on the
linux guest will then:

1. Have direct access to the APQNs derived from the cross product of the AP
   adapter numbers (APID) and queue indexes (APQI) specified in the APM and AQM
   fields of the guests's CRYCB respectively. These APQNs identify the AP queues
   that are valid for use by the guest; meaning, AP commands can be sent by the
   guest to any of these queues for processing.

2. Have authorization to process AP commands to change a control domain
   identified in the ADM field of the guest's CRYCB. The AP command must be sent
   to a valid APQN (see 1 above).

CPU model features:

Three CPU model features are available for controlling guest access to AP
facilities:

1. AP facilities feature

   The AP facilities feature indicates that AP facilities are installed on the
   guest. This feature will be exposed for use only if the AP facilities
   are installed on the host system. The feature is s390-specific and is
   represented as a parameter of the -cpu option on the QEMU command line::

      qemu-system-s390x -cpu $model,ap=on|off

   Where:

      ``$model``
        is the CPU model defined for the guest (defaults to the model of
        the host system if not specified).

      ``ap=on|off``
        indicates whether AP facilities are installed (on) or not
        (off). The default for CPU models zEC12 or newer
        is ``ap=on``. AP facilities must be installed on the guest if a
        vfio-ap device (``-device vfio-ap,sysfsdev=$path``) is configured
        for the guest, or the guest will fail to start.

2. Query Configuration Information (QCI) facility

   The QCI facility is used by the AP bus running on the guest to query the
   configuration of the AP facilities. This facility will be available
   only if the QCI facility is installed on the host system. The feature is
   s390-specific and is represented as a parameter of the -cpu option on the
   QEMU command line::

      qemu-system-s390x -cpu $model,apqci=on|off

   Where:

      ``$model``
        is the CPU model defined for the guest

      ``apqci=on|off``
        indicates whether the QCI facility is installed (on) or
        not (off). The default for CPU models zEC12 or newer
        is ``apqci=on``; for older models, QCI will not be installed.

        If QCI is installed (``apqci=on``) but AP facilities are not
        (``ap=off``), an error message will be logged, but the guest
        will be allowed to start. It makes no sense to have QCI
        installed if the AP facilities are not; this is considered
        an invalid configuration.

        If the QCI facility is not installed, APQNs with an APQI
        greater than 15 will not be detected by the AP bus
        running on the guest.

3. Adjunct Process Facility Test (APFT) facility

   The APFT facility is used by the AP bus running on the guest to test the
   AP facilities available for a given AP queue. This facility will be available
   only if the APFT facility is installed on the host system. The feature is
   s390-specific and is represented as a parameter of the -cpu option on the
   QEMU command line::

      qemu-system-s390x -cpu $model,apft=on|off

   Where:

      ``$model``
        is the CPU model defined for the guest (defaults to the model of
        the host system if not specified).

      ``apft=on|off``
        indicates whether the APFT facility is installed (on) or
        not (off). The default for CPU models zEC12 and
        newer is ``apft=on`` for older models, APFT will not be
        installed.

        If APFT is installed (``apft=on``) but AP facilities are not
        (``ap=off``), an error message will be logged, but the guest
        will be allowed to start. It makes no sense to have APFT
        installed if the AP facilities are not; this is considered
        an invalid configuration.

        It also makes no sense to turn APFT off because the AP bus
        running on the guest will not detect CEX4 and newer devices
        without it. Since only CEX4 and newer devices are supported
        for guest usage, no AP devices can be made accessible to a
        guest started without APFT installed.

Hot plug a vfio-ap device into a running guest
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Only one vfio-ap device can be attached to the virtual machine's ap-bus, so a
vfio-ap device can be hot plugged if and only if no vfio-ap device is attached
to the bus already, whether via the QEMU command line or a prior hot plug
action.

To hot plug a vfio-ap device, use the QEMU ``device_add`` command::

    (qemu) device_add vfio-ap,sysfsdev="$path-to-mdev",id="$id"

Where the ``$path-to-mdev`` value specifies the absolute path to a mediated
device to which AP resources to be used by the guest have been assigned.
``$id`` is the name value for the optional id parameter.

Note that on Linux guests, the AP devices will be created in the
``/sys/bus/ap/devices`` directory when the AP bus subsequently performs its periodic
scan, so there may be a short delay before the AP devices are accessible on the
guest.

The command will fail if:

* A vfio-ap device has already been attached to the virtual machine's ap-bus.

* The CPU model features for controlling guest access to AP facilities are not
  enabled (see 'CPU model features' subsection in the previous section).

Hot unplug a vfio-ap device from a running guest
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A vfio-ap device can be unplugged from a running KVM guest if a vfio-ap device
has been attached to the virtual machine's ap-bus via the QEMU command line
or a prior hot plug action.

To hot unplug a vfio-ap device, use the QEMU ``device_del`` command::

    (qemu) device_del "$id"

Where ``$id`` is the same id that was specified at device creation.

On a Linux guest, the AP devices will be removed from the ``/sys/bus/ap/devices``
directory on the guest when the AP bus subsequently performs its periodic scan,
so there may be a short delay before the AP devices are no longer accessible by
the guest.

The command will fail if the ``$path-to-mdev`` specified on the ``device_del`` command
does not match the value specified when the vfio-ap device was attached to
the virtual machine's ap-bus.

Example: Configure AP Matrices for Three Linux Guests
-----------------------------------------------------

Let's now provide an example to illustrate how KVM guests may be given
access to AP facilities. For this example, we will show how to configure
three guests such that executing the lszcrypt command on the guests would
look like this:

Guest1::

  CARD.DOMAIN TYPE  MODE
  ------------------------------
  05          CEX5C CCA-Coproc
  05.0004     CEX5C CCA-Coproc
  05.00ab     CEX5C CCA-Coproc
  06          CEX5A Accelerator
  06.0004     CEX5A Accelerator
  06.00ab     CEX5C CCA-Coproc

Guest2::

  CARD.DOMAIN TYPE  MODE
  ------------------------------
  05          CEX5A Accelerator
  05.0047     CEX5A Accelerator
  05.00ff     CEX5A Accelerator

Guest3::

  CARD.DOMAIN TYPE  MODE
  ------------------------------
  06          CEX5A Accelerator
  06.0047     CEX5A Accelerator
  06.00ff     CEX5A Accelerator

These are the steps:

1. Install the vfio_ap module on the linux host. The dependency chain for the
   vfio_ap module is:

   * iommu
   * s390
   * zcrypt
   * vfio
   * vfio_mdev
   * vfio_mdev_device
   * KVM

   To build the vfio_ap module, the kernel build must be configured with the
   following Kconfig elements selected:

   * IOMMU_SUPPORT
   * S390
   * ZCRYPT
   * S390_AP_IOMMU
   * VFIO
   * VFIO_MDEV
   * VFIO_MDEV_DEVICE
   * KVM

   If using make menuconfig select the following to build the vfio_ap module::
     -> Device Drivers
        -> IOMMU Hardware Support
           select S390 AP IOMMU Support
        -> VFIO Non-Privileged userspace driver framework
           -> Mediated device driver framework
              -> VFIO driver for Mediated devices
     -> I/O subsystem
        -> VFIO support for AP devices

2. Secure the AP queues to be used by the three guests so that the host can not
   access them. To secure the AP queues 05.0004, 05.0047, 05.00ab, 05.00ff,
   06.0004, 06.0047, 06.00ab, and 06.00ff for use by the vfio_ap device driver,
   the corresponding APQNs must be removed from the default queue drivers pool
   as follows::

      echo -5,-6 > /sys/bus/ap/apmask

      echo -4,-0x47,-0xab,-0xff > /sys/bus/ap/aqmask

   This will result in AP queues 05.0004, 05.0047, 05.00ab, 05.00ff, 06.0004,
   06.0047, 06.00ab, and 06.00ff getting bound to the vfio_ap device driver. The
   sysfs directory for the vfio_ap device driver will now contain symbolic links
   to the AP queue devices bound to it::

     /sys/bus/ap
     ... [drivers]
     ...... [vfio_ap]
     ......... [05.0004]
     ......... [05.0047]
     ......... [05.00ab]
     ......... [05.00ff]
     ......... [06.0004]
     ......... [06.0047]
     ......... [06.00ab]
     ......... [06.00ff]

   Keep in mind that only type 10 and newer adapters (i.e., CEX4 and later)
   can be bound to the vfio_ap device driver. The reason for this is to
   simplify the implementation by not needlessly complicating the design by
   supporting older devices that will go out of service in the relatively near
   future, and for which there are few older systems on which to test.

   The administrator, therefore, must take care to secure only AP queues that
   can be bound to the vfio_ap device driver. The device type for a given AP
   queue device can be read from the parent card's sysfs directory. For example,
   to see the hardware type of the queue 05.0004::

     cat /sys/bus/ap/devices/card05/hwtype

   The hwtype must be 10 or higher (CEX4 or newer) in order to be bound to the
   vfio_ap device driver.

3. Create the mediated devices needed to configure the AP matrixes for the
   three guests and to provide an interface to the vfio_ap driver for
   use by the guests::

     /sys/devices/vfio_ap/matrix/
     ... [mdev_supported_types]
     ...... [vfio_ap-passthrough] (passthrough mediated matrix device type)
     ......... create
     ......... [devices]

   To create the mediated devices for the three guests::

       uuidgen > create
       uuidgen > create
       uuidgen > create

   or

   ::

       echo $uuid1 > create
       echo $uuid2 > create
       echo $uuid3 > create

   This will create three mediated devices in the [devices] subdirectory named
   after the UUID used to create the mediated device. We'll call them $uuid1,
   $uuid2 and $uuid3 and this is the sysfs directory structure after creation::

     /sys/devices/vfio_ap/matrix/
     ... [mdev_supported_types]
     ...... [vfio_ap-passthrough]
     ......... [devices]
     ............ [$uuid1]
     ............... assign_adapter
     ............... assign_control_domain
     ............... assign_domain
     ............... matrix
     ............... unassign_adapter
     ............... unassign_control_domain
     ............... unassign_domain

     ............ [$uuid2]
     ............... assign_adapter
     ............... assign_control_domain
     ............... assign_domain
     ............... matrix
     ............... unassign_adapter
     ............... unassign_control_domain
     ............... unassign_domain

     ............ [$uuid3]
     ............... assign_adapter
     ............... assign_control_domain
     ............... assign_domain
     ............... matrix
     ............... unassign_adapter
     ............... unassign_control_domain
     ............... unassign_domain

4. The administrator now needs to configure the matrixes for the mediated
   devices $uuid1 (for Guest1), $uuid2 (for Guest2) and $uuid3 (for Guest3).

   This is how the matrix is configured for Guest1::

      echo 5 > assign_adapter
      echo 6 > assign_adapter
      echo 4 > assign_domain
      echo 0xab > assign_domain

   Control domains can similarly be assigned using the assign_control_domain
   sysfs file.

   If a mistake is made configuring an adapter, domain or control domain,
   you can use the ``unassign_xxx`` interfaces to unassign the adapter, domain or
   control domain.

   To display the matrix configuration for Guest1::

         cat matrix

   The output will display the APQNs in the format ``xx.yyyy``, where xx is
   the adapter number and yyyy is the domain number. The output for Guest1
   will look like this::

         05.0004
         05.00ab
         06.0004
         06.00ab

   This is how the matrix is configured for Guest2::

      echo 5 > assign_adapter
      echo 0x47 > assign_domain
      echo 0xff > assign_domain

   This is how the matrix is configured for Guest3::

      echo 6 > assign_adapter
      echo 0x47 > assign_domain
      echo 0xff > assign_domain

5. Start Guest1::

   /usr/bin/qemu-system-s390x ... -cpu host,ap=on,apqci=on,apft=on -device vfio-ap,sysfsdev=/sys/devices/vfio_ap/matrix/$uuid1 ...

7. Start Guest2::

   /usr/bin/qemu-system-s390x ... -cpu host,ap=on,apqci=on,apft=on -device vfio-ap,sysfsdev=/sys/devices/vfio_ap/matrix/$uuid2 ...

7. Start Guest3::

   /usr/bin/qemu-system-s390x ... -cpu host,ap=on,apqci=on,apft=on -device vfio-ap,sysfsdev=/sys/devices/vfio_ap/matrix/$uuid3 ...

When the guest is shut down, the mediated matrix devices may be removed.

Using our example again, to remove the mediated matrix device $uuid1::

   /sys/devices/vfio_ap/matrix/
   ... [mdev_supported_types]
   ...... [vfio_ap-passthrough]
   ......... [devices]
   ............ [$uuid1]
   ............... remove


   echo 1 > remove

This will remove all of the mdev matrix device's sysfs structures including
the mdev device itself. To recreate and reconfigure the mdev matrix device,
all of the steps starting with step 3 will have to be performed again. Note
that the remove will fail if a guest using the mdev is still running.

It is not necessary to remove an mdev matrix device, but one may want to
remove it if no guest will use it during the remaining lifetime of the linux
host. If the mdev matrix device is removed, one may want to also reconfigure
the pool of adapters and queues reserved for use by the default drivers.

Limitations
-----------

* The KVM/kernel interfaces do not provide a way to prevent restoring an APQN
  to the default drivers pool of a queue that is still assigned to a mediated
  device in use by a guest. It is incumbent upon the administrator to
  ensure there is no mediated device in use by a guest to which the APQN is
  assigned lest the host be given access to the private data of the AP queue
  device, such as a private key configured specifically for the guest.

* Dynamically assigning AP resources to or unassigning AP resources from a
  mediated matrix device - see `Configuring an AP matrix for a linux guest`_
  section above - while a running guest is using it is currently not supported.

* Live guest migration is not supported for guests using AP devices. If a guest
  is using AP devices, the vfio-ap device configured for the guest must be
  unplugged before migrating the guest (see `Hot unplug a vfio-ap device from a
  running guest`_ section above.)
