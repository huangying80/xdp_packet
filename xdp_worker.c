/*
 * huangying email: hy_gzr@163.com
 */
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <emmintrin.h>
#include <xmmintrin.h>

#include "xdp_worker.h"
#include "xdp_log.h"

#define SYS_CPU_PATH "/sys/devices/system/cpu/cpu"
#define CORE_ID_FILE "topology/core_id"
#define NUMA_NODE_PATH "/sys/devices/system/node"
#define NUMA_NODES_MAX (8)

#define xdp_smp_rmb() _mm_lfence()
#define xdp_smp_wmb() _mm_sfence()
#define xdp_smp_mb()  _mm_mfence()
#define xdp_pause()   _mm_pause()

enum {
    INIT = 0,
    WAIT,
    RUNNING,
    FINISH
};

struct xdp_worker_config {
    volatile uint64_t           quit;
    volatile int                state;
    volatile pthread_t          thread_id; 
    volatile xdp_worker_func_t  worker_func;
    volatile void              *worker_arg;
    volatile int                worker_ret;
    char                        worker_name[16];
    unsigned                    numa_node_id;
    short                       cpu_core_index;
    unsigned                    cpu_core_id;
    cpu_set_t                   cpu_set;
    int                         notify_out[2];
    int                         notify_in[2];
} xdp_workers[XDP_MAX_WORKER];

static uint8_t xdp_workers_set[XDP_MAX_WORKER];

static unsigned short xdp_worker_count = 0;

static int xdp_worker_start_by_id(unsigned short worker_id,
    xdp_worker_func_t func, void *arg);
static int xdp_worker_wait_by_id(unsigned short worker_id);

static void *xdp_worker_loop(__attribute__((unused)) void *arg);

static int xdp_detect_cpu(unsigned short core_id);
static unsigned xdp_cpu_core_id(unsigned short core_id);
static int xdp_parse_cpu_id(const char *filename, unsigned long *val);
static unsigned xdp_cpu_numa_node_id(unsigned short core_id);

void xdp_workers_init(void)
{
    unsigned short   i;
    unsigned short count = 0;

    for (i = 0; i < XDP_MAX_WORKER; i++) {
        xdp_workers_set[i] = 0;
        if (!xdp_detect_cpu(i)) {
            xdp_workers[i].cpu_core_index = -1;    
            continue;
        }
        xdp_workers[i].quit = 0;
        xdp_workers[i].cpu_core_index = count;
        xdp_workers[i].cpu_core_id = xdp_cpu_core_id(i);
        xdp_workers[i].state = INIT;
        xdp_workers[i].thread_id = -1;
        xdp_workers[i].numa_node_id = xdp_cpu_numa_node_id(i);
        xdp_workers[i].worker_func = NULL;
        xdp_workers[i].worker_arg = NULL;
        xdp_workers[i].worker_ret = 0;
        CPU_ZERO(&xdp_workers[i].cpu_set);
        CPU_SET(i, &xdp_workers[i].cpu_set);
        count++;
    }

    xdp_worker_count = count;
}

void xdp_workers_stop(void)
{
    unsigned short worker_id;
    XDP_WORKER_FOREACH(worker_id) {
        xdp_smp_wmb();
        if (xdp_workers[worker_id].state == FINISH) {
            xdp_workers[worker_id].quit = 1;
        }
    }
    XDP_WORKER_FOREACH(worker_id) {
        pthread_join(xdp_workers[worker_id].thread_id, NULL);
    }
}

inline unsigned xdp_worker_get_numa_node(unsigned short worker_id)
{
    return xdp_workers[worker_id].numa_node_id;
}

short xdp_worker_get_next(short i)
{
    i++;
    while (i < XDP_MAX_WORKER) {
        if (xdp_workers_set[i]) {
            return i;
        }
        i++;
    }
    return i;
}

