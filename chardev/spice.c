#include "qemu/osdep.h"
#include "trace.h"
#include "ui/qemu-spice.h"
#include "chardev/char.h"
#include "chardev/spice.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qemu/module.h"
#include "qemu/option.h"
#include <spice/protocol.h>

typedef struct SpiceCharSource {
    GSource               source;
    SpiceChardev       *scd;
} SpiceCharSource;

static int vmc_write(SpiceCharDeviceInstance *sin, const uint8_t *buf, int len)
{
    SpiceChardev *scd = container_of(sin, SpiceChardev, sin);
    Chardev *chr = CHARDEV(scd);
    ssize_t out = 0;
    ssize_t last_out;
    uint8_t* p = (uint8_t*)buf;

    while (len > 0) {
        int can_write = qemu_chr_be_can_write(chr);
        last_out = MIN(len, can_write);
        if (last_out <= 0) {
            break;
        }
        qemu_chr_be_write(chr, p, last_out);
        out += last_out;
        len -= last_out;
        p += last_out;
    }

    trace_spice_vmc_write(out, len + out);
    return out;
}

static int vmc_read(SpiceCharDeviceInstance *sin, uint8_t *buf, int len)
{
    SpiceChardev *scd = container_of(sin, SpiceChardev, sin);
    int bytes = MIN(len, scd->datalen);

    if (bytes > 0) {
        memcpy(buf, scd->datapos, bytes);
        scd->datapos += bytes;
        scd->datalen -= bytes;
        assert(scd->datalen >= 0);
    }
    if (scd->datalen == 0) {
        scd->datapos = 0;
        scd->blocked = false;
    }
    trace_spice_vmc_read(bytes, len);
    return bytes;
}

static void vmc_event(SpiceCharDeviceInstance *sin, uint8_t event)
{
    SpiceChardev *scd = container_of(sin, SpiceChardev, sin);
    Chardev *chr = CHARDEV(scd);
    int chr_event;

    switch (event) {
    case SPICE_PORT_EVENT_BREAK:
        chr_event = CHR_EVENT_BREAK;
        break;
    default:
        return;
    }

    trace_spice_vmc_event(chr_event);
    qemu_chr_be_event(chr, chr_event);
}

static void vmc_state(SpiceCharDeviceInstance *sin, int connected)
{
    SpiceChardev *scd = container_of(sin, SpiceChardev, sin);
    Chardev *chr = CHARDEV(scd);

    if ((chr->be_open && connected) ||
        (!chr->be_open && !connected)) {
        return;
    }

    qemu_chr_be_event(chr,
                      connected ? CHR_EVENT_OPENED : CHR_EVENT_CLOSED);
}

static SpiceCharDeviceInterface vmc_interface = {
    .base.type          = SPICE_INTERFACE_CHAR_DEVICE,
    .base.description   = "spice virtual channel char device",
    .base.major_version = SPICE_INTERFACE_CHAR_DEVICE_MAJOR,
    .base.minor_version = SPICE_INTERFACE_CHAR_DEVICE_MINOR,
    .state              = vmc_state,
    .write              = vmc_write,
    .read               = vmc_read,
    .event              = vmc_event,
    .flags              = SPICE_CHAR_DEVICE_NOTIFY_WRITABLE,
};


static void vmc_register_interface(SpiceChardev *scd)
{
    if (scd->active) {
        return;
    }
    scd->sin.base.sif = &vmc_interface.base;
    qemu_spice.add_interface(&scd->sin.base);
    scd->active = true;
    trace_spice_vmc_register_interface(scd);
}

static void vmc_unregister_interface(SpiceChardev *scd)
{
    if (!scd->active) {
        return;
    }
    spice_server_remove_interface(&scd->sin.base);
    scd->active = false;
    trace_spice_vmc_unregister_interface(scd);
}

static gboolean spice_char_source_prepare(GSource *source, gint *timeout)
{
    SpiceCharSource *src = (SpiceCharSource *)source;
    Chardev *chr = CHARDEV(src->scd);

    *timeout = -1;

    if (!chr->be_open) {
        return true;
    }

    return !src->scd->blocked;
}

