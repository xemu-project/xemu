/*
 * Xen 9p backend
 *
 * Copyright Aporeto 2017
 *
 * Authors:
 *  Stefano Stabellini <stefano@aporeto.com>
 *
 */

/*
 * Not so fast! You might want to read the 9p developer docs first:
 * https://wiki.qemu.org/Documentation/9p
 */

#include "qemu/osdep.h"

#include "hw/9pfs/9p.h"
#include "hw/xen/xen-legacy-backend.h"
#include "hw/9pfs/xen-9pfs.h"
#include "qapi/error.h"
#include "qemu/config-file.h"
#include "qemu/main-loop.h"
#include "qemu/option.h"
#include "qemu/iov.h"
#include "fsdev/qemu-fsdev.h"

#include "trace.h"

#define VERSIONS "1"
#define MAX_RINGS 8
#define MAX_RING_ORDER 9

typedef struct Xen9pfsRing {
    struct Xen9pfsDev *priv;

    int ref;
    xenevtchn_handle   *evtchndev;
    int evtchn;
    int local_port;
    int ring_order;
    struct xen_9pfs_data_intf *intf;
    unsigned char *data;
    struct xen_9pfs_data ring;

    struct iovec *sg;
    QEMUBH *bh;
    Coroutine *co;

    /* local copies, so that we can read/write PDU data directly from
     * the ring */
    RING_IDX out_cons, out_size, in_cons;
    bool inprogress;
} Xen9pfsRing;

typedef struct Xen9pfsDev {
    struct XenLegacyDevice xendev;  /* must be first */
    V9fsState state;
    char *path;
    char *security_model;
    char *tag;
    char *id;

    int num_rings;
    Xen9pfsRing *rings;
    MemReentrancyGuard mem_reentrancy_guard;
} Xen9pfsDev;

static void xen_9pfs_disconnect(struct XenLegacyDevice *xendev);

static void xen_9pfs_in_sg(Xen9pfsRing *ring,
                           struct iovec *in_sg,
                           int *num,
                           uint32_t idx,
                           uint32_t size)
{
    RING_IDX cons, prod, masked_prod, masked_cons;

    cons = ring->intf->in_cons;
    prod = ring->intf->in_prod;
    xen_rmb();
    masked_prod = xen_9pfs_mask(prod, XEN_FLEX_RING_SIZE(ring->ring_order));
    masked_cons = xen_9pfs_mask(cons, XEN_FLEX_RING_SIZE(ring->ring_order));

    if (masked_prod < masked_cons) {
        in_sg[0].iov_base = ring->ring.in + masked_prod;
        in_sg[0].iov_len = masked_cons - masked_prod;
        *num = 1;
    } else {
        in_sg[0].iov_base = ring->ring.in + masked_prod;
        in_sg[0].iov_len = XEN_FLEX_RING_SIZE(ring->ring_order) - masked_prod;
        in_sg[1].iov_base = ring->ring.in;
        in_sg[1].iov_len = masked_cons;
        *num = 2;
    }
}

static void xen_9pfs_out_sg(Xen9pfsRing *ring,
                            struct iovec *out_sg,
                            int *num,
                            uint32_t idx)
{
    RING_IDX cons, prod, masked_prod, masked_cons;

    cons = ring->intf->out_cons;
    prod = ring->intf->out_prod;
    xen_rmb();
    masked_prod = xen_9pfs_mask(prod, XEN_FLEX_RING_SIZE(ring->ring_order));
    masked_cons = xen_9pfs_mask(cons, XEN_FLEX_RING_SIZE(ring->ring_order));

    if (masked_cons < masked_prod) {
        out_sg[0].iov_base = ring->ring.out + masked_cons;
        out_sg[0].iov_len = ring->out_size;
        *num = 1;
    } else {
        if (ring->out_size >
            (XEN_FLEX_RING_SIZE(ring->ring_order) - masked_cons)) {
            out_sg[0].iov_base = ring->ring.out + masked_cons;
            out_sg[0].iov_len = XEN_FLEX_RING_SIZE(ring->ring_order) -
                                masked_cons;
            out_sg[1].iov_base = ring->ring.out;
            out_sg[1].iov_len = ring->out_size -
                                (XEN_FLEX_RING_SIZE(ring->ring_order) -
                                 masked_cons);
            *num = 2;
        } else {
            out_sg[0].iov_base = ring->ring.out + masked_cons;
            out_sg[0].iov_len = ring->out_size;
            *num = 1;
        }
    }
}

