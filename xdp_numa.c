/*
 * huangying email: hy_gzr@163.com
 */
#include <errno.h>

#include "xdp_numa.h"
#include "xdp_log.h"

inline int xdp_numa_check(void)
{
    return numa_available() ? 1 : 0;
}

void xdp_numa_set(int node, int *oldpolicy, struct bitmask *oldmask)
{
    int ret;

    oldmask = numa_allocate_nodemask();
    ret = get_mempolicy(oldpolicy, oldmask->maskp, oldmask->size + 1, 0, 0);
    if (ret < 0) {
        *oldpolicy = MPOL_DEFAULT;
    }
    numa_set_preferred(node);
}

void xdp_numa_restore(int oldpolicy, struct bitmask *oldmask)
{
    if (oldpolicy == MPOL_DEFAULT) {
        numa_set_localalloc();
    } else if (set_mempolicy(oldpolicy, oldmask->maskp,
        oldmask->size + 1) < 0) {
        ERR_OUT("set_mempolicy failed, err: %d", errno); 
        numa_set_localalloc();
    }

    numa_free_cpumask(oldmask);
}
