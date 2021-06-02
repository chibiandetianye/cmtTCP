#include<stdio.h>

#include"object_pool.h"
#include"cmt_memory_pool.h"

cmt_object_pool_t*
cmt_object_pool_create_s(size_t size, size_t elemen,
	size_t align) {
	cmt_object_node_t* tmp;
	cmt_pool_t* pool = get_cmt_pool();
	cmt_object_pool_t* obj_p = cmt_palloc(pool, sizeof(cmt_object_pool_t);
	if (unlikely(obj_p)) {
		printf("%s [%d] failed\n", __FUNCTION__, __LINE__);
		return NULL;
	}

	obj_p->obj_info.align = align;
	obj_p->obj_info.element_count = size;
	obj_p->obj_info.element_size = elemen;
	obj_p->obj_info.free_count = elemen;
	obj_p->obj_queue = (cmt_object_node_t*)cmt_pmemalign(pool, align, elemen);
	if (unlikely(obj_p->obj_queue == NULL)) {
		printf("%s [%d] failed\n", __FUNCTION__, __LINE__);
		cmt_pfree(pool, obj_p, sizeof(cmt_object_pool_t));
		return NULL;
	}
	obj_p->obj_queue->next = NULL;
	tmp = obj_p->obj_queue;

	for (int i = 1; i < size; ++i) {
		tmp->next = (cmt_object_node_t*)cmt_pmemalign(pool, align, elemen);
		if (unlikely(obj_p->obj_queue == NULL)) {
			printf("%s [%d] failed\n", __FUNCTION__, __LINE__);
			tmp = obj_p->obj_queue;
			for (int j = 0; j < i; ++j) {
				cmt_pmemfree(pool, tmp, align, elemen);
			}
			tmp = tmp->next;

			cmt_pfree(pool, obj_p, sizeof(cmt_object_pool_t));
			return NULL;
		}
		tmp = tmp->next;
		tmp->next = NULL;
	}
	return obj_p;
}

cmt_object_pool_t*
cmt_object_pool_create(size_t elm, size_t align) {
	size_t size = DEFAULT_MIN_SIZE;
	return cmt_object_pool_create_s(size, elm, align, init, d);
}

void
cmt_object_pool_destroy(object_pool_t* p) {
	cmt_object_node_t* node = p->obj_queue£¬ tmp;
	cmt_pool_t* pool = get_cmt_pool();
	size_t align = p->obj_info.align;

	while (node) {
		tmp = node->next;
		cmt_pmemfree(pool, p, align);
		node = tmp;
	}

	cmt_pfree(pool, p, sizeof(cmt_object_node_t));
}

always_inline void 
resize(object_pool_t* p) {
	size_t update_size;
	cmt_obj_info_t* info = p->obj_info;
	cmt_obj_node_t* head = p->obj_queue, *new_node;
	size_t size = info->element_size;
	size_t align = info->align;
	
	if (info->element_count * WATER_LEVEL <= info->free_count) {
		return;
	}
	update_size = info->element_count;
	if (info->element_count >= PEEK) {
		update_size = DEFAULT_MIN_SIZE;
	}
	cmt_pool_t* pool = get_cmt_pool();
	for (int i = 0; i < update_size; ++i) {
		new_node = (cmt_obj_node_t*)cmt_pmemalign(pool, align, size);
		if (unlikely(new_node == NULL)) {
			return;
		}
		new_node->next = head;
		head = new_node;
	}
	p->obj_queue = new_node;
	p->obj_info.element_count += update_size;
	p->obj_info.free_count += update_size;
}

void*
cmt_object_get(object_pool_t* pool) {
	resize(pool);
	if (pool->obj_info.free_count <= 0) {
		printf("%s [%d] failed\n", __FUNCTION__, __LINE__);
		return NULL;
	}
	void* p = pool->obj_queue;
	pool->obj_queue = pool->obj_queue->next;
	pool->obj_info.free_count--;
	return p;
}

void
cmt_object_free(object_pool_t* pool, void* p) {
	cmt_object_node_t* node = (cmt_object_node_t*)p;
	node->next = pool->obj_queue;
	pool->obj_queue = node;
	pool->obj_info.free_count++;
}

