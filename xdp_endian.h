/*
 * huangying, email: hy_gzr@163.com
 */
#ifndef __XDP_ENDIAN__
#define __XDP_ENDIAN__

#include <linux/stddef.h>
#include <linux/swab.h>

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

#endif 
