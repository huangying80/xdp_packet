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
XDP_ACTION_DEFINE(ipv4_src)
{
    int           hdrsize;
    __u32        *action;
    __be32        src_ip = 0;
    struct iphdr *iph = cur->pos;
    union  ipv4_key key;

    if (iph + 1 > (struct iphdr *)data_end) {
        pdebug("ipv4 xdp aborted\n");
        return XDP_ABORTED;
    }

    hdrsize = iph->ihl * 4;
    if(hdrsize < sizeof(struct iphdr)) {
        pdebug("ipv4 xdp aborted\n");
        return XDP_ABORTED;
    }

    if (cur->pos + hdrsize > data_end) {
        pdebug("ipv4 xdp aborted\n");
        return XDP_ABORTED;
    }
    cur->pos += hdrsize;
    cur->l4_proto = iph->protocol;
    src_ip = iph->saddr;
    key.b32[0] = 32;
    key.b8[4] = src_ip & 0xff;
    key.b8[5] = (src_ip >> 8) & 0xff;
    key.b8[6] = (src_ip >> 16) & 0xff;
    key.b8[7] = (src_ip >> 24) & 0xff;
    action = bpf_map_lookup_elem(MAP_REF(ipv4_src), &key);
    if (!action) {
        pdebug("ipv4 src xdp pass\n");
        return XDP_PASS;
    }

    return *action;
}

MAP_DEFINE(ipv4_dst) = MAP_INIT_NO_PREALLOC ( 
    BPF_MAP_TYPE_LPM_TRIE,
    8,
    sizeof(__u32),
    MAX_LPM_IPV4_NUM
);
XDP_ACTION_DEFINE(ipv4_dst)
{
    int           hdrsize;
    __u32        *action;
    __be32        dst_ip = 0;
    struct iphdr *iph = cur->pos;
    union  ipv4_key key;

    if (iph + 1 > (struct iphdr *)data_end) {
        pdebug("ipv4 xdp aborted\n");
        return XDP_ABORTED;
    }

    hdrsize = iph->ihl * 4;
    if(hdrsize < sizeof(struct iphdr)) {
        pdebug("ipv4 xdp aborted\n");
        return XDP_ABORTED;
    }

    if (cur->pos + hdrsize > data_end) {
        pdebug("ipv4 xdp aborted\n");
        return XDP_ABORTED;
    }
    cur->pos += hdrsize;
    cur->l4_proto = iph->protocol;
    dst_ip = iph->daddr;
    key.b32[0] = 32;
    key.b8[4] = dst_ip & 0xff;
    key.b8[5] = (dst_ip >> 8) & 0xff;
    key.b8[6] = (dst_ip >> 16) & 0xff;
    key.b8[7] = (dst_ip >> 24) & 0xff;
    action = bpf_map_lookup_elem(MAP_REF(ipv4_dst), &key);
    if (!action) {
        pdebug("ipv4 src xdp pass\n");
        return XDP_PASS;
    }

    return *action;
}


MAP_DEFINE(ipv6_src) = MAP_INIT_NO_PREALLOC (
    BPF_MAP_TYPE_LPM_TRIE,
    20,
    sizeof(__u32),
    MAX_LPM_IPV6_NUM
);
XDP_ACTION_DEFINE(ipv6_src)
{
    struct ipv6hdr *ip6h = cur->pos;
    __u32          *action;

    if (ip6h + 1 > (struct ipv6hdr*)data_end) {
        pdebug("ipv6 src xdp aborted\n");
        return XDP_ABORTED;
    }
    cur->pos = ip6h + 1;
    cur->l4_proto = ip6h->nexthdr;
    struct {
        __u32 prefixlen;
        struct in6_addr ipv6_addr;
    }key6 = {
        .prefixlen = 128,
        .ipv6_addr = ip6h->saddr
    };
    action = bpf_map_lookup_elem(MAP_REF(ipv6_src), &key6);
    if (!action) {
        pdebug("ipv6 src xdp pass\n");
        return XDP_PASS;
    }

    return *action;
}

MAP_DEFINE(ipv6_dst) = MAP_INIT_NO_PREALLOC (
    BPF_MAP_TYPE_LPM_TRIE,
    20,
    sizeof(__u32),
    MAX_LPM_IPV6_NUM
);
XDP_ACTION_DEFINE(ipv6_dst)
{
    struct ipv6hdr *ip6h = cur->pos;
    __u32          *action;

    if (ip6h + 1 > (struct ipv6hdr*)data_end) {
        pdebug("ipv6 src xdp aborted\n");
        return XDP_ABORTED;
    }
    cur->pos = ip6h + 1;
    cur->l4_proto = ip6h->nexthdr;
    struct {
        __u32 prefixlen;
        struct in6_addr ipv6_addr;
    }key6 = {
        .prefixlen = 128,
        .ipv6_addr = ip6h->daddr
    };
    action = bpf_map_lookup_elem(MAP_REF(ipv6_dst), &key6);
    if (!action) {
        pdebug("ipv6 src xdp pass\n");
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
    __u32  action_src = XDP_PASS;
    __u32  action_dst = XDP_PASS;
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

    switch(cur->l3_proto) {
        case ETH_P_IP:
            action_src = CALL_ACTION(ipv4_src);
            if (action_src == XDP_DROP || action_src == XDP_ABORTED) {
                goto out;
            }
            action_dst = CALL_ACTION(ipv4_dst);
            if (action_dst == XDP_DROP || action_dst == XDP_ABORTED) {
                goto out;
            }
            break;
        case ETH_P_IPV6:
            action_src = CALL_ACTION(ipv6_src);
            if (action_src == XDP_DROP || action_src == XDP_ABORTED) {
                goto out;
            }
            action_dst = CALL_ACTION(ipv6_dst);
            if (action_dst == XDP_DROP || action_dst == XDP_ABORTED) {
                goto out;
            }
            break;
        default:
            action_src = XDP_PASS;
            action_dst = XDP_PASS;
            break;
    }
    if (action_dst == XDP_REDIRECT || action_src == XDP_REDIRECT) {
        action = XDP_REDIRECT;
    }
    if (action == XDP_REDIRECT) {
        index = ctx->rx_queue_index;
        value = bpf_map_lookup_elem(MAP_REF(xsks_map), &index);
        if (!value) {
            perror("oops, no queue matched for %d", cur->l3_proto);
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
