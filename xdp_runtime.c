/*
 * huangying email: hy_gzr@163.com
 */
#include <linux/ip.h>
#include <linux/ipv6.h>

#include "xdp_numa.h"
#include "xdp_dev.h"
#include "xdp_runtime.h"
#include "xdp_sock.h"
#include "xdp_prog.h"
#include "xdp_log.h"


#ifndef XDP_RUNTIME_FILL_SIZE
#define XDP_RUNTIME_FILL_SIZE 4096
#endif

#ifndef XDP_RUNTIME_COMP_SIZE
#define XDP_RUNTIME_COMP_SIZE 2048
#endif

#ifndef XDP_RUNTIME_FRAME_SIZE 
#define XDP_RUNTIME_FRAME_SIZE 2048
#endif

#ifndef XDP_RUNTIME_FRAME_HEADROOM 
#define XDP_RUNTIME_FRAME_HEADROOM 128
#endif

#ifndef XDP_RUNTIME_QUEUE_SIZE
#define XDP_RUNTIME_QUEUE_SIZE 1024
#endif


int xdp_runtime_init(struct xdp_runtime *runtime, const char *ifname)
{
    int ret;
   
    runtime->fill_size = XDP_RUNTIME_FILL_SIZE;
    runtime->comp_size = XDP_RUNTIME_COMP_SIZE;
    runtime->frame_size = XDP_RUNTIME_FRAME_SIZE;
    runtime->frame_headroom = XDP_RUNTIME_FRAME_HEADROOM;
    runtime->queue_size = XDP_RUNTIME_QUEUE_SIZE;
    runtime->queue_count = 0;
    runtime->workers = 0; 
    runtime->mempool = NULL;
    runtime->eth_numa_node = xdp_eth_numa_node(ifname);
    if (xdp_eth_get_info(ifname, &runtime->iface) < 0) {
        return -1;
    }
    ret = xdp_prog_init(ifname, NULL, NULL);
    if (ret < 0) {
        return -1;
    }

    return ret;
}

inline void
xdp_runtime_setup_size(struct xdp_runtime *runtime,
    uint32_t fill_size, uint32_t comp_size,
    uint32_t frame_size, uint32_t frame_headroom)
{
    if (fill_size) {
        runtime->fill_size = XDP_ALIGN(fill_size, 2048);
    }
    if (comp_size) {
        runtime->comp_size = XDP_ALIGN(comp_size, 1024);
    }
    if (frame_size) {
        runtime->frame_size = XDP_ALIGN(frame_size, 8);
    }
    if (frame_headroom) {
        runtime->frame_headroom = XDP_ALIGN(frame_headroom, 8);
    }
}

int
xdp_runtime_setup_queue(struct xdp_runtime *runtime,
    size_t queue_count, size_t queue_size)
{
    struct xdp_mempool   *pool;
    struct xdp_framepool *frame_pool;
    struct xdp_iface     *iface;
    size_t                frame_count;
    size_t                frame_size;
    size_t                frame_headroom;
    size_t                mempool_size;
    size_t                fp_size;
    int                   ret = -1;
    int                   i = 0;

    int                   numa_node;

    if (!runtime || !queue_count || !queue_size) {
        return -1;
    }

    frame_count = runtime->fill_size;
    frame_size = runtime->frame_size;
    frame_headroom = runtime->frame_headroom;
    fp_size = xdp_framepool_memory_size(frame_count, frame_size, frame_headroom);
    if (!fp_size) {
        return -1;
    }
    mempool_size = (sizeof(struct xdp_rx_queue) + sizeof(struct xdp_tx_queue))
        * queue_count;
    mempool_size = XDP_ALIGN(mempool_size, XDP_CACHE_LINE);
    mempool_size += fp_size * queue_count;;
    mempool_size += xdp_dev_umem_info_pool_memsize(queue_count);

    numa_node = runtime->eth_numa_node;

    pool = xdp_mempool_create(numa_node, mempool_size); 
    if (!pool) {
        goto out;
    }

    iface = &runtime->iface;
    ret = xdp_dev_configure(pool, iface->ifname, queue_count);
    if (ret < 0) {
        goto out;
    }
    for (i = 0; i < queue_count; i++) {
        frame_pool = xdp_framepool_create(pool, frame_count,
            frame_size, frame_headroom);
        if (!frame_pool) {
            goto out;
        }
        ret = xdp_dev_queue_configure(i, queue_size, frame_pool);    
        if (ret < 0) {
            goto out;
        }
    }

    ret = 0;
    runtime->mempool = pool;
    runtime->queue_size = queue_size;
    runtime->queue_count = queue_count;

out:
    if (ret < 0 && pool) {
        xdp_mempool_release(pool);    
    }

    return ret;
}

