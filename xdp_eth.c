/*
 * huangying email: hy_gzr@163.com
 */
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/ethtool.h>

#include "xdp_log.h"
#include "xdp_eth.h"

#ifndef SIOCETHTOOL
#define SIOCETHTOOL     0x8946
#endif

#ifndef ETHTOOL_GCHANNELS
#define ETHTOOL_GCHANNELS   0x0000003c
#endif

#ifndef ETHTOOL_SCHANNELS
#define ETHTOOL_SCHANNELS   0x0000003d
#endif

#define ETH_NUMA_NODE_PATH "/sys/class/net/%s/device/numa_node"

struct eth_channels {
    uint32_t   cmd;
    uint32_t   max_rx;
    uint32_t   max_tx;
    uint32_t   max_other;
    uint32_t   max_combined;
    uint32_t   rx_count;
    uint32_t   tx_count;
    uint32_t   other_count;
    uint32_t   combined_count;
};


static int _xdp_eth_get_queue(const char *ifname, struct eth_channels *channel)
{
    struct ifreq ifr;
    int     ret = -1;
    int     fd;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        ERR_OUT("create socket failed, err %d", errno);
        return -1;
    }

    channel->cmd = ETHTOOL_GCHANNELS;
    ifr.ifr_data = (void *)channel;
    snprintf(ifr.ifr_name, IF_NAMESIZE, "%s", ifname);
    ret = ioctl(fd, SIOCETHTOOL, &ifr);
    if (ret < 0) {
        if (errno != EOPNOTSUPP) {
            ERR_OUT("ioctl for get channels failed, err %d", errno);
            goto out;
        }
        ret = 0;
    }
out:
    close(fd);
    return ret;
}

int xdp_eth_get_queue(const char *ifname, int *max_queue, int *curr_queue)
{
    struct eth_channels channel;
    int ret;
    ret = _xdp_eth_get_queue(ifname, &channel);
    if (ret < 0) {
        goto out;
    }
    if (channel.max_combined == 0 || errno == EOPNOTSUPP) {
        channel.max_combined = 1;
        channel.combined_count = 1;
    }

    if (max_queue) {
        *max_queue = channel.max_combined;
    }
    if (curr_queue) {
        *curr_queue = channel.combined_count;
    }

    ret = 0;

out:
    return ret;
}

int xdp_eth_set_queue(const char *ifname, int queue)
{
    struct eth_channels channel;
    struct ifreq ifr;
    int     ret = -1;
    int     fd;

    ret = _xdp_eth_get_queue(ifname, &channel);
    if (ret < 0) {
        return -1;
    }
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        ERR_OUT("create socket failed, err %d", errno);
        return -1;
    }

    channel.cmd = ETHTOOL_SCHANNELS;
    channel.combined_count = queue;
    ifr.ifr_data = (void *)&channel;
    snprintf(ifr.ifr_name, IF_NAMESIZE, "%s", ifname);
    ret = ioctl(fd, SIOCETHTOOL, &ifr);
    if (ret < 0) {
        ERR_OUT("ioctl for set channels failed, err %d", errno);
        goto out;
    }
    ret = 0;

out:
    close(fd);
    return ret;
}

int xdp_eth_get_info(const char *ifname, struct xdp_iface *iface)
{
    int ret = -1;
    struct ifreq ifr;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        ERR_OUT("create socket failed, err %d", errno);
        return -1;
    }
    
    snprintf(ifr.ifr_name, IF_NAMESIZE, "%s", ifname);
    ret = ioctl(sock, SIOCGIFINDEX, &ifr);
    if (ret < 0) {
        ERR_OUT("ioctl for get mac addr failed, err %d", errno);
        goto out;
    }
    iface->ifindex = ifr.ifr_ifindex;
    
    if (ioctl(sock, SIOCGIFHWADDR, &ifr)) {
        ERR_OUT("ioctl for get mac addr failed, err %d", errno);
        goto out;
    }
    
    memcpy(iface->mac_addr, ifr.ifr_hwaddr.sa_data, XDP_ETH_MAC_ADDR_LEN);
    snprintf(iface->ifname, IF_NAMESIZE, "%s", ifname);
    ret = 0; 

out:
    close(sock);
    return ret;
}

int xdp_eth_numa_node(const char *ifname)
{
    char     path[PATH_MAX];
    char     buf[128];
    int      fd;
    int      ret = -1;
    ssize_t  len;

    snprintf(path, PATH_MAX, ETH_NUMA_NODE_PATH, ifname);
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        ERR_OUT("open %s failed", path);
        goto out;
    }

    len = read(fd, buf, sizeof(buf) - 1);
    if (len < 0) {
        ERR_OUT("read %s failed, err: %d", path, errno);    
        goto out;
    }
    buf[len] = 0;
    ret = atoi(buf);
