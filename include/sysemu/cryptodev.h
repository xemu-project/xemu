/*
 * QEMU Crypto Device Implementation
 *
 * Copyright (c) 2016 HUAWEI TECHNOLOGIES CO., LTD.
 *
 * Authors:
 *    Gonglei <arei.gonglei@huawei.com>
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
#ifndef CRYPTODEV_H
#define CRYPTODEV_H

#include "qemu/queue.h"
#include "qom/object.h"

/**
 * CryptoDevBackend:
 *
 * The CryptoDevBackend object is an interface
 * for different cryptodev backends, which provides crypto
 * operation wrapper.
 *
 */

#define TYPE_CRYPTODEV_BACKEND "cryptodev-backend"

OBJECT_DECLARE_TYPE(CryptoDevBackend, CryptoDevBackendClass,
                    CRYPTODEV_BACKEND)


#define MAX_CRYPTO_QUEUE_NUM  64

typedef struct CryptoDevBackendConf CryptoDevBackendConf;
typedef struct CryptoDevBackendPeers CryptoDevBackendPeers;
typedef struct CryptoDevBackendClient
                     CryptoDevBackendClient;

enum CryptoDevBackendAlgType {
    CRYPTODEV_BACKEND_ALG_SYM,
    CRYPTODEV_BACKEND_ALG_ASYM,
    CRYPTODEV_BACKEND_ALG__MAX,
};

/**
 * CryptoDevBackendSymSessionInfo:
 *
 * @cipher_alg: algorithm type of CIPHER
 * @key_len: byte length of cipher key
 * @hash_alg: algorithm type of HASH/MAC
 * @hash_result_len: byte length of HASH operation result
 * @auth_key_len: byte length of authenticated key
 * @add_len: byte length of additional authenticated data
 * @op_type: operation type (refer to virtio_crypto.h)
 * @direction: encryption or direction for CIPHER
 * @hash_mode: HASH mode for HASH operation (refer to virtio_crypto.h)
 * @alg_chain_order: order of algorithm chaining (CIPHER then HASH,
 *                   or HASH then CIPHER)
 * @cipher_key: point to a key of CIPHER
 * @auth_key: point to an authenticated key of MAC
 *
 */
typedef struct CryptoDevBackendSymSessionInfo {
    /* corresponding with virtio crypto spec */
    uint32_t cipher_alg;
    uint32_t key_len;
    uint32_t hash_alg;
    uint32_t hash_result_len;
    uint32_t auth_key_len;
    uint32_t add_len;
    uint8_t op_type;
    uint8_t direction;
    uint8_t hash_mode;
    uint8_t alg_chain_order;
    uint8_t *cipher_key;
    uint8_t *auth_key;
} CryptoDevBackendSymSessionInfo;

/**
 * CryptoDevBackendAsymSessionInfo:
 */
typedef struct CryptoDevBackendRsaPara {
    uint32_t padding_algo;
    uint32_t hash_algo;
} CryptoDevBackendRsaPara;

typedef struct CryptoDevBackendAsymSessionInfo {
    /* corresponding with virtio crypto spec */
    uint32_t algo;
    uint32_t keytype;
    uint32_t keylen;
    uint8_t *key;
    union {
        CryptoDevBackendRsaPara rsa;
    } u;
} CryptoDevBackendAsymSessionInfo;

typedef struct CryptoDevBackendSessionInfo {
    uint32_t op_code;
    union {
        CryptoDevBackendSymSessionInfo sym_sess_info;
        CryptoDevBackendAsymSessionInfo asym_sess_info;
    } u;
    uint64_t session_id;
} CryptoDevBackendSessionInfo;

/**
 * CryptoDevBackendSymOpInfo:
 *
 * @aad_len: byte length of additional authenticated data
 * @iv_len: byte length of initialization vector or counter
 * @src_len: byte length of source data
 * @dst_len: byte length of destination data
 * @digest_result_len: byte length of hash digest result
 * @hash_start_src_offset: Starting point for hash processing, specified
 *  as number of bytes from start of packet in source data, only used for
 *  algorithm chain
 * @cipher_start_src_offset: Starting point for cipher processing, specified
 *  as number of bytes from start of packet in source data, only used for
 *  algorithm chain
 * @len_to_hash: byte length of source data on which the hash
 *  operation will be computed, only used for algorithm chain
 * @len_to_cipher: byte length of source data on which the cipher
 *  operation will be computed, only used for algorithm chain
 * @op_type: operation type (refer to virtio_crypto.h)
 * @iv: point to the initialization vector or counter
 * @src: point to the source data
 * @dst: point to the destination data
 * @aad_data: point to the additional authenticated data
 * @digest_result: point to the digest result data
 * @data[0]: point to the extensional memory by one memory allocation
 *
 */
