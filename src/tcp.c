#include<stdint.h>
#include<arpa/inet.h>
#include<assert.h>

#include"tcp.h"
#include"cmtTCP.h"
#include"global.h"
#include"tcp_buffer.h"
#include"ip.h"
#include"tcp_stream.h"
#include"debug.h"

always_inline uint16_t 
cmt_calculate_option(uint8_t flags) {
	uint16_t optlen = 0;

	if (flags & CMT_TCPHDR_SYN) {
		optlen += CMT_TCPOPT_MSS_LEN;
		optlen += CMT_TCPOPT_TIMESTAMPS_LEN;
		optlen += 2;
		optlen += CMT_TCPOPT_WSCALE_LEN + 1;
	}
	else {
		optlen += CMT_TCPOPT_TIMESTAMPS_LEN;
		optlen += 2;
	}
	assert(optlen % 4 == 0);
	return optlen;
}

always_inline uint16_t 
cmt_tcp_calculate_checksum(uint16_t* buf, uint16_t len, uint32_t saddr, uint32_t daddr)
{
	uint32_t sum;
	uint16_t* w;
	int nleft;

	sum = 0;
	nleft = len;
	w = buf;

	while (nleft > 1)
	{
		sum += *w++;
		nleft -= 2;
	}

	// add padding for odd length
	if (nleft)
		sum += *w & ntohs(0xFF00);

	// add pseudo header
	sum += (saddr & 0x0000FFFF) + (saddr >> 16);
	sum += (daddr & 0x0000FFFF) + (daddr >> 16);
	sum += htons(len);
	sum += htons(PROTO_TCP);

	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);

	sum = ~sum;

	return (uint16_t)sum;
}

always_inline void
cmt_tcp_generate_timestamp(cmt_tcp_stream_t* cur_stream, uint8_t* tcpopt, uint32_t cur_ts) {
	uint32_t* ts = (uint32_t*)(tcpopt + 2);

	tcpopt[0] = TCP_OPT_TIMESTAMP;
	tcpopt[1] = CMT_TCPOPT_TIMESTAMPS_LEN;

	ts[0] = htonl(cur_ts);
	ts[1] = htonl(cur_stream->rcv->ts_recent);
}

static void 
cmt_tcp_generate_options(cmt_tcp_stream_t* cur_stream, uint32_t cur_ts,
	uint8_t flags, uint8_t* tcpopt, uint16_t optlen) {

	int i = 0;

	if (flags &	CMT_TCPHDR_SYN) {
		uint16_t mss = cur_stream->sndvar->mss;

		tcpopt[i++] = TCP_OPT_MSS;
		tcpopt[i++] = CMT_TCPOPT_MSS_LEN;
		tcpopt[i++] = mss >> 8;
		tcpopt[i++] = mss % 256;

		tcpopt[i++] = TCP_OPT_NOP;
		tcpopt[i++] = TCP_OPT_NOP;

		cmt_tcp_generate_timestamp(cur_stream, tcpopt + i, cur_ts);
		i += CMT_TCPOPT_TIMESTAMPS_LEN;

		tcpopt[i++] = TCP_OPT_NOP;
		tcpopt[i++] = TCP_OPT_WSCALE;
		tcpopt[i++] = CMT_TCPOPT_WSCALE_LEN;

		tcpopt[i++] = cur_stream->sndvar->wscale_mine;

	}
	else {
		tcpopt[i++] = TCP_OPT_NOP;
		tcpopt[i++] = TCP_OPT_NOP;
		cmt_tcp_generate_timestamp(cur_stream, tcpopt + i, cur_ts);
		i += CMT_TCPOPT_TIMESTAMPS_LEN;
	}

	assert(i == optlen);
}

always_inline uint16_t 
cmt_calculate_chksum(uint16_t* buf, uint16_t len, uint32_t saddr, uint32_t daddr) {
	uint32_t sum;
	uint16_t* w;
	int nleft;

	sum = 0;
	nleft = len;
	w = buf;

	while (nleft > 1)
	{
		sum += *w++;
		nleft -= 2;
	}

	// add padding for odd length
	if (nleft)
		sum += *w & ntohs(0xFF00);

	// add pseudo header
	sum += (saddr & 0x0000FFFF) + (saddr >> 16);
	sum += (daddr & 0x0000FFFF) + (daddr >> 16);
	sum += htons(len);
	sum += htons(PROTO_TCP);

	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);

	sum = ~sum;

	return (uint16_t)sum;
}

always_inline cmt_sender_t* 
cmt_tcp_getsender(cmt_tcp_manager_per* tcp, cmt_tcp_stream_t* cur_stream) {
	return tcp->sender;
}

always_inline void 
cmt_tcp_addto_acklist(cmttcp_manager_per_cpu_t* tcp, cmt_tcp_stream_t* cur_stream) {
	cmt_sender_t* sender = cmt_tcp_getsender(tcp, cur_stream);
	assert(sender != NULL);

	if (!cur_stream->sndvar->on_ack_list) {
		cur_stream->sndvar->on_ack_list = 1;
		tailq_insert_tail(sender->ack_list, cur_stream->sndvar->ack_list);
		sender->ack_list_cnt++;
	}
}

void 
cmt_tcp_addto_controllist(cmttcp_manager_per_cpu_t* tcp, cmt_tcp_stream_t* cur_stream) {
	cmt_sender_t* sender = cmt_tcp_getsender(tcp, cur_stream);
	assert(sender != NULL);

	if (!cur_stream->snd->on_control_list) {
		cur_stream->snd->on_control_list = 1;
		tailq_insert_tail(sender->control_list, cur_stream->sndvar->control_list);
		sender->control_list_cnt++;
	}
}

always_inline void 
cmt_tcp_addto_sendlist(cmt_tcp_manager_per_cpu_t* tcp, cmt_tcp_stream_t* cur_stream) {
	cmt_sender_t* sender = cmt_tcp_getsender(tcp, cur_stream);
	assert(sender != NULL);

	if (!cur_stream->sndvar->sndbuf) {
		assert(0);
		return;
	}

	if (!cur_stream->snd->on_send_list) {
		cur_stream->snd->on_send_list = 1;
		tailq_insert_tail(sender->send_list, cur_stream->sndvar->send_list);
		sender->send_list_cnt++;
	}
}

always_inline void 
cmt_tcp_remove_acklist(cmt_tcp_manager_per_cpu_t* tcp, cmt_tcp_stream_t* cur_stream) {
	cmt_sender_t* sender = cmt_tcp_getsender(tcp, cur_stream);

	if (cur_stream->snd->on_ack_list) {
		cur_stream->snd->on_ack_list = 0;
		tailq_remove(&sender->ack_list, cur_stream->sndvar->ack_list);
		sender->ack_list_cnt--;
	}
}

always_inline void 
cmt_tcp_remove_controllist(cmt_tcp_manager_per_cpu_t* tcp, cmt_tcp_stream_t* cur_stream) {
	cmt_sender_t* sender = cmt_tcp_getsender(tcp, cur_stream);

	if (cur_stream->snd->on_control_list) {
		cur_stream->snd->on_control_list = 0;
		tailq_remove(sender->control_list, cur_stream->sndvar->control_list);
		sender->control_list_cnt--;
	}
}

