#ifndef _CMTTCP_INCLUDE_H_
#define _CMTTCP_INCLUDE_H_

#include<sys/types.h>

#include"queue.h"
#include"cmt_memory_pool.h"
#include"ring.h"
#include"global.h"

#define DEFAULT_WORKING_LIST 4

#ifndef CORE_NUMS
#define CORE_NUMS 8
#endif /** CORE_NUMS */

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif /** ETH_ALEN */

/**	\brief configuration */
#define BACKLOG_SIZE	(10 * 1024)
#define MAX_PKT_SIZE	(2 * 1024)

#define MAX_THREAD_NAME 512

#define CMT_MAX_CONCURRENCY 1024

/**	\brief structure of eth table */
typedef struct eth_table {
	volatile int8_t flag; //wthether mark initing completing

	char dev_name[128];
	int ifindex;
	int stat_print;
	unsigned char haddr[ETH_ALEN];
	uint32_t netmask;
	//	unsigned char dst_haddr[ETH_ALEN];
	uint32_t ip_addr;
} eth_table_t;

/** \brief structure of arp table */
typedef struct arp_table {
	volatile entry_nums;
	tailq_head_t* entry;
	tailq_head_t* gateway;
} arp_table_t;


typedef struct cmttcp_config {
	/** network interface config */
	eth_table_t* eths;
	int* nif_to_eidx; //mapping physics port indexes to that of the configure port-list;
	int eth_nums;

	/** arp config */
	arp_table_t* arp;

	int core_nums;
	int working_core_nums;
	int rss_core_nums;
	int max_concurrency;
} cmttcp_config_t;

typedef enum sched_stat {
	POLLING,
	INTERRUPT,
	DISABLE
} sched_stat;

typedef struct cmt_thread_context {
	char name[MAX_THREAD_NAME];
	int cpu;
	pid_t tid;
	sched_stat state;
} cmt_thread_context_t;

typedef struct working_list_context {
	cmt_thread_context_t cpu_context __attribute__((packed));
	
	cmt_pool_t* pool;
	
	cmt_ring_t* recv_ring _cache_aligned;
	cmt_ring_t* send_ring _cache_aligned;

	struct {
		uint8_t count;
		uint32_t ready;
	} poll_flag _cache_aligned;

	struct {
		pthread_cond_t cond;
		pthread_mutex_t lock;
		uint64_t prev_end;
		unit64_t start_timestamp;
	} pv_flag;
} working_list_context;

typedef struct cmttcp_manager {
	int nums_working_tcp_cores;
	
	struct cmt_socket_map_list {
		cmt_socket_map_t dump;
		cmt_socket_map_t* last;
	} cmt_socket_map_list;

	cmt_socket_map_t* sock_map;

} cmttcp_manager_t;

typedef struct cmttcp_working_list_context {
	cmt_thread_context_t _packed;
	
	cmt_pool_t* pool;
	
	cmttcp_manager_t* manager;

	cmt_ring_t* recv_ring _cache_aligned;
	cmt_ring_t* send_ring _cache_aligned;

	struct {
		uint8_t count;
		uint32_t ready;
	} poll_flag _cache_aligned;

	struct {
		pthread_cond_t cond;
		pthread_mutex_t lock;
		uint64_t prev_end;
		unit64_t start_timestamp;
	} pv_flag;
} cmttcp_working_list_context_t;

cmttcp_manager_t* get_cmttcp_manager();

extern cmttcp_config_t* cmttcp_config;
extern cmttcp_manager_t* cmttcp_manager;

#endif /** _CMTTCP_INCLUDE_H_ */