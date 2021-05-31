#include<stdio.h>

#include"tcp_stream_hash.h"
#include"cmt_memory_pool.h"
#include"tcp.h"
#include"queue.h"

unsigned int 
HashFlow(const void* f) {
	nty_tcp_stream* flow = (nty_tcp_stream*)f;

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
	nty_tcp_stream* flow1 = (nty_tcp_stream*)f1;
	nty_tcp_stream* flow2 = (nty_tcp_stream*)f2;

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

cmt_hashtable_t* 
create_hashtable(hashfn hash, eqfn eq, int bins) {
	int i;
	cmt_pool_t* pool = get_cmt_pool();
	cmt_hashtable_t* ht = cmt_palloc(pool, sizeof(cmt_hashtable_t));
	if (unlikely(ht == NULL)) {
		printf("cmt_palloc: create hashtbale \n");
		return NULL;
	}

	ht->hash = hash;
	ht->eq = eq;
	ht->bins = bins;

	if (is_flow_table(hash)) {
		ht->ht_stream = cmt_pcalloc(bins, sizeof(hash_bucket_t));
		if (unlikely(ht)) {
			cmt_pfree(pool, sizeof(cmt_hashtable_t));
			printf("cmt_pcalloc: createhashtable bins\n");
			return NULL;
		}

		for (i = 0; i < bins; ++i) {
			stailq_init(&ht->ht_stream[i]);
		}
	}
	else if (is_listen_table(hash)) {
		ht->ht_listener = cmt_pcalloc(bins, sizeof(hash_bucket_t));
		if (unlikely(ht->ht_listener)) {
			printf("cmt_pcalloc: create hashtable bins\n");
			cmt_pfree(pool, sizeof(cmt_hashtable_t));
			return NULL;
		}
		
		for (i = 0; i < bins; ++i) {
			stailq_init(&ht->ht_stream);
		}
	}

	return ht;
}

void 
destroy_hashtable(cmt_hashtable_t* ht) {
	cmt_pool_t* pool = get_cmt_pool();
	cmt_pfree(pool, ht->ht_stream, sizeof(hash_bucket_t) * ht->bins);
	cmt_pfree(poolht, sizeof(cmt_hashtable_t));
}