always_inline void 
cmt_tcp_remove_sendlist(cmttcp_manager_per_cpu_t* tcp, cmt_tcp_stream_t* cur_stream) {
	cmt_sender* sender = cmt_tcp_getsender(tcp, cur_stream);

	if (cur_stream->sndvar->on_send_list) {
		cur_stream->sndvar->on_send_list = 0;
		tailq_remove(sender->send_list, cur_stream->sndvar->send_list);
		sender->send_list_cnt--;
	}
}

void 
cmt_tcp_parse_options(cmttcp_stream_t* cur_stream, uint32_t cur_ts, uint8_t* tcpopt, int len) {

	int i = 0;
	unsigned int opt, optlen;

	for (i = 0; i < len; ) {
		opt = *(tcpopt + i++);
		if (opt == TCP_OPT_END) {
			break;
		}
		else if (opt == TCP_OPT_NOP) {
			continue;
		}
		else {
			optlen = *(tcpopt + i++);
			if (i + optlen - 2 > (unsigned int)len) break;

			if (opt == TCP_OPT_MSS) {
				cur_stream->sndvar->mss = *(tcpopt + i++) << 8;
				cur_stream->sndvar->mss += *(tcpopt + i++);
				cur_stream->sndvar->eff_mss = cur_stream->sndvar->mss;
				cur_stream->sndvar->eff_mss -= (CMT_TCPOPT_TIMESTAMPS_LEN + 2);

			}
			else if (opt == TCP_OPT_WSCALE) {
				cur_stream->sndvar->wscale_peer = *(tcpopt + i++);

			}
			else if (opt == TCP_OPT_SACK_PERMIT) {
				cur_stream->sack_permit = 1;

			}
			else if (opt == TCP_OPT_TIMESTAMP) {
				cur_stream->saw_timestamp = 1;
				cur_stream->rcvvar->ts_recent = ntohl(*(uint32_t*)(tcpopt + i));
				cur_stream->rcvvar->ts_last_ts_upd = cur_ts;
				i += 8;

			}
			else {
				i += optlen - 2;
			}
		}
	}

}

void
cmt_tcp_enqueue_acklist(cmttcp_manager_per_cpu_t* tcp, cmt_tcp_stream_t* cur_stream, uint32_t cur_ts, uint8_t opt) {
	if (!(cur_stream->state == CMT_TCP_ESTABLISHED ||
		cur_stream->state == CMT_TCP_CLOSE_WAIT ||
		cur_stream->state == CMT_TCP_FIN_WAIT_1 ||
		cur_stream->state == CMT_TCP_FIN_WAIT_2)) {
		trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid, "Stream %d: Enqueueing ack at state %d\n",
			cur_stream->id, cur_stream->state);
	}

	if (opt == ACK_OPT_NOW) {
		
			cur_stream->sndvar>ack_cnt++;
		
	}
	else if (opt == ACK_OPT_AGGREGATE) {
		if (cur_stream->sndvar->ack_cnt == 0) {
			cur_stream->sndvar->ack_cnt = 1;
		}
	}
	else if (opt == ACK_OPT_WACK) {
		cur_stream->sndvar->is_wack = 1;
	}

	cmt_tcp_addto_acklist(tcp, cur_stream);
}

int 
cmt_tcp_parse_timestamp(cmt_tcp_timestamp_t* ts, uint8_t* tcpopt, int len) {

	int i;
	unsigned int opt, optlen;

	for (i = 0; i < len; i++) {
		opt = *(tcpopt + i++);
		if (opt == TCP_OPT_END) {
			break;
		}
		else if (opt == TCP_OPT_NOP) {
			continue;
		}
		else {
			optlen = *(tcpopt + i++);
			if (i + optlen - 2 > (unsigned int)len) {
				break;
			}
			if (opt == TCP_OPT_TIMESTAMP) {
				ts->ts_val = ntohl(*(uint32_t*)(tcpopt + i));
				ts->ts_ref = ntohl(*(uint32_t*)(tcpopt + i + 4));
				return 1;
			}
			else {
				i += optlen - 2;
			}
		}
	}

	return 0;
}

int 
cmt_tcppkt_alone(cmttcp_manager_per_cpu_t* tcp,
	uint32_t saddr, uint16_t sport, uint32_t daddr, uint16_t dport,
	uint32_t seq, uint32_t ack_seq, uint16_t window, uint8_t flags,
	uint8_t* payload, uint16_t payloadlen,
	uint32_t cur_ts, uint32_t echo_ts) {

}

int 
cmt_tcp_send_tcppkt(cmt_tcp_stream_t* cur_stream,
	uint32_t cur_ts, uint8_t flags, uint8_t* payload, uint16_t payloadlen) {

}


always_inline  int 
cmt_tcp_filter_synpkt(cmttcp_manager_per_cpu_t* tcp, uint32_t ip, uint16_t port) {
	struct sockaddr_in* addr;

	cmt_tcp_listener_t* listener = 
		(cmt_tcp_listener_t*)listen_ht_search(tcp->listeners, &port);
	if (listener == NULL) return 0;

	addr = &listener->s->s_addr;
	if (addr->sin_port == port) {
		if (addr->sin_addr.s_addr != INADDR_ANY) {
			if (ip == addr->sin_addr.s_addr) {
				return 1;
			}
			return 0;
		}
		
		return 0;
	}
	return 0;
}

always_inline cmt_tcp_stream_t* 
cmt_tcp_passive_open(cmttcp_manager_per_cpu_t* tcp, uint32_t cur_ts, const struct iphdr* iph,
	const struct tcphdr* tcph, uint32_t seq, uint16_t window) {

	cmt_tcp_stream_t* cur_stream = create_tcp_stream(tcp, NULL, NTY_TCP_SOCK_STREAM,
		iph->daddr, tcph->dest, iph->saddr, tcph->source);
	if (unlikely(cur_stream == NULL)) {
		trace_error(stderr, tcp->ctx->cpu, tcp->ctx->tid, "bad allloc!");
		return NULL;
	}

	cur_stream->rcvvar->irs = seq;
	cur_stream->sndvar->peer_wnd = window;
	cur_stream->rcv_nxt = cur_stream->rcv->irs;
	cur_stream->sndvar->cwnd = 1;

#if 1
	cur_stream->rcv->recvbuf = recv_get(tcp->recvb_manager, cur_stream->rcvvar->irs + 1);
	if (unlikely(!cur_stream->rcvvar->recvbuf)) {
		cur_stream->state = NTY_TCP_CLOSED;
		cur_stream->close_reason = TCP_NO_MEM;
	}
#endif

	cmt_tcp_parse_options(cur_stream, cur_ts, (uint8_t*)tcph + TCP_HEADER_LEN,
		(tcph->doff << 2) - TCP_HEADER_LEN);
	
	return cur_stream;
}

