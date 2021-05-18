/*
 * huangying email: hy_gzr@163.com
 */
#ifndef _XDP_ETH_H_
#define _XDP_ETH_H_
#include <net/if.h>

#define XDP_ETH_MAC_ADDR_LEN (6)

#ifdef __cplusplus
extern "C" {
#endif

#define XDP_TCP_V4_FLOW 0x01
#define XDP_UDP_V4_FLOW 0x02
#define XDP_TCP_V6_FLOW 0x05
#define XDP_UDP_V6_FLOW 0x06
#define XDP_IPV4_FLOW   0x10
#define XDP_IPV6_FLOW   0x11
#define XDP_FLOW_RSS    0x20000000

#define XDP_RXH_IP_SRC      (1 << 4)
#define XDP_RXH_IP_DST      (1 << 5)
#define XDP_RXH_PORT_SRC    (1 << 6)
#define XDP_RXH_PORT_DST    (1 << 7)

#define XDP_RXH_LAYER4 \
    (XDP_RXH_IP_SRC | XDP_RXH_IP_DST | XDP_RXH_PORT_SRC | XDP_RXH_PORT_DST)

#define XDP_RXH_DISCARD     (1 << 31)



struct xdp_iface {
    char ifname[IF_NAMESIZE];
    int  ifindex;
    char mac_addr[XDP_ETH_MAC_ADDR_LEN];
};

int xdp_eth_get_queue(const char *ifname, int *max_queue, int *curr_queue);
int xdp_eth_set_queue(const char *ifname, int queue);
int xdp_eth_get_info(const char *ifname, struct xdp_iface *iface);
int xdp_eth_numa_node(const char *ifname);
uint32_t xdp_eth_get_rss(const char *ifname, uint32_t flow_type);
int xdp_eth_set_rss(const char *ifname, uint32_t flow_type, uint32_t set_key);

#ifdef __cplusplus
} 
#endif
#endif
