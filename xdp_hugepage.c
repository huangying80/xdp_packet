/*
 * huangying email: hy_gzr@163.com
 */
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "xdp_hugepage.h"
#include "xdp_mempool.h"
#include "xdp_log.h"

#define SYS_HUGEPAGE_PATH "/sys/kernel/mm/hugepages"
#define NUMA_HUGEPAGE_PATH "/sys/devices/system/node/node%d/hugepages"

int xdp_hugepage_init(void *data);
int xdp_hugepage_release(void *data);

static struct xdp_mempool *xdp_hugepage_reserve(int node, size_t size,
    __attribute__((unused))void *data);
static int xdp_hugepage_revert(struct xdp_mempool *pool,
    __attribute__((unused))void *data);

static unsigned long xdp_hugepage_free_pages(const char *dir,
    const char *subdir);
static int xdp_hugepage_info(const char *path,
    size_t *page_size, size_t *page_num);

static int xdp_hugepage_size(size_t *page_size, size_t *page_num);
static int xdp_hugepage_size_numa(int node, size_t *page_size, size_t *page_num);

struct xdp_mempool_ops hugepage_mempool_ops = {
    .private_data = XDP_HUGEPAGE_HOME,
    .init = xdp_hugepage_init,
    .release = xdp_hugepage_release,
    .reserve = xdp_hugepage_reserve,
    .revert = xdp_hugepage_revert,
};

inline struct xdp_mempool_ops *xdp_hugepage_get_ops(void)
{
    return &hugepage_mempool_ops;    
}

int xdp_hugepage_init(void *data)
{
    char  path[PATH_MAX];
    int   ret = -1;
    char *dir = (char *)data;

    snprintf(path, PATH_MAX, "%s/%s", dir, XDP_HUGEPAGE_SUBDIR);
    ret = mount("nodev", path, "hugetlbfs", MS_MGC_VAL, NULL);
    if (ret < 0) {
        ERR_OUT("mount hugetlbfs on %s failed, err %d", path, errno);
    }

    return ret;
}

int xdp_hugepage_release(void *data)
{
    char     path[PATH_MAX];
    int      ret = -1;
    char    *dir = (char *)data;

    snprintf(path, PATH_MAX, "%s/%s/%s", dir, XDP_HUGEPAGE_SUBDIR,
        XDP_HUGEPAGE_MAP_FILE);
    unlink(path);

    snprintf(path, PATH_MAX, "%s/%s", dir, XDP_HUGEPAGE_SUBDIR);
    ret = umount2(path, MNT_FORCE);
    if (ret < 0) {
        ERR_OUT("umount hugetlbfs from %s failed, err %d", path, errno);
    }

    return ret;
}

static int xdp_hugepage_size(size_t *page_size, size_t *page_num)
{
    return xdp_hugepage_info(SYS_HUGEPAGE_PATH, page_size, page_num);
}

static int xdp_hugepage_size_numa(int node, size_t *page_size, size_t *page_num)
{
    char path[PATH_MAX];
    snprintf(path, PATH_MAX, NUMA_HUGEPAGE_PATH, node);
    return xdp_hugepage_info(path, page_size, page_num);
}


