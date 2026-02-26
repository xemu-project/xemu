/*
 * QEMU libpcap network client
 *
 * Copyright (C) 2021 Matt Borgerson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "net/net.h"
#include "net/eth.h"
#include "net/clients.h"
#include "clients.h"
#include "system/system.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "qemu/iov.h"
#include "qemu/cutils.h"
#include "qemu/main-loop.h"
#include "net/pcap.h"

#if 0
#define LOG(fmt, ...) fprintf(stderr, "%s: " fmt "\n", __func__, ##__VA_ARGS__)
#else
#define LOG(...) do {} while (0)
#endif

typedef struct NetPcapState {
    NetClientState nc;
    char *ifname;
    pcap_t *p;
#ifdef WIN32
    HANDLE fd;
#else
    int fd;
    bool read_poll;
#endif
} NetPcapState;

static ssize_t net_pcap_receive(NetClientState *nc, const uint8_t *buf,
                                size_t size)
{
    NetPcapState *s = DO_UPCAST(NetPcapState, nc, nc);
    LOG("qemu->pcap %zd bytes...", size);

    if (pcap_sendpacket(s->p, buf, size)) {
        LOG("pcap_sendpacket failed!\n");
        return -1;
    }

    return size;
}

static void net_pcap_cleanup(NetClientState *nc)
{
    NetPcapState *s = DO_UPCAST(NetPcapState, nc, nc);
#if defined(_WIN32)
    qemu_del_wait_object(s->fd, NULL, NULL);
#endif
    pcap_close(s->p);
    free(s->ifname);
}

static NetClientInfo net_pcap_info = {
    .type = NET_CLIENT_DRIVER_PCAP,
    .size = sizeof(NetPcapState),
    .receive = net_pcap_receive,
    .cleanup = net_pcap_cleanup,
};

static void net_pcap_send(void *opaque)
{
    NetPcapState *s = opaque;
    struct pcap_pkthdr *pkt_header;
    const u_char *buf;
    uint8_t min_pkt[ETH_ZLEN];
    size_t min_pktsz = sizeof(min_pkt);

    int status = pcap_next_ex(s->p, &pkt_header, &buf);
    if (status == 1) {
        /* Success */
    } else if (status == 0) {
        /* Timeout */
        return;
    } else if (status == -1) {
        LOG("pcap_next_ex error: %s", pcap_geterr(s->p));
        return;
    } else {
        LOG("unknown pcap error %d\n", status);
        return;
    }

    size_t size = pkt_header->len;
    assert(size >= 14);
    assert(pkt_header->caplen == size);

    if (size > 0) {
        if (net_peer_needs_padding(&s->nc)) {
            if (eth_pad_short_frame(min_pkt, &min_pktsz, buf, size)) {
                buf = min_pkt;
                size = min_pktsz;
            }
        }

        LOG("pcap->qemu %zd bytes", size);
        qemu_send_packet(&s->nc, buf, size);
    }
}

#if !defined(_WIN32)
static void net_pcap_update_fd_handler(NetPcapState *s)
{
    qemu_set_fd_handler(s->fd, s->read_poll ? net_pcap_send : NULL, NULL, s);
}

static void net_pcap_read_poll(NetPcapState *s, bool enable)
{
    s->read_poll = enable;
    net_pcap_update_fd_handler(s);
}
#endif

int net_init_pcap(const Netdev *netdev, const char *name, NetClientState *peer,
                  Error **errp)
{
    const NetdevPcapOptions *pcap_opts = &netdev->u.pcap;
    NetClientState *nc;
    NetPcapState *s;
    char err[PCAP_ERRBUF_SIZE];
    const int promisc = 1;
    int status;

#ifdef WIN32
    if (pcap_load_library()) {
        error_setg(errp, "failed to load winpcap library");
        return -1;
    }
#endif

    pcap_t *p = pcap_open_live(pcap_opts->ifname, 65536, promisc, 1, err);
    if (p == NULL) {
        error_setg(errp, "failed to open interface '%s' for capture: %s",
                   pcap_opts->ifname, err);
        return -1;
    }

    status = pcap_set_datalink(p, DLT_EN10MB);
    if (status != 0) {
        error_setg(errp, "failed to set data link format to DLT_EN10MB");
        return -1;
    }

#ifdef WIN32
    pcap_setmintocopy(p, 40);
#endif

    nc = qemu_new_net_client(&net_pcap_info, peer, "pcap", name);
    s = DO_UPCAST(NetPcapState, nc, nc);
    s->ifname = strdup(pcap_opts->ifname);
    s->p = p;

    LOG("Initialized with interface %s", s->ifname);

#ifdef WIN32
    s->fd = pcap_getevent(p);
    qemu_add_wait_object(s->fd, net_pcap_send, s);
#else
    s->fd = pcap_get_selectable_fd(p);
    assert(s->fd >= 0);
    net_pcap_read_poll(s, true);
#endif

    return 0;
}