typedef struct CryptoDevBackendSymOpInfo {
    uint32_t aad_len;
    uint32_t iv_len;
    uint32_t src_len;
    uint32_t dst_len;
    uint32_t digest_result_len;
    uint32_t hash_start_src_offset;
    uint32_t cipher_start_src_offset;
    uint32_t len_to_hash;
    uint32_t len_to_cipher;
    uint8_t op_type;
    uint8_t *iv;
    uint8_t *src;
    uint8_t *dst;
    uint8_t *aad_data;
    uint8_t *digest_result;
    uint8_t data[];
} CryptoDevBackendSymOpInfo;


/**
 * CryptoDevBackendAsymOpInfo:
 *
 * @src_len: byte length of source data
 * @dst_len: byte length of destination data
 * @src: point to the source data
 * @dst: point to the destination data
 *
 */
typedef struct CryptoDevBackendAsymOpInfo {
    uint32_t src_len;
    uint32_t dst_len;
    uint8_t *src;
    uint8_t *dst;
} CryptoDevBackendAsymOpInfo;

typedef struct CryptoDevBackendOpInfo {
    enum CryptoDevBackendAlgType algtype;
    uint32_t op_code;
    uint64_t session_id;
    union {
        CryptoDevBackendSymOpInfo *sym_op_info;
        CryptoDevBackendAsymOpInfo *asym_op_info;
    } u;
} CryptoDevBackendOpInfo;

typedef void (*CryptoDevCompletionFunc) (void *opaque, int ret);
struct CryptoDevBackendClass {
    ObjectClass parent_class;

    void (*init)(CryptoDevBackend *backend, Error **errp);
    void (*cleanup)(CryptoDevBackend *backend, Error **errp);

    int (*create_session)(CryptoDevBackend *backend,
                          CryptoDevBackendSessionInfo *sess_info,
                          uint32_t queue_index,
                          CryptoDevCompletionFunc cb,
                          void *opaque);

    int (*close_session)(CryptoDevBackend *backend,
                         uint64_t session_id,
                         uint32_t queue_index,
                         CryptoDevCompletionFunc cb,
                         void *opaque);

    int (*do_op)(CryptoDevBackend *backend,
                 CryptoDevBackendOpInfo *op_info,
                 uint32_t queue_index,
                 CryptoDevCompletionFunc cb,
                 void *opaque);
};

typedef enum CryptoDevBackendOptionsType {
    CRYPTODEV_BACKEND_TYPE_NONE = 0,
    CRYPTODEV_BACKEND_TYPE_BUILTIN = 1,
    CRYPTODEV_BACKEND_TYPE_VHOST_USER = 2,
    CRYPTODEV_BACKEND_TYPE_LKCF = 3,
    CRYPTODEV_BACKEND_TYPE__MAX,
} CryptoDevBackendOptionsType;

struct CryptoDevBackendClient {
    CryptoDevBackendOptionsType type;
    char *model;
    char *name;
    char *info_str;
    unsigned int queue_index;
    int vring_enable;
    QTAILQ_ENTRY(CryptoDevBackendClient) next;
};

struct CryptoDevBackendPeers {
    CryptoDevBackendClient *ccs[MAX_CRYPTO_QUEUE_NUM];
    uint32_t queues;
};

struct CryptoDevBackendConf {
    CryptoDevBackendPeers peers;

    /* Supported service mask */
    uint32_t crypto_services;

    /* Detailed algorithms mask */
    uint32_t cipher_algo_l;
    uint32_t cipher_algo_h;
    uint32_t hash_algo;
    uint32_t mac_algo_l;
    uint32_t mac_algo_h;
    uint32_t aead_algo;
    uint32_t akcipher_algo;
    /* Maximum length of cipher key */
    uint32_t max_cipher_key_len;
    /* Maximum length of authenticated key */
    uint32_t max_auth_key_len;
    /* Maximum size of each crypto request's content */
    uint64_t max_size;
};

struct CryptoDevBackend {
    Object parent_obj;

    bool ready;
    /* Tag the cryptodev backend is used by virtio-crypto or not */
    bool is_used;
    CryptoDevBackendConf conf;
};

