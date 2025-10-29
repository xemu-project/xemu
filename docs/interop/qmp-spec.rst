..
    Copyright (C) 2009-2016 Red Hat, Inc.

    This work is licensed under the terms of the GNU GPL, version 2 or
    later. See the COPYING file in the top-level directory.


===================================
QEMU Machine Protocol Specification
===================================

The QEMU Machine Protocol (QMP) is a JSON-based
protocol which is available for applications to operate QEMU at the
machine-level.  It is also in use by the QEMU Guest Agent (QGA), which
is available for host applications to interact with the guest
operating system. This page specifies the general format of
the protocol; details of the commands and data structures can
be found in the :doc:`qemu-qmp-ref` and the :doc:`qemu-ga-ref`.

.. contents::

Protocol Specification
======================

This section details the protocol format. For the purpose of this
document, "Server" is either QEMU or the QEMU Guest Agent, and
"Client" is any application communicating with it via QMP.

JSON data structures, when mentioned in this document, are always in the
following format:

    json-DATA-STRUCTURE-NAME

Where DATA-STRUCTURE-NAME is any valid JSON data structure, as defined
by the `JSON standard <http://www.ietf.org/rfc/rfc8259.txt>`_.

The server expects its input to be encoded in UTF-8, and sends its
output encoded in ASCII.

For convenience, json-object members mentioned in this document will
be in a certain order. However, in real protocol usage they can be in
ANY order, thus no particular order should be assumed. On the other
hand, use of json-array elements presumes that preserving order is
important unless specifically documented otherwise.  Repeating a key
within a json-object gives unpredictable results.

Also for convenience, the server will accept an extension of
``'single-quoted'`` strings in place of the usual ``"double-quoted"``
json-string, and both input forms of strings understand an additional
escape sequence of ``\'`` for a single quote. The server will only use
double quoting on output.

General Definitions
-------------------

All interactions transmitted by the Server are json-objects, always
terminating with CRLF.

All json-objects members are mandatory when not specified otherwise.

Server Greeting
---------------

Right when connected the Server will issue a greeting message, which signals
that the connection has been successfully established and that the Server is
ready for capabilities negotiation (for more information refer to section
`Capabilities Negotiation`_).

The greeting message format is:

::

  { "QMP": { "version": json-object, "capabilities": json-array } }

Where:

- The ``version`` member contains the Server's version information (the format
  is the same as for the query-version command).
- The ``capabilities`` member specifies the availability of features beyond the
  baseline specification; the order of elements in this array has no
  particular significance.

Capabilities
------------

Currently supported capabilities are:

``oob``
  the QMP server supports "out-of-band" (OOB) command
  execution, as described in section `Out-of-band execution`_.

Issuing Commands
----------------

The format for command execution is:

::

  { "execute": json-string, "arguments": json-object, "id": json-value }

or

::

  { "exec-oob": json-string, "arguments": json-object, "id": json-value }

Where:

- The ``execute`` or ``exec-oob`` member identifies the command to be
  executed by the server.  The latter requests out-of-band execution.
- The ``arguments`` member is used to pass any arguments required for the
  execution of the command, it is optional when no arguments are
  required. Each command documents what contents will be considered
  valid when handling the json-argument.
- The ``id`` member is a transaction identification associated with the
  command execution, it is optional and will be part of the response
  if provided.  The ``id`` member can be any json-value.  A json-number
  incremented for each successive command works fine.

The actual commands are documented in the :doc:`qemu-qmp-ref`.

Out-of-band execution
---------------------

The server normally reads, executes and responds to one command after
the other.  The client therefore receives command responses in issue
order.

With out-of-band execution enabled via `capabilities negotiation`_,
the server reads and queues commands as they arrive.  It executes
commands from the queue one after the other.  Commands executed
out-of-band jump the queue: the command get executed right away,
possibly overtaking prior in-band commands.  The client may therefore
receive such a command's response before responses from prior in-band
commands.

To be able to match responses back to their commands, the client needs
to pass ``id`` with out-of-band commands.  Passing it with all commands
is recommended for clients that accept capability ``oob``.

If the client sends in-band commands faster than the server can
execute them, the server will stop reading requests until the request
queue length is reduced to an acceptable range.

To ensure commands to be executed out-of-band get read and executed,
the client should have at most eight in-band commands in flight.

