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

