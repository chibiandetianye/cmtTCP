#ifndef _OBJECT_POOL_INCLUDE_H_
#define _OBJECT_POOL_INCLUDE_H_

#include<stdint.h>

#include"ring.h"
#include"tcp.h"
#include"global.h"

#define WATER_LEVEL 0.9
#define DEFAULT_MIN_SIZE 10
#define PEEK (1024 * 512)


typedef struct cmt_object_info {
	size_t element_size;
	size_t element_count;
	size_t free_count;
	size_t align;
} cmt_oject_info_t;

typedef struct cmt_object_node {
	cmt_object_node* next;
} cmt_object_node_t;

typedef struct cmt_object_pool {
	cmt_oject_info_t obj_info;
	cmt_object_node_t* obj_queue;
} cmt_object_pool_t;

cmt_object_pool_t*
cmt_object_pool_create_s(size_t size, size_t elemen, 
	size_t align);

cmt_object_pool_t*
cmt_object_pool_create(size_t elem, size_t align);

void
cmt_object_pool_destroy(cmt_object_pool_t* pool);

void*
cmt_object_get(cmt_object_pool_t* pool);

void
cmt_object_free(cmt_object_pool_t* pool, void* p);

always_inline size_t 
get_pool_nums(cmt_object_pool_t* pool) {
	return pool->obj_info.element_count;
}

#endif /** _OBJECT_POOL_INCLUDE_H_ */