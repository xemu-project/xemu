=============================
sPAPR Dynamic Reconfiguration
=============================

sPAPR or pSeries guests make use of a facility called dynamic reconfiguration
to handle hot plugging of dynamic "physical" resources like PCI cards, or
"logical"/para-virtual resources like memory, CPUs, and "physical"
host-bridges, which are generally managed by the host/hypervisor and provided
to guests as virtualized resources. The specifics of dynamic reconfiguration
are documented extensively in section 13 of the Linux on Power Architecture
Reference document ([LoPAR]_). This document provides a summary of that
information as it applies to the implementation within QEMU.

Dynamic-reconfiguration Connectors
==================================

To manage hot plug/unplug of these resources, a firmware abstraction known as
a Dynamic Resource Connector (DRC) is used to assign a particular dynamic
resource to the guest, and provide an interface for the guest to manage
configuration/removal of the resource associated with it.

Device tree description of DRCs
===============================

A set of four Open Firmware device tree array properties are used to describe
the name/index/power-domain/type of each DRC allocated to a guest at
boot time. There may be multiple sets of these arrays, rooted at different
paths in the device tree depending on the type of resource the DRCs manage.

In some cases, the DRCs themselves may be provided by a dynamic resource,
such as the DRCs managing PCI slots on a hot plugged PHB. In this case the
arrays would be fetched as part of the device tree retrieval interfaces
for hot plugged resources described under :ref:`guest-host-interface`.

The array properties are described below. Each entry/element in an array
describes the DRC identified by the element in the corresponding position
of ``ibm,drc-indexes``:

``ibm,drc-names``
-----------------

  First 4-bytes: big-endian (BE) encoded integer denoting the number of entries.

  Each entry: a NULL-terminated ``<name>`` string encoded as a byte array.

    ``<name>`` values for logical/virtual resources are defined in the Linux on
    Power Architecture Reference ([LoPAR]_) section 13.5.2.4, and basically
    consist of the type of the resource followed by a space and a numerical
    value that's unique across resources of that type.

    ``<name>`` values for "physical" resources such as PCI or VIO devices are
    defined as being "location codes", which are the "location labels" of each
    encapsulating device, starting from the chassis down to the individual slot
    for the device, concatenated by a hyphen. This provides a mapping of
    resources to a physical location in a chassis for debugging purposes. For
    QEMU, this mapping is less important, so we assign a location code that
    conforms to naming specifications, but is simply a location label for the
    slot by itself to simplify the implementation. The naming convention for
    location labels is documented in detail in the [LoPAR]_ section 12.3.1.5,
    and in our case amounts to using ``C<n>`` for PCI/VIO device slots, where
    ``<n>`` is unique across all PCI/VIO device slots.

``ibm,drc-indexes``
-------------------

  First 4-bytes: BE-encoded integer denoting the number of entries.

  Each 4-byte entry: BE-encoded ``<index>`` integer that is unique across all
  DRCs in the machine.

    ``<index>`` is arbitrary, but in the case of QEMU we try to maintain the
    convention used to assign them to pSeries guests on pHyp (the hypervisor
    portion of PowerVM):

      ``bit[31:28]``: integer encoding of ``<type>``, where ``<type>`` is:

        ``1`` for CPU resource.

        ``2`` for PHB resource.

        ``3`` for VIO resource.

        ``4`` for PCI resource.

        ``8`` for memory resource.

      ``bit[27:0]``: integer encoding of ``<id>``, where ``<id>`` is unique
      across all resources of specified type.

``ibm,drc-power-domains``
-------------------------

  First 4-bytes: BE-encoded integer denoting the number of entries.

  Each 4-byte entry: 32-bit, BE-encoded ``<index>`` integer that specifies the
  power domain the resource will be assigned to. In the case of QEMU we
  associated all resources with a "live insertion" domain, where the power is
  assumed to be managed automatically. The integer value for this domain is a
  special value of ``-1``.


``ibm,drc-types``
-----------------

  First 4-bytes: BE-encoded integer denoting the number of entries.

  Each entry: a NULL-terminated ``<type>`` string encoded as a byte array.
  ``<type>`` is assigned as follows:

    "CPU" for a CPU.

    "PHB" for a physical host-bridge.

    "SLOT" for a VIO slot.

    "28" for a PCI slot.

    "MEM" for memory resource.

.. _guest-host-interface:

Guest->Host interface to manage dynamic resources
=================================================

