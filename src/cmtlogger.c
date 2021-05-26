#include<stdlib.h>
#include<stdio.h>
#include<errno.h>

#include"cmtlogger.h"

#define CAST_LIST_NODE(x) ((list_node_t*)x)
#define CAST_BUFFER(x)	  ((log_buff_t*)x)

static void 
enqueue_free_buffer(cmt_log_thread_context_t* ctx, cmt_log_buff_t* working_bp) {
	pthread_mutex_lock(&ctx->free_mutex);
	tailq_insert_tail(&ctx->free_queue, CAST_LIST_NODE(working_bp));
	ctx->free_buff_cnt++;

	assert(ctx->free_buff_cnt <= NUM_LOG_BUFF);
	assert(ctx->free_buff_cnt + ctx->job_buff_cnt <= NUM_LOG_BUFF);
	pthread_mutex_unlock(&ctx->free_mutex);
}

cmt_log_buff_t* 
dequeue_free_buffer(cmt_log_thread_context_t* ctx) {
	pthread_mutex_lock(&ctx->free_mutex);
	list_node_t* free_bp = tailq_first(&ctx->free_queue);
	if (free_bp) {
		tailq_remove(&ctx->free_queue, free_bp);
		ctx->free_buff_cnt--;
	}

	assert(ctx->free_buff_cnt >= 0);
	assert(ctx->free_buff_cnt + ctx->job_buff_cnt <= NUM_LOG_BUFF);
	pthread_mutex_unlock(&ctx->free_mutex);
	return (CAST_BUFFER(free_bp));
}

void 
enqueue_job_buffer(cmt_log_thread_context_t* ctx, cmt_log_buff_t* working_bp) {
	tailq_insert_tail(&ctx->working_queue, CAST_LIST_NODE(working_bp));
	ctx->job_buff_cnt++;
	ctx->state = ACTIVE_LOGT;
	assert(ctx->job_buff_cnt <= NUM_LOG_BUFF);
	if (ctx->free_buff_cnt + ctx->job_buff_cnt > NUM_LOG_BUFF) {
		TRACE_ERROR("free_buff_cnt(%d) + job_buff_cnt(%d) > NUM_LOG_BUFF(%d)\n",
			ctx->free_buff_cnt, ctx->job_buff_cnt, NUM_LOG_BUFF);
	}
	assert(ctx->free_buff_cnt + ctx->job_buff_cnt <= NUM_LOG_BUFF);
}

static cmt_log_buff_t*
dequeue_job_bufffer(cmt_log_thread_context_t* ctx) {
	pthread_mutex_lock(&ctx->mutex);
	list_node_t* working_bp = tailq_first(&ctx->working_queue);
	if (working_bp) {
		tailq_remove(&ctx->working_queue, working_bp);
		ctx->job_buff_cnt--;
	}
	else {
		ctx->state = IDLE_LOGT;
	}

	assert(ctx->job_buff_cnt >= 0);
	assert(ctx->free_buff_cnt + ctx->job_buff_cnt <= NUM_LOG_BUFF);
	pthread_mutex_unlock(&ctx->mutex);
	return (CAST_BUFFER(working_bp));
}

void
init_log_thread_context(cmt_log_thread_context_t* ctx, int cpu) {
	int i;
	int sv[2];

	/* initialize log_thread_context */
	memset(ctx, 0, sizeof(log_thread_context_t));
	ctx->cpu = cpu;
	ctx->state = IDLE_LOGT;
	ctx->done = 0;

	if (pipe(sv)) {
		fprintf(stderr, "pipe() failed, errno=%d, errstr=%s\n",
			errno, strerror(errno));
		exit(1);
	}
	ctx->sp_fd = sv[0];
	ctx->pair_sp_fd = sv[1];

	pthread_mutex_init(&ctx->mutex, NULL);
	pthread_mutex_init(&ctx->free_mutex, NULL);

	tailq_init(&ctx->working_queue);
	tailq_init(&ctx->free_queue);

	/* initialize free log_buff */
	cmt_log_buff_t* w_buff = malloc(sizeof(cmt_log_buff_t) * NUM_LOG_BUFF);
	assert(w_buff);
	for (i = 0; i < NUM_LOG_BUFF; i++) {
		EnqueueFreeBuffer(ctx, &w_buff[i]);
	}	
}

void 
thread_log_main(void* arg) {
	size_t len;
	cmt_log_thread_context_t* ctx = (cmt_log_thread_context_t*)arg;
	cmt_log_buff_t* w_buff;
	int cnt;

	mtcp_core_affinitize(ctx->cpu);
	//fprintf(stderr, "[CPU %d] Log thread created. thread: %lu\n", 
	//		ctx->cpu, pthread_self());

	TRACE_LOG("Log thread %d is starting.\n", ctx->cpu);

	while (!ctx->done) {
		/* handle every jobs in job buffer*/
		cnt = 0;
		while ((w_buff = DequeueJobBuffer(ctx))) {
			if (++cnt > NUM_LOG_BUFF) {
				TRACE_ERROR("CPU %d: Exceed NUM_LOG_BUFF %d.\n",
					ctx->cpu, cnt);
				break;
			}
			len = fwrite(w_buff->buff, 1, w_buff->buff_len, w_buff->fid);
			if (len != w_buff->buff_len) {
				TRACE_ERROR("CPU %d: Tried to write %d, but only write %ld\n",
					ctx->cpu, w_buff->buff_len, len);
			}
			//assert(len == w_buff->buff_len);
			EnqueueFreeBuffer(ctx, w_buff);
		}

		/* */
		while (ctx->state == IDLE_LOGT && !ctx->done) {
			char temp[1];
			int ret = read(ctx->sp_fd, temp, 1);
			if (ret)
				break;
		}
	}

	TRACE_LOG("Log thread %d out of first loop.\n", ctx->cpu);
	/* handle every jobs in job buffer*/
	cnt = 0;
	while ((w_buff = DequeueJobBuffer(ctx))) {
		if (++cnt > NUM_LOG_BUFF) {
			TRACE_ERROR("CPU %d: "
				"Exceed NUM_LOG_BUFF %d in final loop.\n", ctx->cpu, cnt);
			break;
		}
		len = fwrite(w_buff->buff, 1, w_buff->buff_len, w_buff->fid);
		assert(len == w_buff->buff_len);
		EnqueueFreeBuffer(ctx, w_buff);
	}

	TRACE_LOG("Log thread %d finished.\n", ctx->cpu);
	pthread_exit(NULL);

	return NULL;
}