static struct xdp_mempool *
xdp_hugepage_reserve(int node, size_t size, __attribute__((unused))void *data)
{
    size_t reserve_size;
    int    fd = -1;
    char   path[PATH_MAX];
    size_t page_size;
    size_t page_num;
    size_t avail_size;
    int    ret = -1;
    void  *addr = NULL;
    char  *dir;
    struct xdp_mempool *pool = NULL;

    dir = (char *)data;
    snprintf(path, PATH_MAX, "%s/%s/%s", dir,
        XDP_HUGEPAGE_SUBDIR, XDP_HUGEPAGE_MAP_FILE);
    fd = open(path, O_CREAT | O_RDWR, 0600);
    if (fd < 0) {
        ERR_OUT("open %s failed, err: %d", path, errno);
        goto out;
    }

    if (node < 0) {
        ret = xdp_hugepage_size(&page_size, &page_num);
    } else {
        ret = xdp_hugepage_size_numa(node, &page_size, &page_num);
    }
    if (ret < 0 || !page_size || !page_num) {
        goto out;    
    }
    reserve_size = XDP_ALIGN(size + sizeof(struct xdp_mempool), page_size);
    avail_size = page_size * page_num;
    if (reserve_size > avail_size) {
        ERR_OUT("size(%lu) is more than hugepage size(%lu)",
            reserve_size, avail_size);
        goto out;
    }
    ret = ftruncate(fd, reserve_size);
    if (ret < 0) {
        ERR_OUT("ftruncate failed, err: %d", errno);
        goto out;
    }
    addr = mmap(NULL, reserve_size, PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_POPULATE | MAP_FIXED,
        fd, 0);
    if (addr == MAP_FAILED) {
        ERR_OUT("mmap failed, err: %d", errno);
        goto out;
    }
    pool = (struct xdp_mempool *)addr;
    pool->total_size = reserve_size;
    pool->start =
        (void *)XDP_ALIGN((size_t)addr + sizeof(struct xdp_mempool), 4096);
    pool->end = addr + reserve_size;
    pool->last = pool->start;
    pool->size = reserve_size - (pool->start - addr);
    pool->numa_node = node;
    pool->ops_index = XPD_HUGEPAGE_OPS;
out:
    close(fd);
    return pool;
}

static int
xdp_hugepage_revert(struct xdp_mempool *pool, __attribute__((unused))void *data)
{
    return munmap(pool, pool->total_size);
}


static int
xdp_hugepage_info(const char *path, size_t *page_size, size_t *page_num)
{
    const char prefix[] = "hugepages-";
    const size_t prefix_len = sizeof(prefix) - 1;
    struct dirent *dirent;
    char    *end_ptr;
    DIR     *dir = NULL;
    int      ret = -1;
    unsigned long long size;
    unsigned long long max_size = 0;
    unsigned long      free_pages = 0;

    dir = opendir(path);
    if (!dir) {
        ERR_OUT("open %s failed", path);    
        goto out;
    }
    while ((dirent = readdir(dir))) {
        if (strncmp(dirent->d_name, prefix, prefix_len)) {
            continue;
        }
        size = strtoll(&dirent->d_name[prefix_len], &end_ptr, 0);           
        switch(*end_ptr) {
            case 'g':
            case 'G':
                size <<= 10;
            case 'm':
            case 'M':
                size <<= 10;
            case 'K':
            case 'k':
                size <<= 10;
            default:
                break;
        }
        if (max_size < size) {
            max_size = size;
            free_pages = xdp_hugepage_free_pages(path, dirent->d_name);
        }
    }

    *page_size = (size_t)max_size;
    *page_num = (size_t)free_pages;
    ret = 0;
out:
    closedir(dir);
    return ret;
}

static unsigned long xdp_hugepage_free_pages(const char *dir, const char *subdir)
{
    unsigned long long ret = 0;
    const char *free_page_file = "free_hugepages";            
    char        path[PATH_MAX];
    char       *end_ptr;
    FILE       *fp = NULL;
    char        buf[BUFSIZ];

    snprintf(path, PATH_MAX, "%s/%s/%s", dir, subdir, free_page_file);
   
    fp = fopen(path, "r");
    if (!fp) {
        ERR_OUT("fopen %s failed", path);
        return 0;
    }
    if (!fgets(buf, sizeof(buf), fp)) {
        ERR_OUT("read %s failed", path);
        goto out;
    }
    ret = strtol(buf, &end_ptr, 0); 
    if (!buf[0] || !end_ptr || *end_ptr != '\n') {
        ERR_OUT("can not parse value in %s", path);
        ret = 0;
        goto out;
    }
out:
    fclose(fp);
    return ret;
}
