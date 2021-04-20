/*
 * huangying email: hy_gzr@163.com
 */
#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <string.h>

#include "bpf.h"
#include "linux/bpf.h"
#include "linux/if_link.h"
#include "bpf/libbpf.h"
#include "bpf_endian.h"

#include "xdp_prog.h"
#include "xdp_log.h"
#include "xdp_kern_prog/xdp_kern_prog.h"

#define XDP_KERN_PROG "xdp_kern_prog.o"
#define XDP_KERN_SEC "xdp_sock"

#ifndef XDP_PROGSEC_LEN
#define XDP_PROGSEC_LEN (32)
#endif

enum {
    L3_MAP_FD = 0,
    IPV4_SRC_MAP_FD,
    IPV6_SRC_MAP_FD,
    IPV4_DST_MAP_FD,
    IPV6_DST_MAP_FD,
    L4_MAP_FD,
    TCPPORT_MAP_FD,
    UDPPORT_MAP_FD,
    XSKS_MAP_FD,
    MAX_MAP_FD
};

struct xdp_prog_conf {
    struct xdp_iface    iface;
    char                prog_path[PATH_MAX];
    char                section[XDP_PROGSEC_LEN];
    __u32               xdp_flags;
};

struct xdp_prog {
    struct bpf_object   *bpf_obj;
    struct bpf_program  *bpf_prog;
    struct xdp_prog_conf      cfg;
    int                  prog_fd;
    int                  map_fd[MAX_MAP_FD];

};

static struct xdp_prog xdp_rt;

static int xdp_load_prog(const struct xdp_prog_conf *cfg, struct xdp_prog *xdp_rt);
static int xdp_unload_prog(const struct xdp_prog_conf *cfg);
static struct bpf_object* load_bpf_object_file(
    const char *filename, int ifindex);
static int xdp_link_attach(int ifindex, __u32 xdp_flags, int prog_fd);
static int xdp_link_detach(int ifindex, __u32 xdp_flags,
    __u32 expected_prog_id);
static int xdp_update_map(int map_fd, __u32 key, __u32 val);
static int xdp_find_map(struct bpf_object *bpf_obj, const char *map_name);

int xdp_prog_init(const char *ifname, const char *prog, const char *section)
{
    int ret = -1;
    int fd;
    struct bpf_object *bpf_obj;
    struct rlimit rlim = {RLIM_INFINITY, RLIM_INFINITY};
    struct xdp_prog_conf cfg;

    if (setrlimit(RLIMIT_MEMLOCK, &rlim)) {
        ERR_OUT("setrlimit(RLIMIT_MEMLOCK) \"%s\"\n", strerror(errno));
        return -1;
    }
    memset(&xdp_rt, 0, sizeof(struct xdp_prog));
    snprintf(cfg.prog_path, PATH_MAX, "%s", prog ? prog : XDP_KERN_PROG);
    snprintf(cfg.section, XDP_PROGSEC_LEN, "%s",
        section ? section : XDP_KERN_SEC);
    ret = xdp_eth_get_info(ifname, &cfg.iface);
    if (ret < 0) {
        return -1;
    }
    cfg.xdp_flags = XDP_FLAGS_UPDATE_IF_NOEXIST | XDP_FLAGS_DRV_MODE;
    ret = xdp_load_prog(&cfg, &xdp_rt);
    if (ret < 0) {
        return -1;
    }

    //here find map
    bpf_obj = xdp_rt.bpf_obj;
    fd = xdp_find_map(bpf_obj, textify(MAP_NAME(layer3)));
    if (fd < 0) {
        goto out;
    }
    xdp_rt.map_fd[L3_MAP_FD] = fd;
    
    fd = xdp_find_map(bpf_obj, textify(MAP_NAME(ipv4_src)));
    if (fd < 0) {
        goto out;
    }
    xdp_rt.map_fd[IPV4_SRC_MAP_FD] = fd;

    fd = xdp_find_map(bpf_obj, textify(MAP_NAME(ipv6_src)));
    if (fd < 0) {
        goto out;
    }
    xdp_rt.map_fd[IPV6_SRC_MAP_FD] = fd;

    fd = xdp_find_map(bpf_obj, textify(MAP_NAME(layer4)));
    if (fd < 0) {
        goto out;
    }
    xdp_rt.map_fd[L4_MAP_FD] = fd;

    fd = xdp_find_map(bpf_obj, textify(MAP_NAME(tcp_port)));
    if (fd < 0) {
        goto out;
    }
    xdp_rt.map_fd[TCPPORT_MAP_FD] = fd;

    fd = xdp_find_map(bpf_obj, textify(MAP_NAME(udp_port)));
    if (fd < 0) {
        goto out;
    }
    xdp_rt.map_fd[UDPPORT_MAP_FD] = fd;

    fd = xdp_find_map(bpf_obj, textify(MAP_NAME(xsks_map)));
    if (fd < 0) {
        goto out;
    }
    xdp_rt.map_fd[XSKS_MAP_FD] = fd;
    memcpy(&xdp_rt.cfg, &cfg, sizeof(struct xdp_prog_conf));

    ret = 0;
out:
    if (ret < 0) {
        xdp_prog_release();
    }
    return ret;
}

