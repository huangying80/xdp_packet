/*
 * huangying, email: hy_gzr@163.com
 */
#ifndef _XDP_NUMA_H_
#define _XDP_NUMA_H_
#include <numa.h>
#include <numaif.h>
int xdp_numa_check(void);
void xdp_numa_set(int node, int *oldpolicy, struct bitmask *oldmask);
void xdp_numa_restore(int oldpolicy, struct bitmask *oldmask);
#endif