int 
cmt_tcp_active_open(cmttcp_manager_per_cpu_t* tcp, cmt_tcp_stream_t* cur_stream, uint32_t cur_ts,
	struct tcphdr* tcph, uint32_t seq, uint32_t ack_seq, uint16_t window) {
	cur_stream->rcv->irs = seq;
	cur_stream->snd_nxt = ack_seq;
	cur_stream->snd->peer_wnd = window;
	cur_stream->rcv->snd_wl1 = cur_stream->rcv->irs - 1;
	cur_stream->rcv_nxt = cur_stream->rcv->irs + 1;
	cur_stream->rcv->last_ack_seq = ack_seq;

	nty_tcp_parse_options(cur_stream, cur_ts, (uint8_t*)tcph + TCP_HEADER_LEN,
		(tcph->doff << 2) - TCP_HEADER_LEN);

	cur_stream->snd->cwnd = ((cur_stream->snd->cwnd == 1) ?
		(cur_stream->snd->mss * 2) : cur_stream->snd->mss);
	cur_stream->snd->ssthresh = cur_stream->snd->mss * 10;

	update_retransmission_timer(tcp, cur_stream, cur_ts);

	return 1;
}

always_inline int 
cmt_tcp_validseq(cmt_tcp_manager_per_cpu_t* tcp, cmt_tcp_stream_t* cur_stream, uint32_t cur_ts,
	struct tcphdr* tcph, uint32_t seq, uint32_t ack_seq, int payloadlen) {
	if (!tcph->rst && cur_stream->saw_timestamp) {
		cmt_tcp_timestamp ts;
		if (!cmt_tcp_parse_timestamp(&ts, (uint8_t*)tcph + TCP_HEADER_LEN,
			(tcph->doff << 2) - TCP_HEADER_LEN)) {
			trace_message(stdout, tcp->ctx->cpu, tcp = > ctx->tid, "no timestamps found\n");
			return 0;
		}

		if (TCP_SEQ_LT(ts.ts_val, cur_stream->rcv->ts_recent)) {
			cmt_tcp_enqueue_acklist(tcp, cur_stream, cur_ts, ACK_OPT_NOW);
		}
		else {
			if (TCP_SEQ_GT(ts.ts_val, cur_stream->rcv->ts_recent)) {
				trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid, "Timestamp update. cur: %u, prior: %u "
					"(time diff: %uus)\n",
					ts.ts_val, cur_stream->rcv->ts_recent,
					TS_TO_USEC(cur_ts - cur_stream->rcv->ts_last_ts_upd));
				cur_stream->rcv->ts_last_ts_upd = cur_ts;
			}
			cur_stream->rcv->ts_recent = ts.ts_val;
			cur_stream->rcv->ts_lastack_rcvd = ts.ts_ref;
		}
	}

	if (!TCP_SEQ_BETWEEN(seq + payloadlen, cur_stream->rcv_nxt,
		cur_stream->rcv_nxt + cur_stream->rcv->rcv_wnd)) {
		if (tcph->rst) {
			trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid, "tcp connection reset\n");
			return 0;
		}
		if (cur_stream->state == NTY_TCP_ESTABLISHED) {
			if (seq + 1 == cur_stream->rcv_nxt) {
				cmt_tcp_enqueue_acklist(tcp, cur_stream, cur_ts, ACK_OPT_AGGREGATE);
				return 0;
			}

			if (TCP_SEQ_LEQ(seq, cur_stream->rcv_nxt)) {
				cmt_tcp_enqueue_acklist(tcp, cur_stream, cur_ts, ACK_OPT_AGGREGATE);
			}
			else {
				cmt_tcp_enqueue_acklist(tcp, cur_stream, cur_ts, ACK_OPT_NOW);
			}
		}
		else {
			if (cur_stream->state == NTY_TCP_TIME_WAIT) {
				trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid,
					"Stream %d: tw expire update to %u\n",
					cur_stream->id, cur_stream->rcv->ts_tw_expire);
				add_to_timewait_list(tcp, cur_stream, cur_ts);
			}
			cmt_tcp_addto_controllist(tcp, cur_stream);
		}
		return 0;
	}
	return 1;
}


static cmt_tcp_stream_t* 
cmt_create_stream(cmt_tcp_manager_t* tcp, uint32_t cur_ts, const struct iphdr* iph,
	int ip_len, const struct tcphdr* tcph, uint32_t seq, uint32_t ack_seq,
	int payloadlen, uint16_t window) {
	nty_tcp_stream* cur_stream;
	int ret = 0;

	if (tcph->syn && !tcph->ack) {
		nty_trace_tcp("ip:0x%x, port:%d\n", iph->daddr, tcph->dest);
		ret = nty_tcp_filter_synpkt(tcp, iph->daddr, tcph->dest);
		if (!ret) {
			nty_trace_tcp("Refusing SYN packet.\n");
			nty_tcppkt_alone(tcp, iph->daddr, tcph->dest, iph->saddr, tcph->source,
				0, seq + payloadlen + 1, 0, NTY_TCPHDR_RST | NTY_TCPHDR_ACK,
				NULL, 0, cur_ts, 0);
			return NULL;
		}
		nty_trace_tcp("nty_create_stream\n");
		cur_stream = nty_tcp_passive_open(tcp, cur_ts, iph, tcph, seq, window);
		if (!cur_stream) {
			nty_trace_tcp("Not available space in flow pool.\n");

			nty_tcppkt_alone(tcp, iph->daddr, tcph->dest, iph->saddr, tcph->source,
				0, seq + payloadlen + 1, 0, NTY_TCPHDR_RST | NTY_TCPHDR_ACK,
				NULL, 0, cur_ts, 0);

			return NULL;
		}
		return cur_stream;
	}
	else if (tcph->rst) {
		nty_trace_tcp("Reset packet comes\n");
		return NULL;
	}
	else {
		if (tcph->ack) {
			nty_tcppkt_alone(tcp, iph->daddr, tcph->dest, iph->saddr, tcph->source,
				ack_seq, 0, 0, NTY_TCPHDR_RST,
				NULL, 0, cur_ts, 0);
		}
		else {
			nty_tcppkt_alone(tcp, iph->daddr, tcph->dest, iph->saddr, tcph->source,
				0, seq + payloadlen, 0, NTY_TCPHDR_RST | NTY_TCPHDR_ACK,
				NULL, 0, cur_ts, 0);
		}
		return NULL;
	}

}


static void 
cmt_tcp_handle_listen(cmt_tcp_manager_per_cpu_t* tcp, uint32_t cur_ts,
	cmt_tcp_stream_t* cur_stream, struct tcphdr* tcph) {
	if (tcph->syn) {
		if (cur_stream->state == NTY_TCP_LISTEN) {
			cur_stream->rcv_nxt++;
		}
		cur_stream->state = NTY_TCP_SYN_RCVD;
		trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid,
			"Stream %d: TCP_ST_SYN_RCVD\n", cur_stream->id);
		cmt_tcp_addto_controllist(tcp, cur_stream);
	}
	else {
		trace_message(stdout, tcp->ctx->cpu, tcp->ctx->id,
			"Stream %d (TCP_ST_LISTEN): "
			"Packet without SYN.\n", cur_stream->id);
		assert(0);
	}
}

