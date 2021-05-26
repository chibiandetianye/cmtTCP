#ifndef _CMT_MALLOC_H_INCLUDE_
#define _CMT_MALLOC_H_INCLUDE_

#include<unistd.h>
#include<stdint.h>

#define CMT_MAX_ALLOC_FROM_POOL (512)

#define CMT_DEFAULT_POOL_SIZE (CMT_PAGESIZE)

#define CMT_POOL_ALIGNMENT 16

#define TOTAL_BINS 7	

#ifndef PTR_SIZE 
#define PTR_SIZE (sizeof(void*))
#endif /* PTR_SIZE */

typedef struct cmt_chunk {
	struct cmt_chunk* next;
} cmt_chunk_t;

typedef struct cmt_pool_bins {
	uint16_t        nums;
	struct cmt_bunk* free;
} cmt_pool_bins_t;

typedef struct large_bins {
	uint64_t size;
	struct   large_bins* next;
} large_bins_t;

typedef struct last_block {
	uint64_t size;
	char* data[0];
} last_block_t;

typedef struct cmt_pool_control_block {
	uint64_t     size;
	uint64_t     remained;
	large_bins_t* large_ctb;
	last_block_t last;
} cmt_pool_control_block_t;

typedef struct cmt_pool {
	cmt_pool_control_block_t cb;
	cmt_pool_bins_t smallbins[TOTAL_BINS];
} cmt_pool_t;

cmt_pool_t* cmt_create_pool(size_t size);
void cmt_destory_pool(cmt_pool_t* pool);
int cmt_reset_pool(cmt_pool_t* pool);

void* cmt_palloc(cmt_pool_t* pool, size_t size);
void* cmt_pcalloc(cmt_pool_t* pool, size_t size);
void cmt_pfree(cmt_pool_t* pool, void* p, size_t size);

#endif /* _CMT_MALLOC_H_INCLUDE_ */