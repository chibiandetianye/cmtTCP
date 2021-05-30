//
// Created by chimt on 2021/5/28.
//
#include<stdio.h>

#include"ring.h"
#include"cmt_errno.h"
#include"cmt_memory_pool.h"


cmt_ring_t*
cmt_ring_create(const char *name, unsigned int count,
                uint32_t sp, uint32_t sc) {
    cmt_ring_t* r;
    unsigned int size, total_size;
    struct prod* p;
    struct cons* c;
    cmt_pool_t pool_;

    if(unlikely(count != ROUND_2_UP(count))) {
        cmt_errno = EINVAL;
        return NULL;
    }

    pool_ = get_cmt_pool();
    size = count * sizeof(void*);
    total_size = sizeof(cmt_ring_t) + size;
    r = (cmt_ring_t *)cmt_pmemalign(pool_, __alignof__(cmt_ring_t), total_size);
    if(unlikely((r == NULL))) {
        cmt_errno = ENONMEM;
        return NULL;
    }

    snprintf(r->name, CMT_RING_NAMESIZE, "%s", name);

    p = &r->prod;
    p->sp_enqueue = sp;
    p->size = count;
    p->mask = count - 1;
    p->head = p->tail = 0;

    c = &r->cons;
    c->sc_dequeue = sc;
    c->size = count;
    c->mask = count - 1;
    c->head = c->tail = 0;

    return r;
}

void
cmt_ring_destory(cmt_ring_t* r) {
    cmt_pool_t* pool_ = get_cmt_pool();
    unsigned int size = sizeof(cmt_ring_t) + r->cons.size * sizeof(void*);
    cmt_pmemfree(p_, r, __alignof__(cmt_ring_t), size);
}
