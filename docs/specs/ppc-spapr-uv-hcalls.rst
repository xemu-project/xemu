===================================
Hypervisor calls and the Ultravisor
===================================

On PPC64 systems supporting Protected Execution Facility (PEF), system memory
can be placed in a secured region where only an ultravisor running in firmware
can provide access to. pSeries guests on such systems can communicate with
the ultravisor (via ultracalls) to switch to a secure virtual machine (SVM) mode
where the guest's memory is relocated to this secured region, making its memory
inaccessible to normal processes/guests running on the host.

The various ultracalls/hypercalls relating to SVM mode are currently only
documented internally, but are planned for direct inclusion into the Linux on
Power Architecture Reference document ([LoPAR]_). An internal ACR has been filed
to reserve a hypercall number range specific to this use case to avoid any
future conflicts with the IBM internally maintained Power Architecture Platform
Reference (PAPR+) documentation specification. This document summarizes some of
these details as they relate to QEMU.

Hypercalls needed by the ultravisor
===================================

Switching to SVM mode involves a number of hcalls issued by the ultravisor to
the hypervisor to orchestrate the movement of guest memory to secure memory and
various other aspects of the SVM mode. Numbers are assigned for these hcalls
within the reserved range ``0xEF00-0xEF80``. The below documents the hcalls
relevant to QEMU.

``H_TPM_COMM`` (``0xef10``)
---------------------------

SVM file systems are encrypted using a symmetric key. This key is then
wrapped/encrypted using the public key of a trusted system which has the private
key stored in the system's TPM. An Ultravisor will use this hcall to
unwrap/unseal the symmetric key using the system's TPM device or a TPM Resource
Manager associated with the device.

The Ultravisor sets up a separate session key with the TPM in advance during
host system boot. All sensitive in and out values will be encrypted using the
session key. Though the hypervisor will see the in and out buffers in raw form,
any sensitive contents will generally be encrypted using this session key.

Arguments:

  ``r3``: ``H_TPM_COMM`` (``0xef10``)

  ``r4``: ``TPM`` operation, one of:

    ``TPM_COMM_OP_EXECUTE`` (``0x1``): send a request to a TPM and receive a
    response, opening a new TPM session if one has not already been opened.

    ``TPM_COMM_OP_CLOSE_SESSION`` (``0x2``): close the existing TPM session, if
    any.

  ``r5``: ``in_buffer``, guest physical address of buffer containing the
  request. Caller may use the same address for both request and response.

  ``r6``: ``in_size``, size of the in buffer. Must be less than or equal to
  4 KB.

  ``r7``: ``out_buffer``, guest physical address of buffer to store the
  response. Caller may use the same address for both request and response.

  ``r8``: ``out_size``, size of the out buffer. Must be at least 4 KB, as this
  is the maximum request/response size supported by most TPM implementations,
  including the TPM Resource Manager in the linux kernel.

Return values:

  ``r3``: one of the following values:

    ``H_Success``: request processed successfully.

    ``H_PARAMETER``: invalid TPM operation.

    ``H_P2``: ``in_buffer`` is invalid.

    ``H_P3``: ``in_size`` is invalid.

    ``H_P4``: ``out_buffer`` is invalid.

    ``H_P5``: ``out_size`` is invalid.

    ``H_RESOURCE``: problem communicating with TPM.

    ``H_FUNCTION``: TPM access is not currently allowed/configured.

    ``r4``: For ``TPM_COMM_OP_EXECUTE``, the size of the response will be stored
    here upon success.
