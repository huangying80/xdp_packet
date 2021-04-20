/*
 * huangying, email: hy_gzr@163.com
 */
#ifndef _XDP_NET_H_
#define _XDP_NET_H_

#include <linux/stddef.h>
#include <linux/swab.h>
#ifdef __cplusplus
extern "C" {
#endif
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
# define __xdp_ntohs(x)			__builtin_bswap16(x)
# define __xdp_htons(x)			__builtin_bswap16(x)
# define __xdp_constant_ntohs(x)	___constant_swab16(x)
# define __xdp_constant_htons(x)	___constant_swab16(x)
# define __xdp_ntohl(x)			__builtin_bswap32(x)
# define __xdp_htonl(x)			__builtin_bswap32(x)
# define __xdp_constant_ntohl(x)	___constant_swab32(x)
# define __xdp_constant_htonl(x)	___constant_swab32(x)
# define __xdp_be64_to_cpu(x)		__builtin_bswap64(x)
# define __xdp_cpu_to_be64(x)		__builtin_bswap64(x)
# define __xdp_constant_be64_to_cpu(x)	___constant_swab64(x)
# define __xdp_constant_cpu_to_be64(x)	___constant_swab64(x)
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
# define __xdp_ntohs(x)			(x)
# define __xdp_htons(x)			(x)
# define __xdp_constant_ntohs(x)	(x)
# define __xdp_constant_htons(x)	(x)
# define __xdp_ntohl(x)			(x)
# define __xdp_htonl(x)			(x)
# define __xdp_constant_ntohl(x)	(x)
# define __xdp_constant_htonl(x)	(x)
# define __xdp_be64_to_cpu(x)		(x)
# define __xdp_cpu_to_be64(x)		(x)
# define __xdp_constant_be64_to_cpu(x)  (x)
# define __xdp_constant_cpu_to_be64(x)  (x)
#else
# error "Fix your compiler's __BYTE_ORDER__?!"
#endif

#define xdp_htons(x)				\
	(__builtin_constant_p(x) ?		\
	 __xdp_constant_htons(x) : __xdp_htons(x))
#define xdp_ntohs(x)				\
	(__builtin_constant_p(x) ?		\
	 __xdp_constant_ntohs(x) : __xdp_ntohs(x))
#define xdp_htonl(x)				\
	(__builtin_constant_p(x) ?		\
	 __xdp_constant_htonl(x) : __xdp_htonl(x))
#define xdp_ntohl(x)				\
	(__builtin_constant_p(x) ?		\
	 __xdp_constant_ntohl(x) : __xdp_ntohl(x))
#define xdp_cpu_to_be64(x)			\
	(__builtin_constant_p(x) ?		\
	 __xdp_constant_cpu_to_be64(x) : __xdp_cpu_to_be64(x))
#define xdp_be64_to_cpu(x)			\
	(__builtin_constant_p(x) ?		\
	 __xdp_constant_be64_to_cpu(x) : __xdp_be64_to_cpu(x))

static inline uint16_t
xdp_raw_checksum_reduce(uint32_t sum)
{
    sum = ((sum & 0xffff0000) >> 16) + (sum & 0xffff);
    sum = ((sum & 0xffff0000) >> 16) + (sum & 0xffff);
    return (uint16_t)sum;
}

static inline uint32_t
__xdp_raw_checksum(const void *buf, size_t len, uint32_t sum)
{
    uintptr_t ptr = (uintptr_t)buf;
    typedef uint16_t __attribute__((__may_alias__)) u16_p;
    const u16_p *u16_buf = (const u16_p *)ptr;

    while (len >= (sizeof(*u16_buf) * 4)) {
        sum += u16_buf[0];
        sum += u16_buf[1];
        sum += u16_buf[2];
        sum += u16_buf[3];
        len -= sizeof(*u16_buf) * 4;
        u16_buf += 4;
    }
    while (len >= sizeof(*u16_buf)) {
        sum += *u16_buf;
        len -= sizeof(*u16_buf);
        u16_buf += 1;
    }

    if (len == 1)
        sum += *((const uint8_t *)u16_buf);

    return sum;
}

static inline uint16_t xdp_raw_checksum(const void *buf, size_t len)
{
    uint32_t sum;
    sum = __xdp_raw_checksum(buf, len, 0);
    return xdp_raw_checksum_reduce(sum);
}

static inline uint16_t
xdp_ipv4_phdr_checksum(const struct iphdr *ipv4_hdr)
{
    struct ipv4_psd_header {
        uint32_t src_addr;
        uint32_t dst_addr;
        uint8_t  zero;
        uint8_t  proto;
        uint16_t len;
    } psd_hdr;

    psd_hdr.src_addr = ipv4_hdr->saddr;
    psd_hdr.dst_addr = ipv4_hdr->daddr;
    psd_hdr.zero = 0;
    psd_hdr.proto = ipv4_hdr->protocol;
    psd_hdr.len = xdp_htons((uint16_t)(xdp_ntohs(ipv4_hdr->tot_len)
        - sizeof(struct iphdr)));
    return xdp_raw_checksum(&psd_hdr, sizeof(psd_hdr));
}

static inline uint16_t
xdp_ipv4_udptcp_checksum(const struct iphdr *iphdr, const void *l4_hdr)
{
    uint32_t cksum;
    uint32_t l4_len;

    l3_len = xdp_ntohs(iphdr->tot_len);
    if (l3_len < sizeof(struct iphdr))
        return 0;

    l4_len = l3_len - sizeof(struct iphdr);

    cksum = xdp_raw_checksum(l4_hdr, l4_len);
    cksum += xdp_ipv4_phdr_checksum(iphdr);

    cksum = ((cksum & 0xffff0000) >> 16) + (cksum & 0xffff);
    cksum = (~cksum) & 0xffff;
    if (cksum == 0)
        cksum = 0xffff;

    return (uint16_t)cksum;
}

static inline uint16_t
xdp_ipv4_checksum(const struct iphdr *ipv4_hdr)
{
    uint16_t cksum;
    cksum = xdp_raw_checksum(ipv4_hdr, sizeof(struct iphdr));
    return (cksum == 0xffff) ? cksum : (uint16_t)~cksum;
}

static inline uint16_t
xdp_ipv6_phdr_checksum(const struct ipv6hdr *ipv6_hdr)
{
    uint32_t sum;
    struct {
        rte_be32_t len;
        rte_be32_t proto;
    } psd_hdr;

    psd_hdr.proto = (uint32_t)(ipv6_hdr->nexthdr << 24);
    psd_hdr.len = ipv6_hdr->payload_len;

    sum = __xdp_raw_checksum(ipv6_hdr->saddr, sizeof(ipv6_hdr->saddr)
        + sizeof(ipv6_hdr->daddr), 0);
    sum = __xdp_raw_checksum(&psd_hdr, sizeof(psd_hdr), sum);
    return xdp_raw_checksum_reduce(sum);
}

static inline uint16_t
xdp_ipv6_udptcp_checksum(const struct ipv6hdr *ipv6_hdr, const void *l4_hdr)
{
    uint32_t cksum;
    uint32_t l4_len;

    l4_len = xdp_ntohs(ipv6_hdr->payload_len);

    cksum = xdp_raw_checksum(l4_hdr, l4_len);
    cksum += xdp_ipv6_phdr_checksum(ipv6_hdr);

    cksum = ((cksum & 0xffff0000) >> 16) + (cksum & 0xffff);
    cksum = (~cksum) & 0xffff;
    if (cksum == 0)
        cksum = 0xffff;

    return (uint16_t)cksum;
}

#ifdef __cplusplus
}
#endif

#endif 
