.. _Deprecated features:

Deprecated features
===================

In general features are intended to be supported indefinitely once
introduced into QEMU. In the event that a feature needs to be removed,
it will be listed in this section. The feature will remain functional for the
release in which it was deprecated and one further release. After these two
releases, the feature is liable to be removed. Deprecated features may also
generate warnings on the console when QEMU starts up, or if activated via a
monitor command, however, this is not a mandatory requirement.

Prior to the 2.10.0 release there was no official policy on how
long features would be deprecated prior to their removal, nor
any documented list of which features were deprecated. Thus
any features deprecated prior to 2.10.0 will be treated as if
they were first deprecated in the 2.10.0 release.

What follows is a list of all features currently marked as
deprecated.

System emulator command line arguments
--------------------------------------

``QEMU_AUDIO_`` environment variables and ``-audio-help`` (since 4.0)
'''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

The ``-audiodev`` argument is now the preferred way to specify audio
backend settings instead of environment variables.  To ease migration to
the new format, the ``-audiodev-help`` option can be used to convert
the current values of the environment variables to ``-audiodev`` options.

Creating sound card devices and vnc without ``audiodev=`` property (since 4.2)
''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

When not using the deprecated legacy audio config, each sound card
should specify an ``audiodev=`` property.  Additionally, when using
vnc, you should specify an ``audiodev=`` property if you plan to
transmit audio through the VNC protocol.

``-chardev`` backend aliases ``tty`` and ``parport`` (since 6.0)
''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

``tty`` and ``parport`` are aliases that will be removed. Instead, the
actual backend names ``serial`` and ``parallel`` should be used.

Short-form boolean options (since 6.0)
''''''''''''''''''''''''''''''''''''''

Boolean options such as ``share=on``/``share=off`` could be written
in short form as ``share`` and ``noshare``.  This is now deprecated
and will cause a warning.

``delay`` option for socket character devices (since 6.0)
'''''''''''''''''''''''''''''''''''''''''''''''''''''''''

The replacement for the ``nodelay`` short-form boolean option is ``nodelay=on``
rather than ``delay=off``.

Userspace local APIC with KVM (x86, since 6.0)
''''''''''''''''''''''''''''''''''''''''''''''

Using ``-M kernel-irqchip=off`` with x86 machine types that include a local
APIC is deprecated.  The ``split`` setting is supported, as is using
``-M kernel-irqchip=off`` with the ISA PC machine type.

hexadecimal sizes with scaling multipliers (since 6.0)
''''''''''''''''''''''''''''''''''''''''''''''''''''''

Input parameters that take a size value should only use a size suffix
(such as 'k' or 'M') when the base is written in decimal, and not when
the value is hexadecimal.  That is, '0x20M' is deprecated, and should
be written either as '32M' or as '0x2000000'.

``-spice password=string`` (since 6.0)
''''''''''''''''''''''''''''''''''''''

This option is insecure because the SPICE password remains visible in
the process listing. This is replaced by the new ``password-secret``
option which lets the password be securely provided on the command
line using a ``secret`` object instance.

``-smp`` ("parameter=0" SMP configurations) (since 6.2)
'''''''''''''''''''''''''''''''''''''''''''''''''''''''

Specified CPU topology parameters must be greater than zero.

In the SMP configuration, users should either provide a CPU topology
parameter with a reasonable value (greater than zero) or just omit it
and QEMU will compute the missing value.

However, historically it was implicitly allowed for users to provide
a parameter with zero value, which is meaningless and could also possibly
cause unexpected results in the -smp parsing. So support for this kind of
configurations (e.g. -smp 8,sockets=0) is deprecated since 6.2 and will
be removed in the near future, users have to ensure that all the topology
members described with -smp are greater than zero.

Plugin argument passing through ``arg=<string>`` (since 6.1)
''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

Passing TCG plugins arguments through ``arg=`` is redundant is makes the
command-line less readable, especially when the argument itself consist of a
name and a value, e.g. ``-plugin plugin_name,arg="arg_name=arg_value"``.
Therefore, the usage of ``arg`` is redundant. Single-word arguments are treated
as short-form boolean values, and passed to plugins as ``arg_name=on``.
However, short-form booleans are deprecated and full explicit ``arg_name=on``
form is preferred.

``-drive if=none`` for the sifive_u OTP device (since 6.2)
''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

Using ``-drive if=none`` to configure the OTP device of the sifive_u
RISC-V machine is deprecated. Use ``-drive if=pflash`` instead.


QEMU Machine Protocol (QMP) commands
------------------------------------

``blockdev-open-tray``, ``blockdev-close-tray`` argument ``device`` (since 2.8)
'''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

Use argument ``id`` instead.

