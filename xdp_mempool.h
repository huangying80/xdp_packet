/*
 * huangying, email: hy_gzr@163.com
 */
#ifndef _XDP_MEMPOOL_H_
#define _XDP_MEMPOOL_H_
#include <stdint.h>
#include <stddef.h>

#ifndef XDP_CACHE_LINE
#define XDP_CACHE_LINE 64
#endif

#define XDP_ALIGN(_s, _m) (((_s) + (_m) - 1) & ~((_m) - 1))

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
} __attribute__((aligned(XDP_CACHE_LINE)));

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
void *xdp_mempool_calloc(struct xdp_mempool *pool, size_t size, size_t align);
#endif
