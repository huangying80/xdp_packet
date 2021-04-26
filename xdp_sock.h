/*
 * huangying, email: hy_gzr@163.com
 */
#ifndef _XDP_SOCK_H_
#define _XDP_SOCK_H_
#include <signal.h>
#include <poll.h>

#include "xdp_eth.h"
#include "xdp_dev.h"
#include "xdp_framepool.h"

#ifdef __cplusplus
extern "C" {
#endif
struct xdp_rx_queue;
struct xdp_tx_queue {
    struct xdp_umem_info    *umem;
    char pad1[0] XDP_CACHE_ALIGN;
    struct xsk_ring_prod     tx XDP_CACHE_ALIGN;
    char pad2[1] XDP_CACHE_ALIGN;
    struct xdp_rx_queue     *pair;
    int                      queue_index;
} XDP_CACHE_ALIGN;

struct xdp_rx_queue {
    struct xdp_umem_info    *umem;
    char pad1[0] XDP_CACHE_ALIGN;
    struct xsk_ring_cons     rx XDP_CACHE_ALIGN;
    char pad2[0] XDP_CACHE_ALIGN;
    struct xsk_socket       *xsk;
    struct xdp_framepool    *framepool;
    struct xdp_tx_queue     *pair;
    struct pollfd            fds[1];
    int                      queue_index;
} XDP_CACHE_ALIGN;
int xdp_sock_configure(struct xdp_iface *iface,
    struct xdp_rx_queue *rxq, uint32_t queue_size,
    struct xdp_umem_info_pool *umem_pool);
int xdp_sock_rx_zc(struct xdp_rx_queue *rxq, struct xdp_frame **bufs,
    uint16_t buf_len);
int xdp_sock_tx_zc(struct xdp_tx_queue *txq, struct xdp_frame **bufs,
    uint16_t buf_len);
#ifdef __cplusplus
} 
#endif
#endif
