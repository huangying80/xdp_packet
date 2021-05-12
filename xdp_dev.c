/*
 * huangying-c, email: hy_gzr@163.com
 */
#include <string.h>

#include "linux/if_xdp.h"

#include "xdp_dev.h"
#include "xdp_sock.h"
#include "xdp_log.h"

struct xdp_dev_conf {
    uint16_t                    queues;
    uint16_t                    max_queues;
    int                         configured;
    int                         numa_node;
    struct xdp_iface           *iface;
    struct  xdp_rx_queue       *rx_queue;
    struct  xdp_tx_queue       *tx_queue;
    struct  xdp_umem_info_pool *umem_pool;
};

static struct xdp_dev_conf xdp_dev;
static struct xdp_rx_queue *xdp_dev_rxq_create(
    struct xdp_mempool *pool, size_t queue_count);
static struct xdp_tx_queue *xdp_dev_txq_create(
    struct xdp_mempool *pool, size_t queue_count);

int
xdp_dev_configure(struct xdp_mempool *pool,
    struct xdp_iface *iface, int queue_count)
{
    int     max_queues;
    int     ret = -1;
    int     i;
    struct xdp_tx_queue *txq = NULL;
    struct xdp_rx_queue *rxq = NULL;
    struct xdp_umem_info_pool *umem_pool = NULL;

    memset(&xdp_dev, 0, sizeof(struct xdp_dev_conf));

    ret = xdp_eth_get_queue(iface->ifname, &max_queues, NULL);
    if (ret < 0) {
        goto out;
    }
    if (queue_count > max_queues || queue_count <= 0) {
        ERR_OUT("specified queue count error, max_queue %d, queue count %d",
            max_queues, queue_count);    
        goto out;
    }

    ret = xdp_eth_set_queue(iface->ifname, queue_count);
    if (ret < 0) {
       goto out; 
    }

    umem_pool = xdp_dev_umem_info_pool_create(pool, queue_count);
    if (!umem_pool) {
        goto out;
    }

    rxq = xdp_dev_rxq_create(pool, queue_count);
    if (!rxq) {
        ERR_OUT("allocate memory failed for rx queue");
        goto out;
    }

    txq = xdp_dev_txq_create(pool, queue_count);
    if (!rxq) {
        ERR_OUT("allocate memory failed for tx queue");
        goto out;
    }
    
    for (i = 0; i < queue_count; i++) {
        txq[i].pair = rxq + i;
        rxq[i].pair = txq + i;
        txq[i].queue_index = i;
        rxq[i].queue_index = i;
    }

    ret = 0;

out:
    if (ret < 0) {
        queue_count = 0; 
        max_queues = 0;
        if (rxq) {
            rxq = NULL;
        }
        if (txq) {
            txq = NULL;
        }
    }

    xdp_dev.max_queues = max_queues;
    xdp_dev.queues = queue_count;
    xdp_dev.rx_queue = rxq;
    xdp_dev.tx_queue = txq;
    xdp_dev.configured = !ret ? 1 : ret;
    xdp_dev.iface = iface;
    xdp_dev.numa_node = xdp_eth_numa_node(iface->ifname);
    xdp_dev.umem_pool = umem_pool;

    return ret;
}

int xdp_dev_queue_configure(uint16_t queue_id, uint32_t queue_size,
    struct xdp_framepool *fp)
{
    struct xdp_rx_queue *rxq;
    int     ret = -1;

    if (xdp_dev.configured != 1) {
        ERR_OUT("dev is not configured");
        goto out;
    }
    if (queue_id >= xdp_dev.queues) {
        ERR_OUT("queue id %u is overflow", queue_id);
        goto out;
    }
    rxq = xdp_dev.rx_queue + queue_id;
    rxq->framepool = fp;

    ret = xdp_sock_configure(xdp_dev.iface, rxq, queue_size,
        xdp_dev.umem_pool);
    if (ret < 0) {
        goto out;
    }
    ret = 0;
out:
    return ret;
}

int xdp_dev_get_info(const char *ifname, struct xdp_dev_info *info)
{
    int ret = 0;
    ret = xdp_eth_get_info(ifname, &info->iface);
    if (ret < 0) {
        goto out;
    }
    if (xdp_dev.configured != 1) {
        ret = -1;
        goto out;
    }
    info->max_queues = xdp_dev.max_queues;    
    info->curr_queue = xdp_dev.queues;

    ret = 0;

out:
    return ret;
}

