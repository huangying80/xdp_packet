/*
 * huangying, email:hy_gzr@163.com
 */
#ifndef _XDP_HUGEPAGE_H_
#define _XDP_HUGEPAGE_H_

#ifndef XDP_HUGEPAGE_SUBDIR
#define XDP_HUGEPAGE_SUBDIR "xdp_hugepages"
#endif

#ifndef XDP_HUGEPAGE_MAP_FILE
#define XDP_HUGEPAGE_MAP_FILE "xdp_map"
#endif

#ifndef XDP_HUGEPAGE_HOME
#define XDP_HUGEPAGE_HOME "/mnt"
#endif

extern struct xdp_mempool_ops hugepage_mempool_ops;
struct xdp_mempool_ops *xdp_hugepage_get_ops(void);
#endif
