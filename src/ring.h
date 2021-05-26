#ifndef _RING_INCLUDE_H_
#define _RING_INCLUDE_H_

#include<stdint.h>
#include<sched.h>
#include<string.h>

#include"queue.h"
#include"global.h"
#include"atomic.h"

#define CMT_RING_NAMESIZE 128
#define RING_SIZE 32
#define CMT_RING_PAUSE_MAX_COUNT 1024

typedef struct cmt_ring {
	char name[CMT_RING_NAMESIZE]; /** name of ring */
	int flags;	/** flags supplied at creation */

	struct prod {
		uint32_t watermark;	/** maximum items before equeue*/
		uint32_t sp_enqueue;	/** true, if single producer */
		uint32_t size;	/** size of ring */
		uint32_t mask;	/** mask (size - 1) of ring */
		volatile uint32_t head;	/** producer head */
		volatile uint32_t tail;	/** producer tail */
	} _cache_aligned prod;

	struct cons {
		uint32_t sc_dequeue;	/** true, if single consumer */
		uint32_t size;	/** size of the ring */
		unit32_t mask;	/** mask (size - 1) of ring */
		volatile uint32_t head;	/** consumer head */
		volatile uint32_t tail;	/** consumer tail */
	} _cache_aligned cons;

	void* ring[0] _cache_aligned;	/** memory space of ring starts here.*/

} cmt_ring_t;

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
		__cmt_ring_dequeue_elems_32(r, nr_size, nr_idx,
			obj_table, nr_num);

	}
}

always_inline int 
_cmt_ring_mp_do_enqueue(cmt_ring_t* r, void* const* obj_table,
	unsigned n) {
	uint32_t prod_head, prod_next;
	uint32_t cons_tail, free_entries;
	const unsigned max = n;
	int success;
	unsigned i, rep = 0;
	uint32_t mask = r->prod.mask;
	int ret;

	do {
		n = max;
		prod_head = r->prod.head;
		cons_tail = r->cons.tail;

		free_entries = (mask + cons_tail - prod_head);

		if (n > free_entries) {
			return -1;
		}
		prod_next = prod_head + n;
		success = CASra(&r->prod.head, prod_head,
			prod_next);
	} while (unlikely(sucess == 0));

	enqueue_ptrs(r, prod_head, obj_table, sizeof(void*), n);

	while (unlikely(r->prod.tail != prod_head)) {
		cmt_pause();

		if (CMT_RING_PAUSE_MAX_COUNT &&
			++rep == CMT_RING_NAMESIZE) {
			sched_yield();
		}
	}
	r->prod.tail = prod_next;
	return ret;

}

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
			return -1;
		}

		cons_next = cons_head + n;
		success = CASra(&r->cons.head, cons_head, cons_next);
	} while (unlikely(success == 0));

	dequeue_ptr(r, cons_head, obj_table, sizeof(void*), n);

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
	enqueue_ptr(r, prod_head, obj_table, sizeof(void*), n);

	RELEASE(&r->prod.head, prod_next);
	RELEASE(&r->prod.tail, prod_next);

	return 0;
	
}

always_inline int
_cmt_ring_sc_do_dequeue(cmt_ring* r, void** obj_table, unsigned n) {
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
	dequeu_ptr(r, cons_head, obj_table, sizeof(void*), n);

	RELEASE(&r->cons.head, cons_next);
	RELEASE(&r->cons.tail, cons_next);

	return n;
}

always_inline int
_cmt_ring_enqueue(cmt_ring_t* r, void* const* obj_table) {
	uint32_t flag = r->prod.sp_enqueue;
	if (flag) {
		_cmt_ring_sp_do_enqueue(r, )
	}
}

always_inline int
_cmt_ring_dequeue(cmt_ring_t* r, void** object_table, unsigned n) {

}

#endif /** _RING_INCLUDE_H_ */