Only a few commands support out-of-band execution.  The ones that do
have ``"allow-oob": true`` in the output of ``query-qmp-schema``.

Commands Responses
------------------

There are two possible responses which the Server will issue as the result
of a command execution: success or error.

As long as the commands were issued with a proper ``id`` field, then the
same ``id`` field will be attached in the corresponding response message
so that requests and responses can match.  Clients should drop all the
responses that have an unknown ``id`` field.

Success
-------

The format of a success response is:

::

  { "return": json-value, "id": json-value }

Where:

- The ``return`` member contains the data returned by the command, which
  is defined on a per-command basis (usually a json-object or
  json-array of json-objects, but sometimes a json-number, json-string,
  or json-array of json-strings); it is an empty json-object if the
  command does not return data.
- The ``id`` member contains the transaction identification associated
  with the command execution if issued by the Client.

Error
-----

The format of an error response is:

::

  { "error": { "class": json-string, "desc": json-string }, "id": json-value }

Where:

- The ``class`` member contains the error class name (eg. ``"GenericError"``).
- The ``desc`` member is a human-readable error message. Clients should
  not attempt to parse this message.
- The ``id`` member contains the transaction identification associated with
  the command execution if issued by the Client.

NOTE: Some errors can occur before the Server is able to read the ``id`` member;
in these cases the ``id`` member will not be part of the error response, even
if provided by the client.

Asynchronous events
-------------------

As a result of state changes, the Server may send messages unilaterally
to the Client at any time, when not in the middle of any other
response. They are called "asynchronous events".

The format of asynchronous events is:

::

  { "event": json-string, "data": json-object,
    "timestamp": { "seconds": json-number, "microseconds": json-number } }

Where:

- The ``event`` member contains the event's name.
- The ``data`` member contains event specific data, which is defined in a
  per-event basis. It is optional.
- The ``timestamp`` member contains the exact time of when the event
  occurred in the Server. It is a fixed json-object with time in
  seconds and microseconds relative to the Unix Epoch (1 Jan 1970); if
  there is a failure to retrieve host time, both members of the
  timestamp will be set to -1.

The actual asynchronous events are documented in the :doc:`qemu-qmp-ref`.

Some events are rate-limited to at most one per second.  If additional
"similar" events arrive within one second, all but the last one are
dropped, and the last one is delayed.  "Similar" normally means same
event type.

Forcing the JSON parser into known-good state
---------------------------------------------

Incomplete or invalid input can leave the server's JSON parser in a
state where it can't parse additional commands.  To get it back into
known-good state, the client should provoke a lexical error.

The cleanest way to do that is sending an ASCII control character
other than ``\t`` (horizontal tab), ``\r`` (carriage return), or
``\n`` (new line).

Sadly, older versions of QEMU can fail to flag this as an error.  If a
client needs to deal with them, it should send a 0xFF byte.

QGA Synchronization
-------------------

When a client connects to QGA over a transport lacking proper
connection semantics such as virtio-serial, QGA may have read partial
input from a previous client.  The client needs to force QGA's parser
into known-good state using the previous section's technique.
Moreover, the client may receive output a previous client didn't read.
To help with skipping that output, QGA provides the
``guest-sync-delimited`` command.  Refer to its documentation for
details.


QMP Examples
============

This section provides some examples of real QMP usage, in all of them
``->`` marks text sent by the Client and ``<-`` marks replies by the Server.

.. admonition:: Example

  Server greeting

  .. code-block:: QMP

    <- { "QMP": {"version": {"qemu": {"micro": 0, "minor": 0, "major": 3},
         "package": "v3.0.0"}, "capabilities": ["oob"] } }

.. admonition:: Example

  Capabilities negotiation

  .. code-block:: QMP

    -> { "execute": "qmp_capabilities", "arguments": { "enable": ["oob"] } }
    <- { "return": {}}

.. admonition:: Example

  Simple 'stop' execution

  .. code-block:: QMP

    -> { "execute": "stop" }
    <- { "return": {} }

.. admonition:: Example

  KVM information

  .. code-block:: QMP

    -> { "execute": "query-kvm", "id": "example" }
    <- { "return": { "enabled": true, "present": true }, "id": "example"}

.. admonition:: Example

  Parsing error

  .. code-block:: QMP

    -> { "execute": }
    <- { "error": { "class": "GenericError", "desc": "JSON parse error, expecting value" } }

