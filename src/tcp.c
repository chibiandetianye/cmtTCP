#include<stdint.h>
#include<arpa/inet.h>

#include"tcp.h"
#include"cmtTCP.h"
#include"global.h"
#include"ip.h"

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
		
	}

	if (opt == ACK_OPT_NOW) {
		if (cur_stream->sndvar->ack_cnt < cur_stream->snd->ack_cnt + 1) {
			cur_stream->sndvar>ack_cnt++;
		}
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

	nty_tcp_stream* cur_stream = CreateTcpStream(tcp, NULL, NTY_TCP_SOCK_STREAM,
		iph->daddr, tcph->dest, iph->saddr, tcph->source);
	if (cur_stream == NULL) {
		nty_trace_tcp("INFO: Could not allocate tcp_stream!\n");
		return NULL;
	}

	cur_stream->rcv->irs = seq;
	cur_stream->snd->peer_wnd = window;
	cur_stream->rcv_nxt = cur_stream->rcv->irs;
	cur_stream->snd->cwnd = 1;

#if 1
	cur_stream->rcv->recvbuf = RBInit(tcp->rbm_rcv, cur_stream->rcv->irs + 1);
	if (!cur_stream->rcv->recvbuf) {
		cur_stream->state = NTY_TCP_CLOSED;
		cur_stream->close_reason = TCP_NO_MEM;

	}
#endif

	nty_tcp_parse_options(cur_stream, cur_ts, (uint8_t*)tcph + TCP_HEADER_LEN,
		(tcph->doff << 2) - TCP_HEADER_LEN);
	nty_trace_tcp("nty_tcp_passive_open : %d, %d\n", cur_stream->rcv_nxt, cur_stream->snd->mss);

	return cur_stream;
}