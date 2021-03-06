#include<assert.h>
#include<stdio.h>

#include"timer.h"
#include"cmt_memory_pool.h"
#include"global.h"
#include"tcp.h"

cmt_rto_hashstore_t* 
init_rto_hashstore(void) {
	cmt_pool_t* pool = get_cmt_pool();
	if (unlikely(pool == NULL)) {
		printf("%d cannot get pool\n", __LINE__);
		return NULL;
	}
	cmt_rto_hashstore_t* hs = cmt_palloc(pool, sizeof(cmt_rto_hashstore_t));
	if (unlikely(!hs)) {
		printf("%d failed to allocate rto hash\n", __LINE__);
		return NULL;
	}

	int i = 0;
	for (i = 0; i < RTO_HASH + 1; i++) {
		tailq_init(&hs->rto_list[i]);
	}
	return hs;
}

void 
add_to_rto_list(cmttcp_manager_per_cpu_t* tcp, cmt_tcp_stream_t* cur_stream) {

	if (!tcp->rto_list_cnt) {
		tcp->rto_list->rto_now_idx = 0;
		tcp->rto_list->rto_now_ts = cur_stream->snd->ts_rto;
	}

	if (cur_stream->on_rto_idx < 0) {
#if 0
		if (cur_stream->on_timeout_list) {
			printf("Stream %u: cannot be in both "
				"rto and timewait list.\n", cur_stream->id);
			return;
		}
#endif
		int diff = (int32_t)(cur_stream->snd->ts_rto - tcp->rto_store->rto_now_ts);
		if (diff < RTO_HASH) {
			int offset = cur_stream->snd->ts_rto % RTO_HASH;//(diff + tcp->rto_store->rto_now_idx) % RTO_HASH;
			cur_stream->on_rto_idx = offset;
			tailq_insert_tail(&(tcp->rto_store->rto_list[offset]),
				cur_stream->timer_link);
		}
		else {
			cur_stream->on_rto_idx = RTO_HASH;
			tailq_insert_tail(&(tcp->rto_store->rto_list[RTO_HASH]),
				cur_stream->timer_link);
		}
		tcp->rto_list_cnt++;
	}
}

void 
remove_from_rto_list(cmttcp_manager_per_cput_t* tcp, cmt_tcp_stream_t* cur_stream) {
	if (cur_stream->on_rto_idx < 0) return;

	tailq_remove(&(tcp->rto_store->rto_list[cur_stream->on_rto_idx]),
		cur_stream->timer_link);

	cur_stream->on_rto_idx = -1;
	tcp->rto_list_cnt--;
}

void
add_to_timewait_list(cmt_tcp_manager_per_cpu_t* tcp, cmt_tcp_stream_t* cur_stream, uint32_t cur_ts) {
	cur_stream->rcv->ts_tw_expire = cur_ts + CMT_TCP_TIMEWAIT;

	if (cur_stream->on_timewait_list) {
		// Update list in sorted way by ts_tw_expire
		tailq_remove(&tcp->timewait_list, cur_stream->timer_link);
		tailq_insert_tail(&tcp->timewait_list, cur_stream->timer_link);
	}
	else {
		if (cur_stream->on_rto_idx >= 0) {
			printf("Stream %u: cannot be in both "
				"timewait and rto list.\n", cur_stream->id);
			//assert(0);
			remove_from_rto_list(tcp, cur_stream);
		}

		cur_stream->on_timewait_list = 1;
		tailq_insert_tail(&tcp->timewait_list, cur_stream->timer_link);
		tcp->timewait_list_cnt++;
	}
}

void 
remove_from_timewait_list(cmttcp_manager_t* tcp, cmt_tcp_stream_t* cur_stream) {
	if (!cur_stream->on_timewait_list) {
		assert(0);
		return;
	}

	tailq_remove(&tcp->timewait_list, cur_stream->timer_link);
	cur_stream->on_timewait_list = 0;
	tcp->timewait_list_cnt--;
}

void 
add_to_timeout_list(cmttcp_manager_per_cpu_t* tcp, cmt_tcp_stream_t* cur_stream) {
	if (cur_stream->on_timeout_list) {
		assert(0);
		return;
	}

	cur_stream->on_timeout_list = 1;
	tailq_insert_tail(&tcp->timeout_list, cur_stream->timeout_link);
	tcp->timeout_list_cnt++;
}

