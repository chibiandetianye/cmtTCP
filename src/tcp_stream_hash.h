#ifndef _TCP_STREAM_HASH_INCLUDE_H_
#define _TCP_STREAM_HASH_INCLUDE_H_

#include<stdint.h>

#include"tcp.h"

#define NUM_BINS_FLOWS		131072
#define NUM_BINS_LISTENER	1024
#define TCP_ACK_CNT			3

typedef struct hash_bucket_ {
	struct hash_bucket_* tqh_first;
	struct hash_bucket_** tqh_last;
} hash_bucket_t;

typedef unsigned int (*hashfn) (const void*);
typedef int (*eqfn) (const void*, const void*);

typedef struct cmt_hashtable {
	uint8_t ht_count;
	uint8_t bins;

	union {
		hash_bucket_t* ht_stream;
		hash_bucket_t* ht_listener;
	};

	hashfn hash;
	eqfn eq;
} cmt_hashtable_t;

void* listen_ht_search(cmt_hashtable_t* ht, const void* it);
void* stream_ht_search(cmt_hashtable_t* ht, const void* it);
int listen_ht_insert(cmt_hashtable_t* ht, void* it);
int stream_ht_insert(cmt_hashtable_t* ht, void* it);

unsigned int hash_flow(const void* f);
int equal_flow(const void* f1, const void* f2);
unsigned int hash_listener(const void* l);
int equal_listener(const void* l1, const void* l2);

cmt_hashtable_t* create_hashtable(hashfn hash, eqfn eq, int bins);
void destroy_hashtable(cmt_hashtable_t* ht);


#endif /** _TCP_STREAM_HASH_INCLUDE_H_ */