Each DRC is given a globally unique DRC index, and resources associated with a
particular DRC are configured/managed by the guest via a number of RTAS calls
which reference individual DRCs based on the DRC index. This can be considered
the guest->host interface.

``rtas-set-power-level``
------------------------

Set the power level for a specified power domain.

  ``arg[0]``: integer identifying power domain.

  ``arg[1]``: new power level for the domain, ``0-100``.

  ``output[0]``: status, ``0`` on success.

  ``output[1]``: power level after command.

``rtas-get-power-level``
------------------------

Get the power level for a specified power domain.

  ``arg[0]``: integer identifying power domain.

  ``output[0]``: status, ``0`` on success.

  ``output[1]``: current power level.

``rtas-set-indicator``
----------------------

Set the state of an indicator or sensor.

  ``arg[0]``: integer identifying sensor/indicator type.

  ``arg[1]``: index of sensor, for DR-related sensors this is generally the DRC
  index.

  ``arg[2]``: desired sensor value.

  ``output[0]``: status, ``0`` on success.

For the purpose of this document we focus on the indicator/sensor types
associated with a DRC. The types are:

* ``9001``: ``isolation-state``, controls/indicates whether a device has been
  made accessible to a guest. Supported sensor values:

    ``0``: ``isolate``, device is made inaccessible by guest OS.

    ``1``: ``unisolate``, device is made available to guest OS.

* ``9002``: ``dr-indicator``, controls "visual" indicator associated with
  device. Supported sensor values:

    ``0``: ``inactive``, resource may be safely removed.

    ``1``: ``active``, resource is in use and cannot be safely removed.

    ``2``: ``identify``, used to visually identify slot for interactive hot plug.

    ``3``: ``action``, in most cases, used in the same manner as identify.

* ``9003``: ``allocation-state``, generally only used for "logical" DR resources
  to request the allocation/deallocation of a resource prior to acquiring it via
  ``isolation-state->unisolate``, or after releasing it via
  ``isolation-state->isolate``, respectively. For "physical" DR (like PCI
  hot plug/unplug) the pre-allocation of the resource is implied and this sensor
  is unused. Supported sensor values:

    ``0``: ``unusable``, tell firmware/system the resource can be
    unallocated/reclaimed and added back to the system resource pool.

    ``1``: ``usable``, request the resource be allocated/reserved for use by
    guest OS.

    ``2``: ``exchange``, used to allocate a spare resource to use for fail-over
    in certain situations. Unused in QEMU.

    ``3``: ``recover``, used to reclaim a previously allocated resource that's
    not currently allocated to the guest OS. Unused in QEMU.

``rtas-get-sensor-state:``
--------------------------

Used to read an indicator or sensor value.

  ``arg[0]``: integer identifying sensor/indicator type.

  ``arg[1]``: index of sensor, for DR-related sensors this is generally the DRC
  index

  ``output[0]``: status, 0 on success

For DR-related operations, the only noteworthy sensor is ``dr-entity-sense``,
which has a type value of ``9003``, as ``allocation-state`` does in the case of
``rtas-set-indicator``. The semantics/encodings of the sensor values are
distinct however.

Supported sensor values for ``dr-entity-sense`` (``9003``) sensor:

  ``0``: empty.

    For physical resources: DRC/slot is empty.

    For logical resources: unused.

  ``1``: present.

    For physical resources: DRC/slot is populated with a device/resource.

    For logical resources: resource has been allocated to the DRC.

  ``2``: unusable.

    For physical resources: unused.

    For logical resources: DRC has no resource allocated to it.

  ``3``: exchange.

    For physical resources: unused.

    For logical resources: resource available for exchange (see
    ``allocation-state`` sensor semantics above).

  ``4``: recovery.

    For physical resources: unused.

    For logical resources: resource available for recovery (see
    ``allocation-state`` sensor semantics above).

``rtas-ibm-configure-connector``
--------------------------------

Used to fetch an OpenFirmware device tree description of the resource associated
with a particular DRC.

  ``arg[0]``: guest physical address of 4096-byte work area buffer.

  ``arg[1]``: 0, or address of additional 4096-byte work area buffer; only
  non-zero if a prior RTAS response indicated a need for additional memory.

  ``output[0]``: status:

    ``0``: completed transmittal of device tree node.

    ``1``: instruct guest to prepare for next device tree sibling node.

    ``2``: instruct guest to prepare for next device tree child node.

    ``3``: instruct guest to prepare for next device tree property.

    ``4``: instruct guest to ascend to parent device tree node.

    ``5``: instruct guest to provide additional work-area buffer via ``arg[1]``.

    ``990x``: instruct guest that operation took too long and to try again
    later.