static void 
cmt_tcp_handle_syn_sent(cmt_tcp_manager_per_cpu_t* tcp, uint32_t cur_ts,
	cmt_tcp_stream_t* cur_stream, const struct iphdr* iph, struct tcphdr* tcph,
	uint32_t seq, uint32_t ack_seq, int payloadlen, uint16_t window) {
	if (tcph->ack) {
		if (TCP_SEQ_LEQ(ack_seq, cur_stream->snd->iss) ||
			TCP_SEQ_GT(ack_seq, cur_stream->snd_nxt)) {
			if (!tcph->rst) {
				cmt_tcppkt_alone(tcp, iph->daddr, tcph->dest, iph->saddr, tcph->source,
					ack_seq, 0, 0, CMT_TCPHDR_RST, NULL, 0, cur_ts, 0);
			}
			return;
		}
		cur_stream->snd->snd_una++;
	}

	if (tcph->rst && tcph->ack) {
		trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid,
			"Stream %d: TCP_CLOSE_WAIT\n", cur_stream->id);
		cur_stream->state = CMT_TCP_CLOSE_WAIT;
		cur_stream->close_reason = TCP_RESET;
		if (cur_stream->s) {
			//.... raise error event
		}
		else {
			
			stream_destroy(tcp, cur_stream);
		}
		return;
	}

	if (tcph->syn && tcph->ack) {
		int ret = cmt_tcp_active_open(tcp, cur_stream, cur_ts,
			tcph, seq, ack_seq, window);
		if (unlikely(!ret))//Always false
			return;

		cur_stream->snd->nrtx = 0;
		cur_stream->rcv_nxt = cur_stream->rcv->irs + 1;
		remove_from_rto_list(tcp, cur_stream);
		cur_stream->state = CMT_TCP_ESTABLISHED;

		trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid,
			"Stream %d: TCP_ST_ESTABLISHED\n", cur_stream->id);

		if (cur_stream->s) {
			//Raise Write Event
		}
		else {
			trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid,
				"Stream established,but no socket\n")
			cmt_tcppkt_alone(tcp, iph->saddr, tcph->dest, iph->daddr, tcph->source,
				0, seq + payloadlen + 1, 0, CMT_TCPHDR_RST | CMT_TCPHDR_ACK, NULL, 0, cur_ts, 0);
			cur_stream->close_reason = TCP_ACTIVE_CLOSE;
			stream_destroy(tcp, cur_stream);
		}

		cmt_tcp_addto_controllist(tcp, cur_stream);
		add_to_timeout_list(tcp, cur_stream);
	}
	else {
		trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid,
			"Stream %d: TCP_ST_RCVD\n", cur_stream->id);
		cur_stream->state = CMT_TCP_SYN_RCVD;
		cur_stream->snd_nxt = cur_stream->snd->iss;
		cmt_tcp_addto_controllist(tcp, cur_stream);
	}
}


static void 
cmt_tcp_handle_syn_rcvd(cmt_tcp_manager_per_cpu_t* tcp, uint32_t cur_ts,
	cmt_tcp_stream_t* cur_stream, struct tcphdr* tcph, uint32_t ack_seq) {

	cmt_tcp_send_t* snd = cur_stream->sndvar;

	if (tcph->ack) {
		if (ack_seq != snd->iss + 1) {
			trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid,
				"Stream %d (TCP_ST_SYN_RCVD): "
				"weird ack_seq: %u, iss: %u\n",
				cur_stream->id, ack_seq, snd->iss);
			exit(1);
			return;
		}

		snd->snd_una++;
		cur_stream->snd_nxt = ack_seq;
		uint32_t prior_cwnd = snd->cwnd;
		snd->cwnd = (prior_cwnd == 1) ? snd->mss * 2 : snd->mss;
		snd->nrtx = 0;

		cur_stream->rcv_nxt = cur_stream->rcv->irs + 1;
		remove_from_rto_list(tcp, cur_stream);

		cur_stream->state = CMT_TCP_ESTABLISHED;
		trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid,
			"Stream %d: TCP_ST_ESTABLISHED\n", cur_stream->id);

		struct cmt_tcp_listener_t* listener = listen_ht_search(tcp->listeners, &tcph->dest);
		int ret = stream_enqueue(listener->acceptq, cur_stream);
		if (ret < 0) {
			cur_stream->close_reason = TCP_NOT_ACCEPTED;
			cur_stream->state = CMT_TCP_CLOSED;
			cmt_tcp_addto_controllist(tcp, cur_stream);
		}
		//
		add_to_timeout_list(tcp, cur_stream);


		if (listener->s) {
			//&& 
			/*
			 * should move to epoll for check s->epoll type.
			 */
			 //AddtoEpollEvent


		}
	}
		else {
			trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid,
				"Stream %d (TCP_ST_SYN_RCVD): No ACK.\n",
				cur_stream->id);

			cur_stream->snd_nxt = snd->iss;
			cmt_tcp_addto_controllist(tcp, cur_stream);
		}


}

static void 
cmt_tcp_handle_established(cmt_tcp_manager_per_cpu_t* tcp, uint32_t cur_ts,
	cmt_tcp_stream_t* cur_stream, struct tcphdr* tcph, uint32_t seq, uint32_t ack_seq,
	uint8_t* payload, int payloadlen, uint16_t window) {
	if (tcph->syn) {
		trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid, 
			"Stream %d (TCP_ST_ESTABLISHED): weird SYN. "
			"seq: %u, expected: %u, ack_seq: %u, expected: %u\n",
			cur_stream->id, seq, cur_stream->rcv_nxt,
			ack_seq, cur_stream->snd_nxt);

		cur_stream->snd_nxt = ack_seq;
		cmt_tcp_addto_controllist(tcp, cur_stream);
		return;
	}

	if (payloadlen > 0) {
		if (cmt_tcp_process_payload(tcp, cur_stream, cur_ts, payload, seq, payloadlen)) {
			cmt_tcp_enqueue_acklist(tcp, cur_stream, cur_ts, ACK_OPT_AGGREGATE);
		}
		else {
			cmt_tcp_enqueue_acklist(tcp, cur_stream, cur_ts, ACK_OPT_NOW);
		}
	}

	if (tcph->ack) {
		if (cur_stream->snd->sndbuf) {
			cmt_tcp_process_ack(tcp, cur_stream, cur_ts,
				tcph, seq, ack_seq, window, payloadlen);
		}
	}

	if (tcph->fin) {
		if (seq + payloadlen == cur_stream->rcv_nxt) {
			cur_stream->state = NTY_TCP_CLOSE_WAIT;
			trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid, 
				"Stream %d: TCP_ST_CLOSE_WAIT\n", cur_stream->id);
			cur_stream->rcv_nxt++;
			cmt_tcp_addto_controllist(tcp, cur_stream);
			//Raise Read Event
			trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid,
				"nty_tcp_flush_read_event\n");

			if (cur_stream->s && !(cur_stream->s->opts & CMT_TCP_NONBLOCK)) {
				cmt_tcp_flush_read_event(cur_stream->rcv);
			}

		}
		else {
			cmt_tcp_enqueue_acklist(tcp, cur_stream, cur_ts, ACK_OPT_NOW);
			return;
		}
	}
	return;
}

