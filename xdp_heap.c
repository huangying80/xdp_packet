/*
 * huangying email: hy_gzr@163.com
 */
#include <sys/mman.h>
#include <errno.h>

#include "xdp_heap.h"
#include "xdp_mempool.h"
#include "xdp_log.h"

static int xdp_heap_init(__attribute__((unused))void *data);
static int xdp_heap_release(__attribute__((unused))void *data);
static struct xdp_mempool *xdp_heap_reserve(int node, size_t size,
    __attribute__((unused))void *data);
static int xdp_heap_revert(struct xdp_mempool *pool,
    __attribute__((unused))void *data);

struct xdp_mempool_ops heap_mempool_ops = {
    .private_data = NULL,
    .init = xdp_heap_init,
    .release = xdp_heap_release,
    .reserve = xdp_heap_reserve,
    .revert = xdp_heap_revert,
};

inline struct xdp_mempool_ops *xdp_heap_get_ops(void)
{
    return &heap_mempool_ops;
}

static int xdp_heap_init(__attribute__((unused))void *data)
{
    return 0;
}

static int xdp_heap_release(__attribute__((unused))void *data)
{
    return 0;
}

static struct xdp_mempool *
xdp_heap_reserve(int node, size_t size, __attribute__((unused))void *data)
{
    size_t  reserve_size;
    struct xdp_mempool *pool = NULL;
    void   *addr = NULL;

    reserve_size = (size_t)XDP_ALIGN(size + sizeof(struct xdp_mempool), XDP_PAGESIZE);
    //reserve_size = 1 << 30;
    addr = mmap(NULL, reserve_size, PROT_READ|PROT_WRITE,
        MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (!addr || addr == MAP_FAILED) {
        ERR_OUT("mmap failed, err: %d", errno);    
        return NULL;
    }
    pool = addr;
    pool->total_size = reserve_size;
    pool->start = addr + sizeof(struct xdp_mempool);
    pool->end = addr + reserve_size;
    pool->last = pool->start;
    pool->size = pool->end - pool->last;
    pool->numa_node = node;
    pool->ops_index = XDP_DEFAULT_OPS;
    return pool;
}

static int
xdp_heap_revert(struct xdp_mempool *pool, __attribute__((unused))void *data)
{
    return munmap(pool, pool->total_size);
}
