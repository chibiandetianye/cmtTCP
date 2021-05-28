//
// Created by chimt on 2021/5/27.
//

#ifndef _RING_INCLUDE_H_
#define _RING_INCLUDE_H_

#include<stdint.h>
#include<sched.h>
#include<string.h>

#include"global.h"
#include"atomic.h"
#include"cmt_memory_pool.h"


#define CMT_RING_NAMESIZE 128
#define RING_SIZE 32
#define CMT_RING_PAUSE_MAX_COUNT 1024

/** \brief data structure of ring
    ---------------------------------------------------------
    The producer and the consumer have a head and a tail index.
    The particularity of these index is that they are not
    between 0 and ring(size). The indexes are between 0 and
    2^32, and we mask their value when we access the ring[]
    field.
 */
typedef struct cmt_ring {
    char name[CMT_RING_NAMESIZE]; /** name of ring */

    struct prod {
        uint32_t sp_enqueue;	/** true, if single producer */
        uint32_t size;	/** size of ring */
        uint32_t mask;	/** mask (size - 1) of ring */
        volatile uint32_t head;	/** producer head */
        volatile uint32_t tail;	/** producer tail */
    } _cache_aligned prod;

    struct cons {
        uint32_t sc_dequeue;	/** true, if single consumer */
        uint32_t size;	/** size of the ring */
        uint32_t mask;	/** mask (size - 1) of ring */
        volatile uint32_t head;	/** consumer head */
        volatile uint32_t tail;	/** consumer tail */
    } _cache_aligned cons;

    void* ring[0] _cache_aligned;	/** memory space of ring starts here.*/

} cmt_ring_t;

/** \brief create a new ring named "name" in memory
    ---------------------------------------------------------
    The new ring size is set to "count", which must be a
    power of 2
    ---------------------------------------------------------
    @param pool memory pool to allocate memory
    @param name the name of the ring
    @param count the size of the ring
    @param sp mark single producer
    @param sc mark single consumer
    @return On success, the pointer to the new allocted ring,
            NULL on error and set cmt_errno
            EINVAL count is not pow of 2
            ENONMEM no more memory unused
 */
cmt_ring_t* cmt_ring_create(cmt_pool_t* pool, const char* name, unsigned int count,
                            uint32_t sp, uint32_t sc);

/** \brief de-allocte all memory used by ring
    ---------------------------------------------------------
    @param pool memory pool
    @param ring ring to free
 */
void cmt_ring_destroy(cmt_pool_t* pool, cmt_ring_t* ring);

always_inline void
_cmt_ring_enqueue_elms_32(cmt_ring_t* r, const uint32_t size,
                          uint32_t idx, const void* obj_table,
                          unsigned int n) {
    unsigned int i;
    uint32_t* ring = (uint32_t*)&r[1];
    const uint32_t* obj = (const uint32_t*)obj_table;
    if (likely(idx + n < size)) {
        for (i = 0; i < (n & ~0x7);	i += 8, idx += 8) {
            ring[idx] = obj[i];
            ring[idx + 1] = obj[i + 1];
            ring[idx + 2] = obj[i + 2];
            ring[idx + 3] = obj[i + 3];
            ring[idx + 4] = obj[i + 4];
            ring[idx + 5] = obj[i + 5];
            ring[idx + 6] = obj[i + 6];
            ring[idx + 7] = obj[i + 7];
        }
        switch (n & 0x7) {
            case 7:
                ring[idx++] = obj[i++];
            case 6:
                ring[idx++] = obj[i++];
            case 5:
                ring[idx++] = obj[i++];
            case 4:
                ring[idx++] = obj[i++];
            case 3:
                ring[idx++] = obj[i++];
            case 2:
                ring[idx++] = obj[i++];
            case 1:
                ring[idx++] = obj[i++];
        }
    }
    else {
        for (i = 0; idx < size; i++, idx++) {
            ring[idx] = obj[i];
        }
        /* Start at the beginning */
        for (idx = 0; i < n; i++, idx++)
            ring[idx] = obj[i];
    }

}

