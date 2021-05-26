/*
 * huangying email: hy_gzr@163.com
 */
#ifndef _XDP_FRAMEPOOL_H_
#define _XDP_FRAMEPOOL_H_
#include "xdp_ring.h"
#include "xdp_mempool.h"

#ifdef __cplusplus
extern "C" {
#endif

#define xdp_frame_get_addr_offset(p, t, o) \
    ((t)((uint8_t *)(p) + (p)->data_off + (o)))
#define xdp_frame_get_addr(p, t) \
    xdp_frame_get_addr_offset(p, t, 0)

struct xdp_framepool;
struct xdp_frame {
    struct xdp_framepool *fpool;
    off_t    data_off;
    size_t   data_len;
} XDP_CACHE_ALIGN;

struct xdp_framepool {
    uint32_t            frame_size;
    uint32_t            frame_headroom;
    struct xdp_ring    *ring;
    size_t              count;
    size_t              comp_count;
    void               *base_addr;
} XDP_CACHE_ALIGN;


size_t xdp_framepool_memory_addr_size(uint32_t len, size_t frame_size,
    size_t frame_headroom);
struct xdp_framepool *xdp_framepool_create(struct xdp_mempool *pool,
    uint32_t len, uint32_t comp_len, size_t frame_size, size_t frame_headroom);
unsigned int xdp_framepool_get_frame(struct xdp_framepool *fp,
    struct xdp_frame **frame_list, size_t count);
unsigned int xdp_framepool_put_frame(struct xdp_framepool *fp,
    struct xdp_frame **frame_list, size_t count);
void xdp_framepool_free_frame(struct xdp_frame *frame);
#ifdef __cplusplus
} 
#endif
#endif
