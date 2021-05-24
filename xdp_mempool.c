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
    struct bitmask           *oldmask = NULL;

    int    oldpolicy;
    int    ret = 0;
    int    numa_on = 0;
    int    i = 0;

     
    if (numa_node >= 0 && xdp_numa_check()) {
        xdp_numa_set(numa_node, &oldpolicy, &oldmask);    
        numa_on = 1;
    }

    while ((ops = mempool_ops[i++])) {
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
        xdp_numa_restore(oldpolicy, oldmask);
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
    void   *addr;

    if (pool->end - pool->last < size) {
        ERR_OUT("memory is not enough for size %lu", size);
        return NULL;
    }
    addr = pool->last;
    if (XDP_IS_POWER2(align)) {
        addr = (void *)XDP_ALIGN((uintptr_t)addr, align);
    } 

    if (pool->end - addr < size) {
        ERR_OUT("aligned addr memory is not enough for size %lu", size);
        return NULL;
    }
    pool->last = addr + size; 

    return addr;
}

void *
xdp_mempool_calloc(struct xdp_mempool *pool, size_t size, size_t n, 
    size_t align)
{
    void *addr;
    size_t total_size = size * n;

    addr = xdp_mempool_alloc(pool, total_size, align);
    memset(addr, 0, total_size);

    return addr;
}