always_inline void
_cmt_ring_enqueue_elms_64(cmt_ring_t* r, uint32_t prod_head, const void* obj_table,
                          unsigned int n) {
    unsigned int i;
    const uint32_t size = r->prod.size;
    uint32_t idx = prod_head & r->prod.mask;
    uint64_t* ring = (uint64_t*)&r[1];
    const uint64_t* obj = (const uint64_t*)obj_table;
    if (likely(idx + n) < size) {
        for (i = 0; i < (n & ~0x3); i += 4, idx += 4) {
            ring[idx] = obj[i];
            ring[idx + 1] = obj[i + 1];
            ring[idx + 2] = obj[i + 2];
            ring[idx + 3] = obj[i + 3];
        }
        switch (n & 0x03) {
            case 3:
                ring[idx++] = obj[i++];
            case 2:
                ring[idx++] = obj[i++];
            case 1:
                ring[idx++] = obj[i++];
        }
    }
    else {
        for (i = 0; idx < size; i++, idx++) {
            ring[idx] = obj[i];
        }
        for (idx = 0; i < n; ++i, idx++) {
            ring[idx] = obj[i];
        }
    }
}

always_inline void
_cmt_ring_enqueue_elms_128(cmt_ring_t* r, uint32_t prod_head, const void* obj_table,
                           unsigned int n) {
    unsigned int i;
    const uint32_t size = r->prod.size;
    uint32_t idx = prod_head & r->prod.mask;
    __uint128_t* ring = (__uint128_t*)&r[1];
    const __uint128_t* obj = (const __uint128_t*)obj_table;
    if (likely(idx + n < size)) {
        for (i = 0; i < (n & ~0x1); i += 2, idx += 2) {
            memcpy((void*)(ring + idx),
                   (const void*)(obj + i), 32);
        }
        switch (n & 0x01) {
            case 1:
                memcpy((void*)(ring + idx),
                       (const void*)(obj + i), 16);

        }
    }
    else {
        for (i = 0; i < size; ++i) {
            memcpy((void*)(ring + idx),
                   (const void*)(obj + i), 16);
        }
        for (idx = 0; i < n; i++, idx++)
            memcpy((void*)(ring + idx),
                   (const void*)(obj + i), 16);
    }

}

/** \brief internal enqueue several objects to the ring
    --------------------------------------------------------------
    @param r a pointer to ring
    @param obj_table a pointer to a table of objects
    @param esize the size of ring element, in bytes. It must be 
            multiple of 4
    @param n the number of objects to pull in the ring
 */
always_inline void
enqueue_ptrs(cmt_ring_t* r, uint32_t prod_head, const void* obj_table,
             unsigned int esize, unsigned int n) {
    if (esize == 8) {
        _cmt_ring_enqueue_elms_64(r, prod_head, obj_table, n);
    }
    else if (esize == 16) {
        _cmt_ring_enqueue_elms_128(r, prod_head, obj_table, n);
    }
    else {
        uint32_t idx, scale, nr_idx, nr_num, nr_size;

        /* Normalize to uint32_t */
        scale = esize / sizeof(uint32_t);
        nr_num = n * scale;
        idx = prod_head & r->prod.mask;
        nr_idx = idx * scale;
        nr_size = r->prod.size * scale;
        _cmt_ring_enqueue_elms_32(r, nr_size, nr_idx,
                                  obj_table, nr_num);
    }
}

always_inline void
_cmt_ring_dequeue_elms_64(cmt_ring_t* r, uint32_t prod_head,
                          void* obj_table, uint32_t n) {
    unsigned int i;
    uint32_t idx = prod_head & r->cons.mask;
    uint32_t size = r->cons.size;
    uint64_t* ring = (uint64_t*)&r[1];
    uint64_t* obj = (uint64_t*)obj_table;
    if (likely(idx + n < size)) {
        for (i = 0; i < (n & ~0x3); i += 4, idx += 4) {
            obj[i] = ring[idx];
            obj[i + 1] = ring[idx + 1];
            obj[i + 2] = ring[idx + 2];
            obj[i + 3] = ring[idx + 3];
        }
        switch (n & 0x3) {
            case 3:
                obj[i++] = ring[idx++];
            case 2:
                obj[i++] = ring[idx++];
            case 1:
                obj[i++] = ring[idx++];
        }
    }
    else {
        for (i = 0; idx < size; i++, idx++) {
            obj[i] = ring[idx];
        }
        for (idx = 0; i < n; i++, idx++) {
            obj[i] = ring[idx];
        }
    }
}

