#include<string.h>

#include "tcp_buffer.h"
#include"cmt_memory_pool.h"
#include"tcp.h"
#include"atomic.h"
#include"debug"

#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)<(b)?(a):(b))

cmt_sb_manager_t* 
cmt_sbmanager_create(size_t chunk_size, uint32_t cnum) {
	cmt_pool_t* pool = get_cmt_pool();
	cmt_sb_manager_t* sbm = (cmt_sb_manager_t*)cmt_palloc(pool, sizeof(cmt_sb_manager_t));
	if (!sbm) {
		printf("SBManagerCreate() failed. \n");
		return NULL;
	}
	sbm->sb_pool = NULL;

	sbm->chunk_size = chunk_size;
	sbm->cur_num = 0;
	sbm->sb_pool = cmt_object_pool_create_s(cnum, chunk_size
		0);
	if (!sbm->mp) {
		printf("Failed to create object pool for sb.\n");
		cmt_pfree(pool, sbm, sizeof(cmt_sb_manager_t);
		return NULL;
	}

	return sbm;
}


void 
cmt_sbmanager_destroy(cmt_sb_manager_t* sbm) {
	cmt_pool_t* pool = get_cmt_pool();
	cmt_object_pool_destroy(sbm->mp);
	cmt_pfree(pool, sbm, sizeof(cmt_sb_manager_t));
}

cmt_send_buffer_t* 
SBInit(cmt_sb_manager_t* sbm, uint32_t init_seq) {
	cmt_send_buffer_t* buf;

	/* first try dequeue from free buffer queue */
	buf = cmt_object_get(sbm->sb_pool);
	if (unlikely(buf == NULL)) {
		printf("Failed to fetch send buffer from pool\n");
		return NULL;
	}
	sbm->cur_num++;

	buf->head = buf->data;

	buf->head_off = buf->tail_off = 0;
	buf->len = buf->cum_len = 0;
	buf->size = sbm->chunk_size;

	buf->init_seq = buf->head_seq = init_seq;

	return buf;
}

void SBFree(cmt_sb_manager_t* sbm, cmt_send_buffer_t* buf)
{
	if (!buf)
		return;
	
	sbm->cur_num--;

	cmt_object_free(sbm->sb_pool, buf)
	
}

size_t 
SBPut(cmt_send_buffer_t* buf, const void* data, size_t len) {
	size_t to_put;

	if (len <= 0)
		return 0;

	/* if no space, return -2 */
	to_put = MIN(len, buf->size - buf->len);
	if (to_put <= 0) {
		return -2;
	}

	if (buf->tail_off + to_put < buf->size) {
		/* if the data fit into the buffer, copy it */
		memcpy(buf->data + buf->tail_off, data, to_put);
		buf->tail_off += to_put;
	}
	else {
		/* if buffer overflows, move the existing payload and merge */
		memmove(buf->data, buf->head, buf->len);
		buf->head = buf->data;
		buf->head_off = 0;
		memcpy(buf->head + buf->len, data, to_put);
		buf->tail_off = buf->len + to_put;
	}
	buf->len += to_put;
	buf->cum_len += to_put;

	return to_put;
}

size_t 
SBRemove(cmt_send_buffer* buf, size_t len) {
	size_t to_remove;

	if (len <= 0)
		return 0;

	to_remove = MIN(len, buf->len);
	if (to_remove <= 0) {
		return -2;
	}

	buf->head_off += to_remove;
	buf->head = buf->data + buf->head_off;
	buf->head_seq += to_remove;
	buf->len -= to_remove;

	/* if buffer is empty, move the head to 0 */
	if (buf->len == 0 && buf->head_off > 0) {
		buf->head = buf->data;
		buf->head_off = buf->tail_off = 0;
	}

	return to_remove;
}



/*** ******************************** sb queue ******************************** ***/


//always_inline void 
//RBPrintInfo(cmt_recv_buffer_t* buff) {
//	printf(
//		"buff_clen %lu, buff_head %p (%d), buff_last (%d)\n",
//		buff->cum_len,
//		buff->head, buff->last_len);
//}

//void RBPrintStr(cmt_recv_buffer* buff)
//{
//	RBPrintInfo(buff);
//}
/*
void RBPrintHex(nty_ring_buffer* buff)
{
	int i;

	RBPrintInfo(buff);

	for (i = 0; i < buff->merged_len; i++) {
		if (i != 0 && i % 16 == 0)
			printf("\n");
		printf("%0x ", *((unsigned char*)buff->head + i));
	}
	printf("\n");
}
*/
cmt_recv_manager_t* 
recv_manager_create(size_t chunk_size, uint32_t cnum) {
	cmt_pool_t* pool = get_cmt_pool();
	cmt_recv_manager_t* rbm = (cmt_recv_manager_t*)cmt_palloc(pool, sizeof(cmt_recv_manager_t));


	if (!rbm) {
		printf("failed rbm_create calloc");
		return NULL;
	}
	rbm->frag_pool = NULL;
	rbm->dataptr_pool = NULL;

	rbm->chunk_size = chunk_size;
	rbm->cnum = 0;
	rbm->frag_pool = cmt_object_pool_create_s(sizeof(cmt_fragment_t), cnum, 0);
	if (unlikely(!rbm->frag_pool)) {
		printf("Failed to allocate mp pool.\n");
		cmt_pfree(rbm, sizeof(cmt_recv_manager_t));
		return NULL;
	}

	rbm->dataptr_pool = cmt_object_pool_create(sizeof(cmt_data_ptr_t));
	if (unlikely(!rbm->dataptr_pool)) {
		printf("Failed to allocate data ptr pool.\n");
		cmt_object_pool_destroy(rbm->frag_pool);
		cmt_pfree(rbm, sizeof(cmt_recv_manager_t));
	}

	return rbm;
}

void 
recv_manager_destroy(cmt_recv_managet_t* cmt) {
	
	if (cmt == NULL) {
		return;
	}
	cmt_pool_t* pool = get_cmt_pool();
	cmt_object_pool_t* dataptr_pool = cmt->dataptr_pool;
	cmt_object_pool_t* frag_pool = cmt->frag_pool;
	cmt_object_pool_destroy(dataptr_pool);
	cmt_object_pool_destroy(frag_pool);
	cmt_pfree(cmt, sizeof(cmt_recv_manger_t));

}

always_inline void 
free_fragment_context_single(cmt_recv_manager_t* rbm, cmt_recv_buffer_t* frag) {
	cmt_object_pool_t* dataptr_pool = rbm->dataptr_pool;
	cmt_object_pool_t* frag_pool = rbm->frag_pool;
	cmt_data_ptr_t* data_ptr = frag->data_list;
	cmt_data_ptr_t* tmp;
	
	while (data_ptr) {
		tmp = data_ptr->next;
		if (data_ptr->data) {
			printf("warning: still data remained\n");
		}
		cmt_object_free(data_ptr);
		data_ptr = tmp;
	}
}

void 
free_fragment_context(cmt_recv_manager_t* rbm, cmt_recv_buffer_t* fctx) {
	cmt_recv_buffer_t* remove;
	cmt_object_pool_t* frag_pool = rbm->frag_pool;

	assert(fctx);
	if (fctx == NULL)
		return;

	while (fctx) {
		remove = fctx;
		fctx = fctx->next;
		free_fragment_context_single(rbm, remove);
		cmt_object_free(remove);
	}
}

cmt_recv_buffer_t* 
allocate_recv_buffer(cmt_recv_manager_t* rbm) {
	cmt_object_pool_t* frag_pool = rbm->frag_pool;
	cmt_recv_buffer_t* buffer = cmt_object_get(frag_pool);
	if (unlikely(buffer == NULL)) {
		printf("oject pool fialed to create recv buffer\n");
	}
	return buffer;
}

always_inline cmt_fragment_t*
allocate_fragment_context(cmt_recv_manager_t* rbm) {
	cmt_object_pool_t* frag_pool = rbm->frag_pool;
	cmt_fragment_t* frag = cmt_object_get(frag_pool);
	if (unlikely(frag == NULL)) {
		printf("object pool failed to create frag buffer\n");
	}
	return frag;
}

always_inline cmt_data_ptr_t*
allocate_dataptr(cmt_recv_manager_t* rbm) {
	cmt_object_pool_t* dataptr_pool = rbm->dataptr_pool;
	cmt_data_ptr_t* data_ptr = cmt_object_get(dataptr_pool);
	if (unlikely(data_ptr)) {
		printf("object pool failed to create data ptr\n");
	}
	return data_ptr;
}


#define MAXSEQ               ((uint32_t)(0xFFFFFFFF))
/*----------------------------------------------------------------------------*/
always_inline uint32_t 
GetMinSeq(uint32_t a, uint32_t b) {
	if (a == b) return a;
	if (a < b)
		return ((b - a) <= MAXSEQ / 2) ? a : b;
	/* b < a */
	return ((a - b) <= MAXSEQ / 2) ? b : a;
}
/*----------------------------------------------------------------------------*/
always_inline uint32_t
GetMaxSeq(uint32_t a, uint32_t b) {
	if (a == b) return a;
	if (a < b)
		return ((b - a) <= MAXSEQ / 2) ? b : a;
	/* b < a */
	return ((a - b) <= MAXSEQ / 2) ? a : b;
}
/*----------------------------------------------------------------------------*/
always_inline int 
CanMerge(const cmt_fragment_t* a, const cmt_fragment_t* b) {
	uint32_t a_end = a->seq + a->len + 1;
	uint32_t b_end = b->seq + b->len + 1;

	if (GetMinSeq(a_end, b->seq) == a_end ||
		GetMinSeq(b_end, a->seq) == b_end)
		return 0;
	return (1);
}

always_inline void
swap_fragment(cmt_fragment_t* a, cmt_fragment_t* b) {
	cmt_fragment_t tmp = *a;
	a->seq = b->seq;
	a->len = b->len;
	a->data = b->data;
	a->stream = b->stream;
	a->next = b->next;

	b->seq = tmp->seq;
	b->len = tmp->len;
	b->data = tmp->data;
	b->next = tmp->next;
	b->stream = b->stream;
}

always_inline void
merge_fragments(cmt_fragment_t* a, cmt_fragment_t* b) {
	/* merge a into b */
	uint32_t min_seq, max_seq;
	uint32_t start_seq, next_seq;
	int flag = 0;
	cmt_fragment_t* start, * end;
	cmt_dataptr_t* data_list_start, *data_list_end, tmp;

	if (a->seq < b->seq) {
		swap_fragment(a, b);
	}

	start = a;
	end = b;
	start_seq = b->seq;
	next_seq = a->seq;
	data_list_start = b->fctx->data_list;
	data_list_end = a->fctx->data_list;
	flag = 1;

	tmp = data_list_start->next;
	while (tmp->seq < next_seq && tmp->seq + len < next_seq) {
		data_list_start = tmp;
		tmp = data_list_start->next;
	}
	if (tmp->seq == next_seq) {
		data_list_start->next = data_list_end;
	}
	else {		
		tmp->len = data_list_end->next_seq - tmp->seq;
		data_list_start = tmp;
		tmp = tmp->next;
		data_list_start->next = data_list_end;
	}
	
	
	min_seq = GetMinSeq(a->seq, b->seq);
	max_seq = GetMaxSeq(a->seq + a->len, b->seq + b->len);
	start->seq = min_seq;
	start->len = max_seq - min_seq;

	/** release unused data */
	//
	//
	//
}

int 
recv_put(cmt_recv_manager_t* rbm, cmt_recv_buffer_t* buff,
	void* data, char* stream, uint32_t len, uint32_t cur_seq) {
	cmt_data_ptr* data_ptr;
	cmt_fragment_t* frag;
	int putx, end_off;
	cmt_fragment_t* iter;
	cmt_fragment_t* prev, * pprev;

	if (len <= 0) {
		return 0;
	}

	data_ptr = allocate_dataptr(rbm);
	if (unlikely(data_ptr == NULL)) {
		return -1;
	}
	data_ptr->seq = cur_seq;
	data_ptr->len = len;
	data_ptr->data = data;
	
	frag = allocate_fragmemt_context();
	if (unlikely(frag == NULL)) {
		return -1;
	}
	frag->next = NULL;
	frag->seq = seq;
	frag->len = len;
	frag->data_list = data_ptr;

	// if data offset is smaller than head sequence, then drop
	if (GetMinSeq(buff->head_seq, cur_seq) != buff->head_seq)
		return 0;

	putx = cur_seq - buff->head_seq;
	end_off = putx + len;
	if (buff->chunk_size < end_off) {
		return -2;
	}

	if (buff->last_len < end_off)
		buff->last_len = end_off;
	
	// traverse the fragment list, and merge the new fragment if possible
	for (iter = buff->fctx, prev = NULL, pprev = NULL;
		iter != NULL;
		pprev = prev, prev = iter, iter = iter->next) {

		if (CanMerge(frag, iter)) {
			/* merge the first fragment into the second fragment */
			merge_fragments(frag, iter);

			/* remove the first fragment */
			if (prev == frag) {
				if (pprev)
					pprev->next = iter;
				else
					buff->fctx = iter;
				prev = pprev;
			}
			
			frag = iter;
			merged = 1;
		}
		else if (merged ||
			GetMaxSeq(cur_seq + len, iter->seq) == iter->seq) {
			/* merged at some point, but no more mergeable
			   then stop it now */
			break;
		}
	}

	if (!merged) {
		if (buff->fctx == NULL) {
			buff->fctx = frag;
		}
		else if (GetMinSeq(cur_seq, buff->fctx->seq) == cur_seq) {
			/* if the new packet's seqnum is before the existing fragments */
			new_ctx->next = buff->fctx;
			buff->fctx = new_ctx;
		}
		else {
			/* if the seqnum is in-between the fragments or
			   at the last */
			assert(GetMinSeq(cur_seq, prev->seq + prev->len) ==
				prev->seq + prev->len);
			prev->next = frag;
			frag->next = iter;
		}
	}
	buff->cum_len = buff->fctx->len;

	return len;
}

size_t recv_remove(cmt_recv_manager_t* rbm, cmt_recv_buffer* buff, size_t len, int option)
{
	/* this function should be called only in application thread */

	if (buff->cum_len < (int)len)
		len = buff->merged_len;

	if (len == 0)
		return 0;


	buff->head_seq += len;

	buff->merged_len -= len;
	buff->last_len -= len;

	// modify fragementation chunks
	if (len == buff->fctx->len) {
		nty_fragment_ctx* remove = buff->fctx;
		buff->fctx = buff->fctx->next;
		if (option == AT_APP) {
			RBFragEnqueue(rbm->free_fragq, remove);
		}
		else if (option == AT_MTCP) {
			RBFragEnqueue(rbm->free_fragq_int, remove);
		}
	}
	else if (len < buff->fctx->len) {
		buff->fctx->seq += len;
		buff->fctx->len -= len;
	}
	else {
		assert(0);
	}

	return len;
}



cmt_stream_manager_t* 
create_stream_manager() {
	cmt_pool_t* pool = get_cmt_pool();

	cmt_stream_manager_t* sm;

	sm = (cmt_stream_manager_t*)cmt_palloc(pool, sizeof(cmt_stream_manager_t));
	if (unlikely(!sm)) {
		printf("failed to alloc stream manager\n");
		return NULL;
	}

	sm->stream_pool = cmt_object_pool_create(sizeof(cmt_tcp_stream_t), 0);
	if (unlikely(sm == NULL)) {
		printf("faield to alloc stream object pool\n");
		return NULL;
	}
	
	sm->stream_size = sizeof(cmt_tcp_strema_t);
	sm->cnum = 0;

	return sq;
}

void 
stream_manager_destroy(cmt_stream_manager_t* sm) {
	if (!sm)
		return;

	cmt_pool_t* pool = get_cmt_pool();
	cmt_object_pool_destroy(sm->stream_pool);
	cmt_pfree(pool, sm, sizeof(cmt_stream_manager_t);
}

int free_stream(cmt_stream_manager_t* sm, cmt_tcp_stream_t* stream)
{
	cmt_oject_pool_t* stream_pool = sm->stream_pool;

	cmt_object_free(stream_pool, stream);

	sm->cum--;

	return 0;
}

cmt_tcp_stream_t* 
get_stream(cmt_stream_manager_t* sm) {
	cmt_object_pool_t* stream_pool = sm->stream_pool;
	cmt_tcp_stream_t* stream = cmt_object_get(stream_pool);
	if (unlikely(stream == NULL)) {
		printf("failed to get stream from object pool\n");
		return stream;
	}

	sm->cnum++;
	return stream;
}

cmt_stream_queue_t* create_stream_queue(int capacity) {
	nty_stream_queue* sq;
	cmt_pool_t* pool = get_cmt_pool();

	sq = (cmt_stream_queue_t*)cmt_palloc(pool, sizeof(cmt_stream_queue_t));
	if (unlikely(!sq)) {
		printf("failed to create stream queue from pool\n");
		return NULL;
	}		

	sq->_q = (struct cmt_tcp_stream**)cmt_palloc(pool, 
		(capacity+1) * sizeof(struct cmt_tcp_stream*));
	if (unlikely(!sq->_q)) {
		printf("failed to create internal queue from pool\n");
		cmt_pfree(pool, sq, sizeof(cmt_stream_queue_t));
		return NULL;
	}

	sq->_capacity = capacity;
	sq->_head = sq->_tail = 0;

	return sq;
}

void 
destroy_stream_queue(cmt_stream_queue_t* sq) {
	if (!sq)
		return;

	cmt_pool_t* pool = get_cmt_pool();

	if (sq->_q) {
		cmt_pfree(pool, (void*)sq->_q), sq->_capacity + 1);
		sq->_q = NULL;
	}

	cmt_pfree(pool, sq, sizeof(cmt_stream_queue_t);
}

int 
stream_enqueue(cmt_stream_queue_t* sq, cmt_tcp_stream_t* stream) {
	index_type h = sq->_head;
	index_type t = sq->_tail;
	index_type nt = NextIndex(sq, t);

	if (nt != h) {
		sq->_q[t] = stream;
		barrier();
		sq->_tail = nt;
		return 0;
	}

	printf("Exceed capacity of stream queue!\n");
	return -1;
}

cmt_tcp_stream_t* 
stream_dequeue(cmt_stream_queue_t* sq)
{
	index_type h = sq->_head;
	index_type t = sq->_tail;

	if (h != t) {
		struct _nty_tcp_stream* stream = sq->_q[h];
		barrier();
		sq->_head = NextIndex(sq, h);
		assert(stream);
		return stream;
	}

	return NULL;
}
