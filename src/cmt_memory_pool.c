//
// Created by chimt on 2021/5/26.
//

#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<pthread.h>

#include"mem_pool.h"
#include"global.h"
#include"cmt_errno.h"

__pthread cmt_pool* pool = NULL;

always_inline void*
aligned_malloc(size_t required_bytes, size_t alignment) {
    size_t offset = alignment - 1 + sizeof(void*);
    void* p1 = (void*)malloc(required_bytes + offset);

    if (unlikely(1 == NULL)) {
        return NULL;
    }
    void** p2 = (void**)(((size_t)p1 + offset) & ~(alignment - 1));

    p2[-1] = p1;

    return p2;
}

always_inline void
aligned_free(void* p2) {
    void* p1 = ((void**)p2)[-1];
    free(p1);
}

cmt_pool_t*
cmt_create_pool(size_t size) {
    size_t totalSize;
    cmt_pool_control_block_t ctb;
    size_t header_size;

    size = size < CMT_DEFAULT_POOL_SIZE ?
           CMT_DEFAULT_POOL_SIZE: size;
    size = ROUND_2_UP(size);
    header_size = sizeof(cmt_pool_t);
    totalSize = size + header_size;

    pool = (cmt_pool_t*)aligned_malloc(totalSize, CMT_CACHELINE);
    if (pool == NULL) {
#ifdef DBGERR
        TRACE_ERROR(stdout, "memalign failed");
#endif
        cmt_errno = ENONMEM;
        return NULL;
    }

    pool->tid = cmt_gettid();
    ctb = pool->cb;
    ctb.size = size;
    ctb.remained = size;
    ctb.large_ctb = NULL;
    ctb.last.size = size;
    ctb.last.data = (char*)((char*)pool + header_size);
    ctb.non_main_arena = NULL;

    memset(pool->smallbins, 0, sizeof(cmt_pool_t) * TOTAL_BINS);

    return pool;
}

cmt_pool_t* 
get_cmt_pool() {
    if (pool == NULL) {
        size_t size = CMT_DEFAULT_POOL_SIZE;
        return cmt_create_pool(size);
    }
    return pool;
}

cmt_pool_t*
get_cmt_pool_s(size_t size) {
    if (NULL == pool) {
        return cmt_create_pool(size);
    }
    return pool;
}

void
cmt_destory_pool(cmt_pool_t* pool) {
    large_chunk_t* largeBins, * next;

    if (pool->cb.remained != pool->cb.size) {
#ifdef DBGERR
        TRACE_ERROR(stdout, "memory in used");
#endif
    }

    for (largeBins = pool->cb.large_ctb; largeBins; largeBins = next) {
        next = largeBins->next;
        free(largeBins);
    }

    aligned_free(pool);
}

int
cmt_reset_pool(cmt_pool_t* pool) {
    large_chunk_t* large_bins, * next;
    cmt_pool_control_block_t* ctb = &pool->cb;
    non_main_arena_t* non_main_arena = ctb->non_main_arena;
    non_main_arena_t * tmp;

    if (pool->cb.remained != pool->cb.size) {
#ifdef DBGERR
        TRACE_ERROR(stdout, "memory in used");
#endif
        cmt_errno = EBUSY;
        return -1;
    }

    for (large_bins = ctb->large_ctb; large_bins; large_bins = next) {
        next = large_bins->next;
        ctb->size -= large_bins->size;
        free(large_bins);
    }
    ctb->remained = ctb->size;
    ctb->large_ctb = NULL;
    ctb->last.size = ctb->size;
    ctb->last.data = (char*)((void*)pool + sizeof(cmt_pool_t));

    while(non_main_arena) {
        tmp = non_main_arena->next;
        free(non_main_arena);
        non_main_arena = tmp;
    }

    memset(pool->smallbins, 0, sizeof(cmt_pool_bin_t) * TOTAL_BINS);

    return 0;
}

#define MAXMEMSIZE 512

#define ALIGN 8
#define ROUND_UP(size, align) (((size) + (align) - 1) & ~((align) - 1))

#define SMALLBINS_IDX(size)   ((size) >> 3)

#define GETSIZE(idx)    ((idx)<<3)

#define NUMBEROFADDEDNODE 20

/** \brief allocate big size memory from pool
    --------------------------------------------------------------
    @param pool memory pool
    @param size memory size
    @return pointer to memory
 */
void* cmt_large_alloc(cmt_pool_t* pool, size_t size);

/** \brief allocate small size memory from pool
    --------------------------------------------------------------
    @param pool memory pool
    @param size memory size
    @return pointer to memory
 */
void* cmt_small_alloc(cmt_pool_t* pool, size_t size);

/** \brief refill bins from remained memory space
    --------------------------------------------------------------
    @param pool memory pool
    @param size memory size
    @param p pointer to pointer to get memory
    @return int 0 sucessed -1 failed
 */
int  refill(cmt_pool_t* pool, void** p, size_t size);

