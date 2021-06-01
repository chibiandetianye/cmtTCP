#include<stdio.h>

#include"tcp_stream_hash.h"
#include"cmt_memory_pool.h"
#include"tcp.h"
#include"queue.h"

cmt_hashtable_t*
create_stream_hashtable(hashfn hash, eqfn eq, int bins) {
	int i;
	cmt_pool_t* pool = get_cmt_pool();
	cmt_hashtable_t* ht = cmt_palloc(pool, sizeof(cmt_hashtable_t));
	if (unlikely(ht == NULL)) {
		printf("cmt_palloc: create tcp stream hashtbale \n");
		return NULL;
	}

	ht->hash = hash;
	ht->eq = eq;
	ht->bins = bins;

	ht->table = cmt_palloc(bins, sizeof(cmt_hash_bucket_t));
	if (unlikely(ht)) {
		cmt_pfree(pool, sizeof(cmt_hashtable_t));
		printf("cmt_pcalloc: create tcp stream hashtable bins\n");
		return NULL;
	}

	for (i = 0; i < bins; ++i) {
		tailq_init(&ht->table[i]);
	}

	return ht;
}

void
destroy_hashtable(cmt_hashtable_t* ht) {
	cmt_pool_t* pool = get_cmt_pool();
	cmt_pfree(pool, ht->table, sizeof(hash_bucket_t) * ht->bins);
	cmt_pfree(poolht, sizeof(cmt_hashtable_t));
}


void* 
ht_search(cmt_hashtable_t* ht, const void* it){
	size_t idx;
	hash_bucket_t* head, walk = NULL;

	idx = ht->hash(it);
	head = &ht->table[idx];
	tailq_foreach(walk, head) {
		if (ht->eq(it, walk)) {
			return walk;
		}
	}
	
	return NULL;
}

int
ht_insert(cmt_hashtable_t* ht, const void* it) {
	size_t idx;
	hash_bucket_t* head;

	idx = ht->hash(it);
	head = &ht->table[idx];

	tailq_insert_head(head, (list_node_t*)it);
	ht->ht_count++;
	return 0;
}

void* 
ht_remove(cmt_hashtable_t* ht, const void* it) {
	size_t idx;
	hash_bucket_t* head, walk = NULL;

	idx = ht->hash(it);
	head = &ht->table(idx);
	tailq_foreach(walk, head) {
		if (ht->eq(it, walk)) {
			tailq_remove((tailq_head_t*)head, walk);
			return walk;
		}
	}
	return NULL;
}

unsigned int 
hash_flow(const void* f) {
	cmt_tcp_stream_t* flow = (cmt_tcp_stream_t*)f;

	unsigned int hash, i;
	char* key = (char*)&flow->saddr;

	for (hash = i = 0; i < 12; i++) {
		hash += key[i];
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}
	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

	return hash & (NUM_BINS_FLOWS - 1);
}

int 
equal_flow(const void* f1, const void* f2) {
	cmt_tcp_stream_t* flow1 = (cmt_tcp_stream_t*)f1;
	cmt_tcp_stream_t* flow2 = (cmt_tcp_stream_t*)f2;

	return (flow1->saddr == flow2->saddr &&
		flow1->sport == flow2->sport &&
		flow1->daddr == flow2->daddr &&
		flow1->dport == flow2->dport);
}

unsigned int 
hash_listener(const void* l) {
	cmt_tcp_listener_t* listener = (cmt_tcp_listener_t*)l;
	
	return listener->s->s_addr.sin_port & (NUM_BINS_LISTENER - 1);
}

int 
equal_listern(const void* l1, const void* l2) {
	cmt_tcp_listener_t* listener1 = (cmt_tcp_listener_t*)l1;
	cmt_tcp_listener_t* listener2 = (cmt_tcp_listener_t*)l2;

	return (listener1->s->s_addr.sin_port == listener2->s->s_addr.sin_port);
}

#define is_flow_table(x)	(x == HashFlow)
#define is_listen_table(x)	(x == HashListener)

cmt_tcp_stream_hashtable_t* 
create_stream_hashtable(hashfn hash, eqfn eq, int bins) {
	int i;
	cmt_pool_t* pool = get_cmt_pool();
	cmt_tcp_hashtable_t* ht = cmt_palloc(pool, sizeof(cmt_tcp_hashtable_t));
	if (unlikely(ht == NULL)) {
		printf("cmt_palloc: create tcp stream hashtbale \n");
		return NULL;
	}

	ht->hash = hash;
	ht->eq = eq;
	ht->bins = bins;

	ht->ht_stream = cmt_palloc(bins, sizeof(cmt_tcp_stream_hash_bucket_t));
	if (unlikely(ht)) {
		cmt_pfree(pool, sizeof(cmt_tcp_stream_hashtable_t));
		printf("cmt_pcalloc: create tcp stream hashtable bins\n");
		return NULL;
	}

	for (i = 0; i < bins; ++i) {
		tailq_init((tailq_head_t*)&ht->ht_stream[i]);
	}	

	return ht;
}