always_inline void
_cmt_ring_dequeue_elms_128(cmt_ring_t* r, uint32_t prod_head,
                           void* obj_table, unsigned int n) {
    unsigned int i;
    const uint32_t size = r->cons.size;
    uint32_t idx = prod_head & r->cons.mask;
    __uint128_t* ring = (__uint128_t*)&r[1];
    __uint128_t* obj = (__uint128_t*)obj_table;
    if (likely(idx + n < size)) {
        for (i = 0; i < (n & ~0x1); i += 2, idx += 2)
            memcpy((void*)(obj + i), (void*)(ring + idx), 32);
        switch (n & 0x1) {
            case 1:
                memcpy((void*)(obj + i), (void*)(ring + idx), 16);
        }
    }
    else {
        for (i = 0; idx < size; i++, idx++)
            memcpy((void*)(obj + i), (void*)(ring + idx), 16);
        /* Start at the beginning */
        for (idx = 0; i < n; i++, idx++)
            memcpy((void*)(obj + i), (void*)(ring + idx), 16);
    }
}

always_inline void
_cmt_ring_dequeue_elems_32(cmt_ring_t* r, const uint32_t size,
                           uint32_t idx, void* obj_table, unsigned int n)
{
    unsigned int i;
    uint32_t* ring = (uint32_t*)&r[1];
    uint32_t* obj = (uint32_t*)obj_table;
    if (likely(idx + n < size)) {
        for (i = 0; i < (n & ~0x7); i += 8, idx += 8) {
            obj[i] = ring[idx];
            obj[i + 1] = ring[idx + 1];
            obj[i + 2] = ring[idx + 2];
            obj[i + 3] = ring[idx + 3];
            obj[i + 4] = ring[idx + 4];
            obj[i + 5] = ring[idx + 5];
            obj[i + 6] = ring[idx + 6];
            obj[i + 7] = ring[idx + 7];
        }
        switch (n & 0x7) {
            case 7:
                obj[i++] = ring[idx++]; /* fallthrough */
            case 6:
                obj[i++] = ring[idx++]; /* fallthrough */
            case 5:
                obj[i++] = ring[idx++]; /* fallthrough */
            case 4:
                obj[i++] = ring[idx++]; /* fallthrough */
            case 3:
                obj[i++] = ring[idx++]; /* fallthrough */
            case 2:
                obj[i++] = ring[idx++]; /* fallthrough */
            case 1:
                obj[i++] = ring[idx++]; /* fallthrough */
        }
    }
    else {
        for (i = 0; idx < size; i++, idx++)
            obj[i] = ring[idx];
        /* Start at the beginning */
        for (idx = 0; i < n; i++, idx++)
            obj[i] = ring[idx];
    }
}

/** \brief internal dequeue several objects from the ring
    --------------------------------------------------------------
    @param r a pointer to ring
    @param obj_table a pointer to a table of objects
    @param esize the size of ring element, in bytes. It must be 
            multiple of 4
    @param n the number of objects to pull in the ring
 */
always_inline void
dequeue_ptrs(cmt_ring_t* r, uint32_t cons_head,
             void *obj_table, uint32_t esize, unsigned int n) {
    if (esize == 8) {
        _cmt_ring_dequeue_elms_64(r, cons_head, obj_table, n);
    }
    else if (esize == 16) {
        _cmt_ring_dequeue_elms_128(r, cons_head, obj_table, n);
    }
    else {
        uint32_t idx, scale, nr_idx, nr_num, nr_size;

        scale = esize / sizeof(uint32_t);
        nr_num = n * scale;
        idx = cons_head & r->cons.mask;
        nr_idx = idx * scale;
        nr_size = r->cons.size * scale;
        _cmt_ring_dequeue_elems_32(r, nr_size, nr_idx,
                                    obj_table, nr_num);

    }
}