/** \brief allocate memory from remained memory space
    @param pool memory pool
    @param size memory size
    @param nums total nums of memory block
    @param pointer to memory with allocated size
 */
void* block_alloc(cmt_pool_t* pool, size_t size, size_t nums);

/** \brief allocate from other bins in small bins list
    --------------------------------------------------------------
    @param pool memory pool
    @param size memory size
    @param p pointer to pointer to memory with allocated size
    @return int 0 successed -1 fail
 */
int alloc_from_other_bins(cmt_pool_t* pool, void **p, size_t size);

/** \brief allocate memory from free large block
    --------------------------------------------------------------
    @param pool memory pool
    @param p pointer to pointer to memory with allocated size
    @param size memory size
    @return int 0 success -1 fail
 */
int alloc_from_large(cmt_pool_t* pool, void **p, size_t size);

/** \brief allocate memory from system only when main_arena
    doesnt have sufficient space
    -----------------------------------------------------------
    @param pool memory pool
    @param size allocated size
    @return 0 success -1 fail
 */
int create_non_main_arena(cmt_pool_t* pool, size_t size);

/** \brief put the remained memory to specified small bins and
    make preparetion for func create_non_main_arena()
    -----------------------------------------------------------
    @param pool memory pool
 */
void clear_block(cmt_pool_t* pool);

void*
cmt_palloc(cmt_pool_t* pool, size_t size) {
    size = ROUND_UP(size, ALIGN);
    if (size > MAXMEMSIZE) {
        return cmt_large_alloc(pool, size);
    }
    return cmt_small_alloc(pool, size);
}

void*
cmt_pcalloc(cmt_pool_t* pool, size_t size) {
    void* p = cmt_palloc(pool, size);

    if(unlikely(p == NULL)) {

        return NULL;
    }
    memset(p, 0, size);
    return p;
}

void
cmt_pfree(cmt_pool_t* pool, void* p, size_t size) {
    cmt_pool_control_block_t* ctb = &pool->cb;
    cmt_pool_bin_t* small_bin;
    cmt_chunk_t* chunk;
    non_main_arena_t *non_main_arena = ctb->non_main_arena;
    non_main_arena_t *tmp;

    size = ROUND_UP(size, ALIGN);

    if (size > MAXMEMSIZE) {
        large_chunk_t* large_bins = (large_chunk_t*)p;
        large_bins->size = size;
        large_bins->next = ctb->large_ctb;
        ctb->large_ctb = large_bins;
        return;
    }

    while(non_main_arena){
        tmp = non_main_arena->next;
        free(non_main_arena);
        non_main_arena = tmp;
    }

    small_bin = &pool->smallbins[SMALLBINS_IDX(size)];
    chunk = (cmt_chunk_t*)p;
    chunk->next = small_bin->free;
    small_bin->free = chunk;

    return;
}

void*
cmt_large_alloc(cmt_pool_t* pool, size_t size) {
    void* p = malloc(size);

    if (p == NULL) {
#ifdef DBGERR
        TRACE_ERROR(stdout, "memory alloc failed");
#endif
        cmt_errno = ENONMEM;
        return NULL;
    }

    return p;
}

void*
cmt_small_alloc(cmt_pool_t* pool, size_t size) {
    size_t index = SMALLBINS_IDX(size);
    cmt_pool_bin_t* sblist, * bin;
    cmt_chunk_t* fchunk;
    void* p;
    int ret;

    bin = &sblist[index];
    fchunk = bin->free;
    if (bin->nums > 0) {
        bin->free = fchunk->next;
        fchunk->next = NULL;
        p = (void*)fchunk;
        --bin->nums;
        return p;
    }
    ret = refill(pool, &p, size);
    if (unlikely(ret < 0)) {
#ifdef DBGERR
        TRACE_ERROR(stdout, "refill failed");
#endif
        size_t non_main_arena_size = CMT_DEFAULT_POOL_SIZE;
        size_t num = NUMBEROFADDEDNODE;
        clear_block(p);
        ret = create_non_main_arena(p, non_main_arena_size);
        if(unlikely(ret < 0)) {
            return NULL;
        }
        p = block_alloc(pool, size, num);
    }
    if(unlikely(p == NULL)) {
        cmt_errno = ENONMEM;
    }
    return p;
}

int
refill(cmt_pool_t* pool, void** p, size_t size) {
    int ret;
    size_t nums = NUMBEROFADDEDNODE;
    /*size_t totalSize = nums * size;*/
    cmt_pool_control_block_t* ctb = &pool->cb;

    if (ctb->last.size > size) {
        *p = block_alloc(pool, size, nums);
        return 0;
    }
    else if (ctb->remained > size) {
        ret = alloc_from_other_bins(pool, p, size);
    }
    if (ret < 0) {
        ret = alloc_from_large(pool, p, size);
    }

#ifdef DBGERR
    if (ret < 0) {
        TRACE_ERROR("no memory unused");
    }
#endif
    return ret;
}