static ssize_t xen_9pfs_pdu_vmarshal(V9fsPDU *pdu,
                                     size_t offset,
                                     const char *fmt,
                                     va_list ap)
{
    Xen9pfsDev *xen_9pfs = container_of(pdu->s, Xen9pfsDev, state);
    struct iovec in_sg[2];
    int num;
    ssize_t ret;

    xen_9pfs_in_sg(&xen_9pfs->rings[pdu->tag % xen_9pfs->num_rings],
                   in_sg, &num, pdu->idx, ROUND_UP(offset + 128, 512));

    ret = v9fs_iov_vmarshal(in_sg, num, offset, 0, fmt, ap);
    if (ret < 0) {
        xen_pv_printf(&xen_9pfs->xendev, 0,
                      "Failed to encode VirtFS reply type %d\n",
                      pdu->id + 1);
        xen_be_set_state(&xen_9pfs->xendev, XenbusStateClosing);
        xen_9pfs_disconnect(&xen_9pfs->xendev);
    }
    return ret;
}

static ssize_t xen_9pfs_pdu_vunmarshal(V9fsPDU *pdu,
                                       size_t offset,
                                       const char *fmt,
                                       va_list ap)
{
    Xen9pfsDev *xen_9pfs = container_of(pdu->s, Xen9pfsDev, state);
    struct iovec out_sg[2];
    int num;
    ssize_t ret;

    xen_9pfs_out_sg(&xen_9pfs->rings[pdu->tag % xen_9pfs->num_rings],
                    out_sg, &num, pdu->idx);

    ret = v9fs_iov_vunmarshal(out_sg, num, offset, 0, fmt, ap);
    if (ret < 0) {
        xen_pv_printf(&xen_9pfs->xendev, 0,
                      "Failed to decode VirtFS request type %d\n", pdu->id);
        xen_be_set_state(&xen_9pfs->xendev, XenbusStateClosing);
        xen_9pfs_disconnect(&xen_9pfs->xendev);
    }
    return ret;
}

static void xen_9pfs_init_out_iov_from_pdu(V9fsPDU *pdu,
                                           struct iovec **piov,
                                           unsigned int *pniov,
                                           size_t size)
{
    Xen9pfsDev *xen_9pfs = container_of(pdu->s, Xen9pfsDev, state);
    Xen9pfsRing *ring = &xen_9pfs->rings[pdu->tag % xen_9pfs->num_rings];
    int num;

    g_free(ring->sg);

    ring->sg = g_new0(struct iovec, 2);
    xen_9pfs_out_sg(ring, ring->sg, &num, pdu->idx);
    *piov = ring->sg;
    *pniov = num;
}

static void xen_9pfs_init_in_iov_from_pdu(V9fsPDU *pdu,
                                          struct iovec **piov,
                                          unsigned int *pniov,
                                          size_t size)
{
    Xen9pfsDev *xen_9pfs = container_of(pdu->s, Xen9pfsDev, state);
    Xen9pfsRing *ring = &xen_9pfs->rings[pdu->tag % xen_9pfs->num_rings];
    int num;
    size_t buf_size;

    g_free(ring->sg);

    ring->sg = g_new0(struct iovec, 2);
    ring->co = qemu_coroutine_self();
    /* make sure other threads see ring->co changes before continuing */
    smp_wmb();

again:
    xen_9pfs_in_sg(ring, ring->sg, &num, pdu->idx, size);
    buf_size = iov_size(ring->sg, num);
    if (buf_size  < size) {
        qemu_coroutine_yield();
        goto again;
    }
    ring->co = NULL;
    /* make sure other threads see ring->co changes before continuing */
    smp_wmb();

    *piov = ring->sg;
    *pniov = num;
}

