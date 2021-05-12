#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
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


#define XDP_NOSET (XDP_REDIRECT + 1)

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
    __u32    port;
    __u16    l3_proto;
    __u8     l4_proto;
};

XSKS_MAP_DEFINE = MAP_INIT (
    BPF_MAP_TYPE_XSKMAP,
    sizeof(int),
    sizeof(int),
    MAX_ETH_QUEUE_MAX
);


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
        pdebug("layer3 xdp aborted");
        return XDP_ABORTED;
    }
    
    cur->pos = eth + 1;
    cur->l3_proto = bpf_ntohs(eth->h_proto);
    pdebug("l3_proto %x, h_proto %x", cur->l3_proto, eth->h_proto);
    key = eth->h_proto;
    action = bpf_map_lookup_elem(MAP_REF(layer3), &key);
    if (!action) {
        return XDP_NOSET;
    }
    return *action;
}


union ipv4_key {
    __u32 b32[2];
    __u8  b8[8];
};
MAP_DEFINE(ipv4_src) = MAP_INIT_NO_PREALLOC ( 
    BPF_MAP_TYPE_LPM_TRIE,
    8,
    sizeof(__u32),
    MAX_LPM_IPV4_NUM
);
MAP_DEFINE(ipv4_dst) = MAP_INIT_NO_PREALLOC ( 
    BPF_MAP_TYPE_LPM_TRIE,
    8,
    sizeof(__u32),
    MAX_LPM_IPV4_NUM
);

XDP_ACTION_DEFINE(ipv4)
{
    int           hdrsize;
    __be32        src_ip = 0;
    __be32        dst_ip = 0;
    __u32         ret = XDP_NOSET;
    __u32        *action;

    struct iphdr       *iph = cur->pos;
    union ipv4_key      key;

    if (iph + 1 > (struct iphdr *)data_end) {
        pdebug("ipv4 xdp aborted");
        return XDP_ABORTED;
    }

    hdrsize = iph->ihl * 4;
    if(hdrsize < sizeof(struct iphdr)) {
        pdebug("ipv4 xdp aborted");
        return XDP_ABORTED;
    }

    if (cur->pos + hdrsize > data_end) {
        pdebug("ipv4 xdp aborted");
        return XDP_ABORTED;
    }

    src_ip = iph->saddr;
    key.b32[0] = 32;
    key.b8[4] = src_ip & 0xff;
    key.b8[5] = (src_ip >> 8) & 0xff;
    key.b8[6] = (src_ip >> 16) & 0xff;
    key.b8[7] = (src_ip >> 24) & 0xff;
    action = bpf_map_lookup_elem(MAP_REF(ipv4_src), &key);
    if (action) {
        ret = *action;
        pdebug("ip src action %u", ret);
        goto out;
    }

    dst_ip = iph->daddr;
    key.b32[0] = 32;
    key.b8[4] = dst_ip & 0xff;
    key.b8[5] = (dst_ip >> 8) & 0xff;
    key.b8[6] = (dst_ip >> 16) & 0xff;
    key.b8[7] = (dst_ip >> 24) & 0xff;
    action = bpf_map_lookup_elem(MAP_REF(ipv4_dst), &key);
    if (action) {
        ret = *action;
    }
    pdebug("ip dst action %u\n", ret);

out:
    cur->pos += hdrsize;
    cur->l4_proto = iph->protocol;
    pdebug("ipv4 l4_proto %u", cur->l4_proto);
    return ret;
}

struct key6 {
    __u32 prefixlen;
    struct in6_addr ipv6_addr;
};