void 
cmt_tcp_handle_last_ack(cmt_tcp_manager_per_cpu_t* tcp, uint32_t cur_ts, 
	const struct iphdr* iph, int ip_len, 
	cmt_tcp_stream_t* cur_stream, struct tcphdr* tcph,
	uint32_t seq, uint32_t ack_seq, int payloadlen, uint16_t window) {

	if (TCP_SEQ_LT(seq, cur_stream->rcv_nxt)) {
		trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid, 
			"Stream %d (TCP_ST_LAST_ACK): "
			"weird seq: %u, expected: %u\n",
			cur_stream->id, seq, cur_stream->rcv_nxt);
		return;
	}
	if (tcph->ack) {
		if (cur_stream->snd->sndbuf) {
			cmt_tcp_process_ack(tcp, cur_stream, cur_ts,
				tcph, seq, ack_seq, window, payloadlen);
		}
		if (!cur_stream->snd->is_fin_sent) {
			trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid, 
				"Stream %d (TCP_ST_LAST_ACK): "
				"No FIN sent yet.\n", cur_stream->id);
			return;
		}
		if (ack_seq == cur_stream->snd->fss + 1) {
			cur_stream->snd->snd_una++;
			update_retransmission_timer(tcp, cur_stream, cur_ts);

			cur_stream->state = NTY_TCP_CLOSED;
			cur_stream->close_reason = TCP_PASSIVE_CLOSE;

			trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid, 
				"Stream %d: NTY_TCP_CLOSED\n", cur_stream->id);
			stream_destroy(tcp, cur_stream);
		}
		else {
			trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid, 
				"Stream %d (TCP_ST_LAST_ACK): Not ACK of FIN. "
				"ack_seq: %u, expected: %u\n",
				cur_stream->id, ack_seq, cur_stream->snd->fss + 1);
			cmt_tcp_addto_controllist(tcp, cur_stream);
		}
	}
	else {
		trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid, 
			"Stream %d (TCP_ST_LAST_ACK): No ACK\n",
			cur_stream->id);
		cmt_tcp_addto_controllist(tcp, cur_stream);
	}
}

void 
cmt_tcp_handle_fin_wait_1(cmt_tcp_manager_per_cpu_t* tcp, uint32_t cur_ts,
	cmt_tcp_stream_t* cur_stream, struct tcphdr* tcph, uint32_t seq, uint32_t ack_seq,
	uint8_t* payload, int payloadlen, uint16_t window) {
	if (TCP_SEQ_LT(seq, cur_stream->rcv_nxt)) {
		trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid, 
			"Stream %d (TCP_ST_LAST_ACK): "
			"weird seq: %u, expected: %u\n",
			cur_stream->id, seq, cur_stream->rcv_nxt);
		cmt_tcp_addto_controllist(tcp, cur_stream);
		return;
	}

	if (tcph->ack) {
		if (cur_stream->snd->sndbuf) {
			cmt_tcp_process_ack(tcp, cur_stream, cur_ts, tcph, seq, ack_seq, window, payloadlen);
		}
		if (cur_stream->snd->is_fin_sent &&
			ack_seq == cur_stream->snd->fss + 1) {
			cur_stream->snd->snd_una = ack_seq;
			if (TCP_SEQ_GT(ack_seq, cur_stream->snd_nxt)) {
				trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid, 
					"Stream %d: update snd_nxt to %u\n",
					cur_stream->id, ack_seq);
				cur_stream->snd_nxt = ack_seq;
			}
			cur_stream->snd->nrtx = 0;
			remove_from_rto_list(tcp, cur_stream);
			cur_stream->state = CMT_TCP_FIN_WAIT_2;

			trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid, 
				"Stream %d: TCP_ST_FIN_WAIT_2\n",
				cur_stream->id);
		}
	}
	else {
		trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid, 
			"Stream %d: does not contain an ack!\n",
			cur_stream->id);
		return;
	}

	if (payloadlen > 0) {
		if (cmt_tcp_process_payload(tcp, cur_stream, cur_ts, payload, seq, payloadlen)) {
			cmt_tcp_enqueue_acklist(tcp, cur_stream, cur_ts, ACK_OPT_AGGREGATE);
		}
		else {
			cmt_tcp_enqueue_acklist(tcp, cur_stream, cur_ts, ACK_OPT_NOW);
		}
	}

	if (tcph->fin) {
		if (seq + payloadlen == cur_stream->rcv_nxt) {
			cur_stream->rcv_nxt++;
			if (cur_stream->state == CMT_TCP_FIN_WAIT_1) {
				cur_stream->state = CMT_TCP_CLOSING;
				trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid, 
					"Stream %d: TCP_ST_CLOSING\n", cur_stream->id);

			}
			else if (cur_stream->state == NTY_TCP_FIN_WAIT_2) {

				cur_stream->state = NTY_TCP_TIMEWAIT;
				trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid, 
					"Stream %d: TCP_ST_TIME_WAIT\n", cur_stream->id);
				add_to_timewait_list(tcp, cur_stream, cur_ts);

			}
			else {
				assert(0);
			}
			cmt_tcp_addto_controllist(tcp, cur_stream);
		}
	}
}

void 
cmt_tcp_handle_fin_wait_2(cmt_tcp_manager_per_cpu_t* tcp, uint32_t cur_ts,
	cmt_tcp_stream_t* cur_stream, struct tcphdr* tcph, uint32_t seq, uint32_t ack_seq,
	uint8_t* payload, int payloadlen, uint16_t window) {

	if (tcph->ack) {
		if (cur_stream->snd->sndbuf) {
			cmt_tcp_process_ack(tcp, cur_stream, cur_ts,
				tcph, seq, ack_seq, window, payloadlen);
		}
	}
	else {
		trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid, 
			"Stream %d: does not contain an ack!\n",
			cur_stream->id);
		return;
	}
	if (payloadlen > 0) {
		if (cmt_tcp_process_payload(tcp, cur_stream, cur_ts, payload, seq, payloadlen)) {
			cmt_tcp_enqueue_acklist(tcp, cur_stream, cur_ts, ACK_OPT_AGGREGATE);
		}
		else {
			cmt_tcp_enqueue_acklist(tcp, cur_stream, cur_ts, ACK_OPT_NOW);
		}
	}
	if (tcph->fin) {
		if (seq + payloadlen == cur_stream->rcv_nxt) {
			cur_stream->state = CMT_TCP_TIME_WAIT;
			cur_stream->rcv_nxt++;
			trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid, 
				"Stream %d: TCP_ST_TIME_WAIT\n", cur_stream->id);

			add_to_timewait_list(tcp, cur_stream, cur_ts);
			cmt_tcp_addto_controllist(tcp, cur_stream);
		}
	}
}

void 
cmt_tcp_handle_closing(cmt_tcp_manager_per_cpu_t* tcp, uint32_t cur_ts,
	cmt_tcp_stream_t* cur_stream, struct tcphdr* tcph, uint32_t seq, uint32_t ack_seq,
	int payloadlen, uint16_t window) {

	if (tcph->ack) {
		if (cur_stream->sndvar->sndbuf) {
			cmt_tcp_process_ack(tcp, cur_stream, cur_ts, tcph, seq, ack_seq, window, payloadlen);
		}
		if (!cur_stream->sndvar->is_fin_sent) {
			trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid, 
				"Stream %d (TCP_ST_CLOSING): "
				"No FIN sent yet.\n", cur_stream->id);
			return;
		}
		if (ack_seq != cur_stream->sndvar->fss + 1) {
			return;
		}
		cur_stream->sndvar->snd_una = ack_seq;
		cur_stream->snd_nxt = ack_seq;
		update_retransmission_timer(tcp, cur_stream, cur_ts);

		cur_stream->state = CMT_TCP_TIME_WAIT;
		trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid, 
			"Stream %d: TCP_ST_TIME_WAIT\n", cur_stream->id);

		add_to_timewait_list(tcp, cur_stream, cur_ts);
	}
	else {
		trace_message(stdout, tcp->ctx->cpu, tcp->ctx->tid, 
			"Stream %d (TCP_ST_CLOSING): Not ACK\n",
			cur_stream->id);
		return;
	}

}