static void xen_9pfs_push_and_notify(V9fsPDU *pdu)
{
    RING_IDX prod;
    Xen9pfsDev *priv = container_of(pdu->s, Xen9pfsDev, state);
    Xen9pfsRing *ring = &priv->rings[pdu->tag % priv->num_rings];

    g_free(ring->sg);
    ring->sg = NULL;

    ring->intf->out_cons = ring->out_cons;
    xen_wmb();

    prod = ring->intf->in_prod;
    xen_rmb();
    ring->intf->in_prod = prod + pdu->size;
    xen_wmb();

    ring->inprogress = false;
    qemu_xen_evtchn_notify(ring->evtchndev, ring->local_port);

    qemu_bh_schedule(ring->bh);
}

static const V9fsTransport xen_9p_transport = {
    .pdu_vmarshal = xen_9pfs_pdu_vmarshal,
    .pdu_vunmarshal = xen_9pfs_pdu_vunmarshal,
    .init_in_iov_from_pdu = xen_9pfs_init_in_iov_from_pdu,
    .init_out_iov_from_pdu = xen_9pfs_init_out_iov_from_pdu,
    .push_and_notify = xen_9pfs_push_and_notify,
};

static int xen_9pfs_init(struct XenLegacyDevice *xendev)
{
    return 0;
}

static int xen_9pfs_receive(Xen9pfsRing *ring)
{
    P9MsgHeader h;
    RING_IDX cons, prod, masked_prod, masked_cons, queued;
    V9fsPDU *pdu;

    if (ring->inprogress) {
        return 0;
    }

    cons = ring->intf->out_cons;
    prod = ring->intf->out_prod;
    xen_rmb();

    queued = xen_9pfs_queued(prod, cons, XEN_FLEX_RING_SIZE(ring->ring_order));
    if (queued < sizeof(h)) {
        return 0;
    }
    ring->inprogress = true;

    masked_prod = xen_9pfs_mask(prod, XEN_FLEX_RING_SIZE(ring->ring_order));
    masked_cons = xen_9pfs_mask(cons, XEN_FLEX_RING_SIZE(ring->ring_order));

    xen_9pfs_read_packet((uint8_t *) &h, ring->ring.out, sizeof(h),
                         masked_prod, &masked_cons,
                         XEN_FLEX_RING_SIZE(ring->ring_order));
    if (queued < le32_to_cpu(h.size_le)) {
        return 0;
    }

    /* cannot fail, because we only handle one request per ring at a time */
    pdu = pdu_alloc(&ring->priv->state);
    ring->out_size = le32_to_cpu(h.size_le);
    ring->out_cons = cons + le32_to_cpu(h.size_le);

    pdu_submit(pdu, &h);

    return 0;
}

static void xen_9pfs_bh(void *opaque)
{
    Xen9pfsRing *ring = opaque;
    bool wait;

again:
    wait = ring->co != NULL && qemu_coroutine_entered(ring->co);
    /* paired with the smb_wmb barriers in xen_9pfs_init_in_iov_from_pdu */
    smp_rmb();
    if (wait) {
        cpu_relax();
        goto again;
    }

    if (ring->co != NULL) {
        qemu_coroutine_enter_if_inactive(ring->co);
    }
    xen_9pfs_receive(ring);
}

static void xen_9pfs_evtchn_event(void *opaque)
{
    Xen9pfsRing *ring = opaque;
    evtchn_port_t port;

    port = qemu_xen_evtchn_pending(ring->evtchndev);
    qemu_xen_evtchn_unmask(ring->evtchndev, port);

    qemu_bh_schedule(ring->bh);
}

