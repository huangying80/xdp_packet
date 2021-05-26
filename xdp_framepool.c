/*
 * huangying email: hy_gzr@163.com
 */
#include <stdlib.h>

#include "xdp_log.h"
#include "xdp_mempool.h"
#include "xdp_ring.h"
#include "xdp_framepool.h"
#include "xdp_dev.h"

#define XDP_FRAMEPOOL_HDR_SIZE \
    (XDP_ALIGN(sizeof(struct xdp_framepool), XDP_CACHE_LINE))

#define XDP_FRAMEPOOL_HDR_ADDR_SIZE \
    (XDP_FRAMEPOOL_HDR_SIZE + XDP_CACHE_LINE)

#define XDP_FRAME_HEADROOM(fh) \
    ((fh) + sizeof(struct xdp_frame))

#define XDP_FRAME_SIZE(fs) (fs)

#define XDP_FRAME_PER_SIZE(fs, fh) \
    (XDP_FRAME_SIZE(fs) + XDP_FRAME_HEADROOM(fh))

#define XDP_FRAMESPACE_SIZE(fs, fh, l) \
    (XDP_ALIGN(XDP_FRAME_PER_SIZE(fs, fh) * (l), XDP_PAGESIZE))

#define XDP_FRAMESPACE_ADDR_SIZE(fs, fh, l) \
    (XDP_FRAMESPACE_SIZE(fs, fh, l) + XDP_PAGESIZE)

size_t
xdp_framepool_memory_addr_size(uint32_t len, size_t frame_size,
    size_t frame_headroom)
{
    size_t framepool_hdr_size;
    size_t frame_space_size;
    size_t ring_memory_size;
    size_t size;

    framepool_hdr_size = XDP_FRAMEPOOL_HDR_ADDR_SIZE;
    len = xdp_align_pow2_32(len);
    ring_memory_size = xdp_ring_memory_addr_size(len);
    if (!ring_memory_size) {
        return 0;
    }

    frame_space_size = XDP_FRAMESPACE_ADDR_SIZE(frame_size, frame_headroom, len);
    size = framepool_hdr_size + ring_memory_size + frame_space_size;

    return size;
}


struct xdp_framepool *
xdp_framepool_create(struct xdp_mempool *pool, uint32_t len,
    uint32_t comp_len, size_t frame_size, size_t frame_headroom)
{
    int      i = 0;
    unsigned int n;
    size_t    per_size;
    size_t    ring_size;
    size_t    size;
    size_t    framepool_hdr_size;
    void     *addr;
    struct xdp_frame    **frame_list;
    struct xdp_ring      *ring = NULL;
    struct xdp_framepool *framepool = NULL;

    framepool_hdr_size = XDP_FRAMEPOOL_HDR_SIZE;
    framepool = xdp_mempool_alloc(pool, framepool_hdr_size, XDP_CACHE_LINE);
    if (!framepool) {
        return NULL;
    }

    ring = xdp_ring_create(pool, len, XDP_RING_SP_ENQ | XDP_RING_SC_DEQ);
    if (!ring) {
        return NULL;
    }

    ring_size = xdp_ring_get_size(ring);
    size = XDP_FRAMESPACE_SIZE(frame_size, frame_headroom, ring_size);
    addr = xdp_mempool_alloc(pool, size, XDP_PAGESIZE);
    if (!addr) {
        return NULL;
    }
    per_size = XDP_FRAME_PER_SIZE(frame_size, frame_headroom);
    frame_list = malloc(sizeof(struct xdp_frame *) * ring_size);
    switch (ring_size % 8) {
        case 0:
            while (i != ring_size) {
                frame_list[i] = (struct xdp_frame *)(addr + i * per_size);
                frame_list[i]->fpool = framepool;
                frame_list[i]->data_off = 0;
                frame_list[i]->data_len = 0;
                i++;
        case 7:
                frame_list[i] = (struct xdp_frame *)(addr + i * per_size);
                frame_list[i]->fpool = framepool;
                frame_list[i]->data_off = 0;
                frame_list[i]->data_len = 0;
                i++;
        case 6:
                frame_list[i] = (struct xdp_frame *)(addr + i * per_size);
                frame_list[i]->fpool = framepool;
                frame_list[i]->data_off = 0;
                frame_list[i]->data_len = 0;
                i++;
        case 5:
                frame_list[i] = (struct xdp_frame *)(addr + i * per_size);
                frame_list[i]->fpool = framepool;
                frame_list[i]->data_off = 0;
                frame_list[i]->data_len = 0;
                i++;
        case 4:
                frame_list[i] = (struct xdp_frame *)(addr + i * per_size);
                frame_list[i]->fpool = framepool;
                frame_list[i]->data_off = 0;
                frame_list[i]->data_len = 0;
                i++;
        case 3:
                frame_list[i] = (struct xdp_frame *)(addr + i * per_size);
                frame_list[i]->fpool = framepool;
                frame_list[i]->data_off = 0;
                frame_list[i]->data_len = 0;
                i++;
        case 2:
                frame_list[i] = (struct xdp_frame *)(addr + i * per_size);
                frame_list[i]->fpool = framepool;
                frame_list[i]->data_off = 0;
                frame_list[i]->data_len = 0;
                i++;
        case 1:
                frame_list[i] = (struct xdp_frame *)(addr + i * per_size);
                frame_list[i]->fpool = framepool;
                frame_list[i]->data_off = 0;
                frame_list[i]->data_len = 0;
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
    framepool->frame_size = XDP_FRAME_SIZE(frame_size);
    framepool->frame_headroom = XDP_FRAME_HEADROOM(frame_headroom); 
    framepool->count = ring_size;
    framepool->comp_count = comp_len;

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