The DRC index is encoded in the first 4-bytes of the first work area buffer.
Work area (``wa``) layout, using 4-byte offsets:

  ``wa[0]``: DRC index of the DRC to fetch device tree nodes from.

  ``wa[1]``: ``0`` (hard-coded).

  ``wa[2]``:

    For next-sibling/next-child response:

      ``wa`` offset of null-terminated string denoting the new node's name.

    For next-property response:

      ``wa`` offset of null-terminated string denoting new property's name.

  ``wa[3]``: for next-property response (unused otherwise):

      Byte-length of new property's value.

  ``wa[4]``: for next-property response (unused otherwise):

      New property's value, encoded as an OFDT-compatible byte array.

Hot plug/unplug events
======================

For most DR operations, the hypervisor will issue host->guest add/remove events
using the EPOW/check-exception notification framework, where the host issues a
check-exception interrupt, then provides an RTAS event log via an
rtas-check-exception call issued by the guest in response. This framework is
documented by PAPR+ v2.7, and already use in by QEMU for generating powerdown
requests via EPOW events.

For DR, this framework has been extended to include hotplug events, which were
previously unneeded due to direct manipulation of DR-related guest userspace
tools by host-level management such as an HMC. This level of management is not
applicable to KVM on Power, hence the reason for extending the notification
framework to support hotplug events.

The format for these EPOW-signalled events is described below under
:ref:`hot-plug-unplug-event-structure`. Note that these events are not formally
part of the PAPR+ specification, and have been superseded by a newer format,
also described below under :ref:`hot-plug-unplug-event-structure`, and so are
now deemed a "legacy" format. The formats are similar, but the "modern" format
contains additional fields/flags, which are denoted for the purposes of this
documentation with ``#ifdef GUEST_SUPPORTS_MODERN`` guards.

QEMU should assume support only for "legacy" fields/flags unless the guest
advertises support for the "modern" format via
``ibm,client-architecture-support`` hcall by setting byte 5, bit 6 of it's
``ibm,architecture-vec-5`` option vector structure (as described by [LoPAR]_,
section B.5.2.3). As with "legacy" format events, "modern" format events are
surfaced to the guest via check-exception RTAS calls, but use a dedicated event
source to signal the guest. This event source is advertised to the guest by the
addition of a ``hot-plug-events`` node under ``/event-sources`` node of the
guest's device tree using the standard format described in [LoPAR]_,
section B.5.12.2.

.. _hot-plug-unplug-event-structure:

Hot plug/unplug event structure
===============================

The hot plug specific payload in QEMU is implemented as follows (with all values
encoded in big-endian format):

.. code-block:: c

   struct rtas_event_log_v6_hp {
   #define SECTION_ID_HOTPLUG              0x4850 /* HP */
       struct section_header {
           uint16_t section_id;            /* set to SECTION_ID_HOTPLUG */
           uint16_t section_length;        /* sizeof(rtas_event_log_v6_hp),
                                            * plus the length of the DRC name
                                            * if a DRC name identifier is
                                            * specified for hotplug_identifier
                                            */
           uint8_t section_version;        /* version 1 */
           uint8_t section_subtype;        /* unused */
           uint16_t creator_component_id;  /* unused */
       } hdr;
   #define RTAS_LOG_V6_HP_TYPE_CPU         1
   #define RTAS_LOG_V6_HP_TYPE_MEMORY      2
   #define RTAS_LOG_V6_HP_TYPE_SLOT        3
   #define RTAS_LOG_V6_HP_TYPE_PHB         4
   #define RTAS_LOG_V6_HP_TYPE_PCI         5
       uint8_t hotplug_type;               /* type of resource/device */
   #define RTAS_LOG_V6_HP_ACTION_ADD       1
   #define RTAS_LOG_V6_HP_ACTION_REMOVE    2
       uint8_t hotplug_action;             /* action (add/remove) */
   #define RTAS_LOG_V6_HP_ID_DRC_NAME          1
   #define RTAS_LOG_V6_HP_ID_DRC_INDEX         2
   #define RTAS_LOG_V6_HP_ID_DRC_COUNT         3
   #ifdef GUEST_SUPPORTS_MODERN
   #define RTAS_LOG_V6_HP_ID_DRC_COUNT_INDEXED 4
   #endif
       uint8_t hotplug_identifier;         /* type of the resource identifier,
                                            * which serves as the discriminator
                                            * for the 'drc' union field below
                                            */
   #ifdef GUEST_SUPPORTS_MODERN
       uint8_t capabilities;               /* capability flags, currently unused
                                            * by QEMU
                                            */
   #else
       uint8_t reserved;
   #endif
       union {
           uint32_t index;                 /* DRC index of resource to take action
                                            * on
                                            */
           uint32_t count;                 /* number of DR resources to take
                                            * action on (guest chooses which)
                                            */
   #ifdef GUEST_SUPPORTS_MODERN
           struct {
               uint32_t count;             /* number of DR resources to take
                                            * action on
                                            */
               uint32_t index;             /* DRC index of first resource to take
                                            * action on. guest will take action
                                            * on DRC index <index> through
                                            * DRC index <index + count - 1> in
                                            * sequential order
                                            */
           } count_indexed;
   #endif
           char name[1];                   /* string representing the name of the
                                            * DRC to take action on
                                            */
       } drc;
   } QEMU_PACKED;

