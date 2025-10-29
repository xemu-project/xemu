/*
 * QEMU crypto TLS anonymous credential support
 *
 * Copyright (c) 2015 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef QCRYPTO_TLSCREDSANON_H
#define QCRYPTO_TLSCREDSANON_H

#include "crypto/tlscreds.h"
#include "qom/object.h"

#define TYPE_QCRYPTO_TLS_CREDS_ANON "tls-creds-anon"
typedef struct QCryptoTLSCredsAnon QCryptoTLSCredsAnon;
DECLARE_INSTANCE_CHECKER(QCryptoTLSCredsAnon, QCRYPTO_TLS_CREDS_ANON,
                         TYPE_QCRYPTO_TLS_CREDS_ANON)


typedef struct QCryptoTLSCredsAnonClass QCryptoTLSCredsAnonClass;

/**
 * QCryptoTLSCredsAnon:
 *
 * The QCryptoTLSCredsAnon object provides a representation
 * of anonymous credentials used perform a TLS handshake.
 * This is primarily provided for backwards compatibility and
 * its use is discouraged as it has poor security characteristics
 * due to lacking MITM attack protection amongst other problems.
 *
 * This is a user creatable object, which can be instantiated
 * via object_new_propv():
 *
 * <example>
 *   <title>Creating anonymous TLS credential objects in code</title>
 *   <programlisting>
 *   Object *obj;
 *   Error *err = NULL;
 *   obj = object_new_propv(TYPE_QCRYPTO_TLS_CREDS_ANON,
 *                          "tlscreds0",
 *                          &err,
 *                          "endpoint", "server",
 *                          "dir", "/path/x509/cert/dir",
 *                          "verify-peer", "yes",
 *                          NULL);
 *   </programlisting>
 * </example>
 *
 * Or via QMP:
 *
 * <example>
 *   <title>Creating anonymous TLS credential objects via QMP</title>
 *   <programlisting>
 *    {
 *       "execute": "object-add", "arguments": {
 *          "id": "tlscreds0",
 *          "qom-type": "tls-creds-anon",
 *          "props": {
 *             "endpoint": "server",
 *             "dir": "/path/to/x509/cert/dir",
 *             "verify-peer": false
 *          }
 *       }
 *    }
 *   </programlisting>
 * </example>
 *
 *
 * Or via the CLI:
 *
 * <example>
 *   <title>Creating anonymous TLS credential objects via CLI</title>
 *   <programlisting>
 *  qemu-system-x86_64 -object tls-creds-anon,id=tlscreds0,\
 *          endpoint=server,verify-peer=off,\
 *          dir=/path/to/x509/certdir/
 *   </programlisting>
 * </example>
 *
 */

struct QCryptoTLSCredsAnonClass {
    QCryptoTLSCredsClass parent_class;
};


#endif /* QCRYPTO_TLSCREDSANON_H */
