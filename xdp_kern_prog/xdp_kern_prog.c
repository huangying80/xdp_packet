#include <linux/if_ether.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/in.h>

#include "linux/bpf.h"
#include "bpf_endian.h"
#include "bpf_helpers.h"

#include "xdp_kern_prog.h"

#ifdef KERN_DEBUG
#define pdebug bpf_printk
#else
#define pdebug(fmt, ...)
#endif
#define perror bpf_printk

#define CALL_ACTION(_type) XDP_ACTION_CALL(_type, cur, data_end)

#define XDP_SOCK_START  SEC("xdp_sock")
#define XDP_SOCK_END    char _license[] SEC("license") = "GPL";


#ifndef MAX_STATIS_NUM
#define MAX_STATIS_NUM (XDP_REDIRECT + 1)
#endif

#ifndef MAX_PORT_NUM
#define MAX_PORT_NUM (1024)
#endif

#ifndef MAX_LAYER3_NUM
#define MAX_LAYER3_NUM (16)
#endif

#ifndef MAX_LAYER4_NUM
#define MAX_LAYER4_NUM (8)
#endif

#ifndef MAX_ETH_QUEUE_MAX
#define MAX_ETH_QUEUE_MAX (128)
#endif

struct hdr_cursor {
    void    *pos;
    __u64    size;
    __u16    port;
    __u16    l3_proto;
    __u8     l4_proto;
};

MAP_DEFINE(xsks_map) = {
    BPF_MAP_TYPE_XSKMAP,
    sizeof(int),
    sizeof(int),
    MAX_ETH_QUEUE_MAX
};

MAP_DEFINE(layer3) = MAP_INIT (
    BPF_MAP_TYPE_PERCPU_HASH,
    sizeof(__u32),
    sizeof(__u32),
    MAX_LAYER3_NUM
);

XDP_ACTION_DEFINE(layer3)
{
    __u32            key;
    __u32           *action;
    struct ethhdr   *eth;
    
    eth = cur->pos;
    if (eth + 1 > (struct ethhdr *)data_end) {
        pdebug("layer3 xdp aborted\n");
        return XDP_ABORTED;
    }
    
    cur->pos = eth + 1;
    cur->l3_proto = bpf_ntohs(eth->h_proto);
    key = eth->h_proto;
    action = bpf_map_lookup_elem(MAP_REF(layer3), &key);
    if (!action) {
        return XDP_PASS;
    }
    return *action;
}

MAP_DEFINE(layer4) = MAP_INIT (
    BPF_MAP_TYPE_PERCPU_HASH,
    sizeof(__u32),
    sizeof(__u32),
    MAX_LAYER4_NUM
);

XDP_ACTION_DEFINE(layer4) //check tcp or udp
{
    __u32    key;
    __u32   *action;

    key = cur->l4_proto;
    action = bpf_map_lookup_elem(MAP_REF(layer4), &key);
    if (!action) {
        return XDP_PASS;
    }
    return *action;
}

MAP_DEFINE(tcp_port) = MAP_INIT (
    BPF_MAP_TYPE_PERCPU_HASH,
    sizeof(__u32),
    sizeof(__u32),
    MAX_PORT_NUM
);

XDP_ACTION_DEFINE(tcp_port)
{
    int       len;
    __u32    *action;

    struct tcphdr *h = cur->pos;
    
    if (h + 1 > (struct tcphdr *)data_end) {
        pdebug("tcp port xdp aborted\n");
        return XDP_ABORTED;
    }
    
    len = h->doff << 2;
    if(len < sizeof(h)) {
        pdebug("tcp port xdp aborted\n");
        return XDP_ABORTED;
    }
    if (cur->pos + len > data_end) {
        pdebug("tcp port xdp aborted\n");
        return XDP_ABORTED;
    }
    cur->pos += len;
    
    cur->port = h->dest;
    action = bpf_map_lookup_elem(MAP_REF(tcp_port), &cur->port);
    if (!action) {
        return XDP_PASS;
    }
    return *action;
}

MAP_DEFINE(udp_port) = MAP_INIT (
    BPF_MAP_TYPE_PERCPU_HASH,
    sizeof(__u32),
    sizeof(__u32),
    MAX_PORT_NUM
);

XDP_ACTION_DEFINE(udp_port)
{
    __u32   *action;
    int      len;
    struct udphdr *h = cur->pos;
    
    if (h + 1 > (struct udphdr *)data_end) {
        pdebug("udp port xdp aborted\n");
        return XDP_ABORTED;
    }
    cur->pos  = h + 1;
    
    len = bpf_ntohs(h->len) - sizeof(struct udphdr);
    if (len < 0) {
        pdebug("udp port xdp aborted\n");
        return XDP_ABORTED;
    }
    
    cur->port = h->dest;
    action = bpf_map_lookup_elem(MAP_REF(udp_port), &cur->port);
    if (!action) {
        return XDP_PASS;
    }
    return *action;
}

XDP_SOCK_START
int xdp_sock_main(struct xdp_md *ctx)
{
    __u32  action = XDP_PASS;
    int   *value;
    int    index;

    struct hdr_cursor   cursor = {0, 0, 0, 0, 0};
    struct hdr_cursor  *cur;

    void *data = (void *)(long)ctx->data;
    void *data_end = (void *)(long)ctx->data_end;
    
    cursor.pos = data;
    cursor.size = data_end - data;
    cur = &cursor;
    
    action = CALL_ACTION(layer3);
    if (action == XDP_REDIRECT) {
        index = ctx->rx_queue_index;
        value = bpf_map_lookup_elem(MAP_REF(xsks_map), &index);
        if (!value) {
            perror("oops, no queue matched for %d in layer3", cur->l3_proto);
            return XDP_ABORTED;
        }
        return bpf_redirect_map(MAP_REF(xsks_map), index, 0);
    }
    if (action != XDP_PASS) {
        goto out;
    }

    action = CALL_ACTION(layer4);
    if (action == XDP_REDIRECT) {
        index = ctx->rx_queue_index;
        value = bpf_map_lookup_elem(MAP_REF(xsks_map), &index);
        if (!value) {
            perror("oops, no queue matched for %d in layer4", cur->l4_proto);
            return XDP_ABORTED;
        }
        return bpf_redirect_map(MAP_REF(xsks_map), index, 0);
    }
    
    if (action != XDP_PASS) {
        goto out;
    }
    switch (cur->l4_proto) {
        case IPPROTO_UDP:
            action = CALL_ACTION(udp_port);
            break;
        case IPPROTO_TCP:
            action = CALL_ACTION(tcp_port);
            break;
        default:
            action = XDP_PASS;
            break;
    }
    if (action == XDP_REDIRECT) {
        index = ctx->rx_queue_index;
        value = bpf_map_lookup_elem(MAP_REF(xsks_map), &index);
        if (!value) {
            perror("oops, no queue matched for port %u in layer3 %u layer4 %u",
                bpf_ntohs(cur->port), cur->l3_proto, cur->l4_proto);
            return XDP_ABORTED;
        }
        return bpf_redirect_map(MAP_REF(xsks_map), index, 0);
    }

out:
    return action;
}
XDP_SOCK_END
