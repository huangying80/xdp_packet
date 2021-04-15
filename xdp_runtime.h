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
    struct xdp_iface    iface;
    struct xdp_mempool *mempool;
};

int xdp_runtime_init(struct xdp_runtime *runtime, const char *ifname);
void xdp_runtime_release(struct xdp_runtime *runtime);

int xdp_runtime_setup_queue(struct xdp_runtime *runtime,
    size_t queue_count, size_t queue_size);
int xdp_runtime_setup_workers(struct xdp_runtime *runtime,
    unsigned short worker_count);
void xdp_runtime_setup_size(struct xdp_runtime *runtime,
    uint32_t fill_size, uint32_t comp_size,
    uint32_t frame_size, uint32_t frame_headroom);

int xdp_runtime_write(struct xdp_frame **frames, uint16_t frame_count,
    uint16_t tx_queue_id);
int xdp_runtime_read(struct xdp_frame **frames, uint16_t frame_count,
    uint16_t rx_queue_id);

int xdp_runtime_tcp_packet(uint16_t port);
int xdp_runtime_tcp_drop(uint16_t port);

int xdp_runtime_udp_packet(uint16_t port);
int xdp_runtime_udp_drop(uint16_t port);

int xdp_runtime_l3_packet(uint16_t l3_protocal);
int xdp_runtime_l3_drop(uint16_t l3_protocal);

int xdp_runtime_l4_packet(uint16_t l4_protocal);
int xdp_runtime_l4_drop(uint16_t l4_protocal);
 
#ifdef __cplusplus
} 
#endif
#endif