inline int
xdp_dev_read(uint16_t rxq_id, struct xdp_frame **bufs, uint16_t bufs_count)
{
    struct xdp_rx_queue *rxq;
    rxq = xdp_dev.rx_queue + rxq_id;
    return xdp_sock_rx_zc(rxq, bufs, bufs_count);
}

inline int
xdp_dev_write(uint16_t txq_id, struct xdp_frame **bufs, uint16_t bufs_count)
{
    struct xdp_tx_queue *txq;
    txq = xdp_dev.tx_queue + txq_id;
    return xdp_sock_tx_zc(txq, bufs, bufs_count);
}

inline size_t xdp_dev_umem_info_pool_memsize(uint32_t n)
{
    size_t size;
    size = sizeof(struct xdp_umem_info_pool);
    size += sizeof(struct xdp_umem_info) * n;
    size = XDP_ALIGN(size, XDP_CACHE_LINE);
    size += XDP_CACHE_LINE;
    return size;
}
    

struct xdp_umem_info_pool *
xdp_dev_umem_info_pool_create(struct xdp_mempool *pool, uint32_t n)
{
    size_t size;
    struct xdp_umem_info_pool *umem_pool = NULL;
    size = xdp_dev_umem_info_pool_memsize(n);
    umem_pool = xdp_mempool_alloc(pool, size, XDP_CACHE_LINE);
    if (!umem_pool) {
        return NULL;
    }
    umem_pool->len = n;
    umem_pool->last = 0;
    return umem_pool;
}

struct xdp_umem_info *
xdp_dev_umem_info_alloc(struct xdp_umem_info_pool *umem_pool, size_t n)
{
    struct xdp_umem_info *umem = NULL;
    if (umem_pool->last + n > umem_pool->len) {
        return NULL;
    }
    umem = &umem_pool->start_addr[umem_pool->last];
    umem_pool->last += n;
    return umem;
}

struct xdp_umem_info *
xdp_dev_umem_info_calloc(struct xdp_umem_info_pool *umem_pool, size_t n)
{
    struct xdp_umem_info *umem = NULL;
    umem = xdp_dev_umem_info_alloc(umem_pool, n);
    memset(umem, 0, n * sizeof(struct xdp_umem_info));
    return umem;
}

inline size_t xdp_dev_queue_memsize(size_t queue_count)
{
    size_t tx_size;
    size_t rx_size;
    rx_size = sizeof(struct xdp_rx_queue) * queue_count;
    rx_size = XDP_ALIGN(rx_size, XDP_CACHE_LINE);
    rx_size += XDP_CACHE_LINE;

    tx_size = sizeof(struct xdp_tx_queue) * queue_count;
    tx_size = XDP_ALIGN(tx_size, XDP_CACHE_LINE);
    tx_size += XDP_CACHE_LINE;
    return tx_size + rx_size;
}

struct xdp_rx_queue *
xdp_dev_rxq_create(struct xdp_mempool *pool, size_t queue_count)
{
    struct xdp_rx_queue *rxq = NULL;
    size_t rx_size;
    rx_size = sizeof(struct xdp_rx_queue) * queue_count;
    rx_size = XDP_ALIGN(rx_size, XDP_CACHE_LINE);
    rx_size += XDP_CACHE_LINE;
    rxq = xdp_mempool_alloc(pool, rx_size, XDP_CACHE_LINE);
    if (rxq) {
        memset(rxq, 0, sizeof(struct xdp_rx_queue) * queue_count);
    }
    
    return rxq;
}

struct xdp_tx_queue *
xdp_dev_txq_create(struct xdp_mempool *pool, size_t queue_count)
{
    struct xdp_tx_queue *txq = NULL;
    size_t tx_size;
    tx_size = sizeof(struct xdp_tx_queue) * queue_count;
    tx_size = XDP_ALIGN(tx_size, XDP_CACHE_LINE);
    tx_size += XDP_CACHE_LINE;
    txq = xdp_mempool_alloc(pool, tx_size, XDP_CACHE_LINE);
    if (txq) {
        memset(txq, 0, sizeof(struct xdp_tx_queue) * queue_count);
    }

    return txq;
}