MAP_DEFINE(ipv6_src) = MAP_INIT_NO_PREALLOC (
    BPF_MAP_TYPE_LPM_TRIE,
    20,
    sizeof(__u32),
    MAX_LPM_IPV6_NUM
);
MAP_DEFINE(ipv6_dst) = MAP_INIT_NO_PREALLOC (
    BPF_MAP_TYPE_LPM_TRIE,
    20,
    sizeof(__u32),
    MAX_LPM_IPV6_NUM
);
XDP_ACTION_DEFINE(ipv6)
{
    __u32    ret = XDP_NOSET;
    __u32   *action;

    struct key6     key;
    struct ipv6hdr *ip6h = cur->pos;

    if (ip6h + 1 > (struct ipv6hdr *)data_end) {
        pdebug("ipv6 src xdp aborted");
        return XDP_ABORTED;
    }

    key.prefixlen = 128,
    key.ipv6_addr = ip6h->saddr;
    action = bpf_map_lookup_elem(MAP_REF(ipv6_src), &key);
    if (action) {
        ret = *action;
        goto out;
    }

    key.prefixlen = 128,
    key.ipv6_addr = ip6h->daddr;
    action = bpf_map_lookup_elem(MAP_REF(ipv6_dst), &key);
    if (action) {
        ret = *action;
    }

out:
    cur->pos = ip6h + 1;
    cur->l4_proto = ip6h->nexthdr;
    return ret;
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
        return XDP_NOSET;
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
        pdebug("tcp port xdp aborted");
        return XDP_ABORTED;
    }
    
    len = h->doff << 2;
    if(len < sizeof(h)) {
        pdebug("tcp port xdp aborted");
        return XDP_ABORTED;
    }
    if (cur->pos + len > data_end) {
        pdebug("tcp port xdp aborted");
        return XDP_ABORTED;
    }
    cur->pos += len;
    
    cur->port = h->dest;
    action = bpf_map_lookup_elem(MAP_REF(tcp_port), &cur->port);
    if (!action) {
        return XDP_NOSET;
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
        pdebug("udp port xdp aborted");
        return XDP_ABORTED;
    }
    cur->pos  = h + 1;
    
    len = bpf_ntohs(h->len) - sizeof(struct udphdr);
    if (len < 0) {
        pdebug("udp port xdp aborted");
        return XDP_ABORTED;
    }
    
    cur->port = h->dest;
    pdebug("cur->port %u", cur->port);
    action = bpf_map_lookup_elem(MAP_REF(udp_port), &cur->port);
    if (!action) {
        return XDP_NOSET;
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
    pdebug("get a packet queue %d", ctx->rx_queue_index); 
    action = CALL_ACTION(layer3);
    pdebug("layer3 action %u, queue %d", action, ctx->rx_queue_index); 
    if (action != XDP_NOSET) {
        goto out;
    }

    switch(cur->l3_proto) {
        case ETH_P_IP:
            action = CALL_ACTION(ipv4);
            break;
        case ETH_P_IPV6:
            action = CALL_ACTION(ipv6);
            break;
        default:
            action = XDP_PASS;
            break;
    }
    pdebug("ip action %u, queue %d", action, ctx->rx_queue_index);
    if (action != XDP_NOSET) {
        goto out;
    }

    action = CALL_ACTION(layer4);
    pdebug("layer 4 action, queue %d", action, ctx->rx_queue_index); 
    if (action != XDP_NOSET) {
        goto out;
    }
    switch (cur->l4_proto) {
        case IPPROTO_UDP:
            action = CALL_ACTION(udp_port);
            pdebug("udp port %u, action %u", bpf_ntohs(cur->port), action); 
            break;
        case IPPROTO_TCP:
            action = CALL_ACTION(tcp_port);
            break;
        default:
            action = XDP_PASS;
            break;
    }
    pdebug("port action %u, queue %d", action, ctx->rx_queue_index); 

out:
    if (action == XDP_REDIRECT) {
        index = ctx->rx_queue_index;
        value = bpf_map_lookup_elem(XSKS_MAP_REF, &index);
        if (!value) {
            perror("oops, no queue matched for port %u in layer3 %x layer4 %x",
                bpf_ntohs(cur->port), cur->l3_proto, cur->l4_proto);
            return XDP_ABORTED;
        }
        action = bpf_redirect_map(XSKS_MAP_REF, index, 0);
        pdebug("value %d, queue %d, action %u", *value, index, action);
    }

    pdebug("final action %u----", action);
    return action;
}
XDP_SOCK_END