unsigned short xdp_workers_enable(unsigned short count)
{
    unsigned short i = 0;
    unsigned short n = 0;

    for (i = 0; i < XDP_MAX_WORKER && n < count; i++) {
        if (xdp_workers_set[i]) {
            continue;
        }
        xdp_workers_set[i] = 1;
        n++;
    }

    return n - count;
}

unsigned short xdp_workers_enable_by_numa(int numa_node, unsigned short count)
{
    int            i;
    unsigned short n = 0;
    for (i = 0; i < XDP_MAX_WORKER && n < count; i++) {
        if (xdp_workers[i].numa_node_id == numa_node) {
            xdp_workers_set[i] = 1;
            n++;
        }
    }

    return n;
}

void xdp_workers_run(void)
{
    unsigned short i;
    char           worker_name[32];
    int            ret = 0;
    int            n;

    XDP_WORKER_FOREACH(i) {
        if (pipe(xdp_workers[i].notify_out) < 0) {
            XDP_PANIC("open pipe failed for notify out, err %d", errno);
        }
        if (pipe(xdp_workers[i].notify_in) < 0) {
            XDP_PANIC("open pipe failed for notify out, err %d", errno);
        }
        xdp_workers[i].state = WAIT;
        ret = pthread_create((pthread_t *)&xdp_workers[i].thread_id, NULL,
            xdp_worker_loop, NULL);
        if (ret) {
            XDP_PANIC("create worker failed, err %d", ret);
        }
        n = snprintf(worker_name, 16, "xdp_wrk_%d", i);
        worker_name[n] = 0;
        ret = pthread_setname_np(xdp_workers[i].thread_id, worker_name);
        if (ret) {
            XDP_PANIC("set worker name failed, err %d", ret);
        }
    }
}

int xdp_worker_start(xdp_worker_func_t func, void *arg)
{
    int ret = 0;
    unsigned short worker_id;
    XDP_WORKER_FOREACH(worker_id) {
        ret = xdp_worker_start_by_id(worker_id, func, arg);        
        if (ret < 0) {
            ERR_OUT("start worker %u failed", worker_id);
            return -1;
        }
    }

    return 0;
}

static int
xdp_worker_start_by_id(unsigned short worker_id,
    xdp_worker_func_t func, void *arg)
{
    int notify_out;
    int notify_in;
    int n = 0;
    char c;

    if (xdp_workers[worker_id].state != WAIT) {
        return -1;
    }

    xdp_workers[worker_id].worker_func = func;
    xdp_workers[worker_id].worker_arg = arg;

    notify_out = xdp_workers[worker_id].notify_in[1];
    notify_in = xdp_workers[worker_id].notify_out[0];

    while (n == 0 || (n < 0 && errno == EINTR)) {
        n = write(notify_out, &c, 1);
    }
    if (n < 0) {
        XDP_PANIC("cannot send notify, err %d", errno);
    }

    do {
        n = read(notify_in, &c, 1);
    } while (n < 0 && errno == EINTR);

    if (n <= 0) {
        XDP_PANIC("cannot read notify, err %d", errno);
    }

    return 0;
}

void xdp_workers_wait(void)
{
    unsigned short worker_id;
    XDP_WORKER_FOREACH(worker_id) {
        xdp_worker_wait_by_id(worker_id);        
    }
}

static int xdp_worker_wait_by_id(unsigned short worker_id)
{
    if (xdp_workers[worker_id].state == WAIT) {
        return 0;
    }

    while(xdp_workers[worker_id].state != FINISH &&
        xdp_workers[worker_id].state != WAIT) {
        xdp_pause();
    }

    xdp_smp_rmb();
    xdp_workers[worker_id].state = WAIT;
    return xdp_workers[worker_id].worker_ret;
}

