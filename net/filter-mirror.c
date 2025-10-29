/*
 * Copyright (c) 2016 HUAWEI TECHNOLOGIES CO., LTD.
 * Copyright (c) 2016 FUJITSU LIMITED
 * Copyright (c) 2016 Intel Corporation
 *
 * Author: Zhang Chen <zhangchen.fnst@cn.fujitsu.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * later.  See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "net/filter.h"
#include "net/net.h"
#include "qapi/error.h"
#include "qom/object.h"
#include "qemu/main-loop.h"
#include "qemu/error-report.h"
#include "trace.h"
#include "chardev/char-fe.h"
#include "qemu/iov.h"
#include "qemu/sockets.h"
#include "block/aio-wait.h"

#define TYPE_FILTER_MIRROR "filter-mirror"
typedef struct MirrorState MirrorState;
DECLARE_INSTANCE_CHECKER(MirrorState, FILTER_MIRROR,
                         TYPE_FILTER_MIRROR)

#define TYPE_FILTER_REDIRECTOR "filter-redirector"
DECLARE_INSTANCE_CHECKER(MirrorState, FILTER_REDIRECTOR,
                         TYPE_FILTER_REDIRECTOR)

#define REDIRECTOR_MAX_LEN NET_BUFSIZE

struct MirrorState {
    NetFilterState parent_obj;
    char *indev;
    char *outdev;
    CharBackend chr_in;
    CharBackend chr_out;
    SocketReadState rs;
    bool vnet_hdr;
};

typedef struct FilterSendCo {
    MirrorState *s;
    char *buf;
    ssize_t size;
    bool done;
    int ret;
} FilterSendCo;

static int _filter_send(MirrorState *s,
                       char *buf,
                       ssize_t size)
{
    NetFilterState *nf = NETFILTER(s);
    int ret = 0;
    uint32_t len = 0;

    len = htonl(size);
    ret = qemu_chr_fe_write_all(&s->chr_out, (uint8_t *)&len, sizeof(len));
    if (ret != sizeof(len)) {
        goto err;
    }

    if (s->vnet_hdr) {
        /*
         * If vnet_hdr = on, we send vnet header len to make other
         * module(like colo-compare) know how to parse net
         * packet correctly.
         */
        ssize_t vnet_hdr_len;

        vnet_hdr_len = nf->netdev->vnet_hdr_len;

        len = htonl(vnet_hdr_len);
        ret = qemu_chr_fe_write_all(&s->chr_out, (uint8_t *)&len, sizeof(len));
        if (ret != sizeof(len)) {
            goto err;
        }
    }

    ret = qemu_chr_fe_write_all(&s->chr_out, (uint8_t *)buf, size);
    if (ret != size) {
        goto err;
    }

    return size;

err:
    return ret < 0 ? ret : -EIO;
}

static void coroutine_fn filter_send_co(void *opaque)
{
    FilterSendCo *data = opaque;

    data->ret = _filter_send(data->s, data->buf, data->size);
    data->done = true;
    g_free(data->buf);
    aio_wait_kick();
}

static int filter_send(MirrorState *s,
                       const struct iovec *iov,
                       int iovcnt)
{
    ssize_t size = iov_size(iov, iovcnt);
    char *buf = NULL;

    if (!size) {
        return 0;
    }

    buf = g_malloc(size);
    iov_to_buf(iov, iovcnt, 0, buf, size);

    FilterSendCo data = {
        .s = s,
        .size = size,
        .buf = buf,
        .ret = 0,
    };

    Coroutine *co = qemu_coroutine_create(filter_send_co, &data);
    qemu_coroutine_enter(co);

    while (!data.done) {
        aio_poll(qemu_get_aio_context(), true);
    }

    return data.ret;
}

static void redirector_to_filter(NetFilterState *nf,
                                 const uint8_t *buf,
                                 int len)
{
    struct iovec iov = {
        .iov_base = (void *)buf,
        .iov_len = len,
    };

    if (nf->direction == NET_FILTER_DIRECTION_ALL ||
        nf->direction == NET_FILTER_DIRECTION_TX) {
        qemu_netfilter_pass_to_next(nf->netdev, 0, &iov, 1, nf);
    }

    if (nf->direction == NET_FILTER_DIRECTION_ALL ||
        nf->direction == NET_FILTER_DIRECTION_RX) {
        qemu_netfilter_pass_to_next(nf->netdev->peer, 0, &iov, 1, nf);
     }
}

static int redirector_chr_can_read(void *opaque)
{
    return REDIRECTOR_MAX_LEN;
}

static void redirector_chr_read(void *opaque, const uint8_t *buf, int size)
{
    NetFilterState *nf = opaque;
    MirrorState *s = FILTER_REDIRECTOR(nf);
    int ret;

    ret = net_fill_rstate(&s->rs, buf, size);

    if (ret == -1) {
        qemu_chr_fe_set_handlers(&s->chr_in, NULL, NULL, NULL,
                                 NULL, NULL, NULL, true);
    }
}

