#ifndef _TIMER_INCLUDE_H_
#define _TIMER_INCLUDE_H_

#include<stdint.h>
#include"queue.h"

#define RTO_HASH		3000

typedef struct cmt_rto_hashstore {
	uint32_t rto_now_idx;
	uint32_t rto_now_ts;
	tailq_head_t rto_list[RTO_HASH + 1];
} cmt_rto_hashstore_t;

cmt_rto_hashstore_t* init_rto_hashstore(void);

#endif /** _TIMER_INCLUDE_H_ */