void 
remove_from_timeout_list(cmttcp_manager_per_cpu_t* tcp, cmt_tcp_stream_t* cur_stream) {
	if (cur_stream->on_timeout_list) {
		cur_stream->on_timeout_list = 0;
		tailq_remove(&tcp->timeout_list, cur_stream->timeout_link);
		tcp->timeout_list_cnt--;
	}
}

void 
update_timeout_list(cmttcp_manager_per_cpu_t* tcp, cmt_tcp_stream_t* cur_stream) {
	if (cur_stream->on_timeout_list) {
		tailq_remove(&tcp->timeout_list, cur_stream->timeout_link);
		tailq_insert_tail(&tcp->timeout_list, cur_stream->timeout_link);
	}
}

void 
update_retransmission_timer(cmttcp_manager_per_cpu_t* tcp,
	cmt_tcp_stream_t* cur_stream, uint32_t cur_ts) {
	assert(cur_stream->snd->rto > 0);
	cur_stream->snd->nrtx = 0;

	/* if in rto list, remove it */
	if (cur_stream->on_rto_idx >= 0) {
		remove_from_rto_list(tcp, cur_stream);
	}

	if (TCP_SEQ_GT(cur_stream->snd_nxt, cur_stream->snd->snd_una)) {
		cur_stream->snd->ts_rto = cur_ts + cur_stream->snd->rto;
		addto_rto_list(tcp, cur_stream);

	}
	else {
		printf("All packets are acked. snd_una: %u, snd_nxt: %u\n",
			cur_stream->snd->snd_una, cur_stream->snd_nxt);
	}
}

