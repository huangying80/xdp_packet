/*
 * huangying email: hy_gzr@163.com
 */
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "xsk.h"
#include "linux/if_link.h"
#include "linux/if_xdp.h"

#include "xdp_sock.h"
#include "xdp_log.h"

#ifndef XDP_SOCK_RX_BATCH_SIZE
#define XDP_SOCK_RX_BATCH_SIZE  (64)
#endif

#ifndef XDP_SOCK_TX_BATCH_SIZE
#define XDP_SOCK_TX_BATCH_SIZE  (64)
#endif

#define xdp_sock_unlikely(x) __builtin_expect(!!(x), 0)

static struct xdp_umem_info* xdp_sock_umem_configure(struct xdp_rx_queue *rxq,
    struct xdp_umem_info_pool *umem_pool);
static void xdp_sock_pull_umem_cq(struct xdp_umem_info *umem, uint32_t size);
static void xdp_sock_kick_tx(struct xdp_tx_queue *txq);
static int xdp_sock_reserve_fq_zc(struct xdp_umem_info *umem,
    uint16_t reserve_size, struct xdp_frame **bufs);

int
xdp_sock_configure(struct xdp_iface *iface,
    struct xdp_rx_queue *rxq, uint32_t queue_size,
    struct xdp_umem_info_pool *umem_pool)
{
    unsigned int n;
    int    reserve_size = XSK_RING_CONS__DEFAULT_NUM_DESCS;
    int    ret = -1;
    struct xsk_socket_config cfg;
    struct xdp_tx_queue     *txq = rxq->pair;
    struct xdp_frame        *fq_bufs[reserve_size];

    rxq->umem = xdp_sock_umem_configure(rxq, umem_pool);
    if (!rxq->umem) {
        goto out;
    }
    txq->umem = rxq->umem;

    cfg.rx_size = queue_size;
    cfg.tx_size = queue_size;
    cfg.libbpf_flags = 0;
    cfg.xdp_flags = XDP_FLAGS_UPDATE_IF_NOEXIST;
    cfg.bind_flags = 0;
#if defined(XDP_USE_NEED_WAKEUP)
    cfg.bind_flags |= XDP_USE_NEED_WAKEUP;
#endif

    ret = xsk_socket__create(&rxq->xsk, iface->ifname, rxq->queue_index,
        rxq->umem->umem, &rxq->rx, &txq->tx, &cfg);
    if (ret < 0) {
        ERR_OUT("xsk_socket__create failed for rxq %d, err %d",
            rxq->queue_index, ret);
        goto out;
    }

    //for zc
    n = xdp_framepool_get_frame(rxq->framepool, fq_bufs, reserve_size);
    if (!n || n != reserve_size) {
        ERR_OUT("xdp_framepool_get_frame faield for rxq %d", rxq->queue_index);
        goto out;
    }
    ret = xdp_sock_reserve_fq_zc(rxq->umem, n, fq_bufs);
    if (ret < 0) {
        ERR_OUT("xdp_socket_reserve_fq_zc faield for rxq %d", rxq->queue_index);
        xsk_socket__delete(rxq->xsk);
        goto out;
    }

    rxq->fds[0].fd = xsk_socket__fd(rxq->xsk);
    rxq->fds[0].events = POLLIN;

    ret = 0;
out:
    return ret;
}


int
xdp_sock_rx_zc(struct xdp_rx_queue *rxq, struct xdp_frame **bufs,
    uint16_t buf_len)
{
    struct xsk_ring_cons  *rx = &rxq->rx;
    struct xdp_umem_info  *umem = rxq->umem;
    struct xdp_frame      *fq_bufs[XDP_SOCK_RX_BATCH_SIZE];
    const struct xdp_desc *desc;
    uint64_t    addr;
    uint64_t    offset;
    uint32_t    len;
    uint32_t    index = 0;
    uint32_t    n;
    uint16_t    frame_n = 0;
    int         rcvd;
    int         i;
    

    frame_n = buf_len < XDP_SOCK_RX_BATCH_SIZE ?
        buf_len : XDP_SOCK_RX_BATCH_SIZE;

    n = xdp_framepool_get_frame(umem->framepool, fq_bufs, frame_n); 
    if (!n) {
        return -1;
    }
    rcvd = xsk_ring_cons__peek(rx, n, &index);    
    if (!rcvd) {
#if defined(XDP_USE_NEED_WAKEUP)
        if (xsk_ring_prod__needs_wakeup(&umem->fq)) {
            poll(rxq->fds, 1, 500);
        }
#endif
        goto out;
    }

    for (i = 0; i < rcvd; i++) {
        desc = xsk_ring_cons__rx_desc(rx, index++);
        addr = desc->addr;
        len = desc->len;
        offset = xsk_umem__extract_offset(addr);
        addr = xsk_umem__extract_addr(addr);
        bufs[i] = (struct xdp_frame *) xsk_umem__get_data(umem->buffer, addr);
        bufs[i]->data_off = offset - sizeof(struct xdp_frame);
        bufs[i]->data_len = len;
    }

    xsk_ring_cons__release(rx, rcvd);
    xdp_sock_reserve_fq_zc(umem, rcvd, fq_bufs);
 
out:
    if (rcvd != n) {
        xdp_framepool_put_frame(umem->framepool, &fq_bufs[rcvd], n - rcvd);
    }
    return rcvd;
}