void*
block_alloc(cmt_pool_t* pool, size_t size, size_t nums) {
    cmt_chunk_t* chunk;
    void* p;
    size_t i;
    cmt_pool_control_block_t* ctb = &pool->cb;
    size_t totalSize = size * nums;
    last_block_t* last = &ctb->last;
    cmt_pool_bin_t* bin = &pool->smallbins[SMALLBINS_IDX(size)];

    ASSERT(last->size > size);

    if (last->size < totalSize) {
        totalSize = ROUND_UP(last->size, size);
    }
    p = (void*)last->data;
    last->data = (char*)last->data + totalSize;
    last->size -= totalSize;
    chunk = (cmt_chunk_t*)((char*)p + size);

    bin->nums = nums - 1;
    bin->free = chunk;

    if (totalSize > size) {
        for (i = 0; i < TOTAL_BINS - 2; ++i) {
            chunk->next = (cmt_chunk_t*)((char*)chunk + size);
            chunk = (cmt_chunk_t*)((char*)chunk + size);
        }
        chunk->next = NULL;
    }

    return p;
}

void
clear_block(cmt_pool_t *pool) {
    cmt_pool_control_block_t *ctb = &pool->cb;
    last_block_t *t_block = &ctb->last;
    size_t idx = SMALLBINS_IDX(t_block->size);
    cmt_pool_bin_t * bin = &pool->smallbins[idx];
    cmt_chunk_t *chunk = (cmt_chunk_t*)(t_block->data);

    chunk->next = bin->free;
    bin->free = chunk;
    bin->nums++;
    t_block->size = 0;
    t_block->data = NULL;
}

int create_non_main_arena(cmt_pool_t *pool, size_t size) {
    cmt_pool_control_block_t *ctb = &pool->cb;
    last_block_t *t_block = &ctb->last;
    non_main_arena_t *arena, *tmp;
    size_t total_size = size + sizeof(non_main_arena_t);
    char *data;

    arena = malloc(total_size);

    if(unlikely(arena == NULL)){
        return -1;
    }

    tmp = ctb->non_main_arena;
    if(tmp == NULL) {
        ctb->non_main_arena = arena;
    }
    while(tmp->next){
        tmp = tmp->next;
    }
    tmp->next = arena;
    data = (char*)arena + sizeof(non_main_arena_t);

    ctb->last.data = data;
    ctb->last.size += size;
    return 0;
}

int
alloc_from_other_bins(cmt_pool_t* pool, void** p, size_t size) {
    cmt_chunk_t* remain;
    size_t i;
    size_t size_idx = SMALLBINS_IDX(size);
    cmt_pool_bin_t* bins = pool->smallbins;

    for (i = size_idx; i < TOTAL_BINS; ++i) {
        if (bins[i].nums > 0) {
            cmt_chunk_t* ptr = bins[i].free;
            *p = (void*)ptr;
            bins[i].free = ptr->next;
            bins[i].nums--;
            remain = (cmt_chunk_t*)((char*)ptr + size);
            i = GETSIZE(i);
            remain->next = bins[SMALLBINS_IDX(i - size)].free;
            bins[SMALLBINS_IDX(i - size)].free = remain;
            return 0;
        }
    }

#ifdef TRACE_ERROR
    TRACE_ERROR("no memory unused");
#endif
    return -1;
}

int
alloc_from_large(cmt_pool_t* pool, void** p, size_t size) {
    cmt_pool_control_block_t* ctb = &pool->cb;
    large_chunk_t* large_bins = ctb->large_ctb;

    if (large_bins) {
        size_t remain_size = large_bins->size - size;
        *p = large_bins;

        if (remain_size > MAXMEMSIZE) {
            large_bins = (large_chunk_t*)((char*)large_bins + size);
            large_bins->size -= size;
            large_bins->next = ((large_chunk_t*)(*p))->next;
            large_chunk_t* tmp = ((large_chunk_t*)(*p));
            tmp->next = NULL;
            tmp->size = 0;
            return 0;
        }

        cmt_pool_bin_t *bins = pool->smallbins;
        cmt_chunk_t* temp = (cmt_chunk_t*)((char*)p + size);
        temp->next = bins[SMALLBINS_IDX(remain_size)].free;
        bins[SMALLBINS_IDX(remain_size)].free = temp;
        return 0;
    }

#ifdef TRACE_ERROR
    TRACE_ERROR("no large memory unused");
#endif
    return -1;
}

void*
cmt_pmemalign(cmt_pool_t* pool, size_t align, size_t size) {
    size_t offset = align - 1 + size;
    size_t total_size = offset + size;
    void * p1 = cmt_palloc(pool, total_size);

    if(unlikely(p1 == NULL)){
        return p1;
    }

    void ** p2 = (void**) (((uintptr_t)p1 + offset) & ~(align - 1));
    p2[-1] = p1;
    return p2;
}

void
cmt_pmemfree(cmt_pool_t* pool, void* p, size_t align, size_t size) {
    void *p1 = ((void**)p)[-1];
    size_t offset = align - 1 + size;
    size_t totalsize = offset + size;
    cmt_pfree(pool, p1, totalsize);
}