int 
handle_rto(cmttcp_manager_per_cpu_t* tcp, uint32_t cur_ts, cmt_tcp_stream_t* cur_stream) {

	uint8_t backoff;

	if (cur_stream->sndvar->nrtx < TCP_MAX_RTX) {
		cur_stream->sndvar->nrtx++;
	}
	else {
		if (cur_stream->state < CMT_TCP_ESTABLISHED) {
			cur_stream->state = CMT_TCP_CLOSED;
			cur_stream->close_reason = TCP_CONN_FAIL;
			destroy_tcp_stream(tcp, cur_stream);
		}
		else {
			cur_stream->state = CMT_TCP_CLOSED;
			cur_stream->close_reason = TCP_CONN_LOST;
			if (cur_stream->socket) {
				//RaiseErrorEvent
			}
			else {
				destroy_tcp_stream(tcp, cur_stream);
			}
		}

		return -1;
	}

	if (cur_stream->sndvar->nrtx > cur_stream->sndvar->max_nrtx) {
		cur_stream->sndvar->max_nrtx = cur_stream->sndvar->nrtx;
	}

	if (cur_stream->state >= CMT_TCP_ESTABLISHED) {
		uint32_t rto_prev;
		backoff = MIN(cur_stream->sndvar->nrtx, TCP_MAX_BACKOFF);

		rto_prev = cur_stream->sndvar->rto;
		cur_stream->sndvar->rto = ((cur_stream->rcvvar->srtt >> 3) + cur_stream->rcvvar->rttvar) << backoff;
		if (cur_stream->sndvar->rto <= 0) {
			cur_stream->sndvar->rto = rto_prev;
		}
	}
	else if (cur_stream->state >= CMT_TCP_SYN_SENT) {
		if (cur_stream->sndvar->nrtx < TCP_MAX_BACKOFF) {
			cur_stream->sndvar->rto <<= 1;
		}
	}

	cur_stream->sndvar->ssthresh = MIN(cur_stream->sndvar->cwnd, cur_stream->sndvar->peer_wnd) / 2;
	if (cur_stream->sndvar->ssthresh < (2 * cur_stream->sndvar->mss)) {
		cur_stream->sndvar->ssthresh = cur_stream->sndvar->mss * 2;
	}
	cur_stream->sndvar->cwnd = cur_stream->sndvar->mss;

	printf("Stream %d Timeout. cwnd: %u, ssthresh: %u\n",
		cur_stream->id, cur_stream->snd->cwnd, cur_stream->snd->ssthresh);

	if (cur_stream->state == CMT_TCP_SYN_SENT) {
		/* SYN lost */
		if (cur_stream->snd->nrtx > TCP_MAX_SYN_RETRY) {
			cur_stream->state = CMT_TCP_CLOSED;
			cur_stream->close_reason = TCP_CONN_FAIL;
			printf("Stream %d: SYN retries exceed maximum retries.\n",
				cur_stream->id);
			if (cur_stream->socket) {
				//RaiseErrorEvent(mtcp, cur_stream);
			}
			else {
				destroy_tcp_stream(tcp, cur_stream);
			}

			return -1;
		}
		printf("Stream %d Retransmit SYN. snd_nxt: %u, snd_una: %u\n",
			cur_stream->id, cur_stream->snd_nxt, cur_stream->snd->snd_una);

	}
	else if (cur_stream->state == CMT_TCP_SYN_RCVD) {
		printf("Stream %d: Retransmit SYN/ACK. snd_nxt: %u, snd_una: %u\n",
			cur_stream->id, cur_stream->snd_nxt, cur_stream->snd->snd_una);
	}
	else if (cur_stream->state == CMT_TCP_ESTABLISHED) {
		/* Data lost */
		printf("Stream %d: Retransmit data. snd_nxt: %u, snd_una: %u\n",
			cur_stream->id, cur_stream->snd_nxt, cur_stream->snd->snd_una);

	}
	else if (cur_stream->state == CMT_TCP_CLOSE_WAIT) {
		/* Data lost */
		printf("Stream %d: Retransmit data. snd_nxt: %u, snd_una: %u\n",
			cur_stream->id, cur_stream->snd_nxt, cur_stream->snd->snd_una);

	}
	else if (cur_stream->state == CMT_TCP_LAST_ACK) {
		/* FIN/ACK lost */
		printf("Stream %d: Retransmit FIN/ACK. "
			"snd_nxt: %u, snd_una: %u\n",
			cur_stream->id, cur_stream->snd_nxt, cur_stream->snd->snd_una);

	}
	else if (cur_stream->state == CMT_TCP_FIN_WAIT_1) {
		/* FIN lost */
		printf("Stream %d: Retransmit FIN. snd_nxt: %u, snd_una: %u\n",
			cur_stream->id, cur_stream->snd_nxt, cur_stream->snd->snd_una);
	}
	else if (cur_stream->state == NTY_TCP_CLOSING) {
		printf("Stream %d: Retransmit ACK. snd_nxt: %u, snd_una: %u\n",
			cur_stream->id, cur_stream->snd_nxt, cur_stream->snd->snd_una);
		//TRACE_DBG("Stream %d: Retransmitting at CLOSING\n", cur_stream->id);

	}
	else {
		printf("Stream %d: not implemented state! state: %d, rto: %u\n",
			cur_stream->id,
			cur_stream->state, cur_stream->snd->rto);
		assert(0);
		return -1;
	}

	cur_stream->snd_nxt = cur_stream->snd->snd_una;
	if (cur_stream->state == CMT_TCP_ESTABLISHED ||
		cur_stream->state == CMT_TCP_CLOSE_WAIT) {

		cmt_tcp_addto_sendlist(tcp, cur_stream);

	}
	else if (cur_stream->state == CMT_TCP_FIN_WAIT_1 ||
		cur_stream->state == CMT_TCP_CLOSING ||
		cur_stream->state == CMT_TCP_LAST_ACK) {

		if (cur_stream->snd->fss == 0) {
			printf("Stream %u: fss not set.\n", cur_stream->id);
		}

		if (TCP_SEQ_LT(cur_stream->snd_nxt, cur_stream->snd->fss)) {

			if (cur_stream->snd->on_control_list) {
				cmt_tcp_remove_controllist(tcp, cur_stream);
			}
			cur_stream->control_list_waiting = 1;
			cmt_tcp_addto_sendlist(tcp, cur_stream);

		}
		else {

			cmt_tcp_addto_controllist(tcp, cur_stream);
		}

	}
	else {
		cmt_tcp_addto_controllist(tcp, cur_stream);
	}

	return 0;
}


