/*
 * huangying, email: hy_gzr@163.com
 */
#ifndef _XDP_PROG_H_
#define _XDP_PROG_H_
#include <net/if.h>
#include <linux/limits.h>

#include "xdp_eth.h"

int xdp_prog_init(const char *ifname, const char *prog, const char *section);
void xdp_prog_release(void);

int xdp_prog_update_l3(uint16_t l3_proto, uint32_t action);
int xdp_prog_update_l4(uint8_t l4_proto, uint32_t action);
int xdp_prog_update_tcpport(uint16_t port, uint32_t action);
int xdp_prog_update_udpport(uint16_t port, uint32_t action);

#endif
