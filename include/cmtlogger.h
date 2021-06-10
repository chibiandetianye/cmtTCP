#ifndef _CMT_LOGGER_INCLUDE_H_
#define _CMT_LOGGER_INCLUDE_H_

#include<stdint.h>
#include<pthread.h>

#include"queue.h"

#define LOG_BUF_SIZE (256 * 1024)
#define NUM_LOG_BUFF (100)

typedef enum {
	IDLE_LOGT,
	ACTIVE_LOGT
} log_thread_state;

typedef struct log_buff {
	list_node_t* buff_link;

	int tid;
	int buff_len;
	char buff[LOG_BUF_SIZE];
} cmt_log_buff_t;

typedef struct log_thread_context {
	pthread_t thread_id;
	int cpu;
	int done;
	int sp_fd;
	int pair_sp_fd;
	int free_buff_cnt;
	int job_buff_cnt;

	uint8_t state;

	pthread_mutex_t mutex;
	pthread_mutex_t free_mutex;

	tailq_head_t* working_queue;
	tailq_head_t* free_queue;
} cmt_log_thread_context_t;

cmt_log_buff_t* dequeue_free_buffer(cmt_log_thread_context_t* ctx);
void enqueue_job_buffer(cmt_log_thread_context_t* ctx, cmt_log_buff_t* working_bp);
void init_log_thread_context(cmt_log_thread_context_t* ctx, int cpu);
void* thread_log_main(void* arg);

#endif /** _CMT_LOGGER_INCLUDE_H_ */