void xpd_prog_release(void)
{
    if (xdp_unload_prog(&xdp_rt.cfg) < 0) {
        DEBUG_OUT("xdp_unload_prog failed");
    }
    
    if (xdp_rt.bpf_prog) {
        bpf_program__unload(xdp_rt.bpf_prog);
        xdp_rt.bpf_prog = NULL;
    }
    if (xdp_rt.bpf_obj) {
        bpf_object__close(xdp_rt.bpf_obj);
        xdp_rt.bpf_obj = NULL;
    }
}

inline int xdp_prog_update_l3(uint16_t l3_proto, uint32_t action)
{
    __u32 key = (__u32)bpf_htons(l3_proto);
    __u32 val = (__u32)action;
    return xdp_update_map(xdp_rt.map_fd[L3_MAP_FD], key, val);
}

inline int
xdp_prog_update_ipv4(struct in_addr *addr, uint32_t prefix,
    int type, uint32_t action)
{
    struct bpf_lpm_trie_key *key;
    uint32_t                 size;
    __u32                    val;
    int                      map_fd;

    if (type == XDP_UPDATE_IP_SRC) {
        map_fd = xdp_rt.map_fd[IPV4_SRC_MAP_FD];
    } else {
        map_fd = xdp_rt.map_fd[IPV4_DST_MAP_FD];
    }
    size = sizeof(struct in_addr);
    key = alloca(sizeof(struct bpf_lpm_trie_key) + size);
    key->prefixlen = prefix;
    memcpy(key->data, addr, size);
    val = (__u32)action;
    if (bpf_map_update_elem(map_fd, key, &val, BPF_ANY) < 0) {
        ERR_OUT("update ipv4 map failed, errno %d", errno);
        return -1;
    }
    return 0;
}

inline int
xdp_prog_update_ipv6(struct in6_addr *addr, uint32_t prefix,
    int type, uint32_t action)
{
    struct bpf_lpm_trie_key *key;
    uint32_t                 size;
    __u32                    val;
    int                      map_fd;

    if (type == XDP_UPDATE_IP_SRC) {
        map_fd = xdp_rt.map_fd[IPV6_SRC_MAP_FD];
    } else {
        map_fd = xdp_rt.map_fd[IPV6_DST_MAP_FD];
    }
    size = sizeof(struct in6_addr);
    key = alloca(sizeof(struct bpf_lpm_trie_key) + size);
    key->prefixlen = prefix;
    memcpy(key->data, addr, size);
    val = (__u32)action;
    if (bpf_map_update_elem(map_fd, key, &val, BPF_ANY) < 0) {
        ERR_OUT("update ipv6 src map failed, errno %d", errno);
        return -1;
    }
    return 0;
}

inline int xdp_prog_update_l4(uint8_t l4_proto, uint32_t action)
{
    __u32 key = (__u32)l4_proto;
    __u32 val = (__u32)action;
    return xdp_update_map(xdp_rt.map_fd[L4_MAP_FD], key, val);
}

inline int xdp_prog_update_tcpport(uint16_t port, uint32_t action)
{
    __u32 key = (__u32)bpf_htons(port);
    __u32 val = (__u32)action;
    return xdp_update_map(xdp_rt.map_fd[TCPPORT_MAP_FD], key, val);
}

inline int xdp_prog_update_udpport(uint16_t port, uint32_t action)
{
    __u32 key = (__u32)bpf_htons(port);
    __u32 val = (__u32)action;
    return xdp_update_map(xdp_rt.map_fd[UDPPORT_MAP_FD], key, val);
}

