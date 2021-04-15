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
struct xdp_iface {
    char ifname[IF_NAMESIZE];
    int  ifindex;
    char mac_addr[XDP_ETH_MAC_ADDR_LEN];
};

int xdp_eth_get_queue(const char *ifname, int *max_queue, int *curr_queue);
int xdp_eth_set_queue(const char *ifname, int queue);
int xdp_eth_get_info(const char *ifname, struct xdp_iface *iface);
int xdp_eth_numa_node(const char *ifname);
#ifdef __cplusplus
} 
#endif
#endif