``ibm,lrdr-capacity``
=====================

``ibm,lrdr-capacity`` is a property in the /rtas device tree node that
identifies the dynamic reconfiguration capabilities of the guest. It consists
of a triple consisting of ``<phys>``, ``<size>`` and ``<maxcpus>``.

  ``<phys>``, encoded in BE format represents the maximum address in bytes and
  hence the maximum memory that can be allocated to the guest.

  ``<size>``, encoded in BE format represents the size increments in which
  memory can be hot-plugged to the guest.

  ``<maxcpus>``, a BE-encoded integer, represents the maximum number of
  processors that the guest can have.

``pseries`` guests use this property to note the maximum allowed CPUs for the
guest.

``ibm,dynamic-reconfiguration-memory``
======================================

``ibm,dynamic-reconfiguration-memory`` is a device tree node that represents
dynamically reconfigurable logical memory blocks (LMB). This node is generated
only when the guest advertises the support for it via
``ibm,client-architecture-support`` call. Memory that is not dynamically
reconfigurable is represented by ``/memory`` nodes. The properties of this node
that are of interest to the sPAPR memory hotplug implementation in QEMU are
described here.

``ibm,lmb-size``
----------------

This 64-bit integer defines the size of each dynamically reconfigurable LMB.

``ibm,associativity-lookup-arrays``
-----------------------------------

This property defines a lookup array in which the NUMA associativity
information for each LMB can be found. It is a property encoded array
that begins with an integer M, the number of associativity lists followed
by an integer N, the number of entries per associativity list and terminated
by M associativity lists each of length N integers.

This property provides the same information as given by ``ibm,associativity``
property in a ``/memory`` node. Each assigned LMB has an index value between
0 and M-1 which is used as an index into this table to select which
associativity list to use for the LMB. This index value for each LMB is defined
in ``ibm,dynamic-memory`` property.

``ibm,dynamic-memory``
----------------------

This property describes the dynamically reconfigurable memory. It is a
property encoded array that has an integer N, the number of LMBs followed
by N LMB list entries.

Each LMB list entry consists of the following elements:

- Logical address of the start of the LMB encoded as a 64-bit integer. This
  corresponds to ``reg`` property in ``/memory`` node.
- DRC index of the LMB that corresponds to ``ibm,my-drc-index`` property
  in a ``/memory`` node.
- Four bytes reserved for expansion.
- Associativity list index for the LMB that is used as an index into
  ``ibm,associativity-lookup-arrays`` property described earlier. This is used
  to retrieve the right associativity list to be used for this LMB.
- A 32-bit flags word. The bit at bit position ``0x00000008`` defines whether
  the LMB is assigned to the partition as of boot time.

``ibm,dynamic-memory-v2``
-------------------------

This property describes the dynamically reconfigurable memory. This is
an alternate and newer way to describe dynamically reconfigurable memory.
It is a property encoded array that has an integer N (the number of
LMB set entries) followed by N LMB set entries. There is an LMB set entry
for each sequential group of LMBs that share common attributes.

Each LMB set entry consists of the following elements:

- Number of sequential LMBs in the entry represented by a 32-bit integer.
- Logical address of the first LMB in the set encoded as a 64-bit integer.
- DRC index of the first LMB in the set.
- Associativity list index that is used as an index into
  ``ibm,associativity-lookup-arrays`` property described earlier. This
  is used to retrieve the right associativity list to be used for all
  the LMBs in this set.
- A 32-bit flags word that applies to all the LMBs in the set.
