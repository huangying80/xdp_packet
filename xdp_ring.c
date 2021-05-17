/*
 * huangying email: hy_gzr@163.com
 */
#include "xdp_ring.h"
inline size_t xdp_ring_memory_size(uint32_t count)
{
    size_t size;
    if (!XDP_IS_POWER2(count)) {
        return 0;
    }
    size = sizeof(struct xdp_ring) + count * sizeof(void *);
    size = XDP_ALIGN(size, XDP_CACHE_LINE);
    return size;
}

struct xdp_ring *
xdp_ring_create(struct xdp_mempool *pool, uint32_t count, int flags)
{
    struct xdp_ring *ring = NULL;
    void            *addr;
    size_t           size;
    uint32_t         n;
    
    n = xdp_align_pow2_32(count);
    size = xdp_ring_memory_size(n);
    if (!size) {
        return NULL;
    }
    addr = xdp_mempool_alloc(pool, size, XDP_CACHE_LINE);
    if (!addr) {
        return NULL;
    }
    ring = (struct xdp_ring *)addr;
   
    ring->size = n;
    ring->mask = n - 1;
    ring->capacity = count;
    ring->prod.head = ring->prod.tail = 0;
    ring->cons.head = ring->cons.tail = 0;
    if (flags & XDP_RING_SP_ENQ) {
        ring->prod.single = 1;
    } else {
        ring->prod.single = 0;
    }
    if (flags & XDP_RING_SC_DEQ) {
        ring->cons.single = 1;
    } else {
        ring->cons.single = 0;
    }

    return ring;
}