``eject`` argument ``device`` (since 2.8)
'''''''''''''''''''''''''''''''''''''''''

Use argument ``id`` instead.

``blockdev-change-medium`` argument ``device`` (since 2.8)
''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

Use argument ``id`` instead.

``block_set_io_throttle`` argument ``device`` (since 2.8)
'''''''''''''''''''''''''''''''''''''''''''''''''''''''''

Use argument ``id`` instead.

``blockdev-add`` empty string argument ``backing`` (since 2.10)
'''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

Use argument value ``null`` instead.

``block-commit`` arguments ``base`` and ``top`` (since 3.1)
'''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

Use arguments ``base-node`` and ``top-node`` instead.

``nbd-server-add`` and ``nbd-server-remove`` (since 5.2)
''''''''''''''''''''''''''''''''''''''''''''''''''''''''

Use the more generic commands ``block-export-add`` and ``block-export-del``
instead.  As part of this deprecation, where ``nbd-server-add`` used a
single ``bitmap``, the new ``block-export-add`` uses a list of ``bitmaps``.

``query-qmp-schema`` return value member ``values`` (since 6.2)
'''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

Member ``values`` in return value elements with meta-type ``enum`` is
deprecated.  Use ``members`` instead.

``drive-backup`` (since 6.2)
''''''''''''''''''''''''''''

Use ``blockdev-backup`` in combination with ``blockdev-add`` instead.
This change primarily separates the creation/opening process of the backup
target with explicit, separate steps. ``blockdev-backup`` uses mostly the
same arguments as ``drive-backup``, except the ``format`` and ``mode``
options are removed in favor of using explicit ``blockdev-create`` and
``blockdev-add`` calls. See :doc:`/interop/live-block-operations` for
details.

Incorrectly typed ``device_add`` arguments (since 6.2)
''''''''''''''''''''''''''''''''''''''''''''''''''''''

Due to shortcomings in the internal implementation of ``device_add``, QEMU
incorrectly accepts certain invalid arguments: Any object or list arguments are
silently ignored. Other argument types are not checked, but an implicit
conversion happens, so that e.g. string values can be assigned to integer
device properties or vice versa.

This is a bug in QEMU that will be fixed in the future so that previously
accepted incorrect commands will return an error. Users should make sure that
all arguments passed to ``device_add`` are consistent with the documented
property types.

``query-sgx`` return value member ``section-size`` (since 7.0)
''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

Member ``section-size`` in return value elements with meta-type ``uint64`` is
deprecated.  Use ``sections`` instead.


``query-sgx-capabilities`` return value member ``section-size`` (since 7.0)
'''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

Member ``section-size`` in return value elements with meta-type ``uint64`` is
deprecated.  Use ``sections`` instead.

System accelerators
-------------------

MIPS ``Trap-and-Emul`` KVM support (since 6.0)
''''''''''''''''''''''''''''''''''''''''''''''

The MIPS ``Trap-and-Emul`` KVM host and guest support has been removed
from Linux upstream kernel, declare it deprecated.

Host Architectures
------------------

BE MIPS (since 7.2)
'''''''''''''''''''

As Debian 10 ("Buster") moved into LTS the big endian 32 bit version of
MIPS moved out of support making it hard to maintain our
cross-compilation CI tests of the architecture. As we no longer have
CI coverage support may bitrot away before the deprecation process
completes. The little endian variants of MIPS (both 32 and 64 bit) are
still a supported host architecture.

QEMU API (QAPI) events
----------------------

``MEM_UNPLUG_ERROR`` (since 6.2)
''''''''''''''''''''''''''''''''''''''''''''''''''''''''

Use the more generic event ``DEVICE_UNPLUG_GUEST_ERROR`` instead.


System emulator machines
------------------------

Arm ``virt`` machine ``dtb-kaslr-seed`` property (since 7.1)
''''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

The ``dtb-kaslr-seed`` property on the ``virt`` board has been
deprecated; use the new name ``dtb-randomness`` instead. The new name
better reflects the way this property affects all random data within
the device tree blob, not just the ``kaslr-seed`` node.

``pc-i440fx-1.4`` up to ``pc-i440fx-1.7`` (since 7.0)
'''''''''''''''''''''''''''''''''''''''''''''''''''''

These old machine types are quite neglected nowadays and thus might have
various pitfalls with regards to live migration. Use a newer machine type
instead.


Backend options
---------------