static void redirector_chr_event(void *opaque, QEMUChrEvent event)
{
    NetFilterState *nf = opaque;
    MirrorState *s = FILTER_REDIRECTOR(nf);

    switch (event) {
    case CHR_EVENT_CLOSED:
        qemu_chr_fe_set_handlers(&s->chr_in, NULL, NULL, NULL,
                                 NULL, NULL, NULL, true);
        break;
    default:
        break;
    }
}

static ssize_t filter_mirror_receive_iov(NetFilterState *nf,
                                         NetClientState *sender,
                                         unsigned flags,
                                         const struct iovec *iov,
                                         int iovcnt,
                                         NetPacketSent *sent_cb)
{
    MirrorState *s = FILTER_MIRROR(nf);
    int ret;

    ret = filter_send(s, iov, iovcnt);
    if (ret < 0) {
        error_report("filter mirror send failed(%s)", strerror(-ret));
    }

    /*
     * we don't hope this error interrupt the normal
     * path of net packet, so we always return zero.
     */
    return 0;
}

static ssize_t filter_redirector_receive_iov(NetFilterState *nf,
                                             NetClientState *sender,
                                             unsigned flags,
                                             const struct iovec *iov,
                                             int iovcnt,
                                             NetPacketSent *sent_cb)
{
    MirrorState *s = FILTER_REDIRECTOR(nf);
    int ret;

    if (qemu_chr_fe_backend_connected(&s->chr_out)) {
        ret = filter_send(s, iov, iovcnt);
        if (ret < 0) {
            error_report("filter redirector send failed(%s)", strerror(-ret));
        }
        return ret;
    } else {
        return 0;
    }
}

static void filter_mirror_cleanup(NetFilterState *nf)
{
    MirrorState *s = FILTER_MIRROR(nf);

    qemu_chr_fe_deinit(&s->chr_out, false);
}

static void filter_redirector_cleanup(NetFilterState *nf)
{
    MirrorState *s = FILTER_REDIRECTOR(nf);

    qemu_chr_fe_deinit(&s->chr_in, false);
    qemu_chr_fe_deinit(&s->chr_out, false);
}

static void filter_mirror_setup(NetFilterState *nf, Error **errp)
{
    MirrorState *s = FILTER_MIRROR(nf);
    Chardev *chr;

    if (s->outdev == NULL) {
        error_set(errp, ERROR_CLASS_DEVICE_NOT_FOUND, "filter-mirror parameter"\
                  " 'outdev' cannot be empty");
        return;
    }

    chr = qemu_chr_find(s->outdev);
    if (chr == NULL) {
        error_set(errp, ERROR_CLASS_DEVICE_NOT_FOUND,
                  "Device '%s' not found", s->outdev);
        return;
    }

    qemu_chr_fe_init(&s->chr_out, chr, errp);
}

static void redirector_rs_finalize(SocketReadState *rs)
{
    MirrorState *s = container_of(rs, MirrorState, rs);
    NetFilterState *nf = NETFILTER(s);

    redirector_to_filter(nf, rs->buf, rs->packet_len);
}

static void filter_redirector_setup(NetFilterState *nf, Error **errp)
{
    MirrorState *s = FILTER_REDIRECTOR(nf);
    Chardev *chr;

    if (!s->indev && !s->outdev) {
        error_setg(errp, "filter redirector needs 'indev' or "
                   "'outdev' at least one property set");
        return;
    } else if (s->indev && s->outdev) {
        if (!strcmp(s->indev, s->outdev)) {
            error_setg(errp, "'indev' and 'outdev' could not be same "
                       "for filter redirector");
            return;
        }
    }

    net_socket_rs_init(&s->rs, redirector_rs_finalize, s->vnet_hdr);

    if (s->indev) {
        chr = qemu_chr_find(s->indev);
        if (chr == NULL) {
            error_set(errp, ERROR_CLASS_DEVICE_NOT_FOUND,
                      "IN Device '%s' not found", s->indev);
            return;
        }

        if (!qemu_chr_fe_init(&s->chr_in, chr, errp)) {
            return;
        }

        qemu_chr_fe_set_handlers(&s->chr_in, redirector_chr_can_read,
                                 redirector_chr_read, redirector_chr_event,
                                 NULL, nf, NULL, true);
    }

    if (s->outdev) {
        chr = qemu_chr_find(s->outdev);
        if (chr == NULL) {
            error_set(errp, ERROR_CLASS_DEVICE_NOT_FOUND,
                      "OUT Device '%s' not found", s->outdev);
            return;
        }
        if (!qemu_chr_fe_init(&s->chr_out, chr, errp)) {
            return;
        }
    }
}

static char *filter_redirector_get_indev(Object *obj, Error **errp)
{
    MirrorState *s = FILTER_REDIRECTOR(obj);

    return g_strdup(s->indev);
}

