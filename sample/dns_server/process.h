#ifndef _PROCESS_H_
#define _PROCESS_H_

#include <string.h>
#include <linux/if_ether.h>
#include "xdp_framepool.h"

#define MAX_QUEUE 128
#define BUF_NUM  32
struct Channel {
    int sendCount;
    struct xdp_frame *send_bufs[BUF_NUM];
    Channel(void)
    {
        sendCount = 0; 
        memset(send_bufs, 0, sizeof(struct xdp_frame *) * BUF_NUM);
    }
};

class DnsProcess {
private:
    static volatile bool running;
    static struct Channel channelList[MAX_QUEUE];
public:
    static int worker(volatile void *args);
    static void swapPort(uint16_t &src, uint16_t &dst);
    static void swapIp(uint32_t &src, uint32_t &dst);
    static void swapMac(unsigned char src[ETH_ALEN], unsigned char dst[ETH_ALEN]);

    //static int process(struct xdp_frame *frame);
    static int processIpv4(struct xdp_frame *frame, struct Channel *chn);
    //static int processIpv6(struct xdp_frame *frame);
};
#endif
