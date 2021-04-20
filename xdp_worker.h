/*
 * huangying, email hy_gzr@163.com
 */
#ifndef _XDP_WORKER_H_
#define _XDP_WORKER_H_
#define XDP_MAX_WORKER (128)

#ifdef __cplusplus
extern "C" {
#endif
#define XDP_WORKER_FOREACH(i)        \
for (i = xdp_worker_get_next(-1); i < XDP_MAX_WORKER; i = xdp_worker_get_next(i))

#define XDP_WORKER_FOREACH_ALL(i) \
for (i = 0; i < XDP_MAX_WORKER; i++)

typedef int (*xdp_worker_func_t) (volatile void *);
void xdp_workers_init(void);
void xdp_workers_stop(void);
int xdp_worker_start(xdp_worker_func_t func, void *arg);
int xdp_worker_start_by_id(unsigned short worker_id,
    xdp_worker_func_t func, void *arg);
void xdp_workers_wait(void);
unsigned xdp_worker_get_numa_node(unsigned short worker_id);
short xdp_worker_get_next(short i);
unsigned short xdp_workers_enable_by_numa(int numa_node, unsigned short count);
unsigned short xdp_workers_enable(unsigned short count);
void xdp_workers_run(void);
#ifdef __cplusplus
} 
#endif
#endif