.. admonition:: Example

  Powerdown event

  .. code-block:: QMP

    <- { "timestamp": { "seconds": 1258551470, "microseconds": 802384 },
        "event": "POWERDOWN" }

.. admonition:: Example

  Out-of-band execution

  .. code-block:: QMP

    -> { "exec-oob": "migrate-pause", "id": 42 }
    <- { "id": 42,
         "error": { "class": "GenericError",
          "desc": "migrate-pause is currently only supported during postcopy-active state" } }


Capabilities Negotiation
========================

When a Client successfully establishes a connection, the Server is in
Capabilities Negotiation mode.

In this mode only the ``qmp_capabilities`` command is allowed to run; all
other commands will return the ``CommandNotFound`` error. Asynchronous
messages are not delivered either.

Clients should use the ``qmp_capabilities`` command to enable capabilities
advertised in the `Server Greeting`_ which they support.

When the ``qmp_capabilities`` command is issued, and if it does not return an
error, the Server enters Command mode where capabilities changes take
effect, all commands (except ``qmp_capabilities``) are allowed and asynchronous
messages are delivered.

Compatibility Considerations
============================

All protocol changes or new features which modify the protocol format in an
incompatible way are disabled by default and will be advertised by the
capabilities array (in the `Server Greeting`_). Thus, Clients can check
that array and enable the capabilities they support.

The QMP Server performs a type check on the arguments to a command.  It
generates an error if a value does not have the expected type for its
key, or if it does not understand a key that the Client included.  The
strictness of the Server catches wrong assumptions of Clients about
the Server's schema.  Clients can assume that, when such validation
errors occur, they will be reported before the command generated any
side effect.

However, Clients must not assume any particular:

- Length of json-arrays
- Size of json-objects; in particular, future versions of QEMU may add
  new keys and Clients should be able to ignore them
- Order of json-object members or json-array elements
- Amount of errors generated by a command, that is, new errors can be added
  to any existing command in newer versions of the Server

Any command or member name beginning with ``x-`` is deemed experimental,
and may be withdrawn or changed in an incompatible manner in a future
release.

Of course, the Server does guarantee to send valid JSON.  But apart from
this, a Client should be "conservative in what they send, and liberal in
what they accept".

Downstream extension of QMP
===========================

We recommend that downstream consumers of QEMU do *not* modify QMP.
Management tools should be able to support both upstream and downstream
versions of QMP without special logic, and downstream extensions are
inherently at odds with that.

However, we recognize that it is sometimes impossible for downstreams to
avoid modifying QMP.  Both upstream and downstream need to take care to
preserve long-term compatibility and interoperability.

To help with that, QMP reserves JSON object member names beginning with
``__`` (double underscore) for downstream use ("downstream names").  This
means upstream will never use any downstream names for its commands,
arguments, errors, asynchronous events, and so forth.

Any new names downstream wishes to add must begin with ``__``.  To
ensure compatibility with other downstreams, it is strongly
recommended that you prefix your downstream names with ``__RFQDN_`` where
RFQDN is a valid, reverse fully qualified domain name which you
control.  For example, a qemu-kvm specific monitor command would be:

::

    (qemu) __org.linux-kvm_enable_irqchip

Downstream must not change the `server greeting`_ other than
to offer additional capabilities.  But see below for why even that is
discouraged.

The section `Compatibility Considerations`_ applies to downstream as well
as to upstream, obviously.  It follows that downstream must behave
exactly like upstream for any input not containing members with
downstream names ("downstream members"), except it may add members
with downstream names to its output.

Thus, a client should not be able to distinguish downstream from
upstream as long as it doesn't send input with downstream members, and
properly ignores any downstream members in the output it receives.

Advice on downstream modifications:

1. Introducing new commands is okay.  If you want to extend an existing
   command, consider introducing a new one with the new behaviour
   instead.

2. Introducing new asynchronous messages is okay.  If you want to extend
   an existing message, consider adding a new one instead.

3. Introducing new errors for use in new commands is okay.  Adding new
   errors to existing commands counts as extension, so 1. applies.

4. New capabilities are strongly discouraged.  Capabilities are for
   evolving the basic protocol, and multiple diverging basic protocol
   dialects are most undesirable.
