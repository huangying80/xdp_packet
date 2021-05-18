/*
 * huangying, email: hy_gzr@163.com
 */
#ifndef _XDP_RUNTIME_H_
#define _XDP_RUNTIME_H_

#include <stdint.h>

#include "xdp_framepool.h"
#include "xdp_eth.h"
#include "xdp_prog.h"
#include "xdp_worker.h"

#define xdp_runtime_likely(x) __builtin_expect(!!(x), 1)
#define xdp_runtime_unlikely(x) __builtin_expect(!!(x), 0)

#define XDP_PACKET_POLICY_DROP XDP_DROP
#define XDP_PACKET_POLICY_KERNEL XDP_PASS
#define XDP_PACKET_POLICY_DIRECT XDP_REDIRECT
#define XDP_PACKET_POLICY_FOWARD XDP_TX

#ifdef __cplusplus
extern "C" {
#endif
struct xdp_runtime {
    uint32_t     fill_size;
    uint32_t     comp_size;
    uint32_t     frame_size;
    uint32_t     frame_headroom;
    uint32_t     queue_size;
    uint32_t     queue_count;
    uint32_t     workers;
    int          eth_numa_node;
    int          numa_node;
    struct xdp_iface    iface;
    struct xdp_mempool *mempool;
    xdp_worker_func_t   worker_func;
};

int xdp_runtime_init(struct xdp_runtime *runtime,
    const char *ifname,
    const char *prog,
    const char *sec);

void xdp_runtime_release(struct xdp_runtime *runtime);

int xdp_runtime_setup_queue(struct xdp_runtime *runtime,
    size_t queue_count, size_t queue_size);
int xdp_runtime_setup_workers(struct xdp_runtime *runtime,
    xdp_worker_func_t worker_func,
    unsigned short worker_count);
int xdp_runtime_startup_workers(struct xdp_runtime *runtime);
void xdp_runtime_setup_size(struct xdp_runtime *runtime,
    uint32_t fill_size, uint32_t comp_size,
    uint32_t frame_size, uint32_t frame_headroom);
int xdp_runtime_setup_numa(struct xdp_runtime *runtime, int numa_node);
int xdp_runtime_setup_rss_ipv4(
    struct xdp_runtime *runtime, int protocol);
int xdp_runtime_setup_rss_ipv6(
    struct xdp_runtime *runtime, int protocol);

int xdp_runtime_tcp_packet(uint16_t port);
int xdp_runtime_tcp_drop(uint16_t port);

int xdp_runtime_udp_packet(uint16_t port);
int xdp_runtime_udp_drop(uint16_t port);

int xdp_runtime_l3_packet(uint16_t l3_protocal);
int xdp_runtime_l3_drop(uint16_t l3_protocal);

int xdp_runtime_l4_packet(uint16_t l4_protocal);
int xdp_runtime_l4_drop(uint16_t l4_protocal);
 
int xdp_runtime_ipv4_packet(const char *ip, uint32_t prefix, int type);
int xdp_runtime_ipv4_drop(const char *ip, uint32_t prefix, int type);

int xdp_runtime_ipv6_packet(const char *ip, uint32_t prefix, int type);
int xdp_runtime_ipv6_drop(const char *ip, uint32_t prefix, int type);
#ifdef __cplusplus
} 
#endif
#endif
