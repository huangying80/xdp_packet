/*
 * huangying, email: hy_gzr@163.com
 */
#ifndef _XDP_RING_H_
#define _XDP_RING_H_
#include <emmintrin.h>
#include <stdint.h>

#include "xdp_mempool.h"

#define XDP_CACHE_ALIGN __attribute__((aligned(XDP_CACHE_LINE)))

#define XDP_RING_SP_ENQ 0x0001
#define XDP_RING_SC_DEQ 0x0002

#define xdp_ring_rmb() _mm_lfence()
#define xdp_ring_wmb() _mm_sfence()
#define xdp_ring_mb()  _mn_mfence()

#define xdp_ring_likely(x) __builtin_expect(!!(x), 1)
#define xdp_ring_unlikely(x) __builtin_expect(!!(x), 0)

struct xdp_headtail {
    volatile uint32_t head;
    volatile uint32_t tail;
    uint32_t          single;
};
struct xdp_ring {
    uint32_t            size;
    uint32_t            mask;
    uint32_t            capacity;
    char                pad0 XDP_CACHE_ALIGN;
    struct xdp_headtail prod XDP_CACHE_ALIGN;
    char                pad1 XDP_CACHE_ALIGN;
    struct xdp_headtail cons XDP_CACHE_ALIGN;
};

struct xdp_ring *xdp_ring_create(struct xdp_mempool *pool,
    uint32_t count, int flags);
size_t xdp_ring_memory_size(uint32_t count);

#define XDP_ENQUEUE_ADDR(r, ring_start, prod_head, obj_table, n, obj_type) \
do { \
	unsigned int i; \
	const uint32_t size = (r)->size; \
	uint32_t idx = prod_head & (r)->mask; \
	obj_type *_ring = (obj_type *)ring_start; \
	if (xdp_ring_likely(idx + n < size)) { \
		for (i = 0; i < (n & ((~(unsigned)0x3))); i+=4, idx+=4) { \
			_ring[idx] = obj_table[i]; \
			_ring[idx+1] = obj_table[i+1]; \
			_ring[idx+2] = obj_table[i+2]; \
			_ring[idx+3] = obj_table[i+3]; \
		} \
		switch (n & 0x3) { \
		case 3: \
			_ring[idx++] = obj_table[i++];  \
		case 2: \
			_ring[idx++] = obj_table[i++];  \
		case 1: \
			_ring[idx++] = obj_table[i++]; \
		} \
	} else { \
		for (i = 0; idx < size; i++, idx++)\
			_ring[idx] = obj_table[i]; \
		for (idx = 0; i < n; i++, idx++) \
			_ring[idx] = obj_table[i]; \
	} \
} while (0)

#define XDP_DEQUEUE_ADDR(r, ring_start, cons_head, obj_table, n, obj_type) \
do { \
	unsigned int i; \
	uint32_t idx = cons_head & (r)->mask; \
	const uint32_t size = (r)->size; \
	obj_type *_ring = (obj_type *)ring_start; \
	if (xdp_ring_likely(idx + n < size)) { \
		for (i = 0; i < (n & (~(unsigned)0x3)); i+=4, idx+=4) {\
			obj_table[i] = _ring[idx]; \
			obj_table[i+1] = _ring[idx+1]; \
			obj_table[i+2] = _ring[idx+2]; \
			obj_table[i+3] = _ring[idx+3]; \
		} \
		switch (n & 0x3) { \
		case 3: \
			obj_table[i++] = _ring[idx++];  \
		case 2: \
			obj_table[i++] = _ring[idx++];  \
		case 1: \
			obj_table[i++] = _ring[idx++]; \
		} \
	} else { \
		for (i = 0; idx < size; i++, idx++) \
			obj_table[i] = _ring[idx]; \
		for (idx = 0; i < n; i++, idx++) \
			obj_table[i] = _ring[idx]; \
	} \
} while (0)

#if (__GNUC__ >= 4 && __GNUC_MINOR__ >=2)
#define xdp_ring_cmpset32(val, old, set) \
        __sync_bool_compare_and_swap((val), (old), (set))
#else
static inline int
xdp_ring_cmpset32(volatile uint32_t *dst, uint32_t exp, uint32_t src)
{
    uint8_t res;
    asm volatile(
        "lock;"
        "cmpxchgl %[src], %[dst];"
        "sete %[res];"
        : [res] "=a" (res), 
        [dst] "=m" (*dst)
        : [src] "r" (src), 
        "a" (exp),
        "m" (*dst)
        : "memory");   
    return res;
}
#endif

inline unsigned int
xdp_ring_move_prod_head(struct xdp_ring *ring,
    unsigned int is_sp, unsigned int n,
    uint32_t *old_head, uint32_t *new_head, uint32_t *free_entries)
{
    const uint32_t  capacity = ring->capacity;
    unsigned int    max = n;
    int             ret = 0;

    do {
        n = max;
        *old_head = ring->prod.head;
        xdp_ring_rmb();
        *free_entries = capacity + ring->cons.tail - *old_head;
        if (xdp_ring_unlikely(n > *free_entries)) {
            n = *free_entries;
        }
        *new_head = *old_head + n;
        if (is_sp) {
            ring->prod.head = *new_head;
            ret = 1;
        } else {
            ret = xdp_ring_cmpset32(&ring->prod.head, *old_head, *new_head);
        }

    } while (xdp_ring_unlikely(!ret));

    return n;
}

