#ifndef _SYNFLOOD_H_
#define _SYNFLOOD_H_
#include "xdp_framepool.h"
#define MAX_QUEUE 128
#define BUF_NUM  64
struct Channel {
    int sendCount;
    struct xdp_frame *send_bufs[BUF_NUM];
    Channel(void)
    {
        sendCount = 0; 
        memset(send_bufs, 0, sizeof(struct xdp_frame *) * BUF_NUM);
    }
};

class SynFlood {
private:
    static volatile bool running;
    static struct Channel channelList[MAX_QUEUE];
    static in_addr_t dstIp;
    static uint16_t  dstPort;
    static uint64_t  packetCount;
    static __thread uint64_t sendCount;
    static void signalHandler(int sig);
public:
    static void setSignal(void);
    static void setPacketCount(uint64_t c);
    static void setDstAddr(const char *ip, uint16_t port);    
    static int sender(volatile void *args);
    static void initPacket(struct xdp_frame *frame, struct Channel *ch);
};
#endif
