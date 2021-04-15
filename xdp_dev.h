/*
 * huangying-c, email: hy_gzr@163.com
 */
#ifndef _XDP_DEV_H_
#define _XDP_DEV_H_
#include "bpf/xsk.h"
#include "xdp_eth.h"
#include "xdp_framepool.h"

struct xdp_dev_info {
    int max_queues;
    int curr_queue;
    struct xdp_iface iface;

};

struct xdp_umem_info {
    struct xdp_framepool    *framepool;
    struct xsk_umem         *umem;
    struct xsk_ring_prod     fq;
    struct xsk_ring_cons     cq;
    void                    *buffer;
};

struct xdp_umem_info_pool {
    size_t len;
    size_t last;
    struct xdp_umem_info start_addr[0];
};

int xdp_dev_configure(struct xdp_mempool *pool,
    const char *ifname, int queue_count);
int xdp_dev_queue_configure(uint16_t queue_id, uint32_t queue_size,
    struct xdp_framepool *fp);
int xdp_dev_get_info(const char *ifname, struct xdp_dev_info *info);
int xdp_dev_read(uint16_t rxq_id, struct xdp_frame **bufs, uint16_t bufs_count);
int xdp_dev_write(uint16_t txq_id, struct xdp_frame **bufs, uint16_t bufs_count);
size_t xdp_dev_umem_info_pool_memsize(uint32_t n);
struct xdp_umem_info_pool *
xdp_dev_umem_info_pool_create(struct xdp_mempool *pool, uint32_t n);
struct xdp_umem_info *xdp_dev_umem_info_calloc(
    struct xdp_umem_info_pool *umem_pool, size_t n);

#endif
