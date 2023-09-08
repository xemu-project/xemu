#include "qemu/osdep.h"
#include "qemu/cutils.h"
#include <termios.h>
#include "qapi/error.h"
#include "qemu/sockets.h"
#include "channel.h"
#include "cutils.h"

#ifdef CONFIG_SOLARIS
#include <stropts.h>
#endif

#define GA_CHANNEL_BAUDRATE_DEFAULT B38400 /* for isa-serial channels */

struct GAChannel {
    GIOChannel *listen_channel;
    GIOChannel *client_channel;
    GAChannelMethod method;
    GAChannelCallback event_cb;
    gpointer user_data;
};

static int ga_channel_client_add(GAChannel *c, int fd);

static gboolean ga_channel_listen_accept(GIOChannel *channel,
                                         GIOCondition condition, gpointer data)
{
    GAChannel *c = data;
    int ret, client_fd;
    bool accepted = false;

    g_assert(channel != NULL);

    client_fd = qemu_accept(g_io_channel_unix_get_fd(channel), NULL, NULL);
    if (client_fd == -1) {
        g_warning("error converting fd to gsocket: %s", strerror(errno));
        goto out;
    }
    qemu_socket_set_nonblock(client_fd);
    ret = ga_channel_client_add(c, client_fd);
    if (ret) {
        g_warning("error setting up connection");
        close(client_fd);
        goto out;
    }
    accepted = true;

out:
    /* only accept 1 connection at a time */
    return !accepted;
}

/* start polling for readable events on listen fd, new==true
 * indicates we should use the existing s->listen_channel
 */
static void ga_channel_listen_add(GAChannel *c, int listen_fd, bool create)
{
    if (create) {
        c->listen_channel = g_io_channel_unix_new(listen_fd);
    }
    g_io_add_watch(c->listen_channel, G_IO_IN, ga_channel_listen_accept, c);
}

static void ga_channel_listen_close(GAChannel *c)
{
    g_assert(c->listen_channel);
    g_io_channel_shutdown(c->listen_channel, true, NULL);
    g_io_channel_unref(c->listen_channel);
    c->listen_channel = NULL;
}

/* cleanup state for closed connection/session, start accepting new
 * connections if we're in listening mode
 */
static void ga_channel_client_close(GAChannel *c)
{
    g_assert(c->client_channel);
    g_io_channel_shutdown(c->client_channel, true, NULL);
    g_io_channel_unref(c->client_channel);
    c->client_channel = NULL;
    if (c->listen_channel) {
        ga_channel_listen_add(c, 0, false);
    }
}

static gboolean ga_channel_client_event(GIOChannel *channel,
                                        GIOCondition condition, gpointer data)
{
    GAChannel *c = data;
    gboolean client_cont;

    g_assert(c);
    if (c->event_cb) {
        client_cont = c->event_cb(condition, c->user_data);
        if (!client_cont) {
            ga_channel_client_close(c);
            return false;
        }
    }
    return true;
}

static int ga_channel_client_add(GAChannel *c, int fd)
{
    GIOChannel *client_channel;
    GError *err = NULL;

    g_assert(c && !c->client_channel);
    client_channel = g_io_channel_unix_new(fd);
    g_assert(client_channel);
    g_io_channel_set_encoding(client_channel, NULL, &err);
    if (err != NULL) {
        g_warning("error setting channel encoding to binary");
        g_error_free(err);
        return -1;
    }
    g_io_add_watch(client_channel, G_IO_IN | G_IO_HUP,
                   ga_channel_client_event, c);
    c->client_channel = client_channel;
    return 0;
}

