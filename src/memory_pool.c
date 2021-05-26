#include<stdlib.h>
#include<stdio.h>

#include"memory_pool.h"


ALWAYS_INLINE void*
aligned_malloc(size_t required_bytes, size_t alignment) {
    int offset = alignment - 1 + sizeof(void*);
    void p1 = (void*)malloc(required_bytes + offset);

    if (p1 == NULL) {
        return NULL;
    }
    void** p2 = (void**)(((size_t)p1 + offset) & ~(alignment - 1));

    p2[-1] = p1;

    return p2;
}

void
aligned_free(void* p2) {
    void* p1 = ((void**)p2)[-1];
    free(p1);
}

cmt_pool_t*
cmt_create_pool(size_t size) {
    cmt_pool_t* pool;
    int ret;
    int totalSize;
    cmt_pool_control_block_t ctb;
    size_t header_size;

    totalSize = size > CMT_PAGESIZE ? CMT_PAGESIZE : size;
    header_size = ROUND_UP(sizeof(cmt_pool_t));
    size = ROUND_UP(size);
    totalSize = header_size + size;

    pool = (cmt_pool_t*)aligned_malloc(totalSize, CMT_CACHELINE);
    if (pool == NULL) {
#ifdef DBGERR
        TRACE_ERROR(stdout, "memalign failed");
#endif
        return NULL;
    }

    ctb = pool->cb;
    ctb.size = totalSize;
    ctb.remained = size;
    ctb.large_ctb = NULL;
    ctb.last.size = size;
    ctb.last.data = (char*)((char*)data + header_size);

    pool->large = NULL;
    memset(pool->smallbins, 0, sizeof(cmt_pool_t) * TOTAL_BINS);

    return pool;
}

void
cmt_destory_pool(cmt_pool_t* pool) {
    large_bins_t* largeBins, * next;

    if (pool->cb.remained != pool->cb.size) {
#ifdef DBGERR
        TRACE_ERROR(stdout, "memory in used");
#endif
    }

    for (largeBins = pool->cb.large_ctb; largeBins; largeBins = next) {
        next = largeBins->next;
        free(largeBins);
    }

    aligned_free(pool, CMT_CACHELINE);
}

int
cmt_reset_pool(cmt_pool_t* pool) {
    large_bins_t* large_bins, * next;
    cmt_pool_control_block_t ctb = pool->cb;

    if (pool->cb.remained != pool->cb.size) {
#ifdef DBGERR
        TRACE_ERROR(stdout, "memory in used");
#endif
        return -1;
    }

    for (large_bins = ctb.large_ctb; large_bins; large_bins = next) {
        next = large_bins->next;
        cb.size -= large_bins->size;
        free(large_bins);
    }
    ctb.remained = ctb.size;
    ctb.large_ctb = NULL;
    ctb.last.size = size;
    ctb.last.data = (char*)((void*)data + ROUND_UP(sizeof(cmt_pool_t)));

    memset(pool->smallbins, 0, sizeof(cmt_pool_bins_t) * TOTAL_BINS);

    return 0;
}

#define MAXMEMSIZE 512

#define ALIGN 8
#define ROUND_UP(size, align) ((size + align - 1) & ~(align - 1))
#define SMALLBINS_INDEX(size, align) ((size + align - 1) / align - 1)
#define IDX_TO_SIZE(idx) (idx << 3)

#define NUMBEROFADDEDNODE 20

void* cmt_large_alloc(cmt_pool_t* pool, size_t size);
void* cmt_small_alloc(cmt_pool_t* pool, size_t size);
int  refill(cmt_pool_t* pool, void** p, size_t size);
void* block_alloc(cmt_pool_t* pool, size_t size, size nums);
int alloc_from_other_bins(cmt_pool_t* pool, size_t size);
int alloc_from_large(cmt_pool_t* pool, size_t size);

void*
cmt_palloc(cmt_pool_t* pool, size_t size) {
    cmt_pool_control_block_t ctb = pool->cb;

    size = ROUND_UP(size, ALIGN);
    if (size > MAXMEMSIZE) {
        return cmt_large_alloc(pool, size);
    }
    return cmt_small_alloc(pool, size);
}

void*
cmt_palloc(cmt_pool_t* pool, size_t size) {
    void* p = cmt_palloc(pool, size);
    memset(p, 0, size);
    return p;
}