inline unsigned int
xdp_ring_move_cons_head(struct xdp_ring *ring,
    unsigned int is_sc, unsigned int n,
    uint32_t *old_head, uint32_t *new_head, uint32_t *entries)
{
    unsigned int max = n;
    int          ret = 0;

    do {
        n = max;
        *old_head = ring->cons.head;
        xdp_ring_rmb();
        *entries = ring->prod.tail - *old_head;
        if (xdp_ring_unlikely(n > *entries)) {
            n = *entries;
        }
        *new_head = *old_head + n;
        if (is_sc) {
            ring->cons.head = *new_head;
            ret = 1;
        } else {
            ret = xdp_ring_cmpset32(&ring->cons.head, *old_head, *new_head);
        }
    } while (xdp_ring_unlikely(!ret));

    return n;
}

inline void
xdp_ring_update_tail(struct xdp_headtail *ht,
    uint32_t old_val, uint32_t new_val,
    uint32_t single, uint32_t enq)
{
    if (enq) {
        xdp_ring_rmb();
    } else {
        xdp_ring_wmb();
    }

    if (!single) {
        while (xdp_ring_unlikely(ht->tail != old_val)) {
            _mm_pause();
        }
    }
    ht->tail = new_val;
}

inline unsigned int
xdp_ring_do_enqueue(struct xdp_ring *ring,
    void * const *obj_list, unsigned int n,
    unsigned int is_sp, unsigned int *free_space)
{
    uint32_t prod_head;
    uint32_t prod_next;
    uint32_t free_entries;

    n = xdp_ring_move_prod_head(ring, is_sp, n,
        &prod_head, &prod_next, &free_entries);
    if (!n) {
        goto out;
    }
    XDP_ENQUEUE_ADDR(ring, &ring[1], prod_head, obj_list, n, void *);
    xdp_ring_update_tail(&ring->prod, prod_head, prod_next, is_sp, 1);
out:
    if (free_space) {
        *free_space = free_entries - n;
    }
    return n; 
}

inline unsigned int
xdp_ring_do_dequeue(struct xdp_ring *ring,
    void **obj_list, unsigned int n,
    unsigned int is_sc, unsigned int *available)
{
    uint32_t cons_head;
    uint32_t cons_next;
    uint32_t entries;

    n = xdp_ring_move_cons_head(ring, is_sc, n,
        &cons_head, &cons_next, &entries);
    if (!n) {
        goto out;
    }
    XDP_DEQUEUE_ADDR(ring, &ring[1], cons_head, obj_list, n, void *);
    xdp_ring_update_tail(&ring->cons, cons_head, cons_next, is_sc, 0);
out:
    if (available) {
        *available = entries - n;
    }

    return n;
}

static inline unsigned int
xdp_ring_multiprod_enqueue_bulk(struct xdp_ring *ring,
    void * const *obj_list, unsigned int n,
    unsigned int *free_space)
{
    return xdp_ring_do_enqueue(ring, obj_list, n, 0, free_space);
}

inline unsigned int
xdp_ring_singleprod_enqueue_bulk(struct xdp_ring *ring,
    void * const *obj_list, unsigned int n,
    unsigned int *free_space)
{
    return xdp_ring_do_enqueue(ring, obj_list, n, 1, free_space);
}

inline unsigned int
xdp_ring_enqueue_bulk(struct xdp_ring *ring,
    void * const *obj_list, unsigned int n,
    unsigned int *free_space)
{
    return xdp_ring_do_enqueue(ring, obj_list, n,
        ring->prod.single, free_space);
}

static inline unsigned int
xdp_ring_multicons_dequeue_bulk(struct xdp_ring *ring,
    void **obj_list, unsigned int n, unsigned int *available)
{
    return xdp_ring_do_dequeue(ring, obj_list, n, 0, available);
}

static inline unsigned int
xdp_ring_singlecons_dequeue_bulk(struct xdp_ring *ring,
    void **obj_list, unsigned int n, unsigned int *available)
{
    return xdp_ring_do_dequeue(ring, obj_list, n, 1, available);
}

inline unsigned int
xdp_ring_dequeue_bulk(struct xdp_ring *ring,
    void **obj_list, unsigned int n,
    unsigned int *available)
{
    return xdp_ring_do_dequeue(ring, obj_list, n,
        ring->cons.single, available);
}

inline unsigned int
xdp_ring_count(struct xdp_ring *ring)
{
    uint32_t prod_tail = ring->prod.tail;
    uint32_t cons_tail = ring->cons.tail;
    uint32_t count = (prod_tail - cons_tail) & ring->mask;
    return (count > ring->capacity) ? ring->capacity : count;
}

inline unsigned int
xdp_ring_free_count(struct xdp_ring *ring)
{
    return ring->capacity - xdp_ring_count(ring);
}

inline unsigned int
xdp_ring_full(struct xdp_ring *ring)
{
    return !xdp_ring_free_count(ring);
}

inline unsigned int
xdp_ring_empty(struct xdp_ring *ring)
{
    return !xdp_ring_count(ring);
}

inline unsigned int
xdp_ring_get_size(const struct xdp_ring *r)
{   
    return r->size;
}

inline unsigned int
xdp_ring_get_capacity(const struct xdp_ring *r)
{   
    return r->capacity;
}

#endif
