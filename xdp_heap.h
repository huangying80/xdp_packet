/*
 * huangying, email: hy_gzr@163.com
 */
#ifndef _XDP_HEAP_H_
#define _XDP_HEAP_H_
extern struct xdp_mempool_ops heap_mempool_ops;
struct xdp_mempool_ops *xdp_heap_get_ops(void);
#endif
