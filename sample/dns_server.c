#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/in.h>

#include "xdp_runtime.h"
#include "xdp_prefetch.h"
#include "xdp_endian.h"
#include "xdp_ipv4.h"

#define CHECK_QUERY_LEN(l, dl, ht, ql) \
xdp_runtime_unlikely((l) != ((dl) + sizeof(struct ht)) || (ql) < DNS_HEAD_SIZE)

int running = 1;

static int frame_process_ipv4(struct xdp_frame *frame)
{
    struct ethhdr   *ethhdr;
    struct iphdr    *iphdr;
    struct udphdr   *udphdr;
    uint8_t         *dns;
    uint16_t         total_len;
    uint16_t         hdr_len;
    uint16_t         udp_len;
    uint16_t         dns_len;

    ethhdr = xdp_frame_get_addr(frame, struct ethhdr *);
    iphdr = (struct iphdr *)(ethhdr + 1);
    udphdr = (struct udphdr *)(iphdr + 1);

    hdr_len = xdp_ipv4_get_hdr_len(iphdr);
    total_len = xdp_ipv4_get_total_len(iphdr); 
    if (!xdp_ipv4_check_len(hdr_len, total_len, frame)) {
        xdp_framepool_free_frame(frame);
        return 0;
    }

    udp_len = xdp_ntohs(udphdr->dgram_len);
    dns_len = udp_len - sizeof(struct udphdr);
    if (CHECK_QUERY_LEN(total_len, udp_len, sizeof(struct iphdr), dns_len)) {
        xdp_framepool_free_frame(frame);
        return 0;
    }

    dns = (uint8_t *)(udphdr + 1);
    dns_parse(dns, dns_len, iphdr->src_addr); 
}

static int frame_process_ipv6(struct xdp_frame *frame)
{
}

static int frame_process(struct xdp_frame *frame)
{
    struct ethhdr *ethhdr;
    uint16_t       eth_type;

    ethhdr = xdp_frame_get_addr(frame, struct ethhdr *);
    eth_type = xdp_ntohs(ethhdr->h_proto);
    switch (eth_type) {
        case ETH_P_IP:    
            ret = frame_process_ipv4(frame);
            break;
        case ETH_P_IPV6:    
            ret = frame_process_ipv6(frame);
            break;
        default:
            xdp_framepool_free_frame(frame);
            ret = -1;
            break;
    }

    return ret;
}

int worker(volatile void *args)
{
    struct xdp_frame *bufs[32];
    int               rcvd;
    int               sent;

    while (running) {
        rcvd = xdp_dev_read(0, bufs, 32);
        if (xdp_runtime_unlikely(rcvd < 0)) {
            ERR_OUT("xdp_dev_read queue %d, failed", 0);
            return -1;
        }
        if (xdp_runtime_likely(rcvd > 0)) {
            sent = xdp_dev_write(0, bufs, rcvd);
        }
    }

    return 0;
}

int main(void)
{
    int         ret;
    const char *eth2 = "eth2";
    struct xdp_runtime runtime;
    
    ret = xdp_runtime_init(&runtime, eth2);
    if (ret < 0) {
        ERR_OUT("xdp_runtime_init failed");
        goto out;
    }
    
    ret = xdp_runtime_udp_packet(53);
    if (ret < 0) {
        ERR_OUT("xdp_runtime_udp_packet 53 failed");
        goto out;
    }

    ret = xdp_runtime_setup_queue(&runtime, 1, 1024);
    if (ret < 0) {
        ERR_OUT("xdp_runtime_setup_queue failed");
        goto out;
    }
    
    ret = xdp_runtime_setup_workers(&runtime, 1);
    if (ret < 0) {
        ERR_OUT("xdp_runtime_setup_workers failed");
        goto out;
    }

out:
    xdp_runtime_release(&runtime);
    return ret;
}
