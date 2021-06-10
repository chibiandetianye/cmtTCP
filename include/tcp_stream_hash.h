#ifndef _TCP_STREAM_HASH_INCLUDE_H_
#define _TCP_STREAM_HASH_INCLUDE_H_

#include<stdint.h>

#include"tcp.h"
#include"queue.h"

#define NUM_BINS_FLOWS		131072
#define NUM_BINS_LISTENER	1024
#define TCP_ACK_CNT			3

typedef struct hash_bucket_ {
	list_node_t* tqh_first;
	list_node_t** tqh_last;
} hash_bucket_t;

typedef struct cmt_tcp_stream_hash_bucket_ {
	cmt_tcp_stream_t*  tqh_first;
	cmt_tcp_stream_t** tqh_last;
} cmt_tcp_stream_hash_bucket_t;

typedef struct cmt_tcp_listener_hash_bucket_ {
	cmt_tcp_listener_t*  tqh_first;
	cmt_tcp_listener_t** tqh_last;
};cmt_tcp_listener_hash_bucket_t

typedef unsigned int (*hashfn) (const void*);
typedef int (*eqfn) (const void*, const void*);

typedef struct cmt_hashtable {
	uint8_t ht_count;
	uint8_t bins;

	hash_bucket_t* table;

	hashfn hash;
	eqfn eq;
} cmt_hashtable_t;

typedef struct cmt_tcp_stream_hashtable {
	uint8_t ht_count;
	uint8_t bins;

	union {
		cmt_tcp_stream_hash_bucket_t*	ht_stream;
		cmt_tcp_listener_hash_bucket_t* ht_listener;
	};

	hashfn hash;
	eqfn eq;
} cmt_tcp_hashtable_t;

/** \brief search a value in hashtable 
	@param ht hash table
	@param it key which used to  search value 
*/
void* ht_search(cmt_hashtable_t* ht, const void* it);
/** \brief insert a value in hashtable 
	@param ht hashtable
	@param it value which need to be inserted in hashtable
*/
int ht_insert(cmt_hashtable_t* ht, const void* it);
/** \brief remove a value in hashtable which equal to it
	@param ht hash table
	@param it value which corresponding value will be remove
*/
void* ht_remove(cmt_hashtable_t* ht, const void* it);
/** \brief create a hashtable
	@param hash hash funciton
	@param eq function used to compare value in hash bucket
	@param bins bins in hashtable
*/
cmt_hashtable_t* create_hashtable(hashfn hash, eqfn eq, int bins);
/** \brief destroy a hashtable 
	@param ht hashtable which will be destroyed
*/
void destroy__hashtable(cmt_hashtable_t* ht);


cmt_tcp_listener_t* listen_ht_search(cmt_tcp_hashtable_t* ht, uint16_t* it);
cmt_tcp_stream_t* stream_ht_search(cmt_tcp_hashtable_t* ht, cmt_tcp_stream_t* it);
cmt_tcp_listener_t* listen_ht_remove(cmt_tcp_hashtable_t* ht, cmt_tcp_listener_t* it);
cmt_tcp_stream_t* stream_ht_remove(cmt_tcp_hashtable_t* ht, cmt_stream_t* it);
int listen_ht_insert(cmt_tcp_hashtable_t* ht, cmt_tcp_listener_t* it);
int stream_ht_insert(cmt_tcp_hashtable_t* ht, cmt_tcp_stream_t* it);
cmt_tcp_stream_hashtable_t* create_stream_hashtable(hashfn hash, eqfn eq, int bins);
void destroy_tcp_hashtable(cmt_tcp_hashtable_t* ht);

unsigned int hash_flow(const void* f);
int equal_flow(const void* f1, const void* f2);
unsigned int hash_listener(const void* l);
int equal_listener(const void* l1, const void* l2);



#endif /** _TCP_STREAM_HASH_INCLUDE_H_ */