void 
cmt_tcp_estimate_rtt(nty_tcp_manager* tcp, nty_tcp_stream* cur_stream, uint32_t mrtt) {

#define TCP_RTO_MIN		0
	long m = mrtt;
	uint32_t tcp_rto_min = TCP_RTO_MIN;
	nty_tcp_recv* rcv = cur_stream->rcv;

	if (m == 0) {
		m = 1;
	}
	if (rcv->srtt != 0) {
		m -= (rcv->srtt >> 3);
		rcv->srtt += 3;
		if (m < 0) {
			m = -m;
			m -= (rcv->mdev >> 2);
			if (m > 0) {
				m >>= 3;
			}
		}
		else {
			m -= (rcv->mdev >> 2);
		}
		rcv->mdev += m;
		if (rcv->mdev > rcv->mdev_max) {
			rcv->mdev_max = rcv->mdev;
			if (rcv->mdev_max > rcv->rttvar) {
				rcv->rttvar = rcv->mdev_max;
			}
		}

		if (TCP_SEQ_GT(cur_stream->snd->snd_una, rcv->rtt_seq)) {
			if (rcv->mdev_max < rcv->rttvar) {
				rcv->rttvar -= (rcv->rttvar - rcv->mdev_max);
			}
			rcv->rtt_seq = cur_stream->snd_nxt;
			rcv->mdev_max = tcp_rto_min;
		}
	}
	else {
		rcv->srtt = m << 3;
		rcv->mdev = m << 1;
		rcv->mdev_max = rcv->rttvar = MAX(rcv->mdev, tcp_rto_min);
		rcv->rtt_seq = cur_stream->snd_nxt;
	}

	nty_trace_tcp("mrtt: %u (%uus), srtt: %u (%ums), mdev: %u, mdev_max: %u, "
		"rttvar: %u, rtt_seq: %u\n", mrtt, mrtt * TIME_TICK,
		rcv->srtt, TS_TO_MSEC((rcv->srtt) >> 3), rcv->mdev,
		rcv->mdev_max, rcv->rttvar, rcv->rtt_seq);

}

static int nty_tcp_process_payload(nty_tcp_manager* tcp, nty_tcp_stream* cur_stream,
	uint32_t cur_ts, uint8_t* payload, uint32_t seq, int payloadlen) {

	nty_tcp_recv* rcv = cur_stream->rcv;

	if (TCP_SEQ_LT(seq + payloadlen, cur_stream->rcv_nxt)) {
		return 0;
	}

	if (TCP_SEQ_GT(seq + payloadlen, cur_stream->rcv_nxt + rcv->rcv_wnd)) {
		return 0;
	}

	if (!rcv->recvbuf) {
		nty_trace_tcp("nty_tcp_process_payload --> \n");
		rcv->recvbuf = RBInit(tcp->rbm_rcv, rcv->irs + 1);
		if (!rcv->recvbuf) {
			cur_stream->state = NTY_TCP_CLOSED;
			cur_stream->close_reason = TCP_NO_MEM;
			//Raise Error Event
			nty_trace_tcp(" Raise Error Event \n");

			return -1;
		}
	}

#if NTY_ENABLE_BLOCKING
	if (pthread_mutex_lock(&rcv->read_lock)) {
		if (errno == EDEADLK) {
			perror("ProcessTCPPayload: read_lock blocked\n");
		}
		assert(0);
	}
#else
	if (SBUF_LOCK(&rcv->read_lock)) {
		if (errno == EDEADLK) {
			perror("ProcessTCPPayload: read_lock blocked\n");
		}
		assert(0);
	}
#endif
	uint32_t prev_rcv_nxt = cur_stream->rcv_nxt;
	int ret = RBPut(tcp->rbm_rcv, rcv->recvbuf, payload, (uint32_t)payloadlen, seq);
	if (ret < 0) {
		nty_trace_tcp("Cannot merge payload. reason: %d\n", ret);
	}

	if (cur_stream->state == NTY_TCP_FIN_WAIT_1 ||
		cur_stream->state == NTY_TCP_FIN_WAIT_2) {
		RBRemove(tcp->rbm_rcv, rcv->recvbuf, rcv->recvbuf->merged_len, AT_MTCP);
	}

	cur_stream->rcv_nxt = rcv->recvbuf->head_seq + rcv->recvbuf->merged_len;
	rcv->rcv_wnd = rcv->recvbuf->size - rcv->recvbuf->merged_len;
#if NTY_ENABLE_BLOCKING
	pthread_mutex_unlock(&rcv->read_lock);
#else
	SBUF_UNLOCK(&rcv->read_lock);
#endif
	if (TCP_SEQ_LEQ(cur_stream->rcv_nxt, prev_rcv_nxt)) {
		return 0;
	}

	if (cur_stream->state == NTY_TCP_ESTABLISHED) {
		//RaiseReadEvent
		//cur_stream->s
		if (cur_stream->s) {
			// && (cur_stream->s->epoll & NTY_EPOLLIN)
			// should move to epoll for check epoll type
			//AddtoEpollEvent
#if NTY_ENABLE_EPOLL_RB
			if (tcp->ep) {
				epoll_event_callback(tcp->ep, cur_stream->s->id, NTY_EPOLLIN);
			}

#else
			nty_epoll_add_event(tcp->ep, NTY_EVENT_QUEUE, cur_stream->s, NTY_EPOLLIN);
#endif
			if (!(cur_stream->s->opts & NTY_TCP_NONBLOCK)) {
				nty_tcp_flush_read_event(rcv);
			}
		}
	}
	return 1;
}

static int nty_tcp_process_rst(nty_tcp_manager* tcp, nty_tcp_stream* cur_stream, uint32_t ack_seq) {
	nty_trace_tcp("Stream %d: TCP RESET (%d)\n",
		cur_stream->id, cur_stream->state);

	if (cur_stream->state <= NTY_TCP_SYN_SENT) return 0;

	if (cur_stream->state == NTY_TCP_SYN_RCVD) {
		if (ack_seq == cur_stream->snd_nxt) {
			cur_stream->state = NTY_TCP_CLOSED;
			cur_stream->close_reason = TCP_RESET;
			DestroyTcpStream(tcp, cur_stream);
		}
		return 1;
	}

	if (cur_stream->state == NTY_TCP_FIN_WAIT_1 ||
		cur_stream->state == NTY_TCP_FIN_WAIT_2 ||
		cur_stream->state == NTY_TCP_LAST_ACK ||
		cur_stream->state == NTY_TCP_CLOSING ||
		cur_stream->state == NTY_TCP_TIME_WAIT) {
		cur_stream->state = NTY_TCP_CLOSED;
		cur_stream->close_reason = TCP_ACTIVE_CLOSE;
		DestroyTcpStream(tcp, cur_stream);

		return 1;
	}

	if (cur_stream->state >= NTY_TCP_ESTABLISHED &&
		cur_stream->state <= NTY_TCP_CLOSE_WAIT) {
		nty_trace_tcp("Stream %d: Notifying connection reset.\n", cur_stream->id);
	}

	if (!(cur_stream->snd->on_closeq ||
		cur_stream->snd->on_closeq_int ||
		cur_stream->snd->on_resetq ||
		cur_stream->snd->on_resetq_int)) {
		cur_stream->state = NTY_TCP_CLOSE_WAIT;
		cur_stream->close_reason = TCP_RESET;
		//close event
	}

	return 1;
}

