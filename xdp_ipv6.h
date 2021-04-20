#ifndef _XDP_IPV6_H_
#define _XDP_IPV6_H_
#ifdef __cplusplus

#include "xdp_framepool.h"

extern "C" {
#endif
#define xdp_ipv6_likely(x) __builtin_expect(!!(x), 1)

static inline int xdp_ipv6_check_len(struct ipv6hdr *hdr,
    struct xdp_frame *frame)
{
    return xdp_ipv6_likely(frame->data_len
        >= sizeof(struct ethhdr) + (hdr->payload_len));
}

#ifdef __cplusplus
}
#endif
#endif