static gboolean ga_channel_open(GAChannel *c, const gchar *path,
                                GAChannelMethod method, int fd, Error **errp)
{
    int ret;
    c->method = method;

    switch (c->method) {
    case GA_CHANNEL_VIRTIO_SERIAL: {
        assert(fd < 0);
        fd = qga_open_cloexec(
            path,
#ifndef CONFIG_SOLARIS
            O_ASYNC |
#endif
            O_RDWR | O_NONBLOCK,
            0
        );
        if (fd == -1) {
            error_setg_errno(errp, errno, "error opening channel '%s'", path);
            return false;
        }
#ifdef CONFIG_SOLARIS
        ret = ioctl(fd, I_SETSIG, S_OUTPUT | S_INPUT | S_HIPRI);
        if (ret == -1) {
            error_setg_errno(errp, errno, "error setting event mask for channel");
            close(fd);
            return false;
        }
#endif
#ifdef __FreeBSD__
        /*
         * In the default state channel sends echo of every command to a
         * client. The client programm doesn't expect this and raises an
         * error. Suppress echo by resetting ECHO terminal flag.
         */
        struct termios tio;
        if (tcgetattr(fd, &tio) < 0) {
            error_setg_errno(errp, errno, "error getting channel termios attrs");
            close(fd);
            return false;
        }
        tio.c_lflag &= ~ECHO;
        if (tcsetattr(fd, TCSAFLUSH, &tio) < 0) {
            error_setg_errno(errp, errno, "error setting channel termios attrs");
            close(fd);
            return false;
        }
#endif /* __FreeBSD__ */
        ret = ga_channel_client_add(c, fd);
        if (ret) {
            error_setg(errp, "error adding channel to main loop");
            close(fd);
            return false;
        }
        break;
    }
    case GA_CHANNEL_ISA_SERIAL: {
        struct termios tio;

        assert(fd < 0);
        fd = qga_open_cloexec(path, O_RDWR | O_NOCTTY | O_NONBLOCK, 0);
        if (fd == -1) {
            error_setg_errno(errp, errno, "error opening channel '%s'", path);
            return false;
        }
        tcgetattr(fd, &tio);
        /* set up serial port for non-canonical, dumb byte streaming */
        tio.c_iflag &= ~(IGNBRK | BRKINT | IGNPAR | PARMRK | INPCK | ISTRIP |
                         INLCR | IGNCR | ICRNL | IXON | IXOFF | IXANY |
                         IMAXBEL);
        tio.c_oflag = 0;
        tio.c_lflag = 0;
        tio.c_cflag |= GA_CHANNEL_BAUDRATE_DEFAULT;
        /* 1 available byte min or reads will block (we'll set non-blocking
         * elsewhere, else we have to deal with read()=0 instead)
         */
        tio.c_cc[VMIN] = 1;
        tio.c_cc[VTIME] = 0;
        /* flush everything waiting for read/xmit, it's garbage at this point */
        tcflush(fd, TCIFLUSH);
        tcsetattr(fd, TCSANOW, &tio);
        ret = ga_channel_client_add(c, fd);
        if (ret) {
            error_setg(errp, "error adding channel to main loop");
            close(fd);
            return false;
        }
        break;
    }
    case GA_CHANNEL_UNIX_LISTEN: {
        if (fd < 0) {
            fd = unix_listen(path, errp);
            if (fd < 0) {
                return false;
            }
        }
        ga_channel_listen_add(c, fd, true);
        break;
    }
    case GA_CHANNEL_VSOCK_LISTEN: {
        if (fd < 0) {
            SocketAddress *addr;
            char *addr_str;

            addr_str = g_strdup_printf("vsock:%s", path);
            addr = socket_parse(addr_str, errp);
            g_free(addr_str);
            if (!addr) {
                return false;
            }

            fd = socket_listen(addr, 1, errp);
            qapi_free_SocketAddress(addr);
            if (fd < 0) {
                return false;
            }
        }
        ga_channel_listen_add(c, fd, true);
        break;
    }
    default:
        error_setg(errp, "error binding/listening to specified socket");
        return false;
    }

    return true;
}

GIOStatus ga_channel_write_all(GAChannel *c, const gchar *buf, gsize size)
{
    GError *err = NULL;
    gsize written = 0;
    GIOStatus status = G_IO_STATUS_NORMAL;

    while (size) {
        g_debug("sending data, count: %d", (int)size);
        status = g_io_channel_write_chars(c->client_channel, buf, size,
                                          &written, &err);
        if (status == G_IO_STATUS_NORMAL) {
            size -= written;
            buf += written;
        } else if (status != G_IO_STATUS_AGAIN) {
            g_warning("error writing to channel: %s", err->message);
            return status;
        }
    }

    do {
        status = g_io_channel_flush(c->client_channel, &err);
    } while (status == G_IO_STATUS_AGAIN);

    if (status != G_IO_STATUS_NORMAL) {
        g_warning("error flushing channel: %s", err->message);
    }

    return status;
}

GIOStatus ga_channel_read(GAChannel *c, gchar *buf, gsize size, gsize *count)
{
    return g_io_channel_read_chars(c->client_channel, buf, size, count, NULL);
}

GAChannel *ga_channel_new(GAChannelMethod method, const gchar *path,
                          int listen_fd, GAChannelCallback cb, gpointer opaque)
{
    Error *err = NULL;
    GAChannel *c = g_new0(GAChannel, 1);
    c->event_cb = cb;
    c->user_data = opaque;

    if (!ga_channel_open(c, path, method, listen_fd, &err)) {
        g_critical("%s", error_get_pretty(err));
        error_free(err);
        ga_channel_free(c);
        return NULL;
    }

    return c;
}

void ga_channel_free(GAChannel *c)
{
    if (c->listen_channel) {
        ga_channel_listen_close(c);
    }
    if (c->client_channel) {
        ga_channel_client_close(c);
    }
    g_free(c);
}
