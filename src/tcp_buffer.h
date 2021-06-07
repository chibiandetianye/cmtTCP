#ifndef _TCP_BUFFER_INCLUDE_H_
#define _TCP_BUFFER_INCLUDE_H_

#include"object_pool.h"

typedef struct cmt_sb_manager
{
	size_t chunk_size;
	uint32_t cur_num;
	cmt_object_pool_t* sb_pool;
} cmt_sb_manager_t;

typedef struct cmt_send_buffer {
	unsigned char* data;
	unsigned char* head;

	uint32_t head_off;
	uint32_t tail_off;
	uint32_t len;
	uint64_t cum_len;
	uint32_t size;

	uint32_t head_seq;
	uint32_t init_seq;
} cmt_send_buffer_t;

typedef uint32_t index_type;
typedef int32_t signed_index_type;

typedef struct _cmt_sb_queue {
	index_type _capacity;
	volatile index_type _head;
	volatile index_type _tail;

	nty_send_buffer* volatile* _q;
} cmt_sb_queue;

/**rb frag queue**/

typedef struct cmt_data_ptr {
	uint32_t seq;
	uint32_t len;
	void* data;
	unsigned char* stream;
	cmt_data_ptr* next;
} cmt_data_ptr_t;

/** recv ring buffer **/

typedef struct cmt_fragment_ctx {
	struct _nty_fragment_t* next;
	uint32_t seq;
	uint32_t len;
	cmt_data_ptr_t* data_list;
} cmt_fragment_t;

typedef struct cmt_recv_buffer {
	uint64_t cum_len;
	int last_len;
	int chunk_size;

	uint32_t head_seq;
	uint32_t init_seq;

	cmt_fragment_t* fctx;
} cmt_recv_buffer_t;

typedef struct cmt_recv_manager {
	size_t chunk_size;
	uint32_t cnum;

	cmt_object_pool_t* recv_pool;
	cmt_object_pool_t* dataptr_pool;
} cmt_recv_manager_t;

typedef struct cmt_stream_queue
{
	index_type _capacity;
	volatile index_type _head;
	volatile index_type _tail;

	struct cmt_tcp_stream* volatile* _q;
} cmt_stream_queue_t;

typedef struct cmt_stream_manager
{
	size_t stream_size;
	uint32_t cnum;
	
	cmt_object_pool_t* stream_pool;
} cmt_stream_manager_t;

cmt_sb_manager_t* cmt_sbmanager_create(size_t chunk_size, uint32_t cnum);

void cmt_sbmanager_destroy(cmt_sb_manager_t* sbm);

cmt_send_buffer_t* SBInit(cmt_sb_manager_t* sbm, uint32_t init_seq);

void SBFree(cmt_sb_manager_t* sbm, cmt_send_buffer_t* buf);

size_t SBPut(cmt_send_buffer_t* buf, const void* data, size_t len);

size_t SBRemove(cmt_send_buffer* buf, size_t len);

cmt_recv_manager_t* recv_manager_create(size_t chunk_size, uint32_t cnum);

void recv_manager_destroy(cmt_recv_managet_t* cmt);

int recv_put(cmt_recv_manager_t* rbm, cmt_recv_buffer_t* buff,
	void* data, char* stream, uint32_t len, uint32_t cur_seq);

size_t recv_remove(cmt_recv_manager_t* rbm, cmt_recv_buffer* buff, size_t len, int option);

cmt_stream_manager_t* create_stream_manager();

void stream_manager_destroy(cmt_stream_manager_t* sm);

always_inline int
stream_queue_is_empty(cmt_stream_queue_t* sq)
{
	return (sq->_head == sq->_tail);
}

int free_stream(cmt_stream_manager_t* sm, cmt_tcp_stream_t* stream);

cmt_tcp_stream_t* get_stream(cmt_stream_manager_t* sm);


#endif /** _TCP_BUFFER_INCLUDE_H_ */