/**
 * cryptodev_backend_new_client:
 * @model: the cryptodev backend model
 * @name: the cryptodev backend name, can be NULL
 *
 * Creates a new cryptodev backend client object
 * with the @name in the model @model.
 *
 * The returned object must be released with
 * cryptodev_backend_free_client() when no
 * longer required
 *
 * Returns: a new cryptodev backend client object
 */
CryptoDevBackendClient *
cryptodev_backend_new_client(const char *model,
                                    const char *name);
/**
 * cryptodev_backend_free_client:
 * @cc: the cryptodev backend client object
 *
 * Release the memory associated with @cc that
 * was previously allocated by cryptodev_backend_new_client()
 */
void cryptodev_backend_free_client(
                  CryptoDevBackendClient *cc);

/**
 * cryptodev_backend_cleanup:
 * @backend: the cryptodev backend object
 * @errp: pointer to a NULL-initialized error object
 *
 * Clean the resouce associated with @backend that realizaed
 * by the specific backend's init() callback
 */
void cryptodev_backend_cleanup(
           CryptoDevBackend *backend,
           Error **errp);

/**
 * cryptodev_backend_create_session:
 * @backend: the cryptodev backend object
 * @sess_info: parameters needed by session creating
 * @queue_index: queue index of cryptodev backend client
 * @errp: pointer to a NULL-initialized error object
 * @cb: callback when session create is compeleted
 * @opaque: parameter passed to callback
 *
 * Create a session for symmetric/asymmetric algorithms
 *
 * Returns: 0 for success and cb will be called when creation is completed,
 * negative value for error, and cb will not be called.
 */
int cryptodev_backend_create_session(
           CryptoDevBackend *backend,
           CryptoDevBackendSessionInfo *sess_info,
           uint32_t queue_index,
           CryptoDevCompletionFunc cb,
           void *opaque);

/**
 * cryptodev_backend_close_session:
 * @backend: the cryptodev backend object
 * @session_id: the session id
 * @queue_index: queue index of cryptodev backend client
 * @errp: pointer to a NULL-initialized error object
 * @cb: callback when session create is compeleted
 * @opaque: parameter passed to callback
 *
 * Close a session for which was previously
 * created by cryptodev_backend_create_session()
 *
 * Returns: 0 for success and cb will be called when creation is completed,
 * negative value for error, and cb will not be called.
 */
int cryptodev_backend_close_session(
           CryptoDevBackend *backend,
           uint64_t session_id,
           uint32_t queue_index,
           CryptoDevCompletionFunc cb,
           void *opaque);

/**
 * cryptodev_backend_crypto_operation:
 * @backend: the cryptodev backend object
 * @opaque1: pointer to a VirtIOCryptoReq object
 * @queue_index: queue index of cryptodev backend client
 * @errp: pointer to a NULL-initialized error object
 * @cb: callbacks when operation is completed
 * @opaque2: parameter passed to cb
 *
 * Do crypto operation, such as encryption and
 * decryption
 *
 * Returns: 0 for success and cb will be called when creation is completed,
 * negative value for error, and cb will not be called.
 */
int cryptodev_backend_crypto_operation(
                 CryptoDevBackend *backend,
                 void *opaque1,
                 uint32_t queue_index,
                 CryptoDevCompletionFunc cb,
                 void *opaque2);

/**
 * cryptodev_backend_set_used:
 * @backend: the cryptodev backend object
 * @used: ture or false
 *
 * Set the cryptodev backend is used by virtio-crypto or not
 */
void cryptodev_backend_set_used(CryptoDevBackend *backend, bool used);

/**
 * cryptodev_backend_is_used:
 * @backend: the cryptodev backend object
 *
 * Return the status that the cryptodev backend is used
 * by virtio-crypto or not
 *
 * Returns: true on used, or false on not used
 */
bool cryptodev_backend_is_used(CryptoDevBackend *backend);

/**
 * cryptodev_backend_set_ready:
 * @backend: the cryptodev backend object
 * @ready: ture or false
 *
 * Set the cryptodev backend is ready or not, which is called
 * by the children of the cryptodev banckend interface.
 */
void cryptodev_backend_set_ready(CryptoDevBackend *backend, bool ready);

/**
 * cryptodev_backend_is_ready:
 * @backend: the cryptodev backend object
 *
 * Return the status that the cryptodev backend is ready or not
 *
 * Returns: true on ready, or false on not ready
 */
bool cryptodev_backend_is_ready(CryptoDevBackend *backend);

#endif /* CRYPTODEV_H */