static void xen_9pfs_disconnect(struct XenLegacyDevice *xendev)
{
    Xen9pfsDev *xen_9pdev = container_of(xendev, Xen9pfsDev, xendev);
    int i;

    trace_xen_9pfs_disconnect(xendev->name);

    for (i = 0; i < xen_9pdev->num_rings; i++) {
        if (xen_9pdev->rings[i].evtchndev != NULL) {
            qemu_set_fd_handler(qemu_xen_evtchn_fd(xen_9pdev->rings[i].evtchndev),
                                NULL, NULL, NULL);
            qemu_xen_evtchn_unbind(xen_9pdev->rings[i].evtchndev,
                                   xen_9pdev->rings[i].local_port);
            xen_9pdev->rings[i].evtchndev = NULL;
        }
        if (xen_9pdev->rings[i].data != NULL) {
            xen_be_unmap_grant_refs(&xen_9pdev->xendev,
                                    xen_9pdev->rings[i].data,
                                    xen_9pdev->rings[i].intf->ref,
                                    (1 << xen_9pdev->rings[i].ring_order));
            xen_9pdev->rings[i].data = NULL;
        }
        if (xen_9pdev->rings[i].intf != NULL) {
            xen_be_unmap_grant_ref(&xen_9pdev->xendev,
                                   xen_9pdev->rings[i].intf,
                                   xen_9pdev->rings[i].ref);
            xen_9pdev->rings[i].intf = NULL;
        }
        if (xen_9pdev->rings[i].bh != NULL) {
            qemu_bh_delete(xen_9pdev->rings[i].bh);
            xen_9pdev->rings[i].bh = NULL;
        }
    }

    g_free(xen_9pdev->id);
    xen_9pdev->id = NULL;
    g_free(xen_9pdev->tag);
    xen_9pdev->tag = NULL;
    g_free(xen_9pdev->path);
    xen_9pdev->path = NULL;
    g_free(xen_9pdev->security_model);
    xen_9pdev->security_model = NULL;
    g_free(xen_9pdev->rings);
    xen_9pdev->rings = NULL;
}

static int xen_9pfs_free(struct XenLegacyDevice *xendev)
{
    trace_xen_9pfs_free(xendev->name);

    return 0;
}