static void *xdp_worker_loop(__attribute__((unused)) void *arg)
{
    volatile xdp_worker_func_t func;
    volatile void *args;
    unsigned short  worker_id;
    uint64_t        quit = 0;
    pthread_t       thread_id;
    int             notify_in;
    int             notify_out;
    int             ret;
    int             n;
    char            c;

    thread_id = pthread_self();
    XDP_WORKER_FOREACH(worker_id) {
        if (thread_id == xdp_workers[worker_id].thread_id) {
            break;
        }
    }
    if (worker_id == XDP_MAX_WORKER) {
        XDP_PANIC("can not get worker id");
    }
    if (!xdp_workers[worker_id].worker_func) {
        XDP_PANIC("worker function is NULL");
    }
    func = xdp_workers[worker_id].worker_func;
    args = xdp_workers[worker_id].worker_arg;

    ret = pthread_setaffinity_np(thread_id, sizeof(cpu_set_t),
        &xdp_workers[worker_id].cpu_set);
    if (ret) {
        XDP_PANIC("set worker affinity failed, err %d", ret);
    }
    notify_in = xdp_workers[worker_id].notify_in[0];
    notify_out = xdp_workers[worker_id].notify_out[1]; 

    while (!quit) {
        do {
            n = read(notify_in, &c, 1);
        } while (ret < 0 && errno == EINTR);
        if (n <= 0) {
            XDP_PANIC("read notify failed, err %d", errno);
        }

        xdp_workers[worker_id].state = RUNNING;
        n = 0;
        while (n == 0 || (n < 0 && errno == EINTR)) {
            n = write(notify_out, &c, 1);
        }
        if (n < 0) {
            XDP_PANIC("send notify failed, err %d", errno);
        }
        ret = func(args);
        xdp_workers[worker_id].worker_ret = ret;
        xdp_smp_wmb();
        xdp_workers[worker_id].state = FINISH; 
        quit = xdp_workers[worker_id].quit;
        xdp_smp_rmb();
    }

    return NULL;
}

static int xdp_detect_cpu(unsigned short core_id)
{
    char path[PATH_MAX];    
    int  len;

    len = snprintf(path, sizeof(path), "%s%u/%s", 
        SYS_CPU_PATH, core_id, CORE_ID_FILE);
    if (len <= 0) {
        return 0;
    }
    if (access(path, F_OK) != 0) {
        return 0;
    }

    return 1;
}

static unsigned xdp_cpu_core_id(unsigned short core_id)
{
    char path[PATH_MAX];
    unsigned long id;
    int len;
    
    len = snprintf(path, sizeof(path), "%s%u/%s",
        SYS_CPU_PATH, core_id, CORE_ID_FILE);
    if (len <= 0) {
        return -1;
    }
    if (xdp_parse_cpu_id(path, &id) < 0) {
        return -1;
    }
    return (unsigned)id;
}

static int xdp_parse_cpu_id(const char *filename, unsigned long *val)
{
    FILE *f;
    char  buf[BUFSIZ];
    char *end = NULL;
    int   ret = -1;
    
    if ((f = fopen(filename, "r")) == NULL) {
        ERR_OUT("%s(): cannot open sysfs value", filename);
        return -1;
    }
    
    if (fgets(buf, sizeof(buf), f) == NULL) {
        ERR_OUT("cannot read sysfs value %s", filename);
        goto out;
    }
    *val = strtoul(buf, &end, 0);
    if ((buf[0] == '\0') || (end == NULL) || (*end != '\n')) {
        ERR_OUT("cannot parse sysfs value %s", filename);
        goto out;
    }
    
    ret = 0;
out:
    fclose(f);
    return ret;
}

static unsigned xdp_cpu_numa_node_id(unsigned short core_id)
{
    unsigned node;
    char path[PATH_MAX];
    for (node = 0; node < NUMA_NODES_MAX; node++) {
        snprintf(path, sizeof(path), "%s/node%u/cpu%u", NUMA_NODE_PATH,
            node, core_id);
        if (access(path, F_OK) == 0) {
            return node;
        }
    }
    return 0;
}
