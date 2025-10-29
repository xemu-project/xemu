/*
 * QEMU crypto secret support
 *
 * Copyright 2020 Yandex N.V.
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

#ifndef QCRYPTO_SECRET_KEYRING_H
#define QCRYPTO_SECRET_KEYRING_H

#include "qapi/qapi-types-crypto.h"
#include "qom/object.h"
#include "crypto/secret_common.h"

#define TYPE_QCRYPTO_SECRET_KEYRING "secret_keyring"
OBJECT_DECLARE_SIMPLE_TYPE(QCryptoSecretKeyring,
                           QCRYPTO_SECRET_KEYRING)


struct QCryptoSecretKeyring {
    QCryptoSecretCommon parent;
    int32_t serial;
};



#endif /* QCRYPTO_SECRET_KEYRING_H */
