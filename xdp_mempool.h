/*
 * huangying, email: hy_gzr@163.com
 */
#ifndef _XDP_MEMPOOL_H_
#define _XDP_MEMPOOL_H_
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef XDP_CACHE_LINE
#define XDP_CACHE_LINE 64
#endif

#define XDP_CACHE_ALIGN __attribute__((aligned(XDP_CACHE_LINE)))
#define XDP_ALIGN(_s, _m) (((_s) + (_m) - 1) & ~((_m) - 1))
#define XDP_IS_POWER2(n) ((n) && !(((n) - 1) & (n)))
#define XDP_PAGESIZE 4096

enum {
    XPD_HUGEPAGE_OPS = 0,
    XDP_DEFAULT_OPS,
};

struct xdp_mempool {
    void            *start;
    void            *end;
    void            *last;
    size_t           size;
    size_t           total_size;
    int              numa_node;
    int              ops_index;
} XDP_CACHE_ALIGN;

struct xdp_mempool_ops {
    void    *private_data;
    int (*init)     (void *);
    int (*release)  (void *);
    struct xdp_mempool* (*reserve) (int, size_t, void *);
    int  (*revert) (struct xdp_mempool *, void *);
};

struct xdp_mempool *xdp_mempool_create(int numa_node, size_t size);
void xdp_mempool_release(struct xdp_mempool *pool);
void *xdp_mempool_alloc(struct xdp_mempool *pool, size_t size, size_t align);
void *xdp_mempool_calloc(struct xdp_mempool *pool, size_t size, size_t n,
    size_t align);

static inline uint32_t xdp_combine_ms1b_32(uint32_t x)
{   
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;

    return x;
}

static inline uint32_t xdp_align_pow2_32(uint32_t x)
{   
    x--;
    x = xdp_combine_ms1b_32(x);
    return x + 1;
}

static inline uint64_t xdp_combine_ms1b_64(uint64_t v)
{   
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;

    return v;
}

static inline uint64_t xdp_align_pow2_64(uint64_t v)
{
    v--;
    v = xdp_combine_ms1b_64(v);
    return v + 1;
}

#ifdef __cplusplus
} 
#endif
#endif