/** \brief enqueue serveral objects on the ring(multi-producers version)
    --------------------------------------------------------------
    This function uses a "compare-and-set" instruction to move the 
    producer index atomically
    --------------------------------------------------------------
    @param r a pointer to the ring
    @param a pointer to a table of void* pointers
    @param n the number of objects to add ing the ring
    @return n actual number of objects enqueue
 */
always_inline int
_cmt_ring_mp_do_enqueue(cmt_ring_t* r, void* const* obj_table,
                        unsigned n) {
    uint32_t prod_head, prod_next;
    uint32_t cons_tail, free_entries;
    const unsigned max = n;
    int success;
    unsigned i, rep = 0;
    uint32_t mask = r->prod.mask;

    do {
        n = max;
        prod_head = r->prod.head;
        cons_tail = r->cons.tail;

        free_entries = (mask + cons_tail - prod_head);

        if (n > free_entries) {
            n = free_entries;
        }
        prod_next = prod_head + n;
        success = CASra(&r->prod.head, prod_head,
                        prod_next);
    } while (unlikely(success == 0));

    enqueue_ptrs(r, prod_head, (void*)obj_table, sizeof(void*), n);

    while (unlikely(r->prod.tail != prod_head)) {
        cmt_pause();

        if (CMT_RING_PAUSE_MAX_COUNT &&
            ++rep == CMT_RING_NAMESIZE) {
            sched_yield();
        }
    }
    r->prod.tail = prod_next;
    return n;
}

/** \brief dequeue several objects from a ring (multi-consumers safe)
    ----------------------------------------------------------------
    This function uses a "compare-and-set" instructions to move the 
    consumer index atomically
    ----------------------------------------------------------------
    @param r a pointer to the ring
    @param obj_table a pointer to a table of void* pointers(objects)
            will be filled
    @param n the number of objects to dequeue from the ring to the 
            obj_table
    @return the number of objects dequeued, betweeen 0 to n
 */
always_inline int
_cmt_ring_mc_do_dequeue(cmt_ring_t* r, void** obj_table, unsigned n) {
    uint32_t cons_head, prod_tail;
    uint32_t cons_next, entries;
    const unsigned max = 0;
    int success;
    unsigned i, rep = 0;
    uint32_t mask = r->prod.mask;

    if (n == 0) {
        return 0;
    }

    do {
        n = max;

        cons_head = r->cons.head;
        prod_tail = r->prod.tail;

        entries = (prod_tail - cons_head);

        if (n > entries) {
            n = entries;
        }

        cons_next = cons_head + n;
        success = CASra(&r->cons.head, cons_head, cons_next);
    } while (unlikely(success == 0));

    dequeue_ptrs(r, cons_head, (void*)obj_table, sizeof(void*), n);

    while (unlikely(r->cons.tail != cons_head)) {
        cmt_pause();

        if (CMT_RING_PAUSE_MAX_COUNT &&
            ++rep == CMT_RING_PAUSE_MAX_COUNT) {
            sched_yield();
        }
    }

    r->cons.tail = cons_next;
    return n;
}

/** \brief enqueue serveral objects on the ring(single-producers version)
    --------------------------------------------------------------
    This function uses a "compare-and-set" instruction to move the 
    producer index atomically
    --------------------------------------------------------------
    @param r a pointer to the ring
    @param a pointer to a table of void* pointers
    @param n the number of objects to add ing the ring
    @return n actual number of objects enqueue
 */
always_inline int
_cmt_ring_sp_do_enqueue(cmt_ring_t* r, void* const *obj_table,
                        unsigned n) {
    uint32_t prod_head, prod_next;
    uint32_t cons_tail, free_entries;
    const unsigned max = n;
    uint32_t mask = r->prod.mask;

    if (n == 0) {
        return 0;
    }

    prod_head = ACQUIRE(&r->prod.head);
    cons_tail = ACQUIRE(&r->cons.tail);
    free_entries = (mask + cons_tail - prod_head);

    if (n > free_entries) {
        return -1;
    }
    prod_next = prod_head + n;
    enqueue_ptrs(r, prod_head, (void*)obj_table, sizeof(void*), n);

    RELEASE(&r->prod.head, prod_next);
    RELEASE(&r->prod.tail, prod_next);

    return 0;

}