static void nty_tcp_process_ack(nty_tcp_manager* tcp, nty_tcp_stream* cur_stream, uint32_t cur_ts,
	struct tcphdr* tcph, uint32_t seq, uint32_t ack_seq, uint16_t window, int payloadlen) {

	nty_tcp_send* snd = cur_stream->snd;

	uint32_t cwindow = window;

	if (!tcph->syn) {
		cwindow = cwindow << snd->wscale_peer;
	}

	uint32_t right_wnd_edge = snd->peer_wnd + cur_stream->rcv->snd_wl1;
	if (cur_stream->state == NTY_TCP_FIN_WAIT_1 ||
		cur_stream->state == NTY_TCP_FIN_WAIT_2 ||
		cur_stream->state == NTY_TCP_CLOSING ||
		cur_stream->state == NTY_TCP_CLOSE_WAIT ||
		cur_stream->state == NTY_TCP_LAST_ACK) {

		if (snd->is_fin_sent && ack_seq == snd->fss + 1)
			ack_seq--;
	}

	if (TCP_SEQ_GT(ack_seq, snd->sndbuf->head_seq + snd->sndbuf->len)) {
		//char *state_str = TCPStateToString(cur_stream);
		nty_trace_tcp("Stream %d (%d): invalid acknologement. ack_seq: %u, possible max_ack_seq: %u\n",
			cur_stream->id, cur_stream->state, ack_seq,
			snd->sndbuf->head_seq + snd->sndbuf->len);

		return;
	}

	uint32_t cwindow_prev;
	if (TCP_SEQ_LT(cur_stream->rcv->snd_wl1, seq) ||
		(cur_stream->rcv->snd_wl1 == seq &&
			TCP_SEQ_LT(cur_stream->rcv->snd_wl2, ack_seq)) ||
		(cur_stream->rcv->snd_wl2 == ack_seq &&
			cwindow > snd->peer_wnd)) {
		cwindow_prev = snd->peer_wnd;
		snd->peer_wnd = cwindow;
		cur_stream->rcv->snd_wl1 = seq;
		cur_stream->rcv->snd_wl2 = ack_seq;

		if (cwindow_prev < cur_stream->snd_nxt - snd->snd_una &&
			snd->peer_wnd >= cur_stream->snd_nxt - snd->snd_una) {
			nty_trace_tcp("%u Broadcasting client window update! "
				"ack_seq: %u, peer_wnd: %u (before: %u), "
				"(snd_nxt - snd_una: %u)\n",
				cur_stream->id, ack_seq, snd->peer_wnd, cwindow_prev,
				cur_stream->snd_nxt - snd->snd_una);
			//RaiseWriteEvent(mtcp, cur_stream);
			nty_tcp_flush_send_event(snd);
		}
	}

	uint8_t dup = 0;
	if (TCP_SEQ_LT(ack_seq, cur_stream->snd_nxt)) {
		if (ack_seq == cur_stream->rcv->last_ack_seq && payloadlen == 0) {
			if (cur_stream->rcv->snd_wl2 + snd->peer_wnd == right_wnd_edge) {
				if (cur_stream->rcv->dup_acks + 1 > cur_stream->rcv->dup_acks) {
					cur_stream->rcv->dup_acks++;
				}
				dup = 1;
			}
		}
	}

	if (!dup) {
		cur_stream->rcv->dup_acks = 0;
		cur_stream->rcv->last_ack_seq = ack_seq;
	}

	if (dup && cur_stream->rcv->dup_acks == 3) {
		nty_trace_tcp("Triple duplicated ACKs!! ack_seq: %u\n", ack_seq);
		if (TCP_SEQ_LT(ack_seq, cur_stream->snd_nxt)) {
			nty_trace_tcp("Reducing snd_nxt from %u to %u\n",
				cur_stream->snd_nxt, ack_seq);

			if (ack_seq != snd->snd_una) {
				nty_trace_tcp("ack_seq and snd_una mismatch on tdp ack. "
					"ack_seq: %u, snd_una: %u\n",
					ack_seq, snd->snd_una);
			}
			cur_stream->snd_nxt = ack_seq;
		}

		snd->ssthresh = MIN(snd->cwnd, snd->peer_wnd) / 2;
		if (snd->ssthresh < 2 * snd->mss) {
			snd->ssthresh = 2 * snd->mss;
		}
		snd->cwnd = snd->ssthresh + 3 * snd->mss;
		nty_trace_tcp("Fast retransmission. cwnd: %u, ssthresh: %u\n",
			snd->cwnd, snd->ssthresh);

		if (snd->nrtx < TCP_MAX_RTX) {
			snd->nrtx++;
		}
		else {
			nty_trace_tcp("Exceed MAX_RTX. \n");
		}
		nty_tcp_addto_sendlist(tcp, cur_stream);
	}
	else if (cur_stream->rcv->dup_acks > 3) {

		if ((uint32_t)(snd->cwnd + snd->mss) > snd->cwnd) {
			snd->cwnd += snd->mss;
			nty_trace_tcp("Dupack cwnd inflate. cwnd: %u, ssthresh: %u\n",
				snd->cwnd, snd->ssthresh);
		}
	}

	if (TCP_SEQ_GT(ack_seq, cur_stream->snd_nxt)) {
		nty_trace_tcp("Updating snd_nxt from %u to %u\n",
			cur_stream->snd_nxt, ack_seq);
		cur_stream->snd_nxt = ack_seq;
		if (snd->sndbuf->len == 0) {
			nty_tcp_remove_sendlist(tcp, cur_stream);
		}
	}

	if (TCP_SEQ_GEQ(snd->sndbuf->head_seq, ack_seq)) {
		return;
	}
	//
	uint32_t rmlen = ack_seq - snd->sndbuf->head_seq;
	if (rmlen > 0) {
		uint16_t packets = rmlen / snd->eff_mss;
		if ((rmlen / snd->eff_mss) * snd->eff_mss > rmlen) {
			packets++;
		}
		if (cur_stream->saw_timestamp) {
			nty_tcp_estimate_rtt(tcp, cur_stream, cur_ts - cur_stream->rcv->ts_lastack_rcvd);
			snd->rto = (cur_stream->rcv->srtt >> 3) + cur_stream->rcv->rttvar;
			assert(snd->rto > 0);
		}
		else {
			nty_trace_tcp("not implemented.\n");
		}

		if (cur_stream->state >= NTY_TCP_ESTABLISHED) {
			if (snd->cwnd < snd->ssthresh) {
				if ((snd->cwnd + snd->mss) > snd->cwnd) {
					snd->cwnd += snd->mss * packets;
				}
				nty_trace_tcp("slow start cwnd : %u, ssthresh: %u\n",
					snd->cwnd, snd->ssthresh);
			}
		}
		else {
			uint32_t new_cwnd = snd->cwnd + packets * snd->mss * snd->mss / snd->cwnd;
			if (new_cwnd > snd->cwnd) {
				snd->cwnd = new_cwnd;
			}
		}
		if (pthread_mutex_lock(&snd->write_lock)) {
			if (errno == EDEADLK) {
				perror("ProcessACK: write_lock blocked\n");
			}
			assert(0);
		}
		int ret = SBRemove(tcp->rbm_snd, snd->sndbuf, rmlen);
		if (ret <= 0) return;

		snd->snd_una = ack_seq;
		uint32_t snd_wnd_prev = snd->snd_wnd;
		snd->snd_wnd = snd->sndbuf->size - snd->sndbuf->len;

		if (snd_wnd_prev <= 0) {
			//Raise Write Event
			nty_tcp_flush_send_event(snd);
		}

		pthread_mutex_unlock(&snd->write_lock);
		UpdateRetransmissionTimer(tcp, cur_stream, cur_ts);
	}

}