int
xdp_runtime_setup_workers(struct xdp_runtime *runtime,
    xdp_worker_func_t worker_func,
    unsigned short worker_count)
{
    int            ret = -1;
    int            numa_node = 0;
    unsigned short count;
    unsigned short n;

    if (!runtime->queue_count) {
        ERR_OUT("queue count is 0");
        goto out;
    }

    if (!worker_count || (size_t)worker_count > runtime->queue_count) {
        worker_count = (unsigned short)runtime->queue_count;
    }
    numa_node = runtime->eth_numa_node;
    if (numa_node < 0) {
        numa_node = 0;
    }

    xdp_workers_init();
    count = xdp_workers_enable_by_numa(numa_node, worker_count);
    n = worker_count - count;
    if (n) {
        count = xdp_workers_enable(n);
        if (count == n) {
            goto out;
        }
    }

    runtime->workers = worker_count;
    runtime->worker_func = worker_func;
    xdp_workers_run();
    ret = 0;

out:
    return ret;
}

int xdp_runtime_startup_workers(struct xdp_runtime *runtime)
{
    int      ret;
    uint16_t qIdx = 0;
    uint16_t worker_id = 0;

    XDP_WORKER_FOREACH(worker_id) {
        ret = xdp_worker_start_by_id(worker_id, runtime->worker_func, &qIdx);
        if (ret < 0) {
            return -1;
        }
        if (++qIdx >= runtime->queue_count) {
            break;
        }

    }
    return 0;
}

void xdp_runtime_release(struct xdp_runtime *runtime)
{
    xdp_workers_stop();
    xdp_prog_release();
    if (runtime->mempool) {
        xdp_mempool_release(runtime->mempool);
    }
}

inline int xdp_runtime_tcp_packet(uint16_t port)
{
    return xdp_prog_update_tcpport(port, XDP_PACKET_POLICY_DIRECT);        
}

inline int xdp_runtime_tcp_drop(uint16_t port)
{
    return xdp_prog_update_tcpport(port, XDP_PACKET_POLICY_DROP);        
}

inline int xdp_runtime_udp_packet(uint16_t port)
{
    return xdp_prog_update_udpport(port, XDP_PACKET_POLICY_DIRECT);        
}

inline int xdp_runtime_udp_drop(uint16_t port)
{
    return xdp_prog_update_udpport(port, XDP_PACKET_POLICY_DROP);        
}

inline int xdp_runtime_l3_packet(uint16_t l3_protocal)
{
    return xdp_prog_update_l3(l3_protocal, XDP_PACKET_POLICY_DIRECT);        
}

inline int xdp_runtime_l3_drop(uint16_t l3_protocal)
{
    return xdp_prog_update_l3(l3_protocal, XDP_PACKET_POLICY_DROP);        
}
inline int xdp_runtime_l4_packet(uint16_t l4_protocal)
{
    return xdp_prog_update_l4(l4_protocal, XDP_PACKET_POLICY_DROP);        
}
inline int xdp_runtime_l4_drop(uint16_t l4_protocal)
{
    return xdp_prog_update_l4(l4_protocal, XDP_PACKET_POLICY_DROP);        
}

inline int xdp_runtime_ipv4_packet(const char *ip, uint32_t prefix, int type)
{
    struct in_addr addr;
    int  ret;
    ret = inet_pton(AF_INET, ip, &addr);
    if (ret != 1) {
        ERR_OUT("xdp_runtime_ipv4_packet failed %s/%u", ip, prefix);
        return -1;
    }

    return xdp_prog_update_ipv4(&addr, prefix, type, XDP_PACKET_POLICY_DIRECT);
}

inline int xdp_runtime_ipv4_drop(const char *ip, uint32_t prefix, int type)
{
    struct in_addr addr;
    int  ret;
    ret = inet_pton(AF_INET, ip, &addr);
    if (ret != 1) {
        ERR_OUT("xdp_runtime_ipv4_drop failed %s/%u", ip, prefix);
        return -1;
    }

    return xdp_prog_update_ipv4(&addr, prefix, type, XDP_PACKET_POLICY_DROP);
}

inline int xdp_runtime_ipv6_packet(const char *ip, uint32_t prefix, int type)
{
    struct in6_addr addr;
    int  ret;
    ret = inet_pton(AF_INET6, ip, &addr);
    if (ret != 1) {
        ERR_OUT("xdp_runtime_ipv6_packet failed %s/%u", ip, prefix);
        return -1;
    }

    return xdp_prog_update_ipv6(&addr, prefix, type, XDP_PACKET_POLICY_DIRECT);
}

inline int xdp_runtime_ipv6_drop(const char *ip, uint32_t prefix, int type)
{
    struct in6_addr addr;
    int  ret;
    ret = inet_pton(AF_INET6, ip, &addr);
    if (ret != 1) {
        ERR_OUT("xdp_runtime_ipv6_drop failed %s/%u", ip, prefix);
        return -1;
    }

    return xdp_prog_update_ipv6(&addr, prefix, type, XDP_PACKET_POLICY_DROP);
}
