#include "process.h"
#include "packet.h"

#define DNS_HEAD_SIZE 12

#define CHECK_DNS_LEN(l, dl, ql) \
xdp_runtime_unlikely((l) != ((dl) + sizeof(struct iphdr)) || (ql) < DNS_HEAD_SIZE)

volatile bool Process::running = true;
struct Channel Process::channelList[MAX_QUEUE];

int Process::processIpv4(struct xdp_frame *frame, struct Channel *chn)
{
    struct ethhdr *ethhdr;
    struct iphdr  *iphdr;
    struct udphdr *udphdr;
    uint8_t       *dns;

    uint16_t       ipv4HdrLen;
    uint16_t       ipv4TotalLen;
    uint16_t       l3PayloadLen;
    uint16_t       udpLen;


    ethhdr = xdp_frame_get_addr(frame, struct ethhdr);
    iphdr = ethhdr + 1;
    udphdr = iphdr + 1;

    ipv4HdrLen = xdp_ipv4_get_hdr_len(iphdr);
    ipvTtotalLen = xdp_ipv4_get_total_len(iphdr);
    if (!xdp_ipv4_check_len(ipv4HdrLen, ipv4TotalLen, frame)) {
        xdp_framepool_free_frame(frame);
        return 0;
    }

    udpLen = xdp_ntohs(udphdr->len);
    dnsLen = udpLen - sizeof(struct udphdr);
    if (CHECK_DNS_LEN(ipv4TotalLen, udpLen, dnsLen)) {
        xdp_framepool_free_frame(frame);
        return 0;
    }
        
    dns = udphdr + 1;
    packet.parse(dns);
    packet.setDomainIpGroup("127.0.0.2");
    packetLen = packet.pack((char *)dns);

    swapPort(udphdr->source, udphdr->dest);
    udphdr->len = xdp_htons(sizeof(struct udphdr) + packetLen);
    udphdr->check = 0;

    swapIp(iphdr->saddr, iphdr->daddr);
    iphdr->ttl = 64;
    l3PayloadLen = sizeof(struct iphdr) + sizeof(struct udphdr) + packetLen;
    iphdr->tot_len = xdp_htons(l3PayloadLen);
    iphdr->check = 0;
    iphdr->check = xdp_ipv4_phdr_checksum(iphdr);

    udphdr->check = xdp_ipv4_udptcp_checksum(iphdr, udphdr);

    swapMac(ethhdr->h_source, ethhdr->h_dest);
    frame->data_len = l3PayloadLen + sizeof(struct ethhdr);

    chn->send_bufs[chn->sendCount++] = frame;
    return 0;
}

void Process::swapPort(uint16_t &src, uint16_t &dst)
{
    src ^= dst;
    dst ^= src;
    src ^= dst;
}

void Process:swapIp(uint32_t &src, uint32_t &dst)
{
    src ^= dst;
    dst ^= src;
    src ^= dst;
}

void Process::swapMac(char src[ETH_ALEN], char dst[ETH_ALEN])
{
    for (int i = 0; i < ETH_ALEN; i++) {
        src[i] ^= dst[i];
        dst[i] ^= src[i];
        src[i] ^= dst[i];
    }
}

int Process:worker(volatile void *args)
{
    uint16_t     qIdx;
    int          rcvd;
    int          sent;
    int          i;
    struct xdp_frame *frame[32];
    struct Channel   *chn;

    qIdx = *(uint16_t *)args;
    chn = &channelList[qIdx];
    
    while (running) {
        rcvd = xdp_dev_read(qIdx, frame, 32);
        for (i = 0; i < 3 && i < rcvd; i++) {
            xdp_prefetch0(xdp_frame_get_addr(frame, void *));
        }
        for (i = 0; i < rcvd - 3; i++) {
            xdp_prefetch0(xdp_frame_get_addr(frame, void *));
            processIpv4(frame[i], chn);
        }
        for (; i < rcvd; i++) {
            processIpv4(frame[i], chn);
        }

        if (xdp_runtime_likely(chn->sendCount > 0)) {
            sent = xdp_dev_write(qIdx, chn->send_bufs, chn->sendCount);
            if (xdp_runtime_unlikey(sent != chn->sendCount) {
                for (i = sent; i < chn->sendCount; i++) {
                    xdp_framepool_free_frame(chn->send_bufs[i]);
                }
            }
        }
    }

    return 0;
}