Using non-persistent backing file with pmem=on (since 6.1)
''''''''''''''''''''''''''''''''''''''''''''''''''''''''''

This option is used when ``memory-backend-file`` is consumed by emulated NVDIMM
device. However enabling ``memory-backend-file.pmem`` option, when backing file
is (a) not DAX capable or (b) not on a filesystem that support direct mapping
of persistent memory, is not safe and may lead to data loss or corruption in case
of host crash.
Options are:

    - modify VM configuration to set ``pmem=off`` to continue using fake NVDIMM
      (without persistence guaranties) with backing file on non DAX storage
    - move backing file to NVDIMM storage and keep ``pmem=on``
      (to have NVDIMM with persistence guaranties).

Device options
--------------

Emulated device options
'''''''''''''''''''''''

``-device virtio-blk,scsi=on|off`` (since 5.0)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The virtio-blk SCSI passthrough feature is a legacy VIRTIO feature.  VIRTIO 1.0
and later do not support it because the virtio-scsi device was introduced for
full SCSI support.  Use virtio-scsi instead when SCSI passthrough is required.

Note this also applies to ``-device virtio-blk-pci,scsi=on|off``, which is an
alias.

``-device sga`` (since 6.2)
^^^^^^^^^^^^^^^^^^^^^^^^^^^

The ``sga`` device loads an option ROM for x86 targets which enables
SeaBIOS to send messages to the serial console. SeaBIOS 1.11.0 onwards
contains native support for this feature and thus use of the option
ROM approach is obsolete. The native SeaBIOS support can be activated
by using ``-machine graphics=off``.

``-device nvme-ns,eui64-default=on|off`` (since 7.1)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In QEMU versions 6.1, 6.2 and 7.0, the ``nvme-ns`` generates an EUI-64
identifier that is not globally unique. If an EUI-64 identifier is required, the
user must set it explicitly using the ``nvme-ns`` device parameter ``eui64``.

``-device nvme,use-intel-id=on|off`` (since 7.1)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The ``nvme`` device originally used a PCI Vendor/Device Identifier combination
from Intel that was not properly allocated. Since version 5.2, the controller
has used a properly allocated identifier. Deprecate the ``use-intel-id``
machine compatibility parameter.


Block device options
''''''''''''''''''''

``"backing": ""`` (since 2.12)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In order to prevent QEMU from automatically opening an image's backing
chain, use ``"backing": null`` instead.

``rbd`` keyvalue pair encoded filenames: ``""`` (since 3.1)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Options for ``rbd`` should be specified according to its runtime options,
like other block drivers.  Legacy parsing of keyvalue pair encoded
filenames is useful to open images with the old format for backing files;
These image files should be updated to use the current format.

Example of legacy encoding::

  json:{"file.driver":"rbd", "file.filename":"rbd:rbd/name"}

The above, converted to the current supported format::

  json:{"file.driver":"rbd", "file.pool":"rbd", "file.image":"name"}

Backwards compatibility
-----------------------

Runnability guarantee of CPU models (since 4.1)
'''''''''''''''''''''''''''''''''''''''''''''''

Previous versions of QEMU never changed existing CPU models in
ways that introduced additional host software or hardware
requirements to the VM.  This allowed management software to
safely change the machine type of an existing VM without
introducing new requirements ("runnability guarantee").  This
prevented CPU models from being updated to include CPU
vulnerability mitigations, leaving guests vulnerable in the
default configuration.

The CPU model runnability guarantee won't apply anymore to
existing CPU models.  Management software that needs runnability
guarantees must resolve the CPU model aliases using the
``alias-of`` field returned by the ``query-cpu-definitions`` QMP
command.

While those guarantees are kept, the return value of
``query-cpu-definitions`` will have existing CPU model aliases
point to a version that doesn't break runnability guarantees
(specifically, version 1 of those CPU models).  In future QEMU
versions, aliases will point to newer CPU model versions
depending on the machine type, so management software must
resolve CPU model aliases before starting a virtual machine.

Tools
-----

virtiofsd
'''''''''

There is a new Rust implementation of ``virtiofsd`` at
``https://gitlab.com/virtio-fs/virtiofsd``;
since this is now marked stable, new development should be done on that
rather than the existing C version in the QEMU tree.
The C version will still accept fixes and patches that
are already in development for the moment, but will eventually
be deleted from this tree.
New deployments should use the Rust version, and existing systems
should consider moving to it.  The command line and feature set
is very close and moving should be simple.


QEMU guest agent
----------------

``--blacklist`` command line option (since 7.2)
'''''''''''''''''''''''''''''''''''''''''''''''

``--blacklist`` has been replaced by ``--block-rpcs`` (which is a better
wording for what this option does). The short form ``-b`` still stays
the same and thus is the preferred way for scripts that should run with
both, older and future versions of QEMU.

``blacklist`` config file option (since 7.2)
''''''''''''''''''''''''''''''''''''''''''''

The ``blacklist`` config file option has been renamed to ``block-rpcs``
(to be in sync with the renaming of the corresponding command line
option).