int nty_tcp_process(nty_nic_context* ctx, unsigned char* stream) {

	struct iphdr* iph = (struct iphdr*)(stream + sizeof(struct ethhdr));
	struct tcphdr* tcph = (struct tcphdr*)(stream + sizeof(struct ethhdr) + sizeof(struct iphdr));

	assert(sizeof(struct iphdr) == (iph->ihl << 2));

	int ip_len = ntohs(iph->tot_len);
	uint8_t* payload = (uint8_t*)tcph + (tcph->doff << 2);
	int tcp_len = ip_len - (iph->ihl << 2);
	int payloadlen = tcp_len - (tcph->doff << 2);

	//unsigned short check = in_cksum((unsigned short*)tcph, tcp_len);
	unsigned short check = nty_tcp_calculate_checksum((uint16_t*)tcph, tcp_len, iph->saddr, iph->daddr);
	nty_trace_tcp("check : %x, orgin : %x, payloadlen:%d\n", check, tcph->check, payloadlen);
	if (check) return -1;

	nty_tcp_stream tstream = { 0 };
#if 1
	tstream.saddr = iph->daddr;
	tstream.sport = tcph->dest;
	tstream.daddr = iph->saddr;
	tstream.dport = tcph->source;
#else
	ts.saddr = iph->saddr;
	ts.sport = tcph->source;
	ts.daddr = iph->daddr;
	ts.dport = tcph->dest;
#endif

	struct timeval cur_ts = { 0 };
	gettimeofday(&cur_ts, NULL);

	uint32_t ts = TIMEVAL_TO_TS(&cur_ts);
	uint32_t seq = ntohl(tcph->seq);
	uint32_t ack_seq = ntohl(tcph->ack_seq);
	uint16_t window = ntohs(tcph->window);


	nty_trace_tcp("saddr:0x%x,sport:%d,daddr:0x%x,dport:%d, seq:%d, ack_seq:%d\n",
		iph->daddr, ntohs(tcph->dest), iph->saddr, ntohs(tcph->source),
		seq, ack_seq);

	nty_tcp_stream* cur_stream = (nty_tcp_stream*)StreamHTSearch(nty_tcp->tcp_flow_table, &tstream);
	if (cur_stream == NULL) {
		cur_stream = nty_create_stream(nty_tcp, ts, iph, ip_len, tcph, seq, ack_seq, payloadlen, window);
		if (!cur_stream) {
			return -2;
		}
	}
	int ret = 0;
	if (cur_stream->state > NTY_TCP_SYN_RCVD) {
		ret = nty_tcp_validseq(nty_tcp, cur_stream, ts, tcph, seq, ack_seq, payloadlen);
		if (!ret) {
			nty_trace_tcp("Stream %d: Unexpected sequence: %u, expected: %u\n",
				cur_stream->id, seq, cur_stream->rcv_nxt);
			return 1;
		}
	}

	nty_trace_tcp("nty_tcp_process state : %d\n", cur_stream->state);

	if (tcph->syn) {
		cur_stream->snd->peer_wnd = window;
	}
	else {
		cur_stream->snd->peer_wnd = (uint32_t)window << cur_stream->snd->wscale_peer;
	}

	cur_stream->last_active_ts = ts;
	UpdateTimeoutList(nty_tcp, cur_stream);

	if (tcph->rst) {
		cur_stream->have_reset = 1;
		if (cur_stream->state > NTY_TCP_SYN_SENT) {
			if (nty_tcp_process_rst(nty_tcp, cur_stream, ack_seq)) {
				return 1;
			}
		}
	}

	switch (cur_stream->state) {
	case NTY_TCP_LISTEN: {
		nty_tcp_handle_listen(nty_tcp, ts, cur_stream, tcph);
		break;
	}
	case NTY_TCP_SYN_SENT: {
		nty_tcp_handle_syn_sent(nty_tcp, ts, cur_stream, iph, tcph, seq,
			ack_seq, payloadlen, window);
		break;
	}
	case NTY_TCP_SYN_RCVD: {
		if (tcph->syn && seq == cur_stream->rcv->irs) {
			nty_tcp_handle_listen(nty_tcp, ts, cur_stream, tcph);
		}
		else {
			nty_tcp_handle_syn_rcvd(nty_tcp, ts, cur_stream, tcph, ack_seq);
			if (payloadlen > 0 && cur_stream->state == NTY_TCP_ESTABLISHED) {
				nty_tcp_handle_established(nty_tcp, ts, cur_stream, tcph, seq, ack_seq,
					payload, payloadlen, window);
			}
		}
		break;
	}
	case NTY_TCP_ESTABLISHED: {
		nty_tcp_handle_established(nty_tcp, ts, cur_stream, tcph, seq, ack_seq,
			payload, payloadlen, window);
		break;
	}
	case NTY_TCP_CLOSE_WAIT: {
		nty_tcp_handle_close_wait(nty_tcp, ts, cur_stream, tcph, seq, ack_seq,
			payloadlen, window);
		break;
	}
	case NTY_TCP_LAST_ACK: {
		nty_tcp_handle_last_ack(nty_tcp, ts, iph, ip_len, cur_stream, tcph,
			seq, ack_seq, payloadlen, window);
		break;
	}
	case NTY_TCP_FIN_WAIT_1: {
		nty_tcp_handle_fin_wait_1(nty_tcp, ts, cur_stream, tcph, seq, ack_seq,
			payload, payloadlen, window);
		break;
	}
	case NTY_TCP_FIN_WAIT_2: {
		nty_tcp_handle_fin_wait_2(nty_tcp, ts, cur_stream, tcph, seq, ack_seq,
			payload, payloadlen, window);
		break;
	}
	case NTY_TCP_CLOSING: {
		nty_tcp_handle_closing(nty_tcp, ts, cur_stream, tcph, seq, ack_seq,
			payloadlen, window);
		break;
	}
	case NTY_TCP_TIME_WAIT: {
		if (cur_stream->on_timewait_list) {
			RemoveFromTimewaitList(nty_tcp, cur_stream);
			AddtoTimewaitList(nty_tcp, cur_stream, ts);
		}
		nty_tcp_addto_controllist(nty_tcp, cur_stream);
		break;
	}
	case NTY_TCP_CLOSED: {
		break;
	}
	}

	return 1;
}