out:
    close(fd);
    return ret;
}


static inline int xdp_flow_type_supported(uint32_t flow_type)
{
    int ret = 0;
    switch (flow_type) {
        case XDP_TCP_V4_FLOW:
        case XDP_UDP_V4_FLOW:
        case XDP_TCP_V6_FLOW:
        case XDP_UDP_V6_FLOW:
            ret = 1; 
            break;
    }

    return ret;
}
static inline int xdp_rss_key_supported(uint32_t flow_type, uint32_t key)
{
    int ret = 0;
    switch (flow_type) {
        case XDP_TCP_V4_FLOW:
        case XDP_TCP_V6_FLOW:
        case XDP_UDP_V4_FLOW:
        case XDP_UDP_V6_FLOW:
            ret = !!(key == XDP_RXH_LAYER4);
            break;
    }
    return ret;
}

static const char *xdp_flow_type_to_str(uint32_t flow_type)
{
    switch (flow_type) {
        case XDP_TCP_V4_FLOW:
            return "ipv4 tcp";
        case XDP_UDP_V4_FLOW:
            return "ipv4 udp";
        case XDP_TCP_V6_FLOW:
            return "ipv6 tcp";
        case XDP_UDP_V6_FLOW:
            return "ipv6 udp";
    }
    return "unknown";
}

uint32_t xdp_eth_get_rss(const char *ifname, uint32_t flow_type)
{
    struct ethtool_rxnfc nfccmd;
    struct ifreq         ifr;

    uint64_t    key = 0;            
    int         ret = 0;
    int         fd = -1;

    if (!xdp_flow_type_supported(flow_type)) {
        ERR_OUT("flow type %X is not supported", flow_type);
        return 0;
    }
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        ERR_OUT("create socket failed, err %d", errno);
        return 0;
    }
    memset(&ifr, 0, sizeof(struct ifreq));
    snprintf(ifr.ifr_name, IF_NAMESIZE, "%s", ifname);
    ifr.ifr_data = (void *)&nfccmd;

    nfccmd.cmd = ETHTOOL_GRXFH;
    nfccmd.flow_type = flow_type;
    
    ret = ioctl(fd, SIOCETHTOOL, &ifr);
    if (ret < 0) {
        ERR_OUT("ioctl for get rss failed, errno %d", errno);
        goto out;
    }
    if ((flow_type & ~XDP_FLOW_RSS) != flow_type) {
        ERR_OUT("%s for rss is not supported", xdp_flow_type_to_str(flow_type));
        goto out;
    }
    
    key = nfccmd.data;
    if (!key) {
        ERR_OUT("%s for rss have no hash key", xdp_flow_type_to_str(flow_type));
        goto out;
    }
    if (key & XDP_RXH_DISCARD) {
        ERR_OUT("%s for rss all discard", xdp_flow_type_to_str(flow_type));
        key = 0;
    }
    
out:
    close(fd);
    return key;
}

int xdp_eth_set_rss(const char *ifname, uint32_t flow_type, uint32_t set_key)
{
    struct ethtool_rxnfc nfccmd;
    struct ifreq         ifr;

    int         fd = -1;
    int         ret = 0;
    uint32_t    get_key = 0;

    if (!xdp_flow_type_supported(flow_type)) {
        ERR_OUT("flow type %X is not supported", flow_type);
        return -1;
    }
    if (!xdp_rss_key_supported(flow_type, set_key)) {
        ERR_OUT("rss key %X is not supported for flow type %X",
            set_key, flow_type);
        return -1;
    }

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        ERR_OUT("create socket failed, err %d", errno);
        return -1;
    }
    memset(&ifr, 0, sizeof(struct ifreq));
    snprintf(ifr.ifr_name, IF_NAMESIZE, "%s", ifname);
    ifr.ifr_data = (void *)&nfccmd;

    nfccmd.cmd = ETHTOOL_SRXFH;
    nfccmd.flow_type = flow_type;
    nfccmd.data = set_key;
    
    ret = ioctl(fd, SIOCETHTOOL, &ifr);
    if (ret < 0) {
        ERR_OUT("ioctl for set rss failed, errno %d", errno);
        goto out;
    }

    get_key = xdp_eth_get_rss(ifname, flow_type);
    if (get_key & set_key) {
        goto out;
    }
    ret = -1;
out:
    close(fd);
    return ret;
}
