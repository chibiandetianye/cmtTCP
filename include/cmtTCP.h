#ifndef _CMTTCP_INCLUDE_H_
#define _CMTTCP_INCLUDE_H_

#include<sys/types.h>

#include"queue.h"
#include"cmt_memory_pool.h"
#include"ring.h"
#include"global.h"
#include"tcp.h"
#include"tcp_stream_hash.h"
#include"socket.h"
#include"addr_pool.h"
#include"tcp_buffer.h"
#include"io_module.h"
#include"timer.h"

#define DEFAULT_WORKING_LIST 4

#define MAXCPUS 16

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

#define CMT_SOCKFD_NR (1024 * 1024)

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

/**	\brief Data structure for recording thread information
	------------------------------------------------------
	Each thread has a unique data structure that records the 
	information and status of the current thread
*/
typedef struct cmt_thread_context {
	char name[MAX_THREAD_NAME];
	int cpu;
	pid_t tid;
	sched_stat state;
} cmt_thread_context_t;

/**	\brief Data structure for working thread
	-------------------------------------------------------
	Created by the main thread, when the main thread receives
	the data packet, the data packet is passed to the worker
	thread to process the corresponding data link layer and 
	network layer information
*/
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

//typedef struct cmttcp_send_manager {
//	cmt_thread_context_t* ctx;
//	cmt_ring_t *
//} cmttcp_send_manager_t;
//
//typedef struct cmttcp_recv_manager {
//	cmt_thread_context_t* ctx;
//} cmttcp_recv_manager_t;

extern struct cmttcp_manager;

/**	\brief Data structure for managering tcp process flow
	-----------------------------------------------------
	Created by the main thread, each thread occupies a core,
	which is responsible for receiving data packets from the
	worker threads or sending data packets to the worker 
	threads for processing. Process multiple tcp packets 
	in a single thread
*/
typedef struct cmttcp_manager_per_cpu {
	cmt_thread_context* ctx;
	struct cmttcp_manager* manager;

	/*
	memory manager
	*/
	cmt_pool_t* pool;
	cmt_sb_manager_t* sendb_manager;
	cmt_recvb_manager_t* recvb_manager;
	cmt_stream_manager_t* stream_manager;
	cmt_recv_manager_t* recv_manager;
	cmt_snd_manager_t* send_manager;

	/*
	 queue for recieving or sending data packet
	*/
	cmt_ring_t* send_ring _cache_aligned;
	cmt_ring_t* recv_ring _cache_aligned;

	cmt_sender_t* sender;

	cmt_tcp_listener_t* listeners;

	/*
		record tcp information
	*/
	cmt_tcp_hashtable_t* tcp_flow_table;
	cmt_sock_table_t* fd_table;
	cmt_socket_map_t* smap;
	addr_pool_t* addr_pool;

	cmt_rto_hashstore_t* rto_list;
	tailq_head* timewait_list;
	tailq_head* timeout_list;

	uint64_t flow_count;
	
	int rto_list_cnt;
	int timewait_list_cnt;
	int timeout_list_cnt;

	/*
		used to synchronize information 
		between threads
	*/
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

} cmttcp_manager_per_cpu_t;


/*	\brief Data structure for managing all tcp processing threads
	-------------------------------------------------------------
*/
typedef struct cmttcp_manager {
	int nums_working_tcp_cores;
	int nums_working_cores;
	cmttcp_manager_per_cpu_t* tcp_mpercpu[MAXCPUS];
	struct working_list_context* working_thread[DEFAULT_WORKING_LIST];

	struct io_module_func* io_module;

	struct cmt_socket_map_list {
		cmt_socket_map_t dump;
		cmt_socket_map_t* last;
	} cmt_socket_map_list;

	cmt_socket_map_t* sock_map;
	cmt_socket_table_t* fdtable;
} cmttcp_manager_t;

/*	\brief get tcp manager
*/
cmttcp_manager_t* get_cmttcp_manager();

/*	\brief initiliaze the moduel
*/
int cmttcp_init();

extern cmttcp_config_t* cmttcp_config;
extern cmttcp_manager_t* cmttcp_manager;

#endif /** _CMTTCP_INCLUDE_H_ */