void
destroy_tcp_hashtable(cmt_tcp_hashtable_t* ht) {
	cmt_pool_t* pool = get_cmt_pool();
	cmt_pfree(pool, ht->ht_stream, sizeof(hash_bucket_t) * ht->bins);
	cmt_pfree(poolht, sizeof(cmt_tcp_hashtable_t));
}

cmt_tcp_listener_t*
listen_ht_remove(cmt_tcp_hashtable_t* ht, cmt_tcp_listener_t* it) {
	cmt_tcp_listener_hash_bucket_t* head;
	size_t idx = ht->hash(it);
	cmt_tcp_listener_t* walk;

	head = &ht->ht_listener[idx];
	walk = head->tqh_first;
	if (ht->eq(walk, it)) {
		ht->tqh_first = walk->ht_next;
		if (walk->ht_next == NULL) {
			tailq_init(head);
		}
	}
	for (; walk->ht_next; walk = walk->ht_next) {
		if (ht->eq(walk->ht_next, it)) {
			walk->ht_next = walk->ht_next->ht_next;
			if (walk->ht_next == NULL) {
				head->tqh_last = &walk;
			}
			return walk;
		}
	}
	return NULL;
}

cmt_tcp_stream_t* 
stream_ht_remove(cmt_tcp_hashtable_t* ht, cmt_tcp_stream_t* it) {
	cmt_tcp_stream_hash_bucket_t* head;
	size_t idx = ht->hash(it);
	cmt_tcp_stream_t* walk;

	head = &ht->ht_stream[idx];
	walk = head->tqh_first;
	if (ht->eq(walk, it)) {
		ht->tqh_first = walk->ht_next;
		if (walk->ht_next == NULL) {
			tailq_init(head);
		}
	}
	for (; walk->ht_next; walk = walk->ht_next) {
		if (ht->eq(walk->ht_next, it)) {
			walk->ht_next = walk->ht_next->ht_next;
			if (walk->ht_next == NULL) {
				head->tqh_last = &walk;
			}
			return walk;
		}
	}
	return NULL;
}

int 
stream_ht_insert(cmt_tcp_hashtable_t* ht, cmt_tcp_stream_t* it) {
	size_t idx;
	cmt_tcp_stream_hash_bucket_t* head;

	idx = ht->hash(it);
	head = ht->ht_stream[idx];
	if ((it->rcvvar->h_next = head->tqh_first) == NULL) {
		head->tqh_last = &it->rcvvar->h_next;
	}
	head->tqh_first = it;

	ht->ht_count++;
	return 0;
}

int
listen_ht_insert(cmt_tcp_hashtable_t* ht, cmt_tcp_listener_t* it) {
	size_t idx;
	cmt_tcp_listener_hash_bucket_t* head;

	idx = ht->hash(it);
	head = ht->ht_listener[idx];
	if ((it->ht_next = head->tqh_first) == NULL) {
		head->tqh_last = &it->ht_next;
	}
	head->tqh_first = it;

	ht->ht_count++;
	idx = ht->hash(it);
}

cmt_tcp_stream_t* 
stream_ht_search(cmt_tcp_hashtable_t* ht, cmt_tcp_stream_t* it) {
	size_t idx;
	cmt_tcp_stream_hash_bucket_t* head;
	cmt_tcp_stream_t* walk;

	idx = ht->hash(it);
	for (walk = head->tqh_first; walk; walk = walk->rcvvar->h_next) {
		if (ht->eqfn(walk, it)) {
			return walk;
		}
	}
	return NULL;
}

cmt_tcp_listener_t* 
listen_ht_search(cmt_tcp_hashtable_t* ht, uint16_t* it) {
	size_t idx;
	cmt_tcp_listener_t* listener, walk;
	cmt_tcp_listener_hash_bucket_t* head;
	uint16_t port = *it;

	cmt_socket_t s;
	s.s_addr.sin_port = port;
	listener.s = &s;

	idx = ht->hash(&listener);
	head = &ht->ht_listener[idx];
	for (walk = head->tqh_first; walk; walk = walk->ht_next) {
		if (ht->eqfn(walk, &listener)) {
			return walk;
		}
	}

	return NULL;
}