static void filter_redirector_set_indev(Object *obj,
                                        const char *value,
                                        Error **errp)
{
    MirrorState *s = FILTER_REDIRECTOR(obj);

    g_free(s->indev);
    s->indev = g_strdup(value);
}

static char *filter_mirror_get_outdev(Object *obj, Error **errp)
{
    MirrorState *s = FILTER_MIRROR(obj);

    return g_strdup(s->outdev);
}

static void filter_mirror_set_outdev(Object *obj,
                                     const char *value,
                                     Error **errp)
{
    MirrorState *s = FILTER_MIRROR(obj);

    g_free(s->outdev);
    s->outdev = g_strdup(value);
    if (!s->outdev) {
        error_setg(errp, "filter mirror needs 'outdev' "
                   "property set");
        return;
    }
}

static bool filter_mirror_get_vnet_hdr(Object *obj, Error **errp)
{
    MirrorState *s = FILTER_MIRROR(obj);

    return s->vnet_hdr;
}

static void filter_mirror_set_vnet_hdr(Object *obj, bool value, Error **errp)
{
    MirrorState *s = FILTER_MIRROR(obj);

    s->vnet_hdr = value;
}

static char *filter_redirector_get_outdev(Object *obj, Error **errp)
{
    MirrorState *s = FILTER_REDIRECTOR(obj);

    return g_strdup(s->outdev);
}

static void filter_redirector_set_outdev(Object *obj,
                                         const char *value,
                                         Error **errp)
{
    MirrorState *s = FILTER_REDIRECTOR(obj);

    g_free(s->outdev);
    s->outdev = g_strdup(value);
}

static bool filter_redirector_get_vnet_hdr(Object *obj, Error **errp)
{
    MirrorState *s = FILTER_REDIRECTOR(obj);

    return s->vnet_hdr;
}

static void filter_redirector_set_vnet_hdr(Object *obj,
                                           bool value,
                                           Error **errp)
{
    MirrorState *s = FILTER_REDIRECTOR(obj);

    s->vnet_hdr = value;
}

static void filter_mirror_class_init(ObjectClass *oc, void *data)
{
    NetFilterClass *nfc = NETFILTER_CLASS(oc);

    object_class_property_add_str(oc, "outdev", filter_mirror_get_outdev,
                                  filter_mirror_set_outdev);
    object_class_property_add_bool(oc, "vnet_hdr_support",
                                   filter_mirror_get_vnet_hdr,
                                   filter_mirror_set_vnet_hdr);

    nfc->setup = filter_mirror_setup;
    nfc->cleanup = filter_mirror_cleanup;
    nfc->receive_iov = filter_mirror_receive_iov;
}

static void filter_redirector_class_init(ObjectClass *oc, void *data)
{
    NetFilterClass *nfc = NETFILTER_CLASS(oc);

    object_class_property_add_str(oc, "indev", filter_redirector_get_indev,
                                  filter_redirector_set_indev);
    object_class_property_add_str(oc, "outdev", filter_redirector_get_outdev,
                                  filter_redirector_set_outdev);
    object_class_property_add_bool(oc, "vnet_hdr_support",
                                   filter_redirector_get_vnet_hdr,
                                   filter_redirector_set_vnet_hdr);

    nfc->setup = filter_redirector_setup;
    nfc->cleanup = filter_redirector_cleanup;
    nfc->receive_iov = filter_redirector_receive_iov;
}

static void filter_mirror_init(Object *obj)
{
    MirrorState *s = FILTER_MIRROR(obj);

    s->vnet_hdr = false;
}

static void filter_redirector_init(Object *obj)
{
    MirrorState *s = FILTER_REDIRECTOR(obj);

    s->vnet_hdr = false;
}

static void filter_mirror_fini(Object *obj)
{
    MirrorState *s = FILTER_MIRROR(obj);

    g_free(s->outdev);
}

static void filter_redirector_fini(Object *obj)
{
    MirrorState *s = FILTER_REDIRECTOR(obj);

    g_free(s->indev);
    g_free(s->outdev);
}

static const TypeInfo filter_redirector_info = {
    .name = TYPE_FILTER_REDIRECTOR,
    .parent = TYPE_NETFILTER,
    .class_init = filter_redirector_class_init,
    .instance_init = filter_redirector_init,
    .instance_finalize = filter_redirector_fini,
    .instance_size = sizeof(MirrorState),
};

static const TypeInfo filter_mirror_info = {
    .name = TYPE_FILTER_MIRROR,
    .parent = TYPE_NETFILTER,
    .class_init = filter_mirror_class_init,
    .instance_init = filter_mirror_init,
    .instance_finalize = filter_mirror_fini,
    .instance_size = sizeof(MirrorState),
};

static void register_types(void)
{
    type_register_static(&filter_mirror_info);
    type_register_static(&filter_redirector_info);
}

type_init(register_types);