/** \brief dequeue several objects from a ring (single-consumers safe)
    ----------------------------------------------------------------
    This function uses a "compare-and-set" instructions to move the 
    consumer index atomically
    ----------------------------------------------------------------
    @param r a pointer to the ring
    @param obj_table a pointer to a table of void* pointers(objects)
            will be filled
    @param n the number of objects to dequeue from the ring to the 
            obj_table
    @return the number of objects dequeued, betweeen 0 to n
 */
always_inline int
_cmt_ring_sc_do_dequeue(cmt_ring_t* r, void** obj_table, unsigned n) {
    uint32_t cons_head, prod_tail;
    uint32_t cons_next, entries;
    const unsigned max = 0;
    uint32_t mask = r->prod.mask;

    if (n == 0) {
        return 0;
    }

    cons_head = ACQUIRE(&r->cons.head);
    prod_tail = ACQUIRE(&r->prod.tail);
    entries = (prod_tail - cons_head);

    if (n > entries) {
        return -1;
    }

    cons_next = cons_head + n;
    dequeue_ptrs(r, cons_head, (void*)obj_table, sizeof(void*), n);

    RELEASE(&r->cons.head, cons_next);
    RELEASE(&r->cons.tail, cons_next);

    return n;
}

/** \brief enqueue one object on a ring
    ---------------------------------------------------------
    This function calls the multi-producer or the s
    ingle-producer version, depending on the default flag on
    structure prod
    ---------------------------------------------------------
    @param r a pointer to the ring structure
    @param obj a pointer to the object to be added
    @return -0: success: object enqueue
                -1: not enough room in the ring to enqueue
 */
always_inline int
cmt_ring_enqueue(cmt_ring_t* r, void *obj) {
    uint32_t sp =  r->prod.sp_enqueue;
    int ret;
    if(sp == 1) {
        ret = _cmt_ring_sp_do_enqueue(r, &obj, 1);
    }
    else {
        ret = _cmt_ring_mp_do_enqueue(r, &obj, 1);
    }
    return ret - 1;
}

/** \brief enqueue serveral object on a ring
    ---------------------------------------------------------
    This function calls the multi-producer or the s
    ingle-producer version, depending on the default flag on
    structure prod
    ---------------------------------------------------------
    @param r a pointer to a ring
    @param obj_table a pointer to pointer to a table of variable
    @param n the number of objects to add in the ring
    @return the number of objects enqueue, between 0 and n
 */
always_inline unsigned int
cmt_ring_enqueue_bulk(cmt_ring_t* r, void** obj_table,
                      unsigned int n) {
    uint32_t sp = r->prod.sp_enqueue;
    int ret;
    if(sp == 1) {
        ret = _cmt_ring_sp_do_enqueue(r, obj_table, n);
    }
    else {
        ret = _cmt_ring_mp_do_enqueue(r, obj_table, n);
    }
    return ret;
}

/** \brief dequeue one object from a ring
    --------------------------------------------------------------
    @param r a pointer to a ring
    @param obj a pointer that will be filled
    @return -0 success
            -1 no object is dequeued
 */
always_inline int
cmt_dequeue(cmt_ring_t* r, void* obj) {
    uint32_t sc = r->cons.sc_dequeue;
    int ret;
    if(sc == 1) {
        ret = _cmt_ring_sc_do_dequeue(r, &obj, 1);
    }
    else {
        ret = _cmt_ring_mc_do_dequeue(r, &obj, 1);
    }
    return ret - 1;
}

/** \brief dequeue several object on a ring
    ---------------------------------------------------------------
    @param r a pointer to a ring
    @param obj_table a pointer to pointer to a table will be filled
    @param n the number of objects to consume
    @return the number of objects dequeue, between 0 and n
 */
always_inline unsigned int
cmt_dequeue_bulk(cmt_ring_t* r, void** obj, unsigned int n) {
    uint32_t sc = r->cons.sc_dequeue;
    int ret;
    if(sc == 1) {
        ret = _cmt_ring_sc_do_dequeue(r, &obj, n);
    }
    else {
        ret = _cmt_ring_mc_do_dequeue(r, &obj, n);
    }
    return ret;
}


#endif /** _RING_INCLUDE_H_ */

