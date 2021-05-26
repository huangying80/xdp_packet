/*
 * huangying, email: hy_gzr@163.com
 */
#ifndef _XDP_PROG_H_
#define _XDP_PROG_H_
#include <net/if.h>
#include <linux/limits.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <arpa/inet.h>

#include "xdp_eth.h"

#ifdef __cplusplus
extern "C" {
#endif
enum {
    XDP_UPDATE_IP_SRC = 0,
    XDP_UPDATE_IP_DST
};

int xdp_prog_init(const char *ifname, const char *prog, const char *section);
void xdp_prog_release(void);

int xdp_prog_update_l3(uint16_t l3_proto, uint32_t action);
int xdp_prog_update_l4(uint8_t l4_proto, uint32_t action);
int xdp_prog_update_tcpport(uint16_t port, uint32_t action);
int xdp_prog_update_udpport(uint16_t port, uint32_t action);
int xdp_prog_update_ipv4(struct in_addr *addr,
    uint32_t prefix, int type, uint32_t action);
int xdp_prog_update_ipv6(struct in6_addr *addr,
    uint32_t prefix, int type, uint32_t action);
#ifdef __cplusplus
} 
#endif
#endif
