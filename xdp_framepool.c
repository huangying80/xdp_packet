/*
 * huangying email: hy_gzr@163.com
 */
#include <stdlib.h>

#include "xdp_log.h"
#include "xdp_mempool.h"
#include "xdp_ring.h"
#include "xdp_framepool.h"

#define XDP_FRAMEPOOL_HDR_SIZE(s, l) \
l = XDP_ALIGN(l, XDP_CACHE_LINE);    \
s = sizeof(struct xdp_framepool) + l * sizeof(struct xdp_frame); \
s = XDP_ALIGN(s, XDP_CACHE_LINE)

inline struct xdp_frame *
xdp_framepool_addr_to_frame(struct xdp_framepool *fpool, void *addr)
{
    return fpool->frame + (addr - fpool->base_addr);
}

inline size_t
xdp_framepool_memory_size(uint32_t len, size_t frame_size,
    size_t frame_headroom)
{
    size_t size;
    size_t ring_memory_size;

    XDP_FRAMEPOOL_HDR_SIZE(size, len);
    
    ring_memory_size = xdp_ring_memory_size(len);
    if (!ring_memory_size) {
        return 0;
    }
    size += ring_memory_size;
    size = XDP_ALIGN(size, XDP_PAGESIZE);
    size += (frame_size + frame_headroom) * len;

    return size;
}

struct xdp_framepool *
xdp_framepool_create(struct xdp_mempool *pool, uint32_t len,
    size_t frame_size, size_t frame_headroom)
{
    int      i = 0;
    unsigned int n;
    size_t    per_size;
    size_t    ring_size;
    size_t    size;
    void     *addr;
    struct xdp_frame    **frame_list;
    struct xdp_ring      *ring = NULL;
    struct xdp_framepool *framepool = NULL;

    XDP_FRAMEPOOL_HDR_SIZE(size, len);
    framepool = xdp_mempool_calloc(pool, size, XDP_CACHE_LINE);
    if (!framepool) {
        return NULL;
    }

    ring = xdp_ring_create(pool, len, XDP_RING_SP_ENQ | XDP_RING_SC_DEQ);
    if (!ring) {
        return NULL;
    }
    per_size = frame_size + frame_headroom;
    ring_size = xdp_ring_get_size(ring);
    size = ring_size * per_size;
    addr = xdp_mempool_alloc(pool, size, XDP_PAGESIZE);
    if (!addr) {
        return NULL;
    }
    frame_list = malloc(sizeof(struct xdp_frame *));
    switch (ring_size % 8) {
        case 0:
            while (i != ring_size) {
                frame_list[i] = framepool->frame + i;
                framepool->frame[i].fpool = framepool;
                framepool->frame[i].addr = (uint8_t *)addr + per_size * i;
                framepool->frame[i].data_off = 0;
                framepool->frame[i].data_len = 0;
                i++;
        case 7:
                frame_list[i] = framepool->frame + i;
                framepool->frame[i].fpool = framepool;
                framepool->frame[i].addr = (uint8_t *)addr + per_size * i;
                framepool->frame[i].data_off = 0;
                framepool->frame[i].data_len = 0;
                i++;
        case 6:
                frame_list[i] = framepool->frame + i;
                framepool->frame[i].fpool = framepool;
                framepool->frame[i].addr = (uint8_t *)addr + per_size * i;
                framepool->frame[i].data_off = 0;
                framepool->frame[i].data_len = 0;
                i++;
        case 5:
                frame_list[i] = framepool->frame + i;
                framepool->frame[i].fpool = framepool;
                framepool->frame[i].addr = (uint8_t *)addr + per_size * i;
                framepool->frame[i].data_off = 0;
                framepool->frame[i].data_len = 0;
                i++;
        case 4:
                frame_list[i] = framepool->frame + i;
                framepool->frame[i].fpool = framepool;
                framepool->frame[i].addr = (uint8_t *)addr + per_size * i;
                framepool->frame[i].data_off = 0;
                framepool->frame[i].data_len = 0;
                i++;
        case 3:
                frame_list[i] = framepool->frame + i;
                framepool->frame[i].fpool = framepool;
                framepool->frame[i].addr = (uint8_t *)addr + per_size * i;
                framepool->frame[i].data_off = 0;
                framepool->frame[i].data_len = 0;
                i++;
        case 2:
                frame_list[i] = framepool->frame + i;
                framepool->frame[i].fpool = framepool;
                framepool->frame[i].addr = (uint8_t *)addr + per_size * i;
                framepool->frame[i].data_off = 0;
                framepool->frame[i].data_len = 0;
                i++;
        case 1:
                frame_list[i] = framepool->frame + i;
                framepool->frame[i].fpool = framepool;
                framepool->frame[i].addr = (uint8_t *)addr + per_size * i;
                framepool->frame[i].data_off = 0;
                framepool->frame[i].data_len = 0;
                i++;
            }
    }
    n = xdp_ring_enqueue_bulk(ring, (void **)frame_list, ring_size, NULL);
    free(frame_list);
    if (n != ring_size) {
        ERR_OUT("framepool xdp_ring_enqueue_bulk failed");
        return NULL;
    }
    framepool->ring = ring;
    framepool->base_addr = addr;
    framepool->frame_size = frame_size;
    framepool->frame_headroom = frame_headroom;
    framepool->count = len;

    return framepool;
}

inline unsigned int
xdp_framepool_get_frame(struct xdp_framepool *fp,
    struct xdp_frame **frame_list, size_t count)
{
    struct xdp_ring *ring;
    unsigned int     n;

    ring = fp->ring;
    n = xdp_ring_dequeue_bulk(ring, (void **)frame_list, count, NULL); 

    return n;
}

inline unsigned int
xdp_framepool_put_frame(struct xdp_framepool *fp,
    struct xdp_frame **frame_list, size_t count)
{
    struct xdp_ring *ring;
    int    n;

    ring = fp->ring;
    n = xdp_ring_enqueue_bulk(ring, (void **)frame_list, count, NULL);

    return n;
}

inline void
xdp_framepool_free_frame(struct xdp_frame *frame)
{
    struct xdp_ring *ring;

    ring = frame->fpool->ring;
    xdp_ring_enqueue_bulk(ring, (void **)&frame, 1, NULL);
}
