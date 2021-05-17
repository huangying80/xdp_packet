/*
 * huangying-c, email: hy_gzr@163.com
 */
#ifndef _XDP_DEV_H_
#define _XDP_DEV_H_
#include "xsk.h"
#include "xdp_eth.h"
#include "xdp_framepool.h"
#include "linux/if_xdp.h"

#ifdef __cplusplus
extern "C" {
#endif

struct xdp_dev_info {
    int max_queues;
    int curr_queue;
    struct xdp_iface iface;

};

struct xdp_umem_info {
    struct xdp_framepool    *framepool;
    struct xsk_umem         *umem;
    char   pad1[0]  XDP_CACHE_ALIGN;
    struct xsk_ring_prod     fq XDP_CACHE_ALIGN;
    char   pad2[0]  XDP_CACHE_ALIGN;
    struct xsk_ring_cons     cq XDP_CACHE_ALIGN;
    void                    *buffer;
} XDP_CACHE_ALIGN;

struct xdp_umem_info_pool {
    size_t len;
    size_t last;
    struct xdp_umem_info start_addr[0];
} XDP_CACHE_ALIGN;

int xdp_dev_configure(struct xdp_mempool *pool,
    struct xdp_iface *iface, int queue_count);
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
size_t xdp_dev_queue_memsize(size_t queue_count);
#define xdp_dev_umem_info_pool_addr_memsize(n) \
    (xdp_dev_umem_info_pool_memsize(n) + XDP_CACHE_LINE)
#define xdp_dev_queue_addr_memsize(n) \
    (xdp_dev_queue_memsize(n) + (XDP_CACHE_LINE << 1))

#ifdef __cplusplus
} 
#endif

#endif
