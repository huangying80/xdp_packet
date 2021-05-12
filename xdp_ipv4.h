/*
 * huangying, email: hy_gzr@163.com
 */
#ifndef _XDP_IPV4_H_
#define _XDP_IPV4_H_
#include <stdint.h>
#include <linux/ip.h>

#include "xdp_net.h"

#ifdef __cplusplsu
extern "C" {
#endif
#define xdp_ipv4_likely(x) __builtin_expect(!!(x), 1)

static inline int xdp_ipv4_check_len(uint16_t hdr_len,
    uint16_t total_len, struct xdp_frame *frame) 
{
    return xdp_ipv4_likely(
        hdr_len == sizeof(struct iphdr)
        && total_len > hdr_len
        && frame->data_len >= sizeof(struct ethhdr) + (total_len));
}

static inline uint16_t xdp_ipv4_get_hdr_len(struct iphdr *hdr)
{
    return hdr->ihl << 2;
}
static inline uint16_t xdp_ipv4_get_total_len(struct iphdr *hdr)
{
    return xdp_ntohs(hdr->tot_len);
}

#ifdef __cplusplsu
}
#endif
#endif