void
cmt_pfree(cmt_pool_t* pool, void* p, size_t size) {
    cmt_pool_control_block_t* ctb = pool->cb;
    cmt_pool_bins_t* small_bin, bin;

    if (size > MAXMEMSIZE) {
        large_bins_t* large_bins = (large_bins_t)p;
        large_bins->size = size;
        large_bins->next = ctb->large_ctb;
        ctb->large_ctb = large_bins;
        return;
    }

    small_bin = &pool->smallbins[SMALLBINS_INDEX(size)];
    bin = (cmt_pool_t*)p;
    p->next = smallBins;
    *small_bin = *p;
    return;
}

void*
cmt_large_alloc(cmt_pool_t* pool, size_t size) {
    void* p = malloc(totalSize);

    if (p == NULL) {
#ifdef DBGERR
        TRACE_ERROR(stdout, "memory alloc failed");
#endif
        return NULL;
    }

    return p;
}

void*
cmt_small_alloc(cmt_pool_t* pool, size_t size) {
    cmt_pool_control_block_t ctb = pool->cb;
    size_t index = SMALLBINS_INDEX(size, ALIGN);
    cmt_pool_bins_t* sblist, * bin;
    cmt_chunk_t* fchunk;
    void* p;
    int ret;

    bin = &sblist[index];
    fchunk = bin->free;
    if (bin->nums > 0) {
        p = fchunk.data;
        --bin->nums;
        bin->free = fchunk->next;
        return p;
    }
    ret = refill(pool, &p, size);
    if (ret < 0) {
#ifdef DBGERR
        TRACE_ERROR(stdout, "refill failed");
#endif
        return NULL;
    }
    return p;
}

int
refill(cmt_pool_t* pool, void** p, size_t size) {
    int ret;
    size_t nums = NUMBEROFADDEDNODE;
    /*size_t totalSize = nums * size;*/
    cmt_pool_control_block_t* ctb = pool->cb;

    if (ctb->last.size() > size) {
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
    cmt_pool_control_block_t* ctb = &pool->ctb;
    size_t totalSize = size * nums;
    last_block_t* last = &ctb->last;
    cmt_pool_bins_t* bin = &pool->smallbins[MEMIDX(size)];

    ASSERT(last->size > size);

    if (last->size < totalSize) {
        totalSize = size;
    }
    p = (void*)last->data;
    last->data = last->data + totalSize;
    last->size -= totalSize;
    chunk = (cmt_chunk_t*)((char*)p + size);

    bin->size = nums - 1;
    bin->free = chunk;

    if (last->size >= size * nums) {
        for (i = 0; i < TOTAL_BINS - 2; ++i) {
            chunk->next = (cmt_chunk_t*)((char*)chunk + size);
            chunk = (cmt_chunk_t*)((char*)chunk + size);
        }
        chunk->next = NULL;
    }

    return p;
}

int
alloc_from_other_bins(cmt_pool_t* pool, void** p, size_t size) {
    cmt_chunk_t* remain;
    size_t i;
    size_t size_idx = MEMIDX(size);
    cmt_pool_bins_t* bins = &pool->smallbins;

    for (i = size_idx; i < TOTAL_BINS; ++i) {
        if (bins[i].nums > 0) {
            void* ptr = bins[i].free;
            *p = ptr;
            bins[i].free = (cmt_chunk_t*)(ptr->next);
            remain = (cmt_chunk_t*)((char*)ptr + size);
            remain->free = bins[SMALLBINS_INDEX(i - size)]->free;
            bins[SMALLBINS_INDEX(i - size)]->free = remain;
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
    cmt_pool_control_block_t* ctb = pool->cb;
    large_bins_t* large_bins = ctb->large_ctb;

    if (large_bins) {
        size_t remain_size = large_bins->size - size;
        *p = largeBins;

        if (remainSize > MAXMEMSIZE) {
            large_bins = (large_bins_t*)((char*)large_bins + size);
            large_bins.size -= size;
            large_bins.next = (large_bins_t*)p->next;
            return 0;
        }

        cmt_chunk_t* temp = (cmt_chunk_t*)((char*)p + size);
        temp->next = ctb[SMALLBINS_INDEX(remain_size)].free;
        ctb[SMALLBINS_INDEX(remain_size)].free = temp;
        return 0;
    }

#ifdef TRACE_ERROR
    TRACE_ERROR("no large memory unused");
#endif
    return -1;
}
