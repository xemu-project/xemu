/*
 * COarse-grain LOck-stepping Virtual Machines for Non-stop Service (COLO)
 * (a.k.a. Fault Tolerance or Continuous Replication)
 *
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
#include "trace.h"
#include "colo.h"
#include "util.h"

uint32_t connection_key_hash(const void *opaque)
{
    const ConnectionKey *key = opaque;
    uint32_t a, b, c;

    /* Jenkins hash */
    a = b = c = JHASH_INITVAL + sizeof(*key);
    a += key->src.s_addr;
    b += key->dst.s_addr;
    c += (key->src_port | key->dst_port << 16);
    __jhash_mix(a, b, c);

    a += key->ip_proto;
    __jhash_final(a, b, c);

    return c;
}

int connection_key_equal(const void *key1, const void *key2)
{
    return memcmp(key1, key2, sizeof(ConnectionKey)) == 0;
}

int parse_packet_early(Packet *pkt)
{
    int network_length;
    static const uint8_t vlan[] = {0x81, 0x00};
    uint8_t *data = pkt->data;
    uint16_t l3_proto;
    ssize_t l2hdr_len;

    assert(data);

    /* Check the received vnet_hdr_len then add the offset */
    if ((pkt->vnet_hdr_len > sizeof(struct virtio_net_hdr_v1_hash)) ||
        (pkt->size < sizeof(struct eth_header) + sizeof(struct vlan_header) +
        pkt->vnet_hdr_len)) {
        /*
         * The received remote packet maybe misconfiguration here,
         * Please enable/disable filter module's the vnet_hdr flag at
         * the same time.
         */
        trace_colo_proxy_main_vnet_info("This received packet load wrong ",
                                        pkt->vnet_hdr_len, pkt->size);
        return 1;
    }
    data += pkt->vnet_hdr_len;

    l2hdr_len = eth_get_l2_hdr_length(data);

    /*
     * TODO: support vlan.
     */
    if (!memcmp(&data[12], vlan, sizeof(vlan))) {
        trace_colo_proxy_main("COLO-proxy don't support vlan");
        return 1;
    }

    pkt->network_header = data + l2hdr_len;

    const struct iovec l2vec = {
        .iov_base = (void *) data,
        .iov_len = l2hdr_len
    };
    l3_proto = eth_get_l3_proto(&l2vec, 1, l2hdr_len);

    if (l3_proto != ETH_P_IP) {
        return 1;
    }

    network_length = pkt->ip->ip_hl * 4;
    if (pkt->size < l2hdr_len + network_length + pkt->vnet_hdr_len) {
        trace_colo_proxy_main("pkt->size < network_header + network_length");
        return 1;
    }
    pkt->transport_header = pkt->network_header + network_length;

    return 0;
}

void extract_ip_and_port(uint32_t tmp_ports, ConnectionKey *key,
                         Packet *pkt, bool reverse)
{
    if (reverse) {
        key->src = pkt->ip->ip_dst;
        key->dst = pkt->ip->ip_src;
        key->src_port = ntohs(tmp_ports & 0xffff);
        key->dst_port = ntohs(tmp_ports >> 16);
    } else {
        key->src = pkt->ip->ip_src;
        key->dst = pkt->ip->ip_dst;
        key->src_port = ntohs(tmp_ports >> 16);
        key->dst_port = ntohs(tmp_ports & 0xffff);
    }
}

void fill_connection_key(Packet *pkt, ConnectionKey *key, bool reverse)
{
    uint32_t tmp_ports = 0;

    key->ip_proto = pkt->ip->ip_p;

    switch (key->ip_proto) {
    case IPPROTO_TCP:
    case IPPROTO_UDP:
    case IPPROTO_DCCP:
    case IPPROTO_ESP:
    case IPPROTO_SCTP:
    case IPPROTO_UDPLITE:
        tmp_ports = *(uint32_t *)(pkt->transport_header);
        break;
    case IPPROTO_AH:
        tmp_ports = *(uint32_t *)(pkt->transport_header + 4);
        break;
    default:
        break;
    }

    extract_ip_and_port(tmp_ports, key, pkt, reverse);
}

Connection *connection_new(ConnectionKey *key)
{
    Connection *conn = g_slice_new0(Connection);

    conn->ip_proto = key->ip_proto;
    conn->processing = false;
    conn->tcp_state = TCPS_CLOSED;
    g_queue_init(&conn->primary_list);
    g_queue_init(&conn->secondary_list);

    return conn;
}

void connection_destroy(void *opaque)
{
    Connection *conn = opaque;

    g_queue_foreach(&conn->primary_list, packet_destroy, NULL);
    g_queue_clear(&conn->primary_list);
    g_queue_foreach(&conn->secondary_list, packet_destroy, NULL);
    g_queue_clear(&conn->secondary_list);
    g_slice_free(Connection, conn);
}

Packet *packet_new(const void *data, int size, int vnet_hdr_len)
{
    Packet *pkt = g_slice_new0(Packet);

    pkt->data = g_memdup(data, size);
    pkt->size = size;
    pkt->creation_ms = qemu_clock_get_ms(QEMU_CLOCK_HOST);
    pkt->vnet_hdr_len = vnet_hdr_len;

    return pkt;
}

/*
 * packet_new_nocopy will not copy data, so the caller can't release
 * the data. And it will be released in packet_destroy.
 */
Packet *packet_new_nocopy(void *data, int size, int vnet_hdr_len)
{
    Packet *pkt = g_slice_new0(Packet);

    pkt->data = data;
    pkt->size = size;
    pkt->creation_ms = qemu_clock_get_ms(QEMU_CLOCK_HOST);
    pkt->vnet_hdr_len = vnet_hdr_len;

    return pkt;
}

void packet_destroy(void *opaque, void *user_data)
{
    Packet *pkt = opaque;

    g_free(pkt->data);
    g_slice_free(Packet, pkt);
}

void packet_destroy_partial(void *opaque, void *user_data)
{
    Packet *pkt = opaque;

    g_slice_free(Packet, pkt);
}

/*
 * Clear hashtable, stop this hash growing really huge
 */
void connection_hashtable_reset(GHashTable *connection_track_table)
{
    g_hash_table_remove_all(connection_track_table);
}

/* if not found, create a new connection and add to hash table */
Connection *connection_get(GHashTable *connection_track_table,
                           ConnectionKey *key,
                           GQueue *conn_list)
{
    Connection *conn = g_hash_table_lookup(connection_track_table, key);

    if (conn == NULL) {
        ConnectionKey *new_key = g_memdup(key, sizeof(*key));

        conn = connection_new(key);

        if (g_hash_table_size(connection_track_table) > HASHTABLE_MAX_SIZE) {
            trace_colo_proxy_main("colo proxy connection hashtable full,"
                                  " clear it");
            connection_hashtable_reset(connection_track_table);
            /*
             * clear the conn_list
             */
            while (conn_list && !g_queue_is_empty(conn_list)) {
                connection_destroy(g_queue_pop_head(conn_list));
            }
        }

        g_hash_table_insert(connection_track_table, new_key, conn);
    }

    return conn;
}

bool connection_has_tracked(GHashTable *connection_track_table,
                            ConnectionKey *key)
{
    Connection *conn = g_hash_table_lookup(connection_track_table, key);

    return conn ? true : false;
}
