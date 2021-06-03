#ifndef _SYNFLOOD_H_
#define _SYNFLOOD_H_
#include <linux/if_ether.h>
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
    static in_addr_t srcIp;
    static uint64_t  packetCount;
    static __thread long sendCount;
    static void signalHandler(int sig);
    static uint8_t dstMac[ETH_ALEN];
    static uint8_t srcMac[ETH_ALEN];
    static int setMac(const char *mac, uint8_t addr[ETH_ALEN]);
    static int8_t digtal(char ch)
    {
        if (ch >= '0' && ch <= '9') {
            return ch - '0';
        }
        if (ch >= 'a' && ch <= 'f') {
            return ch - 'a' + 10;
        }
        if (ch >= 'A' && ch <= 'F') {
            return ch - 'A' + 10;
        }
        return -1;
    }
    static long rate;

public:
    static void setRate(long r)
    {
        rate = r;
    }
    static int setDstMac(const char *mac);
    static int setSrcMac(const char *mac);
    static void setSignal(void);
    static void setPacketCount(uint64_t c);
    static void setDstAddr(const char *ip, uint16_t port);    
    static void setSrcAddr(const char *ip);
    static int sender(volatile void *args);
    static void initPacket(struct xdp_frame *frame, struct Channel *ch);
    static void updatePacket(struct xdp_frame *frame);
};
#endif
