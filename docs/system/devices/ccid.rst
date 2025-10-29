Chip Card Interface Device (CCID)
=================================

USB CCID device
---------------
The USB CCID device is a USB device implementing the CCID specification, which
lets one connect smart card readers that implement the same spec. For more
information see the specification::

  Universal Serial Bus
  Device Class: Smart Card
  CCID
  Specification for
  Integrated Circuit(s) Cards Interface Devices
  Revision 1.1
  April 22rd, 2005

Smartcards are used for authentication, single sign on, decryption in
public/private schemes and digital signatures. A smartcard reader on the client
cannot be used on a guest with simple usb passthrough since it will then not be
available on the client, possibly locking the computer when it is "removed". On
the other hand this device can let you use the smartcard on both the client and
the guest machine. It is also possible to have a completely virtual smart card
reader and smart card (i.e. not backed by a physical device) using this device.

Building
--------
The cryptographic functions and access to the physical card is done via the
libcacard library, whose development package must be installed prior to
building QEMU:

In redhat/fedora::

  yum install libcacard-devel

In ubuntu::

  apt-get install libcacard-dev

Configuring and building::

  ./configure --enable-smartcard && make

Using ccid-card-emulated with hardware
--------------------------------------
Assuming you have a working smartcard on the host with the current
user, using libcacard, QEMU acts as another client using ccid-card-emulated::

  qemu -usb -device usb-ccid -device ccid-card-emulated

Using ccid-card-emulated with certificates stored in files
----------------------------------------------------------
You must create the CA and card certificates. This is a one time process.
We use NSS certificates::

  mkdir fake-smartcard
  cd fake-smartcard
  certutil -N -d sql:$PWD
  certutil -S -d sql:$PWD -s "CN=Fake Smart Card CA" -x -t TC,TC,TC -n fake-smartcard-ca
  certutil -S -d sql:$PWD -t ,, -s "CN=John Doe" -n id-cert -c fake-smartcard-ca
  certutil -S -d sql:$PWD -t ,, -s "CN=John Doe (signing)" --nsCertType smime -n signing-cert -c fake-smartcard-ca
  certutil -S -d sql:$PWD -t ,, -s "CN=John Doe (encryption)" --nsCertType sslClient -n encryption-cert -c fake-smartcard-ca

Note: you must have exactly three certificates.

You can use the emulated card type with the certificates backend::

  qemu -usb -device usb-ccid -device ccid-card-emulated,backend=certificates,db=sql:$PWD,cert1=id-cert,cert2=signing-cert,cert3=encryption-cert

To use the certificates in the guest, export the CA certificate::

  certutil -L -r -d sql:$PWD -o fake-smartcard-ca.cer -n fake-smartcard-ca

and import it in the guest::

  certutil -A -d /etc/pki/nssdb -i fake-smartcard-ca.cer -t TC,TC,TC -n fake-smartcard-ca

In a Linux guest you can then use the CoolKey PKCS #11 module to access
the card::

  certutil -d /etc/pki/nssdb -L -h all

It will prompt you for the PIN (which is the password you assigned to the
certificate database early on), and then show you all three certificates
together with the manually imported CA cert::

  Certificate Nickname                        Trust Attributes
  fake-smartcard-ca                           CT,C,C
  John Doe:CAC ID Certificate                 u,u,u
  John Doe:CAC Email Signature Certificate    u,u,u
  John Doe:CAC Email Encryption Certificate   u,u,u

If this does not happen, CoolKey is not installed or not registered with
NSS. Registration can be done from Firefox or the command line::

  modutil -dbdir /etc/pki/nssdb -add "CAC Module" -libfile /usr/lib64/pkcs11/libcoolkeypk11.so
  modutil -dbdir /etc/pki/nssdb -list

Using ccid-card-passthru with client side hardware
--------------------------------------------------
On the host specify the ccid-card-passthru device with a suitable chardev::

  qemu -chardev socket,server=on,host=0.0.0.0,port=2001,id=ccid,wait=off \
       -usb -device usb-ccid -device ccid-card-passthru,chardev=ccid

On the client run vscclient, built when you built QEMU::

  vscclient <qemu-host> 2001

Using ccid-card-passthru with client side certificates
------------------------------------------------------
This case is not particularly useful, but you can use it to debug
your setup.

Follow instructions above, except run QEMU and vscclient as follows.

Run qemu as per above, and run vscclient from the "fake-smartcard"
directory as follows::

  qemu -chardev socket,server=on,host=0.0.0.0,port=2001,id=ccid,wait=off \
       -usb -device usb-ccid -device ccid-card-passthru,chardev=ccid
  vscclient -e "db=\"sql:$PWD\" use_hw=no soft=(,Test,CAC,,id-cert,signing-cert,encryption-cert)" <qemu-host> 2001


Passthrough protocol scenario
-----------------------------
This is a typical interchange of messages when using the passthru card device.
usb-ccid is a usb device. It defaults to an unattached usb device on startup.
usb-ccid expects a chardev and expects the protocol defined in
cac_card/vscard_common.h to be passed over that.
The usb-ccid device can be in one of three modes:

* detached
* attached with no card
* attached with card

A typical interchange is (the arrow shows who started each exchange, it can be client
originated or guest originated)::

  client event        |    vscclient           |    passthru    |    usb-ccid  |  guest event
  ------------------------------------------------------------------------------------------------
                      |    VSC_Init            |                |              |
                      |    VSC_ReaderAdd       |                |    attach    |
                      |                        |                |              |  sees new usb device.
    card inserted ->  |                        |                |              |
                      |    VSC_ATR             |   insert       |    insert    |  see new card
                      |                        |                |              |
                      |    VSC_APDU            |   VSC_APDU     |              | <- guest sends APDU
  client <-> physical |                        |                |              |
   card APDU exchange |                        |                |              |
   client response -> |    VSC_APDU            |   VSC_APDU     |              |  receive APDU response
                                                      ...
                                      [APDU<->APDU repeats several times]
                                                      ...
     card removed  -> |                        |                |              |
                      |    VSC_CardRemove      |   remove       |   remove     |   card removed
                                                      ...
                                      [(card insert, apdu's, card remove) repeat]
                                                      ...
    kill/quit         |                        |                |              |
      vscclient       |                        |                |              |
                      |    VSC_ReaderRemove    |                |   detach     |
                      |                        |                |              |   usb device removed.

libcacard
---------
Both ccid-card-emulated and vscclient use libcacard as the card emulator.
libcacard implements a completely virtual CAC (DoD standard for smart
cards) compliant card and uses NSS to retrieve certificates and do
any encryption. The backend can then be a real reader and card, or
certificates stored in files.