int xdp_load_prog(const struct xdp_prog_conf *cfg, struct xdp_prog *xdp_rt)
{
    int                  offload_ifindex = 0;
    int                  prog_fd = -1;
    int                  err;

    struct bpf_program  *bpf_prog;
    struct bpf_object   *bpf_obj;

    if (!cfg) {
        ERR_OUT("ELF prog configuration error");
        return -1;
    }
    if (!cfg->prog_path[0] || !cfg->section[0]) {
        ERR_OUT("xdp prog path or section error");
        return -1;
    }
    if (cfg->xdp_flags & XDP_FLAGS_HW_MODE) {
        offload_ifindex = cfg->iface.ifindex;
    }

    bpf_obj = load_bpf_object_file(cfg->prog_path, offload_ifindex);
    if (!bpf_obj) {
        ERR_OUT("load BPF-OBJ failed %s", cfg->prog_path);
        return -1;
    }

    bpf_prog = bpf_object__find_program_by_title(bpf_obj, cfg->section);
    if (!bpf_prog) {
        ERR_OUT("couldn't find a program in ELF section '%s'", cfg->section);
        bpf_object__close(bpf_obj);
        return -1;
    }

    prog_fd = bpf_program__fd(bpf_prog);
    if (prog_fd <= 0) {
        ERR_OUT("bpf_program_fd failed");
        bpf_program__unload(bpf_prog);
        bpf_object__close(bpf_obj);
        return -1;
    }

    err = xdp_link_attach(cfg->iface.ifindex, cfg->xdp_flags, prog_fd);
    if (err < 0) {
        ERR_OUT("attach %s(%s)to %s failed",
            cfg->prog_path, cfg->section, cfg->iface.ifname);
        bpf_program__unload(bpf_prog);
        bpf_object__close(bpf_obj);
        return -1;
    }
    xdp_rt->bpf_obj = bpf_obj;
    xdp_rt->bpf_prog = bpf_prog;
    xdp_rt->prog_fd = prog_fd;
    return 0;
}

int xdp_unload_prog(const struct xdp_prog_conf *cfg)
{
    int     err;
    __u32   curr_prog_id = 0;
    err = xdp_link_detach(cfg->iface.ifindex, cfg->xdp_flags, 0);
    if (err < 0) {
        ERR_OUT("detach from %s failed", cfg->iface.ifname);
        return -1;
    }

    err = bpf_get_link_xdp_id(cfg->iface.ifindex,
        &curr_prog_id, cfg->xdp_flags);
    if (curr_prog_id != 0) {
        ERR_OUT("detach from %s error,maybe unload mode is not match",
            cfg->iface.ifname);
        return -1;
    }
    return 0; 
}

static int xdp_update_map(int map_fd, __u32 key, __u32 val)
{
    int err;
    err = bpf_map_update_elem(map_fd, &key, &val, BPF_ANY);
    if (err < 0) {
        ERR_OUT("update map key(%u) value(%u) err %d", key, val, err);
        return -1;
    }

    return 0;
}

struct bpf_object *load_bpf_object_file(const char *filename, int ifindex)
{
    int                prog_fd = -1;
    int                err;
    struct bpf_object *obj;
    
    struct bpf_prog_load_attr prog_load_attr = {
            .prog_type = BPF_PROG_TYPE_XDP,
            .ifindex   = ifindex,
            .file = filename,
    };
    
    err = bpf_prog_load_xattr(&prog_load_attr, &obj, &prog_fd);
    if (err < 0) {
        ERR_OUT("load BPF-OBJ %s %d: %s", filename, err, strerror(-err));
        return NULL;
    }

    return obj;
}

int xdp_link_attach(int ifindex, __u32 xdp_flags, int prog_fd)
{
	int err;

	err = bpf_set_link_xdp_fd(ifindex, prog_fd, xdp_flags);
	if (err < 0) {
		ERR_OUT("ifindex(%d) link set xdp fd failed (%d): %s",
			ifindex, -err, strerror(-err));
		switch (-err) {
		case EBUSY:
		case EEXIST:
			ERR_OUT("XDP already loaded on device");
			break;
		case EOPNOTSUPP:
			ERR_OUT("Native-XDP not supported");
			break;
		default:
			break;
		}
		return err;
	}
    return 0;
}

int xdp_link_detach(int ifindex, __u32 xdp_flags, __u32 expected_prog_id)
{
    __u32   curr_prog_id;
    int     err;

    err = bpf_get_link_xdp_id(ifindex, &curr_prog_id, xdp_flags);
    if (err) {
        ERR_OUT("get link xdp id failed (err=%d): %s", -err, strerror(-err));
        return -1;
    }

    if (!curr_prog_id) {
        ERR_OUT("no curr XDP prog on ifindex:%d", ifindex);
        return 0;
    }

    if (expected_prog_id && curr_prog_id != expected_prog_id) {
        ERR_OUT("expected prog ID(%d) no match(%d), not removing",
            expected_prog_id, curr_prog_id);
        return -1;
    }

    if ((err = bpf_set_link_xdp_fd(ifindex, -1, xdp_flags)) < 0) {
        ERR_OUT("link set xdp failed (err=%d): %s", err, strerror(-err));
        return -1;
    }

    return 0;
}

int xdp_find_map(struct bpf_object *bpf_obj, const char *map_name)
{
    struct bpf_map *map;

    map = bpf_object__find_map_by_name(bpf_obj, map_name);
    if (!map) {
        ERR_OUT("cannot find map by name %s", map_name);
        return -1;
    }
    
    return bpf_map__fd(map); 
}