always_inline void 
rearrange_rto_store(cmttcp_manager_per_cpu_t* tcp) {
	list_node_t* walk, * next;
	tailq_head_t* rto_list = &tcp->rto_list->rto_list[RTO_HASH];
	int cnt = 0;

	for (walk = tailq_first(rto_list);
		walk != NULL; walk = next) {
		next = tailq_next(walk);

		int diff = (int32_t)(tcp->rto_store->rto_now_ts - walk->sndvar->ts_rto);
		if (diff < RTO_HASH) {
			int offset = (diff + tcp->rto_store->rto_now_idx) % RTO_HASH;
			tailq_remove(&tcp->rto_store->rto_list[RTO_HASH],
				walk);
			walk->on_rto_idx = offset;
			tailq_insert_tail(&(tcp->rto_store->rto_list[offset]),
				timer_link);
		}
		cnt++;
	}
}


void 
check_rtm_timeout(cmttcp_manager_per_cpu_t* tcp, uint32_t cur_ts, int thresh) {

	list_node_t* walk, * next;
	struct rto_head* rto_list;

	if (!tcp->rto_list_cnt) {
		return;
	}

	int cnt = 0;

	while (1) {

		rto_list = &tcp->rto_store->rto_list[tcp->rto_store->rto_now_idx];
		if ((int32_t)(cur_ts - tcp->rto_store->rto_now_ts) < 0) {
			break;
		}

		for (walk = tailq_first(rto_list); walk != NULL; walk = next) {
			if (++cnt > thresh) break;

			next = tailq_next(walk->sndvar->timer_link);

			if (walk->on_rto_idx >= 0) {
				tailq_remove(rto_list, walk);
				tcp->rto_list_cnt--;

				walk->on_rto_idx = -1;
				HandleRTO(tcp, cur_ts, walk);

			}
			else {
				printf("Stream %d: not on rto list.\n", walk->id);
			}
		}

		if (cnt < thresh) break;
		else {
			tcp->rto_store->rto_now_idx = (tcp->rto_store->rto_now_idx + 1) % RTO_HASH;
			tcp->rto_store->rto_now_ts++;
			if (!(tcp->rto_store->rto_now_idx % 1000)) {
				RearrangeRTOStore(tcp);
			}
		}
	}

}

void check_timewait_expire(cmt_tcp_manager_per_cpu_t* tcp, uint32_t cur_ts, int thresh)
{
	nty_tcp_stream* walk, * next;
	int cnt;

	cnt = 0;

	for (walk = TAILQ_FIRST(&tcp->timewait_list);
		walk != NULL; walk = next) {
		if (++cnt > thresh)
			break;
		next = TAILQ_NEXT(walk, snd->timer_link);

		if (walk->on_timewait_list) {
			if ((int32_t)(cur_ts - walk->rcv->ts_tw_expire) >= 0) {
				if (!walk->snd->on_control_list) {

					TAILQ_REMOVE(&tcp->timewait_list, walk, snd->timer_link);
					walk->on_timewait_list = 0;
					tcp->timewait_list_cnt--;

					walk->state = NTY_TCP_CLOSED;
					walk->close_reason = TCP_ACTIVE_CLOSE;
					nty_trace_timer("Stream %d: TCP_ST_CLOSED\n", walk->id);
					DestroyTcpStream(tcp, walk);
				}
			}
			else {
				break;
			}
		}
		else {
			nty_trace_timer("Stream %d: not on timewait list.\n", walk->id);
		}
	}

}

void check_connection_timeout(cmt_tcp_manager_per_cpu_t* tcp, uint32_t cur_ts, int thresh)
{
	nty_tcp_stream* walk, * next;
	int cnt;

	cnt = 0;
	for (walk = TAILQ_FIRST(&tcp->timeout_list);
		walk != NULL; walk = next) {
		if (++cnt > thresh)
			break;
		next = TAILQ_NEXT(walk, snd->timeout_link);
		if ((int32_t)(cur_ts - walk->last_active_ts) >=
			(NTY_TCP_TIMEOUT * 1000)) {

			walk->on_timeout_list = 0;
			TAILQ_REMOVE(&tcp->timeout_list, walk, snd->timeout_link);
			tcp->timeout_list_cnt--;
			walk->state = NTY_TCP_CLOSED;
			walk->close_reason = TCP_TIMEDOUT;

			if (walk->socket) {
				//RaiseErrorEvent(mtcp, walk);
			}
			else {
				DestroyTcpStream(tcp, walk);
			}
		}
		else {
			break;
		}

	}
}