static int xen_9pfs_connect(struct XenLegacyDevice *xendev)
{
    Error *err = NULL;
    int i;
    Xen9pfsDev *xen_9pdev = container_of(xendev, Xen9pfsDev, xendev);
    V9fsState *s = &xen_9pdev->state;
    QemuOpts *fsdev;

    trace_xen_9pfs_connect(xendev->name);

    if (xenstore_read_fe_int(&xen_9pdev->xendev, "num-rings",
                             &xen_9pdev->num_rings) == -1 ||
        xen_9pdev->num_rings > MAX_RINGS || xen_9pdev->num_rings < 1) {
        return -1;
    }

    xen_9pdev->rings = g_new0(Xen9pfsRing, xen_9pdev->num_rings);
    for (i = 0; i < xen_9pdev->num_rings; i++) {
        char *str;
        int ring_order;

        xen_9pdev->rings[i].priv = xen_9pdev;
        xen_9pdev->rings[i].evtchn = -1;
        xen_9pdev->rings[i].local_port = -1;

        str = g_strdup_printf("ring-ref%u", i);
        if (xenstore_read_fe_int(&xen_9pdev->xendev, str,
                                 &xen_9pdev->rings[i].ref) == -1) {
            g_free(str);
            goto out;
        }
        g_free(str);
        str = g_strdup_printf("event-channel-%u", i);
        if (xenstore_read_fe_int(&xen_9pdev->xendev, str,
                                 &xen_9pdev->rings[i].evtchn) == -1) {
            g_free(str);
            goto out;
        }
        g_free(str);

        xen_9pdev->rings[i].intf =
            xen_be_map_grant_ref(&xen_9pdev->xendev,
                                 xen_9pdev->rings[i].ref,
                                 PROT_READ | PROT_WRITE);
        if (!xen_9pdev->rings[i].intf) {
            goto out;
        }
        ring_order = xen_9pdev->rings[i].intf->ring_order;
        if (ring_order > MAX_RING_ORDER) {
            goto out;
        }
        xen_9pdev->rings[i].ring_order = ring_order;
        xen_9pdev->rings[i].data =
            xen_be_map_grant_refs(&xen_9pdev->xendev,
                                  xen_9pdev->rings[i].intf->ref,
                                  (1 << ring_order),
                                  PROT_READ | PROT_WRITE);
        if (!xen_9pdev->rings[i].data) {
            goto out;
        }
        xen_9pdev->rings[i].ring.in = xen_9pdev->rings[i].data;
        xen_9pdev->rings[i].ring.out = xen_9pdev->rings[i].data +
                                       XEN_FLEX_RING_SIZE(ring_order);

        xen_9pdev->rings[i].bh = qemu_bh_new_guarded(xen_9pfs_bh,
                                                     &xen_9pdev->rings[i],
                                                     &xen_9pdev->mem_reentrancy_guard);
        xen_9pdev->rings[i].out_cons = 0;
        xen_9pdev->rings[i].out_size = 0;
        xen_9pdev->rings[i].inprogress = false;


        xen_9pdev->rings[i].evtchndev = qemu_xen_evtchn_open();
        if (xen_9pdev->rings[i].evtchndev == NULL) {
            goto out;
        }
        qemu_set_cloexec(qemu_xen_evtchn_fd(xen_9pdev->rings[i].evtchndev));
        xen_9pdev->rings[i].local_port = qemu_xen_evtchn_bind_interdomain
                                            (xen_9pdev->rings[i].evtchndev,
                                             xendev->dom,
                                             xen_9pdev->rings[i].evtchn);
        if (xen_9pdev->rings[i].local_port == -1) {
            xen_pv_printf(xendev, 0,
                          "xenevtchn_bind_interdomain failed port=%d\n",
                          xen_9pdev->rings[i].evtchn);
            goto out;
        }
        xen_pv_printf(xendev, 2, "bind evtchn port %d\n", xendev->local_port);
        qemu_set_fd_handler(qemu_xen_evtchn_fd(xen_9pdev->rings[i].evtchndev),
                            xen_9pfs_evtchn_event, NULL, &xen_9pdev->rings[i]);
    }

    xen_9pdev->security_model = xenstore_read_be_str(xendev, "security_model");
    xen_9pdev->path = xenstore_read_be_str(xendev, "path");
    xen_9pdev->id = s->fsconf.fsdev_id =
        g_strdup_printf("xen9p%d", xendev->dev);
    xen_9pdev->tag = s->fsconf.tag = xenstore_read_fe_str(xendev, "tag");
    fsdev = qemu_opts_create(qemu_find_opts("fsdev"),
            s->fsconf.tag,
            1, NULL);
    qemu_opt_set(fsdev, "fsdriver", "local", NULL);
    qemu_opt_set(fsdev, "path", xen_9pdev->path, NULL);
    qemu_opt_set(fsdev, "security_model", xen_9pdev->security_model, NULL);
    qemu_opts_set_id(fsdev, s->fsconf.fsdev_id);
    qemu_fsdev_add(fsdev, &err);
    if (err) {
        error_report_err(err);
    }
    v9fs_device_realize_common(s, &xen_9p_transport, NULL);

    return 0;

out:
    xen_9pfs_free(xendev);
    return -1;
}

static void xen_9pfs_alloc(struct XenLegacyDevice *xendev)
{
    trace_xen_9pfs_alloc(xendev->name);

    xenstore_write_be_str(xendev, "versions", VERSIONS);
    xenstore_write_be_int(xendev, "max-rings", MAX_RINGS);
    xenstore_write_be_int(xendev, "max-ring-page-order", MAX_RING_ORDER);
}

static const struct XenDevOps xen_9pfs_ops = {
    .size       = sizeof(Xen9pfsDev),
    .flags      = DEVOPS_FLAG_NEED_GNTDEV,
    .alloc      = xen_9pfs_alloc,
    .init       = xen_9pfs_init,
    .initialise = xen_9pfs_connect,
    .disconnect = xen_9pfs_disconnect,
    .free       = xen_9pfs_free,
};

static void xen_9pfs_register_backend(void)
{
    xen_be_register("9pfs", &xen_9pfs_ops);
}
xen_backend_init(xen_9pfs_register_backend);
