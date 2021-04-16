/*
 * huangying, email: hy_gzr@163.com
 */

#ifndef _XDP_PREFETCH_X86_64_H_
#define _XDP_PREFETCH_X86_64_H_
#include <xmmintrin.h>
#ifdef __cplusplus
extern "C" {
#endif

#ifdef XDP_PREFETCH_DEFAULT_NUM
#define XDP_PREFETCH_DEFAULT_NUM 3
#endif

#ifdef _mm_prefetch
static inline void xdp_prefetch0(const volatile void *p)
{
    _mm_prefetch(p, _MM_HINT_T0);
}

static inline void xdp_prefetch1(const volatile void *p)
{
    _mm_prefetch(p, _MM_HINT_T1);
}
static inline void xdp_prefetch2(const volatile void *p)
{
    _mm_prefetch(p, _MM_HINT_T2);
}
static inline void xdp_prefetch_nta(const volatile void *p)
{
    _mm_prefetch(p, _MM_HINT_NTA);
}
#else
static inline void xdp_prefetch0(const volatile void *p)
{
	asm volatile ("prefetcht0 %[p]" : : [p] "m" (*(const volatile char *)p));
}

static inline void xdp_prefetch1(const volatile void *p)
{
	asm volatile ("prefetcht1 %[p]" : : [p] "m" (*(const volatile char *)p));
}

static inline void xdp_prefetch2(const volatile void *p)
{
	asm volatile ("prefetcht2 %[p]" : : [p] "m" (*(const volatile char *)p));
}

static inline void xdp_prefetch_nta(const volatile void *p)
{
	asm volatile ("prefetchnta %[p]" : : [p] "m" (*(const volatile char *)p));
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* _RTE_PREFETCH_X86_64_H_ */
