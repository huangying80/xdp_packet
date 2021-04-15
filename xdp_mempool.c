/*
 * huangying email: hy_gzr@163.com
 */
#include <string.h>

#include "xdp_numa.h"
#include "xdp_mempool.h"
#include "xdp_heap.h"
#include "xdp_hugepage.h"
#include "xdp_log.h"

static struct xdp_mempool_ops *mempool_ops[] = {
    &hugepage_mempool_ops,
    &heap_mempool_ops,
    NULL};

struct xdp_mempool* xdp_mempool_create(int numa_node, size_t size)
{
    struct xdp_mempool_ops   *ops = NULL;
    struct xdp_mempool       *pool = NULL;
    struct bitmask            oldmask;

    int    oldpolicy;
    int    ret = 0;
    int    numa_on = 0;

     
    if (numa_node >= 0 && xdp_numa_check()) {
        xdp_numa_set(numa_node, &oldpolicy, &oldmask);    
        numa_on = 1;
    }

    for (ops = mempool_ops[0]; ops; ops++) {
        ret = ops->init(ops->private_data);
        if (ret < 0) {
            continue;
        }
        pool = ops->reserve(numa_node, size, ops->private_data);
        if (pool) {
            break;
        }
        ops->release(ops->private_data);
    }

    if (numa_on) {
        xdp_numa_restore(oldpolicy, &oldmask);
    }
    return pool;
}

void xdp_mempool_release(struct xdp_mempool *pool)
{
    struct xdp_mempool_ops *ops;
    if (!pool) {
        return;
    }
    ops = mempool_ops[pool->ops_index];
    ops->revert(pool, ops->private_data);
    ops->release(ops->private_data);
}

void *xdp_mempool_alloc(struct xdp_mempool *pool, size_t size, size_t align)
{
    void *addr;
    
    if (size & (size - 1)) {
        ERR_OUT("size %lu is error", size);
        return NULL;
    }
    if (pool->last + size >= pool->end) {
        ERR_OUT("memory is not enough for size %lu", size);
        return NULL;
    }
    if (!align) {
        align = XDP_CACHE_LINE;
    }
    addr = (void *)XDP_ALIGN((size_t)pool->last, align);
    pool->last = addr + size; 

    return addr;
}

void *xdp_mempool_calloc(struct xdp_mempool *pool, size_t size, size_t align)
{
    void *addr;

    addr = xdp_mempool_alloc(pool, size, align);
    memset(addr, 0, size);

    return addr;
}