static gboolean spice_char_source_check(GSource *source)
{
    SpiceCharSource *src = (SpiceCharSource *)source;
    Chardev *chr = CHARDEV(src->scd);

    if (!chr->be_open) {
        return true;
    }

    return !src->scd->blocked;
}

static gboolean spice_char_source_dispatch(GSource *source,
    GSourceFunc callback, gpointer user_data)
{
    SpiceCharSource *src = (SpiceCharSource *)source;
    Chardev *chr = CHARDEV(src->scd);
    GIOFunc func = (GIOFunc)callback;
    GIOCondition cond = chr->be_open ? G_IO_OUT : G_IO_HUP;

    return func(NULL, cond, user_data);
}

static GSourceFuncs SpiceCharSourceFuncs = {
    .prepare  = spice_char_source_prepare,
    .check    = spice_char_source_check,
    .dispatch = spice_char_source_dispatch,
};

static GSource *spice_chr_add_watch(Chardev *chr, GIOCondition cond)
{
    SpiceChardev *scd = SPICE_CHARDEV(chr);
    SpiceCharSource *src;

    assert(cond & G_IO_OUT);

    src = (SpiceCharSource *)g_source_new(&SpiceCharSourceFuncs,
                                          sizeof(SpiceCharSource));
    src->scd = scd;

    return (GSource *)src;
}

static int spice_chr_write(Chardev *chr, const uint8_t *buf, int len)
{
    SpiceChardev *s = SPICE_CHARDEV(chr);
    int read_bytes;

    assert(s->datalen == 0);

    if (!chr->be_open) {
        trace_spice_chr_discard_write(len);
        return len;
    }

    s->datapos = buf;
    s->datalen = len;
    spice_server_char_device_wakeup(&s->sin);
    read_bytes = len - s->datalen;
    if (read_bytes != len) {
        /* We'll get passed in the unconsumed data with the next call */
        s->datalen = 0;
        s->datapos = NULL;
        s->blocked = true;
    }
    return read_bytes;
}

static void char_spice_finalize(Object *obj)
{
    SpiceChardev *s = SPICE_CHARDEV(obj);

    vmc_unregister_interface(s);

    g_free((char *)s->sin.subtype);
    g_free((char *)s->sin.portname);
}

static void spice_vmc_set_fe_open(struct Chardev *chr, int fe_open)
{
    SpiceChardev *s = SPICE_CHARDEV(chr);
    if (fe_open) {
        vmc_register_interface(s);
    } else {
        vmc_unregister_interface(s);
    }
}

static void spice_port_set_fe_open(struct Chardev *chr, int fe_open)
{
    SpiceChardev *s = SPICE_CHARDEV(chr);

    if (fe_open) {
        spice_server_port_event(&s->sin, SPICE_PORT_EVENT_OPENED);
    } else {
        spice_server_port_event(&s->sin, SPICE_PORT_EVENT_CLOSED);
    }
}

static void spice_chr_accept_input(struct Chardev *chr)
{
    SpiceChardev *s = SPICE_CHARDEV(chr);

    spice_server_char_device_wakeup(&s->sin);
}

static void chr_open(Chardev *chr, const char *subtype)
{
    SpiceChardev *s = SPICE_CHARDEV(chr);

    s->active = false;
    s->sin.subtype = g_strdup(subtype);
}

static void qemu_chr_open_spice_vmc(Chardev *chr,
                                    ChardevBackend *backend,
                                    bool *be_opened,
                                    Error **errp)
{
    ChardevSpiceChannel *spicevmc = backend->u.spicevmc.data;
    const char *type = spicevmc->type;
    const char **psubtype = spice_server_char_device_recognized_subtypes();

    for (; *psubtype != NULL; ++psubtype) {
        if (strcmp(type, *psubtype) == 0) {
            break;
        }
    }
    if (*psubtype == NULL) {
        char *subtypes = g_strjoinv(", ",
            (gchar **)spice_server_char_device_recognized_subtypes());

        error_setg(errp, "unsupported type name: %s", type);
        error_append_hint(errp, "allowed spice char type names: %s\n",
                          subtypes);

        g_free(subtypes);
        return;
    }

    *be_opened = false;
#if SPICE_SERVER_VERSION < 0x000e02
    /* Spice < 0.14.2 doesn't explicitly open smartcard chardev */
    if (strcmp(type, "smartcard") == 0) {
        *be_opened = true;
    }
#endif
    chr_open(chr, type);
}