int
xdp_sock_tx_zc(struct xdp_tx_queue *txq, struct xdp_frame **bufs,
    uint16_t buf_len)
{
    struct xdp_umem_info *umem = txq->umem;
    struct xdp_desc      *desc;
    struct xdp_frame     *frame;

    uint16_t    i;
    uint16_t    count = 0;
    uint32_t    index;
    uint64_t    addr;
    uint64_t    offset;
    uint32_t    free_thresh = umem->cq.size >> 1;
     
    if (xsk_cons_nb_avail(&umem->cq, free_thresh) >= free_thresh) {
        xdp_sock_pull_umem_cq(umem, XSK_RING_CONS__DEFAULT_NUM_DESCS);
    }
    for (i = 0; i < buf_len; i++) {
        frame = bufs[i];    
        if (!xsk_ring_prod__reserve(&txq->tx, 1, &index)) {
            xdp_sock_kick_tx(txq);
            if (!xsk_ring_prod__reserve(&txq->tx, 1, &index)) {
                goto out;
            }
        }
        desc = xsk_ring_prod__tx_desc(&txq->tx, index);
        desc->len = frame->data_len;
        addr = (void *)frame - umem->buffer;
        offset = (frame->data_off + sizeof(struct xdp_frame)) << XSK_UNALIGNED_BUF_OFFSET_SHIFT;
        desc->addr = addr | offset;
        count++;
    }
    xdp_sock_kick_tx(txq);
out:
    xsk_ring_prod__submit(&txq->tx, count);
    return count;
}

static struct xdp_umem_info*
xdp_sock_umem_configure(struct xdp_rx_queue *rxq,
    struct xdp_umem_info_pool *umem_pool)
{
    int     ret = 0;
    struct xdp_umem_info *xdp_umem = NULL;
    struct xsk_umem_config cfg = { //only fo zc
        .fill_size = XSK_RING_CONS__DEFAULT_NUM_DESCS << 1,
        .comp_size = XSK_RING_CONS__DEFAULT_NUM_DESCS,
        .flags = XDP_UMEM_UNALIGNED_CHUNK_FLAG
    };
    struct xdp_framepool *fpool = rxq->framepool;

    cfg.frame_size = fpool->frame_size;
    cfg.frame_headroom = fpool->frame_headroom;
    
    xdp_umem = xdp_dev_umem_info_calloc(umem_pool, 1);
    if (!xdp_umem) {
        ERR_OUT("allocate umem failed");
        ret = -1;
        goto out;
    }
    xdp_umem->framepool = fpool;
    ret = xsk_umem__create(&xdp_umem->umem, fpool->base_addr,
        fpool->count * fpool->frame_size,
        &xdp_umem->fq, &xdp_umem->cq, &cfg);
    if (ret < 0) {
        ERR_OUT("xsk_umem__create failed");
        goto out;
    }
    xdp_umem->buffer = fpool->base_addr;

out:
    if (ret < 0 && xdp_umem) {
        xdp_umem = NULL;
    }
    return xdp_umem;
}


static void xdp_sock_pull_umem_cq(struct xdp_umem_info *umem, uint32_t size)
{
    struct xsk_ring_cons *cq = &umem->cq;
    size_t      i;
    size_t      n;
    uint32_t    index = 0;
    uint32_t    addr;
    struct xdp_frame *frame;

    n = xsk_ring_cons__peek(cq, size, &index);
    for (i = 0; i < n; i++) {
        addr = *xsk_ring_cons__comp_addr(cq, index++);
        addr = xsk_umem__extract_addr(addr);
        frame = xsk_umem__get_data(umem->buffer, addr);
        xdp_framepool_free_frame(frame);
    }

    xsk_ring_cons__release(cq, n);
}

static void xdp_sock_kick_tx(struct xdp_tx_queue *txq)
{
    struct xdp_umem_info *umem = txq->umem;
    xdp_sock_pull_umem_cq(umem, XSK_RING_CONS__DEFAULT_NUM_DESCS);

#if defined(XDP_USE_NEED_WAKEUP)
    if (xsk_ring_prod__needs_wakeup(&txq->tx)) {
#endif
        while (send(xsk_socket__fd(txq->pair->xsk), NULL,
            0, MSG_DONTWAIT) < 0) {
            if (errno != EBUSY && errno != EAGAIN && errno != EINTR) {
                break;
            }
            if (errno == EAGAIN) {
                xdp_sock_pull_umem_cq(umem, XSK_RING_CONS__DEFAULT_NUM_DESCS);
            }
        }
#if defined(XDP_USE_NEED_WAKEUP)
    }
#endif
    //xdp_sock_pull_umem_cq(umem, XDP_SOCK_TX_BATCH_SIZE);
}

static int
xdp_sock_reserve_fq_zc(struct xdp_umem_info *umem, uint16_t reserve_size,
    struct xdp_frame **bufs)
{
    struct xsk_ring_prod *fq = &umem->fq;
    uint64_t    addr;
    __u64      *fq_addr;
    uint32_t    index = 0;
    uint16_t    i;

    if (xdp_sock_unlikely(!xsk_ring_prod__reserve(fq, reserve_size, &index))) {
        xdp_framepool_put_frame(umem->framepool, bufs, reserve_size);
        ERR_OUT("Failed to reserve enough fq descs");
        return -1;
    }

    for (i = 0; i < reserve_size; i++) {
        fq_addr = xsk_ring_prod__fill_addr(fq, index++);
        addr = (void *)bufs[i] - umem->buffer;
        *fq_addr = addr;
    }
    xsk_ring_prod__submit(fq, reserve_size);
    return 0;
}