static void qemu_chr_open_spice_port(Chardev *chr,
                                     ChardevBackend *backend,
                                     bool *be_opened,
                                     Error **errp)
{
    ChardevSpicePort *spiceport = backend->u.spiceport.data;
    const char *name = spiceport->fqdn;
    SpiceChardev *s;

    if (name == NULL) {
        error_setg(errp, "missing name parameter");
        return;
    }

    if (!using_spice) {
        error_setg(errp, "spice not enabled");
        return;
    }

    chr_open(chr, "port");

    *be_opened = false;
    s = SPICE_CHARDEV(chr);
    s->sin.portname = g_strdup(name);

    vmc_register_interface(s);
}

static void qemu_chr_parse_spice_vmc(QemuOpts *opts, ChardevBackend *backend,
                                     Error **errp)
{
    const char *name = qemu_opt_get(opts, "name");
    ChardevSpiceChannel *spicevmc;

    if (name == NULL) {
        error_setg(errp, "chardev: spice channel: no name given");
        return;
    }
    backend->type = CHARDEV_BACKEND_KIND_SPICEVMC;
    spicevmc = backend->u.spicevmc.data = g_new0(ChardevSpiceChannel, 1);
    qemu_chr_parse_common(opts, qapi_ChardevSpiceChannel_base(spicevmc));
    spicevmc->type = g_strdup(name);
}

static void qemu_chr_parse_spice_port(QemuOpts *opts, ChardevBackend *backend,
                                      Error **errp)
{
    const char *name = qemu_opt_get(opts, "name");
    ChardevSpicePort *spiceport;

    if (name == NULL) {
        error_setg(errp, "chardev: spice port: no name given");
        return;
    }
    backend->type = CHARDEV_BACKEND_KIND_SPICEPORT;
    spiceport = backend->u.spiceport.data = g_new0(ChardevSpicePort, 1);
    qemu_chr_parse_common(opts, qapi_ChardevSpicePort_base(spiceport));
    spiceport->fqdn = g_strdup(name);
}

static void char_spice_class_init(ObjectClass *oc, void *data)
{
    ChardevClass *cc = CHARDEV_CLASS(oc);

    cc->chr_write = spice_chr_write;
    cc->chr_add_watch = spice_chr_add_watch;
    cc->chr_accept_input = spice_chr_accept_input;
}

static const TypeInfo char_spice_type_info = {
    .name = TYPE_CHARDEV_SPICE,
    .parent = TYPE_CHARDEV,
    .instance_size = sizeof(SpiceChardev),
    .instance_finalize = char_spice_finalize,
    .class_init = char_spice_class_init,
    .abstract = true,
};
module_obj(TYPE_CHARDEV_SPICE);

static void char_spicevmc_class_init(ObjectClass *oc, void *data)
{
    ChardevClass *cc = CHARDEV_CLASS(oc);

    cc->parse = qemu_chr_parse_spice_vmc;
    cc->open = qemu_chr_open_spice_vmc;
    cc->chr_set_fe_open = spice_vmc_set_fe_open;
}

static const TypeInfo char_spicevmc_type_info = {
    .name = TYPE_CHARDEV_SPICEVMC,
    .parent = TYPE_CHARDEV_SPICE,
    .class_init = char_spicevmc_class_init,
};
module_obj(TYPE_CHARDEV_SPICEVMC);

static void char_spiceport_class_init(ObjectClass *oc, void *data)
{
    ChardevClass *cc = CHARDEV_CLASS(oc);

    cc->parse = qemu_chr_parse_spice_port;
    cc->open = qemu_chr_open_spice_port;
    cc->chr_set_fe_open = spice_port_set_fe_open;
}

static const TypeInfo char_spiceport_type_info = {
    .name = TYPE_CHARDEV_SPICEPORT,
    .parent = TYPE_CHARDEV_SPICE,
    .class_init = char_spiceport_class_init,
};
module_obj(TYPE_CHARDEV_SPICEPORT);

static void register_types(void)
{
    type_register_static(&char_spice_type_info);
    type_register_static(&char_spicevmc_type_info);
    type_register_static(&char_spiceport_type_info);
}

type_init(register_types);

module_dep("ui-spice-core");
